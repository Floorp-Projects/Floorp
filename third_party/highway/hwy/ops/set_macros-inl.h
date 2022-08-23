// Copyright 2020 Google LLC
// SPDX-License-Identifier: Apache-2.0
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

// Sets macros based on HWY_TARGET.

// This include guard is toggled by foreach_target, so avoid the usual _H_
// suffix to prevent copybara from renaming it.
#if defined(HWY_SET_MACROS_PER_TARGET) == defined(HWY_TARGET_TOGGLE)
#ifdef HWY_SET_MACROS_PER_TARGET
#undef HWY_SET_MACROS_PER_TARGET
#else
#define HWY_SET_MACROS_PER_TARGET
#endif

#endif  // HWY_SET_MACROS_PER_TARGET

#include "hwy/detect_targets.h"

#undef HWY_NAMESPACE
#undef HWY_ALIGN
#undef HWY_MAX_BYTES
#undef HWY_LANES

#undef HWY_HAVE_SCALABLE
#undef HWY_HAVE_INTEGER64
#undef HWY_HAVE_FLOAT16
#undef HWY_HAVE_FLOAT64
#undef HWY_MEM_OPS_MIGHT_FAULT
#undef HWY_NATIVE_FMA
#undef HWY_CAP_GE256
#undef HWY_CAP_GE512

#undef HWY_TARGET_STR

#if defined(HWY_DISABLE_PCLMUL_AES)
#define HWY_TARGET_STR_PCLMUL_AES ""
#else
#define HWY_TARGET_STR_PCLMUL_AES ",pclmul,aes"
#endif

#if defined(HWY_DISABLE_BMI2_FMA)
#define HWY_TARGET_STR_BMI2_FMA ""
#else
#define HWY_TARGET_STR_BMI2_FMA ",bmi,bmi2,fma"
#endif

#if defined(HWY_DISABLE_F16C)
#define HWY_TARGET_STR_F16C ""
#else
#define HWY_TARGET_STR_F16C ",f16c"
#endif

#define HWY_TARGET_STR_SSSE3 "sse2,ssse3"

#define HWY_TARGET_STR_SSE4 \
  HWY_TARGET_STR_SSSE3 ",sse4.1,sse4.2" HWY_TARGET_STR_PCLMUL_AES
// Include previous targets, which are the half-vectors of the next target.
#define HWY_TARGET_STR_AVX2 \
  HWY_TARGET_STR_SSE4 ",avx,avx2" HWY_TARGET_STR_BMI2_FMA HWY_TARGET_STR_F16C
#define HWY_TARGET_STR_AVX3 \
  HWY_TARGET_STR_AVX2 ",avx512f,avx512vl,avx512dq,avx512bw"

// Before include guard so we redefine HWY_TARGET_STR on each include,
// governed by the current HWY_TARGET.

//-----------------------------------------------------------------------------
// SSSE3
#if HWY_TARGET == HWY_SSSE3

#define HWY_NAMESPACE N_SSSE3
#define HWY_ALIGN alignas(16)
#define HWY_MAX_BYTES 16
#define HWY_LANES(T) (16 / sizeof(T))

#define HWY_HAVE_SCALABLE 0
#define HWY_HAVE_INTEGER64 1
#define HWY_HAVE_FLOAT16 1
#define HWY_HAVE_FLOAT64 1
#define HWY_MEM_OPS_MIGHT_FAULT 1
#define HWY_NATIVE_FMA 0
#define HWY_CAP_GE256 0
#define HWY_CAP_GE512 0

#define HWY_TARGET_STR HWY_TARGET_STR_SSSE3

//-----------------------------------------------------------------------------
// SSE4
#elif HWY_TARGET == HWY_SSE4

#define HWY_NAMESPACE N_SSE4
#define HWY_ALIGN alignas(16)
#define HWY_MAX_BYTES 16
#define HWY_LANES(T) (16 / sizeof(T))

#define HWY_HAVE_SCALABLE 0
#define HWY_HAVE_INTEGER64 1
#define HWY_HAVE_FLOAT16 1
#define HWY_HAVE_FLOAT64 1
#define HWY_MEM_OPS_MIGHT_FAULT 1
#define HWY_NATIVE_FMA 0
#define HWY_CAP_GE256 0
#define HWY_CAP_GE512 0

