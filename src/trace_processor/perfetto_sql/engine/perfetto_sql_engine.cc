/*
 * Copyright (C) 2023 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "src/trace_processor/perfetto_sql/engine/perfetto_sql_engine.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "perfetto/base/compiler.h"
#include "perfetto/base/logging.h"
#include "perfetto/base/status.h"
#include "perfetto/ext/base/flat_hash_map.h"
#include "perfetto/ext/base/status_or.h"
#include "perfetto/ext/base/string_utils.h"
#include "perfetto/ext/base/string_view.h"
#include "src/trace_processor/containers/string_pool.h"
#include "src/trace_processor/db/runtime_table.h"
#include "src/trace_processor/db/table.h"
#include "src/trace_processor/perfetto_sql/engine/created_function.h"
#include "src/trace_processor/perfetto_sql/engine/function_util.h"
#include "src/trace_processor/perfetto_sql/engine/perfetto_sql_parser.h"
#include "src/trace_processor/perfetto_sql/engine/perfetto_sql_preprocessor.h"
#include "src/trace_processor/perfetto_sql/engine/runtime_table_function.h"
#include "src/trace_processor/perfetto_sql/intrinsics/table_functions/static_table_function.h"
#include "src/trace_processor/sqlite/db_sqlite_table.h"
#include "src/trace_processor/sqlite/scoped_db.h"
#include "src/trace_processor/sqlite/sql_source.h"
#include "src/trace_processor/sqlite/sqlite_engine.h"
#include "src/trace_processor/tp_metatrace.h"
#include "src/trace_processor/util/sql_argument.h"
#include "src/trace_processor/util/sql_modules.h"
#include "src/trace_processor/util/status_macros.h"

#include "protos/perfetto/trace_processor/metatrace_categories.pbzero.h"

// Implementation details
// ----------------------
//
// The execution of PerfettoSQL statements is the joint responsibility of
// several classes which all are linked together in the following way:
//
//  PerfettoSqlEngine -> PerfettoSqlParser -> PerfettoSqlPreprocessor
//
// The responsibility of each of these classes is as follows:
//
// * PerfettoSqlEngine: this class is responsible for the end-to-end processing
//   of statements. It calls into PerfettoSqlParser to incrementally receive
//   parsed SQL statements and then executes them. If the statement is a
//   PerfettoSQL-only statement, the execution happens entirely in this class.
//   Otherwise, if the statement is a valid SQLite statement, SQLite is called
//   into to perform the execution.
// * PerfettoSqlParser: this class is responsible for taking a chunk of SQL and
//   incrementally converting them into parsed SQL statement. The parser calls
//   into the PerfettoSqlPreprocessor to split the SQL chunk into a statement
//   and perform any macro expansion. It then tries to parse any
//   PerfettoSQL-only statements into their component parts and leaves SQLite
//   statements as-is for execution by SQLite.
// * PerfettoSqlPreprocessor: this class is responsible for taking a chunk of
//   SQL and breaking them into statements, while also expanding any macros
//   which might be present inside.
namespace perfetto::trace_processor {
namespace {

void IncrementCountForStmt(const SqliteEngine::PreparedStatement& p_stmt,
                           PerfettoSqlEngine::ExecutionStats* res) {
  res->statement_count++;

  // If the stmt is already done, it clearly didn't have any output.
  if (p_stmt.IsDone())
    return;

  sqlite3_stmt* stmt = p_stmt.sqlite_stmt();
  if (sqlite3_column_count(stmt) == 1) {
    sqlite3_value* value = sqlite3_column_value(stmt, 0);

    // If the "VOID" pointer associated to the return value is not null,
    // that means this is a function which is forced to return a value
    // (because all functions in SQLite have to) but doesn't actually
    // wait to (i.e. it wants to be treated like CREATE TABLE or similar).
    // Because of this, ignore the return value of this function.
    // See |WrapSqlFunction| for where this is set.
    if (sqlite3_value_pointer(value, "VOID") != nullptr) {
      return;
    }

    // If the statement only has a single column and that column is named
    // "suppress_query_output", treat it as a statement without output for
    // accounting purposes. This allows an escape hatch for cases where the
    // user explicitly wants to ignore functions as having output.
    if (strcmp(sqlite3_column_name(stmt, 0), "suppress_query_output") == 0) {
      return;
    }
  }

  // Otherwise, the statement has output and so increment the count.
  res->statement_count_with_output++;
}

base::Status AddTracebackIfNeeded(base::Status status,
                                  const SqlSource& source) {
  if (status.ok()) {
    return status;
  }
  if (status.GetPayload("perfetto.dev/has_traceback") == "true") {
    return status;
  }
  // Since the error is with the statement as a whole, just pass zero so the
  // traceback points to the start of the statement.
  std::string traceback = source.AsTraceback(0);
  status = base::ErrStatus("%s%s", traceback.c_str(), status.c_message());
  status.SetPayload("perfetto.dev/has_traceback", "true");
  return status;
}

// This function is used when the PerfettoSQL has been fully executed by the
// PerfettoSqlEngine and a SqlSoruce is needed for SQLite to execute.
SqlSource RewriteToDummySql(const SqlSource& source) {
  return source.RewriteAllIgnoreExisting(
      SqlSource::FromTraceProcessorImplementation("SELECT 0 WHERE 0"));
}

constexpr std::array<const char*, 3> kTokensAllowedInMacro({
    "ColumnName",
    "Expr",
    "TableOrSubquery",
});

bool IsTokenAllowedInMacro(const std::string& view) {
  std::string lower = base::ToLower(view);
  return std::any_of(kTokensAllowedInMacro.begin(), kTokensAllowedInMacro.end(),
                     [&lower](const std::string& allowed_token) {
                       return lower == base::ToLower(allowed_token);
                     });
}

std::string GetTokenNamesAllowedInMacro() {
  std::vector<std::string> result;
  result.reserve(kTokensAllowedInMacro.size());
  for (const char* token : kTokensAllowedInMacro) {
    result.emplace_back(token);
  }
  return base::Join(result, ", ");
}

}  // namespace

PerfettoSqlEngine::PerfettoSqlEngine(StringPool* pool)
    : pool_(pool), engine_(new SqliteEngine()) {
  // Initialize `perfetto_tables` table, which will contain the names of all of
  // the registered tables.
  char* errmsg_raw = nullptr;
  int err =
      sqlite3_exec(engine_->db(), "CREATE TABLE perfetto_tables(name STRING);",
                   nullptr, nullptr, &errmsg_raw);
  ScopedSqliteString errmsg(errmsg_raw);
  if (err != SQLITE_OK) {
    PERFETTO_FATAL("Failed to initialize perfetto_tables: %s", errmsg_raw);
  }

  {
    auto ctx = std::make_unique<RuntimeTableFunctionModule::Context>();
    runtime_table_fn_context_ = ctx.get();
    engine_->RegisterVirtualTableModule<RuntimeTableFunctionModule>(
        "runtime_table_function", std::move(ctx));
  }
  {
    auto ctx = std::make_unique<DbSqliteModule::Context>();
    runtime_table_context_ = ctx.get();
    engine_->RegisterVirtualTableModule<DbSqliteModule>("runtime_table",
                                                        std::move(ctx));
  }
  {
    auto ctx = std::make_unique<DbSqliteModule::Context>();
    static_table_context_ = ctx.get();
    engine_->RegisterVirtualTableModule<DbSqliteModule>("static_table",
                                                        std::move(ctx));
  }
  {
    auto ctx = std::make_unique<DbSqliteModule::Context>();
    static_table_fn_context_ = ctx.get();
    engine_->RegisterVirtualTableModule<DbSqliteModule>("static_table_function",
                                                        std::move(ctx));
  }
}

void PerfettoSqlEngine::RegisterStaticTable(Table* table,
                                            const std::string& table_name,
                                            Table::Schema schema) {
  // Make sure we didn't accidentally leak a state from a previous table
  // creation.
  PERFETTO_CHECK(!static_table_context_->temporary_create_state);
  static_table_context_->temporary_create_state =
      std::make_unique<DbSqliteModule::State>(table, std::move(schema));

  base::StackString<1024> sql(
      R"(
        CREATE VIRTUAL TABLE %s USING static_table;
        INSERT INTO perfetto_tables(name) VALUES('%s');
      )",
      table_name.c_str(), table_name.c_str());
  auto status =
      Execute(SqlSource::FromTraceProcessorImplementation(sql.ToStdString()));
  if (!status.ok()) {
    PERFETTO_FATAL("%s", status.status().c_message());
  }

  PERFETTO_CHECK(!static_table_context_->temporary_create_state);
}

void PerfettoSqlEngine::RegisterStaticTableFunction(
    std::unique_ptr<StaticTableFunction> fn) {
  std::string name = fn->TableName();

  // Make sure we didn't accidentally leak a state from a previous table
  // creation.
  PERFETTO_CHECK(!static_table_fn_context_->temporary_create_state);
  static_table_fn_context_->temporary_create_state =
      std::make_unique<DbSqliteModule::State>(std::move(fn));

  base::StackString<1024> sql(
      "CREATE VIRTUAL TABLE %s USING static_table_function;", name.c_str());
  auto status =
      Execute(SqlSource::FromTraceProcessorImplementation(sql.ToStdString()));
  if (!status.ok()) {
    PERFETTO_FATAL("%s", status.status().c_message());
  }

  PERFETTO_CHECK(!static_table_fn_context_->temporary_create_state);
}

base::StatusOr<PerfettoSqlEngine::ExecutionStats> PerfettoSqlEngine::Execute(
    SqlSource sql) {
  auto res = ExecuteUntilLastStatement(std::move(sql));
  RETURN_IF_ERROR(res.status());
  if (res->stmt.IsDone()) {
    return res->stats;
  }
  while (res->stmt.Step()) {
  }
  RETURN_IF_ERROR(res->stmt.status());
  return res->stats;
}

base::StatusOr<PerfettoSqlEngine::ExecutionResult>
PerfettoSqlEngine::ExecuteUntilLastStatement(SqlSource sql_source) {
  // A SQL string can contain several statements. Some of them might be comment
  // only, e.g. "SELECT 1; /* comment */; SELECT 2;". Some statements can also
  // be PerfettoSQL statements which we need to transpile before execution or
  // execute without delegating to SQLite.
  //
  // The logic here is the following:
  //  - We parse the statement as a PerfettoSQL statement.
  //  - If the statement is something we can execute, execute it instantly and
  //    prepare a dummy SQLite statement so the rest of the code continues to
  //    work correctly.
  //  - If the statement is actually an SQLite statement, we invoke PrepareStmt.
  //  - We step once to make sure side effects take effect (e.g. for CREATE
  //    TABLE statements, tables are created).
  //  - If we encounter a valid statement afterwards, we step internally through
  //    all rows of the previous one. This ensures that any further side effects
  //    take hold *before* we step into the next statement.
  //  - Once no further statements are encountered, we return the prepared
  //    statement for the last valid statement.
  std::optional<SqliteEngine::PreparedStatement> res;
  ExecutionStats stats;
  PerfettoSqlParser parser(std::move(sql_source), macros_);
  while (parser.Next()) {
    std::optional<SqlSource> source;
    if (auto* cf = std::get_if<PerfettoSqlParser::CreateFunction>(
            &parser.statement())) {
      RETURN_IF_ERROR(AddTracebackIfNeeded(ExecuteCreateFunction(*cf),
                                           parser.statement_sql()));
      source = RewriteToDummySql(parser.statement_sql());
    } else if (auto* cst = std::get_if<PerfettoSqlParser::CreateTable>(
                   &parser.statement())) {
      RETURN_IF_ERROR(AddTracebackIfNeeded(ExecuteCreateTable(*cst),
                                           parser.statement_sql()));
      source = RewriteToDummySql(parser.statement_sql());
    } else if (auto* create_view = std::get_if<PerfettoSqlParser::CreateView>(
                   &parser.statement())) {
      RETURN_IF_ERROR(AddTracebackIfNeeded(ExecuteCreateView(*create_view),
                                           parser.statement_sql()));
      source = RewriteToDummySql(parser.statement_sql());
    } else if (auto* include = std::get_if<PerfettoSqlParser::Include>(
                   &parser.statement())) {
      RETURN_IF_ERROR(ExecuteInclude(*include, parser));
      source = RewriteToDummySql(parser.statement_sql());
    } else if (auto* macro = std::get_if<PerfettoSqlParser::CreateMacro>(
                   &parser.statement())) {
      auto sql = macro->sql;
      RETURN_IF_ERROR(ExecuteCreateMacro(*macro));
      source = RewriteToDummySql(sql);
    } else if (auto* index = std::get_if<PerfettoSqlParser::CreateIndex>(
                   &parser.statement())) {
      // TODO(mayzner): Enable.
      base::ignore_result(index);
      return base::ErrStatus("CREATE PERFETTO INDEX not implemented");
      // source = RewriteToDummySql(parser.statement_sql());
    } else {
      // If none of the above matched, this must just be an SQL statement
      // directly executable by SQLite.
      auto* sql =
          std::get_if<PerfettoSqlParser::SqliteSql>(&parser.statement());
      PERFETTO_CHECK(sql);
      source = parser.statement_sql();
    }

    // Try to get SQLite to prepare the statement.
    std::optional<SqliteEngine::PreparedStatement> cur_stmt;
    {
      PERFETTO_TP_TRACE(metatrace::Category::QUERY_TIMELINE, "QUERY_PREPARE");
      auto stmt = engine_->PrepareStatement(std::move(*source));
      RETURN_IF_ERROR(stmt.status());
      cur_stmt = std::move(stmt);
    }

    // The only situation where we'd have an ok status but also no prepared
    // statement is if the SQL was a pure comment. However, the PerfettoSQL
    // parser should filter out such statements so this should never happen.
    PERFETTO_DCHECK(cur_stmt->sqlite_stmt());

    // Before stepping into |cur_stmt|, we need to finish iterating through
    // the previous statement so we don't have two clashing statements (e.g.
    // SELECT * FROM v and DROP VIEW v) partially stepped into.
    if (res && !res->IsDone()) {
      PERFETTO_TP_TRACE(metatrace::Category::QUERY_TIMELINE,
                        "STMT_STEP_UNTIL_DONE",
                        [&res](metatrace::Record* record) {
                          record->AddArg("Original SQL", res->original_sql());
                          record->AddArg("Executed SQL", res->sql());
                        });
      while (res->Step()) {
      }
      RETURN_IF_ERROR(res->status());
    }

    // Propogate the current statement to the next iteration.
    res = std::move(cur_stmt);

    // Step the newly prepared statement once. This is considered to be
    // "executing" the statement.
    {
      PERFETTO_TP_TRACE(metatrace::Category::QUERY_TIMELINE, "STMT_FIRST_STEP",
                        [&res](metatrace::Record* record) {
                          record->AddArg("Original SQL", res->original_sql());
                          record->AddArg("Executed SQL", res->sql());
                        });
      PERFETTO_DLOG("Executing statement");
      PERFETTO_DLOG("Original SQL: %s", res->original_sql());
      PERFETTO_DLOG("Executed SQL: %s", res->sql());
      res->Step();
      RETURN_IF_ERROR(res->status());
    }

    // Increment the neecessary counts for the statement.
    IncrementCountForStmt(*res, &stats);
  }
  RETURN_IF_ERROR(parser.status());

  // If we didn't manage to prepare a single statement, that means everything
  // in the SQL was treated as a comment.
  if (!res)
    return base::ErrStatus("No valid SQL to run");

  // Update the output statement and column count.
  stats.column_count =
      static_cast<uint32_t>(sqlite3_column_count(res->sqlite_stmt()));
  return ExecutionResult{std::move(*res), stats};
}

