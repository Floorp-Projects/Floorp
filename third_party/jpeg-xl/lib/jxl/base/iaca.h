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

#ifndef LIB_JXL_BASE_IACA_H_
#define LIB_JXL_BASE_IACA_H_

#include "lib/jxl/base/compiler_specific.h"

// IACA (Intel's Code Analyzer) analyzes instruction latencies, but only for
// code between special markers. These functions embed such markers in an
// executable, but only for reading via IACA - they deliberately trigger a
// crash if executed to ensure they are removed in normal builds.

#ifndef JXL_IACA_ENABLED
#define JXL_IACA_ENABLED 0
#endif

namespace jxl {

// Call before the region of interest.
static JXL_INLINE void BeginIACA() {
#if JXL_IACA_ENABLED && (JXL_COMPILER_GCC || JXL_COMPILER_CLANG)
  asm volatile(
      // UD2 "instruction" raises an invalid opcode exception.
      ".byte 0x0F, 0x0B\n\t"
      // Magic sequence recognized by IACA (MOV + addr32 fs:NOP). This actually
      // clobbers EBX, but we don't care because the code won't be run, and we
      // want IACA to observe the same code the compiler would have generated
      // without this marker.
      "movl $111, %%ebx\n\t"
      ".byte 0x64, 0x67, 0x90\n\t"
      :
      :
      // (Allegedly) clobbering memory may prevent reordering.
      : "memory");
#endif
}

// Call after the region of interest.
static JXL_INLINE void EndIACA() {
#if JXL_IACA_ENABLED && (JXL_COMPILER_GCC || JXL_COMPILER_CLANG)
  asm volatile(
      // See above.
      "movl $222, %%ebx\n\t"
      ".byte 0x64, 0x67, 0x90\n\t"
      // UD2
      ".byte 0x0F, 0x0B\n\t"
      :
      :
      // (Allegedly) clobbering memory may prevent reordering.
      : "memory");
#endif
}

// Add to a scope to mark a region.
struct ScopeIACA {
  JXL_INLINE ScopeIACA() { BeginIACA(); }
  JXL_INLINE ~ScopeIACA() { EndIACA(); }
};

}  // namespace jxl

#endif  // LIB_JXL_BASE_IACA_H_
