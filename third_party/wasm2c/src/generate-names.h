/*
 * Copyright 2016 WebAssembly Community Group participants
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

#ifndef WABT_GENERATE_NAMES_H_
#define WABT_GENERATE_NAMES_H_

#include "src/common.h"

namespace wabt {

struct Module;

enum NameOpts {
  None = 0,
  AlphaNames = 1 << 0,
};

Result GenerateNames(struct Module*, NameOpts opts = NameOpts::None);

inline std::string IndexToAlphaName(Index index) {
  std::string s;
  do {
    // For multiple chars, put most frequently changing char first.
    s += 'a' + (index % 26);
    index /= 26;
    // Continue remaining sequence with 'a' rather than 'b'.
  } while (index--);
  return s;
}

}  // namespace wabt

#endif /* WABT_GENERATE_NAMES_H_ */