base::Status PerfettoSqlEngine::RegisterRuntimeFunction(
    bool replace,
    const FunctionPrototype& prototype,
    const std::string& return_type_str,
    SqlSource sql) {
  // Parse the return type into a enum format.
  auto opt_return_type =
      sql_argument::ParseType(base::StringView(return_type_str));
  if (!opt_return_type) {
    return base::ErrStatus(
        "CREATE PERFETTO FUNCTION[prototype=%s, return=%s]: unknown return "
        "type specified",
        prototype.ToString().c_str(), return_type_str.c_str());
  }

  int created_argc = static_cast<int>(prototype.arguments.size());
  auto* ctx = static_cast<CreatedFunction::Context*>(
      sqlite_engine()->GetFunctionContext(prototype.function_name,
                                          created_argc));
  if (ctx) {
    if (CreatedFunction::IsValid(ctx) && !replace) {
      return base::ErrStatus(
          "CREATE PERFETTO FUNCTION[prototype=%s]: function already exists",
          prototype.ToString().c_str());
    }
    CreatedFunction::Reset(ctx, this);
  } else {
    // We register the function with SQLite before we prepare the statement so
    // the statement can reference the function itself, enabling recursive
    // calls.
    std::unique_ptr<CreatedFunction::Context> created_fn_ctx =
        CreatedFunction::MakeContext(this);
    ctx = created_fn_ctx.get();
    RETURN_IF_ERROR(RegisterFunctionWithSqlite<CreatedFunction>(
        prototype.function_name.c_str(), created_argc,
        std::move(created_fn_ctx)));
    runtime_function_count_++;
  }
  return CreatedFunction::Prepare(ctx, prototype, *opt_return_type,
                                  std::move(sql));
}

