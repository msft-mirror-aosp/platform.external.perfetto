/*
 * Copyright (C) 2024 The Android Open Source Project
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

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "perfetto/ext/base/file_utils.h"
#include "perfetto/ext/base/temp_file.h"
#include "src/base/test/utils.h"
#include "src/trace_redaction/find_package_uid.h"
#include "src/trace_redaction/prune_package_list.h"
#include "src/trace_redaction/trace_redactor.h"
#include "test/gtest_and_gmock.h"

#include "protos/perfetto/trace/android/packages_list.pbzero.h"
#include "protos/perfetto/trace/trace.pbzero.h"

namespace perfetto::trace_redaction {

namespace {
using PackagesList = protos::pbzero::PackagesList;
using PackageInfo = protos::pbzero::PackagesList::PackageInfo;
using Trace = protos::pbzero::Trace;
using TracePacket = protos::pbzero::TracePacket;

constexpr std::string_view kTracePath =
    "test/data/trace-redaction-general.pftrace";

constexpr std::string_view kPackageName =
    "com.Unity.com.unity.multiplayer.samples.coop";

constexpr uint64_t kPackageUid = 10252;

class TraceRedactorIntegrationTest : public testing::Test {
 public:
  TraceRedactorIntegrationTest() = default;
  ~TraceRedactorIntegrationTest() override = default;

 protected:
  void SetUp() override {
    src_trace_ = base::GetTestDataPath(std::string(kTracePath));
    dest_trace_ = std::make_unique<base::TempFile>(base::TempFile::Create());
  }

  const std::string& src_trace() const { return src_trace_; }

  const std::string& dest_trace() const { return dest_trace_->path(); }

  std::vector<protozero::ConstBytes> GetPackageInfos(
      const Trace::Decoder& trace) const {
    std::vector<protozero::ConstBytes> infos;

    for (auto packet_it = trace.packet(); packet_it; ++packet_it) {
      TracePacket::Decoder packet_decoder(*packet_it);
      if (packet_decoder.has_packages_list()) {
        PackagesList::Decoder list_it(packet_decoder.packages_list());
        for (auto info_it = list_it.packages(); info_it; ++info_it) {
          PackageInfo::Decoder info(*info_it);
          infos.push_back(*info_it);
        }
      }
    }

    return infos;
  }

 private:
  std::string src_trace_;
  std::unique_ptr<base::TempFile> dest_trace_;
};

TEST_F(TraceRedactorIntegrationTest, FindsPackageAndFiltersPackageList) {
  TraceRedactor redaction;
  redaction.collectors()->emplace_back(new FindPackageUid());
  redaction.transformers()->emplace_back(new PrunePackageList());

  Context context;
  context.package_name = kPackageName;

  auto result = redaction.Redact(src_trace(), dest_trace(), &context);

  ASSERT_TRUE(result.ok()) << result.message();

  std::string redacted_buffer;
  ASSERT_TRUE(base::ReadFile(dest_trace(), &redacted_buffer));

  Trace::Decoder redacted_trace(redacted_buffer);
  std::vector<protozero::ConstBytes> infos = GetPackageInfos(redacted_trace);

  // It is possible for two packages_list to appear in the trace. The
  // find_package_uid will stop after the first one is found. Package uids are
  // appear as n * 1,000,000 where n is some integer. It is also possible for
  // two packages_list to contain copies of each other - for example
  // "com.Unity.com.unity.multiplayer.samples.coop" appears in both
  // packages_list.
  ASSERT_GE(infos.size(), 1u);

  for (const auto& info_buffer : infos) {
    PackageInfo::Decoder info(info_buffer);

    ASSERT_TRUE(info.has_name());
    ASSERT_EQ(info.name().ToStdString(), kPackageName);

    ASSERT_TRUE(info.has_uid());
    ASSERT_EQ(NormalizeUid(info.uid()), NormalizeUid(kPackageUid));
  }

  ASSERT_TRUE(context.package_uid.has_value());
  ASSERT_EQ(NormalizeUid(context.package_uid.value()),
            NormalizeUid(kPackageUid));
}

}  // namespace
}  // namespace perfetto::trace_redaction