#define HWY_TARGET_STR HWY_TARGET_STR_SSE4

//-----------------------------------------------------------------------------
// AVX2
#elif HWY_TARGET == HWY_AVX2

#define HWY_NAMESPACE N_AVX2
#define HWY_ALIGN alignas(32)
#define HWY_MAX_BYTES 32
#define HWY_LANES(T) (32 / sizeof(T))

#define HWY_HAVE_SCALABLE 0
#define HWY_HAVE_INTEGER64 1
#define HWY_HAVE_FLOAT16 1
#define HWY_HAVE_FLOAT64 1
#define HWY_MEM_OPS_MIGHT_FAULT 1

#ifdef HWY_DISABLE_BMI2_FMA
#define HWY_NATIVE_FMA 0
#else
#define HWY_NATIVE_FMA 1
#endif

#define HWY_CAP_GE256 1
#define HWY_CAP_GE512 0

#define HWY_TARGET_STR HWY_TARGET_STR_AVX2

//-----------------------------------------------------------------------------
// AVX3[_DL]
#elif HWY_TARGET == HWY_AVX3 || HWY_TARGET == HWY_AVX3_DL

#define HWY_ALIGN alignas(64)
#define HWY_MAX_BYTES 64
#define HWY_LANES(T) (64 / sizeof(T))

#define HWY_HAVE_SCALABLE 0
#define HWY_HAVE_INTEGER64 1
#define HWY_HAVE_FLOAT16 1
#define HWY_HAVE_FLOAT64 1
#define HWY_MEM_OPS_MIGHT_FAULT 0
#define HWY_NATIVE_FMA 1
#define HWY_CAP_GE256 1
#define HWY_CAP_GE512 1

#if HWY_TARGET == HWY_AVX3

#define HWY_NAMESPACE N_AVX3
#define HWY_TARGET_STR HWY_TARGET_STR_AVX3

#elif HWY_TARGET == HWY_AVX3_DL

#define HWY_NAMESPACE N_AVX3_DL
#define HWY_TARGET_STR                                            \
  HWY_TARGET_STR_AVX3                                             \
  ",vpclmulqdq,avx512vbmi,avx512vbmi2,vaes,avxvnni,avx512bitalg," \
  "avx512vpopcntdq"

#else
#error "Logic error"
#endif  // HWY_TARGET == HWY_AVX3_DL

//-----------------------------------------------------------------------------
// PPC8
#elif HWY_TARGET == HWY_PPC8

#define HWY_ALIGN alignas(16)
#define HWY_MAX_BYTES 16
#define HWY_LANES(T) (16 / sizeof(T))

#define HWY_HAVE_SCALABLE 0
#define HWY_HAVE_INTEGER64 1
#define HWY_HAVE_FLOAT16 0
#define HWY_HAVE_FLOAT64 1
#define HWY_MEM_OPS_MIGHT_FAULT 1
#define HWY_NATIVE_FMA 1
#define HWY_CAP_GE256 0
#define HWY_CAP_GE512 0

#define HWY_NAMESPACE N_PPC8

#define HWY_TARGET_STR "altivec,vsx"

//-----------------------------------------------------------------------------
// NEON
#elif HWY_TARGET == HWY_NEON

#define HWY_ALIGN alignas(16)
#define HWY_MAX_BYTES 16
#define HWY_LANES(T) (16 / sizeof(T))

#define HWY_HAVE_SCALABLE 0
#define HWY_HAVE_INTEGER64 1
#define HWY_HAVE_FLOAT16 1

#if HWY_ARCH_ARM_A64
#define HWY_HAVE_FLOAT64 1
#else
#define HWY_HAVE_FLOAT64 0
#endif

#define HWY_MEM_OPS_MIGHT_FAULT 1

#if defined(__ARM_VFPV4__) || HWY_ARCH_ARM_A64
#define HWY_NATIVE_FMA 1
#else
#define HWY_NATIVE_FMA 0
#endif

#define HWY_CAP_GE256 0
#define HWY_CAP_GE512 0

#define HWY_NAMESPACE N_NEON

