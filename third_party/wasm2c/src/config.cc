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

#include "config.h"

#include <cstdarg>
#include <cstdio>

#if COMPILER_IS_MSVC && _M_X64
#include <emmintrin.h>
#elif COMPILER_IS_MSVC && _M_IX86
#include <float.h>
#endif

/* c99-style vsnprintf for MSVC < 2015. See http://stackoverflow.com/a/8712996
 using _snprintf or vsnprintf will not-properly null-terminate, and will return
 -1 instead of the number of characters needed on overflow. */
#if COMPILER_IS_MSVC
int wabt_vsnprintf(char* str, size_t size, const char* format, va_list ap) {
  int result = -1;
  if (size != 0) {
    result = _vsnprintf_s(str, size, _TRUNCATE, format, ap);
  }
  if (result == -1) {
    result = _vscprintf(format, ap);
  }
  return result;
}

#if !HAVE_SNPRINTF
int wabt_snprintf(char* str, size_t size, const char* format, ...) {
  va_list args;
  va_start(args, format);
  int result = wabt_vsnprintf(str, size, format, args);
  va_end(args);
  return result;
}
#endif
#endif

#if COMPILER_IS_MSVC && _M_IX86
// Allow the following functions to change the floating-point environment (e.g.
// update to 64-bit precision in the mantissa). This is only needed for x87
// floats, which are only used on MSVC 32-bit.
#pragma fenv_access (on)
namespace {

typedef unsigned int FPControl;

FPControl Set64BitPrecisionControl() {
  FPControl old_ctrl = _control87(0, 0);
  _control87(_PC_64, _MCW_PC);
  return old_ctrl;
}

void ResetPrecisionControl(FPControl old_ctrl) {
  _control87(old_ctrl, _MCW_PC);
}

}  // end of anonymous namespace
#endif

double wabt_convert_uint64_to_double(uint64_t x) {
#if COMPILER_IS_MSVC && _M_X64
  // MSVC on x64 generates uint64 -> float conversions but doesn't do
  // round-to-nearest-ties-to-even, which is required by WebAssembly.
  __m128d result = _mm_setzero_pd();
  if (x & 0x8000000000000000ULL) {
    result = _mm_cvtsi64_sd(result, (x >> 1) | (x & 1));
    result = _mm_add_sd(result, result);
  } else {
    result = _mm_cvtsi64_sd(result, x);
  }
  return _mm_cvtsd_f64(result);
#elif COMPILER_IS_MSVC && _M_IX86
  // MSVC on x86 converts from i64 -> double -> float, which causes incorrect
  // rounding. Using the x87 float stack instead preserves the correct
  // rounding.
  FPControl old_ctrl = Set64BitPrecisionControl();
  static const double c = 18446744073709551616.0;
  double result;
  __asm fild x;
  if (x & 0x8000000000000000ULL) {
    __asm fadd c;
  }
  __asm fstp result;
  ResetPrecisionControl(old_ctrl);
  return result;
#else
  return static_cast<double>(x);
#endif
}

float wabt_convert_uint64_to_float(uint64_t x) {
#if COMPILER_IS_MSVC && _M_X64
  // MSVC on x64 generates uint64 -> float conversions but doesn't do
  // round-to-nearest-ties-to-even, which is required by WebAssembly.
  __m128 result = _mm_setzero_ps();
  if (x & 0x8000000000000000ULL) {
    result = _mm_cvtsi64_ss(result, (x >> 1) | (x & 1));
    result = _mm_add_ss(result, result);
  } else {
    result = _mm_cvtsi64_ss(result, x);
  }
  return _mm_cvtss_f32(result);
#elif COMPILER_IS_MSVC && _M_IX86
  // MSVC on x86 converts from i64 -> double -> float, which causes incorrect
  // rounding. Using the x87 float stack instead preserves the correct
  // rounding.
  FPControl old_ctrl = Set64BitPrecisionControl();
  static const float c = 18446744073709551616.0f;
  float result;
  __asm fild x;
  if (x & 0x8000000000000000ULL) {
    __asm fadd c;
  }
  __asm fstp result;
  ResetPrecisionControl(old_ctrl);
  return result;
#else
  return static_cast<float>(x);
#endif
}

double wabt_convert_int64_to_double(int64_t x) {
#if COMPILER_IS_MSVC && _M_IX86
  double result;
  __asm fild x;
  __asm fstp result;
  return result;
#else
  return static_cast<double>(x);
#endif
}

float wabt_convert_int64_to_float(int64_t x) {
#if COMPILER_IS_MSVC && _M_IX86
  float result;
  __asm fild x;
  __asm fstp result;
  return result;
#else
  return static_cast<float>(x);
#endif
}

#if COMPILER_IS_MSVC && _M_IX86
#pragma fenv_access (off)
#endif
