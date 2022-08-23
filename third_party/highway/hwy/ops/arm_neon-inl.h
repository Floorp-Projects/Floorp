// Copyright 2019 Google LLC
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

// 128-bit ARM64 NEON vectors and operations.
// External include guard in highway.h - see comment there.

// ARM NEON intrinsics are documented at:
// https://developer.arm.com/architectures/instruction-sets/intrinsics/#f:@navigationhierarchiessimdisa=[Neon]

#include <stddef.h>
#include <stdint.h>

#include "hwy/base.h"  // before HWY_DIAGNOSTICS

HWY_DIAGNOSTICS(push)
HWY_DIAGNOSTICS_OFF(disable : 4701, ignored "-Wuninitialized")
#include <arm_neon.h>
HWY_DIAGNOSTICS(pop)

#include "hwy/ops/shared-inl.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

namespace detail {  // for code folding and Raw128

// Macros used to define single and double function calls for multiple types
// for full and half vectors. These macros are undefined at the end of the file.

// HWY_NEON_BUILD_TPL_* is the template<...> prefix to the function.
#define HWY_NEON_BUILD_TPL_1
#define HWY_NEON_BUILD_TPL_2
#define HWY_NEON_BUILD_TPL_3

// HWY_NEON_BUILD_RET_* is return type; type arg is without _t suffix so we can
// extend it to int32x4x2_t packs.
#define HWY_NEON_BUILD_RET_1(type, size) Vec128<type##_t, size>
#define HWY_NEON_BUILD_RET_2(type, size) Vec128<type##_t, size>
#define HWY_NEON_BUILD_RET_3(type, size) Vec128<type##_t, size>

// HWY_NEON_BUILD_PARAM_* is the list of parameters the function receives.
#define HWY_NEON_BUILD_PARAM_1(type, size) const Vec128<type##_t, size> a
#define HWY_NEON_BUILD_PARAM_2(type, size) \
  const Vec128<type##_t, size> a, const Vec128<type##_t, size> b
#define HWY_NEON_BUILD_PARAM_3(type, size)                        \
  const Vec128<type##_t, size> a, const Vec128<type##_t, size> b, \
      const Vec128<type##_t, size> c

// HWY_NEON_BUILD_ARG_* is the list of arguments passed to the underlying
// function.
#define HWY_NEON_BUILD_ARG_1 a.raw
#define HWY_NEON_BUILD_ARG_2 a.raw, b.raw
#define HWY_NEON_BUILD_ARG_3 a.raw, b.raw, c.raw

// We use HWY_NEON_EVAL(func, ...) to delay the evaluation of func until after
// the __VA_ARGS__ have been expanded. This allows "func" to be a macro on
// itself like with some of the library "functions" such as vshlq_u8. For
// example, HWY_NEON_EVAL(vshlq_u8, MY_PARAMS) where MY_PARAMS is defined as
// "a, b" (without the quotes) will end up expanding "vshlq_u8(a, b)" if needed.
// Directly writing vshlq_u8(MY_PARAMS) would fail since vshlq_u8() macro
// expects two arguments.
#define HWY_NEON_EVAL(func, ...) func(__VA_ARGS__)

// Main macro definition that defines a single function for the given type and
// size of vector, using the underlying (prefix##infix##suffix) function and
// the template, return type, parameters and arguments defined by the "args"
// parameters passed here (see HWY_NEON_BUILD_* macros defined before).
#define HWY_NEON_DEF_FUNCTION(type, size, name, prefix, infix, suffix, args) \
  HWY_CONCAT(HWY_NEON_BUILD_TPL_, args)                                      \
  HWY_API HWY_CONCAT(HWY_NEON_BUILD_RET_, args)(type, size)                  \
      name(HWY_CONCAT(HWY_NEON_BUILD_PARAM_, args)(type, size)) {            \
    return HWY_CONCAT(HWY_NEON_BUILD_RET_, args)(type, size)(                \
        HWY_NEON_EVAL(prefix##infix##suffix, HWY_NEON_BUILD_ARG_##args));    \
  }

// The HWY_NEON_DEF_FUNCTION_* macros define all the variants of a function
// called "name" using the set of neon functions starting with the given
// "prefix" for all the variants of certain types, as specified next to each
// macro. For example, the prefix "vsub" can be used to define the operator-
// using args=2.

// uint8_t
#define HWY_NEON_DEF_FUNCTION_UINT_8(name, prefix, infix, args)      \
  HWY_NEON_DEF_FUNCTION(uint8, 16, name, prefix##q, infix, u8, args) \
  HWY_NEON_DEF_FUNCTION(uint8, 8, name, prefix, infix, u8, args)     \
  HWY_NEON_DEF_FUNCTION(uint8, 4, name, prefix, infix, u8, args)     \
  HWY_NEON_DEF_FUNCTION(uint8, 2, name, prefix, infix, u8, args)     \
  HWY_NEON_DEF_FUNCTION(uint8, 1, name, prefix, infix, u8, args)

// int8_t
#define HWY_NEON_DEF_FUNCTION_INT_8(name, prefix, infix, args)      \
  HWY_NEON_DEF_FUNCTION(int8, 16, name, prefix##q, infix, s8, args) \
  HWY_NEON_DEF_FUNCTION(int8, 8, name, prefix, infix, s8, args)     \
  HWY_NEON_DEF_FUNCTION(int8, 4, name, prefix, infix, s8, args)     \
  HWY_NEON_DEF_FUNCTION(int8, 2, name, prefix, infix, s8, args)     \
  HWY_NEON_DEF_FUNCTION(int8, 1, name, prefix, infix, s8, args)

// uint16_t
#define HWY_NEON_DEF_FUNCTION_UINT_16(name, prefix, infix, args)      \
  HWY_NEON_DEF_FUNCTION(uint16, 8, name, prefix##q, infix, u16, args) \
  HWY_NEON_DEF_FUNCTION(uint16, 4, name, prefix, infix, u16, args)    \
  HWY_NEON_DEF_FUNCTION(uint16, 2, name, prefix, infix, u16, args)    \
  HWY_NEON_DEF_FUNCTION(uint16, 1, name, prefix, infix, u16, args)

// int16_t
#define HWY_NEON_DEF_FUNCTION_INT_16(name, prefix, infix, args)      \
  HWY_NEON_DEF_FUNCTION(int16, 8, name, prefix##q, infix, s16, args) \
  HWY_NEON_DEF_FUNCTION(int16, 4, name, prefix, infix, s16, args)    \
  HWY_NEON_DEF_FUNCTION(int16, 2, name, prefix, infix, s16, args)    \
  HWY_NEON_DEF_FUNCTION(int16, 1, name, prefix, infix, s16, args)

// uint32_t
#define HWY_NEON_DEF_FUNCTION_UINT_32(name, prefix, infix, args)      \
  HWY_NEON_DEF_FUNCTION(uint32, 4, name, prefix##q, infix, u32, args) \
  HWY_NEON_DEF_FUNCTION(uint32, 2, name, prefix, infix, u32, args)    \
  HWY_NEON_DEF_FUNCTION(uint32, 1, name, prefix, infix, u32, args)

// int32_t
#define HWY_NEON_DEF_FUNCTION_INT_32(name, prefix, infix, args)      \
  HWY_NEON_DEF_FUNCTION(int32, 4, name, prefix##q, infix, s32, args) \
  HWY_NEON_DEF_FUNCTION(int32, 2, name, prefix, infix, s32, args)    \
  HWY_NEON_DEF_FUNCTION(int32, 1, name, prefix, infix, s32, args)

// uint64_t
#define HWY_NEON_DEF_FUNCTION_UINT_64(name, prefix, infix, args)      \
  HWY_NEON_DEF_FUNCTION(uint64, 2, name, prefix##q, infix, u64, args) \
  HWY_NEON_DEF_FUNCTION(uint64, 1, name, prefix, infix, u64, args)

// int64_t
#define HWY_NEON_DEF_FUNCTION_INT_64(name, prefix, infix, args)      \
  HWY_NEON_DEF_FUNCTION(int64, 2, name, prefix##q, infix, s64, args) \
  HWY_NEON_DEF_FUNCTION(int64, 1, name, prefix, infix, s64, args)

// float
#define HWY_NEON_DEF_FUNCTION_FLOAT_32(name, prefix, infix, args)      \
  HWY_NEON_DEF_FUNCTION(float32, 4, name, prefix##q, infix, f32, args) \
  HWY_NEON_DEF_FUNCTION(float32, 2, name, prefix, infix, f32, args)    \
  HWY_NEON_DEF_FUNCTION(float32, 1, name, prefix, infix, f32, args)

// double
#if HWY_ARCH_ARM_A64
#define HWY_NEON_DEF_FUNCTION_FLOAT_64(name, prefix, infix, args)      \
  HWY_NEON_DEF_FUNCTION(float64, 2, name, prefix##q, infix, f64, args) \
  HWY_NEON_DEF_FUNCTION(float64, 1, name, prefix, infix, f64, args)
#else
#define HWY_NEON_DEF_FUNCTION_FLOAT_64(name, prefix, infix, args)
#endif

// float and double

#define HWY_NEON_DEF_FUNCTION_ALL_FLOATS(name, prefix, infix, args) \
  HWY_NEON_DEF_FUNCTION_FLOAT_32(name, prefix, infix, args)         \
  HWY_NEON_DEF_FUNCTION_FLOAT_64(name, prefix, infix, args)

// Helper macros to define for more than one type.
// uint8_t, uint16_t and uint32_t
#define HWY_NEON_DEF_FUNCTION_UINT_8_16_32(name, prefix, infix, args) \
  HWY_NEON_DEF_FUNCTION_UINT_8(name, prefix, infix, args)             \
  HWY_NEON_DEF_FUNCTION_UINT_16(name, prefix, infix, args)            \
  HWY_NEON_DEF_FUNCTION_UINT_32(name, prefix, infix, args)

// int8_t, int16_t and int32_t
#define HWY_NEON_DEF_FUNCTION_INT_8_16_32(name, prefix, infix, args) \
  HWY_NEON_DEF_FUNCTION_INT_8(name, prefix, infix, args)             \
  HWY_NEON_DEF_FUNCTION_INT_16(name, prefix, infix, args)            \
  HWY_NEON_DEF_FUNCTION_INT_32(name, prefix, infix, args)

// uint8_t, uint16_t, uint32_t and uint64_t
#define HWY_NEON_DEF_FUNCTION_UINTS(name, prefix, infix, args)  \
  HWY_NEON_DEF_FUNCTION_UINT_8_16_32(name, prefix, infix, args) \
  HWY_NEON_DEF_FUNCTION_UINT_64(name, prefix, infix, args)

// int8_t, int16_t, int32_t and int64_t
#define HWY_NEON_DEF_FUNCTION_INTS(name, prefix, infix, args)  \
  HWY_NEON_DEF_FUNCTION_INT_8_16_32(name, prefix, infix, args) \
  HWY_NEON_DEF_FUNCTION_INT_64(name, prefix, infix, args)

// All int*_t and uint*_t up to 64
#define HWY_NEON_DEF_FUNCTION_INTS_UINTS(name, prefix, infix, args) \
  HWY_NEON_DEF_FUNCTION_INTS(name, prefix, infix, args)             \
  HWY_NEON_DEF_FUNCTION_UINTS(name, prefix, infix, args)

// All previous types.
#define HWY_NEON_DEF_FUNCTION_ALL_TYPES(name, prefix, infix, args) \
  HWY_NEON_DEF_FUNCTION_INTS_UINTS(name, prefix, infix, args)      \
  HWY_NEON_DEF_FUNCTION_ALL_FLOATS(name, prefix, infix, args)

#define HWY_NEON_DEF_FUNCTION_UIF81632(name, prefix, infix, args) \
  HWY_NEON_DEF_FUNCTION_UINT_8_16_32(name, prefix, infix, args)   \
  HWY_NEON_DEF_FUNCTION_INT_8_16_32(name, prefix, infix, args)    \
  HWY_NEON_DEF_FUNCTION_FLOAT_32(name, prefix, infix, args)

// Emulation of some intrinsics on armv7.
#if HWY_ARCH_ARM_V7
#define vuzp1_s8(x, y) vuzp_s8(x, y).val[0]
#define vuzp1_u8(x, y) vuzp_u8(x, y).val[0]
#define vuzp1_s16(x, y) vuzp_s16(x, y).val[0]
#define vuzp1_u16(x, y) vuzp_u16(x, y).val[0]
#define vuzp1_s32(x, y) vuzp_s32(x, y).val[0]
#define vuzp1_u32(x, y) vuzp_u32(x, y).val[0]
#define vuzp1_f32(x, y) vuzp_f32(x, y).val[0]
#define vuzp1q_s8(x, y) vuzpq_s8(x, y).val[0]
#define vuzp1q_u8(x, y) vuzpq_u8(x, y).val[0]
#define vuzp1q_s16(x, y) vuzpq_s16(x, y).val[0]
#define vuzp1q_u16(x, y) vuzpq_u16(x, y).val[0]
#define vuzp1q_s32(x, y) vuzpq_s32(x, y).val[0]
#define vuzp1q_u32(x, y) vuzpq_u32(x, y).val[0]
#define vuzp1q_f32(x, y) vuzpq_f32(x, y).val[0]
#define vuzp2_s8(x, y) vuzp_s8(x, y).val[1]
#define vuzp2_u8(x, y) vuzp_u8(x, y).val[1]
#define vuzp2_s16(x, y) vuzp_s16(x, y).val[1]
#define vuzp2_u16(x, y) vuzp_u16(x, y).val[1]
#define vuzp2_s32(x, y) vuzp_s32(x, y).val[1]
#define vuzp2_u32(x, y) vuzp_u32(x, y).val[1]
#define vuzp2_f32(x, y) vuzp_f32(x, y).val[1]
#define vuzp2q_s8(x, y) vuzpq_s8(x, y).val[1]
#define vuzp2q_u8(x, y) vuzpq_u8(x, y).val[1]
#define vuzp2q_s16(x, y) vuzpq_s16(x, y).val[1]
#define vuzp2q_u16(x, y) vuzpq_u16(x, y).val[1]
#define vuzp2q_s32(x, y) vuzpq_s32(x, y).val[1]
#define vuzp2q_u32(x, y) vuzpq_u32(x, y).val[1]
#define vuzp2q_f32(x, y) vuzpq_f32(x, y).val[1]
#define vzip1_s8(x, y) vzip_s8(x, y).val[0]
#define vzip1_u8(x, y) vzip_u8(x, y).val[0]
#define vzip1_s16(x, y) vzip_s16(x, y).val[0]
#define vzip1_u16(x, y) vzip_u16(x, y).val[0]
#define vzip1_f32(x, y) vzip_f32(x, y).val[0]
#define vzip1_u32(x, y) vzip_u32(x, y).val[0]
#define vzip1_s32(x, y) vzip_s32(x, y).val[0]
#define vzip1q_s8(x, y) vzipq_s8(x, y).val[0]
#define vzip1q_u8(x, y) vzipq_u8(x, y).val[0]
#define vzip1q_s16(x, y) vzipq_s16(x, y).val[0]
#define vzip1q_u16(x, y) vzipq_u16(x, y).val[0]
#define vzip1q_s32(x, y) vzipq_s32(x, y).val[0]
#define vzip1q_u32(x, y) vzipq_u32(x, y).val[0]
#define vzip1q_f32(x, y) vzipq_f32(x, y).val[0]
#define vzip2_s8(x, y) vzip_s8(x, y).val[1]
#define vzip2_u8(x, y) vzip_u8(x, y).val[1]
#define vzip2_s16(x, y) vzip_s16(x, y).val[1]
#define vzip2_u16(x, y) vzip_u16(x, y).val[1]
#define vzip2_s32(x, y) vzip_s32(x, y).val[1]
#define vzip2_u32(x, y) vzip_u32(x, y).val[1]
#define vzip2_f32(x, y) vzip_f32(x, y).val[1]
#define vzip2q_s8(x, y) vzipq_s8(x, y).val[1]
#define vzip2q_u8(x, y) vzipq_u8(x, y).val[1]
#define vzip2q_s16(x, y) vzipq_s16(x, y).val[1]
#define vzip2q_u16(x, y) vzipq_u16(x, y).val[1]
#define vzip2q_s32(x, y) vzipq_s32(x, y).val[1]
#define vzip2q_u32(x, y) vzipq_u32(x, y).val[1]
#define vzip2q_f32(x, y) vzipq_f32(x, y).val[1]
#endif

// Wrappers over uint8x16x2_t etc. so we can define StoreInterleaved2 overloads
// for all vector types, even those (bfloat16_t) where the underlying vector is
// the same as others (uint16_t).
template <typename T, size_t N>
struct Tuple2;
template <typename T, size_t N>
struct Tuple3;
template <typename T, size_t N>
struct Tuple4;

template <>
struct Tuple2<uint8_t, 16> {
  uint8x16x2_t raw;
};
template <size_t N>
struct Tuple2<uint8_t, N> {
  uint8x8x2_t raw;
};
template <>
struct Tuple2<int8_t, 16> {
  int8x16x2_t raw;
};
template <size_t N>
struct Tuple2<int8_t, N> {
  int8x8x2_t raw;
};
template <>
struct Tuple2<uint16_t, 8> {
  uint16x8x2_t raw;
};
template <size_t N>
struct Tuple2<uint16_t, N> {
  uint16x4x2_t raw;
};
template <>
struct Tuple2<int16_t, 8> {
  int16x8x2_t raw;
};
template <size_t N>
struct Tuple2<int16_t, N> {
  int16x4x2_t raw;
};
template <>
struct Tuple2<uint32_t, 4> {
  uint32x4x2_t raw;
};
template <size_t N>
struct Tuple2<uint32_t, N> {
  uint32x2x2_t raw;
};
template <>
struct Tuple2<int32_t, 4> {
  int32x4x2_t raw;
};
template <size_t N>
struct Tuple2<int32_t, N> {
  int32x2x2_t raw;
};
template <>
struct Tuple2<uint64_t, 2> {
  uint64x2x2_t raw;
};
template <size_t N>
struct Tuple2<uint64_t, N> {
  uint64x1x2_t raw;
};
template <>
struct Tuple2<int64_t, 2> {
  int64x2x2_t raw;
};
template <size_t N>
struct Tuple2<int64_t, N> {
  int64x1x2_t raw;
};

template <>
struct Tuple2<float16_t, 8> {
  uint16x8x2_t raw;
};
template <size_t N>
struct Tuple2<float16_t, N> {
  uint16x4x2_t raw;
};
template <>
struct Tuple2<bfloat16_t, 8> {
  uint16x8x2_t raw;
};
template <size_t N>
struct Tuple2<bfloat16_t, N> {
  uint16x4x2_t raw;
};

template <>
struct Tuple2<float32_t, 4> {
  float32x4x2_t raw;
};
template <size_t N>
struct Tuple2<float32_t, N> {
  float32x2x2_t raw;
};
#if HWY_ARCH_ARM_A64
template <>
struct Tuple2<float64_t, 2> {
  float64x2x2_t raw;
};
template <size_t N>
struct Tuple2<float64_t, N> {
  float64x1x2_t raw;
};
#endif  // HWY_ARCH_ARM_A64

template <>
struct Tuple3<uint8_t, 16> {
  uint8x16x3_t raw;
};
template <size_t N>
struct Tuple3<uint8_t, N> {
  uint8x8x3_t raw;
};
template <>
struct Tuple3<int8_t, 16> {
  int8x16x3_t raw;
};
template <size_t N>
struct Tuple3<int8_t, N> {
  int8x8x3_t raw;
};
template <>
struct Tuple3<uint16_t, 8> {
  uint16x8x3_t raw;
};
template <size_t N>
struct Tuple3<uint16_t, N> {
  uint16x4x3_t raw;
};
template <>
struct Tuple3<int16_t, 8> {
  int16x8x3_t raw;
};
template <size_t N>
struct Tuple3<int16_t, N> {
  int16x4x3_t raw;
};
template <>
struct Tuple3<uint32_t, 4> {
  uint32x4x3_t raw;
};
template <size_t N>
struct Tuple3<uint32_t, N> {
  uint32x2x3_t raw;
};
template <>
struct Tuple3<int32_t, 4> {
  int32x4x3_t raw;
};
template <size_t N>
struct Tuple3<int32_t, N> {
  int32x2x3_t raw;
};
template <>
struct Tuple3<uint64_t, 2> {
  uint64x2x3_t raw;
};
template <size_t N>
struct Tuple3<uint64_t, N> {
  uint64x1x3_t raw;
};
template <>
struct Tuple3<int64_t, 2> {
  int64x2x3_t raw;
};
template <size_t N>
struct Tuple3<int64_t, N> {
  int64x1x3_t raw;
};

template <>
struct Tuple3<float16_t, 8> {
  uint16x8x3_t raw;
};
template <size_t N>
struct Tuple3<float16_t, N> {
  uint16x4x3_t raw;
};
template <>
struct Tuple3<bfloat16_t, 8> {
  uint16x8x3_t raw;
};
template <size_t N>
struct Tuple3<bfloat16_t, N> {
  uint16x4x3_t raw;
};

template <>
struct Tuple3<float32_t, 4> {
  float32x4x3_t raw;
};
template <size_t N>
struct Tuple3<float32_t, N> {
  float32x2x3_t raw;
};
#if HWY_ARCH_ARM_A64
template <>
struct Tuple3<float64_t, 2> {
  float64x2x3_t raw;
};
template <size_t N>
struct Tuple3<float64_t, N> {
  float64x1x3_t raw;
};
#endif  // HWY_ARCH_ARM_A64

template <>
struct Tuple4<uint8_t, 16> {
  uint8x16x4_t raw;
};
template <size_t N>
struct Tuple4<uint8_t, N> {
  uint8x8x4_t raw;
};
template <>
struct Tuple4<int8_t, 16> {
  int8x16x4_t raw;
};
template <size_t N>
struct Tuple4<int8_t, N> {
  int8x8x4_t raw;
};
template <>
struct Tuple4<uint16_t, 8> {
  uint16x8x4_t raw;
};
template <size_t N>
struct Tuple4<uint16_t, N> {
  uint16x4x4_t raw;
};
template <>
struct Tuple4<int16_t, 8> {
  int16x8x4_t raw;
};
template <size_t N>
struct Tuple4<int16_t, N> {
  int16x4x4_t raw;
};
template <>
struct Tuple4<uint32_t, 4> {
  uint32x4x4_t raw;
};
template <size_t N>
struct Tuple4<uint32_t, N> {
  uint32x2x4_t raw;
};
template <>
struct Tuple4<int32_t, 4> {
  int32x4x4_t raw;
};
template <size_t N>
struct Tuple4<int32_t, N> {
  int32x2x4_t raw;
};
template <>
struct Tuple4<uint64_t, 2> {
  uint64x2x4_t raw;
};
template <size_t N>
struct Tuple4<uint64_t, N> {
  uint64x1x4_t raw;
};
template <>
struct Tuple4<int64_t, 2> {
  int64x2x4_t raw;
};
template <size_t N>
struct Tuple4<int64_t, N> {
  int64x1x4_t raw;
};

template <>
struct Tuple4<float16_t, 8> {
  uint16x8x4_t raw;
};
template <size_t N>
struct Tuple4<float16_t, N> {
  uint16x4x4_t raw;
};
template <>
struct Tuple4<bfloat16_t, 8> {
  uint16x8x4_t raw;
};
template <size_t N>
struct Tuple4<bfloat16_t, N> {
  uint16x4x4_t raw;
};

template <>
struct Tuple4<float32_t, 4> {
  float32x4x4_t raw;
};
template <size_t N>
struct Tuple4<float32_t, N> {
  float32x2x4_t raw;
};
#if HWY_ARCH_ARM_A64
template <>
struct Tuple4<float64_t, 2> {
  float64x2x4_t raw;
};
template <size_t N>
struct Tuple4<float64_t, N> {
  float64x1x4_t raw;
};
#endif  // HWY_ARCH_ARM_A64

template <typename T, size_t N>
struct Raw128;

// 128
template <>
struct Raw128<uint8_t, 16> {
  using type = uint8x16_t;
};

template <>
struct Raw128<uint16_t, 8> {
  using type = uint16x8_t;
};

template <>
struct Raw128<uint32_t, 4> {
  using type = uint32x4_t;
};

template <>
struct Raw128<uint64_t, 2> {
  using type = uint64x2_t;
};

template <>
struct Raw128<int8_t, 16> {
  using type = int8x16_t;
};

template <>
struct Raw128<int16_t, 8> {
  using type = int16x8_t;
};

template <>
struct Raw128<int32_t, 4> {
  using type = int32x4_t;
};

template <>
struct Raw128<int64_t, 2> {
  using type = int64x2_t;
};

template <>
struct Raw128<float16_t, 8> {
  using type = uint16x8_t;
};

template <>
struct Raw128<bfloat16_t, 8> {
  using type = uint16x8_t;
};

template <>
struct Raw128<float, 4> {
  using type = float32x4_t;
};

#if HWY_ARCH_ARM_A64
template <>
struct Raw128<double, 2> {
  using type = float64x2_t;
};
#endif

// 64
template <>
struct Raw128<uint8_t, 8> {
  using type = uint8x8_t;
};

template <>
struct Raw128<uint16_t, 4> {
  using type = uint16x4_t;
};

template <>
struct Raw128<uint32_t, 2> {
  using type = uint32x2_t;
};

template <>
struct Raw128<uint64_t, 1> {
  using type = uint64x1_t;
};

template <>
struct Raw128<int8_t, 8> {
  using type = int8x8_t;
};

template <>
struct Raw128<int16_t, 4> {
  using type = int16x4_t;
};

template <>
struct Raw128<int32_t, 2> {
  using type = int32x2_t;
};

template <>
struct Raw128<int64_t, 1> {
  using type = int64x1_t;
};

template <>
struct Raw128<float16_t, 4> {
  using type = uint16x4_t;
};

template <>
struct Raw128<bfloat16_t, 4> {
  using type = uint16x4_t;
};

template <>
struct Raw128<float, 2> {
  using type = float32x2_t;
};

#if HWY_ARCH_ARM_A64
template <>
struct Raw128<double, 1> {
  using type = float64x1_t;
};
#endif

// 32 (same as 64)
template <>
struct Raw128<uint8_t, 4> : public Raw128<uint8_t, 8> {};

template <>
struct Raw128<uint16_t, 2> : public Raw128<uint16_t, 4> {};

template <>
struct Raw128<uint32_t, 1> : public Raw128<uint32_t, 2> {};

template <>
struct Raw128<int8_t, 4> : public Raw128<int8_t, 8> {};

template <>
struct Raw128<int16_t, 2> : public Raw128<int16_t, 4> {};

template <>
struct Raw128<int32_t, 1> : public Raw128<int32_t, 2> {};

template <>
struct Raw128<float16_t, 2> : public Raw128<float16_t, 4> {};

template <>
struct Raw128<bfloat16_t, 2> : public Raw128<bfloat16_t, 4> {};

template <>
struct Raw128<float, 1> : public Raw128<float, 2> {};

// 16 (same as 64)
template <>
struct Raw128<uint8_t, 2> : public Raw128<uint8_t, 8> {};

template <>
struct Raw128<uint16_t, 1> : public Raw128<uint16_t, 4> {};

template <>
struct Raw128<int8_t, 2> : public Raw128<int8_t, 8> {};

template <>
struct Raw128<int16_t, 1> : public Raw128<int16_t, 4> {};

template <>
struct Raw128<float16_t, 1> : public Raw128<float16_t, 4> {};

template <>
struct Raw128<bfloat16_t, 1> : public Raw128<bfloat16_t, 4> {};

// 8 (same as 64)
template <>
struct Raw128<uint8_t, 1> : public Raw128<uint8_t, 8> {};

template <>
struct Raw128<int8_t, 1> : public Raw128<int8_t, 8> {};

}  // namespace detail

template <typename T, size_t N = 16 / sizeof(T)>
class Vec128 {
  using Raw = typename detail::Raw128<T, N>::type;

 public:
  HWY_INLINE Vec128() {}
  Vec128(const Vec128&) = default;
  Vec128& operator=(const Vec128&) = default;
  HWY_INLINE explicit Vec128(const Raw raw) : raw(raw) {}

  // Compound assignment. Only usable if there is a corresponding non-member
  // binary operator overload. For example, only f32 and f64 support division.
  HWY_INLINE Vec128& operator*=(const Vec128 other) {
    return *this = (*this * other);
  }
  HWY_INLINE Vec128& operator/=(const Vec128 other) {
    return *this = (*this / other);
  }
  HWY_INLINE Vec128& operator+=(const Vec128 other) {
    return *this = (*this + other);
  }
  HWY_INLINE Vec128& operator-=(const Vec128 other) {
    return *this = (*this - other);
  }
  HWY_INLINE Vec128& operator&=(const Vec128 other) {
    return *this = (*this & other);
  }
  HWY_INLINE Vec128& operator|=(const Vec128 other) {
    return *this = (*this | other);
  }
  HWY_INLINE Vec128& operator^=(const Vec128 other) {
    return *this = (*this ^ other);
  }

  Raw raw;
};

template <typename T>
using Vec64 = Vec128<T, 8 / sizeof(T)>;

template <typename T>
using Vec32 = Vec128<T, 4 / sizeof(T)>;

// FF..FF or 0.
template <typename T, size_t N = 16 / sizeof(T)>
class Mask128 {
  // ARM C Language Extensions return and expect unsigned type.
  using Raw = typename detail::Raw128<MakeUnsigned<T>, N>::type;

 public:
  HWY_INLINE Mask128() {}
  Mask128(const Mask128&) = default;
  Mask128& operator=(const Mask128&) = default;
  HWY_INLINE explicit Mask128(const Raw raw) : raw(raw) {}

  Raw raw;
};

template <typename T>
using Mask64 = Mask128<T, 8 / sizeof(T)>;

namespace detail {

// Deduce Simd<T, N, 0> from Vec128<T, N>
struct DeduceD {
  template <typename T, size_t N>
  Simd<T, N, 0> operator()(Vec128<T, N>) const {
    return Simd<T, N, 0>();
  }
};

}  // namespace detail

template <class V>
using DFromV = decltype(detail::DeduceD()(V()));

template <class V>
using TFromV = TFromD<DFromV<V>>;

// ------------------------------ BitCast

namespace detail {

// Converts from Vec128<T, N> to Vec128<uint8_t, N * sizeof(T)> using the
// vreinterpret*_u8_*() set of functions.
#define HWY_NEON_BUILD_TPL_HWY_CAST_TO_U8
#define HWY_NEON_BUILD_RET_HWY_CAST_TO_U8(type, size) \
  Vec128<uint8_t, size * sizeof(type##_t)>
#define HWY_NEON_BUILD_PARAM_HWY_CAST_TO_U8(type, size) Vec128<type##_t, size> v
#define HWY_NEON_BUILD_ARG_HWY_CAST_TO_U8 v.raw

// Special case of u8 to u8 since vreinterpret*_u8_u8 is obviously not defined.
template <size_t N>
HWY_INLINE Vec128<uint8_t, N> BitCastToByte(Vec128<uint8_t, N> v) {
  return v;
}

HWY_NEON_DEF_FUNCTION_ALL_FLOATS(BitCastToByte, vreinterpret, _u8_,
                                 HWY_CAST_TO_U8)
HWY_NEON_DEF_FUNCTION_INTS(BitCastToByte, vreinterpret, _u8_, HWY_CAST_TO_U8)
HWY_NEON_DEF_FUNCTION_UINT_16(BitCastToByte, vreinterpret, _u8_, HWY_CAST_TO_U8)
HWY_NEON_DEF_FUNCTION_UINT_32(BitCastToByte, vreinterpret, _u8_, HWY_CAST_TO_U8)
HWY_NEON_DEF_FUNCTION_UINT_64(BitCastToByte, vreinterpret, _u8_, HWY_CAST_TO_U8)

// Special cases for [b]float16_t, which have the same Raw as uint16_t.
template <size_t N>
HWY_INLINE Vec128<uint8_t, N * 2> BitCastToByte(Vec128<float16_t, N> v) {
  return BitCastToByte(Vec128<uint16_t, N>(v.raw));
}
template <size_t N>
HWY_INLINE Vec128<uint8_t, N * 2> BitCastToByte(Vec128<bfloat16_t, N> v) {
  return BitCastToByte(Vec128<uint16_t, N>(v.raw));
}

#undef HWY_NEON_BUILD_TPL_HWY_CAST_TO_U8
#undef HWY_NEON_BUILD_RET_HWY_CAST_TO_U8
#undef HWY_NEON_BUILD_PARAM_HWY_CAST_TO_U8
#undef HWY_NEON_BUILD_ARG_HWY_CAST_TO_U8

template <size_t N>
HWY_INLINE Vec128<uint8_t, N> BitCastFromByte(Simd<uint8_t, N, 0> /* tag */,
                                              Vec128<uint8_t, N> v) {
  return v;
}

// 64-bit or less:

template <size_t N, HWY_IF_LE64(int8_t, N)>
HWY_INLINE Vec128<int8_t, N> BitCastFromByte(Simd<int8_t, N, 0> /* tag */,
                                             Vec128<uint8_t, N> v) {
  return Vec128<int8_t, N>(vreinterpret_s8_u8(v.raw));
}
template <size_t N, HWY_IF_LE64(uint16_t, N)>
HWY_INLINE Vec128<uint16_t, N> BitCastFromByte(Simd<uint16_t, N, 0> /* tag */,
                                               Vec128<uint8_t, N * 2> v) {
  return Vec128<uint16_t, N>(vreinterpret_u16_u8(v.raw));
}
template <size_t N, HWY_IF_LE64(int16_t, N)>
HWY_INLINE Vec128<int16_t, N> BitCastFromByte(Simd<int16_t, N, 0> /* tag */,
                                              Vec128<uint8_t, N * 2> v) {
  return Vec128<int16_t, N>(vreinterpret_s16_u8(v.raw));
}
template <size_t N, HWY_IF_LE64(uint32_t, N)>
HWY_INLINE Vec128<uint32_t, N> BitCastFromByte(Simd<uint32_t, N, 0> /* tag */,
                                               Vec128<uint8_t, N * 4> v) {
  return Vec128<uint32_t, N>(vreinterpret_u32_u8(v.raw));
}
template <size_t N, HWY_IF_LE64(int32_t, N)>
HWY_INLINE Vec128<int32_t, N> BitCastFromByte(Simd<int32_t, N, 0> /* tag */,
                                              Vec128<uint8_t, N * 4> v) {
  return Vec128<int32_t, N>(vreinterpret_s32_u8(v.raw));
}
template <size_t N, HWY_IF_LE64(float, N)>
HWY_INLINE Vec128<float, N> BitCastFromByte(Simd<float, N, 0> /* tag */,
                                            Vec128<uint8_t, N * 4> v) {
  return Vec128<float, N>(vreinterpret_f32_u8(v.raw));
}
HWY_INLINE Vec64<uint64_t> BitCastFromByte(Full64<uint64_t> /* tag */,
                                           Vec128<uint8_t, 1 * 8> v) {
  return Vec64<uint64_t>(vreinterpret_u64_u8(v.raw));
}
HWY_INLINE Vec64<int64_t> BitCastFromByte(Full64<int64_t> /* tag */,
                                          Vec128<uint8_t, 1 * 8> v) {
  return Vec64<int64_t>(vreinterpret_s64_u8(v.raw));
}
#if HWY_ARCH_ARM_A64
HWY_INLINE Vec64<double> BitCastFromByte(Full64<double> /* tag */,
                                         Vec128<uint8_t, 1 * 8> v) {
  return Vec64<double>(vreinterpret_f64_u8(v.raw));
}
#endif

// 128-bit full:

HWY_INLINE Vec128<int8_t> BitCastFromByte(Full128<int8_t> /* tag */,
                                          Vec128<uint8_t> v) {
  return Vec128<int8_t>(vreinterpretq_s8_u8(v.raw));
}
HWY_INLINE Vec128<uint16_t> BitCastFromByte(Full128<uint16_t> /* tag */,
                                            Vec128<uint8_t> v) {
  return Vec128<uint16_t>(vreinterpretq_u16_u8(v.raw));
}
HWY_INLINE Vec128<int16_t> BitCastFromByte(Full128<int16_t> /* tag */,
                                           Vec128<uint8_t> v) {
  return Vec128<int16_t>(vreinterpretq_s16_u8(v.raw));
}
HWY_INLINE Vec128<uint32_t> BitCastFromByte(Full128<uint32_t> /* tag */,
                                            Vec128<uint8_t> v) {
  return Vec128<uint32_t>(vreinterpretq_u32_u8(v.raw));
}
HWY_INLINE Vec128<int32_t> BitCastFromByte(Full128<int32_t> /* tag */,
                                           Vec128<uint8_t> v) {
  return Vec128<int32_t>(vreinterpretq_s32_u8(v.raw));
}
HWY_INLINE Vec128<float> BitCastFromByte(Full128<float> /* tag */,
                                         Vec128<uint8_t> v) {
  return Vec128<float>(vreinterpretq_f32_u8(v.raw));
}
HWY_INLINE Vec128<uint64_t> BitCastFromByte(Full128<uint64_t> /* tag */,
                                            Vec128<uint8_t> v) {
  return Vec128<uint64_t>(vreinterpretq_u64_u8(v.raw));
}
HWY_INLINE Vec128<int64_t> BitCastFromByte(Full128<int64_t> /* tag */,
                                           Vec128<uint8_t> v) {
  return Vec128<int64_t>(vreinterpretq_s64_u8(v.raw));
}

#if HWY_ARCH_ARM_A64
HWY_INLINE Vec128<double> BitCastFromByte(Full128<double> /* tag */,
                                          Vec128<uint8_t> v) {
  return Vec128<double>(vreinterpretq_f64_u8(v.raw));
}
#endif

// Special cases for [b]float16_t, which have the same Raw as uint16_t.
template <size_t N>
HWY_INLINE Vec128<float16_t, N> BitCastFromByte(Simd<float16_t, N, 0> /* tag */,
                                                Vec128<uint8_t, N * 2> v) {
  return Vec128<float16_t, N>(BitCastFromByte(Simd<uint16_t, N, 0>(), v).raw);
}
template <size_t N>
HWY_INLINE Vec128<bfloat16_t, N> BitCastFromByte(
    Simd<bfloat16_t, N, 0> /* tag */, Vec128<uint8_t, N * 2> v) {
  return Vec128<bfloat16_t, N>(BitCastFromByte(Simd<uint16_t, N, 0>(), v).raw);
}

}  // namespace detail

template <typename T, size_t N, typename FromT>
HWY_API Vec128<T, N> BitCast(Simd<T, N, 0> d,
                             Vec128<FromT, N * sizeof(T) / sizeof(FromT)> v) {
  return detail::BitCastFromByte(d, detail::BitCastToByte(v));
}

// ------------------------------ Set

// Returns a vector with all lanes set to "t".
#define HWY_NEON_BUILD_TPL_HWY_SET1
#define HWY_NEON_BUILD_RET_HWY_SET1(type, size) Vec128<type##_t, size>
#define HWY_NEON_BUILD_PARAM_HWY_SET1(type, size) \
  Simd<type##_t, size, 0> /* tag */, const type##_t t
#define HWY_NEON_BUILD_ARG_HWY_SET1 t

HWY_NEON_DEF_FUNCTION_ALL_TYPES(Set, vdup, _n_, HWY_SET1)

#undef HWY_NEON_BUILD_TPL_HWY_SET1
#undef HWY_NEON_BUILD_RET_HWY_SET1
#undef HWY_NEON_BUILD_PARAM_HWY_SET1
#undef HWY_NEON_BUILD_ARG_HWY_SET1

// Returns an all-zero vector.
template <typename T, size_t N>
HWY_API Vec128<T, N> Zero(Simd<T, N, 0> d) {
  return Set(d, 0);
}

template <size_t N>
HWY_API Vec128<bfloat16_t, N> Zero(Simd<bfloat16_t, N, 0> /* tag */) {
  return Vec128<bfloat16_t, N>(Zero(Simd<uint16_t, N, 0>()).raw);
}

template <class D>
using VFromD = decltype(Zero(D()));

// Returns a vector with uninitialized elements.
template <typename T, size_t N>
HWY_API Vec128<T, N> Undefined(Simd<T, N, 0> /*d*/) {
  HWY_DIAGNOSTICS(push)
  HWY_DIAGNOSTICS_OFF(disable : 4701, ignored "-Wuninitialized")
  typename detail::Raw128<T, N>::type a;
  return Vec128<T, N>(a);
  HWY_DIAGNOSTICS(pop)
}

// Returns a vector with lane i=[0, N) set to "first" + i.
template <typename T, size_t N, typename T2>
Vec128<T, N> Iota(const Simd<T, N, 0> d, const T2 first) {
  HWY_ALIGN T lanes[16 / sizeof(T)];
  for (size_t i = 0; i < 16 / sizeof(T); ++i) {
    lanes[i] = static_cast<T>(first + static_cast<T2>(i));
  }
  return Load(d, lanes);
}

// ------------------------------ GetLane

namespace detail {
#define HWY_NEON_BUILD_TPL_HWY_GET template <size_t kLane>
#define HWY_NEON_BUILD_RET_HWY_GET(type, size) type##_t
#define HWY_NEON_BUILD_PARAM_HWY_GET(type, size) Vec128<type##_t, size> v
#define HWY_NEON_BUILD_ARG_HWY_GET v.raw, kLane

HWY_NEON_DEF_FUNCTION_ALL_TYPES(GetLane, vget, _lane_, HWY_GET)

#undef HWY_NEON_BUILD_TPL_HWY_GET
#undef HWY_NEON_BUILD_RET_HWY_GET
#undef HWY_NEON_BUILD_PARAM_HWY_GET
#undef HWY_NEON_BUILD_ARG_HWY_GET

}  // namespace detail

template <class V>
HWY_API TFromV<V> GetLane(const V v) {
  return detail::GetLane<0>(v);
}

// ------------------------------ ExtractLane

// Requires one overload per vector length because GetLane<3> is a compile error
// if v is a uint32x2_t.
template <typename T>
HWY_API T ExtractLane(const Vec128<T, 1> v, size_t i) {
  HWY_DASSERT(i == 0);
  (void)i;
  return detail::GetLane<0>(v);
}

template <typename T>
HWY_API T ExtractLane(const Vec128<T, 2> v, size_t i) {
#if !HWY_IS_DEBUG_BUILD && HWY_COMPILER_GCC  // includes clang
  if (__builtin_constant_p(i)) {
    switch (i) {
      case 0:
        return detail::GetLane<0>(v);
      case 1:
        return detail::GetLane<1>(v);
    }
  }
#endif
  alignas(16) T lanes[2];
  Store(v, DFromV<decltype(v)>(), lanes);
  return lanes[i];
}

template <typename T>
HWY_API T ExtractLane(const Vec128<T, 4> v, size_t i) {
#if !HWY_IS_DEBUG_BUILD && HWY_COMPILER_GCC  // includes clang
  if (__builtin_constant_p(i)) {
    switch (i) {
      case 0:
        return detail::GetLane<0>(v);
      case 1:
        return detail::GetLane<1>(v);
      case 2:
        return detail::GetLane<2>(v);
      case 3:
        return detail::GetLane<3>(v);
    }
  }
#endif
  alignas(16) T lanes[4];
  Store(v, DFromV<decltype(v)>(), lanes);
  return lanes[i];
}

template <typename T>
HWY_API T ExtractLane(const Vec128<T, 8> v, size_t i) {
#if !HWY_IS_DEBUG_BUILD && HWY_COMPILER_GCC  // includes clang
  if (__builtin_constant_p(i)) {
    switch (i) {
      case 0:
        return detail::GetLane<0>(v);
      case 1:
        return detail::GetLane<1>(v);
      case 2:
        return detail::GetLane<2>(v);
      case 3:
        return detail::GetLane<3>(v);
      case 4:
        return detail::GetLane<4>(v);
      case 5:
        return detail::GetLane<5>(v);
      case 6:
        return detail::GetLane<6>(v);
      case 7:
        return detail::GetLane<7>(v);
    }
  }
#endif
  alignas(16) T lanes[8];
  Store(v, DFromV<decltype(v)>(), lanes);
  return lanes[i];
}

template <typename T>
HWY_API T ExtractLane(const Vec128<T, 16> v, size_t i) {
#if !HWY_IS_DEBUG_BUILD && HWY_COMPILER_GCC  // includes clang
  if (__builtin_constant_p(i)) {
    switch (i) {
      case 0:
        return detail::GetLane<0>(v);
      case 1:
        return detail::GetLane<1>(v);
      case 2:
        return detail::GetLane<2>(v);
      case 3:
        return detail::GetLane<3>(v);
      case 4:
        return detail::GetLane<4>(v);
      case 5:
        return detail::GetLane<5>(v);
      case 6:
        return detail::GetLane<6>(v);
      case 7:
        return detail::GetLane<7>(v);
      case 8:
        return detail::GetLane<8>(v);
      case 9:
        return detail::GetLane<9>(v);
      case 10:
        return detail::GetLane<10>(v);
      case 11:
        return detail::GetLane<11>(v);
      case 12:
        return detail::GetLane<12>(v);
      case 13:
        return detail::GetLane<13>(v);
      case 14:
        return detail::GetLane<14>(v);
      case 15:
        return detail::GetLane<15>(v);
    }
  }
#endif
  alignas(16) T lanes[16];
  Store(v, DFromV<decltype(v)>(), lanes);
  return lanes[i];
}

// ------------------------------ InsertLane

namespace detail {
#define HWY_NEON_BUILD_TPL_HWY_INSERT template <size_t kLane>
#define HWY_NEON_BUILD_RET_HWY_INSERT(type, size) Vec128<type##_t, size>
#define HWY_NEON_BUILD_PARAM_HWY_INSERT(type, size) \
  Vec128<type##_t, size> v, type##_t t
#define HWY_NEON_BUILD_ARG_HWY_INSERT t, v.raw, kLane

HWY_NEON_DEF_FUNCTION_ALL_TYPES(InsertLane, vset, _lane_, HWY_INSERT)

#undef HWY_NEON_BUILD_TPL_HWY_INSERT
#undef HWY_NEON_BUILD_RET_HWY_INSERT
#undef HWY_NEON_BUILD_PARAM_HWY_INSERT
#undef HWY_NEON_BUILD_ARG_HWY_INSERT

}  // namespace detail

// Requires one overload per vector length because InsertLane<3> may be a
// compile error.

template <typename T>
HWY_API Vec128<T, 1> InsertLane(const Vec128<T, 1> v, size_t i, T t) {
  HWY_DASSERT(i == 0);
  (void)i;
  return Set(DFromV<decltype(v)>(), t);
}

template <typename T>
HWY_API Vec128<T, 2> InsertLane(const Vec128<T, 2> v, size_t i, T t) {
#if !HWY_IS_DEBUG_BUILD && HWY_COMPILER_GCC  // includes clang
  if (__builtin_constant_p(i)) {
    switch (i) {
      case 0:
        return detail::InsertLane<0>(v, t);
      case 1:
        return detail::InsertLane<1>(v, t);
    }
  }
#endif
  const DFromV<decltype(v)> d;
  alignas(16) T lanes[2];
  Store(v, d, lanes);
  lanes[i] = t;
  return Load(d, lanes);
}

template <typename T>
HWY_API Vec128<T, 4> InsertLane(const Vec128<T, 4> v, size_t i, T t) {
#if !HWY_IS_DEBUG_BUILD && HWY_COMPILER_GCC  // includes clang
  if (__builtin_constant_p(i)) {
    switch (i) {
      case 0:
        return detail::InsertLane<0>(v, t);
      case 1:
        return detail::InsertLane<1>(v, t);
      case 2:
        return detail::InsertLane<2>(v, t);
      case 3:
        return detail::InsertLane<3>(v, t);
    }
  }
#endif
  const DFromV<decltype(v)> d;
  alignas(16) T lanes[4];
  Store(v, d, lanes);
  lanes[i] = t;
  return Load(d, lanes);
}

template <typename T>
HWY_API Vec128<T, 8> InsertLane(const Vec128<T, 8> v, size_t i, T t) {
#if !HWY_IS_DEBUG_BUILD && HWY_COMPILER_GCC  // includes clang
  if (__builtin_constant_p(i)) {
    switch (i) {
      case 0:
        return detail::InsertLane<0>(v, t);
      case 1:
        return detail::InsertLane<1>(v, t);
      case 2:
        return detail::InsertLane<2>(v, t);
      case 3:
        return detail::InsertLane<3>(v, t);
      case 4:
        return detail::InsertLane<4>(v, t);
      case 5:
        return detail::InsertLane<5>(v, t);
      case 6:
        return detail::InsertLane<6>(v, t);
      case 7:
        return detail::InsertLane<7>(v, t);
    }
  }
#endif
  const DFromV<decltype(v)> d;
  alignas(16) T lanes[8];
  Store(v, d, lanes);
  lanes[i] = t;
  return Load(d, lanes);
}

template <typename T>
HWY_API Vec128<T, 16> InsertLane(const Vec128<T, 16> v, size_t i, T t) {
#if !HWY_IS_DEBUG_BUILD && HWY_COMPILER_GCC  // includes clang
  if (__builtin_constant_p(i)) {
    switch (i) {
      case 0:
        return detail::InsertLane<0>(v, t);
      case 1:
        return detail::InsertLane<1>(v, t);
      case 2:
        return detail::InsertLane<2>(v, t);
      case 3:
        return detail::InsertLane<3>(v, t);
      case 4:
        return detail::InsertLane<4>(v, t);
      case 5:
        return detail::InsertLane<5>(v, t);
      case 6:
        return detail::InsertLane<6>(v, t);
      case 7:
        return detail::InsertLane<7>(v, t);
      case 8:
        return detail::InsertLane<8>(v, t);
      case 9:
        return detail::InsertLane<9>(v, t);
      case 10:
        return detail::InsertLane<10>(v, t);
      case 11:
        return detail::InsertLane<11>(v, t);
      case 12:
        return detail::InsertLane<12>(v, t);
      case 13:
        return detail::InsertLane<13>(v, t);
      case 14:
        return detail::InsertLane<14>(v, t);
      case 15:
        return detail::InsertLane<15>(v, t);
    }
  }
#endif
  const DFromV<decltype(v)> d;
  alignas(16) T lanes[16];
  Store(v, d, lanes);
  lanes[i] = t;
  return Load(d, lanes);
}

// ================================================== ARITHMETIC

// ------------------------------ Addition
HWY_NEON_DEF_FUNCTION_ALL_TYPES(operator+, vadd, _, 2)

// ------------------------------ Subtraction
HWY_NEON_DEF_FUNCTION_ALL_TYPES(operator-, vsub, _, 2)

// ------------------------------ SumsOf8

HWY_API Vec128<uint64_t> SumsOf8(const Vec128<uint8_t> v) {
  return Vec128<uint64_t>(vpaddlq_u32(vpaddlq_u16(vpaddlq_u8(v.raw))));
}
HWY_API Vec64<uint64_t> SumsOf8(const Vec64<uint8_t> v) {
  return Vec64<uint64_t>(vpaddl_u32(vpaddl_u16(vpaddl_u8(v.raw))));
}

// ------------------------------ SaturatedAdd
// Only defined for uint8_t, uint16_t and their signed versions, as in other
// architectures.

// Returns a + b clamped to the destination range.
HWY_NEON_DEF_FUNCTION_INT_8(SaturatedAdd, vqadd, _, 2)
HWY_NEON_DEF_FUNCTION_INT_16(SaturatedAdd, vqadd, _, 2)
HWY_NEON_DEF_FUNCTION_UINT_8(SaturatedAdd, vqadd, _, 2)
HWY_NEON_DEF_FUNCTION_UINT_16(SaturatedAdd, vqadd, _, 2)

// ------------------------------ SaturatedSub

// Returns a - b clamped to the destination range.
HWY_NEON_DEF_FUNCTION_INT_8(SaturatedSub, vqsub, _, 2)
HWY_NEON_DEF_FUNCTION_INT_16(SaturatedSub, vqsub, _, 2)
HWY_NEON_DEF_FUNCTION_UINT_8(SaturatedSub, vqsub, _, 2)
HWY_NEON_DEF_FUNCTION_UINT_16(SaturatedSub, vqsub, _, 2)

// Not part of API, used in implementation.
namespace detail {
HWY_NEON_DEF_FUNCTION_UINT_32(SaturatedSub, vqsub, _, 2)
HWY_NEON_DEF_FUNCTION_UINT_64(SaturatedSub, vqsub, _, 2)
HWY_NEON_DEF_FUNCTION_INT_32(SaturatedSub, vqsub, _, 2)
HWY_NEON_DEF_FUNCTION_INT_64(SaturatedSub, vqsub, _, 2)
}  // namespace detail

// ------------------------------ Average

// Returns (a + b + 1) / 2
HWY_NEON_DEF_FUNCTION_UINT_8(AverageRound, vrhadd, _, 2)
HWY_NEON_DEF_FUNCTION_UINT_16(AverageRound, vrhadd, _, 2)

// ------------------------------ Neg

HWY_NEON_DEF_FUNCTION_ALL_FLOATS(Neg, vneg, _, 1)
HWY_NEON_DEF_FUNCTION_INT_8_16_32(Neg, vneg, _, 1)  // i64 implemented below

HWY_API Vec64<int64_t> Neg(const Vec64<int64_t> v) {
#if HWY_ARCH_ARM_A64
  return Vec64<int64_t>(vneg_s64(v.raw));
#else
  return Zero(Full64<int64_t>()) - v;
#endif
}

HWY_API Vec128<int64_t> Neg(const Vec128<int64_t> v) {
#if HWY_ARCH_ARM_A64
  return Vec128<int64_t>(vnegq_s64(v.raw));
#else
  return Zero(Full128<int64_t>()) - v;
#endif
}

// ------------------------------ ShiftLeft

// Customize HWY_NEON_DEF_FUNCTION to special-case count=0 (not supported).
#pragma push_macro("HWY_NEON_DEF_FUNCTION")
#undef HWY_NEON_DEF_FUNCTION
#define HWY_NEON_DEF_FUNCTION(type, size, name, prefix, infix, suffix, args)   \
  template <int kBits>                                                         \
  HWY_API Vec128<type##_t, size> name(const Vec128<type##_t, size> v) {        \
    return kBits == 0 ? v                                                      \
                      : Vec128<type##_t, size>(HWY_NEON_EVAL(                  \
                            prefix##infix##suffix, v.raw, HWY_MAX(1, kBits))); \
  }

HWY_NEON_DEF_FUNCTION_INTS_UINTS(ShiftLeft, vshl, _n_, ignored)

HWY_NEON_DEF_FUNCTION_UINTS(ShiftRight, vshr, _n_, ignored)
HWY_NEON_DEF_FUNCTION_INTS(ShiftRight, vshr, _n_, ignored)

#pragma pop_macro("HWY_NEON_DEF_FUNCTION")

// ------------------------------ RotateRight (ShiftRight, Or)

template <int kBits, size_t N>
HWY_API Vec128<uint32_t, N> RotateRight(const Vec128<uint32_t, N> v) {
  static_assert(0 <= kBits && kBits < 32, "Invalid shift count");
  if (kBits == 0) return v;
  return Or(ShiftRight<kBits>(v), ShiftLeft<HWY_MIN(31, 32 - kBits)>(v));
}

template <int kBits, size_t N>
HWY_API Vec128<uint64_t, N> RotateRight(const Vec128<uint64_t, N> v) {
  static_assert(0 <= kBits && kBits < 64, "Invalid shift count");
  if (kBits == 0) return v;
  return Or(ShiftRight<kBits>(v), ShiftLeft<HWY_MIN(63, 64 - kBits)>(v));
}

// NOTE: vxarq_u64 can be applied to uint64_t, but we do not yet have a
// mechanism for checking for extensions to ARMv8.

// ------------------------------ Shl

HWY_API Vec128<uint8_t> operator<<(const Vec128<uint8_t> v,
                                   const Vec128<uint8_t> bits) {
  return Vec128<uint8_t>(vshlq_u8(v.raw, vreinterpretq_s8_u8(bits.raw)));
}
template <size_t N, HWY_IF_LE64(uint8_t, N)>
HWY_API Vec128<uint8_t, N> operator<<(const Vec128<uint8_t, N> v,
                                      const Vec128<uint8_t, N> bits) {
  return Vec128<uint8_t, N>(vshl_u8(v.raw, vreinterpret_s8_u8(bits.raw)));
}

HWY_API Vec128<uint16_t> operator<<(const Vec128<uint16_t> v,
                                    const Vec128<uint16_t> bits) {
  return Vec128<uint16_t>(vshlq_u16(v.raw, vreinterpretq_s16_u16(bits.raw)));
}
template <size_t N, HWY_IF_LE64(uint16_t, N)>
HWY_API Vec128<uint16_t, N> operator<<(const Vec128<uint16_t, N> v,
                                       const Vec128<uint16_t, N> bits) {
  return Vec128<uint16_t, N>(vshl_u16(v.raw, vreinterpret_s16_u16(bits.raw)));
}

HWY_API Vec128<uint32_t> operator<<(const Vec128<uint32_t> v,
                                    const Vec128<uint32_t> bits) {
  return Vec128<uint32_t>(vshlq_u32(v.raw, vreinterpretq_s32_u32(bits.raw)));
}
template <size_t N, HWY_IF_LE64(uint32_t, N)>
HWY_API Vec128<uint32_t, N> operator<<(const Vec128<uint32_t, N> v,
                                       const Vec128<uint32_t, N> bits) {
  return Vec128<uint32_t, N>(vshl_u32(v.raw, vreinterpret_s32_u32(bits.raw)));
}

HWY_API Vec128<uint64_t> operator<<(const Vec128<uint64_t> v,
                                    const Vec128<uint64_t> bits) {
  return Vec128<uint64_t>(vshlq_u64(v.raw, vreinterpretq_s64_u64(bits.raw)));
}
HWY_API Vec64<uint64_t> operator<<(const Vec64<uint64_t> v,
                                   const Vec64<uint64_t> bits) {
  return Vec64<uint64_t>(vshl_u64(v.raw, vreinterpret_s64_u64(bits.raw)));
}

HWY_API Vec128<int8_t> operator<<(const Vec128<int8_t> v,
                                  const Vec128<int8_t> bits) {
  return Vec128<int8_t>(vshlq_s8(v.raw, bits.raw));
}
template <size_t N, HWY_IF_LE64(int8_t, N)>
HWY_API Vec128<int8_t, N> operator<<(const Vec128<int8_t, N> v,
                                     const Vec128<int8_t, N> bits) {
  return Vec128<int8_t, N>(vshl_s8(v.raw, bits.raw));
}

HWY_API Vec128<int16_t> operator<<(const Vec128<int16_t> v,
                                   const Vec128<int16_t> bits) {
  return Vec128<int16_t>(vshlq_s16(v.raw, bits.raw));
}
template <size_t N, HWY_IF_LE64(int16_t, N)>
HWY_API Vec128<int16_t, N> operator<<(const Vec128<int16_t, N> v,
                                      const Vec128<int16_t, N> bits) {
  return Vec128<int16_t, N>(vshl_s16(v.raw, bits.raw));
}

HWY_API Vec128<int32_t> operator<<(const Vec128<int32_t> v,
                                   const Vec128<int32_t> bits) {
  return Vec128<int32_t>(vshlq_s32(v.raw, bits.raw));
}
template <size_t N, HWY_IF_LE64(int32_t, N)>
HWY_API Vec128<int32_t, N> operator<<(const Vec128<int32_t, N> v,
                                      const Vec128<int32_t, N> bits) {
  return Vec128<int32_t, N>(vshl_s32(v.raw, bits.raw));
}

HWY_API Vec128<int64_t> operator<<(const Vec128<int64_t> v,
                                   const Vec128<int64_t> bits) {
  return Vec128<int64_t>(vshlq_s64(v.raw, bits.raw));
}
HWY_API Vec64<int64_t> operator<<(const Vec64<int64_t> v,
                                  const Vec64<int64_t> bits) {
  return Vec64<int64_t>(vshl_s64(v.raw, bits.raw));
}

// ------------------------------ Shr (Neg)

HWY_API Vec128<uint8_t> operator>>(const Vec128<uint8_t> v,
                                   const Vec128<uint8_t> bits) {
  const int8x16_t neg_bits = Neg(BitCast(Full128<int8_t>(), bits)).raw;
  return Vec128<uint8_t>(vshlq_u8(v.raw, neg_bits));
}
template <size_t N, HWY_IF_LE64(uint8_t, N)>
HWY_API Vec128<uint8_t, N> operator>>(const Vec128<uint8_t, N> v,
                                      const Vec128<uint8_t, N> bits) {
  const int8x8_t neg_bits = Neg(BitCast(Simd<int8_t, N, 0>(), bits)).raw;
  return Vec128<uint8_t, N>(vshl_u8(v.raw, neg_bits));
}

HWY_API Vec128<uint16_t> operator>>(const Vec128<uint16_t> v,
                                    const Vec128<uint16_t> bits) {
  const int16x8_t neg_bits = Neg(BitCast(Full128<int16_t>(), bits)).raw;
  return Vec128<uint16_t>(vshlq_u16(v.raw, neg_bits));
}
template <size_t N, HWY_IF_LE64(uint16_t, N)>
HWY_API Vec128<uint16_t, N> operator>>(const Vec128<uint16_t, N> v,
                                       const Vec128<uint16_t, N> bits) {
  const int16x4_t neg_bits = Neg(BitCast(Simd<int16_t, N, 0>(), bits)).raw;
  return Vec128<uint16_t, N>(vshl_u16(v.raw, neg_bits));
}

HWY_API Vec128<uint32_t> operator>>(const Vec128<uint32_t> v,
                                    const Vec128<uint32_t> bits) {
  const int32x4_t neg_bits = Neg(BitCast(Full128<int32_t>(), bits)).raw;
  return Vec128<uint32_t>(vshlq_u32(v.raw, neg_bits));
}
template <size_t N, HWY_IF_LE64(uint32_t, N)>
HWY_API Vec128<uint32_t, N> operator>>(const Vec128<uint32_t, N> v,
                                       const Vec128<uint32_t, N> bits) {
  const int32x2_t neg_bits = Neg(BitCast(Simd<int32_t, N, 0>(), bits)).raw;
  return Vec128<uint32_t, N>(vshl_u32(v.raw, neg_bits));
}

HWY_API Vec128<uint64_t> operator>>(const Vec128<uint64_t> v,
                                    const Vec128<uint64_t> bits) {
  const int64x2_t neg_bits = Neg(BitCast(Full128<int64_t>(), bits)).raw;
  return Vec128<uint64_t>(vshlq_u64(v.raw, neg_bits));
}
HWY_API Vec64<uint64_t> operator>>(const Vec64<uint64_t> v,
                                   const Vec64<uint64_t> bits) {
  const int64x1_t neg_bits = Neg(BitCast(Full64<int64_t>(), bits)).raw;
  return Vec64<uint64_t>(vshl_u64(v.raw, neg_bits));
}

HWY_API Vec128<int8_t> operator>>(const Vec128<int8_t> v,
                                  const Vec128<int8_t> bits) {
  return Vec128<int8_t>(vshlq_s8(v.raw, Neg(bits).raw));
}
template <size_t N, HWY_IF_LE64(int8_t, N)>
HWY_API Vec128<int8_t, N> operator>>(const Vec128<int8_t, N> v,
                                     const Vec128<int8_t, N> bits) {
  return Vec128<int8_t, N>(vshl_s8(v.raw, Neg(bits).raw));
}

HWY_API Vec128<int16_t> operator>>(const Vec128<int16_t> v,
                                   const Vec128<int16_t> bits) {
  return Vec128<int16_t>(vshlq_s16(v.raw, Neg(bits).raw));
}
template <size_t N, HWY_IF_LE64(int16_t, N)>
HWY_API Vec128<int16_t, N> operator>>(const Vec128<int16_t, N> v,
                                      const Vec128<int16_t, N> bits) {
  return Vec128<int16_t, N>(vshl_s16(v.raw, Neg(bits).raw));
}

HWY_API Vec128<int32_t> operator>>(const Vec128<int32_t> v,
                                   const Vec128<int32_t> bits) {
  return Vec128<int32_t>(vshlq_s32(v.raw, Neg(bits).raw));
}
template <size_t N, HWY_IF_LE64(int32_t, N)>
HWY_API Vec128<int32_t, N> operator>>(const Vec128<int32_t, N> v,
                                      const Vec128<int32_t, N> bits) {
  return Vec128<int32_t, N>(vshl_s32(v.raw, Neg(bits).raw));
}

HWY_API Vec128<int64_t> operator>>(const Vec128<int64_t> v,
                                   const Vec128<int64_t> bits) {
  return Vec128<int64_t>(vshlq_s64(v.raw, Neg(bits).raw));
}
HWY_API Vec64<int64_t> operator>>(const Vec64<int64_t> v,
                                  const Vec64<int64_t> bits) {
  return Vec64<int64_t>(vshl_s64(v.raw, Neg(bits).raw));
}

// ------------------------------ ShiftLeftSame (Shl)

template <typename T, size_t N>
HWY_API Vec128<T, N> ShiftLeftSame(const Vec128<T, N> v, int bits) {
  return v << Set(Simd<T, N, 0>(), static_cast<T>(bits));
}
template <typename T, size_t N>
HWY_API Vec128<T, N> ShiftRightSame(const Vec128<T, N> v, int bits) {
  return v >> Set(Simd<T, N, 0>(), static_cast<T>(bits));
}

// ------------------------------ Integer multiplication

// Unsigned
HWY_API Vec128<uint16_t> operator*(const Vec128<uint16_t> a,
                                   const Vec128<uint16_t> b) {
  return Vec128<uint16_t>(vmulq_u16(a.raw, b.raw));
}
HWY_API Vec128<uint32_t> operator*(const Vec128<uint32_t> a,
                                   const Vec128<uint32_t> b) {
  return Vec128<uint32_t>(vmulq_u32(a.raw, b.raw));
}

template <size_t N, HWY_IF_LE64(uint16_t, N)>
HWY_API Vec128<uint16_t, N> operator*(const Vec128<uint16_t, N> a,
                                      const Vec128<uint16_t, N> b) {
  return Vec128<uint16_t, N>(vmul_u16(a.raw, b.raw));
}
template <size_t N, HWY_IF_LE64(uint32_t, N)>
HWY_API Vec128<uint32_t, N> operator*(const Vec128<uint32_t, N> a,
                                      const Vec128<uint32_t, N> b) {
  return Vec128<uint32_t, N>(vmul_u32(a.raw, b.raw));
}

// Signed
HWY_API Vec128<int16_t> operator*(const Vec128<int16_t> a,
                                  const Vec128<int16_t> b) {
  return Vec128<int16_t>(vmulq_s16(a.raw, b.raw));
}
HWY_API Vec128<int32_t> operator*(const Vec128<int32_t> a,
                                  const Vec128<int32_t> b) {
  return Vec128<int32_t>(vmulq_s32(a.raw, b.raw));
}

template <size_t N, HWY_IF_LE64(uint16_t, N)>
HWY_API Vec128<int16_t, N> operator*(const Vec128<int16_t, N> a,
                                     const Vec128<int16_t, N> b) {
  return Vec128<int16_t, N>(vmul_s16(a.raw, b.raw));
}
template <size_t N, HWY_IF_LE64(int32_t, N)>
HWY_API Vec128<int32_t, N> operator*(const Vec128<int32_t, N> a,
                                     const Vec128<int32_t, N> b) {
  return Vec128<int32_t, N>(vmul_s32(a.raw, b.raw));
}

// Returns the upper 16 bits of a * b in each lane.
HWY_API Vec128<int16_t> MulHigh(const Vec128<int16_t> a,
                                const Vec128<int16_t> b) {
  int32x4_t rlo = vmull_s16(vget_low_s16(a.raw), vget_low_s16(b.raw));
#if HWY_ARCH_ARM_A64
  int32x4_t rhi = vmull_high_s16(a.raw, b.raw);
#else
  int32x4_t rhi = vmull_s16(vget_high_s16(a.raw), vget_high_s16(b.raw));
#endif
  return Vec128<int16_t>(
      vuzp2q_s16(vreinterpretq_s16_s32(rlo), vreinterpretq_s16_s32(rhi)));
}
HWY_API Vec128<uint16_t> MulHigh(const Vec128<uint16_t> a,
                                 const Vec128<uint16_t> b) {
  uint32x4_t rlo = vmull_u16(vget_low_u16(a.raw), vget_low_u16(b.raw));
#if HWY_ARCH_ARM_A64
  uint32x4_t rhi = vmull_high_u16(a.raw, b.raw);
#else
  uint32x4_t rhi = vmull_u16(vget_high_u16(a.raw), vget_high_u16(b.raw));
#endif
  return Vec128<uint16_t>(
      vuzp2q_u16(vreinterpretq_u16_u32(rlo), vreinterpretq_u16_u32(rhi)));
}

template <size_t N, HWY_IF_LE64(int16_t, N)>
HWY_API Vec128<int16_t, N> MulHigh(const Vec128<int16_t, N> a,
                                   const Vec128<int16_t, N> b) {
  int16x8_t hi_lo = vreinterpretq_s16_s32(vmull_s16(a.raw, b.raw));
  return Vec128<int16_t, N>(vget_low_s16(vuzp2q_s16(hi_lo, hi_lo)));
}
template <size_t N, HWY_IF_LE64(uint16_t, N)>
HWY_API Vec128<uint16_t, N> MulHigh(const Vec128<uint16_t, N> a,
                                    const Vec128<uint16_t, N> b) {
  uint16x8_t hi_lo = vreinterpretq_u16_u32(vmull_u16(a.raw, b.raw));
  return Vec128<uint16_t, N>(vget_low_u16(vuzp2q_u16(hi_lo, hi_lo)));
}

HWY_API Vec128<int16_t> MulFixedPoint15(Vec128<int16_t> a, Vec128<int16_t> b) {
  return Vec128<int16_t>(vqrdmulhq_s16(a.raw, b.raw));
}
template <size_t N, HWY_IF_LE64(int16_t, N)>
HWY_API Vec128<int16_t, N> MulFixedPoint15(Vec128<int16_t, N> a,
                                           Vec128<int16_t, N> b) {
  return Vec128<int16_t, N>(vqrdmulh_s16(a.raw, b.raw));
}

// ------------------------------ Floating-point mul / div

HWY_NEON_DEF_FUNCTION_ALL_FLOATS(operator*, vmul, _, 2)

// Approximate reciprocal
HWY_API Vec128<float> ApproximateReciprocal(const Vec128<float> v) {
  return Vec128<float>(vrecpeq_f32(v.raw));
}
template <size_t N>
HWY_API Vec128<float, N> ApproximateReciprocal(const Vec128<float, N> v) {
  return Vec128<float, N>(vrecpe_f32(v.raw));
}

#if HWY_ARCH_ARM_A64
HWY_NEON_DEF_FUNCTION_ALL_FLOATS(operator/, vdiv, _, 2)
#else
// Not defined on armv7: approximate
namespace detail {

HWY_INLINE Vec128<float> ReciprocalNewtonRaphsonStep(
    const Vec128<float> recip, const Vec128<float> divisor) {
  return Vec128<float>(vrecpsq_f32(recip.raw, divisor.raw));
}
template <size_t N>
HWY_INLINE Vec128<float, N> ReciprocalNewtonRaphsonStep(
    const Vec128<float, N> recip, Vec128<float, N> divisor) {
  return Vec128<float, N>(vrecps_f32(recip.raw, divisor.raw));
}

}  // namespace detail

template <size_t N>
HWY_API Vec128<float, N> operator/(const Vec128<float, N> a,
                                   const Vec128<float, N> b) {
  auto x = ApproximateReciprocal(b);
  x *= detail::ReciprocalNewtonRaphsonStep(x, b);
  x *= detail::ReciprocalNewtonRaphsonStep(x, b);
  x *= detail::ReciprocalNewtonRaphsonStep(x, b);
  return a * x;
}
#endif

// ------------------------------ Absolute value of difference.

HWY_API Vec128<float> AbsDiff(const Vec128<float> a, const Vec128<float> b) {
  return Vec128<float>(vabdq_f32(a.raw, b.raw));
}
template <size_t N, HWY_IF_LE64(float, N)>
HWY_API Vec128<float, N> AbsDiff(const Vec128<float, N> a,
                                 const Vec128<float, N> b) {
  return Vec128<float, N>(vabd_f32(a.raw, b.raw));
}

// ------------------------------ Floating-point multiply-add variants

// Returns add + mul * x
#if defined(__ARM_VFPV4__) || HWY_ARCH_ARM_A64
template <size_t N, HWY_IF_LE64(float, N)>
HWY_API Vec128<float, N> MulAdd(const Vec128<float, N> mul,
                                const Vec128<float, N> x,
                                const Vec128<float, N> add) {
  return Vec128<float, N>(vfma_f32(add.raw, mul.raw, x.raw));
}
HWY_API Vec128<float> MulAdd(const Vec128<float> mul, const Vec128<float> x,
                             const Vec128<float> add) {
  return Vec128<float>(vfmaq_f32(add.raw, mul.raw, x.raw));
}
#else
// Emulate FMA for floats.
template <size_t N>
HWY_API Vec128<float, N> MulAdd(const Vec128<float, N> mul,
                                const Vec128<float, N> x,
                                const Vec128<float, N> add) {
  return mul * x + add;
}
#endif

#if HWY_ARCH_ARM_A64
HWY_API Vec64<double> MulAdd(const Vec64<double> mul, const Vec64<double> x,
                             const Vec64<double> add) {
  return Vec64<double>(vfma_f64(add.raw, mul.raw, x.raw));
}
HWY_API Vec128<double> MulAdd(const Vec128<double> mul, const Vec128<double> x,
                              const Vec128<double> add) {
  return Vec128<double>(vfmaq_f64(add.raw, mul.raw, x.raw));
}
#endif

// Returns add - mul * x
#if defined(__ARM_VFPV4__) || HWY_ARCH_ARM_A64
template <size_t N, HWY_IF_LE64(float, N)>
HWY_API Vec128<float, N> NegMulAdd(const Vec128<float, N> mul,
                                   const Vec128<float, N> x,
                                   const Vec128<float, N> add) {
  return Vec128<float, N>(vfms_f32(add.raw, mul.raw, x.raw));
}
HWY_API Vec128<float> NegMulAdd(const Vec128<float> mul, const Vec128<float> x,
                                const Vec128<float> add) {
  return Vec128<float>(vfmsq_f32(add.raw, mul.raw, x.raw));
}
#else
// Emulate FMA for floats.
template <size_t N>
HWY_API Vec128<float, N> NegMulAdd(const Vec128<float, N> mul,
                                   const Vec128<float, N> x,
                                   const Vec128<float, N> add) {
  return add - mul * x;
}
#endif

#if HWY_ARCH_ARM_A64
HWY_API Vec64<double> NegMulAdd(const Vec64<double> mul, const Vec64<double> x,
                                const Vec64<double> add) {
  return Vec64<double>(vfms_f64(add.raw, mul.raw, x.raw));
}
HWY_API Vec128<double> NegMulAdd(const Vec128<double> mul,
                                 const Vec128<double> x,
                                 const Vec128<double> add) {
  return Vec128<double>(vfmsq_f64(add.raw, mul.raw, x.raw));
}
#endif

// Returns mul * x - sub
template <size_t N>
HWY_API Vec128<float, N> MulSub(const Vec128<float, N> mul,
                                const Vec128<float, N> x,
                                const Vec128<float, N> sub) {
  return MulAdd(mul, x, Neg(sub));
}

// Returns -mul * x - sub
template <size_t N>
HWY_API Vec128<float, N> NegMulSub(const Vec128<float, N> mul,
                                   const Vec128<float, N> x,
                                   const Vec128<float, N> sub) {
  return Neg(MulAdd(mul, x, sub));
}

#if HWY_ARCH_ARM_A64
template <size_t N>
HWY_API Vec128<double, N> MulSub(const Vec128<double, N> mul,
                                 const Vec128<double, N> x,
                                 const Vec128<double, N> sub) {
  return MulAdd(mul, x, Neg(sub));
}
template <size_t N>
HWY_API Vec128<double, N> NegMulSub(const Vec128<double, N> mul,
                                    const Vec128<double, N> x,
                                    const Vec128<double, N> sub) {
  return Neg(MulAdd(mul, x, sub));
}
#endif

// ------------------------------ Floating-point square root (IfThenZeroElse)

// Approximate reciprocal square root
HWY_API Vec128<float> ApproximateReciprocalSqrt(const Vec128<float> v) {
  return Vec128<float>(vrsqrteq_f32(v.raw));
}
template <size_t N>
HWY_API Vec128<float, N> ApproximateReciprocalSqrt(const Vec128<float, N> v) {
  return Vec128<float, N>(vrsqrte_f32(v.raw));
}

// Full precision square root
#if HWY_ARCH_ARM_A64
HWY_NEON_DEF_FUNCTION_ALL_FLOATS(Sqrt, vsqrt, _, 1)
#else
namespace detail {

HWY_INLINE Vec128<float> ReciprocalSqrtStep(const Vec128<float> root,
                                            const Vec128<float> recip) {
  return Vec128<float>(vrsqrtsq_f32(root.raw, recip.raw));
}
template <size_t N>
HWY_INLINE Vec128<float, N> ReciprocalSqrtStep(const Vec128<float, N> root,
                                               Vec128<float, N> recip) {
  return Vec128<float, N>(vrsqrts_f32(root.raw, recip.raw));
}

}  // namespace detail

// Not defined on armv7: approximate
template <size_t N>
HWY_API Vec128<float, N> Sqrt(const Vec128<float, N> v) {
  auto recip = ApproximateReciprocalSqrt(v);

  recip *= detail::ReciprocalSqrtStep(v * recip, recip);
  recip *= detail::ReciprocalSqrtStep(v * recip, recip);
  recip *= detail::ReciprocalSqrtStep(v * recip, recip);

  const auto root = v * recip;
  return IfThenZeroElse(v == Zero(Simd<float, N, 0>()), root);
}
#endif

// ================================================== LOGICAL

// ------------------------------ Not

// There is no 64-bit vmvn, so cast instead of using HWY_NEON_DEF_FUNCTION.
template <typename T>
HWY_API Vec128<T> Not(const Vec128<T> v) {
  const Full128<T> d;
  const Repartition<uint8_t, decltype(d)> d8;
  return BitCast(d, Vec128<uint8_t>(vmvnq_u8(BitCast(d8, v).raw)));
}
template <typename T, size_t N, HWY_IF_LE64(T, N)>
HWY_API Vec128<T, N> Not(const Vec128<T, N> v) {
  const Simd<T, N, 0> d;
  const Repartition<uint8_t, decltype(d)> d8;
  using V8 = decltype(Zero(d8));
  return BitCast(d, V8(vmvn_u8(BitCast(d8, v).raw)));
}

// ------------------------------ And
HWY_NEON_DEF_FUNCTION_INTS_UINTS(And, vand, _, 2)

// Uses the u32/64 defined above.
template <typename T, size_t N, HWY_IF_FLOAT(T)>
HWY_API Vec128<T, N> And(const Vec128<T, N> a, const Vec128<T, N> b) {
  const DFromV<decltype(a)> d;
  const RebindToUnsigned<decltype(d)> du;
  return BitCast(d, BitCast(du, a) & BitCast(du, b));
}

// ------------------------------ AndNot

namespace detail {
// reversed_andnot returns a & ~b.
HWY_NEON_DEF_FUNCTION_INTS_UINTS(reversed_andnot, vbic, _, 2)
}  // namespace detail

// Returns ~not_mask & mask.
template <typename T, size_t N, HWY_IF_NOT_FLOAT(T)>
HWY_API Vec128<T, N> AndNot(const Vec128<T, N> not_mask,
                            const Vec128<T, N> mask) {
  return detail::reversed_andnot(mask, not_mask);
}

// Uses the u32/64 defined above.
template <typename T, size_t N, HWY_IF_FLOAT(T)>
HWY_API Vec128<T, N> AndNot(const Vec128<T, N> not_mask,
                            const Vec128<T, N> mask) {
  const DFromV<decltype(mask)> d;
  const RebindToUnsigned<decltype(d)> du;
  VFromD<decltype(du)> ret =
      detail::reversed_andnot(BitCast(du, mask), BitCast(du, not_mask));
  return BitCast(d, ret);
}

// ------------------------------ Or

HWY_NEON_DEF_FUNCTION_INTS_UINTS(Or, vorr, _, 2)

// Uses the u32/64 defined above.
template <typename T, size_t N, HWY_IF_FLOAT(T)>
HWY_API Vec128<T, N> Or(const Vec128<T, N> a, const Vec128<T, N> b) {
  const DFromV<decltype(a)> d;
  const RebindToUnsigned<decltype(d)> du;
  return BitCast(d, BitCast(du, a) | BitCast(du, b));
}

// ------------------------------ Xor

HWY_NEON_DEF_FUNCTION_INTS_UINTS(Xor, veor, _, 2)

// Uses the u32/64 defined above.
template <typename T, size_t N, HWY_IF_FLOAT(T)>
HWY_API Vec128<T, N> Xor(const Vec128<T, N> a, const Vec128<T, N> b) {
  const DFromV<decltype(a)> d;
  const RebindToUnsigned<decltype(d)> du;
  return BitCast(d, BitCast(du, a) ^ BitCast(du, b));
}

// ------------------------------ Or3

template <typename T, size_t N>
HWY_API Vec128<T, N> Or3(Vec128<T, N> o1, Vec128<T, N> o2, Vec128<T, N> o3) {
  return Or(o1, Or(o2, o3));
}

// ------------------------------ OrAnd

template <typename T, size_t N>
HWY_API Vec128<T, N> OrAnd(Vec128<T, N> o, Vec128<T, N> a1, Vec128<T, N> a2) {
  return Or(o, And(a1, a2));
}

// ------------------------------ IfVecThenElse

template <typename T, size_t N>
HWY_API Vec128<T, N> IfVecThenElse(Vec128<T, N> mask, Vec128<T, N> yes,
                                   Vec128<T, N> no) {
  return IfThenElse(MaskFromVec(mask), yes, no);
}

// ------------------------------ Operator overloads (internal-only if float)

template <typename T, size_t N>
HWY_API Vec128<T, N> operator&(const Vec128<T, N> a, const Vec128<T, N> b) {
  return And(a, b);
}

template <typename T, size_t N>
HWY_API Vec128<T, N> operator|(const Vec128<T, N> a, const Vec128<T, N> b) {
  return Or(a, b);
}

template <typename T, size_t N>
HWY_API Vec128<T, N> operator^(const Vec128<T, N> a, const Vec128<T, N> b) {
  return Xor(a, b);
}

// ------------------------------ PopulationCount

#ifdef HWY_NATIVE_POPCNT
#undef HWY_NATIVE_POPCNT
#else
#define HWY_NATIVE_POPCNT
#endif

namespace detail {

template <typename T>
HWY_INLINE Vec128<T> PopulationCount(hwy::SizeTag<1> /* tag */, Vec128<T> v) {
  const Full128<uint8_t> d8;
  return Vec128<T>(vcntq_u8(BitCast(d8, v).raw));
}
template <typename T, size_t N, HWY_IF_LE64(T, N)>
HWY_INLINE Vec128<T, N> PopulationCount(hwy::SizeTag<1> /* tag */,
                                        Vec128<T, N> v) {
  const Simd<uint8_t, N, 0> d8;
  return Vec128<T, N>(vcnt_u8(BitCast(d8, v).raw));
}

// ARM lacks popcount for lane sizes > 1, so take pairwise sums of the bytes.
template <typename T>
HWY_INLINE Vec128<T> PopulationCount(hwy::SizeTag<2> /* tag */, Vec128<T> v) {
  const Full128<uint8_t> d8;
  const uint8x16_t bytes = vcntq_u8(BitCast(d8, v).raw);
  return Vec128<T>(vpaddlq_u8(bytes));
}
template <typename T, size_t N, HWY_IF_LE64(T, N)>
HWY_INLINE Vec128<T, N> PopulationCount(hwy::SizeTag<2> /* tag */,
                                        Vec128<T, N> v) {
  const Repartition<uint8_t, Simd<T, N, 0>> d8;
  const uint8x8_t bytes = vcnt_u8(BitCast(d8, v).raw);
  return Vec128<T, N>(vpaddl_u8(bytes));
}

template <typename T>
HWY_INLINE Vec128<T> PopulationCount(hwy::SizeTag<4> /* tag */, Vec128<T> v) {
  const Full128<uint8_t> d8;
  const uint8x16_t bytes = vcntq_u8(BitCast(d8, v).raw);
  return Vec128<T>(vpaddlq_u16(vpaddlq_u8(bytes)));
}
template <typename T, size_t N, HWY_IF_LE64(T, N)>
HWY_INLINE Vec128<T, N> PopulationCount(hwy::SizeTag<4> /* tag */,
                                        Vec128<T, N> v) {
  const Repartition<uint8_t, Simd<T, N, 0>> d8;
  const uint8x8_t bytes = vcnt_u8(BitCast(d8, v).raw);
  return Vec128<T, N>(vpaddl_u16(vpaddl_u8(bytes)));
}

template <typename T>
HWY_INLINE Vec128<T> PopulationCount(hwy::SizeTag<8> /* tag */, Vec128<T> v) {
  const Full128<uint8_t> d8;
  const uint8x16_t bytes = vcntq_u8(BitCast(d8, v).raw);
  return Vec128<T>(vpaddlq_u32(vpaddlq_u16(vpaddlq_u8(bytes))));
}
template <typename T, size_t N, HWY_IF_LE64(T, N)>
HWY_INLINE Vec128<T, N> PopulationCount(hwy::SizeTag<8> /* tag */,
                                        Vec128<T, N> v) {
  const Repartition<uint8_t, Simd<T, N, 0>> d8;
  const uint8x8_t bytes = vcnt_u8(BitCast(d8, v).raw);
  return Vec128<T, N>(vpaddl_u32(vpaddl_u16(vpaddl_u8(bytes))));
}

}  // namespace detail

template <typename T, size_t N, HWY_IF_NOT_FLOAT(T)>
HWY_API Vec128<T, N> PopulationCount(Vec128<T, N> v) {
  return detail::PopulationCount(hwy::SizeTag<sizeof(T)>(), v);
}

// ================================================== SIGN

// ------------------------------ Abs

// Returns absolute value, except that LimitsMin() maps to LimitsMax() + 1.
HWY_API Vec128<int8_t> Abs(const Vec128<int8_t> v) {
  return Vec128<int8_t>(vabsq_s8(v.raw));
}
HWY_API Vec128<int16_t> Abs(const Vec128<int16_t> v) {
  return Vec128<int16_t>(vabsq_s16(v.raw));
}
HWY_API Vec128<int32_t> Abs(const Vec128<int32_t> v) {
  return Vec128<int32_t>(vabsq_s32(v.raw));
}
// i64 is implemented after BroadcastSignBit.
HWY_API Vec128<float> Abs(const Vec128<float> v) {
  return Vec128<float>(vabsq_f32(v.raw));
}

template <size_t N, HWY_IF_LE64(int8_t, N)>
HWY_API Vec128<int8_t, N> Abs(const Vec128<int8_t, N> v) {
  return Vec128<int8_t, N>(vabs_s8(v.raw));
}
template <size_t N, HWY_IF_LE64(int16_t, N)>
HWY_API Vec128<int16_t, N> Abs(const Vec128<int16_t, N> v) {
  return Vec128<int16_t, N>(vabs_s16(v.raw));
}
template <size_t N, HWY_IF_LE64(int32_t, N)>
HWY_API Vec128<int32_t, N> Abs(const Vec128<int32_t, N> v) {
  return Vec128<int32_t, N>(vabs_s32(v.raw));
}
template <size_t N, HWY_IF_LE64(float, N)>
HWY_API Vec128<float, N> Abs(const Vec128<float, N> v) {
  return Vec128<float, N>(vabs_f32(v.raw));
}

#if HWY_ARCH_ARM_A64
HWY_API Vec128<double> Abs(const Vec128<double> v) {
  return Vec128<double>(vabsq_f64(v.raw));
}

HWY_API Vec64<double> Abs(const Vec64<double> v) {
  return Vec64<double>(vabs_f64(v.raw));
}
#endif

// ------------------------------ CopySign

template <typename T, size_t N>
HWY_API Vec128<T, N> CopySign(const Vec128<T, N> magn,
                              const Vec128<T, N> sign) {
  static_assert(IsFloat<T>(), "Only makes sense for floating-point");
  const auto msb = SignBit(Simd<T, N, 0>());
  return Or(AndNot(msb, magn), And(msb, sign));
}

template <typename T, size_t N>
HWY_API Vec128<T, N> CopySignToAbs(const Vec128<T, N> abs,
                                   const Vec128<T, N> sign) {
  static_assert(IsFloat<T>(), "Only makes sense for floating-point");
  return Or(abs, And(SignBit(Simd<T, N, 0>()), sign));
}

// ------------------------------ BroadcastSignBit

template <typename T, size_t N, HWY_IF_SIGNED(T)>
HWY_API Vec128<T, N> BroadcastSignBit(const Vec128<T, N> v) {
  return ShiftRight<sizeof(T) * 8 - 1>(v);
}

// ================================================== MASK

// ------------------------------ To/from vector

// Mask and Vec have the same representation (true = FF..FF).
template <typename T, size_t N>
HWY_API Mask128<T, N> MaskFromVec(const Vec128<T, N> v) {
  const Simd<MakeUnsigned<T>, N, 0> du;
  return Mask128<T, N>(BitCast(du, v).raw);
}

template <typename T, size_t N>
HWY_API Vec128<T, N> VecFromMask(Simd<T, N, 0> d, const Mask128<T, N> v) {
  return BitCast(d, Vec128<MakeUnsigned<T>, N>(v.raw));
}

// ------------------------------ RebindMask

template <typename TFrom, typename TTo, size_t N>
HWY_API Mask128<TTo, N> RebindMask(Simd<TTo, N, 0> dto, Mask128<TFrom, N> m) {
  static_assert(sizeof(TFrom) == sizeof(TTo), "Must have same size");
  return MaskFromVec(BitCast(dto, VecFromMask(Simd<TFrom, N, 0>(), m)));
}

// ------------------------------ IfThenElse(mask, yes, no) = mask ? b : a.

#define HWY_NEON_BUILD_TPL_HWY_IF
#define HWY_NEON_BUILD_RET_HWY_IF(type, size) Vec128<type##_t, size>
#define HWY_NEON_BUILD_PARAM_HWY_IF(type, size)                         \
  const Mask128<type##_t, size> mask, const Vec128<type##_t, size> yes, \
      const Vec128<type##_t, size> no
#define HWY_NEON_BUILD_ARG_HWY_IF mask.raw, yes.raw, no.raw

HWY_NEON_DEF_FUNCTION_ALL_TYPES(IfThenElse, vbsl, _, HWY_IF)

#undef HWY_NEON_BUILD_TPL_HWY_IF
#undef HWY_NEON_BUILD_RET_HWY_IF
#undef HWY_NEON_BUILD_PARAM_HWY_IF
#undef HWY_NEON_BUILD_ARG_HWY_IF

// mask ? yes : 0
template <typename T, size_t N>
HWY_API Vec128<T, N> IfThenElseZero(const Mask128<T, N> mask,
                                    const Vec128<T, N> yes) {
  return yes & VecFromMask(Simd<T, N, 0>(), mask);
}

// mask ? 0 : no
template <typename T, size_t N>
HWY_API Vec128<T, N> IfThenZeroElse(const Mask128<T, N> mask,
                                    const Vec128<T, N> no) {
  return AndNot(VecFromMask(Simd<T, N, 0>(), mask), no);
}

template <typename T, size_t N>
HWY_API Vec128<T, N> IfNegativeThenElse(Vec128<T, N> v, Vec128<T, N> yes,
                                        Vec128<T, N> no) {
  static_assert(IsSigned<T>(), "Only works for signed/float");
  const Simd<T, N, 0> d;
  const RebindToSigned<decltype(d)> di;

  Mask128<T, N> m = MaskFromVec(BitCast(d, BroadcastSignBit(BitCast(di, v))));
  return IfThenElse(m, yes, no);
}

template <typename T, size_t N>
HWY_API Vec128<T, N> ZeroIfNegative(Vec128<T, N> v) {
  const auto zero = Zero(Simd<T, N, 0>());
  return Max(zero, v);
}

// ------------------------------ Mask logical

template <typename T, size_t N>
HWY_API Mask128<T, N> Not(const Mask128<T, N> m) {
  return MaskFromVec(Not(VecFromMask(Simd<T, N, 0>(), m)));
}

template <typename T, size_t N>
HWY_API Mask128<T, N> And(const Mask128<T, N> a, Mask128<T, N> b) {
  const Simd<T, N, 0> d;
  return MaskFromVec(And(VecFromMask(d, a), VecFromMask(d, b)));
}

template <typename T, size_t N>
HWY_API Mask128<T, N> AndNot(const Mask128<T, N> a, Mask128<T, N> b) {
  const Simd<T, N, 0> d;
  return MaskFromVec(AndNot(VecFromMask(d, a), VecFromMask(d, b)));
}

template <typename T, size_t N>
HWY_API Mask128<T, N> Or(const Mask128<T, N> a, Mask128<T, N> b) {
  const Simd<T, N, 0> d;
  return MaskFromVec(Or(VecFromMask(d, a), VecFromMask(d, b)));
}

template <typename T, size_t N>
HWY_API Mask128<T, N> Xor(const Mask128<T, N> a, Mask128<T, N> b) {
  const Simd<T, N, 0> d;
  return MaskFromVec(Xor(VecFromMask(d, a), VecFromMask(d, b)));
}

// ================================================== COMPARE

// Comparisons fill a lane with 1-bits if the condition is true, else 0.

// ------------------------------ Shuffle2301 (for i64 compares)

// Swap 32-bit halves in 64-bits
HWY_API Vec64<uint32_t> Shuffle2301(const Vec64<uint32_t> v) {
  return Vec64<uint32_t>(vrev64_u32(v.raw));
}
HWY_API Vec64<int32_t> Shuffle2301(const Vec64<int32_t> v) {
  return Vec64<int32_t>(vrev64_s32(v.raw));
}
HWY_API Vec64<float> Shuffle2301(const Vec64<float> v) {
  return Vec64<float>(vrev64_f32(v.raw));
}
HWY_API Vec128<uint32_t> Shuffle2301(const Vec128<uint32_t> v) {
  return Vec128<uint32_t>(vrev64q_u32(v.raw));
}
HWY_API Vec128<int32_t> Shuffle2301(const Vec128<int32_t> v) {
  return Vec128<int32_t>(vrev64q_s32(v.raw));
}
HWY_API Vec128<float> Shuffle2301(const Vec128<float> v) {
  return Vec128<float>(vrev64q_f32(v.raw));
}

#define HWY_NEON_BUILD_TPL_HWY_COMPARE
#define HWY_NEON_BUILD_RET_HWY_COMPARE(type, size) Mask128<type##_t, size>
#define HWY_NEON_BUILD_PARAM_HWY_COMPARE(type, size) \
  const Vec128<type##_t, size> a, const Vec128<type##_t, size> b
#define HWY_NEON_BUILD_ARG_HWY_COMPARE a.raw, b.raw

// ------------------------------ Equality
HWY_NEON_DEF_FUNCTION_ALL_FLOATS(operator==, vceq, _, HWY_COMPARE)
#if HWY_ARCH_ARM_A64
HWY_NEON_DEF_FUNCTION_INTS_UINTS(operator==, vceq, _, HWY_COMPARE)
#else
// No 64-bit comparisons on armv7: emulate them below, after Shuffle2301.
HWY_NEON_DEF_FUNCTION_INT_8_16_32(operator==, vceq, _, HWY_COMPARE)
HWY_NEON_DEF_FUNCTION_UINT_8_16_32(operator==, vceq, _, HWY_COMPARE)
#endif

// ------------------------------ Strict inequality (signed, float)
#if HWY_ARCH_ARM_A64
HWY_NEON_DEF_FUNCTION_INTS_UINTS(operator<, vclt, _, HWY_COMPARE)
#else
HWY_NEON_DEF_FUNCTION_UINT_8_16_32(operator<, vclt, _, HWY_COMPARE)
HWY_NEON_DEF_FUNCTION_INT_8_16_32(operator<, vclt, _, HWY_COMPARE)
#endif
HWY_NEON_DEF_FUNCTION_ALL_FLOATS(operator<, vclt, _, HWY_COMPARE)

// ------------------------------ Weak inequality (float)
HWY_NEON_DEF_FUNCTION_ALL_FLOATS(operator<=, vcle, _, HWY_COMPARE)

#undef HWY_NEON_BUILD_TPL_HWY_COMPARE
#undef HWY_NEON_BUILD_RET_HWY_COMPARE
#undef HWY_NEON_BUILD_PARAM_HWY_COMPARE
#undef HWY_NEON_BUILD_ARG_HWY_COMPARE

// ------------------------------ ARMv7 i64 compare (Shuffle2301, Eq)

#if HWY_ARCH_ARM_V7

template <size_t N>
HWY_API Mask128<int64_t, N> operator==(const Vec128<int64_t, N> a,
                                       const Vec128<int64_t, N> b) {
  const Simd<int32_t, N * 2, 0> d32;
  const Simd<int64_t, N, 0> d64;
  const auto cmp32 = VecFromMask(d32, Eq(BitCast(d32, a), BitCast(d32, b)));
  const auto cmp64 = cmp32 & Shuffle2301(cmp32);
  return MaskFromVec(BitCast(d64, cmp64));
}

template <size_t N>
HWY_API Mask128<uint64_t, N> operator==(const Vec128<uint64_t, N> a,
                                        const Vec128<uint64_t, N> b) {
  const Simd<uint32_t, N * 2, 0> d32;
  const Simd<uint64_t, N, 0> d64;
  const auto cmp32 = VecFromMask(d32, Eq(BitCast(d32, a), BitCast(d32, b)));
  const auto cmp64 = cmp32 & Shuffle2301(cmp32);
  return MaskFromVec(BitCast(d64, cmp64));
}

HWY_API Mask128<int64_t> operator<(const Vec128<int64_t> a,
                                   const Vec128<int64_t> b) {
  const int64x2_t sub = vqsubq_s64(a.raw, b.raw);
  return MaskFromVec(BroadcastSignBit(Vec128<int64_t>(sub)));
}
HWY_API Mask128<int64_t, 1> operator<(const Vec64<int64_t> a,
                                      const Vec64<int64_t> b) {
  const int64x1_t sub = vqsub_s64(a.raw, b.raw);
  return MaskFromVec(BroadcastSignBit(Vec64<int64_t>(sub)));
}

template <size_t N>
HWY_API Mask128<uint64_t, N> operator<(const Vec128<uint64_t, N> a,
                                       const Vec128<uint64_t, N> b) {
  const DFromV<decltype(a)> du;
  const RebindToSigned<decltype(du)> di;
  const Vec128<uint64_t, N> msb = AndNot(a, b) | AndNot(a ^ b, a - b);
  return MaskFromVec(BitCast(du, BroadcastSignBit(BitCast(di, msb))));
}

#endif

// ------------------------------ operator!= (operator==)

// Customize HWY_NEON_DEF_FUNCTION to call 2 functions.
#pragma push_macro("HWY_NEON_DEF_FUNCTION")
#undef HWY_NEON_DEF_FUNCTION
// This cannot have _any_ template argument (in x86_128 we can at least have N
// as an argument), otherwise it is not more specialized than rewritten
// operator== in C++20, leading to compile errors.
#define HWY_NEON_DEF_FUNCTION(type, size, name, prefix, infix, suffix, args) \
  HWY_API Mask128<type##_t, size> name(Vec128<type##_t, size> a,             \
                                       Vec128<type##_t, size> b) {           \
    return Not(a == b);                                                      \
  }

HWY_NEON_DEF_FUNCTION_ALL_TYPES(operator!=, ignored, ignored, ignored)

#pragma pop_macro("HWY_NEON_DEF_FUNCTION")

// ------------------------------ Reversed comparisons

template <typename T, size_t N>
HWY_API Mask128<T, N> operator>(Vec128<T, N> a, Vec128<T, N> b) {
  return operator<(b, a);
}
template <typename T, size_t N>
HWY_API Mask128<T, N> operator>=(Vec128<T, N> a, Vec128<T, N> b) {
  return operator<=(b, a);
}

// ------------------------------ FirstN (Iota, Lt)

template <typename T, size_t N>
HWY_API Mask128<T, N> FirstN(const Simd<T, N, 0> d, size_t num) {
  const RebindToSigned<decltype(d)> di;  // Signed comparisons are cheaper.
  return RebindMask(d, Iota(di, 0) < Set(di, static_cast<MakeSigned<T>>(num)));
}

// ------------------------------ TestBit (Eq)

#define HWY_NEON_BUILD_TPL_HWY_TESTBIT
#define HWY_NEON_BUILD_RET_HWY_TESTBIT(type, size) Mask128<type##_t, size>
#define HWY_NEON_BUILD_PARAM_HWY_TESTBIT(type, size) \
  Vec128<type##_t, size> v, Vec128<type##_t, size> bit
#define HWY_NEON_BUILD_ARG_HWY_TESTBIT v.raw, bit.raw

#if HWY_ARCH_ARM_A64
HWY_NEON_DEF_FUNCTION_INTS_UINTS(TestBit, vtst, _, HWY_TESTBIT)
#else
// No 64-bit versions on armv7
HWY_NEON_DEF_FUNCTION_UINT_8_16_32(TestBit, vtst, _, HWY_TESTBIT)
HWY_NEON_DEF_FUNCTION_INT_8_16_32(TestBit, vtst, _, HWY_TESTBIT)

template <size_t N>
HWY_API Mask128<uint64_t, N> TestBit(Vec128<uint64_t, N> v,
                                     Vec128<uint64_t, N> bit) {
  return (v & bit) == bit;
}
template <size_t N>
HWY_API Mask128<int64_t, N> TestBit(Vec128<int64_t, N> v,
                                    Vec128<int64_t, N> bit) {
  return (v & bit) == bit;
}

#endif
#undef HWY_NEON_BUILD_TPL_HWY_TESTBIT
#undef HWY_NEON_BUILD_RET_HWY_TESTBIT
#undef HWY_NEON_BUILD_PARAM_HWY_TESTBIT
#undef HWY_NEON_BUILD_ARG_HWY_TESTBIT

// ------------------------------ Abs i64 (IfThenElse, BroadcastSignBit)
HWY_API Vec128<int64_t> Abs(const Vec128<int64_t> v) {
#if HWY_ARCH_ARM_A64
  return Vec128<int64_t>(vabsq_s64(v.raw));
#else
  const auto zero = Zero(Full128<int64_t>());
  return IfThenElse(MaskFromVec(BroadcastSignBit(v)), zero - v, v);
#endif
}
HWY_API Vec64<int64_t> Abs(const Vec64<int64_t> v) {
#if HWY_ARCH_ARM_A64
  return Vec64<int64_t>(vabs_s64(v.raw));
#else
  const auto zero = Zero(Full64<int64_t>());
  return IfThenElse(MaskFromVec(BroadcastSignBit(v)), zero - v, v);
#endif
}

// ------------------------------ Min (IfThenElse, BroadcastSignBit)

// Unsigned
HWY_NEON_DEF_FUNCTION_UINT_8_16_32(Min, vmin, _, 2)

template <size_t N>
HWY_API Vec128<uint64_t, N> Min(const Vec128<uint64_t, N> a,
                                const Vec128<uint64_t, N> b) {
#if HWY_ARCH_ARM_A64
  return IfThenElse(b < a, b, a);
#else
  const DFromV<decltype(a)> du;
  const RebindToSigned<decltype(du)> di;
  return BitCast(du, BitCast(di, a) - BitCast(di, detail::SaturatedSub(a, b)));
#endif
}

// Signed
HWY_NEON_DEF_FUNCTION_INT_8_16_32(Min, vmin, _, 2)

template <size_t N>
HWY_API Vec128<int64_t, N> Min(const Vec128<int64_t, N> a,
                               const Vec128<int64_t, N> b) {
#if HWY_ARCH_ARM_A64
  return IfThenElse(b < a, b, a);
#else
  const Vec128<int64_t, N> sign = detail::SaturatedSub(a, b);
  return IfThenElse(MaskFromVec(BroadcastSignBit(sign)), a, b);
#endif
}

// Float: IEEE minimumNumber on v8, otherwise NaN if any is NaN.
#if HWY_ARCH_ARM_A64
HWY_NEON_DEF_FUNCTION_ALL_FLOATS(Min, vminnm, _, 2)
#else
HWY_NEON_DEF_FUNCTION_ALL_FLOATS(Min, vmin, _, 2)
#endif

// ------------------------------ Max (IfThenElse, BroadcastSignBit)

// Unsigned (no u64)
HWY_NEON_DEF_FUNCTION_UINT_8_16_32(Max, vmax, _, 2)

template <size_t N>
HWY_API Vec128<uint64_t, N> Max(const Vec128<uint64_t, N> a,
                                const Vec128<uint64_t, N> b) {
#if HWY_ARCH_ARM_A64
  return IfThenElse(b < a, a, b);
#else
  const DFromV<decltype(a)> du;
  const RebindToSigned<decltype(du)> di;
  return BitCast(du, BitCast(di, b) + BitCast(di, detail::SaturatedSub(a, b)));
#endif
}

// Signed (no i64)
HWY_NEON_DEF_FUNCTION_INT_8_16_32(Max, vmax, _, 2)

template <size_t N>
HWY_API Vec128<int64_t, N> Max(const Vec128<int64_t, N> a,
                               const Vec128<int64_t, N> b) {
#if HWY_ARCH_ARM_A64
  return IfThenElse(b < a, a, b);
#else
  const Vec128<int64_t, N> sign = detail::SaturatedSub(a, b);
  return IfThenElse(MaskFromVec(BroadcastSignBit(sign)), b, a);
#endif
}

// Float: IEEE maximumNumber on v8, otherwise NaN if any is NaN.
#if HWY_ARCH_ARM_A64
HWY_NEON_DEF_FUNCTION_ALL_FLOATS(Max, vmaxnm, _, 2)
#else
HWY_NEON_DEF_FUNCTION_ALL_FLOATS(Max, vmax, _, 2)
#endif

// ================================================== MEMORY

// ------------------------------ Load 128

HWY_API Vec128<uint8_t> LoadU(Full128<uint8_t> /* tag */,
                              const uint8_t* HWY_RESTRICT unaligned) {
  return Vec128<uint8_t>(vld1q_u8(unaligned));
}
HWY_API Vec128<uint16_t> LoadU(Full128<uint16_t> /* tag */,
                               const uint16_t* HWY_RESTRICT unaligned) {
  return Vec128<uint16_t>(vld1q_u16(unaligned));
}
HWY_API Vec128<uint32_t> LoadU(Full128<uint32_t> /* tag */,
                               const uint32_t* HWY_RESTRICT unaligned) {
  return Vec128<uint32_t>(vld1q_u32(unaligned));
}
HWY_API Vec128<uint64_t> LoadU(Full128<uint64_t> /* tag */,
                               const uint64_t* HWY_RESTRICT unaligned) {
  return Vec128<uint64_t>(vld1q_u64(unaligned));
}
HWY_API Vec128<int8_t> LoadU(Full128<int8_t> /* tag */,
                             const int8_t* HWY_RESTRICT unaligned) {
  return Vec128<int8_t>(vld1q_s8(unaligned));
}
HWY_API Vec128<int16_t> LoadU(Full128<int16_t> /* tag */,
                              const int16_t* HWY_RESTRICT unaligned) {
  return Vec128<int16_t>(vld1q_s16(unaligned));
}
HWY_API Vec128<int32_t> LoadU(Full128<int32_t> /* tag */,
                              const int32_t* HWY_RESTRICT unaligned) {
  return Vec128<int32_t>(vld1q_s32(unaligned));
}
HWY_API Vec128<int64_t> LoadU(Full128<int64_t> /* tag */,
                              const int64_t* HWY_RESTRICT unaligned) {
  return Vec128<int64_t>(vld1q_s64(unaligned));
}
HWY_API Vec128<float> LoadU(Full128<float> /* tag */,
                            const float* HWY_RESTRICT unaligned) {
  return Vec128<float>(vld1q_f32(unaligned));
}
#if HWY_ARCH_ARM_A64
HWY_API Vec128<double> LoadU(Full128<double> /* tag */,
                             const double* HWY_RESTRICT unaligned) {
  return Vec128<double>(vld1q_f64(unaligned));
}
#endif

// ------------------------------ Load 64

HWY_API Vec64<uint8_t> LoadU(Full64<uint8_t> /* tag */,
                             const uint8_t* HWY_RESTRICT p) {
  return Vec64<uint8_t>(vld1_u8(p));
}
HWY_API Vec64<uint16_t> LoadU(Full64<uint16_t> /* tag */,
                              const uint16_t* HWY_RESTRICT p) {
  return Vec64<uint16_t>(vld1_u16(p));
}
HWY_API Vec64<uint32_t> LoadU(Full64<uint32_t> /* tag */,
                              const uint32_t* HWY_RESTRICT p) {
  return Vec64<uint32_t>(vld1_u32(p));
}
HWY_API Vec64<uint64_t> LoadU(Full64<uint64_t> /* tag */,
                              const uint64_t* HWY_RESTRICT p) {
  return Vec64<uint64_t>(vld1_u64(p));
}
HWY_API Vec64<int8_t> LoadU(Full64<int8_t> /* tag */,
                            const int8_t* HWY_RESTRICT p) {
  return Vec64<int8_t>(vld1_s8(p));
}
HWY_API Vec64<int16_t> LoadU(Full64<int16_t> /* tag */,
                             const int16_t* HWY_RESTRICT p) {
  return Vec64<int16_t>(vld1_s16(p));
}
HWY_API Vec64<int32_t> LoadU(Full64<int32_t> /* tag */,
                             const int32_t* HWY_RESTRICT p) {
  return Vec64<int32_t>(vld1_s32(p));
}
HWY_API Vec64<int64_t> LoadU(Full64<int64_t> /* tag */,
                             const int64_t* HWY_RESTRICT p) {
  return Vec64<int64_t>(vld1_s64(p));
}
HWY_API Vec64<float> LoadU(Full64<float> /* tag */,
                           const float* HWY_RESTRICT p) {
  return Vec64<float>(vld1_f32(p));
}
#if HWY_ARCH_ARM_A64
HWY_API Vec64<double> LoadU(Full64<double> /* tag */,
                            const double* HWY_RESTRICT p) {
  return Vec64<double>(vld1_f64(p));
}
#endif
// ------------------------------ Load 32

// Actual 32-bit broadcast load - used to implement the other lane types
// because reinterpret_cast of the pointer leads to incorrect codegen on GCC.
HWY_API Vec32<uint32_t> LoadU(Full32<uint32_t> /*tag*/,
                              const uint32_t* HWY_RESTRICT p) {
  return Vec32<uint32_t>(vld1_dup_u32(p));
}
HWY_API Vec32<int32_t> LoadU(Full32<int32_t> /*tag*/,
                             const int32_t* HWY_RESTRICT p) {
  return Vec32<int32_t>(vld1_dup_s32(p));
}
HWY_API Vec32<float> LoadU(Full32<float> /*tag*/, const float* HWY_RESTRICT p) {
  return Vec32<float>(vld1_dup_f32(p));
}

template <typename T, HWY_IF_LANE_SIZE_LT(T, 4)>
HWY_API Vec32<T> LoadU(Full32<T> d, const T* HWY_RESTRICT p) {
  const Repartition<uint32_t, decltype(d)> d32;
  uint32_t buf;
  CopyBytes<4>(p, &buf);
  return BitCast(d, LoadU(d32, &buf));
}

// ------------------------------ Load 16

// Actual 16-bit broadcast load - used to implement the other lane types
// because reinterpret_cast of the pointer leads to incorrect codegen on GCC.
HWY_API Vec128<uint16_t, 1> LoadU(Simd<uint16_t, 1, 0> /*tag*/,
                                  const uint16_t* HWY_RESTRICT p) {
  return Vec128<uint16_t, 1>(vld1_dup_u16(p));
}
HWY_API Vec128<int16_t, 1> LoadU(Simd<int16_t, 1, 0> /*tag*/,
                                 const int16_t* HWY_RESTRICT p) {
  return Vec128<int16_t, 1>(vld1_dup_s16(p));
}

template <typename T, HWY_IF_LANE_SIZE_LT(T, 2)>
HWY_API Vec128<T, 2> LoadU(Simd<T, 2, 0> d, const T* HWY_RESTRICT p) {
  const Repartition<uint16_t, decltype(d)> d16;
  uint16_t buf;
  CopyBytes<2>(p, &buf);
  return BitCast(d, LoadU(d16, &buf));
}

// ------------------------------ Load 8

HWY_API Vec128<uint8_t, 1> LoadU(Simd<uint8_t, 1, 0>,
                                 const uint8_t* HWY_RESTRICT p) {
  return Vec128<uint8_t, 1>(vld1_dup_u8(p));
}

HWY_API Vec128<int8_t, 1> LoadU(Simd<int8_t, 1, 0>,
                                const int8_t* HWY_RESTRICT p) {
  return Vec128<int8_t, 1>(vld1_dup_s8(p));
}

// [b]float16_t use the same Raw as uint16_t, so forward to that.
template <size_t N>
HWY_API Vec128<float16_t, N> LoadU(Simd<float16_t, N, 0> d,
                                   const float16_t* HWY_RESTRICT p) {
  const RebindToUnsigned<decltype(d)> du16;
  const auto pu16 = reinterpret_cast<const uint16_t*>(p);
  return Vec128<float16_t, N>(LoadU(du16, pu16).raw);
}
template <size_t N>
HWY_API Vec128<bfloat16_t, N> LoadU(Simd<bfloat16_t, N, 0> d,
                                    const bfloat16_t* HWY_RESTRICT p) {
  const RebindToUnsigned<decltype(d)> du16;
  const auto pu16 = reinterpret_cast<const uint16_t*>(p);
  return Vec128<bfloat16_t, N>(LoadU(du16, pu16).raw);
}

// On ARM, Load is the same as LoadU.
template <typename T, size_t N>
HWY_API Vec128<T, N> Load(Simd<T, N, 0> d, const T* HWY_RESTRICT p) {
  return LoadU(d, p);
}

template <typename T, size_t N>
HWY_API Vec128<T, N> MaskedLoad(Mask128<T, N> m, Simd<T, N, 0> d,
                                const T* HWY_RESTRICT aligned) {
  return IfThenElseZero(m, Load(d, aligned));
}

// 128-bit SIMD => nothing to duplicate, same as an unaligned load.
template <typename T, size_t N, HWY_IF_LE128(T, N)>
HWY_API Vec128<T, N> LoadDup128(Simd<T, N, 0> d,
                                const T* const HWY_RESTRICT p) {
  return LoadU(d, p);
}

// ------------------------------ Store 128

HWY_API void StoreU(const Vec128<uint8_t> v, Full128<uint8_t> /* tag */,
                    uint8_t* HWY_RESTRICT unaligned) {
  vst1q_u8(unaligned, v.raw);
}
HWY_API void StoreU(const Vec128<uint16_t> v, Full128<uint16_t> /* tag */,
                    uint16_t* HWY_RESTRICT unaligned) {
  vst1q_u16(unaligned, v.raw);
}
HWY_API void StoreU(const Vec128<uint32_t> v, Full128<uint32_t> /* tag */,
                    uint32_t* HWY_RESTRICT unaligned) {
  vst1q_u32(unaligned, v.raw);
}
HWY_API void StoreU(const Vec128<uint64_t> v, Full128<uint64_t> /* tag */,
                    uint64_t* HWY_RESTRICT unaligned) {
  vst1q_u64(unaligned, v.raw);
}
HWY_API void StoreU(const Vec128<int8_t> v, Full128<int8_t> /* tag */,
                    int8_t* HWY_RESTRICT unaligned) {
  vst1q_s8(unaligned, v.raw);
}
HWY_API void StoreU(const Vec128<int16_t> v, Full128<int16_t> /* tag */,
                    int16_t* HWY_RESTRICT unaligned) {
  vst1q_s16(unaligned, v.raw);
}
HWY_API void StoreU(const Vec128<int32_t> v, Full128<int32_t> /* tag */,
                    int32_t* HWY_RESTRICT unaligned) {
  vst1q_s32(unaligned, v.raw);
}
HWY_API void StoreU(const Vec128<int64_t> v, Full128<int64_t> /* tag */,
                    int64_t* HWY_RESTRICT unaligned) {
  vst1q_s64(unaligned, v.raw);
}
HWY_API void StoreU(const Vec128<float> v, Full128<float> /* tag */,
                    float* HWY_RESTRICT unaligned) {
  vst1q_f32(unaligned, v.raw);
}
#if HWY_ARCH_ARM_A64
HWY_API void StoreU(const Vec128<double> v, Full128<double> /* tag */,
                    double* HWY_RESTRICT unaligned) {
  vst1q_f64(unaligned, v.raw);
}
#endif

// ------------------------------ Store 64

HWY_API void StoreU(const Vec64<uint8_t> v, Full64<uint8_t> /* tag */,
                    uint8_t* HWY_RESTRICT p) {
  vst1_u8(p, v.raw);
}
HWY_API void StoreU(const Vec64<uint16_t> v, Full64<uint16_t> /* tag */,
                    uint16_t* HWY_RESTRICT p) {
  vst1_u16(p, v.raw);
}
HWY_API void StoreU(const Vec64<uint32_t> v, Full64<uint32_t> /* tag */,
                    uint32_t* HWY_RESTRICT p) {
  vst1_u32(p, v.raw);
}
HWY_API void StoreU(const Vec64<uint64_t> v, Full64<uint64_t> /* tag */,
                    uint64_t* HWY_RESTRICT p) {
  vst1_u64(p, v.raw);
}
HWY_API void StoreU(const Vec64<int8_t> v, Full64<int8_t> /* tag */,
                    int8_t* HWY_RESTRICT p) {
  vst1_s8(p, v.raw);
}
HWY_API void StoreU(const Vec64<int16_t> v, Full64<int16_t> /* tag */,
                    int16_t* HWY_RESTRICT p) {
  vst1_s16(p, v.raw);
}
HWY_API void StoreU(const Vec64<int32_t> v, Full64<int32_t> /* tag */,
                    int32_t* HWY_RESTRICT p) {
  vst1_s32(p, v.raw);
}
HWY_API void StoreU(const Vec64<int64_t> v, Full64<int64_t> /* tag */,
                    int64_t* HWY_RESTRICT p) {
  vst1_s64(p, v.raw);
}
HWY_API void StoreU(const Vec64<float> v, Full64<float> /* tag */,
                    float* HWY_RESTRICT p) {
  vst1_f32(p, v.raw);
}
#if HWY_ARCH_ARM_A64
HWY_API void StoreU(const Vec64<double> v, Full64<double> /* tag */,
                    double* HWY_RESTRICT p) {
  vst1_f64(p, v.raw);
}
#endif

// ------------------------------ Store 32

HWY_API void StoreU(const Vec32<uint32_t> v, Full32<uint32_t>,
                    uint32_t* HWY_RESTRICT p) {
  vst1_lane_u32(p, v.raw, 0);
}
HWY_API void StoreU(const Vec32<int32_t> v, Full32<int32_t>,
                    int32_t* HWY_RESTRICT p) {
  vst1_lane_s32(p, v.raw, 0);
}
HWY_API void StoreU(const Vec32<float> v, Full32<float>,
                    float* HWY_RESTRICT p) {
  vst1_lane_f32(p, v.raw, 0);
}

template <typename T, HWY_IF_LANE_SIZE_LT(T, 4)>
HWY_API void StoreU(const Vec32<T> v, Full32<T> d, T* HWY_RESTRICT p) {
  const Repartition<uint32_t, decltype(d)> d32;
  const uint32_t buf = GetLane(BitCast(d32, v));
  CopyBytes<4>(&buf, p);
}

// ------------------------------ Store 16

HWY_API void StoreU(const Vec128<uint16_t, 1> v, Simd<uint16_t, 1, 0>,
                    uint16_t* HWY_RESTRICT p) {
  vst1_lane_u16(p, v.raw, 0);
}
HWY_API void StoreU(const Vec128<int16_t, 1> v, Simd<int16_t, 1, 0>,
                    int16_t* HWY_RESTRICT p) {
  vst1_lane_s16(p, v.raw, 0);
}

template <typename T, HWY_IF_LANE_SIZE_LT(T, 2)>
HWY_API void StoreU(const Vec128<T, 2> v, Simd<T, 2, 0> d, T* HWY_RESTRICT p) {
  const Repartition<uint16_t, decltype(d)> d16;
  const uint16_t buf = GetLane(BitCast(d16, v));
  CopyBytes<2>(&buf, p);
}

// ------------------------------ Store 8

HWY_API void StoreU(const Vec128<uint8_t, 1> v, Simd<uint8_t, 1, 0>,
                    uint8_t* HWY_RESTRICT p) {
  vst1_lane_u8(p, v.raw, 0);
}
HWY_API void StoreU(const Vec128<int8_t, 1> v, Simd<int8_t, 1, 0>,
                    int8_t* HWY_RESTRICT p) {
  vst1_lane_s8(p, v.raw, 0);
}

// [b]float16_t use the same Raw as uint16_t, so forward to that.
template <size_t N>
HWY_API void StoreU(Vec128<float16_t, N> v, Simd<float16_t, N, 0> d,
                    float16_t* HWY_RESTRICT p) {
  const RebindToUnsigned<decltype(d)> du16;
  const auto pu16 = reinterpret_cast<uint16_t*>(p);
  return StoreU(Vec128<uint16_t, N>(v.raw), du16, pu16);
}
template <size_t N>
HWY_API void StoreU(Vec128<bfloat16_t, N> v, Simd<bfloat16_t, N, 0> d,
                    bfloat16_t* HWY_RESTRICT p) {
  const RebindToUnsigned<decltype(d)> du16;
  const auto pu16 = reinterpret_cast<uint16_t*>(p);
  return StoreU(Vec128<uint16_t, N>(v.raw), du16, pu16);
}

// On ARM, Store is the same as StoreU.
template <typename T, size_t N>
HWY_API void Store(Vec128<T, N> v, Simd<T, N, 0> d, T* HWY_RESTRICT aligned) {
  StoreU(v, d, aligned);
}

template <typename T, size_t N>
HWY_API void BlendedStore(Vec128<T, N> v, Mask128<T, N> m, Simd<T, N, 0> d,
                          T* HWY_RESTRICT p) {
  // Treat as unsigned so that we correctly support float16.
  const RebindToUnsigned<decltype(d)> du;
  const auto blended =
      IfThenElse(RebindMask(du, m), BitCast(du, v), BitCast(du, LoadU(d, p)));
  StoreU(BitCast(d, blended), d, p);
}

// ------------------------------ Non-temporal stores

// Same as aligned stores on non-x86.

template <typename T, size_t N>
HWY_API void Stream(const Vec128<T, N> v, Simd<T, N, 0> d,
                    T* HWY_RESTRICT aligned) {
  Store(v, d, aligned);
}

// ================================================== CONVERT

// ------------------------------ Promotions (part w/ narrow lanes -> full)

// Unsigned: zero-extend to full vector.
HWY_API Vec128<uint16_t> PromoteTo(Full128<uint16_t> /* tag */,
                                   const Vec64<uint8_t> v) {
  return Vec128<uint16_t>(vmovl_u8(v.raw));
}
HWY_API Vec128<uint32_t> PromoteTo(Full128<uint32_t> /* tag */,
                                   const Vec32<uint8_t> v) {
  uint16x8_t a = vmovl_u8(v.raw);
  return Vec128<uint32_t>(vmovl_u16(vget_low_u16(a)));
}
HWY_API Vec128<uint32_t> PromoteTo(Full128<uint32_t> /* tag */,
                                   const Vec64<uint16_t> v) {
  return Vec128<uint32_t>(vmovl_u16(v.raw));
}
HWY_API Vec128<uint64_t> PromoteTo(Full128<uint64_t> /* tag */,
                                   const Vec64<uint32_t> v) {
  return Vec128<uint64_t>(vmovl_u32(v.raw));
}
HWY_API Vec128<int16_t> PromoteTo(Full128<int16_t> d, const Vec64<uint8_t> v) {
  return BitCast(d, Vec128<uint16_t>(vmovl_u8(v.raw)));
}
HWY_API Vec128<int32_t> PromoteTo(Full128<int32_t> d, const Vec32<uint8_t> v) {
  uint16x8_t a = vmovl_u8(v.raw);
  return BitCast(d, Vec128<uint32_t>(vmovl_u16(vget_low_u16(a))));
}
HWY_API Vec128<int32_t> PromoteTo(Full128<int32_t> d, const Vec64<uint16_t> v) {
  return BitCast(d, Vec128<uint32_t>(vmovl_u16(v.raw)));
}

// Unsigned: zero-extend to half vector.
template <size_t N, HWY_IF_LE64(uint16_t, N)>
HWY_API Vec128<uint16_t, N> PromoteTo(Simd<uint16_t, N, 0> /* tag */,
                                      const Vec128<uint8_t, N> v) {
  return Vec128<uint16_t, N>(vget_low_u16(vmovl_u8(v.raw)));
}
template <size_t N, HWY_IF_LE64(uint32_t, N)>
HWY_API Vec128<uint32_t, N> PromoteTo(Simd<uint32_t, N, 0> /* tag */,
                                      const Vec128<uint8_t, N> v) {
  uint16x8_t a = vmovl_u8(v.raw);
  return Vec128<uint32_t, N>(vget_low_u32(vmovl_u16(vget_low_u16(a))));
}
template <size_t N>
HWY_API Vec128<uint32_t, N> PromoteTo(Simd<uint32_t, N, 0> /* tag */,
                                      const Vec128<uint16_t, N> v) {
  return Vec128<uint32_t, N>(vget_low_u32(vmovl_u16(v.raw)));
}
template <size_t N, HWY_IF_LE64(uint64_t, N)>
HWY_API Vec128<uint64_t, N> PromoteTo(Simd<uint64_t, N, 0> /* tag */,
                                      const Vec128<uint32_t, N> v) {
  return Vec128<uint64_t, N>(vget_low_u64(vmovl_u32(v.raw)));
}
template <size_t N, HWY_IF_LE64(int16_t, N)>
HWY_API Vec128<int16_t, N> PromoteTo(Simd<int16_t, N, 0> d,
                                     const Vec128<uint8_t, N> v) {
  return BitCast(d, Vec128<uint16_t, N>(vget_low_u16(vmovl_u8(v.raw))));
}
template <size_t N, HWY_IF_LE64(int32_t, N)>
HWY_API Vec128<int32_t, N> PromoteTo(Simd<int32_t, N, 0> /* tag */,
                                     const Vec128<uint8_t, N> v) {
  uint16x8_t a = vmovl_u8(v.raw);
  uint32x4_t b = vmovl_u16(vget_low_u16(a));
  return Vec128<int32_t, N>(vget_low_s32(vreinterpretq_s32_u32(b)));
}
template <size_t N, HWY_IF_LE64(int32_t, N)>
HWY_API Vec128<int32_t, N> PromoteTo(Simd<int32_t, N, 0> /* tag */,
                                     const Vec128<uint16_t, N> v) {
  uint32x4_t a = vmovl_u16(v.raw);
  return Vec128<int32_t, N>(vget_low_s32(vreinterpretq_s32_u32(a)));
}

// Signed: replicate sign bit to full vector.
HWY_API Vec128<int16_t> PromoteTo(Full128<int16_t> /* tag */,
                                  const Vec64<int8_t> v) {
  return Vec128<int16_t>(vmovl_s8(v.raw));
}
HWY_API Vec128<int32_t> PromoteTo(Full128<int32_t> /* tag */,
                                  const Vec32<int8_t> v) {
  int16x8_t a = vmovl_s8(v.raw);
  return Vec128<int32_t>(vmovl_s16(vget_low_s16(a)));
}
HWY_API Vec128<int32_t> PromoteTo(Full128<int32_t> /* tag */,
                                  const Vec64<int16_t> v) {
  return Vec128<int32_t>(vmovl_s16(v.raw));
}
HWY_API Vec128<int64_t> PromoteTo(Full128<int64_t> /* tag */,
                                  const Vec64<int32_t> v) {
  return Vec128<int64_t>(vmovl_s32(v.raw));
}

// Signed: replicate sign bit to half vector.
template <size_t N>
HWY_API Vec128<int16_t, N> PromoteTo(Simd<int16_t, N, 0> /* tag */,
                                     const Vec128<int8_t, N> v) {
  return Vec128<int16_t, N>(vget_low_s16(vmovl_s8(v.raw)));
}
template <size_t N>
HWY_API Vec128<int32_t, N> PromoteTo(Simd<int32_t, N, 0> /* tag */,
                                     const Vec128<int8_t, N> v) {
  int16x8_t a = vmovl_s8(v.raw);
  int32x4_t b = vmovl_s16(vget_low_s16(a));
  return Vec128<int32_t, N>(vget_low_s32(b));
}
template <size_t N>
HWY_API Vec128<int32_t, N> PromoteTo(Simd<int32_t, N, 0> /* tag */,
                                     const Vec128<int16_t, N> v) {
  return Vec128<int32_t, N>(vget_low_s32(vmovl_s16(v.raw)));
}
template <size_t N>
HWY_API Vec128<int64_t, N> PromoteTo(Simd<int64_t, N, 0> /* tag */,
                                     const Vec128<int32_t, N> v) {
  return Vec128<int64_t, N>(vget_low_s64(vmovl_s32(v.raw)));
}

#if __ARM_FP & 2

HWY_API Vec128<float> PromoteTo(Full128<float> /* tag */,
                                const Vec128<float16_t, 4> v) {
  const float32x4_t f32 = vcvt_f32_f16(vreinterpret_f16_u16(v.raw));
  return Vec128<float>(f32);
}
template <size_t N>
HWY_API Vec128<float, N> PromoteTo(Simd<float, N, 0> /* tag */,
                                   const Vec128<float16_t, N> v) {
  const float32x4_t f32 = vcvt_f32_f16(vreinterpret_f16_u16(v.raw));
  return Vec128<float, N>(vget_low_f32(f32));
}

#else

template <size_t N>
HWY_API Vec128<float, N> PromoteTo(Simd<float, N, 0> df32,
                                   const Vec128<float16_t, N> v) {
  const RebindToSigned<decltype(df32)> di32;
  const RebindToUnsigned<decltype(df32)> du32;
  // Expand to u32 so we can shift.
  const auto bits16 = PromoteTo(du32, Vec128<uint16_t, N>{v.raw});
  const auto sign = ShiftRight<15>(bits16);
  const auto biased_exp = ShiftRight<10>(bits16) & Set(du32, 0x1F);
  const auto mantissa = bits16 & Set(du32, 0x3FF);
  const auto subnormal =
      BitCast(du32, ConvertTo(df32, BitCast(di32, mantissa)) *
                        Set(df32, 1.0f / 16384 / 1024));

  const auto biased_exp32 = biased_exp + Set(du32, 127 - 15);
  const auto mantissa32 = ShiftLeft<23 - 10>(mantissa);
  const auto normal = ShiftLeft<23>(biased_exp32) | mantissa32;
  const auto bits32 = IfThenElse(biased_exp == Zero(du32), subnormal, normal);
  return BitCast(df32, ShiftLeft<31>(sign) | bits32);
}

#endif

#if HWY_ARCH_ARM_A64

HWY_API Vec128<double> PromoteTo(Full128<double> /* tag */,
                                 const Vec64<float> v) {
  return Vec128<double>(vcvt_f64_f32(v.raw));
}

HWY_API Vec64<double> PromoteTo(Full64<double> /* tag */,
                                const Vec32<float> v) {
  return Vec64<double>(vget_low_f64(vcvt_f64_f32(v.raw)));
}

HWY_API Vec128<double> PromoteTo(Full128<double> /* tag */,
                                 const Vec64<int32_t> v) {
  const int64x2_t i64 = vmovl_s32(v.raw);
  return Vec128<double>(vcvtq_f64_s64(i64));
}

HWY_API Vec64<double> PromoteTo(Full64<double> /* tag */,
                                const Vec32<int32_t> v) {
  const int64x1_t i64 = vget_low_s64(vmovl_s32(v.raw));
  return Vec64<double>(vcvt_f64_s64(i64));
}

#endif

// ------------------------------ Demotions (full -> part w/ narrow lanes)

// From full vector to half or quarter
HWY_API Vec64<uint16_t> DemoteTo(Full64<uint16_t> /* tag */,
                                 const Vec128<int32_t> v) {
  return Vec64<uint16_t>(vqmovun_s32(v.raw));
}
HWY_API Vec64<int16_t> DemoteTo(Full64<int16_t> /* tag */,
                                const Vec128<int32_t> v) {
  return Vec64<int16_t>(vqmovn_s32(v.raw));
}
HWY_API Vec32<uint8_t> DemoteTo(Full32<uint8_t> /* tag */,
                                const Vec128<int32_t> v) {
  const uint16x4_t a = vqmovun_s32(v.raw);
  return Vec32<uint8_t>(vqmovn_u16(vcombine_u16(a, a)));
}
HWY_API Vec64<uint8_t> DemoteTo(Full64<uint8_t> /* tag */,
                                const Vec128<int16_t> v) {
  return Vec64<uint8_t>(vqmovun_s16(v.raw));
}
HWY_API Vec32<int8_t> DemoteTo(Full32<int8_t> /* tag */,
                               const Vec128<int32_t> v) {
  const int16x4_t a = vqmovn_s32(v.raw);
  return Vec32<int8_t>(vqmovn_s16(vcombine_s16(a, a)));
}
HWY_API Vec64<int8_t> DemoteTo(Full64<int8_t> /* tag */,
                               const Vec128<int16_t> v) {
  return Vec64<int8_t>(vqmovn_s16(v.raw));
}

// From half vector to partial half
template <size_t N, HWY_IF_LE64(int32_t, N)>
HWY_API Vec128<uint16_t, N> DemoteTo(Simd<uint16_t, N, 0> /* tag */,
                                     const Vec128<int32_t, N> v) {
  return Vec128<uint16_t, N>(vqmovun_s32(vcombine_s32(v.raw, v.raw)));
}
template <size_t N, HWY_IF_LE64(int32_t, N)>
HWY_API Vec128<int16_t, N> DemoteTo(Simd<int16_t, N, 0> /* tag */,
                                    const Vec128<int32_t, N> v) {
  return Vec128<int16_t, N>(vqmovn_s32(vcombine_s32(v.raw, v.raw)));
}
template <size_t N, HWY_IF_LE64(int32_t, N)>
HWY_API Vec128<uint8_t, N> DemoteTo(Simd<uint8_t, N, 0> /* tag */,
                                    const Vec128<int32_t, N> v) {
  const uint16x4_t a = vqmovun_s32(vcombine_s32(v.raw, v.raw));
  return Vec128<uint8_t, N>(vqmovn_u16(vcombine_u16(a, a)));
}
template <size_t N, HWY_IF_LE64(int16_t, N)>
HWY_API Vec128<uint8_t, N> DemoteTo(Simd<uint8_t, N, 0> /* tag */,
                                    const Vec128<int16_t, N> v) {
  return Vec128<uint8_t, N>(vqmovun_s16(vcombine_s16(v.raw, v.raw)));
}
template <size_t N, HWY_IF_LE64(int32_t, N)>
HWY_API Vec128<int8_t, N> DemoteTo(Simd<int8_t, N, 0> /* tag */,
                                   const Vec128<int32_t, N> v) {
  const int16x4_t a = vqmovn_s32(vcombine_s32(v.raw, v.raw));
  return Vec128<int8_t, N>(vqmovn_s16(vcombine_s16(a, a)));
}
template <size_t N, HWY_IF_LE64(int16_t, N)>
HWY_API Vec128<int8_t, N> DemoteTo(Simd<int8_t, N, 0> /* tag */,
                                   const Vec128<int16_t, N> v) {
  return Vec128<int8_t, N>(vqmovn_s16(vcombine_s16(v.raw, v.raw)));
}

#if __ARM_FP & 2

HWY_API Vec128<float16_t, 4> DemoteTo(Full64<float16_t> /* tag */,
                                      const Vec128<float> v) {
  return Vec128<float16_t, 4>{vreinterpret_u16_f16(vcvt_f16_f32(v.raw))};
}
template <size_t N>
HWY_API Vec128<float16_t, N> DemoteTo(Simd<float16_t, N, 0> /* tag */,
                                      const Vec128<float, N> v) {
  const float16x4_t f16 = vcvt_f16_f32(vcombine_f32(v.raw, v.raw));
  return Vec128<float16_t, N>(vreinterpret_u16_f16(f16));
}

#else

template <size_t N>
HWY_API Vec128<float16_t, N> DemoteTo(Simd<float16_t, N, 0> df16,
                                      const Vec128<float, N> v) {
  const RebindToUnsigned<decltype(df16)> du16;
  const Rebind<uint32_t, decltype(du16)> du;
  const RebindToSigned<decltype(du)> di;
  const auto bits32 = BitCast(du, v);
  const auto sign = ShiftRight<31>(bits32);
  const auto biased_exp32 = ShiftRight<23>(bits32) & Set(du, 0xFF);
  const auto mantissa32 = bits32 & Set(du, 0x7FFFFF);

  const auto k15 = Set(di, 15);
  const auto exp = Min(BitCast(di, biased_exp32) - Set(di, 127), k15);
  const auto is_tiny = exp < Set(di, -24);

  const auto is_subnormal = exp < Set(di, -14);
  const auto biased_exp16 =
      BitCast(du, IfThenZeroElse(is_subnormal, exp + k15));
  const auto sub_exp = BitCast(du, Set(di, -14) - exp);  // [1, 11)
  const auto sub_m = (Set(du, 1) << (Set(du, 10) - sub_exp)) +
                     (mantissa32 >> (Set(du, 13) + sub_exp));
  const auto mantissa16 = IfThenElse(RebindMask(du, is_subnormal), sub_m,
                                     ShiftRight<13>(mantissa32));  // <1024

  const auto sign16 = ShiftLeft<15>(sign);
  const auto normal16 = sign16 | ShiftLeft<10>(biased_exp16) | mantissa16;
  const auto bits16 = IfThenZeroElse(is_tiny, BitCast(di, normal16));
  return Vec128<float16_t, N>(DemoteTo(du16, bits16).raw);
}

#endif

template <size_t N>
HWY_API Vec128<bfloat16_t, N> DemoteTo(Simd<bfloat16_t, N, 0> dbf16,
                                       const Vec128<float, N> v) {
  const Rebind<int32_t, decltype(dbf16)> di32;
  const Rebind<uint32_t, decltype(dbf16)> du32;  // for logical shift right
  const Rebind<uint16_t, decltype(dbf16)> du16;
  const auto bits_in_32 = BitCast(di32, ShiftRight<16>(BitCast(du32, v)));
  return BitCast(dbf16, DemoteTo(du16, bits_in_32));
}

#if HWY_ARCH_ARM_A64

HWY_API Vec64<float> DemoteTo(Full64<float> /* tag */, const Vec128<double> v) {
  return Vec64<float>(vcvt_f32_f64(v.raw));
}
HWY_API Vec32<float> DemoteTo(Full32<float> /* tag */, const Vec64<double> v) {
  return Vec32<float>(vcvt_f32_f64(vcombine_f64(v.raw, v.raw)));
}

HWY_API Vec64<int32_t> DemoteTo(Full64<int32_t> /* tag */,
                                const Vec128<double> v) {
  const int64x2_t i64 = vcvtq_s64_f64(v.raw);
  return Vec64<int32_t>(vqmovn_s64(i64));
}
HWY_API Vec32<int32_t> DemoteTo(Full32<int32_t> /* tag */,
                                const Vec64<double> v) {
  const int64x1_t i64 = vcvt_s64_f64(v.raw);
  // There is no i64x1 -> i32x1 narrow, so expand to int64x2_t first.
  const int64x2_t i64x2 = vcombine_s64(i64, i64);
  return Vec32<int32_t>(vqmovn_s64(i64x2));
}

#endif

HWY_API Vec32<uint8_t> U8FromU32(const Vec128<uint32_t> v) {
  const uint8x16_t org_v = detail::BitCastToByte(v).raw;
  const uint8x16_t w = vuzp1q_u8(org_v, org_v);
  return Vec32<uint8_t>(vget_low_u8(vuzp1q_u8(w, w)));
}
template <size_t N, HWY_IF_LE64(uint32_t, N)>
HWY_API Vec128<uint8_t, N> U8FromU32(const Vec128<uint32_t, N> v) {
  const uint8x8_t org_v = detail::BitCastToByte(v).raw;
  const uint8x8_t w = vuzp1_u8(org_v, org_v);
  return Vec128<uint8_t, N>(vuzp1_u8(w, w));
}

// In the following DemoteTo functions, |b| is purposely undefined.
// The value a needs to be extended to 128 bits so that vqmovn can be
// used and |b| is undefined so that no extra overhead is introduced.
HWY_DIAGNOSTICS(push)
HWY_DIAGNOSTICS_OFF(disable : 4701, ignored "-Wuninitialized")

template <size_t N>
HWY_API Vec128<uint8_t, N> DemoteTo(Simd<uint8_t, N, 0> /* tag */,
                                    const Vec128<int32_t> v) {
  Vec128<uint16_t, N> a = DemoteTo(Simd<uint16_t, N, 0>(), v);
  Vec128<uint16_t, N> b;
  uint16x8_t c = vcombine_u16(a.raw, b.raw);
  return Vec128<uint8_t, N>(vqmovn_u16(c));
}

template <size_t N>
HWY_API Vec128<int8_t, N> DemoteTo(Simd<int8_t, N, 0> /* tag */,
                                   const Vec128<int32_t> v) {
  Vec128<int16_t, N> a = DemoteTo(Simd<int16_t, N, 0>(), v);
  Vec128<int16_t, N> b;
  int16x8_t c = vcombine_s16(a.raw, b.raw);
  return Vec128<int8_t, N>(vqmovn_s16(c));
}

HWY_DIAGNOSTICS(pop)

// ------------------------------ Convert integer <=> floating-point

HWY_API Vec128<float> ConvertTo(Full128<float> /* tag */,
                                const Vec128<int32_t> v) {
  return Vec128<float>(vcvtq_f32_s32(v.raw));
}
template <size_t N, HWY_IF_LE64(int32_t, N)>
HWY_API Vec128<float, N> ConvertTo(Simd<float, N, 0> /* tag */,
                                   const Vec128<int32_t, N> v) {
  return Vec128<float, N>(vcvt_f32_s32(v.raw));
}

// Truncates (rounds toward zero).
HWY_API Vec128<int32_t> ConvertTo(Full128<int32_t> /* tag */,
                                  const Vec128<float> v) {
  return Vec128<int32_t>(vcvtq_s32_f32(v.raw));
}
template <size_t N, HWY_IF_LE64(float, N)>
HWY_API Vec128<int32_t, N> ConvertTo(Simd<int32_t, N, 0> /* tag */,
                                     const Vec128<float, N> v) {
  return Vec128<int32_t, N>(vcvt_s32_f32(v.raw));
}

#if HWY_ARCH_ARM_A64

HWY_API Vec128<double> ConvertTo(Full128<double> /* tag */,
                                 const Vec128<int64_t> v) {
  return Vec128<double>(vcvtq_f64_s64(v.raw));
}
HWY_API Vec64<double> ConvertTo(Full64<double> /* tag */,
                                const Vec64<int64_t> v) {
  return Vec64<double>(vcvt_f64_s64(v.raw));
}

// Truncates (rounds toward zero).
HWY_API Vec128<int64_t> ConvertTo(Full128<int64_t> /* tag */,
                                  const Vec128<double> v) {
  return Vec128<int64_t>(vcvtq_s64_f64(v.raw));
}
HWY_API Vec64<int64_t> ConvertTo(Full64<int64_t> /* tag */,
                                 const Vec64<double> v) {
  return Vec64<int64_t>(vcvt_s64_f64(v.raw));
}

#endif

// ------------------------------ Round (IfThenElse, mask, logical)

#if HWY_ARCH_ARM_A64
// Toward nearest integer
HWY_NEON_DEF_FUNCTION_ALL_FLOATS(Round, vrndn, _, 1)

// Toward zero, aka truncate
HWY_NEON_DEF_FUNCTION_ALL_FLOATS(Trunc, vrnd, _, 1)

// Toward +infinity, aka ceiling
HWY_NEON_DEF_FUNCTION_ALL_FLOATS(Ceil, vrndp, _, 1)

// Toward -infinity, aka floor
HWY_NEON_DEF_FUNCTION_ALL_FLOATS(Floor, vrndm, _, 1)
#else

// ------------------------------ Trunc

// ARMv7 only supports truncation to integer. We can either convert back to
// float (3 floating-point and 2 logic operations) or manipulate the binary32
// representation, clearing the lowest 23-exp mantissa bits. This requires 9
// integer operations and 3 constants, which is likely more expensive.

namespace detail {

// The original value is already the desired result if NaN or the magnitude is
// large (i.e. the value is already an integer).
template <size_t N>
HWY_INLINE Mask128<float, N> UseInt(const Vec128<float, N> v) {
  return Abs(v) < Set(Simd<float, N, 0>(), MantissaEnd<float>());
}

}  // namespace detail

template <size_t N>
HWY_API Vec128<float, N> Trunc(const Vec128<float, N> v) {
  const DFromV<decltype(v)> df;
  const RebindToSigned<decltype(df)> di;

  const auto integer = ConvertTo(di, v);  // round toward 0
  const auto int_f = ConvertTo(df, integer);

  return IfThenElse(detail::UseInt(v), int_f, v);
}

template <size_t N>
HWY_API Vec128<float, N> Round(const Vec128<float, N> v) {
  const DFromV<decltype(v)> df;

  // ARMv7 also lacks a native NearestInt, but we can instead rely on rounding
  // (we assume the current mode is nearest-even) after addition with a large
  // value such that no mantissa bits remain. We may need a compiler flag for
  // precise floating-point to prevent this from being "optimized" out.
  const auto max = Set(df, MantissaEnd<float>());
  const auto large = CopySignToAbs(max, v);
  const auto added = large + v;
  const auto rounded = added - large;

  // Keep original if NaN or the magnitude is large (already an int).
  return IfThenElse(Abs(v) < max, rounded, v);
}

template <size_t N>
HWY_API Vec128<float, N> Ceil(const Vec128<float, N> v) {
  const DFromV<decltype(v)> df;
  const RebindToSigned<decltype(df)> di;

  const auto integer = ConvertTo(di, v);  // round toward 0
  const auto int_f = ConvertTo(df, integer);

  // Truncating a positive non-integer ends up smaller; if so, add 1.
  const auto neg1 = ConvertTo(df, VecFromMask(di, RebindMask(di, int_f < v)));

  return IfThenElse(detail::UseInt(v), int_f - neg1, v);
}

template <size_t N>
HWY_API Vec128<float, N> Floor(const Vec128<float, N> v) {
  const DFromV<decltype(v)> df;
  const RebindToSigned<decltype(df)> di;

  const auto integer = ConvertTo(di, v);  // round toward 0
  const auto int_f = ConvertTo(df, integer);

  // Truncating a negative non-integer ends up larger; if so, subtract 1.
  const auto neg1 = ConvertTo(df, VecFromMask(di, RebindMask(di, int_f > v)));

  return IfThenElse(detail::UseInt(v), int_f + neg1, v);
}

#endif

// ------------------------------ NearestInt (Round)

#if HWY_ARCH_ARM_A64

HWY_API Vec128<int32_t> NearestInt(const Vec128<float> v) {
  return Vec128<int32_t>(vcvtnq_s32_f32(v.raw));
}
template <size_t N, HWY_IF_LE64(float, N)>
HWY_API Vec128<int32_t, N> NearestInt(const Vec128<float, N> v) {
  return Vec128<int32_t, N>(vcvtn_s32_f32(v.raw));
}

#else

template <size_t N>
HWY_API Vec128<int32_t, N> NearestInt(const Vec128<float, N> v) {
  const RebindToSigned<DFromV<decltype(v)>> di;
  return ConvertTo(di, Round(v));
}

#endif

// ------------------------------ Floating-point classification
template <typename T, size_t N>
HWY_API Mask128<T, N> IsNaN(const Vec128<T, N> v) {
  return v != v;
}

template <typename T, size_t N, HWY_IF_FLOAT(T)>
HWY_API Mask128<T, N> IsInf(const Vec128<T, N> v) {
  const Simd<T, N, 0> d;
  const RebindToSigned<decltype(d)> di;
  const VFromD<decltype(di)> vi = BitCast(di, v);
  // 'Shift left' to clear the sign bit, check for exponent=max and mantissa=0.
  return RebindMask(d, Eq(Add(vi, vi), Set(di, hwy::MaxExponentTimes2<T>())));
}

// Returns whether normal/subnormal/zero.
template <typename T, size_t N, HWY_IF_FLOAT(T)>
HWY_API Mask128<T, N> IsFinite(const Vec128<T, N> v) {
  const Simd<T, N, 0> d;
  const RebindToUnsigned<decltype(d)> du;
  const RebindToSigned<decltype(d)> di;  // cheaper than unsigned comparison
  const VFromD<decltype(du)> vu = BitCast(du, v);
  // 'Shift left' to clear the sign bit, then right so we can compare with the
  // max exponent (cannot compare with MaxExponentTimes2 directly because it is
  // negative and non-negative floats would be greater).
  const VFromD<decltype(di)> exp =
      BitCast(di, ShiftRight<hwy::MantissaBits<T>() + 1>(Add(vu, vu)));
  return RebindMask(d, Lt(exp, Set(di, hwy::MaxExponentField<T>())));
}

// ================================================== SWIZZLE

// ------------------------------ LowerHalf

// <= 64 bit: just return different type
template <typename T, size_t N, HWY_IF_LE64(uint8_t, N)>
HWY_API Vec128<T, N / 2> LowerHalf(const Vec128<T, N> v) {
  return Vec128<T, N / 2>(v.raw);
}

HWY_API Vec64<uint8_t> LowerHalf(const Vec128<uint8_t> v) {
  return Vec64<uint8_t>(vget_low_u8(v.raw));
}
HWY_API Vec64<uint16_t> LowerHalf(const Vec128<uint16_t> v) {
  return Vec64<uint16_t>(vget_low_u16(v.raw));
}
HWY_API Vec64<uint32_t> LowerHalf(const Vec128<uint32_t> v) {
  return Vec64<uint32_t>(vget_low_u32(v.raw));
}
HWY_API Vec64<uint64_t> LowerHalf(const Vec128<uint64_t> v) {
  return Vec64<uint64_t>(vget_low_u64(v.raw));
}
HWY_API Vec64<int8_t> LowerHalf(const Vec128<int8_t> v) {
  return Vec64<int8_t>(vget_low_s8(v.raw));
}
HWY_API Vec64<int16_t> LowerHalf(const Vec128<int16_t> v) {
  return Vec64<int16_t>(vget_low_s16(v.raw));
}
HWY_API Vec64<int32_t> LowerHalf(const Vec128<int32_t> v) {
  return Vec64<int32_t>(vget_low_s32(v.raw));
}
HWY_API Vec64<int64_t> LowerHalf(const Vec128<int64_t> v) {
  return Vec64<int64_t>(vget_low_s64(v.raw));
}
HWY_API Vec64<float> LowerHalf(const Vec128<float> v) {
  return Vec64<float>(vget_low_f32(v.raw));
}
#if HWY_ARCH_ARM_A64
HWY_API Vec64<double> LowerHalf(const Vec128<double> v) {
  return Vec64<double>(vget_low_f64(v.raw));
}
#endif

template <typename T, size_t N>
HWY_API Vec128<T, N / 2> LowerHalf(Simd<T, N / 2, 0> /* tag */,
                                   Vec128<T, N> v) {
  return LowerHalf(v);
}

// ------------------------------ CombineShiftRightBytes

// 128-bit
template <int kBytes, typename T, class V128 = Vec128<T>>
HWY_API V128 CombineShiftRightBytes(Full128<T> d, V128 hi, V128 lo) {
  static_assert(0 < kBytes && kBytes < 16, "kBytes must be in [1, 15]");
  const Repartition<uint8_t, decltype(d)> d8;
  uint8x16_t v8 = vextq_u8(BitCast(d8, lo).raw, BitCast(d8, hi).raw, kBytes);
  return BitCast(d, Vec128<uint8_t>(v8));
}

// 64-bit
template <int kBytes, typename T>
HWY_API Vec64<T> CombineShiftRightBytes(Full64<T> d, Vec64<T> hi, Vec64<T> lo) {
  static_assert(0 < kBytes && kBytes < 8, "kBytes must be in [1, 7]");
  const Repartition<uint8_t, decltype(d)> d8;
  uint8x8_t v8 = vext_u8(BitCast(d8, lo).raw, BitCast(d8, hi).raw, kBytes);
  return BitCast(d, VFromD<decltype(d8)>(v8));
}

// <= 32-bit defined after ShiftLeftBytes.

// ------------------------------ Shift vector by constant #bytes

namespace detail {

// Partially specialize because kBytes = 0 and >= size are compile errors;
// callers replace the latter with 0xFF for easier specialization.
template <int kBytes>
struct ShiftLeftBytesT {
  // Full
  template <class T>
  HWY_INLINE Vec128<T> operator()(const Vec128<T> v) {
    const Full128<T> d;
    return CombineShiftRightBytes<16 - kBytes>(d, v, Zero(d));
  }

  // Partial
  template <class T, size_t N, HWY_IF_LE64(T, N)>
  HWY_INLINE Vec128<T, N> operator()(const Vec128<T, N> v) {
    // Expand to 64-bit so we only use the native EXT instruction.
    const Full64<T> d64;
    const auto zero64 = Zero(d64);
    const decltype(zero64) v64(v.raw);
    return Vec128<T, N>(
        CombineShiftRightBytes<8 - kBytes>(d64, v64, zero64).raw);
  }
};
template <>
struct ShiftLeftBytesT<0> {
  template <class T, size_t N>
  HWY_INLINE Vec128<T, N> operator()(const Vec128<T, N> v) {
    return v;
  }
};
template <>
struct ShiftLeftBytesT<0xFF> {
  template <class T, size_t N>
  HWY_INLINE Vec128<T, N> operator()(const Vec128<T, N> /* v */) {
    return Zero(Simd<T, N, 0>());
  }
};

template <int kBytes>
struct ShiftRightBytesT {
  template <class T, size_t N>
  HWY_INLINE Vec128<T, N> operator()(Vec128<T, N> v) {
    const Simd<T, N, 0> d;
    // For < 64-bit vectors, zero undefined lanes so we shift in zeros.
    if (N * sizeof(T) < 8) {
      constexpr size_t kReg = N * sizeof(T) == 16 ? 16 : 8;
      const Simd<T, kReg / sizeof(T), 0> dreg;
      v = Vec128<T, N>(
          IfThenElseZero(FirstN(dreg, N), VFromD<decltype(dreg)>(v.raw)).raw);
    }
    return CombineShiftRightBytes<kBytes>(d, Zero(d), v);
  }
};
template <>
struct ShiftRightBytesT<0> {
  template <class T, size_t N>
  HWY_INLINE Vec128<T, N> operator()(const Vec128<T, N> v) {
    return v;
  }
};
template <>
struct ShiftRightBytesT<0xFF> {
  template <class T, size_t N>
  HWY_INLINE Vec128<T, N> operator()(const Vec128<T, N> /* v */) {
    return Zero(Simd<T, N, 0>());
  }
};

}  // namespace detail

template <int kBytes, typename T, size_t N>
HWY_API Vec128<T, N> ShiftLeftBytes(Simd<T, N, 0> /* tag */, Vec128<T, N> v) {
  return detail::ShiftLeftBytesT < kBytes >= N * sizeof(T) ? 0xFF
                                                           : kBytes > ()(v);
}

template <int kBytes, typename T, size_t N>
HWY_API Vec128<T, N> ShiftLeftBytes(const Vec128<T, N> v) {
  return ShiftLeftBytes<kBytes>(Simd<T, N, 0>(), v);
}

template <int kLanes, typename T, size_t N>
HWY_API Vec128<T, N> ShiftLeftLanes(Simd<T, N, 0> d, const Vec128<T, N> v) {
  const Repartition<uint8_t, decltype(d)> d8;
  return BitCast(d, ShiftLeftBytes<kLanes * sizeof(T)>(BitCast(d8, v)));
}

template <int kLanes, typename T, size_t N>
HWY_API Vec128<T, N> ShiftLeftLanes(const Vec128<T, N> v) {
  return ShiftLeftLanes<kLanes>(Simd<T, N, 0>(), v);
}

// 0x01..0F, kBytes = 1 => 0x0001..0E
template <int kBytes, typename T, size_t N>
HWY_API Vec128<T, N> ShiftRightBytes(Simd<T, N, 0> /* tag */, Vec128<T, N> v) {
  return detail::ShiftRightBytesT < kBytes >= N * sizeof(T) ? 0xFF
                                                            : kBytes > ()(v);
}

template <int kLanes, typename T, size_t N>
HWY_API Vec128<T, N> ShiftRightLanes(Simd<T, N, 0> d, const Vec128<T, N> v) {
  const Repartition<uint8_t, decltype(d)> d8;
  return BitCast(d, ShiftRightBytes<kLanes * sizeof(T)>(d8, BitCast(d8, v)));
}

// Calls ShiftLeftBytes
template <int kBytes, typename T, size_t N, HWY_IF_LE32(T, N)>
HWY_API Vec128<T, N> CombineShiftRightBytes(Simd<T, N, 0> d, Vec128<T, N> hi,
                                            Vec128<T, N> lo) {
  constexpr size_t kSize = N * sizeof(T);
  static_assert(0 < kBytes && kBytes < kSize, "kBytes invalid");
  const Repartition<uint8_t, decltype(d)> d8;
  const Full64<uint8_t> d_full8;
  const Repartition<T, decltype(d_full8)> d_full;
  using V64 = VFromD<decltype(d_full8)>;
  const V64 hi64(BitCast(d8, hi).raw);
  // Move into most-significant bytes
  const V64 lo64 = ShiftLeftBytes<8 - kSize>(V64(BitCast(d8, lo).raw));
  const V64 r = CombineShiftRightBytes<8 - kSize + kBytes>(d_full8, hi64, lo64);
  // After casting to full 64-bit vector of correct type, shrink to 32-bit
  return Vec128<T, N>(BitCast(d_full, r).raw);
}

// ------------------------------ UpperHalf (ShiftRightBytes)

// Full input
HWY_API Vec64<uint8_t> UpperHalf(Full64<uint8_t> /* tag */,
                                 const Vec128<uint8_t> v) {
  return Vec64<uint8_t>(vget_high_u8(v.raw));
}
HWY_API Vec64<uint16_t> UpperHalf(Full64<uint16_t> /* tag */,
                                  const Vec128<uint16_t> v) {
  return Vec64<uint16_t>(vget_high_u16(v.raw));
}
HWY_API Vec64<uint32_t> UpperHalf(Full64<uint32_t> /* tag */,
                                  const Vec128<uint32_t> v) {
  return Vec64<uint32_t>(vget_high_u32(v.raw));
}
HWY_API Vec64<uint64_t> UpperHalf(Full64<uint64_t> /* tag */,
                                  const Vec128<uint64_t> v) {
  return Vec64<uint64_t>(vget_high_u64(v.raw));
}
HWY_API Vec64<int8_t> UpperHalf(Full64<int8_t> /* tag */,
                                const Vec128<int8_t> v) {
  return Vec64<int8_t>(vget_high_s8(v.raw));
}
HWY_API Vec64<int16_t> UpperHalf(Full64<int16_t> /* tag */,
                                 const Vec128<int16_t> v) {
  return Vec64<int16_t>(vget_high_s16(v.raw));
}
HWY_API Vec64<int32_t> UpperHalf(Full64<int32_t> /* tag */,
                                 const Vec128<int32_t> v) {
  return Vec64<int32_t>(vget_high_s32(v.raw));
}
HWY_API Vec64<int64_t> UpperHalf(Full64<int64_t> /* tag */,
                                 const Vec128<int64_t> v) {
  return Vec64<int64_t>(vget_high_s64(v.raw));
}
HWY_API Vec64<float> UpperHalf(Full64<float> /* tag */, const Vec128<float> v) {
  return Vec64<float>(vget_high_f32(v.raw));
}
#if HWY_ARCH_ARM_A64
HWY_API Vec64<double> UpperHalf(Full64<double> /* tag */,
                                const Vec128<double> v) {
  return Vec64<double>(vget_high_f64(v.raw));
}
#endif

// Partial
template <typename T, size_t N, HWY_IF_LE64(T, N)>
HWY_API Vec128<T, (N + 1) / 2> UpperHalf(Half<Simd<T, N, 0>> /* tag */,
                                         Vec128<T, N> v) {
  const DFromV<decltype(v)> d;
  const RebindToUnsigned<decltype(d)> du;
  const auto vu = BitCast(du, v);
  const auto upper = BitCast(d, ShiftRightBytes<N * sizeof(T) / 2>(du, vu));
  return Vec128<T, (N + 1) / 2>(upper.raw);
}

// ------------------------------ Broadcast/splat any lane

#if HWY_ARCH_ARM_A64
// Unsigned
template <int kLane>
HWY_API Vec128<uint16_t> Broadcast(const Vec128<uint16_t> v) {
  static_assert(0 <= kLane && kLane < 8, "Invalid lane");
  return Vec128<uint16_t>(vdupq_laneq_u16(v.raw, kLane));
}
template <int kLane, size_t N, HWY_IF_LE64(uint16_t, N)>
HWY_API Vec128<uint16_t, N> Broadcast(const Vec128<uint16_t, N> v) {
  static_assert(0 <= kLane && kLane < N, "Invalid lane");
  return Vec128<uint16_t, N>(vdup_lane_u16(v.raw, kLane));
}
template <int kLane>
HWY_API Vec128<uint32_t> Broadcast(const Vec128<uint32_t> v) {
  static_assert(0 <= kLane && kLane < 4, "Invalid lane");
  return Vec128<uint32_t>(vdupq_laneq_u32(v.raw, kLane));
}
template <int kLane, size_t N, HWY_IF_LE64(uint32_t, N)>
HWY_API Vec128<uint32_t, N> Broadcast(const Vec128<uint32_t, N> v) {
  static_assert(0 <= kLane && kLane < N, "Invalid lane");
  return Vec128<uint32_t, N>(vdup_lane_u32(v.raw, kLane));
}
template <int kLane>
HWY_API Vec128<uint64_t> Broadcast(const Vec128<uint64_t> v) {
  static_assert(0 <= kLane && kLane < 2, "Invalid lane");
  return Vec128<uint64_t>(vdupq_laneq_u64(v.raw, kLane));
}
// Vec64<uint64_t> is defined below.

// Signed
template <int kLane>
HWY_API Vec128<int16_t> Broadcast(const Vec128<int16_t> v) {
  static_assert(0 <= kLane && kLane < 8, "Invalid lane");
  return Vec128<int16_t>(vdupq_laneq_s16(v.raw, kLane));
}
template <int kLane, size_t N, HWY_IF_LE64(int16_t, N)>
HWY_API Vec128<int16_t, N> Broadcast(const Vec128<int16_t, N> v) {
  static_assert(0 <= kLane && kLane < N, "Invalid lane");
  return Vec128<int16_t, N>(vdup_lane_s16(v.raw, kLane));
}
template <int kLane>
HWY_API Vec128<int32_t> Broadcast(const Vec128<int32_t> v) {
  static_assert(0 <= kLane && kLane < 4, "Invalid lane");
  return Vec128<int32_t>(vdupq_laneq_s32(v.raw, kLane));
}
template <int kLane, size_t N, HWY_IF_LE64(int32_t, N)>
HWY_API Vec128<int32_t, N> Broadcast(const Vec128<int32_t, N> v) {
  static_assert(0 <= kLane && kLane < N, "Invalid lane");
  return Vec128<int32_t, N>(vdup_lane_s32(v.raw, kLane));
}
template <int kLane>
HWY_API Vec128<int64_t> Broadcast(const Vec128<int64_t> v) {
  static_assert(0 <= kLane && kLane < 2, "Invalid lane");
  return Vec128<int64_t>(vdupq_laneq_s64(v.raw, kLane));
}
// Vec64<int64_t> is defined below.

// Float
template <int kLane>
HWY_API Vec128<float> Broadcast(const Vec128<float> v) {
  static_assert(0 <= kLane && kLane < 4, "Invalid lane");
  return Vec128<float>(vdupq_laneq_f32(v.raw, kLane));
}
template <int kLane, size_t N, HWY_IF_LE64(float, N)>
HWY_API Vec128<float, N> Broadcast(const Vec128<float, N> v) {
  static_assert(0 <= kLane && kLane < N, "Invalid lane");
  return Vec128<float, N>(vdup_lane_f32(v.raw, kLane));
}
template <int kLane>
HWY_API Vec128<double> Broadcast(const Vec128<double> v) {
  static_assert(0 <= kLane && kLane < 2, "Invalid lane");
  return Vec128<double>(vdupq_laneq_f64(v.raw, kLane));
}
template <int kLane>
HWY_API Vec64<double> Broadcast(const Vec64<double> v) {
  static_assert(0 <= kLane && kLane < 1, "Invalid lane");
  return v;
}

#else
// No vdupq_laneq_* on armv7: use vgetq_lane_* + vdupq_n_*.

// Unsigned
template <int kLane>
HWY_API Vec128<uint16_t> Broadcast(const Vec128<uint16_t> v) {
  static_assert(0 <= kLane && kLane < 8, "Invalid lane");
  return Vec128<uint16_t>(vdupq_n_u16(vgetq_lane_u16(v.raw, kLane)));
}
template <int kLane, size_t N, HWY_IF_LE64(uint16_t, N)>
HWY_API Vec128<uint16_t, N> Broadcast(const Vec128<uint16_t, N> v) {
  static_assert(0 <= kLane && kLane < N, "Invalid lane");
  return Vec128<uint16_t, N>(vdup_lane_u16(v.raw, kLane));
}
template <int kLane>
HWY_API Vec128<uint32_t> Broadcast(const Vec128<uint32_t> v) {
  static_assert(0 <= kLane && kLane < 4, "Invalid lane");
  return Vec128<uint32_t>(vdupq_n_u32(vgetq_lane_u32(v.raw, kLane)));
}
template <int kLane, size_t N, HWY_IF_LE64(uint32_t, N)>
HWY_API Vec128<uint32_t, N> Broadcast(const Vec128<uint32_t, N> v) {
  static_assert(0 <= kLane && kLane < N, "Invalid lane");
  return Vec128<uint32_t, N>(vdup_lane_u32(v.raw, kLane));
}
template <int kLane>
HWY_API Vec128<uint64_t> Broadcast(const Vec128<uint64_t> v) {
  static_assert(0 <= kLane && kLane < 2, "Invalid lane");
  return Vec128<uint64_t>(vdupq_n_u64(vgetq_lane_u64(v.raw, kLane)));
}
// Vec64<uint64_t> is defined below.

// Signed
template <int kLane>
HWY_API Vec128<int16_t> Broadcast(const Vec128<int16_t> v) {
  static_assert(0 <= kLane && kLane < 8, "Invalid lane");
  return Vec128<int16_t>(vdupq_n_s16(vgetq_lane_s16(v.raw, kLane)));
}
template <int kLane, size_t N, HWY_IF_LE64(int16_t, N)>
HWY_API Vec128<int16_t, N> Broadcast(const Vec128<int16_t, N> v) {
  static_assert(0 <= kLane && kLane < N, "Invalid lane");
  return Vec128<int16_t, N>(vdup_lane_s16(v.raw, kLane));
}
template <int kLane>
HWY_API Vec128<int32_t> Broadcast(const Vec128<int32_t> v) {
  static_assert(0 <= kLane && kLane < 4, "Invalid lane");
  return Vec128<int32_t>(vdupq_n_s32(vgetq_lane_s32(v.raw, kLane)));
}
template <int kLane, size_t N, HWY_IF_LE64(int32_t, N)>
HWY_API Vec128<int32_t, N> Broadcast(const Vec128<int32_t, N> v) {
  static_assert(0 <= kLane && kLane < N, "Invalid lane");
  return Vec128<int32_t, N>(vdup_lane_s32(v.raw, kLane));
}
template <int kLane>
HWY_API Vec128<int64_t> Broadcast(const Vec128<int64_t> v) {
  static_assert(0 <= kLane && kLane < 2, "Invalid lane");
  return Vec128<int64_t>(vdupq_n_s64(vgetq_lane_s64(v.raw, kLane)));
}
// Vec64<int64_t> is defined below.

// Float
template <int kLane>
HWY_API Vec128<float> Broadcast(const Vec128<float> v) {
  static_assert(0 <= kLane && kLane < 4, "Invalid lane");
  return Vec128<float>(vdupq_n_f32(vgetq_lane_f32(v.raw, kLane)));
}
template <int kLane, size_t N, HWY_IF_LE64(float, N)>
HWY_API Vec128<float, N> Broadcast(const Vec128<float, N> v) {
  static_assert(0 <= kLane && kLane < N, "Invalid lane");
  return Vec128<float, N>(vdup_lane_f32(v.raw, kLane));
}

#endif

template <int kLane>
HWY_API Vec64<uint64_t> Broadcast(const Vec64<uint64_t> v) {
  static_assert(0 <= kLane && kLane < 1, "Invalid lane");
  return v;
}
template <int kLane>
HWY_API Vec64<int64_t> Broadcast(const Vec64<int64_t> v) {
  static_assert(0 <= kLane && kLane < 1, "Invalid lane");
  return v;
}

// ------------------------------ TableLookupLanes

// Returned by SetTableIndices for use by TableLookupLanes.
template <typename T, size_t N>
struct Indices128 {
  typename detail::Raw128<T, N>::type raw;
};

template <typename T, size_t N, typename TI, HWY_IF_LE128(T, N)>
HWY_API Indices128<T, N> IndicesFromVec(Simd<T, N, 0> d, Vec128<TI, N> vec) {
  static_assert(sizeof(T) == sizeof(TI), "Index size must match lane");
#if HWY_IS_DEBUG_BUILD
  const Rebind<TI, decltype(d)> di;
  HWY_DASSERT(AllFalse(di, Lt(vec, Zero(di))) &&
              AllTrue(di, Lt(vec, Set(di, static_cast<TI>(N)))));
#endif

  const Repartition<uint8_t, decltype(d)> d8;
  using V8 = VFromD<decltype(d8)>;
  const Repartition<uint16_t, decltype(d)> d16;

  // Broadcast each lane index to all bytes of T and shift to bytes
  static_assert(sizeof(T) == 4 || sizeof(T) == 8, "");
  if (sizeof(T) == 4) {
    alignas(16) constexpr uint8_t kBroadcastLaneBytes[16] = {
        0, 0, 0, 0, 4, 4, 4, 4, 8, 8, 8, 8, 12, 12, 12, 12};
    const V8 lane_indices =
        TableLookupBytes(BitCast(d8, vec), Load(d8, kBroadcastLaneBytes));
    const V8 byte_indices =
        BitCast(d8, ShiftLeft<2>(BitCast(d16, lane_indices)));
    alignas(16) constexpr uint8_t kByteOffsets[16] = {0, 1, 2, 3, 0, 1, 2, 3,
                                                      0, 1, 2, 3, 0, 1, 2, 3};
    const V8 sum = Add(byte_indices, Load(d8, kByteOffsets));
    return Indices128<T, N>{BitCast(d, sum).raw};
  } else {
    alignas(16) constexpr uint8_t kBroadcastLaneBytes[16] = {
        0, 0, 0, 0, 0, 0, 0, 0, 8, 8, 8, 8, 8, 8, 8, 8};
    const V8 lane_indices =
        TableLookupBytes(BitCast(d8, vec), Load(d8, kBroadcastLaneBytes));
    const V8 byte_indices =
        BitCast(d8, ShiftLeft<3>(BitCast(d16, lane_indices)));
    alignas(16) constexpr uint8_t kByteOffsets[16] = {0, 1, 2, 3, 4, 5, 6, 7,
                                                      0, 1, 2, 3, 4, 5, 6, 7};
    const V8 sum = Add(byte_indices, Load(d8, kByteOffsets));
    return Indices128<T, N>{BitCast(d, sum).raw};
  }
}

template <typename T, size_t N, typename TI, HWY_IF_LE128(T, N)>
HWY_API Indices128<T, N> SetTableIndices(Simd<T, N, 0> d, const TI* idx) {
  const Rebind<TI, decltype(d)> di;
  return IndicesFromVec(d, LoadU(di, idx));
}

template <typename T, size_t N>
HWY_API Vec128<T, N> TableLookupLanes(Vec128<T, N> v, Indices128<T, N> idx) {
  const DFromV<decltype(v)> d;
  const RebindToSigned<decltype(d)> di;
  return BitCast(
      d, TableLookupBytes(BitCast(di, v), BitCast(di, Vec128<T, N>{idx.raw})));
}

// ------------------------------ Reverse (Shuffle0123, Shuffle2301, Shuffle01)

// Single lane: no change
template <typename T>
HWY_API Vec128<T, 1> Reverse(Simd<T, 1, 0> /* tag */, const Vec128<T, 1> v) {
  return v;
}

// Two lanes: shuffle
template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec128<T, 2> Reverse(Simd<T, 2, 0> /* tag */, const Vec128<T, 2> v) {
  return Vec128<T, 2>(Shuffle2301(v));
}

template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Vec128<T> Reverse(Full128<T> /* tag */, const Vec128<T> v) {
  return Shuffle01(v);
}

// Four lanes: shuffle
template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec128<T> Reverse(Full128<T> /* tag */, const Vec128<T> v) {
  return Shuffle0123(v);
}

// 16-bit
template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Vec128<T, N> Reverse(Simd<T, N, 0> d, const Vec128<T, N> v) {
  const RepartitionToWide<RebindToUnsigned<decltype(d)>> du32;
  return BitCast(d, RotateRight<16>(Reverse(du32, BitCast(du32, v))));
}

// ------------------------------ Reverse2

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 2), HWY_IF_LE64(T, N)>
HWY_API Vec128<T, N> Reverse2(Simd<T, N, 0> d, const Vec128<T, N> v) {
  const RebindToUnsigned<decltype(d)> du;
  return BitCast(d, Vec128<uint16_t, N>(vrev32_u16(BitCast(du, v).raw)));
}
template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Vec128<T> Reverse2(Full128<T> d, const Vec128<T> v) {
  const RebindToUnsigned<decltype(d)> du;
  return BitCast(d, Vec128<uint16_t>(vrev32q_u16(BitCast(du, v).raw)));
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 4), HWY_IF_LE64(T, N)>
HWY_API Vec128<T, N> Reverse2(Simd<T, N, 0> d, const Vec128<T, N> v) {
  const RebindToUnsigned<decltype(d)> du;
  return BitCast(d, Vec128<uint32_t, N>(vrev64_u32(BitCast(du, v).raw)));
}
template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec128<T> Reverse2(Full128<T> d, const Vec128<T> v) {
  const RebindToUnsigned<decltype(d)> du;
  return BitCast(d, Vec128<uint32_t>(vrev64q_u32(BitCast(du, v).raw)));
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Vec128<T, N> Reverse2(Simd<T, N, 0> /* tag */, const Vec128<T, N> v) {
  return Shuffle01(v);
}

// ------------------------------ Reverse4

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 2), HWY_IF_LE64(T, N)>
HWY_API Vec128<T, N> Reverse4(Simd<T, N, 0> d, const Vec128<T, N> v) {
  const RebindToUnsigned<decltype(d)> du;
  return BitCast(d, Vec128<uint16_t, N>(vrev64_u16(BitCast(du, v).raw)));
}
template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Vec128<T> Reverse4(Full128<T> d, const Vec128<T> v) {
  const RebindToUnsigned<decltype(d)> du;
  return BitCast(d, Vec128<uint16_t>(vrev64q_u16(BitCast(du, v).raw)));
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec128<T, N> Reverse4(Simd<T, N, 0> /* tag */, const Vec128<T, N> v) {
  return Shuffle0123(v);
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Vec128<T, N> Reverse4(Simd<T, N, 0> /* tag */, const Vec128<T, N>) {
  HWY_ASSERT(0);  // don't have 8 u64 lanes
}

// ------------------------------ Reverse8

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Vec128<T, N> Reverse8(Simd<T, N, 0> d, const Vec128<T, N> v) {
  return Reverse(d, v);
}

template <typename T, size_t N, HWY_IF_NOT_LANE_SIZE(T, 2)>
HWY_API Vec128<T, N> Reverse8(Simd<T, N, 0>, const Vec128<T, N>) {
  HWY_ASSERT(0);  // don't have 8 lanes unless 16-bit
}

// ------------------------------ Other shuffles (TableLookupBytes)

// Notation: let Vec128<int32_t> have lanes 3,2,1,0 (0 is least-significant).
// Shuffle0321 rotates one lane to the right (the previous least-significant
// lane is now most-significant). These could also be implemented via
// CombineShiftRightBytes but the shuffle_abcd notation is more convenient.

// Swap 64-bit halves
template <typename T>
HWY_API Vec128<T> Shuffle1032(const Vec128<T> v) {
  return CombineShiftRightBytes<8>(Full128<T>(), v, v);
}
template <typename T>
HWY_API Vec128<T> Shuffle01(const Vec128<T> v) {
  return CombineShiftRightBytes<8>(Full128<T>(), v, v);
}

// Rotate right 32 bits
template <typename T>
HWY_API Vec128<T> Shuffle0321(const Vec128<T> v) {
  return CombineShiftRightBytes<4>(Full128<T>(), v, v);
}

// Rotate left 32 bits
template <typename T>
HWY_API Vec128<T> Shuffle2103(const Vec128<T> v) {
  return CombineShiftRightBytes<12>(Full128<T>(), v, v);
}

// Reverse
template <typename T>
HWY_API Vec128<T> Shuffle0123(const Vec128<T> v) {
  return Shuffle2301(Shuffle1032(v));
}

// ------------------------------ InterleaveLower

// Interleaves lanes from halves of the 128-bit blocks of "a" (which provides
// the least-significant lane) and "b". To concatenate two half-width integers
// into one, use ZipLower/Upper instead (also works with scalar).
HWY_NEON_DEF_FUNCTION_INT_8_16_32(InterleaveLower, vzip1, _, 2)
HWY_NEON_DEF_FUNCTION_UINT_8_16_32(InterleaveLower, vzip1, _, 2)

#if HWY_ARCH_ARM_A64
// N=1 makes no sense (in that case, there would be no upper/lower).
HWY_API Vec128<uint64_t> InterleaveLower(const Vec128<uint64_t> a,
                                         const Vec128<uint64_t> b) {
  return Vec128<uint64_t>(vzip1q_u64(a.raw, b.raw));
}
HWY_API Vec128<int64_t> InterleaveLower(const Vec128<int64_t> a,
                                        const Vec128<int64_t> b) {
  return Vec128<int64_t>(vzip1q_s64(a.raw, b.raw));
}
HWY_API Vec128<double> InterleaveLower(const Vec128<double> a,
                                       const Vec128<double> b) {
  return Vec128<double>(vzip1q_f64(a.raw, b.raw));
}
#else
// ARMv7 emulation.
HWY_API Vec128<uint64_t> InterleaveLower(const Vec128<uint64_t> a,
                                         const Vec128<uint64_t> b) {
  return CombineShiftRightBytes<8>(Full128<uint64_t>(), b, Shuffle01(a));
}
HWY_API Vec128<int64_t> InterleaveLower(const Vec128<int64_t> a,
                                        const Vec128<int64_t> b) {
  return CombineShiftRightBytes<8>(Full128<int64_t>(), b, Shuffle01(a));
}
#endif

// Floats
HWY_API Vec128<float> InterleaveLower(const Vec128<float> a,
                                      const Vec128<float> b) {
  return Vec128<float>(vzip1q_f32(a.raw, b.raw));
}
template <size_t N, HWY_IF_LE64(float, N)>
HWY_API Vec128<float, N> InterleaveLower(const Vec128<float, N> a,
                                         const Vec128<float, N> b) {
  return Vec128<float, N>(vzip1_f32(a.raw, b.raw));
}

// < 64 bit parts
template <typename T, size_t N, HWY_IF_LE32(T, N)>
HWY_API Vec128<T, N> InterleaveLower(Vec128<T, N> a, Vec128<T, N> b) {
  return Vec128<T, N>(InterleaveLower(Vec64<T>(a.raw), Vec64<T>(b.raw)).raw);
}

// Additional overload for the optional Simd<> tag.
template <typename T, size_t N, class V = Vec128<T, N>>
HWY_API V InterleaveLower(Simd<T, N, 0> /* tag */, V a, V b) {
  return InterleaveLower(a, b);
}

// ------------------------------ InterleaveUpper (UpperHalf)

// All functions inside detail lack the required D parameter.
namespace detail {
HWY_NEON_DEF_FUNCTION_INT_8_16_32(InterleaveUpper, vzip2, _, 2)
HWY_NEON_DEF_FUNCTION_UINT_8_16_32(InterleaveUpper, vzip2, _, 2)

#if HWY_ARCH_ARM_A64
// N=1 makes no sense (in that case, there would be no upper/lower).
HWY_API Vec128<uint64_t> InterleaveUpper(const Vec128<uint64_t> a,
                                         const Vec128<uint64_t> b) {
  return Vec128<uint64_t>(vzip2q_u64(a.raw, b.raw));
}
HWY_API Vec128<int64_t> InterleaveUpper(Vec128<int64_t> a, Vec128<int64_t> b) {
  return Vec128<int64_t>(vzip2q_s64(a.raw, b.raw));
}
HWY_API Vec128<double> InterleaveUpper(Vec128<double> a, Vec128<double> b) {
  return Vec128<double>(vzip2q_f64(a.raw, b.raw));
}
#else
// ARMv7 emulation.
HWY_API Vec128<uint64_t> InterleaveUpper(const Vec128<uint64_t> a,
                                         const Vec128<uint64_t> b) {
  return CombineShiftRightBytes<8>(Full128<uint64_t>(), Shuffle01(b), a);
}
HWY_API Vec128<int64_t> InterleaveUpper(Vec128<int64_t> a, Vec128<int64_t> b) {
  return CombineShiftRightBytes<8>(Full128<int64_t>(), Shuffle01(b), a);
}
#endif

HWY_API Vec128<float> InterleaveUpper(Vec128<float> a, Vec128<float> b) {
  return Vec128<float>(vzip2q_f32(a.raw, b.raw));
}
HWY_API Vec64<float> InterleaveUpper(const Vec64<float> a,
                                     const Vec64<float> b) {
  return Vec64<float>(vzip2_f32(a.raw, b.raw));
}

}  // namespace detail

// Full register
template <typename T, size_t N, HWY_IF_GE64(T, N), class V = Vec128<T, N>>
HWY_API V InterleaveUpper(Simd<T, N, 0> /* tag */, V a, V b) {
  return detail::InterleaveUpper(a, b);
}

// Partial
template <typename T, size_t N, HWY_IF_LE32(T, N), class V = Vec128<T, N>>
HWY_API V InterleaveUpper(Simd<T, N, 0> d, V a, V b) {
  const Half<decltype(d)> d2;
  return InterleaveLower(d, V(UpperHalf(d2, a).raw), V(UpperHalf(d2, b).raw));
}

// ------------------------------ ZipLower/ZipUpper (InterleaveLower)

// Same as Interleave*, except that the return lanes are double-width integers;
// this is necessary because the single-lane scalar cannot return two values.
template <class V, class DW = RepartitionToWide<DFromV<V>>>
HWY_API VFromD<DW> ZipLower(V a, V b) {
  return BitCast(DW(), InterleaveLower(a, b));
}
template <class V, class D = DFromV<V>, class DW = RepartitionToWide<D>>
HWY_API VFromD<DW> ZipLower(DW dw, V a, V b) {
  return BitCast(dw, InterleaveLower(D(), a, b));
}

template <class V, class D = DFromV<V>, class DW = RepartitionToWide<D>>
HWY_API VFromD<DW> ZipUpper(DW dw, V a, V b) {
  return BitCast(dw, InterleaveUpper(D(), a, b));
}

// ------------------------------ ReorderWidenMulAccumulate (MulAdd, ZipLower)

template <size_t N>
HWY_API Vec128<float, N> ReorderWidenMulAccumulate(Simd<float, N, 0> df32,
                                                   Vec128<bfloat16_t, 2 * N> a,
                                                   Vec128<bfloat16_t, 2 * N> b,
                                                   const Vec128<float, N> sum0,
                                                   Vec128<float, N>& sum1) {
  const Repartition<uint16_t, decltype(df32)> du16;
  const RebindToUnsigned<decltype(df32)> du32;
  const Vec128<uint16_t, 2 * N> zero = Zero(du16);
  const Vec128<uint32_t, N> a0 = ZipLower(du32, zero, BitCast(du16, a));
  const Vec128<uint32_t, N> a1 = ZipUpper(du32, zero, BitCast(du16, a));
  const Vec128<uint32_t, N> b0 = ZipLower(du32, zero, BitCast(du16, b));
  const Vec128<uint32_t, N> b1 = ZipUpper(du32, zero, BitCast(du16, b));
  sum1 = MulAdd(BitCast(df32, a1), BitCast(df32, b1), sum1);
  return MulAdd(BitCast(df32, a0), BitCast(df32, b0), sum0);
}

// ================================================== COMBINE

// ------------------------------ Combine (InterleaveLower)

// Full result
HWY_API Vec128<uint8_t> Combine(Full128<uint8_t> /* tag */, Vec64<uint8_t> hi,
                                Vec64<uint8_t> lo) {
  return Vec128<uint8_t>(vcombine_u8(lo.raw, hi.raw));
}
HWY_API Vec128<uint16_t> Combine(Full128<uint16_t> /* tag */,
                                 Vec64<uint16_t> hi, Vec64<uint16_t> lo) {
  return Vec128<uint16_t>(vcombine_u16(lo.raw, hi.raw));
}
HWY_API Vec128<uint32_t> Combine(Full128<uint32_t> /* tag */,
                                 Vec64<uint32_t> hi, Vec64<uint32_t> lo) {
  return Vec128<uint32_t>(vcombine_u32(lo.raw, hi.raw));
}
HWY_API Vec128<uint64_t> Combine(Full128<uint64_t> /* tag */,
                                 Vec64<uint64_t> hi, Vec64<uint64_t> lo) {
  return Vec128<uint64_t>(vcombine_u64(lo.raw, hi.raw));
}

HWY_API Vec128<int8_t> Combine(Full128<int8_t> /* tag */, Vec64<int8_t> hi,
                               Vec64<int8_t> lo) {
  return Vec128<int8_t>(vcombine_s8(lo.raw, hi.raw));
}
HWY_API Vec128<int16_t> Combine(Full128<int16_t> /* tag */, Vec64<int16_t> hi,
                                Vec64<int16_t> lo) {
  return Vec128<int16_t>(vcombine_s16(lo.raw, hi.raw));
}
HWY_API Vec128<int32_t> Combine(Full128<int32_t> /* tag */, Vec64<int32_t> hi,
                                Vec64<int32_t> lo) {
  return Vec128<int32_t>(vcombine_s32(lo.raw, hi.raw));
}
HWY_API Vec128<int64_t> Combine(Full128<int64_t> /* tag */, Vec64<int64_t> hi,
                                Vec64<int64_t> lo) {
  return Vec128<int64_t>(vcombine_s64(lo.raw, hi.raw));
}

HWY_API Vec128<float> Combine(Full128<float> /* tag */, Vec64<float> hi,
                              Vec64<float> lo) {
  return Vec128<float>(vcombine_f32(lo.raw, hi.raw));
}
#if HWY_ARCH_ARM_A64
HWY_API Vec128<double> Combine(Full128<double> /* tag */, Vec64<double> hi,
                               Vec64<double> lo) {
  return Vec128<double>(vcombine_f64(lo.raw, hi.raw));
}
#endif

// < 64bit input, <= 64 bit result
template <typename T, size_t N, HWY_IF_LE64(T, N)>
HWY_API Vec128<T, N> Combine(Simd<T, N, 0> d, Vec128<T, N / 2> hi,
                             Vec128<T, N / 2> lo) {
  // First double N (only lower halves will be used).
  const Vec128<T, N> hi2(hi.raw);
  const Vec128<T, N> lo2(lo.raw);
  // Repartition to two unsigned lanes (each the size of the valid input).
  const Simd<UnsignedFromSize<N * sizeof(T) / 2>, 2, 0> du;
  return BitCast(d, InterleaveLower(BitCast(du, lo2), BitCast(du, hi2)));
}

// ------------------------------ ZeroExtendVector (Combine)

template <typename T, size_t N>
HWY_API Vec128<T, N> ZeroExtendVector(Simd<T, N, 0> d, Vec128<T, N / 2> lo) {
  return Combine(d, Zero(Half<decltype(d)>()), lo);
}

// ------------------------------ ConcatLowerLower

// 64 or 128-bit input: just interleave
template <typename T, size_t N, HWY_IF_GE64(T, N)>
HWY_API Vec128<T, N> ConcatLowerLower(const Simd<T, N, 0> d, Vec128<T, N> hi,
                                      Vec128<T, N> lo) {
  // Treat half-width input as a single lane and interleave them.
  const Repartition<UnsignedFromSize<N * sizeof(T) / 2>, decltype(d)> du;
  return BitCast(d, InterleaveLower(BitCast(du, lo), BitCast(du, hi)));
}

namespace detail {
#if HWY_ARCH_ARM_A64
HWY_NEON_DEF_FUNCTION_UIF81632(InterleaveEven, vtrn1, _, 2)
HWY_NEON_DEF_FUNCTION_UIF81632(InterleaveOdd, vtrn2, _, 2)
#else

// vtrn returns a struct with even and odd result.
#define HWY_NEON_BUILD_TPL_HWY_TRN
#define HWY_NEON_BUILD_RET_HWY_TRN(type, size) type##x##size##x2_t
// Pass raw args so we can accept uint16x2 args, for which there is no
// corresponding uint16x2x2 return type.
#define HWY_NEON_BUILD_PARAM_HWY_TRN(TYPE, size) \
  Raw128<TYPE##_t, size>::type a, Raw128<TYPE##_t, size>::type b
#define HWY_NEON_BUILD_ARG_HWY_TRN a, b

// Cannot use UINT8 etc. type macros because the x2_t tuples are only defined
// for full and half vectors.
HWY_NEON_DEF_FUNCTION(uint8, 16, InterleaveEvenOdd, vtrnq, _, u8, HWY_TRN)
HWY_NEON_DEF_FUNCTION(uint8, 8, InterleaveEvenOdd, vtrn, _, u8, HWY_TRN)
HWY_NEON_DEF_FUNCTION(uint16, 8, InterleaveEvenOdd, vtrnq, _, u16, HWY_TRN)
HWY_NEON_DEF_FUNCTION(uint16, 4, InterleaveEvenOdd, vtrn, _, u16, HWY_TRN)
HWY_NEON_DEF_FUNCTION(uint32, 4, InterleaveEvenOdd, vtrnq, _, u32, HWY_TRN)
HWY_NEON_DEF_FUNCTION(uint32, 2, InterleaveEvenOdd, vtrn, _, u32, HWY_TRN)
HWY_NEON_DEF_FUNCTION(int8, 16, InterleaveEvenOdd, vtrnq, _, s8, HWY_TRN)
HWY_NEON_DEF_FUNCTION(int8, 8, InterleaveEvenOdd, vtrn, _, s8, HWY_TRN)
HWY_NEON_DEF_FUNCTION(int16, 8, InterleaveEvenOdd, vtrnq, _, s16, HWY_TRN)
HWY_NEON_DEF_FUNCTION(int16, 4, InterleaveEvenOdd, vtrn, _, s16, HWY_TRN)
HWY_NEON_DEF_FUNCTION(int32, 4, InterleaveEvenOdd, vtrnq, _, s32, HWY_TRN)
HWY_NEON_DEF_FUNCTION(int32, 2, InterleaveEvenOdd, vtrn, _, s32, HWY_TRN)
HWY_NEON_DEF_FUNCTION(float32, 4, InterleaveEvenOdd, vtrnq, _, f32, HWY_TRN)
HWY_NEON_DEF_FUNCTION(float32, 2, InterleaveEvenOdd, vtrn, _, f32, HWY_TRN)
#endif
}  // namespace detail

// <= 32-bit input/output
template <typename T, size_t N, HWY_IF_LE32(T, N)>
HWY_API Vec128<T, N> ConcatLowerLower(const Simd<T, N, 0> d, Vec128<T, N> hi,
                                      Vec128<T, N> lo) {
  // Treat half-width input as two lanes and take every second one.
  const Repartition<UnsignedFromSize<N * sizeof(T) / 2>, decltype(d)> du;
#if HWY_ARCH_ARM_A64
  return BitCast(d, detail::InterleaveEven(BitCast(du, lo), BitCast(du, hi)));
#else
  using VU = VFromD<decltype(du)>;
  return BitCast(
      d, VU(detail::InterleaveEvenOdd(BitCast(du, lo).raw, BitCast(du, hi).raw)
                .val[0]));
#endif
}

// ------------------------------ ConcatUpperUpper

// 64 or 128-bit input: just interleave
template <typename T, size_t N, HWY_IF_GE64(T, N)>
HWY_API Vec128<T, N> ConcatUpperUpper(const Simd<T, N, 0> d, Vec128<T, N> hi,
                                      Vec128<T, N> lo) {
  // Treat half-width input as a single lane and interleave them.
  const Repartition<UnsignedFromSize<N * sizeof(T) / 2>, decltype(d)> du;
  return BitCast(d, InterleaveUpper(du, BitCast(du, lo), BitCast(du, hi)));
}

// <= 32-bit input/output
template <typename T, size_t N, HWY_IF_LE32(T, N)>
HWY_API Vec128<T, N> ConcatUpperUpper(const Simd<T, N, 0> d, Vec128<T, N> hi,
                                      Vec128<T, N> lo) {
  // Treat half-width input as two lanes and take every second one.
  const Repartition<UnsignedFromSize<N * sizeof(T) / 2>, decltype(d)> du;
#if HWY_ARCH_ARM_A64
  return BitCast(d, detail::InterleaveOdd(BitCast(du, lo), BitCast(du, hi)));
#else
  using VU = VFromD<decltype(du)>;
  return BitCast(
      d, VU(detail::InterleaveEvenOdd(BitCast(du, lo).raw, BitCast(du, hi).raw)
                .val[1]));
#endif
}

// ------------------------------ ConcatLowerUpper (ShiftLeftBytes)

// 64 or 128-bit input: extract from concatenated
template <typename T, size_t N, HWY_IF_GE64(T, N)>
HWY_API Vec128<T, N> ConcatLowerUpper(const Simd<T, N, 0> d, Vec128<T, N> hi,
                                      Vec128<T, N> lo) {
  return CombineShiftRightBytes<N * sizeof(T) / 2>(d, hi, lo);
}

// <= 32-bit input/output
template <typename T, size_t N, HWY_IF_LE32(T, N)>
HWY_API Vec128<T, N> ConcatLowerUpper(const Simd<T, N, 0> d, Vec128<T, N> hi,
                                      Vec128<T, N> lo) {
  constexpr size_t kSize = N * sizeof(T);
  const Repartition<uint8_t, decltype(d)> d8;
  const Full64<uint8_t> d8x8;
  const Full64<T> d64;
  using V8x8 = VFromD<decltype(d8x8)>;
  const V8x8 hi8x8(BitCast(d8, hi).raw);
  // Move into most-significant bytes
  const V8x8 lo8x8 = ShiftLeftBytes<8 - kSize>(V8x8(BitCast(d8, lo).raw));
  const V8x8 r = CombineShiftRightBytes<8 - kSize / 2>(d8x8, hi8x8, lo8x8);
  // Back to original lane type, then shrink N.
  return Vec128<T, N>(BitCast(d64, r).raw);
}

// ------------------------------ ConcatUpperLower

// Works for all N.
template <typename T, size_t N>
HWY_API Vec128<T, N> ConcatUpperLower(Simd<T, N, 0> d, Vec128<T, N> hi,
                                      Vec128<T, N> lo) {
  return IfThenElse(FirstN(d, Lanes(d) / 2), lo, hi);
}

// ------------------------------ ConcatOdd (InterleaveUpper)

namespace detail {
// There is no vuzpq_u64.
HWY_NEON_DEF_FUNCTION_UIF81632(ConcatEven, vuzp1, _, 2)
HWY_NEON_DEF_FUNCTION_UIF81632(ConcatOdd, vuzp2, _, 2)
}  // namespace detail

// Full/half vector
template <typename T, size_t N,
          hwy::EnableIf<N != 2 && sizeof(T) * N >= 8>* = nullptr>
HWY_API Vec128<T, N> ConcatOdd(Simd<T, N, 0> /* tag */, Vec128<T, N> hi,
                               Vec128<T, N> lo) {
  return detail::ConcatOdd(lo, hi);
}

// 8-bit x4
template <typename T, HWY_IF_LANE_SIZE(T, 1)>
HWY_API Vec128<T, 4> ConcatOdd(Simd<T, 4, 0> d, Vec128<T, 4> hi,
                               Vec128<T, 4> lo) {
  const Twice<decltype(d)> d2;
  const Repartition<uint16_t, decltype(d2)> dw2;
  const VFromD<decltype(d2)> hi2(hi.raw);
  const VFromD<decltype(d2)> lo2(lo.raw);
  const VFromD<decltype(dw2)> Hx1Lx1 = BitCast(dw2, ConcatOdd(d2, hi2, lo2));
  // Compact into two pairs of u8, skipping the invalid x lanes. Could also use
  // vcopy_lane_u16, but that's A64-only.
  return Vec128<T, 4>(BitCast(d2, ConcatEven(dw2, Hx1Lx1, Hx1Lx1)).raw);
}

// Any type x2
template <typename T>
HWY_API Vec128<T, 2> ConcatOdd(Simd<T, 2, 0> d, Vec128<T, 2> hi,
                               Vec128<T, 2> lo) {
  return InterleaveUpper(d, lo, hi);
}

// ------------------------------ ConcatEven (InterleaveLower)

// Full/half vector
template <typename T, size_t N,
          hwy::EnableIf<N != 2 && sizeof(T) * N >= 8>* = nullptr>
HWY_API Vec128<T, N> ConcatEven(Simd<T, N, 0> /* tag */, Vec128<T, N> hi,
                                Vec128<T, N> lo) {
  return detail::ConcatEven(lo, hi);
}

// 8-bit x4
template <typename T, HWY_IF_LANE_SIZE(T, 1)>
HWY_API Vec128<T, 4> ConcatEven(Simd<T, 4, 0> d, Vec128<T, 4> hi,
                                Vec128<T, 4> lo) {
  const Twice<decltype(d)> d2;
  const Repartition<uint16_t, decltype(d2)> dw2;
  const VFromD<decltype(d2)> hi2(hi.raw);
  const VFromD<decltype(d2)> lo2(lo.raw);
  const VFromD<decltype(dw2)> Hx0Lx0 = BitCast(dw2, ConcatEven(d2, hi2, lo2));
  // Compact into two pairs of u8, skipping the invalid x lanes. Could also use
  // vcopy_lane_u16, but that's A64-only.
  return Vec128<T, 4>(BitCast(d2, ConcatEven(dw2, Hx0Lx0, Hx0Lx0)).raw);
}

// Any type x2
template <typename T>
HWY_API Vec128<T, 2> ConcatEven(Simd<T, 2, 0> d, Vec128<T, 2> hi,
                                Vec128<T, 2> lo) {
  return InterleaveLower(d, lo, hi);
}

// ------------------------------ DupEven (InterleaveLower)

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec128<T, N> DupEven(Vec128<T, N> v) {
#if HWY_ARCH_ARM_A64
  return detail::InterleaveEven(v, v);
#else
  return Vec128<T, N>(detail::InterleaveEvenOdd(v.raw, v.raw).val[0]);
#endif
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Vec128<T, N> DupEven(const Vec128<T, N> v) {
  return InterleaveLower(Simd<T, N, 0>(), v, v);
}

// ------------------------------ DupOdd (InterleaveUpper)

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec128<T, N> DupOdd(Vec128<T, N> v) {
#if HWY_ARCH_ARM_A64
  return detail::InterleaveOdd(v, v);
#else
  return Vec128<T, N>(detail::InterleaveEvenOdd(v.raw, v.raw).val[1]);
#endif
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Vec128<T, N> DupOdd(const Vec128<T, N> v) {
  return InterleaveUpper(Simd<T, N, 0>(), v, v);
}

// ------------------------------ OddEven (IfThenElse)

template <typename T, size_t N>
HWY_API Vec128<T, N> OddEven(const Vec128<T, N> a, const Vec128<T, N> b) {
  const Simd<T, N, 0> d;
  const Repartition<uint8_t, decltype(d)> d8;
  alignas(16) constexpr uint8_t kBytes[16] = {
      ((0 / sizeof(T)) & 1) ? 0 : 0xFF,  ((1 / sizeof(T)) & 1) ? 0 : 0xFF,
      ((2 / sizeof(T)) & 1) ? 0 : 0xFF,  ((3 / sizeof(T)) & 1) ? 0 : 0xFF,
      ((4 / sizeof(T)) & 1) ? 0 : 0xFF,  ((5 / sizeof(T)) & 1) ? 0 : 0xFF,
      ((6 / sizeof(T)) & 1) ? 0 : 0xFF,  ((7 / sizeof(T)) & 1) ? 0 : 0xFF,
      ((8 / sizeof(T)) & 1) ? 0 : 0xFF,  ((9 / sizeof(T)) & 1) ? 0 : 0xFF,
      ((10 / sizeof(T)) & 1) ? 0 : 0xFF, ((11 / sizeof(T)) & 1) ? 0 : 0xFF,
      ((12 / sizeof(T)) & 1) ? 0 : 0xFF, ((13 / sizeof(T)) & 1) ? 0 : 0xFF,
      ((14 / sizeof(T)) & 1) ? 0 : 0xFF, ((15 / sizeof(T)) & 1) ? 0 : 0xFF,
  };
  const auto vec = BitCast(d, Load(d8, kBytes));
  return IfThenElse(MaskFromVec(vec), b, a);
}

// ------------------------------ OddEvenBlocks
template <typename T, size_t N>
HWY_API Vec128<T, N> OddEvenBlocks(Vec128<T, N> /* odd */, Vec128<T, N> even) {
  return even;
}

// ------------------------------ SwapAdjacentBlocks

template <typename T, size_t N>
HWY_API Vec128<T, N> SwapAdjacentBlocks(Vec128<T, N> v) {
  return v;
}

// ------------------------------ ReverseBlocks

// Single block: no change
template <typename T>
HWY_API Vec128<T> ReverseBlocks(Full128<T> /* tag */, const Vec128<T> v) {
  return v;
}

// ------------------------------ ReorderDemote2To (OddEven)

template <size_t N>
HWY_API Vec128<bfloat16_t, 2 * N> ReorderDemote2To(
    Simd<bfloat16_t, 2 * N, 0> dbf16, Vec128<float, N> a, Vec128<float, N> b) {
  const RebindToUnsigned<decltype(dbf16)> du16;
  const Repartition<uint32_t, decltype(dbf16)> du32;
  const Vec128<uint32_t, N> b_in_even = ShiftRight<16>(BitCast(du32, b));
  return BitCast(dbf16, OddEven(BitCast(du16, a), BitCast(du16, b_in_even)));
}

// ================================================== CRYPTO

#if defined(__ARM_FEATURE_AES) || \
    (HWY_HAVE_RUNTIME_DISPATCH && HWY_ARCH_ARM_A64)

// Per-target flag to prevent generic_ops-inl.h from defining AESRound.
#ifdef HWY_NATIVE_AES
#undef HWY_NATIVE_AES
#else
#define HWY_NATIVE_AES
#endif

HWY_API Vec128<uint8_t> AESRound(Vec128<uint8_t> state,
                                 Vec128<uint8_t> round_key) {
  // NOTE: it is important that AESE and AESMC be consecutive instructions so
  // they can be fused. AESE includes AddRoundKey, which is a different ordering
  // than the AES-NI semantics we adopted, so XOR by 0 and later with the actual
  // round key (the compiler will hopefully optimize this for multiple rounds).
  return Vec128<uint8_t>(vaesmcq_u8(vaeseq_u8(state.raw, vdupq_n_u8(0)))) ^
         round_key;
}

HWY_API Vec128<uint8_t> AESLastRound(Vec128<uint8_t> state,
                                     Vec128<uint8_t> round_key) {
  return Vec128<uint8_t>(vaeseq_u8(state.raw, vdupq_n_u8(0))) ^ round_key;
}

HWY_API Vec128<uint64_t> CLMulLower(Vec128<uint64_t> a, Vec128<uint64_t> b) {
  return Vec128<uint64_t>((uint64x2_t)vmull_p64(GetLane(a), GetLane(b)));
}

HWY_API Vec128<uint64_t> CLMulUpper(Vec128<uint64_t> a, Vec128<uint64_t> b) {
  return Vec128<uint64_t>(
      (uint64x2_t)vmull_high_p64((poly64x2_t)a.raw, (poly64x2_t)b.raw));
}

#endif  // __ARM_FEATURE_AES

// ================================================== MISC

template <size_t N>
HWY_API Vec128<float, N> PromoteTo(Simd<float, N, 0> df32,
                                   const Vec128<bfloat16_t, N> v) {
  const Rebind<uint16_t, decltype(df32)> du16;
  const RebindToSigned<decltype(df32)> di32;
  return BitCast(df32, ShiftLeft<16>(PromoteTo(di32, BitCast(du16, v))));
}

// ------------------------------ Truncations

template <typename From, typename To, HWY_IF_UNSIGNED(From),
          HWY_IF_UNSIGNED(To),
          hwy::EnableIf<(sizeof(To) < sizeof(From))>* = nullptr>
HWY_API Vec128<To, 1> TruncateTo(Simd<To, 1, 0> /* tag */,
                                 const Vec128<From, 1> v) {
  const Repartition<To, DFromV<decltype(v)>> d;
  const auto v1 = BitCast(d, v);
  return Vec128<To, 1>{v1.raw};
}

HWY_API Vec128<uint8_t, 2> TruncateTo(Simd<uint8_t, 2, 0> /* tag */,
                                      const Vec128<uint64_t, 2> v) {
  const Repartition<uint8_t, DFromV<decltype(v)>> d;
  const auto v1 = BitCast(d, v);
  const auto v2 = detail::ConcatEven(v1, v1);
  const auto v3 = detail::ConcatEven(v2, v2);
  const auto v4 = detail::ConcatEven(v3, v3);
  return LowerHalf(LowerHalf(LowerHalf(v4)));
}

HWY_API Vec32<uint16_t> TruncateTo(Simd<uint16_t, 2, 0> /* tag */,
                                   const Vec128<uint64_t, 2> v) {
  const Repartition<uint16_t, DFromV<decltype(v)>> d;
  const auto v1 = BitCast(d, v);
  const auto v2 = detail::ConcatEven(v1, v1);
  const auto v3 = detail::ConcatEven(v2, v2);
  return LowerHalf(LowerHalf(v3));
}

HWY_API Vec64<uint32_t> TruncateTo(Simd<uint32_t, 2, 0> /* tag */,
                                   const Vec128<uint64_t, 2> v) {
  const Repartition<uint32_t, DFromV<decltype(v)>> d;
  const auto v1 = BitCast(d, v);
  const auto v2 = detail::ConcatEven(v1, v1);
  return LowerHalf(v2);
}

template <size_t N, hwy::EnableIf<N >= 2>* = nullptr>
HWY_API Vec128<uint8_t, N> TruncateTo(Simd<uint8_t, N, 0> /* tag */,
                                      const Vec128<uint32_t, N> v) {
  const Repartition<uint8_t, DFromV<decltype(v)>> d;
  const auto v1 = BitCast(d, v);
  const auto v2 = detail::ConcatEven(v1, v1);
  const auto v3 = detail::ConcatEven(v2, v2);
  return LowerHalf(LowerHalf(v3));
}

template <size_t N, hwy::EnableIf<N >= 2>* = nullptr>
HWY_API Vec128<uint16_t, N> TruncateTo(Simd<uint16_t, N, 0> /* tag */,
                                       const Vec128<uint32_t, N> v) {
  const Repartition<uint16_t, DFromV<decltype(v)>> d;
  const auto v1 = BitCast(d, v);
  const auto v2 = detail::ConcatEven(v1, v1);
  return LowerHalf(v2);
}

template <size_t N, hwy::EnableIf<N >= 2>* = nullptr>
HWY_API Vec128<uint8_t, N> TruncateTo(Simd<uint8_t, N, 0> /* tag */,
                                      const Vec128<uint16_t, N> v) {
  const Repartition<uint8_t, DFromV<decltype(v)>> d;
  const auto v1 = BitCast(d, v);
  const auto v2 = detail::ConcatEven(v1, v1);
  return LowerHalf(v2);
}

// ------------------------------ MulEven (ConcatEven)

// Multiplies even lanes (0, 2 ..) and places the double-wide result into
// even and the upper half into its odd neighbor lane.
HWY_API Vec128<int64_t> MulEven(Vec128<int32_t> a, Vec128<int32_t> b) {
  const Full128<int32_t> d;
  int32x4_t a_packed = ConcatEven(d, a, a).raw;
  int32x4_t b_packed = ConcatEven(d, b, b).raw;
  return Vec128<int64_t>(
      vmull_s32(vget_low_s32(a_packed), vget_low_s32(b_packed)));
}
HWY_API Vec128<uint64_t> MulEven(Vec128<uint32_t> a, Vec128<uint32_t> b) {
  const Full128<uint32_t> d;
  uint32x4_t a_packed = ConcatEven(d, a, a).raw;
  uint32x4_t b_packed = ConcatEven(d, b, b).raw;
  return Vec128<uint64_t>(
      vmull_u32(vget_low_u32(a_packed), vget_low_u32(b_packed)));
}

template <size_t N>
HWY_API Vec128<int64_t, (N + 1) / 2> MulEven(const Vec128<int32_t, N> a,
                                             const Vec128<int32_t, N> b) {
  const DFromV<decltype(a)> d;
  int32x2_t a_packed = ConcatEven(d, a, a).raw;
  int32x2_t b_packed = ConcatEven(d, b, b).raw;
  return Vec128<int64_t, (N + 1) / 2>(
      vget_low_s64(vmull_s32(a_packed, b_packed)));
}
template <size_t N>
HWY_API Vec128<uint64_t, (N + 1) / 2> MulEven(const Vec128<uint32_t, N> a,
                                              const Vec128<uint32_t, N> b) {
  const DFromV<decltype(a)> d;
  uint32x2_t a_packed = ConcatEven(d, a, a).raw;
  uint32x2_t b_packed = ConcatEven(d, b, b).raw;
  return Vec128<uint64_t, (N + 1) / 2>(
      vget_low_u64(vmull_u32(a_packed, b_packed)));
}

HWY_INLINE Vec128<uint64_t> MulEven(Vec128<uint64_t> a, Vec128<uint64_t> b) {
  uint64_t hi;
  uint64_t lo = Mul128(vgetq_lane_u64(a.raw, 0), vgetq_lane_u64(b.raw, 0), &hi);
  return Vec128<uint64_t>(vsetq_lane_u64(hi, vdupq_n_u64(lo), 1));
}

HWY_INLINE Vec128<uint64_t> MulOdd(Vec128<uint64_t> a, Vec128<uint64_t> b) {
  uint64_t hi;
  uint64_t lo = Mul128(vgetq_lane_u64(a.raw, 1), vgetq_lane_u64(b.raw, 1), &hi);
  return Vec128<uint64_t>(vsetq_lane_u64(hi, vdupq_n_u64(lo), 1));
}

// ------------------------------ TableLookupBytes (Combine, LowerHalf)

// Both full
template <typename T, typename TI>
HWY_API Vec128<TI> TableLookupBytes(const Vec128<T> bytes,
                                    const Vec128<TI> from) {
  const Full128<TI> d;
  const Repartition<uint8_t, decltype(d)> d8;
#if HWY_ARCH_ARM_A64
  return BitCast(d, Vec128<uint8_t>(vqtbl1q_u8(BitCast(d8, bytes).raw,
                                               BitCast(d8, from).raw)));
#else
  uint8x16_t table0 = BitCast(d8, bytes).raw;
  uint8x8x2_t table;
  table.val[0] = vget_low_u8(table0);
  table.val[1] = vget_high_u8(table0);
  uint8x16_t idx = BitCast(d8, from).raw;
  uint8x8_t low = vtbl2_u8(table, vget_low_u8(idx));
  uint8x8_t hi = vtbl2_u8(table, vget_high_u8(idx));
  return BitCast(d, Vec128<uint8_t>(vcombine_u8(low, hi)));
#endif
}

// Partial index vector
template <typename T, typename TI, size_t NI, HWY_IF_LE64(TI, NI)>
HWY_API Vec128<TI, NI> TableLookupBytes(const Vec128<T> bytes,
                                        const Vec128<TI, NI> from) {
  const Full128<TI> d_full;
  const Vec64<TI> from64(from.raw);
  const auto idx_full = Combine(d_full, from64, from64);
  const auto out_full = TableLookupBytes(bytes, idx_full);
  return Vec128<TI, NI>(LowerHalf(Half<decltype(d_full)>(), out_full).raw);
}

// Partial table vector
template <typename T, size_t N, typename TI, HWY_IF_LE64(T, N)>
HWY_API Vec128<TI> TableLookupBytes(const Vec128<T, N> bytes,
                                    const Vec128<TI> from) {
  const Full128<T> d_full;
  return TableLookupBytes(Combine(d_full, bytes, bytes), from);
}

// Partial both
template <typename T, size_t N, typename TI, size_t NI, HWY_IF_LE64(T, N),
          HWY_IF_LE64(TI, NI)>
HWY_API VFromD<Repartition<T, Simd<TI, NI, 0>>> TableLookupBytes(
    Vec128<T, N> bytes, Vec128<TI, NI> from) {
  const Simd<T, N, 0> d;
  const Simd<TI, NI, 0> d_idx;
  const Repartition<uint8_t, decltype(d_idx)> d_idx8;
  // uint8x8
  const auto bytes8 = BitCast(Repartition<uint8_t, decltype(d)>(), bytes);
  const auto from8 = BitCast(d_idx8, from);
  const VFromD<decltype(d_idx8)> v8(vtbl1_u8(bytes8.raw, from8.raw));
  return BitCast(d_idx, v8);
}

// For all vector widths; ARM anyway zeroes if >= 0x10.
template <class V, class VI>
HWY_API VI TableLookupBytesOr0(const V bytes, const VI from) {
  return TableLookupBytes(bytes, from);
}

// ------------------------------ Scatter (Store)

template <typename T, size_t N, typename Offset, HWY_IF_LE128(T, N)>
HWY_API void ScatterOffset(Vec128<T, N> v, Simd<T, N, 0> d,
                           T* HWY_RESTRICT base,
                           const Vec128<Offset, N> offset) {
  static_assert(sizeof(T) == sizeof(Offset), "Must match for portability");

  alignas(16) T lanes[N];
  Store(v, d, lanes);

  alignas(16) Offset offset_lanes[N];
  Store(offset, Rebind<Offset, decltype(d)>(), offset_lanes);

  uint8_t* base_bytes = reinterpret_cast<uint8_t*>(base);
  for (size_t i = 0; i < N; ++i) {
    CopyBytes<sizeof(T)>(&lanes[i], base_bytes + offset_lanes[i]);
  }
}

template <typename T, size_t N, typename Index, HWY_IF_LE128(T, N)>
HWY_API void ScatterIndex(Vec128<T, N> v, Simd<T, N, 0> d, T* HWY_RESTRICT base,
                          const Vec128<Index, N> index) {
  static_assert(sizeof(T) == sizeof(Index), "Must match for portability");

  alignas(16) T lanes[N];
  Store(v, d, lanes);

  alignas(16) Index index_lanes[N];
  Store(index, Rebind<Index, decltype(d)>(), index_lanes);

  for (size_t i = 0; i < N; ++i) {
    base[index_lanes[i]] = lanes[i];
  }
}

// ------------------------------ Gather (Load/Store)

template <typename T, size_t N, typename Offset>
HWY_API Vec128<T, N> GatherOffset(const Simd<T, N, 0> d,
                                  const T* HWY_RESTRICT base,
                                  const Vec128<Offset, N> offset) {
  static_assert(sizeof(T) == sizeof(Offset), "Must match for portability");

  alignas(16) Offset offset_lanes[N];
  Store(offset, Rebind<Offset, decltype(d)>(), offset_lanes);

  alignas(16) T lanes[N];
  const uint8_t* base_bytes = reinterpret_cast<const uint8_t*>(base);
  for (size_t i = 0; i < N; ++i) {
    CopyBytes<sizeof(T)>(base_bytes + offset_lanes[i], &lanes[i]);
  }
  return Load(d, lanes);
}

template <typename T, size_t N, typename Index>
HWY_API Vec128<T, N> GatherIndex(const Simd<T, N, 0> d,
                                 const T* HWY_RESTRICT base,
                                 const Vec128<Index, N> index) {
  static_assert(sizeof(T) == sizeof(Index), "Must match for portability");

  alignas(16) Index index_lanes[N];
  Store(index, Rebind<Index, decltype(d)>(), index_lanes);

  alignas(16) T lanes[N];
  for (size_t i = 0; i < N; ++i) {
    lanes[i] = base[index_lanes[i]];
  }
  return Load(d, lanes);
}

// ------------------------------ Reductions

namespace detail {

// N=1 for any T: no-op
template <typename T>
HWY_INLINE Vec128<T, 1> SumOfLanes(const Vec128<T, 1> v) {
  return v;
}
template <typename T>
HWY_INLINE Vec128<T, 1> MinOfLanes(hwy::SizeTag<sizeof(T)> /* tag */,
                                   const Vec128<T, 1> v) {
  return v;
}
template <typename T>
HWY_INLINE Vec128<T, 1> MaxOfLanes(hwy::SizeTag<sizeof(T)> /* tag */,
                                   const Vec128<T, 1> v) {
  return v;
}

// u32/i32/f32: N=2
template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_INLINE Vec128<T, 2> SumOfLanes(const Vec128<T, 2> v10) {
  return v10 + Shuffle2301(v10);
}
template <typename T>
HWY_INLINE Vec128<T, 2> MinOfLanes(hwy::SizeTag<4> /* tag */,
                                   const Vec128<T, 2> v10) {
  return Min(v10, Shuffle2301(v10));
}
template <typename T>
HWY_INLINE Vec128<T, 2> MaxOfLanes(hwy::SizeTag<4> /* tag */,
                                   const Vec128<T, 2> v10) {
  return Max(v10, Shuffle2301(v10));
}

// full vectors
#if HWY_ARCH_ARM_A64
HWY_INLINE Vec128<uint32_t> SumOfLanes(const Vec128<uint32_t> v) {
  return Vec128<uint32_t>(vdupq_n_u32(vaddvq_u32(v.raw)));
}
HWY_INLINE Vec128<int32_t> SumOfLanes(const Vec128<int32_t> v) {
  return Vec128<int32_t>(vdupq_n_s32(vaddvq_s32(v.raw)));
}
HWY_INLINE Vec128<float> SumOfLanes(const Vec128<float> v) {
  return Vec128<float>(vdupq_n_f32(vaddvq_f32(v.raw)));
}
HWY_INLINE Vec128<uint64_t> SumOfLanes(const Vec128<uint64_t> v) {
  return Vec128<uint64_t>(vdupq_n_u64(vaddvq_u64(v.raw)));
}
HWY_INLINE Vec128<int64_t> SumOfLanes(const Vec128<int64_t> v) {
  return Vec128<int64_t>(vdupq_n_s64(vaddvq_s64(v.raw)));
}
HWY_INLINE Vec128<double> SumOfLanes(const Vec128<double> v) {
  return Vec128<double>(vdupq_n_f64(vaddvq_f64(v.raw)));
}
#else
// ARMv7 version for everything except doubles.
HWY_INLINE Vec128<uint32_t> SumOfLanes(const Vec128<uint32_t> v) {
  uint32x4x2_t v0 = vuzpq_u32(v.raw, v.raw);
  uint32x4_t c0 = vaddq_u32(v0.val[0], v0.val[1]);
  uint32x4x2_t v1 = vuzpq_u32(c0, c0);
  return Vec128<uint32_t>(vaddq_u32(v1.val[0], v1.val[1]));
}
HWY_INLINE Vec128<int32_t> SumOfLanes(const Vec128<int32_t> v) {
  int32x4x2_t v0 = vuzpq_s32(v.raw, v.raw);
  int32x4_t c0 = vaddq_s32(v0.val[0], v0.val[1]);
  int32x4x2_t v1 = vuzpq_s32(c0, c0);
  return Vec128<int32_t>(vaddq_s32(v1.val[0], v1.val[1]));
}
HWY_INLINE Vec128<float> SumOfLanes(const Vec128<float> v) {
  float32x4x2_t v0 = vuzpq_f32(v.raw, v.raw);
  float32x4_t c0 = vaddq_f32(v0.val[0], v0.val[1]);
  float32x4x2_t v1 = vuzpq_f32(c0, c0);
  return Vec128<float>(vaddq_f32(v1.val[0], v1.val[1]));
}
HWY_INLINE Vec128<uint64_t> SumOfLanes(const Vec128<uint64_t> v) {
  return v + Shuffle01(v);
}
HWY_INLINE Vec128<int64_t> SumOfLanes(const Vec128<int64_t> v) {
  return v + Shuffle01(v);
}
#endif

template <typename T>
HWY_INLINE Vec128<T> MinOfLanes(hwy::SizeTag<4> /* tag */,
                                const Vec128<T> v3210) {
  const Vec128<T> v1032 = Shuffle1032(v3210);
  const Vec128<T> v31_20_31_20 = Min(v3210, v1032);
  const Vec128<T> v20_31_20_31 = Shuffle0321(v31_20_31_20);
  return Min(v20_31_20_31, v31_20_31_20);
}
template <typename T>
HWY_INLINE Vec128<T> MaxOfLanes(hwy::SizeTag<4> /* tag */,
                                const Vec128<T> v3210) {
  const Vec128<T> v1032 = Shuffle1032(v3210);
  const Vec128<T> v31_20_31_20 = Max(v3210, v1032);
  const Vec128<T> v20_31_20_31 = Shuffle0321(v31_20_31_20);
  return Max(v20_31_20_31, v31_20_31_20);
}

// For u64/i64[/f64].
template <typename T>
HWY_INLINE Vec128<T> MinOfLanes(hwy::SizeTag<8> /* tag */,
                                const Vec128<T> v10) {
  const Vec128<T> v01 = Shuffle01(v10);
  return Min(v10, v01);
}
template <typename T>
HWY_INLINE Vec128<T> MaxOfLanes(hwy::SizeTag<8> /* tag */,
                                const Vec128<T> v10) {
  const Vec128<T> v01 = Shuffle01(v10);
  return Max(v10, v01);
}

// u16/i16
template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 2), HWY_IF_GE32(T, N)>
HWY_API Vec128<T, N> MinOfLanes(hwy::SizeTag<2> /* tag */, Vec128<T, N> v) {
  const Repartition<int32_t, Simd<T, N, 0>> d32;
  const auto even = And(BitCast(d32, v), Set(d32, 0xFFFF));
  const auto odd = ShiftRight<16>(BitCast(d32, v));
  const auto min = MinOfLanes(d32, Min(even, odd));
  // Also broadcast into odd lanes.
  return BitCast(Simd<T, N, 0>(), Or(min, ShiftLeft<16>(min)));
}
template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 2), HWY_IF_GE32(T, N)>
HWY_API Vec128<T, N> MaxOfLanes(hwy::SizeTag<2> /* tag */, Vec128<T, N> v) {
  const Repartition<int32_t, Simd<T, N, 0>> d32;
  const auto even = And(BitCast(d32, v), Set(d32, 0xFFFF));
  const auto odd = ShiftRight<16>(BitCast(d32, v));
  const auto min = MaxOfLanes(d32, Max(even, odd));
  // Also broadcast into odd lanes.
  return BitCast(Simd<T, N, 0>(), Or(min, ShiftLeft<16>(min)));
}

}  // namespace detail

template <typename T, size_t N>
HWY_API Vec128<T, N> SumOfLanes(Simd<T, N, 0> /* tag */, const Vec128<T, N> v) {
  return detail::SumOfLanes(v);
}
template <typename T, size_t N>
HWY_API Vec128<T, N> MinOfLanes(Simd<T, N, 0> /* tag */, const Vec128<T, N> v) {
  return detail::MinOfLanes(hwy::SizeTag<sizeof(T)>(), v);
}
template <typename T, size_t N>
HWY_API Vec128<T, N> MaxOfLanes(Simd<T, N, 0> /* tag */, const Vec128<T, N> v) {
  return detail::MaxOfLanes(hwy::SizeTag<sizeof(T)>(), v);
}

// ------------------------------ LoadMaskBits (TestBit)

namespace detail {

// Helper function to set 64 bits and potentially return a smaller vector. The
// overload is required to call the q vs non-q intrinsics. Note that 8-bit
// LoadMaskBits only requires 16 bits, but 64 avoids casting.
template <typename T, size_t N, HWY_IF_LE64(T, N)>
HWY_INLINE Vec128<T, N> Set64(Simd<T, N, 0> /* tag */, uint64_t mask_bits) {
  const auto v64 = Vec64<uint64_t>(vdup_n_u64(mask_bits));
  return Vec128<T, N>(BitCast(Full64<T>(), v64).raw);
}
template <typename T>
HWY_INLINE Vec128<T> Set64(Full128<T> d, uint64_t mask_bits) {
  return BitCast(d, Vec128<uint64_t>(vdupq_n_u64(mask_bits)));
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 1)>
HWY_INLINE Mask128<T, N> LoadMaskBits(Simd<T, N, 0> d, uint64_t mask_bits) {
  const RebindToUnsigned<decltype(d)> du;
  // Easier than Set(), which would require an >8-bit type, which would not
  // compile for T=uint8_t, N=1.
  const auto vmask_bits = Set64(du, mask_bits);

  // Replicate bytes 8x such that each byte contains the bit that governs it.
  alignas(16) constexpr uint8_t kRep8[16] = {0, 0, 0, 0, 0, 0, 0, 0,
                                             1, 1, 1, 1, 1, 1, 1, 1};
  const auto rep8 = TableLookupBytes(vmask_bits, Load(du, kRep8));

  alignas(16) constexpr uint8_t kBit[16] = {1, 2, 4, 8, 16, 32, 64, 128,
                                            1, 2, 4, 8, 16, 32, 64, 128};
  return RebindMask(d, TestBit(rep8, LoadDup128(du, kBit)));
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 2)>
HWY_INLINE Mask128<T, N> LoadMaskBits(Simd<T, N, 0> d, uint64_t mask_bits) {
  const RebindToUnsigned<decltype(d)> du;
  alignas(16) constexpr uint16_t kBit[8] = {1, 2, 4, 8, 16, 32, 64, 128};
  const auto vmask_bits = Set(du, static_cast<uint16_t>(mask_bits));
  return RebindMask(d, TestBit(vmask_bits, Load(du, kBit)));
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 4)>
HWY_INLINE Mask128<T, N> LoadMaskBits(Simd<T, N, 0> d, uint64_t mask_bits) {
  const RebindToUnsigned<decltype(d)> du;
  alignas(16) constexpr uint32_t kBit[8] = {1, 2, 4, 8};
  const auto vmask_bits = Set(du, static_cast<uint32_t>(mask_bits));
  return RebindMask(d, TestBit(vmask_bits, Load(du, kBit)));
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 8)>
HWY_INLINE Mask128<T, N> LoadMaskBits(Simd<T, N, 0> d, uint64_t mask_bits) {
  const RebindToUnsigned<decltype(d)> du;
  alignas(16) constexpr uint64_t kBit[8] = {1, 2};
  return RebindMask(d, TestBit(Set(du, mask_bits), Load(du, kBit)));
}

}  // namespace detail

// `p` points to at least 8 readable bytes, not all of which need be valid.
template <typename T, size_t N, HWY_IF_LE128(T, N)>
HWY_API Mask128<T, N> LoadMaskBits(Simd<T, N, 0> d,
                                   const uint8_t* HWY_RESTRICT bits) {
  uint64_t mask_bits = 0;
  CopyBytes<(N + 7) / 8>(bits, &mask_bits);
  return detail::LoadMaskBits(d, mask_bits);
}

// ------------------------------ Mask

namespace detail {

// Returns mask[i]? 0xF : 0 in each nibble. This is more efficient than
// BitsFromMask for use in (partial) CountTrue, FindFirstTrue and AllFalse.
template <typename T>
HWY_INLINE uint64_t NibblesFromMask(const Full128<T> d, Mask128<T> mask) {
  const Full128<uint16_t> du16;
  const Vec128<uint16_t> vu16 = BitCast(du16, VecFromMask(d, mask));
  const Vec64<uint8_t> nib(vshrn_n_u16(vu16.raw, 4));
  return GetLane(BitCast(Full64<uint64_t>(), nib));
}

template <typename T>
HWY_INLINE uint64_t NibblesFromMask(const Full64<T> d, Mask64<T> mask) {
  // There is no vshrn_n_u16 for uint16x4, so zero-extend.
  const Twice<decltype(d)> d2;
  const Vec128<T> v128 = ZeroExtendVector(d2, VecFromMask(d, mask));
  // No need to mask, upper half is zero thanks to ZeroExtendVector.
  return NibblesFromMask(d2, MaskFromVec(v128));
}

template <typename T, size_t N, HWY_IF_LE32(T, N)>
HWY_INLINE uint64_t NibblesFromMask(Simd<T, N, 0> /*d*/, Mask128<T, N> mask) {
  const Mask64<T> mask64(mask.raw);
  const uint64_t nib = NibblesFromMask(Full64<T>(), mask64);
  // Clear nibbles from upper half of 64-bits
  constexpr size_t kBytes = sizeof(T) * N;
  return nib & ((1ull << (kBytes * 4)) - 1);
}

template <typename T>
HWY_INLINE uint64_t BitsFromMask(hwy::SizeTag<1> /*tag*/,
                                 const Mask128<T> mask) {
  alignas(16) constexpr uint8_t kSliceLanes[16] = {
      1, 2, 4, 8, 0x10, 0x20, 0x40, 0x80, 1, 2, 4, 8, 0x10, 0x20, 0x40, 0x80,
  };
  const Full128<uint8_t> du;
  const Vec128<uint8_t> values =
      BitCast(du, VecFromMask(Full128<T>(), mask)) & Load(du, kSliceLanes);

#if HWY_ARCH_ARM_A64
  // Can't vaddv - we need two separate bytes (16 bits).
  const uint8x8_t x2 = vget_low_u8(vpaddq_u8(values.raw, values.raw));
  const uint8x8_t x4 = vpadd_u8(x2, x2);
  const uint8x8_t x8 = vpadd_u8(x4, x4);
  return vget_lane_u64(vreinterpret_u64_u8(x8), 0);
#else
  // Don't have vpaddq, so keep doubling lane size.
  const uint16x8_t x2 = vpaddlq_u8(values.raw);
  const uint32x4_t x4 = vpaddlq_u16(x2);
  const uint64x2_t x8 = vpaddlq_u32(x4);
  return (vgetq_lane_u64(x8, 1) << 8) | vgetq_lane_u64(x8, 0);
#endif
}

template <typename T, size_t N, HWY_IF_LE64(T, N)>
HWY_INLINE uint64_t BitsFromMask(hwy::SizeTag<1> /*tag*/,
                                 const Mask128<T, N> mask) {
  // Upper lanes of partial loads are undefined. OnlyActive will fix this if
  // we load all kSliceLanes so the upper lanes do not pollute the valid bits.
  alignas(8) constexpr uint8_t kSliceLanes[8] = {1,    2,    4,    8,
                                                 0x10, 0x20, 0x40, 0x80};
  const Simd<T, N, 0> d;
  const RebindToUnsigned<decltype(d)> du;
  const Vec128<uint8_t, N> slice(Load(Full64<uint8_t>(), kSliceLanes).raw);
  const Vec128<uint8_t, N> values = BitCast(du, VecFromMask(d, mask)) & slice;

#if HWY_ARCH_ARM_A64
  return vaddv_u8(values.raw);
#else
  const uint16x4_t x2 = vpaddl_u8(values.raw);
  const uint32x2_t x4 = vpaddl_u16(x2);
  const uint64x1_t x8 = vpaddl_u32(x4);
  return vget_lane_u64(x8, 0);
#endif
}

template <typename T>
HWY_INLINE uint64_t BitsFromMask(hwy::SizeTag<2> /*tag*/,
                                 const Mask128<T> mask) {
  alignas(16) constexpr uint16_t kSliceLanes[8] = {1,    2,    4,    8,
                                                   0x10, 0x20, 0x40, 0x80};
  const Full128<T> d;
  const Full128<uint16_t> du;
  const Vec128<uint16_t> values =
      BitCast(du, VecFromMask(d, mask)) & Load(du, kSliceLanes);
#if HWY_ARCH_ARM_A64
  return vaddvq_u16(values.raw);
#else
  const uint32x4_t x2 = vpaddlq_u16(values.raw);
  const uint64x2_t x4 = vpaddlq_u32(x2);
  return vgetq_lane_u64(x4, 0) + vgetq_lane_u64(x4, 1);
#endif
}

template <typename T, size_t N, HWY_IF_LE64(T, N)>
HWY_INLINE uint64_t BitsFromMask(hwy::SizeTag<2> /*tag*/,
                                 const Mask128<T, N> mask) {
  // Upper lanes of partial loads are undefined. OnlyActive will fix this if
  // we load all kSliceLanes so the upper lanes do not pollute the valid bits.
  alignas(8) constexpr uint16_t kSliceLanes[4] = {1, 2, 4, 8};
  const Simd<T, N, 0> d;
  const RebindToUnsigned<decltype(d)> du;
  const Vec128<uint16_t, N> slice(Load(Full64<uint16_t>(), kSliceLanes).raw);
  const Vec128<uint16_t, N> values = BitCast(du, VecFromMask(d, mask)) & slice;
#if HWY_ARCH_ARM_A64
  return vaddv_u16(values.raw);
#else
  const uint32x2_t x2 = vpaddl_u16(values.raw);
  const uint64x1_t x4 = vpaddl_u32(x2);
  return vget_lane_u64(x4, 0);
#endif
}

template <typename T>
HWY_INLINE uint64_t BitsFromMask(hwy::SizeTag<4> /*tag*/,
                                 const Mask128<T> mask) {
  alignas(16) constexpr uint32_t kSliceLanes[4] = {1, 2, 4, 8};
  const Full128<T> d;
  const Full128<uint32_t> du;
  const Vec128<uint32_t> values =
      BitCast(du, VecFromMask(d, mask)) & Load(du, kSliceLanes);
#if HWY_ARCH_ARM_A64
  return vaddvq_u32(values.raw);
#else
  const uint64x2_t x2 = vpaddlq_u32(values.raw);
  return vgetq_lane_u64(x2, 0) + vgetq_lane_u64(x2, 1);
#endif
}

template <typename T, size_t N, HWY_IF_LE64(T, N)>
HWY_INLINE uint64_t BitsFromMask(hwy::SizeTag<4> /*tag*/,
                                 const Mask128<T, N> mask) {
  // Upper lanes of partial loads are undefined. OnlyActive will fix this if
  // we load all kSliceLanes so the upper lanes do not pollute the valid bits.
  alignas(8) constexpr uint32_t kSliceLanes[2] = {1, 2};
  const Simd<T, N, 0> d;
  const RebindToUnsigned<decltype(d)> du;
  const Vec128<uint32_t, N> slice(Load(Full64<uint32_t>(), kSliceLanes).raw);
  const Vec128<uint32_t, N> values = BitCast(du, VecFromMask(d, mask)) & slice;
#if HWY_ARCH_ARM_A64
  return vaddv_u32(values.raw);
#else
  const uint64x1_t x2 = vpaddl_u32(values.raw);
  return vget_lane_u64(x2, 0);
#endif
}

template <typename T>
HWY_INLINE uint64_t BitsFromMask(hwy::SizeTag<8> /*tag*/, const Mask128<T> m) {
  alignas(16) constexpr uint64_t kSliceLanes[2] = {1, 2};
  const Full128<T> d;
  const Full128<uint64_t> du;
  const Vec128<uint64_t> values =
      BitCast(du, VecFromMask(d, m)) & Load(du, kSliceLanes);
#if HWY_ARCH_ARM_A64
  return vaddvq_u64(values.raw);
#else
  return vgetq_lane_u64(values.raw, 0) + vgetq_lane_u64(values.raw, 1);
#endif
}

template <typename T>
HWY_INLINE uint64_t BitsFromMask(hwy::SizeTag<8> /*tag*/,
                                 const Mask128<T, 1> m) {
  const Full64<T> d;
  const Full64<uint64_t> du;
  const Vec64<uint64_t> values = BitCast(du, VecFromMask(d, m)) & Set(du, 1);
  return vget_lane_u64(values.raw, 0);
}

// Returns the lowest N for the BitsFromMask result.
template <typename T, size_t N>
constexpr uint64_t OnlyActive(uint64_t bits) {
  return ((N * sizeof(T)) >= 8) ? bits : (bits & ((1ull << N) - 1));
}

template <typename T, size_t N>
HWY_INLINE uint64_t BitsFromMask(const Mask128<T, N> mask) {
  return OnlyActive<T, N>(BitsFromMask(hwy::SizeTag<sizeof(T)>(), mask));
}

// Returns number of lanes whose mask is set.
//
// Masks are either FF..FF or 0. Unfortunately there is no reduce-sub op
// ("vsubv"). ANDing with 1 would work but requires a constant. Negating also
// changes each lane to 1 (if mask set) or 0.
// NOTE: PopCount also operates on vectors, so we still have to do horizontal
// sums separately. We specialize CountTrue for full vectors (negating instead
// of PopCount because it avoids an extra shift), and use PopCount of
// NibblesFromMask for partial vectors.

template <typename T>
HWY_INLINE size_t CountTrue(hwy::SizeTag<1> /*tag*/, const Mask128<T> mask) {
  const Full128<int8_t> di;
  const int8x16_t ones =
      vnegq_s8(BitCast(di, VecFromMask(Full128<T>(), mask)).raw);

#if HWY_ARCH_ARM_A64
  return static_cast<size_t>(vaddvq_s8(ones));
#else
  const int16x8_t x2 = vpaddlq_s8(ones);
  const int32x4_t x4 = vpaddlq_s16(x2);
  const int64x2_t x8 = vpaddlq_s32(x4);
  return static_cast<size_t>(vgetq_lane_s64(x8, 0) + vgetq_lane_s64(x8, 1));
#endif
}
template <typename T>
HWY_INLINE size_t CountTrue(hwy::SizeTag<2> /*tag*/, const Mask128<T> mask) {
  const Full128<int16_t> di;
  const int16x8_t ones =
      vnegq_s16(BitCast(di, VecFromMask(Full128<T>(), mask)).raw);

#if HWY_ARCH_ARM_A64
  return static_cast<size_t>(vaddvq_s16(ones));
#else
  const int32x4_t x2 = vpaddlq_s16(ones);
  const int64x2_t x4 = vpaddlq_s32(x2);
  return static_cast<size_t>(vgetq_lane_s64(x4, 0) + vgetq_lane_s64(x4, 1));
#endif
}

template <typename T>
HWY_INLINE size_t CountTrue(hwy::SizeTag<4> /*tag*/, const Mask128<T> mask) {
  const Full128<int32_t> di;
  const int32x4_t ones =
      vnegq_s32(BitCast(di, VecFromMask(Full128<T>(), mask)).raw);

#if HWY_ARCH_ARM_A64
  return static_cast<size_t>(vaddvq_s32(ones));
#else
  const int64x2_t x2 = vpaddlq_s32(ones);
  return static_cast<size_t>(vgetq_lane_s64(x2, 0) + vgetq_lane_s64(x2, 1));
#endif
}

template <typename T>
HWY_INLINE size_t CountTrue(hwy::SizeTag<8> /*tag*/, const Mask128<T> mask) {
#if HWY_ARCH_ARM_A64
  const Full128<int64_t> di;
  const int64x2_t ones =
      vnegq_s64(BitCast(di, VecFromMask(Full128<T>(), mask)).raw);
  return static_cast<size_t>(vaddvq_s64(ones));
#else
  const Full128<uint64_t> du;
  const auto mask_u = VecFromMask(du, RebindMask(du, mask));
  const uint64x2_t ones = vshrq_n_u64(mask_u.raw, 63);
  return static_cast<size_t>(vgetq_lane_u64(ones, 0) + vgetq_lane_u64(ones, 1));
#endif
}

}  // namespace detail

// Full
template <typename T>
HWY_API size_t CountTrue(Full128<T> /* tag */, const Mask128<T> mask) {
  return detail::CountTrue(hwy::SizeTag<sizeof(T)>(), mask);
}

// Partial
template <typename T, size_t N, HWY_IF_LE64(T, N)>
HWY_API size_t CountTrue(Simd<T, N, 0> d, const Mask128<T, N> mask) {
  constexpr int kDiv = 4 * sizeof(T);
  return PopCount(detail::NibblesFromMask(d, mask)) / kDiv;
}
template <typename T, size_t N>
HWY_API intptr_t FindFirstTrue(const Simd<T, N, 0> d,
                               const Mask128<T, N> mask) {
  const uint64_t nib = detail::NibblesFromMask(d, mask);
  if (nib == 0) return -1;
  constexpr int kDiv = 4 * sizeof(T);
  return static_cast<intptr_t>(Num0BitsBelowLS1Bit_Nonzero64(nib) / kDiv);
}

// `p` points to at least 8 writable bytes.
template <typename T, size_t N>
HWY_API size_t StoreMaskBits(Simd<T, N, 0> /* tag */, const Mask128<T, N> mask,
                             uint8_t* bits) {
  const uint64_t mask_bits = detail::BitsFromMask(mask);
  const size_t kNumBytes = (N + 7) / 8;
  CopyBytes<kNumBytes>(&mask_bits, bits);
  return kNumBytes;
}

template <typename T, size_t N>
HWY_API bool AllFalse(const Simd<T, N, 0> d, const Mask128<T, N> m) {
  return detail::NibblesFromMask(d, m) == 0;
}

// Full
template <typename T>
HWY_API bool AllTrue(const Full128<T> d, const Mask128<T> m) {
  return detail::NibblesFromMask(d, m) == ~0ull;
}
// Partial
template <typename T, size_t N, HWY_IF_LE64(T, N)>
HWY_API bool AllTrue(const Simd<T, N, 0> d, const Mask128<T, N> m) {
  constexpr size_t kBytes = sizeof(T) * N;
  return detail::NibblesFromMask(d, m) == (1ull << (kBytes * 4)) - 1;
}

// ------------------------------ Compress

template <typename T>
struct CompressIsPartition {
  enum { value = 1 };
};

namespace detail {

// Load 8 bytes, replicate into upper half so ZipLower can use the lower half.
HWY_INLINE Vec128<uint8_t> Load8Bytes(Full128<uint8_t> /*d*/,
                                      const uint8_t* bytes) {
  return Vec128<uint8_t>(vreinterpretq_u8_u64(
      vld1q_dup_u64(reinterpret_cast<const uint64_t*>(bytes))));
}

// Load 8 bytes and return half-reg with N <= 8 bytes.
template <size_t N, HWY_IF_LE64(uint8_t, N)>
HWY_INLINE Vec128<uint8_t, N> Load8Bytes(Simd<uint8_t, N, 0> d,
                                         const uint8_t* bytes) {
  return Load(d, bytes);
}

template <typename T, size_t N>
HWY_INLINE Vec128<T, N> IdxFromBits(hwy::SizeTag<2> /*tag*/,
                                    const uint64_t mask_bits) {
  HWY_DASSERT(mask_bits < 256);
  const Simd<T, N, 0> d;
  const Repartition<uint8_t, decltype(d)> d8;
  const Simd<uint16_t, N, 0> du;

  // ARM does not provide an equivalent of AVX2 permutevar, so we need byte
  // indices for VTBL (one vector's worth for each of 256 combinations of
  // 8 mask bits). Loading them directly would require 4 KiB. We can instead
  // store lane indices and convert to byte indices (2*lane + 0..1), with the
  // doubling baked into the table. AVX2 Compress32 stores eight 4-bit lane
  // indices (total 1 KiB), broadcasts them into each 32-bit lane and shifts.
  // Here, 16-bit lanes are too narrow to hold all bits, and unpacking nibbles
  // is likely more costly than the higher cache footprint from storing bytes.
  alignas(16) constexpr uint8_t table[256 * 8] = {
      // PrintCompress16x8Tables
      0,  2,  4,  6,  8,  10, 12, 14, /**/ 0, 2,  4,  6,  8,  10, 12, 14,  //
      2,  0,  4,  6,  8,  10, 12, 14, /**/ 0, 2,  4,  6,  8,  10, 12, 14,  //
      4,  0,  2,  6,  8,  10, 12, 14, /**/ 0, 4,  2,  6,  8,  10, 12, 14,  //
      2,  4,  0,  6,  8,  10, 12, 14, /**/ 0, 2,  4,  6,  8,  10, 12, 14,  //
      6,  0,  2,  4,  8,  10, 12, 14, /**/ 0, 6,  2,  4,  8,  10, 12, 14,  //
      2,  6,  0,  4,  8,  10, 12, 14, /**/ 0, 2,  6,  4,  8,  10, 12, 14,  //
      4,  6,  0,  2,  8,  10, 12, 14, /**/ 0, 4,  6,  2,  8,  10, 12, 14,  //
      2,  4,  6,  0,  8,  10, 12, 14, /**/ 0, 2,  4,  6,  8,  10, 12, 14,  //
      8,  0,  2,  4,  6,  10, 12, 14, /**/ 0, 8,  2,  4,  6,  10, 12, 14,  //
      2,  8,  0,  4,  6,  10, 12, 14, /**/ 0, 2,  8,  4,  6,  10, 12, 14,  //
      4,  8,  0,  2,  6,  10, 12, 14, /**/ 0, 4,  8,  2,  6,  10, 12, 14,  //
      2,  4,  8,  0,  6,  10, 12, 14, /**/ 0, 2,  4,  8,  6,  10, 12, 14,  //
      6,  8,  0,  2,  4,  10, 12, 14, /**/ 0, 6,  8,  2,  4,  10, 12, 14,  //
      2,  6,  8,  0,  4,  10, 12, 14, /**/ 0, 2,  6,  8,  4,  10, 12, 14,  //
      4,  6,  8,  0,  2,  10, 12, 14, /**/ 0, 4,  6,  8,  2,  10, 12, 14,  //
      2,  4,  6,  8,  0,  10, 12, 14, /**/ 0, 2,  4,  6,  8,  10, 12, 14,  //
      10, 0,  2,  4,  6,  8,  12, 14, /**/ 0, 10, 2,  4,  6,  8,  12, 14,  //
      2,  10, 0,  4,  6,  8,  12, 14, /**/ 0, 2,  10, 4,  6,  8,  12, 14,  //
      4,  10, 0,  2,  6,  8,  12, 14, /**/ 0, 4,  10, 2,  6,  8,  12, 14,  //
      2,  4,  10, 0,  6,  8,  12, 14, /**/ 0, 2,  4,  10, 6,  8,  12, 14,  //
      6,  10, 0,  2,  4,  8,  12, 14, /**/ 0, 6,  10, 2,  4,  8,  12, 14,  //
      2,  6,  10, 0,  4,  8,  12, 14, /**/ 0, 2,  6,  10, 4,  8,  12, 14,  //
      4,  6,  10, 0,  2,  8,  12, 14, /**/ 0, 4,  6,  10, 2,  8,  12, 14,  //
      2,  4,  6,  10, 0,  8,  12, 14, /**/ 0, 2,  4,  6,  10, 8,  12, 14,  //
      8,  10, 0,  2,  4,  6,  12, 14, /**/ 0, 8,  10, 2,  4,  6,  12, 14,  //
      2,  8,  10, 0,  4,  6,  12, 14, /**/ 0, 2,  8,  10, 4,  6,  12, 14,  //
      4,  8,  10, 0,  2,  6,  12, 14, /**/ 0, 4,  8,  10, 2,  6,  12, 14,  //
      2,  4,  8,  10, 0,  6,  12, 14, /**/ 0, 2,  4,  8,  10, 6,  12, 14,  //
      6,  8,  10, 0,  2,  4,  12, 14, /**/ 0, 6,  8,  10, 2,  4,  12, 14,  //
      2,  6,  8,  10, 0,  4,  12, 14, /**/ 0, 2,  6,  8,  10, 4,  12, 14,  //
      4,  6,  8,  10, 0,  2,  12, 14, /**/ 0, 4,  6,  8,  10, 2,  12, 14,  //
      2,  4,  6,  8,  10, 0,  12, 14, /**/ 0, 2,  4,  6,  8,  10, 12, 14,  //
      12, 0,  2,  4,  6,  8,  10, 14, /**/ 0, 12, 2,  4,  6,  8,  10, 14,  //
      2,  12, 0,  4,  6,  8,  10, 14, /**/ 0, 2,  12, 4,  6,  8,  10, 14,  //
      4,  12, 0,  2,  6,  8,  10, 14, /**/ 0, 4,  12, 2,  6,  8,  10, 14,  //
      2,  4,  12, 0,  6,  8,  10, 14, /**/ 0, 2,  4,  12, 6,  8,  10, 14,  //
      6,  12, 0,  2,  4,  8,  10, 14, /**/ 0, 6,  12, 2,  4,  8,  10, 14,  //
      2,  6,  12, 0,  4,  8,  10, 14, /**/ 0, 2,  6,  12, 4,  8,  10, 14,  //
      4,  6,  12, 0,  2,  8,  10, 14, /**/ 0, 4,  6,  12, 2,  8,  10, 14,  //
      2,  4,  6,  12, 0,  8,  10, 14, /**/ 0, 2,  4,  6,  12, 8,  10, 14,  //
      8,  12, 0,  2,  4,  6,  10, 14, /**/ 0, 8,  12, 2,  4,  6,  10, 14,  //
      2,  8,  12, 0,  4,  6,  10, 14, /**/ 0, 2,  8,  12, 4,  6,  10, 14,  //
      4,  8,  12, 0,  2,  6,  10, 14, /**/ 0, 4,  8,  12, 2,  6,  10, 14,  //
      2,  4,  8,  12, 0,  6,  10, 14, /**/ 0, 2,  4,  8,  12, 6,  10, 14,  //
      6,  8,  12, 0,  2,  4,  10, 14, /**/ 0, 6,  8,  12, 2,  4,  10, 14,  //
      2,  6,  8,  12, 0,  4,  10, 14, /**/ 0, 2,  6,  8,  12, 4,  10, 14,  //
      4,  6,  8,  12, 0,  2,  10, 14, /**/ 0, 4,  6,  8,  12, 2,  10, 14,  //
      2,  4,  6,  8,  12, 0,  10, 14, /**/ 0, 2,  4,  6,  8,  12, 10, 14,  //
      10, 12, 0,  2,  4,  6,  8,  14, /**/ 0, 10, 12, 2,  4,  6,  8,  14,  //
      2,  10, 12, 0,  4,  6,  8,  14, /**/ 0, 2,  10, 12, 4,  6,  8,  14,  //
      4,  10, 12, 0,  2,  6,  8,  14, /**/ 0, 4,  10, 12, 2,  6,  8,  14,  //
      2,  4,  10, 12, 0,  6,  8,  14, /**/ 0, 2,  4,  10, 12, 6,  8,  14,  //
      6,  10, 12, 0,  2,  4,  8,  14, /**/ 0, 6,  10, 12, 2,  4,  8,  14,  //
      2,  6,  10, 12, 0,  4,  8,  14, /**/ 0, 2,  6,  10, 12, 4,  8,  14,  //
      4,  6,  10, 12, 0,  2,  8,  14, /**/ 0, 4,  6,  10, 12, 2,  8,  14,  //
      2,  4,  6,  10, 12, 0,  8,  14, /**/ 0, 2,  4,  6,  10, 12, 8,  14,  //
      8,  10, 12, 0,  2,  4,  6,  14, /**/ 0, 8,  10, 12, 2,  4,  6,  14,  //
      2,  8,  10, 12, 0,  4,  6,  14, /**/ 0, 2,  8,  10, 12, 4,  6,  14,  //
      4,  8,  10, 12, 0,  2,  6,  14, /**/ 0, 4,  8,  10, 12, 2,  6,  14,  //
      2,  4,  8,  10, 12, 0,  6,  14, /**/ 0, 2,  4,  8,  10, 12, 6,  14,  //
      6,  8,  10, 12, 0,  2,  4,  14, /**/ 0, 6,  8,  10, 12, 2,  4,  14,  //
      2,  6,  8,  10, 12, 0,  4,  14, /**/ 0, 2,  6,  8,  10, 12, 4,  14,  //
      4,  6,  8,  10, 12, 0,  2,  14, /**/ 0, 4,  6,  8,  10, 12, 2,  14,  //
      2,  4,  6,  8,  10, 12, 0,  14, /**/ 0, 2,  4,  6,  8,  10, 12, 14,  //
      14, 0,  2,  4,  6,  8,  10, 12, /**/ 0, 14, 2,  4,  6,  8,  10, 12,  //
      2,  14, 0,  4,  6,  8,  10, 12, /**/ 0, 2,  14, 4,  6,  8,  10, 12,  //
      4,  14, 0,  2,  6,  8,  10, 12, /**/ 0, 4,  14, 2,  6,  8,  10, 12,  //
      2,  4,  14, 0,  6,  8,  10, 12, /**/ 0, 2,  4,  14, 6,  8,  10, 12,  //
      6,  14, 0,  2,  4,  8,  10, 12, /**/ 0, 6,  14, 2,  4,  8,  10, 12,  //
      2,  6,  14, 0,  4,  8,  10, 12, /**/ 0, 2,  6,  14, 4,  8,  10, 12,  //
      4,  6,  14, 0,  2,  8,  10, 12, /**/ 0, 4,  6,  14, 2,  8,  10, 12,  //
      2,  4,  6,  14, 0,  8,  10, 12, /**/ 0, 2,  4,  6,  14, 8,  10, 12,  //
      8,  14, 0,  2,  4,  6,  10, 12, /**/ 0, 8,  14, 2,  4,  6,  10, 12,  //
      2,  8,  14, 0,  4,  6,  10, 12, /**/ 0, 2,  8,  14, 4,  6,  10, 12,  //
      4,  8,  14, 0,  2,  6,  10, 12, /**/ 0, 4,  8,  14, 2,  6,  10, 12,  //
      2,  4,  8,  14, 0,  6,  10, 12, /**/ 0, 2,  4,  8,  14, 6,  10, 12,  //
      6,  8,  14, 0,  2,  4,  10, 12, /**/ 0, 6,  8,  14, 2,  4,  10, 12,  //
      2,  6,  8,  14, 0,  4,  10, 12, /**/ 0, 2,  6,  8,  14, 4,  10, 12,  //
      4,  6,  8,  14, 0,  2,  10, 12, /**/ 0, 4,  6,  8,  14, 2,  10, 12,  //
      2,  4,  6,  8,  14, 0,  10, 12, /**/ 0, 2,  4,  6,  8,  14, 10, 12,  //
      10, 14, 0,  2,  4,  6,  8,  12, /**/ 0, 10, 14, 2,  4,  6,  8,  12,  //
      2,  10, 14, 0,  4,  6,  8,  12, /**/ 0, 2,  10, 14, 4,  6,  8,  12,  //
      4,  10, 14, 0,  2,  6,  8,  12, /**/ 0, 4,  10, 14, 2,  6,  8,  12,  //
      2,  4,  10, 14, 0,  6,  8,  12, /**/ 0, 2,  4,  10, 14, 6,  8,  12,  //
      6,  10, 14, 0,  2,  4,  8,  12, /**/ 0, 6,  10, 14, 2,  4,  8,  12,  //
      2,  6,  10, 14, 0,  4,  8,  12, /**/ 0, 2,  6,  10, 14, 4,  8,  12,  //
      4,  6,  10, 14, 0,  2,  8,  12, /**/ 0, 4,  6,  10, 14, 2,  8,  12,  //
      2,  4,  6,  10, 14, 0,  8,  12, /**/ 0, 2,  4,  6,  10, 14, 8,  12,  //
      8,  10, 14, 0,  2,  4,  6,  12, /**/ 0, 8,  10, 14, 2,  4,  6,  12,  //
      2,  8,  10, 14, 0,  4,  6,  12, /**/ 0, 2,  8,  10, 14, 4,  6,  12,  //
      4,  8,  10, 14, 0,  2,  6,  12, /**/ 0, 4,  8,  10, 14, 2,  6,  12,  //
      2,  4,  8,  10, 14, 0,  6,  12, /**/ 0, 2,  4,  8,  10, 14, 6,  12,  //
      6,  8,  10, 14, 0,  2,  4,  12, /**/ 0, 6,  8,  10, 14, 2,  4,  12,  //
      2,  6,  8,  10, 14, 0,  4,  12, /**/ 0, 2,  6,  8,  10, 14, 4,  12,  //
      4,  6,  8,  10, 14, 0,  2,  12, /**/ 0, 4,  6,  8,  10, 14, 2,  12,  //
      2,  4,  6,  8,  10, 14, 0,  12, /**/ 0, 2,  4,  6,  8,  10, 14, 12,  //
      12, 14, 0,  2,  4,  6,  8,  10, /**/ 0, 12, 14, 2,  4,  6,  8,  10,  //
      2,  12, 14, 0,  4,  6,  8,  10, /**/ 0, 2,  12, 14, 4,  6,  8,  10,  //
      4,  12, 14, 0,  2,  6,  8,  10, /**/ 0, 4,  12, 14, 2,  6,  8,  10,  //
      2,  4,  12, 14, 0,  6,  8,  10, /**/ 0, 2,  4,  12, 14, 6,  8,  10,  //
      6,  12, 14, 0,  2,  4,  8,  10, /**/ 0, 6,  12, 14, 2,  4,  8,  10,  //
      2,  6,  12, 14, 0,  4,  8,  10, /**/ 0, 2,  6,  12, 14, 4,  8,  10,  //
      4,  6,  12, 14, 0,  2,  8,  10, /**/ 0, 4,  6,  12, 14, 2,  8,  10,  //
      2,  4,  6,  12, 14, 0,  8,  10, /**/ 0, 2,  4,  6,  12, 14, 8,  10,  //
      8,  12, 14, 0,  2,  4,  6,  10, /**/ 0, 8,  12, 14, 2,  4,  6,  10,  //
      2,  8,  12, 14, 0,  4,  6,  10, /**/ 0, 2,  8,  12, 14, 4,  6,  10,  //
      4,  8,  12, 14, 0,  2,  6,  10, /**/ 0, 4,  8,  12, 14, 2,  6,  10,  //
      2,  4,  8,  12, 14, 0,  6,  10, /**/ 0, 2,  4,  8,  12, 14, 6,  10,  //
      6,  8,  12, 14, 0,  2,  4,  10, /**/ 0, 6,  8,  12, 14, 2,  4,  10,  //
      2,  6,  8,  12, 14, 0,  4,  10, /**/ 0, 2,  6,  8,  12, 14, 4,  10,  //
      4,  6,  8,  12, 14, 0,  2,  10, /**/ 0, 4,  6,  8,  12, 14, 2,  10,  //
      2,  4,  6,  8,  12, 14, 0,  10, /**/ 0, 2,  4,  6,  8,  12, 14, 10,  //
      10, 12, 14, 0,  2,  4,  6,  8,  /**/ 0, 10, 12, 14, 2,  4,  6,  8,   //
      2,  10, 12, 14, 0,  4,  6,  8,  /**/ 0, 2,  10, 12, 14, 4,  6,  8,   //
      4,  10, 12, 14, 0,  2,  6,  8,  /**/ 0, 4,  10, 12, 14, 2,  6,  8,   //
      2,  4,  10, 12, 14, 0,  6,  8,  /**/ 0, 2,  4,  10, 12, 14, 6,  8,   //
      6,  10, 12, 14, 0,  2,  4,  8,  /**/ 0, 6,  10, 12, 14, 2,  4,  8,   //
      2,  6,  10, 12, 14, 0,  4,  8,  /**/ 0, 2,  6,  10, 12, 14, 4,  8,   //
      4,  6,  10, 12, 14, 0,  2,  8,  /**/ 0, 4,  6,  10, 12, 14, 2,  8,   //
      2,  4,  6,  10, 12, 14, 0,  8,  /**/ 0, 2,  4,  6,  10, 12, 14, 8,   //
      8,  10, 12, 14, 0,  2,  4,  6,  /**/ 0, 8,  10, 12, 14, 2,  4,  6,   //
      2,  8,  10, 12, 14, 0,  4,  6,  /**/ 0, 2,  8,  10, 12, 14, 4,  6,   //
      4,  8,  10, 12, 14, 0,  2,  6,  /**/ 0, 4,  8,  10, 12, 14, 2,  6,   //
      2,  4,  8,  10, 12, 14, 0,  6,  /**/ 0, 2,  4,  8,  10, 12, 14, 6,   //
      6,  8,  10, 12, 14, 0,  2,  4,  /**/ 0, 6,  8,  10, 12, 14, 2,  4,   //
      2,  6,  8,  10, 12, 14, 0,  4,  /**/ 0, 2,  6,  8,  10, 12, 14, 4,   //
      4,  6,  8,  10, 12, 14, 0,  2,  /**/ 0, 4,  6,  8,  10, 12, 14, 2,   //
      2,  4,  6,  8,  10, 12, 14, 0,  /**/ 0, 2,  4,  6,  8,  10, 12, 14};

  const Vec128<uint8_t, 2 * N> byte_idx = Load8Bytes(d8, table + mask_bits * 8);
  const Vec128<uint16_t, N> pairs = ZipLower(byte_idx, byte_idx);
  return BitCast(d, pairs + Set(du, 0x0100));
}

template <typename T, size_t N>
HWY_INLINE Vec128<T, N> IdxFromNotBits(hwy::SizeTag<2> /*tag*/,
                                       const uint64_t mask_bits) {
  HWY_DASSERT(mask_bits < 256);
  const Simd<T, N, 0> d;
  const Repartition<uint8_t, decltype(d)> d8;
  const Simd<uint16_t, N, 0> du;

  // ARM does not provide an equivalent of AVX2 permutevar, so we need byte
  // indices for VTBL (one vector's worth for each of 256 combinations of
  // 8 mask bits). Loading them directly would require 4 KiB. We can instead
  // store lane indices and convert to byte indices (2*lane + 0..1), with the
  // doubling baked into the table. AVX2 Compress32 stores eight 4-bit lane
  // indices (total 1 KiB), broadcasts them into each 32-bit lane and shifts.
  // Here, 16-bit lanes are too narrow to hold all bits, and unpacking nibbles
  // is likely more costly than the higher cache footprint from storing bytes.
  alignas(16) constexpr uint8_t table[256 * 8] = {
      // PrintCompressNot16x8Tables
      0, 2,  4,  6,  8,  10, 12, 14, /**/ 2,  4,  6,  8,  10, 12, 14, 0,   //
      0, 4,  6,  8,  10, 12, 14, 2,  /**/ 4,  6,  8,  10, 12, 14, 0,  2,   //
      0, 2,  6,  8,  10, 12, 14, 4,  /**/ 2,  6,  8,  10, 12, 14, 0,  4,   //
      0, 6,  8,  10, 12, 14, 2,  4,  /**/ 6,  8,  10, 12, 14, 0,  2,  4,   //
      0, 2,  4,  8,  10, 12, 14, 6,  /**/ 2,  4,  8,  10, 12, 14, 0,  6,   //
      0, 4,  8,  10, 12, 14, 2,  6,  /**/ 4,  8,  10, 12, 14, 0,  2,  6,   //
      0, 2,  8,  10, 12, 14, 4,  6,  /**/ 2,  8,  10, 12, 14, 0,  4,  6,   //
      0, 8,  10, 12, 14, 2,  4,  6,  /**/ 8,  10, 12, 14, 0,  2,  4,  6,   //
      0, 2,  4,  6,  10, 12, 14, 8,  /**/ 2,  4,  6,  10, 12, 14, 0,  8,   //
      0, 4,  6,  10, 12, 14, 2,  8,  /**/ 4,  6,  10, 12, 14, 0,  2,  8,   //
      0, 2,  6,  10, 12, 14, 4,  8,  /**/ 2,  6,  10, 12, 14, 0,  4,  8,   //
      0, 6,  10, 12, 14, 2,  4,  8,  /**/ 6,  10, 12, 14, 0,  2,  4,  8,   //
      0, 2,  4,  10, 12, 14, 6,  8,  /**/ 2,  4,  10, 12, 14, 0,  6,  8,   //
      0, 4,  10, 12, 14, 2,  6,  8,  /**/ 4,  10, 12, 14, 0,  2,  6,  8,   //
      0, 2,  10, 12, 14, 4,  6,  8,  /**/ 2,  10, 12, 14, 0,  4,  6,  8,   //
      0, 10, 12, 14, 2,  4,  6,  8,  /**/ 10, 12, 14, 0,  2,  4,  6,  8,   //
      0, 2,  4,  6,  8,  12, 14, 10, /**/ 2,  4,  6,  8,  12, 14, 0,  10,  //
      0, 4,  6,  8,  12, 14, 2,  10, /**/ 4,  6,  8,  12, 14, 0,  2,  10,  //
      0, 2,  6,  8,  12, 14, 4,  10, /**/ 2,  6,  8,  12, 14, 0,  4,  10,  //
      0, 6,  8,  12, 14, 2,  4,  10, /**/ 6,  8,  12, 14, 0,  2,  4,  10,  //
      0, 2,  4,  8,  12, 14, 6,  10, /**/ 2,  4,  8,  12, 14, 0,  6,  10,  //
      0, 4,  8,  12, 14, 2,  6,  10, /**/ 4,  8,  12, 14, 0,  2,  6,  10,  //
      0, 2,  8,  12, 14, 4,  6,  10, /**/ 2,  8,  12, 14, 0,  4,  6,  10,  //
      0, 8,  12, 14, 2,  4,  6,  10, /**/ 8,  12, 14, 0,  2,  4,  6,  10,  //
      0, 2,  4,  6,  12, 14, 8,  10, /**/ 2,  4,  6,  12, 14, 0,  8,  10,  //
      0, 4,  6,  12, 14, 2,  8,  10, /**/ 4,  6,  12, 14, 0,  2,  8,  10,  //
      0, 2,  6,  12, 14, 4,  8,  10, /**/ 2,  6,  12, 14, 0,  4,  8,  10,  //
      0, 6,  12, 14, 2,  4,  8,  10, /**/ 6,  12, 14, 0,  2,  4,  8,  10,  //
      0, 2,  4,  12, 14, 6,  8,  10, /**/ 2,  4,  12, 14, 0,  6,  8,  10,  //
      0, 4,  12, 14, 2,  6,  8,  10, /**/ 4,  12, 14, 0,  2,  6,  8,  10,  //
      0, 2,  12, 14, 4,  6,  8,  10, /**/ 2,  12, 14, 0,  4,  6,  8,  10,  //
      0, 12, 14, 2,  4,  6,  8,  10, /**/ 12, 14, 0,  2,  4,  6,  8,  10,  //
      0, 2,  4,  6,  8,  10, 14, 12, /**/ 2,  4,  6,  8,  10, 14, 0,  12,  //
      0, 4,  6,  8,  10, 14, 2,  12, /**/ 4,  6,  8,  10, 14, 0,  2,  12,  //
      0, 2,  6,  8,  10, 14, 4,  12, /**/ 2,  6,  8,  10, 14, 0,  4,  12,  //
      0, 6,  8,  10, 14, 2,  4,  12, /**/ 6,  8,  10, 14, 0,  2,  4,  12,  //
      0, 2,  4,  8,  10, 14, 6,  12, /**/ 2,  4,  8,  10, 14, 0,  6,  12,  //
      0, 4,  8,  10, 14, 2,  6,  12, /**/ 4,  8,  10, 14, 0,  2,  6,  12,  //
      0, 2,  8,  10, 14, 4,  6,  12, /**/ 2,  8,  10, 14, 0,  4,  6,  12,  //
      0, 8,  10, 14, 2,  4,  6,  12, /**/ 8,  10, 14, 0,  2,  4,  6,  12,  //
      0, 2,  4,  6,  10, 14, 8,  12, /**/ 2,  4,  6,  10, 14, 0,  8,  12,  //
      0, 4,  6,  10, 14, 2,  8,  12, /**/ 4,  6,  10, 14, 0,  2,  8,  12,  //
      0, 2,  6,  10, 14, 4,  8,  12, /**/ 2,  6,  10, 14, 0,  4,  8,  12,  //
      0, 6,  10, 14, 2,  4,  8,  12, /**/ 6,  10, 14, 0,  2,  4,  8,  12,  //
      0, 2,  4,  10, 14, 6,  8,  12, /**/ 2,  4,  10, 14, 0,  6,  8,  12,  //
      0, 4,  10, 14, 2,  6,  8,  12, /**/ 4,  10, 14, 0,  2,  6,  8,  12,  //
      0, 2,  10, 14, 4,  6,  8,  12, /**/ 2,  10, 14, 0,  4,  6,  8,  12,  //
      0, 10, 14, 2,  4,  6,  8,  12, /**/ 10, 14, 0,  2,  4,  6,  8,  12,  //
      0, 2,  4,  6,  8,  14, 10, 12, /**/ 2,  4,  6,  8,  14, 0,  10, 12,  //
      0, 4,  6,  8,  14, 2,  10, 12, /**/ 4,  6,  8,  14, 0,  2,  10, 12,  //
      0, 2,  6,  8,  14, 4,  10, 12, /**/ 2,  6,  8,  14, 0,  4,  10, 12,  //
      0, 6,  8,  14, 2,  4,  10, 12, /**/ 6,  8,  14, 0,  2,  4,  10, 12,  //
      0, 2,  4,  8,  14, 6,  10, 12, /**/ 2,  4,  8,  14, 0,  6,  10, 12,  //
      0, 4,  8,  14, 2,  6,  10, 12, /**/ 4,  8,  14, 0,  2,  6,  10, 12,  //
      0, 2,  8,  14, 4,  6,  10, 12, /**/ 2,  8,  14, 0,  4,  6,  10, 12,  //
      0, 8,  14, 2,  4,  6,  10, 12, /**/ 8,  14, 0,  2,  4,  6,  10, 12,  //
      0, 2,  4,  6,  14, 8,  10, 12, /**/ 2,  4,  6,  14, 0,  8,  10, 12,  //
      0, 4,  6,  14, 2,  8,  10, 12, /**/ 4,  6,  14, 0,  2,  8,  10, 12,  //
      0, 2,  6,  14, 4,  8,  10, 12, /**/ 2,  6,  14, 0,  4,  8,  10, 12,  //
      0, 6,  14, 2,  4,  8,  10, 12, /**/ 6,  14, 0,  2,  4,  8,  10, 12,  //
      0, 2,  4,  14, 6,  8,  10, 12, /**/ 2,  4,  14, 0,  6,  8,  10, 12,  //
      0, 4,  14, 2,  6,  8,  10, 12, /**/ 4,  14, 0,  2,  6,  8,  10, 12,  //
      0, 2,  14, 4,  6,  8,  10, 12, /**/ 2,  14, 0,  4,  6,  8,  10, 12,  //
      0, 14, 2,  4,  6,  8,  10, 12, /**/ 14, 0,  2,  4,  6,  8,  10, 12,  //
      0, 2,  4,  6,  8,  10, 12, 14, /**/ 2,  4,  6,  8,  10, 12, 0,  14,  //
      0, 4,  6,  8,  10, 12, 2,  14, /**/ 4,  6,  8,  10, 12, 0,  2,  14,  //
      0, 2,  6,  8,  10, 12, 4,  14, /**/ 2,  6,  8,  10, 12, 0,  4,  14,  //
      0, 6,  8,  10, 12, 2,  4,  14, /**/ 6,  8,  10, 12, 0,  2,  4,  14,  //
      0, 2,  4,  8,  10, 12, 6,  14, /**/ 2,  4,  8,  10, 12, 0,  6,  14,  //
      0, 4,  8,  10, 12, 2,  6,  14, /**/ 4,  8,  10, 12, 0,  2,  6,  14,  //
      0, 2,  8,  10, 12, 4,  6,  14, /**/ 2,  8,  10, 12, 0,  4,  6,  14,  //
      0, 8,  10, 12, 2,  4,  6,  14, /**/ 8,  10, 12, 0,  2,  4,  6,  14,  //
      0, 2,  4,  6,  10, 12, 8,  14, /**/ 2,  4,  6,  10, 12, 0,  8,  14,  //
      0, 4,  6,  10, 12, 2,  8,  14, /**/ 4,  6,  10, 12, 0,  2,  8,  14,  //
      0, 2,  6,  10, 12, 4,  8,  14, /**/ 2,  6,  10, 12, 0,  4,  8,  14,  //
      0, 6,  10, 12, 2,  4,  8,  14, /**/ 6,  10, 12, 0,  2,  4,  8,  14,  //
      0, 2,  4,  10, 12, 6,  8,  14, /**/ 2,  4,  10, 12, 0,  6,  8,  14,  //
      0, 4,  10, 12, 2,  6,  8,  14, /**/ 4,  10, 12, 0,  2,  6,  8,  14,  //
      0, 2,  10, 12, 4,  6,  8,  14, /**/ 2,  10, 12, 0,  4,  6,  8,  14,  //
      0, 10, 12, 2,  4,  6,  8,  14, /**/ 10, 12, 0,  2,  4,  6,  8,  14,  //
      0, 2,  4,  6,  8,  12, 10, 14, /**/ 2,  4,  6,  8,  12, 0,  10, 14,  //
      0, 4,  6,  8,  12, 2,  10, 14, /**/ 4,  6,  8,  12, 0,  2,  10, 14,  //
      0, 2,  6,  8,  12, 4,  10, 14, /**/ 2,  6,  8,  12, 0,  4,  10, 14,  //
      0, 6,  8,  12, 2,  4,  10, 14, /**/ 6,  8,  12, 0,  2,  4,  10, 14,  //
      0, 2,  4,  8,  12, 6,  10, 14, /**/ 2,  4,  8,  12, 0,  6,  10, 14,  //
      0, 4,  8,  12, 2,  6,  10, 14, /**/ 4,  8,  12, 0,  2,  6,  10, 14,  //
      0, 2,  8,  12, 4,  6,  10, 14, /**/ 2,  8,  12, 0,  4,  6,  10, 14,  //
      0, 8,  12, 2,  4,  6,  10, 14, /**/ 8,  12, 0,  2,  4,  6,  10, 14,  //
      0, 2,  4,  6,  12, 8,  10, 14, /**/ 2,  4,  6,  12, 0,  8,  10, 14,  //
      0, 4,  6,  12, 2,  8,  10, 14, /**/ 4,  6,  12, 0,  2,  8,  10, 14,  //
      0, 2,  6,  12, 4,  8,  10, 14, /**/ 2,  6,  12, 0,  4,  8,  10, 14,  //
      0, 6,  12, 2,  4,  8,  10, 14, /**/ 6,  12, 0,  2,  4,  8,  10, 14,  //
      0, 2,  4,  12, 6,  8,  10, 14, /**/ 2,  4,  12, 0,  6,  8,  10, 14,  //
      0, 4,  12, 2,  6,  8,  10, 14, /**/ 4,  12, 0,  2,  6,  8,  10, 14,  //
      0, 2,  12, 4,  6,  8,  10, 14, /**/ 2,  12, 0,  4,  6,  8,  10, 14,  //
      0, 12, 2,  4,  6,  8,  10, 14, /**/ 12, 0,  2,  4,  6,  8,  10, 14,  //
      0, 2,  4,  6,  8,  10, 12, 14, /**/ 2,  4,  6,  8,  10, 0,  12, 14,  //
      0, 4,  6,  8,  10, 2,  12, 14, /**/ 4,  6,  8,  10, 0,  2,  12, 14,  //
      0, 2,  6,  8,  10, 4,  12, 14, /**/ 2,  6,  8,  10, 0,  4,  12, 14,  //
      0, 6,  8,  10, 2,  4,  12, 14, /**/ 6,  8,  10, 0,  2,  4,  12, 14,  //
      0, 2,  4,  8,  10, 6,  12, 14, /**/ 2,  4,  8,  10, 0,  6,  12, 14,  //
      0, 4,  8,  10, 2,  6,  12, 14, /**/ 4,  8,  10, 0,  2,  6,  12, 14,  //
      0, 2,  8,  10, 4,  6,  12, 14, /**/ 2,  8,  10, 0,  4,  6,  12, 14,  //
      0, 8,  10, 2,  4,  6,  12, 14, /**/ 8,  10, 0,  2,  4,  6,  12, 14,  //
      0, 2,  4,  6,  10, 8,  12, 14, /**/ 2,  4,  6,  10, 0,  8,  12, 14,  //
      0, 4,  6,  10, 2,  8,  12, 14, /**/ 4,  6,  10, 0,  2,  8,  12, 14,  //
      0, 2,  6,  10, 4,  8,  12, 14, /**/ 2,  6,  10, 0,  4,  8,  12, 14,  //
      0, 6,  10, 2,  4,  8,  12, 14, /**/ 6,  10, 0,  2,  4,  8,  12, 14,  //
      0, 2,  4,  10, 6,  8,  12, 14, /**/ 2,  4,  10, 0,  6,  8,  12, 14,  //
      0, 4,  10, 2,  6,  8,  12, 14, /**/ 4,  10, 0,  2,  6,  8,  12, 14,  //
      0, 2,  10, 4,  6,  8,  12, 14, /**/ 2,  10, 0,  4,  6,  8,  12, 14,  //
      0, 10, 2,  4,  6,  8,  12, 14, /**/ 10, 0,  2,  4,  6,  8,  12, 14,  //
      0, 2,  4,  6,  8,  10, 12, 14, /**/ 2,  4,  6,  8,  0,  10, 12, 14,  //
      0, 4,  6,  8,  2,  10, 12, 14, /**/ 4,  6,  8,  0,  2,  10, 12, 14,  //
      0, 2,  6,  8,  4,  10, 12, 14, /**/ 2,  6,  8,  0,  4,  10, 12, 14,  //
      0, 6,  8,  2,  4,  10, 12, 14, /**/ 6,  8,  0,  2,  4,  10, 12, 14,  //
      0, 2,  4,  8,  6,  10, 12, 14, /**/ 2,  4,  8,  0,  6,  10, 12, 14,  //
      0, 4,  8,  2,  6,  10, 12, 14, /**/ 4,  8,  0,  2,  6,  10, 12, 14,  //
      0, 2,  8,  4,  6,  10, 12, 14, /**/ 2,  8,  0,  4,  6,  10, 12, 14,  //
      0, 8,  2,  4,  6,  10, 12, 14, /**/ 8,  0,  2,  4,  6,  10, 12, 14,  //
      0, 2,  4,  6,  8,  10, 12, 14, /**/ 2,  4,  6,  0,  8,  10, 12, 14,  //
      0, 4,  6,  2,  8,  10, 12, 14, /**/ 4,  6,  0,  2,  8,  10, 12, 14,  //
      0, 2,  6,  4,  8,  10, 12, 14, /**/ 2,  6,  0,  4,  8,  10, 12, 14,  //
      0, 6,  2,  4,  8,  10, 12, 14, /**/ 6,  0,  2,  4,  8,  10, 12, 14,  //
      0, 2,  4,  6,  8,  10, 12, 14, /**/ 2,  4,  0,  6,  8,  10, 12, 14,  //
      0, 4,  2,  6,  8,  10, 12, 14, /**/ 4,  0,  2,  6,  8,  10, 12, 14,  //
      0, 2,  4,  6,  8,  10, 12, 14, /**/ 2,  0,  4,  6,  8,  10, 12, 14,  //
      0, 2,  4,  6,  8,  10, 12, 14, /**/ 0,  2,  4,  6,  8,  10, 12, 14};

  const Vec128<uint8_t, 2 * N> byte_idx = Load8Bytes(d8, table + mask_bits * 8);
  const Vec128<uint16_t, N> pairs = ZipLower(byte_idx, byte_idx);
  return BitCast(d, pairs + Set(du, 0x0100));
}

template <typename T, size_t N>
HWY_INLINE Vec128<T, N> IdxFromBits(hwy::SizeTag<4> /*tag*/,
                                    const uint64_t mask_bits) {
  HWY_DASSERT(mask_bits < 16);

  // There are only 4 lanes, so we can afford to load the index vector directly.
  alignas(16) constexpr uint8_t u8_indices[16 * 16] = {
      // PrintCompress32x4Tables
      0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15,  //
      0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15,  //
      4,  5,  6,  7,  0,  1,  2,  3,  8,  9,  10, 11, 12, 13, 14, 15,  //
      0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15,  //
      8,  9,  10, 11, 0,  1,  2,  3,  4,  5,  6,  7,  12, 13, 14, 15,  //
      0,  1,  2,  3,  8,  9,  10, 11, 4,  5,  6,  7,  12, 13, 14, 15,  //
      4,  5,  6,  7,  8,  9,  10, 11, 0,  1,  2,  3,  12, 13, 14, 15,  //
      0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15,  //
      12, 13, 14, 15, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11,  //
      0,  1,  2,  3,  12, 13, 14, 15, 4,  5,  6,  7,  8,  9,  10, 11,  //
      4,  5,  6,  7,  12, 13, 14, 15, 0,  1,  2,  3,  8,  9,  10, 11,  //
      0,  1,  2,  3,  4,  5,  6,  7,  12, 13, 14, 15, 8,  9,  10, 11,  //
      8,  9,  10, 11, 12, 13, 14, 15, 0,  1,  2,  3,  4,  5,  6,  7,   //
      0,  1,  2,  3,  8,  9,  10, 11, 12, 13, 14, 15, 4,  5,  6,  7,   //
      4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 0,  1,  2,  3,   //
      0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15};
  const Simd<T, N, 0> d;
  const Repartition<uint8_t, decltype(d)> d8;
  return BitCast(d, Load(d8, u8_indices + 16 * mask_bits));
}

template <typename T, size_t N>
HWY_INLINE Vec128<T, N> IdxFromNotBits(hwy::SizeTag<4> /*tag*/,
                                       const uint64_t mask_bits) {
  HWY_DASSERT(mask_bits < 16);

  // There are only 4 lanes, so we can afford to load the index vector directly.
  alignas(16) constexpr uint8_t u8_indices[16 * 16] = {
      // PrintCompressNot32x4Tables
      0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 4,  5,
      6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 0,  1,  2,  3,  0,  1,  2,  3,
      8,  9,  10, 11, 12, 13, 14, 15, 4,  5,  6,  7,  8,  9,  10, 11, 12, 13,
      14, 15, 0,  1,  2,  3,  4,  5,  6,  7,  0,  1,  2,  3,  4,  5,  6,  7,
      12, 13, 14, 15, 8,  9,  10, 11, 4,  5,  6,  7,  12, 13, 14, 15, 0,  1,
      2,  3,  8,  9,  10, 11, 0,  1,  2,  3,  12, 13, 14, 15, 4,  5,  6,  7,
      8,  9,  10, 11, 12, 13, 14, 15, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
      10, 11, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15,
      4,  5,  6,  7,  8,  9,  10, 11, 0,  1,  2,  3,  12, 13, 14, 15, 0,  1,
      2,  3,  8,  9,  10, 11, 4,  5,  6,  7,  12, 13, 14, 15, 8,  9,  10, 11,
      0,  1,  2,  3,  4,  5,  6,  7,  12, 13, 14, 15, 0,  1,  2,  3,  4,  5,
      6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 4,  5,  6,  7,  0,  1,  2,  3,
      8,  9,  10, 11, 12, 13, 14, 15, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
      10, 11, 12, 13, 14, 15, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11,
      12, 13, 14, 15};
  const Simd<T, N, 0> d;
  const Repartition<uint8_t, decltype(d)> d8;
  return BitCast(d, Load(d8, u8_indices + 16 * mask_bits));
}

#if HWY_HAVE_INTEGER64 || HWY_HAVE_FLOAT64

template <typename T, size_t N>
HWY_INLINE Vec128<T, N> IdxFromBits(hwy::SizeTag<8> /*tag*/,
                                    const uint64_t mask_bits) {
  HWY_DASSERT(mask_bits < 4);

  // There are only 2 lanes, so we can afford to load the index vector directly.
  alignas(16) constexpr uint8_t u8_indices[64] = {
      // PrintCompress64x2Tables
      0, 1, 2,  3,  4,  5,  6,  7,  8, 9, 10, 11, 12, 13, 14, 15,
      0, 1, 2,  3,  4,  5,  6,  7,  8, 9, 10, 11, 12, 13, 14, 15,
      8, 9, 10, 11, 12, 13, 14, 15, 0, 1, 2,  3,  4,  5,  6,  7,
      0, 1, 2,  3,  4,  5,  6,  7,  8, 9, 10, 11, 12, 13, 14, 15};

  const Simd<T, N, 0> d;
  const Repartition<uint8_t, decltype(d)> d8;
  return BitCast(d, Load(d8, u8_indices + 16 * mask_bits));
}

template <typename T, size_t N>
HWY_INLINE Vec128<T, N> IdxFromNotBits(hwy::SizeTag<8> /*tag*/,
                                       const uint64_t mask_bits) {
  HWY_DASSERT(mask_bits < 4);

  // There are only 2 lanes, so we can afford to load the index vector directly.
  alignas(16) constexpr uint8_t u8_indices[4 * 16] = {
      // PrintCompressNot64x2Tables
      0, 1, 2,  3,  4,  5,  6,  7,  8, 9, 10, 11, 12, 13, 14, 15,
      8, 9, 10, 11, 12, 13, 14, 15, 0, 1, 2,  3,  4,  5,  6,  7,
      0, 1, 2,  3,  4,  5,  6,  7,  8, 9, 10, 11, 12, 13, 14, 15,
      0, 1, 2,  3,  4,  5,  6,  7,  8, 9, 10, 11, 12, 13, 14, 15};

  const Simd<T, N, 0> d;
  const Repartition<uint8_t, decltype(d)> d8;
  return BitCast(d, Load(d8, u8_indices + 16 * mask_bits));
}

#endif

// Helper function called by both Compress and CompressStore - avoids a
// redundant BitsFromMask in the latter.
template <typename T, size_t N>
HWY_INLINE Vec128<T, N> Compress(Vec128<T, N> v, const uint64_t mask_bits) {
  const auto idx =
      detail::IdxFromBits<T, N>(hwy::SizeTag<sizeof(T)>(), mask_bits);
  using D = Simd<T, N, 0>;
  const RebindToSigned<D> di;
  return BitCast(D(), TableLookupBytes(BitCast(di, v), BitCast(di, idx)));
}

template <typename T, size_t N>
HWY_INLINE Vec128<T, N> CompressNot(Vec128<T, N> v, const uint64_t mask_bits) {
  const auto idx =
      detail::IdxFromNotBits<T, N>(hwy::SizeTag<sizeof(T)>(), mask_bits);
  using D = Simd<T, N, 0>;
  const RebindToSigned<D> di;
  return BitCast(D(), TableLookupBytes(BitCast(di, v), BitCast(di, idx)));
}

}  // namespace detail

// Single lane: no-op
template <typename T>
HWY_API Vec128<T, 1> Compress(Vec128<T, 1> v, Mask128<T, 1> /*m*/) {
  return v;
}

// Two lanes: conditional swap
template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Vec128<T, N> Compress(Vec128<T, N> v, const Mask128<T, N> mask) {
  // If mask[1] = 1 and mask[0] = 0, then swap both halves, else keep.
  const Simd<T, N, 0> d;
  const Vec128<T, N> m = VecFromMask(d, mask);
  const Vec128<T, N> maskL = DupEven(m);
  const Vec128<T, N> maskH = DupOdd(m);
  const Vec128<T, N> swap = AndNot(maskL, maskH);
  return IfVecThenElse(swap, Shuffle01(v), v);
}

// General case
template <typename T, size_t N, HWY_IF_NOT_LANE_SIZE(T, 8)>
HWY_API Vec128<T, N> Compress(Vec128<T, N> v, const Mask128<T, N> mask) {
  return detail::Compress(v, detail::BitsFromMask(mask));
}

// Single lane: no-op
template <typename T>
HWY_API Vec128<T, 1> CompressNot(Vec128<T, 1> v, Mask128<T, 1> /*m*/) {
  return v;
}

// Two lanes: conditional swap
template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Vec128<T> CompressNot(Vec128<T> v, Mask128<T> mask) {
  // If mask[1] = 0 and mask[0] = 1, then swap both halves, else keep.
  const Full128<T> d;
  const Vec128<T> m = VecFromMask(d, mask);
  const Vec128<T> maskL = DupEven(m);
  const Vec128<T> maskH = DupOdd(m);
  const Vec128<T> swap = AndNot(maskH, maskL);
  return IfVecThenElse(swap, Shuffle01(v), v);
}

// General case
template <typename T, size_t N, HWY_IF_NOT_LANE_SIZE(T, 8)>
HWY_API Vec128<T, N> CompressNot(Vec128<T, N> v, Mask128<T, N> mask) {
  // For partial vectors, we cannot pull the Not() into the table because
  // BitsFromMask clears the upper bits.
  if (N < 16 / sizeof(T)) {
    return detail::Compress(v, detail::BitsFromMask(Not(mask)));
  }
  return detail::CompressNot(v, detail::BitsFromMask(mask));
}

// ------------------------------ CompressBlocksNot
HWY_API Vec128<uint64_t> CompressBlocksNot(Vec128<uint64_t> v,
                                           Mask128<uint64_t> /* m */) {
  return v;
}

// ------------------------------ CompressBits

template <typename T, size_t N>
HWY_INLINE Vec128<T, N> CompressBits(Vec128<T, N> v,
                                     const uint8_t* HWY_RESTRICT bits) {
  uint64_t mask_bits = 0;
  constexpr size_t kNumBytes = (N + 7) / 8;
  CopyBytes<kNumBytes>(bits, &mask_bits);
  if (N < 8) {
    mask_bits &= (1ull << N) - 1;
  }

  return detail::Compress(v, mask_bits);
}

// ------------------------------ CompressStore
template <typename T, size_t N>
HWY_API size_t CompressStore(Vec128<T, N> v, const Mask128<T, N> mask,
                             Simd<T, N, 0> d, T* HWY_RESTRICT unaligned) {
  const uint64_t mask_bits = detail::BitsFromMask(mask);
  StoreU(detail::Compress(v, mask_bits), d, unaligned);
  return PopCount(mask_bits);
}

// ------------------------------ CompressBlendedStore
template <typename T, size_t N>
HWY_API size_t CompressBlendedStore(Vec128<T, N> v, Mask128<T, N> m,
                                    Simd<T, N, 0> d,
                                    T* HWY_RESTRICT unaligned) {
  const RebindToUnsigned<decltype(d)> du;  // so we can support fp16/bf16
  using TU = TFromD<decltype(du)>;
  const uint64_t mask_bits = detail::BitsFromMask(m);
  const size_t count = PopCount(mask_bits);
  const Mask128<T, N> store_mask = RebindMask(d, FirstN(du, count));
  const Vec128<TU, N> compressed = detail::Compress(BitCast(du, v), mask_bits);
  BlendedStore(BitCast(d, compressed), store_mask, d, unaligned);
  return count;
}

// ------------------------------ CompressBitsStore

template <typename T, size_t N>
HWY_API size_t CompressBitsStore(Vec128<T, N> v,
                                 const uint8_t* HWY_RESTRICT bits,
                                 Simd<T, N, 0> d, T* HWY_RESTRICT unaligned) {
  uint64_t mask_bits = 0;
  constexpr size_t kNumBytes = (N + 7) / 8;
  CopyBytes<kNumBytes>(bits, &mask_bits);
  if (N < 8) {
    mask_bits &= (1ull << N) - 1;
  }

  StoreU(detail::Compress(v, mask_bits), d, unaligned);
  return PopCount(mask_bits);
}

// ------------------------------ LoadInterleaved2

// Per-target flag to prevent generic_ops-inl.h from defining LoadInterleaved2.
#ifdef HWY_NATIVE_LOAD_STORE_INTERLEAVED
#undef HWY_NATIVE_LOAD_STORE_INTERLEAVED
#else
#define HWY_NATIVE_LOAD_STORE_INTERLEAVED
#endif

namespace detail {
#define HWY_NEON_BUILD_TPL_HWY_LOAD_INT
#define HWY_NEON_BUILD_ARG_HWY_LOAD_INT from

#if HWY_ARCH_ARM_A64
#define HWY_IF_LOAD_INT(T, N) HWY_IF_GE64(T, N)
#define HWY_NEON_DEF_FUNCTION_LOAD_INT HWY_NEON_DEF_FUNCTION_ALL_TYPES
#else
// Exclude 64x2 and f64x1, which are only supported on aarch64
#define HWY_IF_LOAD_INT(T, N) \
  hwy::EnableIf<N * sizeof(T) >= 8 && (N == 1 || sizeof(T) < 8)>* = nullptr
#define HWY_NEON_DEF_FUNCTION_LOAD_INT(name, prefix, infix, args) \
  HWY_NEON_DEF_FUNCTION_INT_8_16_32(name, prefix, infix, args)    \
  HWY_NEON_DEF_FUNCTION_UINT_8_16_32(name, prefix, infix, args)   \
  HWY_NEON_DEF_FUNCTION_FLOAT_32(name, prefix, infix, args)       \
  HWY_NEON_DEF_FUNCTION(int64, 1, name, prefix, infix, s64, args) \
  HWY_NEON_DEF_FUNCTION(uint64, 1, name, prefix, infix, u64, args)
#endif  // HWY_ARCH_ARM_A64

// Must return raw tuple because Tuple2 lack a ctor, and we cannot use
// brace-initialization in HWY_NEON_DEF_FUNCTION because some functions return
// void.
#define HWY_NEON_BUILD_RET_HWY_LOAD_INT(type, size) \
  decltype(Tuple2<type##_t, size>().raw)
// Tuple tag arg allows overloading (cannot just overload on return type)
#define HWY_NEON_BUILD_PARAM_HWY_LOAD_INT(type, size) \
  const type##_t *from, Tuple2<type##_t, size>
HWY_NEON_DEF_FUNCTION_LOAD_INT(LoadInterleaved2, vld2, _, HWY_LOAD_INT)
#undef HWY_NEON_BUILD_RET_HWY_LOAD_INT
#undef HWY_NEON_BUILD_PARAM_HWY_LOAD_INT

#define HWY_NEON_BUILD_RET_HWY_LOAD_INT(type, size) \
  decltype(Tuple3<type##_t, size>().raw)
#define HWY_NEON_BUILD_PARAM_HWY_LOAD_INT(type, size) \
  const type##_t *from, Tuple3<type##_t, size>
HWY_NEON_DEF_FUNCTION_LOAD_INT(LoadInterleaved3, vld3, _, HWY_LOAD_INT)
#undef HWY_NEON_BUILD_PARAM_HWY_LOAD_INT
#undef HWY_NEON_BUILD_RET_HWY_LOAD_INT

#define HWY_NEON_BUILD_RET_HWY_LOAD_INT(type, size) \
  decltype(Tuple4<type##_t, size>().raw)
#define HWY_NEON_BUILD_PARAM_HWY_LOAD_INT(type, size) \
  const type##_t *from, Tuple4<type##_t, size>
HWY_NEON_DEF_FUNCTION_LOAD_INT(LoadInterleaved4, vld4, _, HWY_LOAD_INT)
#undef HWY_NEON_BUILD_PARAM_HWY_LOAD_INT
#undef HWY_NEON_BUILD_RET_HWY_LOAD_INT

#undef HWY_NEON_DEF_FUNCTION_LOAD_INT
#undef HWY_NEON_BUILD_TPL_HWY_LOAD_INT
#undef HWY_NEON_BUILD_ARG_HWY_LOAD_INT
}  // namespace detail

template <typename T, size_t N, HWY_IF_LOAD_INT(T, N)>
HWY_API void LoadInterleaved2(Simd<T, N, 0> /*tag*/,
                              const T* HWY_RESTRICT unaligned, Vec128<T, N>& v0,
                              Vec128<T, N>& v1) {
  auto raw = detail::LoadInterleaved2(unaligned, detail::Tuple2<T, N>());
  v0 = Vec128<T, N>(raw.val[0]);
  v1 = Vec128<T, N>(raw.val[1]);
}

// <= 32 bits: avoid loading more than N bytes by copying to buffer
template <typename T, size_t N, HWY_IF_LE32(T, N)>
HWY_API void LoadInterleaved2(Simd<T, N, 0> /*tag*/,
                              const T* HWY_RESTRICT unaligned, Vec128<T, N>& v0,
                              Vec128<T, N>& v1) {
  // The smallest vector registers are 64-bits and we want space for two.
  alignas(16) T buf[2 * 8 / sizeof(T)] = {};
  CopyBytes<N * 2 * sizeof(T)>(unaligned, buf);
  auto raw = detail::LoadInterleaved2(buf, detail::Tuple2<T, N>());
  v0 = Vec128<T, N>(raw.val[0]);
  v1 = Vec128<T, N>(raw.val[1]);
}

#if HWY_ARCH_ARM_V7
// 64x2: split into two 64x1
template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_API void LoadInterleaved2(Full128<T> d, T* HWY_RESTRICT unaligned,
                              Vec128<T>& v0, Vec128<T>& v1) {
  const Half<decltype(d)> dh;
  VFromD<decltype(dh)> v00, v10, v01, v11;
  LoadInterleaved2(dh, unaligned, v00, v10);
  LoadInterleaved2(dh, unaligned + 2, v01, v11);
  v0 = Combine(d, v01, v00);
  v1 = Combine(d, v11, v10);
}
#endif  // HWY_ARCH_ARM_V7

// ------------------------------ LoadInterleaved3

template <typename T, size_t N, HWY_IF_LOAD_INT(T, N)>
HWY_API void LoadInterleaved3(Simd<T, N, 0> /*tag*/,
                              const T* HWY_RESTRICT unaligned, Vec128<T, N>& v0,
                              Vec128<T, N>& v1, Vec128<T, N>& v2) {
  auto raw = detail::LoadInterleaved3(unaligned, detail::Tuple3<T, N>());
  v0 = Vec128<T, N>(raw.val[0]);
  v1 = Vec128<T, N>(raw.val[1]);
  v2 = Vec128<T, N>(raw.val[2]);
}

// <= 32 bits: avoid writing more than N bytes by copying to buffer
template <typename T, size_t N, HWY_IF_LE32(T, N)>
HWY_API void LoadInterleaved3(Simd<T, N, 0> /*tag*/,
                              const T* HWY_RESTRICT unaligned, Vec128<T, N>& v0,
                              Vec128<T, N>& v1, Vec128<T, N>& v2) {
  // The smallest vector registers are 64-bits and we want space for three.
  alignas(16) T buf[3 * 8 / sizeof(T)] = {};
  CopyBytes<N * 3 * sizeof(T)>(unaligned, buf);
  auto raw = detail::LoadInterleaved3(buf, detail::Tuple3<T, N>());
  v0 = Vec128<T, N>(raw.val[0]);
  v1 = Vec128<T, N>(raw.val[1]);
  v2 = Vec128<T, N>(raw.val[2]);
}

#if HWY_ARCH_ARM_V7
// 64x2: split into two 64x1
template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_API void LoadInterleaved3(Full128<T> d, const T* HWY_RESTRICT unaligned,
                              Vec128<T>& v0, Vec128<T>& v1, Vec128<T>& v2) {
  const Half<decltype(d)> dh;
  VFromD<decltype(dh)> v00, v10, v20, v01, v11, v21;
  LoadInterleaved3(dh, unaligned, v00, v10, v20);
  LoadInterleaved3(dh, unaligned + 3, v01, v11, v21);
  v0 = Combine(d, v01, v00);
  v1 = Combine(d, v11, v10);
  v2 = Combine(d, v21, v20);
}
#endif  // HWY_ARCH_ARM_V7

// ------------------------------ LoadInterleaved4

template <typename T, size_t N, HWY_IF_LOAD_INT(T, N)>
HWY_API void LoadInterleaved4(Simd<T, N, 0> /*tag*/,
                              const T* HWY_RESTRICT unaligned, Vec128<T, N>& v0,
                              Vec128<T, N>& v1, Vec128<T, N>& v2,
                              Vec128<T, N>& v3) {
  auto raw = detail::LoadInterleaved4(unaligned, detail::Tuple4<T, N>());
  v0 = Vec128<T, N>(raw.val[0]);
  v1 = Vec128<T, N>(raw.val[1]);
  v2 = Vec128<T, N>(raw.val[2]);
  v3 = Vec128<T, N>(raw.val[3]);
}

// <= 32 bits: avoid writing more than N bytes by copying to buffer
template <typename T, size_t N, HWY_IF_LE32(T, N)>
HWY_API void LoadInterleaved4(Simd<T, N, 0> /*tag*/,
                              const T* HWY_RESTRICT unaligned, Vec128<T, N>& v0,
                              Vec128<T, N>& v1, Vec128<T, N>& v2,
                              Vec128<T, N>& v3) {
  alignas(16) T buf[4 * 8 / sizeof(T)] = {};
  CopyBytes<N * 4 * sizeof(T)>(unaligned, buf);
  auto raw = detail::LoadInterleaved4(buf, detail::Tuple4<T, N>());
  v0 = Vec128<T, N>(raw.val[0]);
  v1 = Vec128<T, N>(raw.val[1]);
  v2 = Vec128<T, N>(raw.val[2]);
  v3 = Vec128<T, N>(raw.val[3]);
}

#if HWY_ARCH_ARM_V7
// 64x2: split into two 64x1
template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_API void LoadInterleaved4(Full128<T> d, const T* HWY_RESTRICT unaligned,
                              Vec128<T>& v0, Vec128<T>& v1, Vec128<T>& v2,
                              Vec128<T>& v3) {
  const Half<decltype(d)> dh;
  VFromD<decltype(dh)> v00, v10, v20, v30, v01, v11, v21, v31;
  LoadInterleaved4(dh, unaligned, v00, v10, v20, v30);
  LoadInterleaved4(dh, unaligned + 4, v01, v11, v21, v31);
  v0 = Combine(d, v01, v00);
  v1 = Combine(d, v11, v10);
  v2 = Combine(d, v21, v20);
  v3 = Combine(d, v31, v30);
}
#endif  // HWY_ARCH_ARM_V7

#undef HWY_IF_LOAD_INT

// ------------------------------ StoreInterleaved2

namespace detail {
#define HWY_NEON_BUILD_TPL_HWY_STORE_INT
#define HWY_NEON_BUILD_RET_HWY_STORE_INT(type, size) void
#define HWY_NEON_BUILD_ARG_HWY_STORE_INT to, tup.raw

#if HWY_ARCH_ARM_A64
#define HWY_IF_STORE_INT(T, N) HWY_IF_GE64(T, N)
#define HWY_NEON_DEF_FUNCTION_STORE_INT HWY_NEON_DEF_FUNCTION_ALL_TYPES
#else
// Exclude 64x2 and f64x1, which are only supported on aarch64
#define HWY_IF_STORE_INT(T, N) \
  hwy::EnableIf<N * sizeof(T) >= 8 && (N == 1 || sizeof(T) < 8)>* = nullptr
#define HWY_NEON_DEF_FUNCTION_STORE_INT(name, prefix, infix, args) \
  HWY_NEON_DEF_FUNCTION_INT_8_16_32(name, prefix, infix, args)     \
  HWY_NEON_DEF_FUNCTION_UINT_8_16_32(name, prefix, infix, args)    \
  HWY_NEON_DEF_FUNCTION_FLOAT_32(name, prefix, infix, args)        \
  HWY_NEON_DEF_FUNCTION(int64, 1, name, prefix, infix, s64, args)  \
  HWY_NEON_DEF_FUNCTION(uint64, 1, name, prefix, infix, u64, args)
#endif  // HWY_ARCH_ARM_A64

#define HWY_NEON_BUILD_PARAM_HWY_STORE_INT(type, size) \
  Tuple2<type##_t, size> tup, type##_t *to
HWY_NEON_DEF_FUNCTION_STORE_INT(StoreInterleaved2, vst2, _, HWY_STORE_INT)
#undef HWY_NEON_BUILD_PARAM_HWY_STORE_INT

#define HWY_NEON_BUILD_PARAM_HWY_STORE_INT(type, size) \
  Tuple3<type##_t, size> tup, type##_t *to
HWY_NEON_DEF_FUNCTION_STORE_INT(StoreInterleaved3, vst3, _, HWY_STORE_INT)
#undef HWY_NEON_BUILD_PARAM_HWY_STORE_INT

#define HWY_NEON_BUILD_PARAM_HWY_STORE_INT(type, size) \
  Tuple4<type##_t, size> tup, type##_t *to
HWY_NEON_DEF_FUNCTION_STORE_INT(StoreInterleaved4, vst4, _, HWY_STORE_INT)
#undef HWY_NEON_BUILD_PARAM_HWY_STORE_INT

#undef HWY_NEON_DEF_FUNCTION_STORE_INT
#undef HWY_NEON_BUILD_TPL_HWY_STORE_INT
#undef HWY_NEON_BUILD_RET_HWY_STORE_INT
#undef HWY_NEON_BUILD_ARG_HWY_STORE_INT
}  // namespace detail

template <typename T, size_t N, HWY_IF_STORE_INT(T, N)>
HWY_API void StoreInterleaved2(const Vec128<T, N> v0, const Vec128<T, N> v1,
                               Simd<T, N, 0> /*tag*/,
                               T* HWY_RESTRICT unaligned) {
  detail::Tuple2<T, N> tup = {{{v0.raw, v1.raw}}};
  detail::StoreInterleaved2(tup, unaligned);
}

// <= 32 bits: avoid writing more than N bytes by copying to buffer
template <typename T, size_t N, HWY_IF_LE32(T, N)>
HWY_API void StoreInterleaved2(const Vec128<T, N> v0, const Vec128<T, N> v1,
                               Simd<T, N, 0> /*tag*/,
                               T* HWY_RESTRICT unaligned) {
  alignas(16) T buf[2 * 8 / sizeof(T)];
  detail::Tuple2<T, N> tup = {{{v0.raw, v1.raw}}};
  detail::StoreInterleaved2(tup, buf);
  CopyBytes<N * 2 * sizeof(T)>(buf, unaligned);
}

#if HWY_ARCH_ARM_V7
// 64x2: split into two 64x1
template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_API void StoreInterleaved2(const Vec128<T> v0, const Vec128<T> v1,
                               Full128<T> d, T* HWY_RESTRICT unaligned) {
  const Half<decltype(d)> dh;
  StoreInterleaved2(LowerHalf(dh, v0), LowerHalf(dh, v1), dh, unaligned);
  StoreInterleaved2(UpperHalf(dh, v0), UpperHalf(dh, v1), dh, unaligned + 2);
}
#endif  // HWY_ARCH_ARM_V7

// ------------------------------ StoreInterleaved3

template <typename T, size_t N, HWY_IF_STORE_INT(T, N)>
HWY_API void StoreInterleaved3(const Vec128<T, N> v0, const Vec128<T, N> v1,
                               const Vec128<T, N> v2, Simd<T, N, 0> /*tag*/,
                               T* HWY_RESTRICT unaligned) {
  detail::Tuple3<T, N> tup = {{{v0.raw, v1.raw, v2.raw}}};
  detail::StoreInterleaved3(tup, unaligned);
}

// <= 32 bits: avoid writing more than N bytes by copying to buffer
template <typename T, size_t N, HWY_IF_LE32(T, N)>
HWY_API void StoreInterleaved3(const Vec128<T, N> v0, const Vec128<T, N> v1,
                               const Vec128<T, N> v2, Simd<T, N, 0> /*tag*/,
                               T* HWY_RESTRICT unaligned) {
  alignas(16) T buf[3 * 8 / sizeof(T)];
  detail::Tuple3<T, N> tup = {{{v0.raw, v1.raw, v2.raw}}};
  detail::StoreInterleaved3(tup, buf);
  CopyBytes<N * 3 * sizeof(T)>(buf, unaligned);
}

#if HWY_ARCH_ARM_V7
// 64x2: split into two 64x1
template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_API void StoreInterleaved3(const Vec128<T> v0, const Vec128<T> v1,
                               const Vec128<T> v2, Full128<T> d,
                               T* HWY_RESTRICT unaligned) {
  const Half<decltype(d)> dh;
  StoreInterleaved3(LowerHalf(dh, v0), LowerHalf(dh, v1), LowerHalf(dh, v2), dh,
                    unaligned);
  StoreInterleaved3(UpperHalf(dh, v0), UpperHalf(dh, v1), UpperHalf(dh, v2), dh,
                    unaligned + 3);
}
#endif  // HWY_ARCH_ARM_V7

// ------------------------------ StoreInterleaved4

template <typename T, size_t N, HWY_IF_STORE_INT(T, N)>
HWY_API void StoreInterleaved4(const Vec128<T, N> v0, const Vec128<T, N> v1,
                               const Vec128<T, N> v2, const Vec128<T, N> v3,
                               Simd<T, N, 0> /*tag*/,
                               T* HWY_RESTRICT unaligned) {
  detail::Tuple4<T, N> tup = {{{v0.raw, v1.raw, v2.raw, v3.raw}}};
  detail::StoreInterleaved4(tup, unaligned);
}

// <= 32 bits: avoid writing more than N bytes by copying to buffer
template <typename T, size_t N, HWY_IF_LE32(T, N)>
HWY_API void StoreInterleaved4(const Vec128<T, N> v0, const Vec128<T, N> v1,
                               const Vec128<T, N> v2, const Vec128<T, N> v3,
                               Simd<T, N, 0> /*tag*/,
                               T* HWY_RESTRICT unaligned) {
  alignas(16) T buf[4 * 8 / sizeof(T)];
  detail::Tuple4<T, N> tup = {{{v0.raw, v1.raw, v2.raw, v3.raw}}};
  detail::StoreInterleaved4(tup, buf);
  CopyBytes<N * 4 * sizeof(T)>(buf, unaligned);
}

#if HWY_ARCH_ARM_V7
// 64x2: split into two 64x1
template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_API void StoreInterleaved4(const Vec128<T> v0, const Vec128<T> v1,
                               const Vec128<T> v2, const Vec128<T> v3,
                               Full128<T> d, T* HWY_RESTRICT unaligned) {
  const Half<decltype(d)> dh;
  StoreInterleaved4(LowerHalf(dh, v0), LowerHalf(dh, v1), LowerHalf(dh, v2),
                    LowerHalf(dh, v3), dh, unaligned);
  StoreInterleaved4(UpperHalf(dh, v0), UpperHalf(dh, v1), UpperHalf(dh, v2),
                    UpperHalf(dh, v3), dh, unaligned + 4);
}
#endif  // HWY_ARCH_ARM_V7

#undef HWY_IF_STORE_INT

// ------------------------------ Lt128

template <typename T, size_t N, HWY_IF_LE128(T, N)>
HWY_INLINE Mask128<T, N> Lt128(Simd<T, N, 0> d, Vec128<T, N> a,
                               Vec128<T, N> b) {
  static_assert(!IsSigned<T>() && sizeof(T) == 8, "Use u64");
  // Truth table of Eq and Lt for Hi and Lo u64.
  // (removed lines with (=H && cH) or (=L && cL) - cannot both be true)
  // =H =L cH cL  | out = cH | (=H & cL)
  //  0  0  0  0  |  0
  //  0  0  0  1  |  0
  //  0  0  1  0  |  1
  //  0  0  1  1  |  1
  //  0  1  0  0  |  0
  //  0  1  0  1  |  0
  //  0  1  1  0  |  1
  //  1  0  0  0  |  0
  //  1  0  0  1  |  1
  //  1  1  0  0  |  0
  const Mask128<T, N> eqHL = Eq(a, b);
  const Vec128<T, N> ltHL = VecFromMask(d, Lt(a, b));
  // We need to bring cL to the upper lane/bit corresponding to cH. Comparing
  // the result of InterleaveUpper/Lower requires 9 ops, whereas shifting the
  // comparison result leftwards requires only 4. IfThenElse compiles to the
  // same code as OrAnd().
  const Vec128<T, N> ltLx = DupEven(ltHL);
  const Vec128<T, N> outHx = IfThenElse(eqHL, ltLx, ltHL);
  return MaskFromVec(DupOdd(outHx));
}

template <typename T, size_t N, HWY_IF_LE128(T, N)>
HWY_INLINE Mask128<T, N> Lt128Upper(Simd<T, N, 0> d, Vec128<T, N> a,
                                    Vec128<T, N> b) {
  const Vec128<T, N> ltHL = VecFromMask(d, Lt(a, b));
  return MaskFromVec(InterleaveUpper(d, ltHL, ltHL));
}

// ------------------------------ Eq128

template <typename T, size_t N, HWY_IF_LE128(T, N)>
HWY_INLINE Mask128<T, N> Eq128(Simd<T, N, 0> d, Vec128<T, N> a,
                               Vec128<T, N> b) {
  static_assert(!IsSigned<T>() && sizeof(T) == 8, "Use u64");
  const Vec128<T, N> eqHL = VecFromMask(d, Eq(a, b));
  return MaskFromVec(And(Reverse2(d, eqHL), eqHL));
}

template <typename T, size_t N, HWY_IF_LE128(T, N)>
HWY_INLINE Mask128<T, N> Eq128Upper(Simd<T, N, 0> d, Vec128<T, N> a,
                                    Vec128<T, N> b) {
  const Vec128<T, N> eqHL = VecFromMask(d, Eq(a, b));
  return MaskFromVec(InterleaveUpper(d, eqHL, eqHL));
}

// ------------------------------ Min128, Max128 (Lt128)

// Without a native OddEven, it seems infeasible to go faster than Lt128.
template <class D>
HWY_INLINE VFromD<D> Min128(D d, const VFromD<D> a, const VFromD<D> b) {
  return IfThenElse(Lt128(d, a, b), a, b);
}

template <class D>
HWY_INLINE VFromD<D> Max128(D d, const VFromD<D> a, const VFromD<D> b) {
  return IfThenElse(Lt128(d, b, a), a, b);
}

template <class D>
HWY_INLINE VFromD<D> Min128Upper(D d, const VFromD<D> a, const VFromD<D> b) {
  return IfThenElse(Lt128Upper(d, a, b), a, b);
}

template <class D>
HWY_INLINE VFromD<D> Max128Upper(D d, const VFromD<D> a, const VFromD<D> b) {
  return IfThenElse(Lt128Upper(d, b, a), a, b);
}

// ================================================== Operator wrapper

// These apply to all x86_*-inl.h because there are no restrictions on V.

template <class V>
HWY_API V Add(V a, V b) {
  return a + b;
}
template <class V>
HWY_API V Sub(V a, V b) {
  return a - b;
}

template <class V>
HWY_API V Mul(V a, V b) {
  return a * b;
}
template <class V>
HWY_API V Div(V a, V b) {
  return a / b;
}

template <class V>
V Shl(V a, V b) {
  return a << b;
}
template <class V>
V Shr(V a, V b) {
  return a >> b;
}

template <class V>
HWY_API auto Eq(V a, V b) -> decltype(a == b) {
  return a == b;
}
template <class V>
HWY_API auto Ne(V a, V b) -> decltype(a == b) {
  return a != b;
}
template <class V>
HWY_API auto Lt(V a, V b) -> decltype(a == b) {
  return a < b;
}

template <class V>
HWY_API auto Gt(V a, V b) -> decltype(a == b) {
  return a > b;
}
template <class V>
HWY_API auto Ge(V a, V b) -> decltype(a == b) {
  return a >= b;
}

template <class V>
HWY_API auto Le(V a, V b) -> decltype(a == b) {
  return a <= b;
}

namespace detail {  // for code folding
#if HWY_ARCH_ARM_V7
#undef vuzp1_s8
#undef vuzp1_u8
#undef vuzp1_s16
#undef vuzp1_u16
#undef vuzp1_s32
#undef vuzp1_u32
#undef vuzp1_f32
#undef vuzp1q_s8
#undef vuzp1q_u8
#undef vuzp1q_s16
#undef vuzp1q_u16
#undef vuzp1q_s32
#undef vuzp1q_u32
#undef vuzp1q_f32
#undef vuzp2_s8
#undef vuzp2_u8
#undef vuzp2_s16
#undef vuzp2_u16
#undef vuzp2_s32
#undef vuzp2_u32
#undef vuzp2_f32
#undef vuzp2q_s8
#undef vuzp2q_u8
#undef vuzp2q_s16
#undef vuzp2q_u16
#undef vuzp2q_s32
#undef vuzp2q_u32
#undef vuzp2q_f32
#undef vzip1_s8
#undef vzip1_u8
#undef vzip1_s16
#undef vzip1_u16
#undef vzip1_s32
#undef vzip1_u32
#undef vzip1_f32
#undef vzip1q_s8
#undef vzip1q_u8
#undef vzip1q_s16
#undef vzip1q_u16
#undef vzip1q_s32
#undef vzip1q_u32
#undef vzip1q_f32
#undef vzip2_s8
#undef vzip2_u8
#undef vzip2_s16
#undef vzip2_u16
#undef vzip2_s32
#undef vzip2_u32
#undef vzip2_f32
#undef vzip2q_s8
#undef vzip2q_u8
#undef vzip2q_s16
#undef vzip2q_u16
#undef vzip2q_s32
#undef vzip2q_u32
#undef vzip2q_f32
#endif

#undef HWY_NEON_BUILD_ARG_1
#undef HWY_NEON_BUILD_ARG_2
#undef HWY_NEON_BUILD_ARG_3
#undef HWY_NEON_BUILD_PARAM_1
#undef HWY_NEON_BUILD_PARAM_2
#undef HWY_NEON_BUILD_PARAM_3
#undef HWY_NEON_BUILD_RET_1
#undef HWY_NEON_BUILD_RET_2
#undef HWY_NEON_BUILD_RET_3
#undef HWY_NEON_BUILD_TPL_1
#undef HWY_NEON_BUILD_TPL_2
#undef HWY_NEON_BUILD_TPL_3
#undef HWY_NEON_DEF_FUNCTION
#undef HWY_NEON_DEF_FUNCTION_ALL_FLOATS
#undef HWY_NEON_DEF_FUNCTION_ALL_TYPES
#undef HWY_NEON_DEF_FUNCTION_FLOAT_64
#undef HWY_NEON_DEF_FUNCTION_INTS
#undef HWY_NEON_DEF_FUNCTION_INTS_UINTS
#undef HWY_NEON_DEF_FUNCTION_INT_16
#undef HWY_NEON_DEF_FUNCTION_INT_32
#undef HWY_NEON_DEF_FUNCTION_INT_8
#undef HWY_NEON_DEF_FUNCTION_INT_8_16_32
#undef HWY_NEON_DEF_FUNCTION_TPL
#undef HWY_NEON_DEF_FUNCTION_UIF81632
#undef HWY_NEON_DEF_FUNCTION_UINTS
#undef HWY_NEON_DEF_FUNCTION_UINT_16
#undef HWY_NEON_DEF_FUNCTION_UINT_32
#undef HWY_NEON_DEF_FUNCTION_UINT_8
#undef HWY_NEON_DEF_FUNCTION_UINT_8_16_32
#undef HWY_NEON_EVAL
}  // namespace detail

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();