base::Status PerfettoSqlEngine::ExecuteCreateTable(
    const PerfettoSqlParser::CreateTable& create_table) {
  PERFETTO_TP_TRACE(metatrace::Category::QUERY_TIMELINE,
                    "CREATE_PERFETTO_TABLE",
                    [&create_table](metatrace::Record* record) {
                      record->AddArg("Table", create_table.name);
                    });
  auto stmt_or = engine_->PrepareStatement(create_table.sql);
  RETURN_IF_ERROR(stmt_or.status());
  SqliteEngine::PreparedStatement stmt = std::move(stmt_or);

  base::StatusOr<std::vector<std::string>> maybe_column_names =
      GetColumnNamesFromSelectStatement(stmt, "CREATE PERFETTO TABLE");
  RETURN_IF_ERROR(maybe_column_names.status());
  std::vector<std::string> column_names = *maybe_column_names;

  RETURN_IF_ERROR(ValidateColumnNames(column_names, create_table.schema,
                                      "CREATE PERFETTO TABLE"));

  size_t column_count = column_names.size();
  RuntimeTable::Builder builder(pool_, std::move(column_names));
  uint32_t rows = 0;
  int res;
  for (res = sqlite3_step(stmt.sqlite_stmt()); res == SQLITE_ROW;
       ++rows, res = sqlite3_step(stmt.sqlite_stmt())) {
    for (uint32_t i = 0; i < column_count; ++i) {
      int int_i = static_cast<int>(i);
      switch (sqlite3_column_type(stmt.sqlite_stmt(), int_i)) {
        case SQLITE_NULL:
          RETURN_IF_ERROR(builder.AddNull(i));
          break;
        case SQLITE_INTEGER:
          RETURN_IF_ERROR(builder.AddInteger(
              i, sqlite3_column_int64(stmt.sqlite_stmt(), int_i)));
          break;
        case SQLITE_FLOAT:
          RETURN_IF_ERROR(builder.AddFloat(
              i, sqlite3_column_double(stmt.sqlite_stmt(), int_i)));
          break;
        case SQLITE_TEXT: {
          RETURN_IF_ERROR(builder.AddText(
              i, reinterpret_cast<const char*>(
                     sqlite3_column_text(stmt.sqlite_stmt(), int_i))));
          break;
        }
        case SQLITE_BLOB:
          return base::ErrStatus(
              "CREATE PERFETTO TABLE on column '%s' in table '%s': bytes "
              "columns are not supported",
              sqlite3_column_name(stmt.sqlite_stmt(), int_i),
              create_table.name.c_str());
      }
    }
  }
  if (res != SQLITE_DONE) {
    return base::ErrStatus("%s: SQLite error while creating table body: %s",
                           create_table.name.c_str(),
                           sqlite3_errmsg(engine_->db()));
  }
  ASSIGN_OR_RETURN(auto table, std::move(builder).Build(rows));

  // TODO(lalitm): unfortunately, in the (very unlikely) event that there is a
  // sqlite3_interrupt call between the DROP and CREATE, we can end up with the
  // non-atomic query execution. Fixing this is extremely difficult as it
  // involves telling SQLite that we want the drop/create to be atomic.
  //
  // We would need to do with the transaction API but given we have no usage of
  // this until now, investigating that needs some proper work.
  if (create_table.replace) {
    base::StackString<1024> drop("DROP TABLE IF EXISTS %s",
                                 create_table.name.c_str());
    auto drop_res = Execute(
        SqlSource::FromTraceProcessorImplementation(drop.ToStdString()));
    RETURN_IF_ERROR(drop_res.status());
  }

  base::StackString<1024> create("CREATE VIRTUAL TABLE %s USING runtime_table",
                                 create_table.name.c_str());

  // Make sure we didn't accidentally leak a state from a previous function
  // creation.
  PERFETTO_CHECK(!runtime_table_context_->temporary_create_state);

  // Move the state into the context so that it will be picked up in xCreate
  // of RuntimeTableFunctionModule.
  runtime_table_context_->temporary_create_state =
      std::make_unique<DbSqliteModule::State>(std::move(table));
  auto status =
      Execute(SqlSource::FromTraceProcessorImplementation(create.ToStdString()))
          .status();

  // If an error happened, it's possible that the state was not picked up.
  // Therefore, always reset the state just in case. OTOH if the creation
  // succeeded, the state should always have been captured.
  if (status.ok()) {
    PERFETTO_CHECK(!runtime_table_context_->temporary_create_state);
  } else {
    runtime_table_context_->temporary_create_state.reset();
  }
  return status;
}

