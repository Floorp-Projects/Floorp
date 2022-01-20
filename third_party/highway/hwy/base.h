// Copyright 2020 Google LLC
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

#ifndef HIGHWAY_HWY_BASE_H_
#define HIGHWAY_HWY_BASE_H_

// For SIMD module implementations and their callers, target-independent.

#include <stddef.h>
#include <stdint.h>

#include <atomic>
#include <cfloat>

#include "hwy/detect_compiler_arch.h"

//------------------------------------------------------------------------------
// Compiler-specific definitions

#define HWY_STR_IMPL(macro) #macro
#define HWY_STR(macro) HWY_STR_IMPL(macro)

#if HWY_COMPILER_MSVC

#include <intrin.h>

#define HWY_RESTRICT __restrict
#define HWY_INLINE __forceinline
#define HWY_NOINLINE __declspec(noinline)
#define HWY_FLATTEN
#define HWY_NORETURN __declspec(noreturn)
#define HWY_LIKELY(expr) (expr)
#define HWY_UNLIKELY(expr) (expr)
#define HWY_PRAGMA(tokens) __pragma(tokens)
#define HWY_DIAGNOSTICS(tokens) HWY_PRAGMA(warning(tokens))
#define HWY_DIAGNOSTICS_OFF(msc, gcc) HWY_DIAGNOSTICS(msc)
#define HWY_MAYBE_UNUSED
#define HWY_HAS_ASSUME_ALIGNED 0
#if (_MSC_VER >= 1700)
#define HWY_MUST_USE_RESULT _Check_return_
#else
#define HWY_MUST_USE_RESULT
#endif

#else

#define HWY_RESTRICT __restrict__
#define HWY_INLINE inline __attribute__((always_inline))
#define HWY_NOINLINE __attribute__((noinline))
#define HWY_FLATTEN __attribute__((flatten))
#define HWY_NORETURN __attribute__((noreturn))
#define HWY_LIKELY(expr) __builtin_expect(!!(expr), 1)
#define HWY_UNLIKELY(expr) __builtin_expect(!!(expr), 0)
#define HWY_PRAGMA(tokens) _Pragma(#tokens)
#define HWY_DIAGNOSTICS(tokens) HWY_PRAGMA(GCC diagnostic tokens)
#define HWY_DIAGNOSTICS_OFF(msc, gcc) HWY_DIAGNOSTICS(gcc)
// Encountered "attribute list cannot appear here" when using the C++17
// [[maybe_unused]], so only use the old style attribute for now.
#define HWY_MAYBE_UNUSED __attribute__((unused))
#define HWY_MUST_USE_RESULT __attribute__((warn_unused_result))

#endif  // !HWY_COMPILER_MSVC

//------------------------------------------------------------------------------
// Builtin/attributes

// Enables error-checking of format strings.
#if HWY_HAS_ATTRIBUTE(__format__)
#define HWY_FORMAT(idx_fmt, idx_arg) \
  __attribute__((__format__(__printf__, idx_fmt, idx_arg)))
#else
#define HWY_FORMAT(idx_fmt, idx_arg)
#endif

// Returns a void* pointer which the compiler then assumes is N-byte aligned.
// Example: float* HWY_RESTRICT aligned = (float*)HWY_ASSUME_ALIGNED(in, 32);
//
// The assignment semantics are required by GCC/Clang. ICC provides an in-place
// __assume_aligned, whereas MSVC's __assume appears unsuitable.
#if HWY_HAS_BUILTIN(__builtin_assume_aligned)
#define HWY_ASSUME_ALIGNED(ptr, align) __builtin_assume_aligned((ptr), (align))
#else
#define HWY_ASSUME_ALIGNED(ptr, align) (ptr) /* not supported */
#endif

// Clang and GCC require attributes on each function into which SIMD intrinsics
// are inlined. Support both per-function annotation (HWY_ATTR) for lambdas and
// automatic annotation via pragmas.
#if HWY_COMPILER_CLANG
#define HWY_PUSH_ATTRIBUTES(targets_str)                                \
  HWY_PRAGMA(clang attribute push(__attribute__((target(targets_str))), \
                                  apply_to = function))
#define HWY_POP_ATTRIBUTES HWY_PRAGMA(clang attribute pop)
#elif HWY_COMPILER_GCC
#define HWY_PUSH_ATTRIBUTES(targets_str) \
  HWY_PRAGMA(GCC push_options) HWY_PRAGMA(GCC target targets_str)
