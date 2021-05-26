// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

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