base::Status PerfettoSqlEngine::ExecuteCreateView(
    const PerfettoSqlParser::CreateView& create_view) {
  // Verify that the underlying SQL statement is valid.
  auto stmt = sqlite_engine()->PrepareStatement(create_view.select_sql);
  RETURN_IF_ERROR(stmt.status());

  if (create_view.replace) {
    base::StackString<1024> drop_if_exists("DROP VIEW IF EXISTS %s",
                                           create_view.name.c_str());
    RETURN_IF_ERROR(Execute(SqlSource::FromTraceProcessorImplementation(
                                drop_if_exists.ToStdString()))
                        .status());
  }

  // If the schema is specified, verify that the column names match it.
  if (!create_view.schema.empty()) {
    base::StatusOr<std::vector<std::string>> maybe_column_names =
        GetColumnNamesFromSelectStatement(stmt, "CREATE PERFETTO VIEW");
    RETURN_IF_ERROR(maybe_column_names.status());
    std::vector<std::string> column_names = *maybe_column_names;

    RETURN_IF_ERROR(ValidateColumnNames(column_names, create_view.schema,
                                        "CREATE PERFETTO VIEW"));
  }

  RETURN_IF_ERROR(Execute(create_view.create_view_sql).status());
  return base::OkStatus();
}

