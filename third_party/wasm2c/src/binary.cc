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

#include "wabt/binary.h"

namespace wabt {

BinarySectionOrder GetSectionOrder(BinarySection sec) {
  switch (sec) {
#define V(Name, name, code) \
  case BinarySection::Name: \
    return BinarySectionOrder::Name;
    WABT_FOREACH_BINARY_SECTION(V)
#undef V
    default:
      WABT_UNREACHABLE;
  }
}

const char* GetSectionName(BinarySection sec) {
  switch (sec) {
#define V(Name, name, code) \
  case BinarySection::Name: \
    return #Name;
    WABT_FOREACH_BINARY_SECTION(V)
#undef V
    default:
      WABT_UNREACHABLE;
  }
}

// clang-format off
const char* NameSubsectionName[] = {
    "module",
    "function",
    "local",
    "label",
    "type",
    "table",
    "memory",
    "global",
    "elemseg",
    "dataseg",
    "tag",
};
// clang-format on

const char* GetNameSectionSubsectionName(NameSectionSubsection subsec) {
  static_assert(WABT_ENUM_COUNT(NameSectionSubsection) ==
                    WABT_ARRAY_SIZE(NameSubsectionName),
                "Malformed ExprTypeName array");
  return NameSubsectionName[size_t(subsec)];
}

}  // namespace wabt
