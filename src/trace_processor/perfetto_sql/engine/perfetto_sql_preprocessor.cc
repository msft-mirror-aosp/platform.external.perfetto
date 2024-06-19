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

#include "src/trace_processor/perfetto_sql/engine/perfetto_sql_preprocessor.h"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "perfetto/base/logging.h"
#include "perfetto/base/status.h"
#include "perfetto/ext/base/flat_hash_map.h"
#include "perfetto/ext/base/status_or.h"
#include "perfetto/ext/base/string_utils.h"
#include "src/trace_processor/sqlite/sql_source.h"
#include "src/trace_processor/sqlite/sqlite_tokenizer.h"
#include "src/trace_processor/util/status_macros.h"

namespace perfetto::trace_processor {
namespace {

base::Status ErrorAtToken(const SqliteTokenizer& tokenizer,
                          const SqliteTokenizer::Token& token,
                          const char* error) {
  std::string traceback = tokenizer.AsTraceback(token);
  return base::ErrStatus("%s%s", traceback.c_str(), error);
}

struct InvocationArg {
  std::optional<SqlSource> arg;
  bool has_more;
};

base::StatusOr<InvocationArg> ParseMacroInvocationArg(
    SqliteTokenizer& tokenizer,
    SqliteTokenizer::Token& tok,
    bool has_prev_args) {
  uint32_t nested_parens = 0;
  bool seen_token_in_arg = false;
  auto start = tokenizer.NextNonWhitespace();
  for (tok = start;; tok = tokenizer.NextNonWhitespace()) {
    if (tok.IsTerminal()) {
      if (tok.token_type == SqliteTokenType::TK_SEMI) {
        // TODO(b/290185551): add a link to macro documentation.
        return ErrorAtToken(tokenizer, tok,
                            "Semi-colon is not allowed in macro invocation");
      }
      // TODO(b/290185551): add a link to macro documentation.
      return ErrorAtToken(tokenizer, tok, "Macro invocation not complete");
    }

    bool is_arg_terminator = tok.token_type == SqliteTokenType::TK_RP ||
                             tok.token_type == SqliteTokenType::TK_COMMA;
    if (nested_parens == 0 && is_arg_terminator) {
      bool token_required =
          has_prev_args || tok.token_type != SqliteTokenType::TK_RP;
      if (!seen_token_in_arg && token_required) {
        // TODO(b/290185551): add a link to macro documentation.
        return ErrorAtToken(tokenizer, tok, "Macro arg is empty");
      }
      return InvocationArg{
          seen_token_in_arg ? std::make_optional(tokenizer.Substr(start, tok))
                            : std::optional<SqlSource>(std::nullopt),
          tok.token_type == SqliteTokenType::TK_COMMA,
      };
    }
    seen_token_in_arg = true;

    if (tok.token_type == SqliteTokenType::TK_LP) {
      nested_parens++;
      continue;
    }
    if (tok.token_type == SqliteTokenType::TK_RP) {
      nested_parens--;
      continue;
    }
  }
}

base::StatusOr<SqlSource> ExecuteStringify(
    SqliteTokenizer& tokenizer,
    SqliteTokenizer::Token& tok,
    const SqliteTokenizer::Token& name_token) {
  ASSIGN_OR_RETURN(InvocationArg invocation_arg,
                   ParseMacroInvocationArg(tokenizer, tok, false));
  if (!invocation_arg.arg || invocation_arg.has_more) {
    return ErrorAtToken(tokenizer, name_token,
                        "stringify: stringify must have exactly one argument");
  }
  std::string res = "'";
  res.append(invocation_arg.arg->sql());
  res.append("'");
  return invocation_arg.arg->RewriteAllIgnoreExisting(
      SqlSource::FromTraceProcessorImplementation(std::move(res)));
}

}  // namespace

PerfettoSqlPreprocessor::PerfettoSqlPreprocessor(
    SqlSource source,
    const base::FlatHashMap<std::string, Macro>& macros)
    : global_tokenizer_(std::move(source)), macros_(&macros) {}

bool PerfettoSqlPreprocessor::NextStatement() {
  PERFETTO_CHECK(status_.ok());

  // Skip through any number of semi-colons (representing empty statements).
  SqliteTokenizer::Token tok = global_tokenizer_.NextNonWhitespace();
  while (tok.token_type == SqliteTokenType::TK_SEMI) {
    tok = global_tokenizer_.NextNonWhitespace();
  }

  // If we still see a terminal token at this point, we must have hit EOF.
  if (tok.IsTerminal()) {
    PERFETTO_DCHECK(tok.token_type != SqliteTokenType::TK_SEMI);
    return false;
  }

  SqlSource stmt =
      global_tokenizer_.Substr(tok, global_tokenizer_.NextTerminal());
  auto stmt_or = RewriteInternal(stmt, {});
  if (stmt_or.ok()) {
    statement_ = std::move(*stmt_or);
    return true;
  }
  status_ = stmt_or.status();
  return false;
}

base::StatusOr<SqlSource> PerfettoSqlPreprocessor::RewriteInternal(
    const SqlSource& source,
    const std::unordered_map<std::string, SqlSource>& arg_bindings) {
  SqlSource::Rewriter rewriter(source);
  SqliteTokenizer tokenizer(source);
  for (SqliteTokenizer::Token tok = tokenizer.NextNonWhitespace(), prev;;
       prev = tok, tok = tokenizer.NextNonWhitespace()) {
    if (tok.IsTerminal()) {
      break;
    }
    if (tok.token_type == SqliteTokenType::TK_VARIABLE &&
        !seen_macros_.empty()) {
      PERFETTO_CHECK(tok.str.size() >= 2);
      if (tok.str[0] != '$') {
        return ErrorAtToken(tokenizer, tok, "Variables must start with $");
      }
      auto binding_it = arg_bindings.find(std::string(tok.str.substr(1)));
      if (binding_it == arg_bindings.end()) {
        return ErrorAtToken(tokenizer, tok, "Variable not found");
      }
      tokenizer.RewriteToken(rewriter, tok, binding_it->second);
      continue;
    }
    if (tok.token_type != SqliteTokenType::TK_ILLEGAL || tok.str != "!") {
      continue;
    }

    const auto& name_token = prev;
    if (name_token.token_type == SqliteTokenType::TK_VARIABLE) {
      // TODO(b/290185551): add a link to macro documentation.
      return ErrorAtToken(tokenizer, name_token,
                          "Macro name cannot contain a variable");
    }
    if (name_token.token_type != SqliteTokenType::TK_ID) {
      // TODO(b/290185551): add a link to macro documentation.
      return ErrorAtToken(tokenizer, name_token, "Macro invocation is invalid");
    }

    // Get the opening left parenthesis.
    tok = tokenizer.NextNonWhitespace();
    if (tok.token_type != SqliteTokenType::TK_LP) {
      // TODO(b/290185551): add a link to macro documentation.
      return ErrorAtToken(tokenizer, tok,
                          "( expected to open macro invocation");
    }

    std::string macro_name(name_token.str);
    if (macro_name == "__intrinsic_stringify") {
      ASSIGN_OR_RETURN(SqlSource res,
                       ExecuteStringify(tokenizer, tok, name_token));
      tokenizer.Rewrite(rewriter, prev, tok, std::move(res),
                        SqliteTokenizer::EndToken::kInclusive);
      continue;
    }

    ASSIGN_OR_RETURN(
        MacroInvocation invocation,
        ParseMacroInvocation(tokenizer, tok, prev, macro_name, arg_bindings));
    const Macro* m = invocation.macro;
    seen_macros_.emplace(m->name);
    ASSIGN_OR_RETURN(SqlSource res,
                     RewriteInternal(m->sql, invocation.arg_bindings));
    seen_macros_.erase(m->name);
    tokenizer.Rewrite(rewriter, prev, tok, std::move(res),
                      SqliteTokenizer::EndToken::kInclusive);
  }
  return std::move(rewriter).Build();
}

base::StatusOr<PerfettoSqlPreprocessor::MacroInvocation>
PerfettoSqlPreprocessor::ParseMacroInvocation(
    SqliteTokenizer& tokenizer,
    SqliteTokenizer::Token& tok,
    const SqliteTokenizer::Token& name_token,
    const std::string& macro_name,
    const std::unordered_map<std::string, SqlSource>& arg_bindings) {
  Macro* macro = macros_->Find(macro_name);
  if (!macro) {
    // TODO(b/290185551): add a link to macro documentation.
    base::StackString<1024> err("Macro %s does not exist", macro_name.c_str());
    return ErrorAtToken(tokenizer, name_token, err.c_str());
  }

  if (seen_macros_.count(macro_name)) {
    // TODO(b/290185551): add a link to macro documentation.
    return ErrorAtToken(tokenizer, name_token,
                        "Macros cannot be recursive or mutually recursive");
  }

  std::unordered_map<std::string, SqlSource> inner_bindings;
  for (bool has_more = true; has_more;) {
    ASSIGN_OR_RETURN(
        InvocationArg invocation_arg,
        ParseMacroInvocationArg(tokenizer, tok, !inner_bindings.empty()));
    if (invocation_arg.arg) {
      ASSIGN_OR_RETURN(
          SqlSource res,
          RewriteInternal(invocation_arg.arg.value(), arg_bindings));
      if (macro->args.size() <= inner_bindings.size()) {
        // TODO(lalitm): add a link to macro documentation.
        return ErrorAtToken(tokenizer, name_token,
                            "Macro invoked with too many args");
      }
      inner_bindings.emplace(macro->args[inner_bindings.size()],
                             std::move(res));
    }
    has_more = invocation_arg.has_more;
  }

  if (inner_bindings.size() < macro->args.size()) {
    // TODO(lalitm): add a link to macro documentation.
    return ErrorAtToken(tokenizer, name_token,
                        "Macro invoked with too few args");
  }
  PERFETTO_CHECK(inner_bindings.size() == macro->args.size());
  return MacroInvocation{macro, std::move(inner_bindings)};
}

}  // namespace perfetto::trace_processor