base::Status PerfettoSqlEngine::EnableSqlFunctionMemoization(
    const std::string& name) {
  constexpr size_t kSupportedArgCount = 1;
  auto* ctx = static_cast<CreatedFunction::Context*>(
      sqlite_engine()->GetFunctionContext(name, kSupportedArgCount));
  if (!ctx) {
    return base::ErrStatus(
        "EXPERIMENTAL_MEMOIZE: Function %s(INT) does not exist", name.c_str());
  }
  return CreatedFunction::EnableMemoization(ctx);
}

base::Status PerfettoSqlEngine::ExecuteInclude(
    const PerfettoSqlParser::Include& include,
    const PerfettoSqlParser& parser) {
  std::string key = include.key;
  PERFETTO_TP_TRACE(metatrace::Category::QUERY_TIMELINE, "Include",
                    [key](metatrace::Record* r) { r->AddArg("Module", key); });

  if (key == "*") {
    for (auto moduleIt = modules_.GetIterator(); moduleIt; ++moduleIt) {
      RETURN_IF_ERROR(IncludeModuleImpl(moduleIt.value(), key, parser));
    }
    return base::OkStatus();
  }

  std::string module_name = sql_modules::GetModuleName(key);
  auto* module = FindModule(module_name);
  if (!module) {
    return base::ErrStatus("INCLUDE: Unknown module name provided - %s",
                           key.c_str());
  }
  return IncludeModuleImpl(*module, key, parser);
}

