/*
 * Copyright (C) 2020 The Android Open Source Project
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

#include "src/trace_processor/util/gzip_utils.h"

// For bazel build.
#include "perfetto/base/build_config.h"
#include "perfetto/base/compiler.h"

#if PERFETTO_BUILDFLAG(PERFETTO_ZLIB)
#include <zlib.h>
#else
struct z_stream_s {};
#endif

namespace perfetto {
namespace trace_processor {
namespace util {

bool IsGzipSupported() {
#if PERFETTO_BUILDFLAG(PERFETTO_ZLIB)
  return true;
#else
  return false;
#endif
}

#if PERFETTO_BUILDFLAG(PERFETTO_ZLIB)  // Real Implementation

GzipDecompressor::GzipDecompressor() : z_stream_(new z_stream()) {
  z_stream_->zalloc = nullptr;
  z_stream_->zfree = nullptr;
  z_stream_->opaque = nullptr;
  inflateInit2(z_stream_.get(), 32 + MAX_WBITS);
}

GzipDecompressor::~GzipDecompressor() {
  inflateEnd(z_stream_.get());
}

void GzipDecompressor::Reset() {
  inflateReset(z_stream_.get());
}

void GzipDecompressor::Feed(const uint8_t* data, size_t size) {
  // This const_cast is not harmfull as zlib will not modify the data in this
  // pointer. This is only necessary because of the build flags we use to be
  // compatible with other embedders.
  z_stream_->next_in = const_cast<uint8_t*>(data);
  z_stream_->avail_in = static_cast<uInt>(size);
}

GzipDecompressor::Result GzipDecompressor::ExtractOutput(uint8_t* out,
                                                         size_t out_size) {
  if (z_stream_->avail_in == 0)
    return Result{ResultCode::kNeedsMoreInput, 0};

  z_stream_->next_out = out;
  z_stream_->avail_out = static_cast<uInt>(out_size);

  int ret = inflate(z_stream_.get(), Z_NO_FLUSH);
  switch (ret) {
    case Z_NEED_DICT:
    case Z_DATA_ERROR:
    case Z_MEM_ERROR:
      // Ignore inflateEnd error as we will error out anyway.
      inflateEnd(z_stream_.get());
      return Result{ResultCode::kError, 0};
    case Z_STREAM_END:
      return Result{ResultCode::kEof, out_size - z_stream_->avail_out};
    case Z_BUF_ERROR:
      return Result{ResultCode::kNeedsMoreInput, 0};
    default:
      return Result{ResultCode::kOk, out_size - z_stream_->avail_out};
  }
}

#else  // Dummy Implementation

GzipDecompressor::GzipDecompressor() = default;
GzipDecompressor::~GzipDecompressor() = default;
void GzipDecompressor::Reset() {}
void GzipDecompressor::Feed(const uint8_t*, size_t) {}
GzipDecompressor::Result GzipDecompressor::ExtractOutput(uint8_t*, size_t) {
  return Result{ResultCode::kError, 0};
}

#endif  // PERFETTO_BUILDFLAG(PERFETTO_ZLIB)

// static
std::vector<uint8_t> GzipDecompressor::DecompressFully(const uint8_t* data,
                                                       size_t len) {
  std::vector<uint8_t> whole_data;
  GzipDecompressor decompressor;
  auto decom_output_consumer = [&](const uint8_t* buf, size_t buf_len) {
    whole_data.insert(whole_data.end(), buf, buf + buf_len);
  };
  decompressor.FeedAndExtract(data, len, decom_output_consumer);
  return whole_data;
}

}  // namespace util
}  // namespace trace_processor
}  // namespace perfetto
