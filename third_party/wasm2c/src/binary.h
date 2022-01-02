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

#ifndef WABT_BINARY_H_
#define WABT_BINARY_H_

#include "src/common.h"

#define WABT_BINARY_MAGIC 0x6d736100
#define WABT_BINARY_VERSION 1
#define WABT_BINARY_LIMITS_HAS_MAX_FLAG 0x1
#define WABT_BINARY_LIMITS_IS_SHARED_FLAG 0x2
#define WABT_BINARY_LIMITS_IS_64_FLAG 0x4
#define WABT_BINARY_LIMITS_ALL_FLAGS                                     \
  (WABT_BINARY_LIMITS_HAS_MAX_FLAG | WABT_BINARY_LIMITS_IS_SHARED_FLAG | \
   WABT_BINARY_LIMITS_IS_64_FLAG)

#define WABT_BINARY_SECTION_NAME "name"
#define WABT_BINARY_SECTION_RELOC "reloc"
#define WABT_BINARY_SECTION_LINKING "linking"
#define WABT_BINARY_SECTION_DYLINK "dylink"
#define WABT_BINARY_SECTION_DYLINK0 "dylink.0"

#define WABT_FOREACH_BINARY_SECTION(V) \
  V(Custom, custom, 0)                 \
  V(Type, type, 1)                     \
  V(Import, import, 2)                 \
  V(Function, function, 3)             \
  V(Table, table, 4)                   \
  V(Memory, memory, 5)                 \
  V(Tag, tag, 13)                      \
  V(Global, global, 6)                 \
  V(Export, export, 7)                 \
  V(Start, start, 8)                   \
  V(Elem, elem, 9)                     \
  V(DataCount, data_count, 12)         \
  V(Code, code, 10)                    \
  V(Data, data, 11)

namespace wabt {

/* clang-format off */
enum class BinarySection {
#define V(Name, name, code) Name = code,
  WABT_FOREACH_BINARY_SECTION(V)
#undef V
  Invalid = ~0,

  First = Custom,
  Last = Tag,
};
/* clang-format on */
static const int kBinarySectionCount = WABT_ENUM_COUNT(BinarySection);

enum class BinarySectionOrder {
#define V(Name, name, code) Name,
  WABT_FOREACH_BINARY_SECTION(V)
#undef V
};

BinarySectionOrder GetSectionOrder(BinarySection);
const char* GetSectionName(BinarySection);

enum class NameSectionSubsection {
  Module = 0,
  Function = 1,
  Local = 2,
  Label = 3,
  Type = 4,
  Table = 5,
  Memory = 6,
  Global = 7,
  ElemSegment = 8,
  DataSegment = 9,
  Last = DataSegment,
};
const char* GetNameSectionSubsectionName(NameSectionSubsection subsec);

}  // namespace wabt

#endif /* WABT_BINARY_H_ */