// Can use pragmas instead of -march compiler flag
#if HWY_HAVE_RUNTIME_DISPATCH
#if HWY_ARCH_ARM_V7
#define HWY_TARGET_STR "+neon-vfpv4"
#else
#define HWY_TARGET_STR "+crypto"
#endif  // HWY_ARCH_ARM_V7
#else
// HWY_TARGET_STR remains undefined
#endif

//-----------------------------------------------------------------------------
// SVE[2]
#elif HWY_TARGET == HWY_SVE2 || HWY_TARGET == HWY_SVE || \
    HWY_TARGET == HWY_SVE_256 || HWY_TARGET == HWY_SVE2_128

// SVE only requires lane alignment, not natural alignment of the entire vector.
#define HWY_ALIGN alignas(8)

// Value ensures MaxLanes() is the tightest possible upper bound to reduce
// overallocation.
#define HWY_LANES(T) ((HWY_MAX_BYTES) / sizeof(T))

#define HWY_HAVE_SCALABLE 1
#define HWY_HAVE_INTEGER64 1
#define HWY_HAVE_FLOAT16 1
#define HWY_HAVE_FLOAT64 1
#define HWY_MEM_OPS_MIGHT_FAULT 0
#define HWY_NATIVE_FMA 1
#define HWY_CAP_GE256 0
#define HWY_CAP_GE512 0

#if HWY_TARGET == HWY_SVE2
#define HWY_NAMESPACE N_SVE2
#define HWY_MAX_BYTES 256
#elif HWY_TARGET == HWY_SVE_256
#define HWY_NAMESPACE N_SVE_256
#define HWY_MAX_BYTES 32
#elif HWY_TARGET == HWY_SVE2_128
#define HWY_NAMESPACE N_SVE2_128
#define HWY_MAX_BYTES 16
#else
#define HWY_NAMESPACE N_SVE
#define HWY_MAX_BYTES 256
#endif

// Can use pragmas instead of -march compiler flag
#if HWY_HAVE_RUNTIME_DISPATCH
#if HWY_TARGET == HWY_SVE2 || HWY_TARGET == HWY_SVE2_128
#define HWY_TARGET_STR "+sve2-aes"
#else
#define HWY_TARGET_STR "+sve"
#endif
#else
// HWY_TARGET_STR remains undefined
#endif

//-----------------------------------------------------------------------------
// WASM
#elif HWY_TARGET == HWY_WASM

#define HWY_ALIGN alignas(16)
#define HWY_MAX_BYTES 16
#define HWY_LANES(T) (16 / sizeof(T))

#define HWY_HAVE_SCALABLE 0
#define HWY_HAVE_INTEGER64 1
#define HWY_HAVE_FLOAT16 1
#define HWY_HAVE_FLOAT64 0
#define HWY_MEM_OPS_MIGHT_FAULT 1
#define HWY_NATIVE_FMA 0
#define HWY_CAP_GE256 0
#define HWY_CAP_GE512 0

#define HWY_NAMESPACE N_WASM

#define HWY_TARGET_STR "simd128"

//-----------------------------------------------------------------------------
// WASM_EMU256
#elif HWY_TARGET == HWY_WASM_EMU256

#define HWY_ALIGN alignas(32)
#define HWY_MAX_BYTES 32
#define HWY_LANES(T) (32 / sizeof(T))

#define HWY_HAVE_SCALABLE 0
#define HWY_HAVE_INTEGER64 1
#define HWY_HAVE_FLOAT16 1
#define HWY_HAVE_FLOAT64 0
#define HWY_MEM_OPS_MIGHT_FAULT 1
#define HWY_NATIVE_FMA 0
#define HWY_CAP_GE256 0
#define HWY_CAP_GE512 0

#define HWY_NAMESPACE N_WASM_EMU256

#define HWY_TARGET_STR "simd128"

//-----------------------------------------------------------------------------
// RVV
#elif HWY_TARGET == HWY_RVV

// RVV only requires lane alignment, not natural alignment of the entire vector,
// and the compiler already aligns builtin types, so nothing to do here.
#define HWY_ALIGN

