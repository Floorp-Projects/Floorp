/*
 * Copyright 2021 WebAssembly Community Group participants
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

#ifndef WABT_STRING_FORMAT_H_
#define WABT_STRING_FORMAT_H_

#include <cstdarg>
#include <string>
#include <vector>

#include "wabt/config.h"

#define PRIstringview "%.*s"
#define WABT_PRINTF_STRING_VIEW_ARG(x) \
  static_cast<int>((x).length()), (x).data()

#define PRItypecode "%s%#x"
#define WABT_PRINTF_TYPE_CODE(x) \
  (static_cast<int32_t>(x) < 0 ? "-" : ""), std::abs(static_cast<int32_t>(x))

#define WABT_DEFAULT_SNPRINTF_ALLOCA_BUFSIZE 128
#define WABT_SNPRINTF_ALLOCA(buffer, len, format)                          \
  va_list args;                                                            \
  va_list args_copy;                                                       \
  va_start(args, format);                                                  \
  va_copy(args_copy, args);                                                \
  char fixed_buf[WABT_DEFAULT_SNPRINTF_ALLOCA_BUFSIZE];                    \
  char* buffer = fixed_buf;                                                \
  size_t len = wabt_vsnprintf(fixed_buf, sizeof(fixed_buf), format, args); \
  va_end(args);                                                            \
  if (len + 1 > sizeof(fixed_buf)) {                                       \
    buffer = static_cast<char*>(alloca(len + 1));                          \
    len = wabt_vsnprintf(buffer, len + 1, format, args_copy);              \
  }                                                                        \
  va_end(args_copy)

namespace wabt {

inline std::string WABT_PRINTF_FORMAT(1, 2)
    StringPrintf(const char* format, ...) {
  va_list args;
  va_list args_copy;
  va_start(args, format);
  va_copy(args_copy, args);
  size_t len = wabt_vsnprintf(nullptr, 0, format, args) + 1;  // For \0.
  std::vector<char> buffer(len);
  va_end(args);
  wabt_vsnprintf(buffer.data(), len, format, args_copy);
  va_end(args_copy);
  return std::string(buffer.data(), len - 1);
}

}  // namespace wabt

#endif  // WABT_STRING_FORMAT_H_
