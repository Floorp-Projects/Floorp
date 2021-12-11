/*
 * Copyright 2017 WebAssembly Community Group participants
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "src/lexer-source.h"

#include <algorithm>

namespace wabt {

LexerSource::LexerSource(const void* data, Offset size)
    : data_(data), size_(size), read_offset_(0) {}

std::unique_ptr<LexerSource> LexerSource::Clone() {
  LexerSource* result = new LexerSource(data_, size_);
  result->read_offset_ = read_offset_;
  return std::unique_ptr<LexerSource>(result);
}

Result LexerSource::Tell(Offset* out_offset) {
  *out_offset = read_offset_;
  return Result::Ok;
}

size_t LexerSource::Fill(void* dest, Offset size) {
  Offset read_size = std::min(size, size_ - read_offset_);
  if (read_size > 0) {
    const void* src = static_cast<const char*>(data_) + read_offset_;
    memcpy(dest, src, read_size);
    read_offset_ += read_size;
  }
  return read_size;
}

Result LexerSource::Seek(Offset offset) {
  if (offset < size_) {
    read_offset_ = offset;
    return Result::Ok;
  }
  return Result::Error;
}

Result LexerSource::ReadRange(OffsetRange range, std::vector<char>* out_data) {
  OffsetRange clamped = range;
  clamped.start = std::min(clamped.start, size_);
  clamped.end = std::min(clamped.end, size_);
  if (clamped.size()) {
    out_data->resize(clamped.size());
    const void* src = static_cast<const char*>(data_) + clamped.start;
    memcpy(out_data->data(), src, clamped.size());
  }
  return Result::Ok;
}

}  // namespace wabt