base::Status PerfettoSqlEngine::IncludeModuleImpl(
    sql_modules::RegisteredModule& module,
    const std::string& key,
    const PerfettoSqlParser& parser) {
  if (!key.empty() && key.back() == '*') {
    // If the key ends with a wildcard, iterate through all the keys in the
    // module and include matching ones.
    std::string prefix = key.substr(0, key.size() - 1);
    for (auto fileIt = module.include_key_to_file.GetIterator(); fileIt;
         ++fileIt) {
      if (!base::StartsWith(fileIt.key(), prefix))
        continue;
      PERFETTO_TP_TRACE(
          metatrace::Category::QUERY_TIMELINE,
          "Include (expanded from wildcard)",
          [&](metatrace::Record* r) { r->AddArg("Module", fileIt.key()); });
      RETURN_IF_ERROR(IncludeFileImpl(fileIt.value(), fileIt.key(), parser));
    }
    return base::OkStatus();
  }
  auto* module_file = module.include_key_to_file.Find(key);
  if (!module_file) {
    return base::ErrStatus("INCLUDE: unknown module '%s'", key.c_str());
  }
  return IncludeFileImpl(*module_file, key, parser);
}

base::Status PerfettoSqlEngine::IncludeFileImpl(
    sql_modules::RegisteredModule::ModuleFile& file,
    const std::string& key,
    const PerfettoSqlParser& parser) {
  // INCLUDE is noop for already included files.
  if (file.included) {
    return base::OkStatus();
  }

  auto it = Execute(SqlSource::FromModuleInclude(file.sql, key));
  if (!it.status().ok()) {
    return base::ErrStatus("%s%s",
                           parser.statement_sql().AsTraceback(0).c_str(),
                           it.status().c_message());
  }
  if (it->statement_count_with_output > 0)
    return base::ErrStatus("INCLUDE: Included module returning values.");
  file.included = true;
  return base::OkStatus();
}

