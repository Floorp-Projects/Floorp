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

#ifndef WABT_LEXER_SOURCE_H_
#define WABT_LEXER_SOURCE_H_

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "wabt/common.h"
#include "wabt/range.h"

namespace wabt {

class LexerSource {
 public:
  LexerSource(const void* data, Offset size);

  std::unique_ptr<LexerSource> Clone();
  Result Tell(Offset* out_offset);
  size_t Fill(void* dest, size_t size);
  Result ReadRange(OffsetRange, std::vector<char>* out_data);
  Result Seek(Offset offset);

  WABT_DISALLOW_COPY_AND_ASSIGN(LexerSource);

  const void* data() { return data_; }
  Offset size() { return size_; }

 private:
  const void* data_;
  Offset size_;
  Offset read_offset_;
};

}  // namespace wabt

#endif  // WABT_LEXER_SOURCE_H_
