// Copyright (c) the JPEG XL Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "lib/jxl/base/status.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include <string>

#if defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER) || \
    defined(THREAD_SANITIZER)
#include "sanitizer/common_interface_defs.h"  // __sanitizer_print_stack_trace
#endif                                        // defined(*_SANITIZER)

namespace jxl {

bool Debug(const char* format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  return false;
}

bool Abort() {
#if defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER) || \
    defined(THREAD_SANITIZER)
  // If compiled with any sanitizer print a stack trace. This call doesn't crash
  // the program, instead the trap below will crash it also allowing gdb to
  // break there.
  __sanitizer_print_stack_trace();
#endif  // defined(*_SANITIZER)

#if JXL_COMPILER_MSVC
  __debugbreak();
  abort();
#else
  __builtin_trap();
#endif
}

}  // namespace jxl