base::Status PerfettoSqlEngine::ExecuteCreateFunction(
    const PerfettoSqlParser::CreateFunction& cf) {
  if (!cf.is_table) {
    return RegisterRuntimeFunction(cf.replace, cf.prototype, cf.returns,
                                   cf.sql);
  }

  auto state = std::make_unique<RuntimeTableFunctionModule::State>(
      RuntimeTableFunctionModule::State{
          this, cf.sql, cf.prototype, {}, std::nullopt});

  // Parse the return type into a enum format.
  {
    base::Status status = sql_argument::ParseArgumentDefinitions(
        cf.returns, state->return_values);
    if (!status.ok()) {
      return base::ErrStatus(
          "CREATE PERFETTO FUNCTION[prototype=%s, return=%s]: unknown return "
          "type specified",
          state->prototype.ToString().c_str(), cf.returns.c_str());
    }
  }

  // Verify that the provided SQL prepares to a statement correctly.
  auto stmt = sqlite_engine()->PrepareStatement(cf.sql);
  RETURN_IF_ERROR(stmt.status());

  // Verify that every argument name in the function appears in the
  // argument list.
  //
  // We intentionally loop from 1 to |used_param_count| because SQL
  // parameters are 1-indexed *not* 0-indexed.
  int used_param_count = sqlite3_bind_parameter_count(stmt.sqlite_stmt());
  for (int i = 1; i <= used_param_count; ++i) {
    const char* name = sqlite3_bind_parameter_name(stmt.sqlite_stmt(), i);

    if (!name) {
      return base::ErrStatus(
          "%s: \"Nameless\" SQL parameters cannot be used in the SQL "
          "statements of view functions.",
          state->prototype.function_name.c_str());
    }

    if (!base::StringView(name).StartsWith("$")) {
      return base::ErrStatus(
          "%s: invalid parameter name %s used in the SQL definition of "
          "the view function: all parameters must be prefixed with '$' not "
          "':' or '@'.",
          state->prototype.function_name.c_str(), name);
    }

    auto it = std::find_if(state->prototype.arguments.begin(),
                           state->prototype.arguments.end(),
                           [name](const sql_argument::ArgumentDefinition& arg) {
                             return arg.dollar_name() == name;
                           });
    if (it == state->prototype.arguments.end()) {
      return base::ErrStatus(
          "%s: parameter %s does not appear in the list of arguments in the "
          "prototype of the view function.",
          state->prototype.function_name.c_str(), name);
    }
  }

  // Verify that the prepared statement column count matches the return
  // count.
  auto col_count =
      static_cast<uint32_t>(sqlite3_column_count(stmt.sqlite_stmt()));
  if (col_count != state->return_values.size()) {
    return base::ErrStatus(
        "%s: number of return values %u does not match SQL statement column "
        "count %zu.",
        state->prototype.function_name.c_str(), col_count,
        state->return_values.size());
  }

  // Verify that the return names matches the prepared statment column names.
  for (uint32_t i = 0; i < col_count; ++i) {
    const char* name =
        sqlite3_column_name(stmt.sqlite_stmt(), static_cast<int>(i));
    if (name != state->return_values[i].name()) {
      return base::ErrStatus(
          "%s: column %s at index %u does not match return value name %s.",
          state->prototype.function_name.c_str(), name, i,
          state->return_values[i].name().c_str());
    }
  }
  state->temporary_create_stmt = std::move(stmt);

  // TODO(lalitm): this suffers the same non-atomic DROP/CREATE problem as
  // CREATE PERFETTO TABLE implementation above: see the comment there for
  // more info on this.
  if (cf.replace) {
    base::StackString<1024> drop("DROP TABLE IF EXISTS %s",
                                 state->prototype.function_name.c_str());
    auto res = Execute(
        SqlSource::FromTraceProcessorImplementation(drop.ToStdString()));
    RETURN_IF_ERROR(res.status());
  }

  base::StackString<1024> create(
      "CREATE VIRTUAL TABLE %s USING runtime_table_function",
      state->prototype.function_name.c_str());

  // Make sure we didn't accidentally leak a state from a previous function
  // creation.
  PERFETTO_CHECK(!runtime_table_fn_context_->temporary_create_state);

  // Move the state into the context so that it will be picked up in xCreate
  // of RuntimeTableFunctionModule.
  runtime_table_fn_context_->temporary_create_state = std::move(state);
  auto status = Execute(cf.sql.RewriteAllIgnoreExisting(
                            SqlSource::FromTraceProcessorImplementation(
                                create.ToStdString())))
                    .status();

  // If an error happened, it's possible that the state was not picked up.
  // Therefore, always reset the state just in case. OTOH if the creation
  // succeeded, the state should always have been captured.
  if (status.ok()) {
    PERFETTO_CHECK(!runtime_table_fn_context_->temporary_create_state);
  } else {
    runtime_table_fn_context_->temporary_create_state.reset();
  }
  return status;
}

base::Status PerfettoSqlEngine::ExecuteCreateMacro(
    const PerfettoSqlParser::CreateMacro& create_macro) {
  // Check that the argument types is one of the allowed types.
  for (const auto& [name, type] : create_macro.args) {
    if (!IsTokenAllowedInMacro(type.sql())) {
      // TODO(lalitm): add a link to create macro documentation.
      return base::ErrStatus(
          "%sMacro '%s' argument '%s' is unknown type '%s'. Allowed types: %s",
          type.AsTraceback(0).c_str(), create_macro.name.sql().c_str(),
          name.sql().c_str(), type.sql().c_str(),
          GetTokenNamesAllowedInMacro().c_str());
    }
  }
  if (!IsTokenAllowedInMacro(create_macro.returns.sql())) {
    // TODO(lalitm): add a link to create macro documentation.
    return base::ErrStatus(
        "%sMacro %s return type %s is unknown. Allowed types: %s",
        create_macro.returns.AsTraceback(0).c_str(),
        create_macro.name.sql().c_str(), create_macro.returns.sql().c_str(),
        GetTokenNamesAllowedInMacro().c_str());
  }

  std::vector<std::string> args;
  args.reserve(create_macro.args.size());
  for (const auto& arg : create_macro.args) {
    args.push_back(arg.first.sql());
  }
  PerfettoSqlPreprocessor::Macro macro{
      create_macro.replace,
      create_macro.name.sql(),
      std::move(args),
      create_macro.sql,
  };
  if (auto* it = macros_.Find(create_macro.name.sql()); it) {
    if (!create_macro.replace) {
      // TODO(lalitm): add a link to create macro documentation.
      return base::ErrStatus("%sMacro already exists",
                             create_macro.name.AsTraceback(0).c_str());
    }
    *it = std::move(macro);
    return base::OkStatus();
  }
  std::string name = macro.name;
  auto it_and_inserted = macros_.Insert(std::move(name), std::move(macro));
  PERFETTO_CHECK(it_and_inserted.second);
  return base::OkStatus();
}