// The spec requires VLEN <= 2^16 bits, so the limit is 2^16 bytes (LMUL=8).
#define HWY_MAX_BYTES 65536

// = HWY_MAX_BYTES divided by max LMUL=8 because MaxLanes includes the actual
// LMUL. This is the tightest possible upper bound.
#define HWY_LANES(T) (8192 / sizeof(T))

#define HWY_HAVE_SCALABLE 1
#define HWY_HAVE_INTEGER64 1
#define HWY_HAVE_FLOAT64 1
#define HWY_MEM_OPS_MIGHT_FAULT 0
#define HWY_NATIVE_FMA 1
#define HWY_CAP_GE256 0
#define HWY_CAP_GE512 0

#if defined(__riscv_zvfh)
#define HWY_HAVE_FLOAT16 1
#else
#define HWY_HAVE_FLOAT16 0
#endif

#define HWY_NAMESPACE N_RVV

// HWY_TARGET_STR remains undefined so HWY_ATTR is a no-op.
// (rv64gcv is not a valid target)

//-----------------------------------------------------------------------------
// EMU128
#elif HWY_TARGET == HWY_EMU128

#define HWY_ALIGN alignas(16)
#define HWY_MAX_BYTES 16
#define HWY_LANES(T) (16 / sizeof(T))

#define HWY_HAVE_SCALABLE 0
#define HWY_HAVE_INTEGER64 1
#define HWY_HAVE_FLOAT16 1
#define HWY_HAVE_FLOAT64 1
#define HWY_MEM_OPS_MIGHT_FAULT 1
#define HWY_NATIVE_FMA 0
#define HWY_CAP_GE256 0
#define HWY_CAP_GE512 0

#define HWY_NAMESPACE N_EMU128

// HWY_TARGET_STR remains undefined so HWY_ATTR is a no-op.

//-----------------------------------------------------------------------------
// SCALAR
#elif HWY_TARGET == HWY_SCALAR

#define HWY_ALIGN
#define HWY_MAX_BYTES 8
#define HWY_LANES(T) 1

#define HWY_HAVE_SCALABLE 0
#define HWY_HAVE_INTEGER64 1
#define HWY_HAVE_FLOAT16 1
#define HWY_HAVE_FLOAT64 1
#define HWY_MEM_OPS_MIGHT_FAULT 0
#define HWY_NATIVE_FMA 0
#define HWY_CAP_GE256 0
#define HWY_CAP_GE512 0

#define HWY_NAMESPACE N_SCALAR

// HWY_TARGET_STR remains undefined so HWY_ATTR is a no-op.

#else
#pragma message("HWY_TARGET does not match any known target")
#endif  // HWY_TARGET

// Override this to 1 in asan/msan builds, which will still fault.
#if HWY_IS_ASAN || HWY_IS_MSAN
#undef HWY_MEM_OPS_MIGHT_FAULT
#define HWY_MEM_OPS_MIGHT_FAULT 1
#endif

// Clang <9 requires this be invoked at file scope, before any namespace.
#undef HWY_BEFORE_NAMESPACE
#if defined(HWY_TARGET_STR)
#define HWY_BEFORE_NAMESPACE()        \
  HWY_PUSH_ATTRIBUTES(HWY_TARGET_STR) \
  static_assert(true, "For requiring trailing semicolon")
#else
// avoids compiler warning if no HWY_TARGET_STR
#define HWY_BEFORE_NAMESPACE() \
  static_assert(true, "For requiring trailing semicolon")
#endif

// Clang <9 requires any namespaces be closed before this macro.
#undef HWY_AFTER_NAMESPACE
#if defined(HWY_TARGET_STR)
#define HWY_AFTER_NAMESPACE() \
  HWY_POP_ATTRIBUTES          \
  static_assert(true, "For requiring trailing semicolon")
#else
// avoids compiler warning if no HWY_TARGET_STR
#define HWY_AFTER_NAMESPACE() \
  static_assert(true, "For requiring trailing semicolon")
#endif

#undef HWY_ATTR
#if defined(HWY_TARGET_STR) && HWY_HAS_ATTRIBUTE(target)
#define HWY_ATTR __attribute__((target(HWY_TARGET_STR)))
#else
#define HWY_ATTR
#endif