#define HWY_POP_ATTRIBUTES HWY_PRAGMA(GCC pop_options)
#else
#define HWY_PUSH_ATTRIBUTES(targets_str)
#define HWY_POP_ATTRIBUTES
#endif

//------------------------------------------------------------------------------
// Macros

#define HWY_API static HWY_INLINE HWY_FLATTEN HWY_MAYBE_UNUSED

#define HWY_CONCAT_IMPL(a, b) a##b
#define HWY_CONCAT(a, b) HWY_CONCAT_IMPL(a, b)

#define HWY_MIN(a, b) ((a) < (b) ? (a) : (b))
#define HWY_MAX(a, b) ((a) > (b) ? (a) : (b))

// Compile-time fence to prevent undesirable code reordering. On Clang x86, the
// typical asm volatile("" : : : "memory") has no effect, whereas atomic fence
// does, without generating code.
#if HWY_ARCH_X86
#define HWY_FENCE std::atomic_thread_fence(std::memory_order_acq_rel)
#else
// TODO(janwas): investigate alternatives. On ARM, the above generates barriers.
#define HWY_FENCE
#endif

// 4 instances of a given literal value, useful as input to LoadDup128.
#define HWY_REP4(literal) literal, literal, literal, literal

#define HWY_ABORT(format, ...) \
  ::hwy::Abort(__FILE__, __LINE__, format, ##__VA_ARGS__)

// Always enabled.
#define HWY_ASSERT(condition)             \
  do {                                    \
    if (!(condition)) {                   \
      HWY_ABORT("Assert %s", #condition); \
    }                                     \
  } while (0)

#if HWY_HAS_FEATURE(memory_sanitizer) || defined(MEMORY_SANITIZER)
#define HWY_IS_MSAN 1
#else
#define HWY_IS_MSAN 0
#endif

#if HWY_HAS_FEATURE(address_sanitizer) || defined(ADDRESS_SANITIZER)
#define HWY_IS_ASAN 1
#else
#define HWY_IS_ASAN 0
#endif

#if HWY_HAS_FEATURE(thread_sanitizer) || defined(THREAD_SANITIZER)
#define HWY_IS_TSAN 1
#else
#define HWY_IS_TSAN 0
#endif

// For enabling HWY_DASSERT and shortening tests in slower debug builds
#if !defined(HWY_IS_DEBUG_BUILD)
// Clang does not define NDEBUG, but it and GCC define __OPTIMIZE__, and recent
// MSVC defines NDEBUG (if not, could instead check _DEBUG).
#if (!defined(__OPTIMIZE__) && !defined(NDEBUG)) || HWY_IS_ASAN || \
    HWY_IS_MSAN || HWY_IS_TSAN || defined(__clang_analyzer__)
#define HWY_IS_DEBUG_BUILD 1
#else
#define HWY_IS_DEBUG_BUILD 0
#endif
#endif  // HWY_IS_DEBUG_BUILD

#if HWY_IS_DEBUG_BUILD
#define HWY_DASSERT(condition) HWY_ASSERT(condition)
#else
#define HWY_DASSERT(condition) \
  do {                         \
  } while (0)
#endif

#if defined(HWY_EMULATE_SVE)
class FarmFloat16;
#endif

namespace hwy {

//------------------------------------------------------------------------------
// kMaxVectorSize (undocumented, pending removal)

#if HWY_ARCH_X86
static constexpr HWY_MAYBE_UNUSED size_t kMaxVectorSize = 64;  // AVX-512
#elif HWY_ARCH_RVV && defined(__riscv_vector)
// Not actually an upper bound on the size.
static constexpr HWY_MAYBE_UNUSED size_t kMaxVectorSize = 4096;
#else
static constexpr HWY_MAYBE_UNUSED size_t kMaxVectorSize = 16;
#endif

//------------------------------------------------------------------------------
// Alignment

// For stack-allocated partial arrays or LoadDup128.
#if HWY_ARCH_X86
#define HWY_ALIGN_MAX alignas(64)
#elif HWY_ARCH_RVV && defined(__riscv_vector)
#define HWY_ALIGN_MAX alignas(8)  // only elements need be aligned
#else
#define HWY_ALIGN_MAX alignas(16)
#endif

//------------------------------------------------------------------------------
// Lane types

// Match [u]int##_t naming scheme so rvv-inl.h macros can obtain the type name
// by concatenating base type and bits.

#if HWY_ARCH_ARM && (__ARM_FP & 2)
#define HWY_NATIVE_FLOAT16 1
#else
#define HWY_NATIVE_FLOAT16 0
#endif

#pragma pack(push, 1)

#if defined(HWY_EMULATE_SVE)
using float16_t = FarmFloat16;
#elif HWY_NATIVE_FLOAT16
using float16_t = __fp16;
// Clang does not allow __fp16 arguments, but scalar.h requires LaneType
// arguments, so use a wrapper.
// TODO(janwas): replace with _Float16 when that is supported?
#else
struct float16_t {
  uint16_t bits;
};
#endif

struct bfloat16_t {
  uint16_t bits;
};

#pragma pack(pop)

using float32_t = float;
using float64_t = double;

//------------------------------------------------------------------------------
// Controlling overload resolution (SFINAE)

template <bool Condition, class T>
struct EnableIfT {};
template <class T>
struct EnableIfT<true, T> {
  using type = T;
};

template <bool Condition, class T = void>
using EnableIf = typename EnableIfT<Condition, T>::type;

template <typename T, typename U>
struct IsSameT {
  enum { value = 0 };
};

template <typename T>
struct IsSameT<T, T> {
  enum { value = 1 };
};

template <typename T, typename U>
HWY_API constexpr bool IsSame() {
  return IsSameT<T, U>::value;
}

// Insert into template/function arguments to enable this overload only for
// vectors of AT MOST this many bits.
//
// Note that enabling for exactly 128 bits is unnecessary because a function can
// simply be overloaded with Vec128<T> and/or Full128<T> tag. Enabling for other
// sizes (e.g. 64 bit) can be achieved via Simd<T, 8 / sizeof(T)>.
#define HWY_IF_LE128(T, N) hwy::EnableIf<N * sizeof(T) <= 16>* = nullptr
#define HWY_IF_LE64(T, N) hwy::EnableIf<N * sizeof(T) <= 8>* = nullptr
#define HWY_IF_LE32(T, N) hwy::EnableIf<N * sizeof(T) <= 4>* = nullptr
#define HWY_IF_GE32(T, N) hwy::EnableIf<N * sizeof(T) >= 4>* = nullptr
#define HWY_IF_GE64(T, N) hwy::EnableIf<N * sizeof(T) >= 8>* = nullptr
#define HWY_IF_GE128(T, N) hwy::EnableIf<N * sizeof(T) >= 16>* = nullptr
#define HWY_IF_GT128(T, N) hwy::EnableIf<(N * sizeof(T) > 16)>* = nullptr

#define HWY_IF_UNSIGNED(T) hwy::EnableIf<!IsSigned<T>()>* = nullptr
#define HWY_IF_SIGNED(T) \
  hwy::EnableIf<IsSigned<T>() && !IsFloat<T>()>* = nullptr
#define HWY_IF_FLOAT(T) hwy::EnableIf<hwy::IsFloat<T>()>* = nullptr
#define HWY_IF_NOT_FLOAT(T) hwy::EnableIf<!hwy::IsFloat<T>()>* = nullptr

#define HWY_IF_LANE_SIZE(T, bytes) \
  hwy::EnableIf<sizeof(T) == (bytes)>* = nullptr
#define HWY_IF_NOT_LANE_SIZE(T, bytes) \
  hwy::EnableIf<sizeof(T) != (bytes)>* = nullptr

// Empty struct used as a size tag type.
template <size_t N>
struct SizeTag {};

template <class T>
struct RemoveConstT {
  using type = T;
};
template <class T>
struct RemoveConstT<const T> {
  using type = T;
};

template <class T>
using RemoveConst = typename RemoveConstT<T>::type;

//------------------------------------------------------------------------------
// Type traits

template <typename T>
HWY_API constexpr bool IsFloat() {
  // Cannot use T(1.25) != T(1) for float16_t, which can only be converted to or
  // from a float, not compared.
  return IsSame<T, float>() || IsSame<T, double>();
}

template <typename T>
HWY_API constexpr bool IsSigned() {
  return T(0) > T(-1);
}
template <>
constexpr bool IsSigned<float16_t>() {
  return true;
}
template <>
constexpr bool IsSigned<bfloat16_t>() {
  return true;
}

// Largest/smallest representable integer values.
template <typename T>
HWY_API constexpr T LimitsMax() {
  static_assert(!IsFloat<T>(), "Only for integer types");
  return IsSigned<T>() ? T((1ULL << (sizeof(T) * 8 - 1)) - 1)
                       : static_cast<T>(~0ull);
}
template <typename T>
HWY_API constexpr T LimitsMin() {
  static_assert(!IsFloat<T>(), "Only for integer types");
  return IsSigned<T>() ? T(-1) - LimitsMax<T>() : T(0);
}

// Largest/smallest representable value (integer or float). This naming avoids
// confusion with numeric_limits<float>::min() (the smallest positive value).
template <typename T>
HWY_API constexpr T LowestValue() {
  return LimitsMin<T>();
}
template <>
constexpr float LowestValue<float>() {
  return -FLT_MAX;
}
template <>
constexpr double LowestValue<double>() {
  return -DBL_MAX;
}

template <typename T>
HWY_API constexpr T HighestValue() {
  return LimitsMax<T>();
}
template <>
constexpr float HighestValue<float>() {
  return FLT_MAX;
}
template <>
constexpr double HighestValue<double>() {
  return DBL_MAX;
}

// Returns bitmask of the exponent field in IEEE binary32/64.
template <typename T>
constexpr T ExponentMask() {
  static_assert(sizeof(T) == 0, "Only instantiate the specializations");
  return 0;
}
template <>
constexpr uint32_t ExponentMask<uint32_t>() {
  return 0x7F800000;
}
template <>
constexpr uint64_t ExponentMask<uint64_t>() {
  return 0x7FF0000000000000ULL;
}

// Returns 1 << mantissa_bits as a floating-point number. All integers whose
// absolute value are less than this can be represented exactly.
template <typename T>
constexpr T MantissaEnd() {
  static_assert(sizeof(T) == 0, "Only instantiate the specializations");
  return 0;
}
template <>
constexpr float MantissaEnd<float>() {
  return 8388608.0f;  // 1 << 23
}
template <>
constexpr double MantissaEnd<double>() {
  // floating point literal with p52 requires C++17.
  return 4503599627370496.0;  // 1 << 52
}

//------------------------------------------------------------------------------
// Type relations

namespace detail {

template <typename T>
struct Relations;
template <>
struct Relations<uint8_t> {
  using Unsigned = uint8_t;
  using Signed = int8_t;
  using Wide = uint16_t;
};
template <>
struct Relations<int8_t> {
  using Unsigned = uint8_t;
  using Signed = int8_t;
  using Wide = int16_t;
};
template <>
struct Relations<uint16_t> {
  using Unsigned = uint16_t;
  using Signed = int16_t;
  using Wide = uint32_t;
  using Narrow = uint8_t;
};
template <>
struct Relations<int16_t> {
  using Unsigned = uint16_t;
  using Signed = int16_t;
  using Wide = int32_t;
  using Narrow = int8_t;
};
template <>
struct Relations<uint32_t> {
  using Unsigned = uint32_t;
  using Signed = int32_t;
  using Float = float;
  using Wide = uint64_t;
  using Narrow = uint16_t;
};
template <>
struct Relations<int32_t> {
  using Unsigned = uint32_t;
  using Signed = int32_t;
  using Float = float;
  using Wide = int64_t;
  using Narrow = int16_t;
};
template <>
struct Relations<uint64_t> {
  using Unsigned = uint64_t;
  using Signed = int64_t;
  using Float = double;
  using Narrow = uint32_t;
};
template <>
struct Relations<int64_t> {
  using Unsigned = uint64_t;
  using Signed = int64_t;
  using Float = double;
  using Narrow = int32_t;
};
template <>
struct Relations<float16_t> {
  using Unsigned = uint16_t;
  using Signed = int16_t;
  using Float = float16_t;
  using Wide = float;
};
template <>
struct Relations<bfloat16_t> {
  using Unsigned = uint16_t;
  using Signed = int16_t;
  using Wide = float;
};
template <>
struct Relations<float> {
  using Unsigned = uint32_t;
  using Signed = int32_t;
  using Float = float;
  using Wide = double;
  using Narrow = float16_t;
};
template <>
struct Relations<double> {
  using Unsigned = uint64_t;
  using Signed = int64_t;
  using Float = double;
  using Narrow = float;
};

template <size_t N>
struct TypeFromSize;
template <>
struct TypeFromSize<1> {
  using Unsigned = uint8_t;
  using Signed = int8_t;
};
template <>
struct TypeFromSize<2> {
  using Unsigned = uint16_t;
  using Signed = int16_t;
};
template <>
struct TypeFromSize<4> {
  using Unsigned = uint32_t;
  using Signed = int32_t;
  using Float = float;
};
template <>
struct TypeFromSize<8> {
  using Unsigned = uint64_t;
  using Signed = int64_t;
  using Float = double;
};

}  // namespace detail

// Aliases for types of a different category, but the same size.
template <typename T>
using MakeUnsigned = typename detail::Relations<T>::Unsigned;
template <typename T>
using MakeSigned = typename detail::Relations<T>::Signed;
template <typename T>
using MakeFloat = typename detail::Relations<T>::Float;

// Aliases for types of the same category, but different size.
template <typename T>
using MakeWide = typename detail::Relations<T>::Wide;
template <typename T>
using MakeNarrow = typename detail::Relations<T>::Narrow;

// Obtain type from its size [bytes].
template <size_t N>
using UnsignedFromSize = typename detail::TypeFromSize<N>::Unsigned;
template <size_t N>
using SignedFromSize = typename detail::TypeFromSize<N>::Signed;
template <size_t N>
using FloatFromSize = typename detail::TypeFromSize<N>::Float;

//------------------------------------------------------------------------------
// Helper functions

template <typename T1, typename T2>
constexpr inline T1 DivCeil(T1 a, T2 b) {
  return (a + b - 1) / b;
}

// Works for any `align`; if a power of two, compiler emits ADD+AND.
constexpr inline size_t RoundUpTo(size_t what, size_t align) {
  return DivCeil(what, align) * align;
}

// Undefined results for x == 0.
HWY_API size_t Num0BitsBelowLS1Bit_Nonzero32(const uint32_t x) {
#if HWY_COMPILER_MSVC
  unsigned long index;  // NOLINT
  _BitScanForward(&index, x);
  return index;
#else   // HWY_COMPILER_MSVC
  return static_cast<size_t>(__builtin_ctz(x));
#endif  // HWY_COMPILER_MSVC
}

HWY_API size_t Num0BitsBelowLS1Bit_Nonzero64(const uint64_t x) {
#if HWY_COMPILER_MSVC
#if HWY_ARCH_X86_64
  unsigned long index;  // NOLINT
  _BitScanForward64(&index, x);
  return index;
#else   // HWY_ARCH_X86_64
  // _BitScanForward64 not available
  uint32_t lsb = static_cast<uint32_t>(x & 0xFFFFFFFF);
  unsigned long index;
  if (lsb == 0) {
    uint32_t msb = static_cast<uint32_t>(x >> 32u);
    _BitScanForward(&index, msb);
    return 32 + index;
  } else {
    _BitScanForward(&index, lsb);
    return index;
  }
#endif  // HWY_ARCH_X86_64
#else   // HWY_COMPILER_MSVC
  return static_cast<size_t>(__builtin_ctzll(x));
#endif  // HWY_COMPILER_MSVC
}

// Undefined results for x == 0.
HWY_API size_t Num0BitsAboveMS1Bit_Nonzero32(const uint32_t x) {
#if HWY_COMPILER_MSVC
  unsigned long index;  // NOLINT
  _BitScanReverse(&index, x);
  return 31 - index;
#else   // HWY_COMPILER_MSVC
  return static_cast<size_t>(__builtin_clz(x));
#endif  // HWY_COMPILER_MSVC
}

HWY_API size_t Num0BitsAboveMS1Bit_Nonzero64(const uint64_t x) {
#if HWY_COMPILER_MSVC
#if HWY_ARCH_X86_64
  unsigned long index;  // NOLINT
  _BitScanReverse64(&index, x);
  return 63 - index;
#else   // HWY_ARCH_X86_64
  // _BitScanReverse64 not available
  const uint32_t msb = static_cast<uint32_t>(x >> 32u);
  unsigned long index;
  if (msb == 0) {
    const uint32_t lsb = static_cast<uint32_t>(x & 0xFFFFFFFF);
    _BitScanReverse(&index, lsb);
    return 63 - index;
  } else {
    _BitScanReverse(&index, msb);
    return 31 - index;
  }
#endif  // HWY_ARCH_X86_64
#else   // HWY_COMPILER_MSVC
  return static_cast<size_t>(__builtin_clzll(x));
#endif  // HWY_COMPILER_MSVC
}

HWY_API size_t PopCount(uint64_t x) {
#if HWY_COMPILER_CLANG || HWY_COMPILER_GCC
  return static_cast<size_t>(__builtin_popcountll(x));
  // This instruction has a separate feature flag, but is often called from
  // non-SIMD code, so we don't want to require dynamic dispatch. It was first
  // supported by Intel in Nehalem (SSE4.2), but MSVC only predefines a macro
  // for AVX, so check for that.
#elif HWY_COMPILER_MSVC && HWY_ARCH_X86_64 && defined(__AVX__)
  return _mm_popcnt_u64(x);
#elif HWY_COMPILER_MSVC && HWY_ARCH_X86_32 && defined(__AVX__)
  return _mm_popcnt_u32(uint32_t(x)) + _mm_popcnt_u32(uint32_t(x >> 32));
#else
  x -= ((x >> 1) & 0x5555555555555555ULL);
  x = (((x >> 2) & 0x3333333333333333ULL) + (x & 0x3333333333333333ULL));
  x = (((x >> 4) + x) & 0x0F0F0F0F0F0F0F0FULL);
  x += (x >> 8);
  x += (x >> 16);
  x += (x >> 32);
  return static_cast<size_t>(x & 0x7Fu);
#endif
}

template <typename TI>
HWY_API constexpr size_t FloorLog2(TI x) {
  return x == 1 ? 0 : FloorLog2(x >> 1) + 1;
}

template <typename TI>
HWY_API constexpr size_t CeilLog2(TI x) {
  return x == 1 ? 0 : FloorLog2(x - 1) + 1;
}

#if HWY_COMPILER_MSVC && HWY_ARCH_X86_64
#pragma intrinsic(_umul128)
#endif

// 64 x 64 = 128 bit multiplication
HWY_API uint64_t Mul128(uint64_t a, uint64_t b, uint64_t* HWY_RESTRICT upper) {
#if defined(__SIZEOF_INT128__)
  __uint128_t product = (__uint128_t)a * (__uint128_t)b;
  *upper = (uint64_t)(product >> 64);
  return (uint64_t)(product & 0xFFFFFFFFFFFFFFFFULL);
#elif HWY_COMPILER_MSVC && HWY_ARCH_X86_64
  return _umul128(a, b, upper);
#else
  constexpr uint64_t kLo32 = 0xFFFFFFFFU;
  const uint64_t lo_lo = (a & kLo32) * (b & kLo32);
  const uint64_t hi_lo = (a >> 32) * (b & kLo32);
  const uint64_t lo_hi = (a & kLo32) * (b >> 32);
  const uint64_t hi_hi = (a >> 32) * (b >> 32);
  const uint64_t t = (lo_lo >> 32) + (hi_lo & kLo32) + lo_hi;
  *upper = (hi_lo >> 32) + (t >> 32) + hi_hi;
  return (t << 32) | (lo_lo & kLo32);
#endif
}

// The source/destination must not overlap/alias.
template <size_t kBytes, typename From, typename To>
HWY_API void CopyBytes(const From* from, To* to) {
#if HWY_COMPILER_MSVC
  const uint8_t* HWY_RESTRICT from_bytes =
      reinterpret_cast<const uint8_t*>(from);
  uint8_t* HWY_RESTRICT to_bytes = reinterpret_cast<uint8_t*>(to);
  for (size_t i = 0; i < kBytes; ++i) {
    to_bytes[i] = from_bytes[i];
  }
#else
  // Avoids horrible codegen on Clang (series of PINSRB)
  __builtin_memcpy(to, from, kBytes);
#endif
}

HWY_API float F32FromBF16(bfloat16_t bf) {
  uint32_t bits = bf.bits;
  bits <<= 16;
  float f;
  CopyBytes<4>(&bits, &f);
  return f;
}

HWY_API bfloat16_t BF16FromF32(float f) {
  uint32_t bits;
  CopyBytes<4>(&f, &bits);
  bfloat16_t bf;
  bf.bits = static_cast<uint16_t>(bits >> 16);
  return bf;
}

HWY_NORETURN void HWY_FORMAT(3, 4)
    Abort(const char* file, int line, const char* format, ...);

}  // namespace hwy

#endif  // HIGHWAY_HWY_BASE_H_