base::StatusOr<std::vector<std::string>>
PerfettoSqlEngine::GetColumnNamesFromSelectStatement(
    const SqliteEngine::PreparedStatement& stmt,
    const char* tag) {
  auto columns =
      static_cast<uint32_t>(sqlite3_column_count(stmt.sqlite_stmt()));
  std::vector<std::string> column_names;
  for (uint32_t i = 0; i < columns; ++i) {
    std::string col_name =
        sqlite3_column_name(stmt.sqlite_stmt(), static_cast<int>(i));
    if (col_name.empty()) {
      return base::ErrStatus("%s: column %d: name must not be empty", tag, i);
    }
    if (!std::isalpha(col_name.front())) {
      return base::ErrStatus(
          "%s: Column %i: name '%s' has to start with a letter.", tag, i,
          col_name.c_str());
    }
    if (!sql_argument::IsValidName(base::StringView(col_name))) {
      return base::ErrStatus(
          "%s: Column %i: name '%s' has to contain only alphanumeric "
          "characters and underscores.",
          tag, i, col_name.c_str());
    }
    column_names.push_back(col_name);
  }
  return column_names;
}

base::Status PerfettoSqlEngine::ValidateColumnNames(
    const std::vector<std::string>& column_names,
    const std::vector<sql_argument::ArgumentDefinition>& schema,
    const char* tag) {
  std::vector<std::string> duplicate_columns;
  for (auto it = column_names.begin(); it != column_names.end(); ++it) {
    if (std::count(it + 1, column_names.end(), *it) > 0) {
      duplicate_columns.push_back(*it);
    }
  }
  if (!duplicate_columns.empty()) {
    return base::ErrStatus("%s: multiple columns are named: %s", tag,
                           base::Join(duplicate_columns, ", ").c_str());
  }

  // If the user has not provided a schema, we have nothing further to validate.
  if (schema.empty()) {
    return base::OkStatus();
  }

  std::vector<std::string> columns_missing_from_query;
  std::vector<std::string> columns_missing_from_schema;

  for (const std::string& name : column_names) {
    bool present =
        std::find_if(schema.begin(), schema.end(), [&name](const auto& arg) {
          return arg.name() == base::StringView(name);
        }) != schema.end();
    if (!present) {
      columns_missing_from_schema.push_back(name);
    }
  }

  for (const auto& arg : schema) {
    bool present = std::find_if(column_names.begin(), column_names.end(),
                                [&arg](const std::string& name) {
                                  return arg.name() == base::StringView(name);
                                }) != column_names.end();
    if (!present) {
      columns_missing_from_query.push_back(arg.name().ToStdString());
    }
  }

  if (columns_missing_from_query.empty() &&
      columns_missing_from_schema.empty()) {
    return base::OkStatus();
  }

  if (columns_missing_from_query.empty()) {
    return base::ErrStatus(
        "%s: the following columns are missing from the schema: %s", tag,
        base::Join(columns_missing_from_schema, ", ").c_str());
  }

  if (columns_missing_from_schema.empty()) {
    return base::ErrStatus(
        "%s: the following columns are declared in the schema, but do not "
        "exist: %s",
        tag, base::Join(columns_missing_from_query, ", ").c_str());
  }

  return base::ErrStatus(
      "%s: the following columns are declared in the schema, but do not exist: "
      "%s; and the folowing columns exist, but are not declared: %s",
      tag, base::Join(columns_missing_from_query, ", ").c_str(),
      base::Join(columns_missing_from_schema, ", ").c_str());
}

const RuntimeTable* PerfettoSqlEngine::GetRuntimeTableOrNull(
    std::string_view name) const {
  auto* state = runtime_table_context_->manager.FindStateByName(name);
  return state ? state->runtime_table.get() : nullptr;
}

RuntimeTable* PerfettoSqlEngine::GetMutableRuntimeTableOrNull(
    std::string_view name) {
  auto* state = runtime_table_context_->manager.FindStateByName(name);
  return state ? state->runtime_table.get() : nullptr;
}

const Table* PerfettoSqlEngine::GetStaticTableOrNull(
    std::string_view name) const {
  auto* state = static_table_context_->manager.FindStateByName(name);
  return state ? state->static_table : nullptr;
}

Table* PerfettoSqlEngine::GetMutableStaticTableOrNull(std::string_view name) {
  auto* state = static_table_context_->manager.FindStateByName(name);
  return state ? state->static_table : nullptr;
}

}  // namespace perfetto::trace_processor
