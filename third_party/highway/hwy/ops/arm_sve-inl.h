// Copyright 2021 Google LLC
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

// ARM SVE[2] vectors (length not known at compile time).
// External include guard in highway.h - see comment there.

#include <stddef.h>
#include <stdint.h>

#include <arm_sve.h>

#include "hwy/base.h"
#include "hwy/ops/shared-inl.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

template <class V>
struct DFromV_t {};  // specialized in macros
template <class V>
using DFromV = typename DFromV_t<RemoveConst<V>>::type;

template <class V>
using TFromV = TFromD<DFromV<V>>;

#define HWY_IF_UNSIGNED_V(V) HWY_IF_UNSIGNED(TFromV<V>)
#define HWY_IF_SIGNED_V(V) HWY_IF_SIGNED(TFromV<V>)
#define HWY_IF_FLOAT_V(V) HWY_IF_FLOAT(TFromV<V>)
#define HWY_IF_LANE_SIZE_V(V, bytes) HWY_IF_LANE_SIZE(TFromV<V>, bytes)

// ================================================== MACROS

// Generate specializations and function definitions using X macros. Although
// harder to read and debug, writing everything manually is too bulky.

namespace detail {  // for code folding

// Unsigned:
#define HWY_SVE_FOREACH_U08(X_MACRO, NAME, OP) X_MACRO(uint, u, 8, 8, NAME, OP)
#define HWY_SVE_FOREACH_U16(X_MACRO, NAME, OP) X_MACRO(uint, u, 16, 8, NAME, OP)
#define HWY_SVE_FOREACH_U32(X_MACRO, NAME, OP) \
  X_MACRO(uint, u, 32, 16, NAME, OP)
#define HWY_SVE_FOREACH_U64(X_MACRO, NAME, OP) \
  X_MACRO(uint, u, 64, 32, NAME, OP)

// Signed:
#define HWY_SVE_FOREACH_I08(X_MACRO, NAME, OP) X_MACRO(int, s, 8, 8, NAME, OP)
#define HWY_SVE_FOREACH_I16(X_MACRO, NAME, OP) X_MACRO(int, s, 16, 8, NAME, OP)
#define HWY_SVE_FOREACH_I32(X_MACRO, NAME, OP) X_MACRO(int, s, 32, 16, NAME, OP)
#define HWY_SVE_FOREACH_I64(X_MACRO, NAME, OP) X_MACRO(int, s, 64, 32, NAME, OP)

// Float:
#define HWY_SVE_FOREACH_F16(X_MACRO, NAME, OP) \
  X_MACRO(float, f, 16, 16, NAME, OP)
#define HWY_SVE_FOREACH_F32(X_MACRO, NAME, OP) \
  X_MACRO(float, f, 32, 16, NAME, OP)
#define HWY_SVE_FOREACH_F64(X_MACRO, NAME, OP) \
  X_MACRO(float, f, 64, 32, NAME, OP)

// For all element sizes:
#define HWY_SVE_FOREACH_U(X_MACRO, NAME, OP) \
  HWY_SVE_FOREACH_U08(X_MACRO, NAME, OP)     \
  HWY_SVE_FOREACH_U16(X_MACRO, NAME, OP)     \
  HWY_SVE_FOREACH_U32(X_MACRO, NAME, OP)     \
  HWY_SVE_FOREACH_U64(X_MACRO, NAME, OP)

#define HWY_SVE_FOREACH_I(X_MACRO, NAME, OP) \
  HWY_SVE_FOREACH_I08(X_MACRO, NAME, OP)     \
  HWY_SVE_FOREACH_I16(X_MACRO, NAME, OP)     \
  HWY_SVE_FOREACH_I32(X_MACRO, NAME, OP)     \
  HWY_SVE_FOREACH_I64(X_MACRO, NAME, OP)

#define HWY_SVE_FOREACH_F(X_MACRO, NAME, OP) \
  HWY_SVE_FOREACH_F16(X_MACRO, NAME, OP)     \
  HWY_SVE_FOREACH_F32(X_MACRO, NAME, OP)     \
  HWY_SVE_FOREACH_F64(X_MACRO, NAME, OP)

// Commonly used type categories for a given element size:
#define HWY_SVE_FOREACH_UI08(X_MACRO, NAME, OP) \
  HWY_SVE_FOREACH_U08(X_MACRO, NAME, OP)        \
  HWY_SVE_FOREACH_I08(X_MACRO, NAME, OP)

#define HWY_SVE_FOREACH_UI16(X_MACRO, NAME, OP) \
  HWY_SVE_FOREACH_U16(X_MACRO, NAME, OP)        \
  HWY_SVE_FOREACH_I16(X_MACRO, NAME, OP)

#define HWY_SVE_FOREACH_UI32(X_MACRO, NAME, OP) \
  HWY_SVE_FOREACH_U32(X_MACRO, NAME, OP)        \
  HWY_SVE_FOREACH_I32(X_MACRO, NAME, OP)

#define HWY_SVE_FOREACH_UI64(X_MACRO, NAME, OP) \
  HWY_SVE_FOREACH_U64(X_MACRO, NAME, OP)        \
  HWY_SVE_FOREACH_I64(X_MACRO, NAME, OP)

#define HWY_SVE_FOREACH_UIF3264(X_MACRO, NAME, OP) \
  HWY_SVE_FOREACH_UI32(X_MACRO, NAME, OP)          \
  HWY_SVE_FOREACH_UI64(X_MACRO, NAME, OP)          \
  HWY_SVE_FOREACH_F32(X_MACRO, NAME, OP)           \
  HWY_SVE_FOREACH_F64(X_MACRO, NAME, OP)

// Commonly used type categories:
#define HWY_SVE_FOREACH_UI(X_MACRO, NAME, OP) \
  HWY_SVE_FOREACH_U(X_MACRO, NAME, OP)        \
  HWY_SVE_FOREACH_I(X_MACRO, NAME, OP)

#define HWY_SVE_FOREACH_IF(X_MACRO, NAME, OP) \
  HWY_SVE_FOREACH_I(X_MACRO, NAME, OP)        \
  HWY_SVE_FOREACH_F(X_MACRO, NAME, OP)

#define HWY_SVE_FOREACH(X_MACRO, NAME, OP) \
  HWY_SVE_FOREACH_U(X_MACRO, NAME, OP)     \
  HWY_SVE_FOREACH_I(X_MACRO, NAME, OP)     \
  HWY_SVE_FOREACH_F(X_MACRO, NAME, OP)

// Assemble types for use in x-macros
#define HWY_SVE_T(BASE, BITS) BASE##BITS##_t
#define HWY_SVE_D(BASE, BITS, N, POW2) Simd<HWY_SVE_T(BASE, BITS), N, POW2>
#define HWY_SVE_V(BASE, BITS) sv##BASE##BITS##_t

}  // namespace detail

#define HWY_SPECIALIZE(BASE, CHAR, BITS, HALF, NAME, OP) \
  template <>                                            \
  struct DFromV_t<HWY_SVE_V(BASE, BITS)> {               \
    using type = ScalableTag<HWY_SVE_T(BASE, BITS)>;     \
  };

HWY_SVE_FOREACH(HWY_SPECIALIZE, _, _)
#undef HWY_SPECIALIZE

// Note: _x (don't-care value for inactive lanes) avoids additional MOVPRFX
// instructions, and we anyway only use it when the predicate is ptrue.

// vector = f(vector), e.g. Not
#define HWY_SVE_RETV_ARGPV(BASE, CHAR, BITS, HALF, NAME, OP)    \
  HWY_API HWY_SVE_V(BASE, BITS) NAME(HWY_SVE_V(BASE, BITS) v) { \
    return sv##OP##_##CHAR##BITS##_x(HWY_SVE_PTRUE(BITS), v);   \
  }
#define HWY_SVE_RETV_ARGV(BASE, CHAR, BITS, HALF, NAME, OP)     \
  HWY_API HWY_SVE_V(BASE, BITS) NAME(HWY_SVE_V(BASE, BITS) v) { \
    return sv##OP##_##CHAR##BITS(v);                            \
  }

// vector = f(vector, scalar), e.g. detail::AddN
#define HWY_SVE_RETV_ARGPVN(BASE, CHAR, BITS, HALF, NAME, OP)    \
  HWY_API HWY_SVE_V(BASE, BITS)                                  \
      NAME(HWY_SVE_V(BASE, BITS) a, HWY_SVE_T(BASE, BITS) b) {   \
    return sv##OP##_##CHAR##BITS##_x(HWY_SVE_PTRUE(BITS), a, b); \
  }
#define HWY_SVE_RETV_ARGVN(BASE, CHAR, BITS, HALF, NAME, OP)   \
  HWY_API HWY_SVE_V(BASE, BITS)                                \
      NAME(HWY_SVE_V(BASE, BITS) a, HWY_SVE_T(BASE, BITS) b) { \
    return sv##OP##_##CHAR##BITS(a, b);                        \
  }

// vector = f(vector, vector), e.g. Add
#define HWY_SVE_RETV_ARGPVV(BASE, CHAR, BITS, HALF, NAME, OP)    \
  HWY_API HWY_SVE_V(BASE, BITS)                                  \
      NAME(HWY_SVE_V(BASE, BITS) a, HWY_SVE_V(BASE, BITS) b) {   \
    return sv##OP##_##CHAR##BITS##_x(HWY_SVE_PTRUE(BITS), a, b); \
  }
#define HWY_SVE_RETV_ARGVV(BASE, CHAR, BITS, HALF, NAME, OP)   \
  HWY_API HWY_SVE_V(BASE, BITS)                                \
      NAME(HWY_SVE_V(BASE, BITS) a, HWY_SVE_V(BASE, BITS) b) { \
    return sv##OP##_##CHAR##BITS(a, b);                        \
  }

// ------------------------------ Lanes

namespace detail {

// Returns actual lanes of a hardware vector without rounding to a power of two.
HWY_INLINE size_t AllHardwareLanes(hwy::SizeTag<1> /* tag */) {
  return svcntb_pat(SV_ALL);
}
HWY_INLINE size_t AllHardwareLanes(hwy::SizeTag<2> /* tag */) {
  return svcnth_pat(SV_ALL);
}
HWY_INLINE size_t AllHardwareLanes(hwy::SizeTag<4> /* tag */) {
  return svcntw_pat(SV_ALL);
}
HWY_INLINE size_t AllHardwareLanes(hwy::SizeTag<8> /* tag */) {
  return svcntd_pat(SV_ALL);
}

// Returns actual lanes of a hardware vector, rounded down to a power of two.
HWY_INLINE size_t HardwareLanes(hwy::SizeTag<1> /* tag */) {
  return svcntb_pat(SV_POW2);
}
HWY_INLINE size_t HardwareLanes(hwy::SizeTag<2> /* tag */) {
  return svcnth_pat(SV_POW2);
}
HWY_INLINE size_t HardwareLanes(hwy::SizeTag<4> /* tag */) {
  return svcntw_pat(SV_POW2);
}
HWY_INLINE size_t HardwareLanes(hwy::SizeTag<8> /* tag */) {
  return svcntd_pat(SV_POW2);
}

}  // namespace detail

// Returns actual number of lanes after capping by N and shifting. May return 0
// (e.g. for "1/8th" of a u32x4 - would be 1 for 1/8th of u32x8).
template <typename T, size_t N, int kPow2>
HWY_API size_t Lanes(Simd<T, N, kPow2> d) {
  const size_t actual = detail::HardwareLanes(hwy::SizeTag<sizeof(T)>());
  // Common case of full vectors: avoid any extra instructions.
  if (detail::IsFull(d)) return actual;
  return HWY_MIN(detail::ScaleByPower(actual, kPow2), N);
}

// ================================================== MASK INIT

// One mask bit per byte; only the one belonging to the lowest byte is valid.

// ------------------------------ FirstN
#define HWY_SVE_FIRSTN(BASE, CHAR, BITS, HALF, NAME, OP)                       \
  template <size_t N, int kPow2>                                               \
  HWY_API svbool_t NAME(HWY_SVE_D(BASE, BITS, N, kPow2) d, size_t count) {     \
    const size_t limit = detail::IsFull(d) ? count : HWY_MIN(Lanes(d), count); \
    return sv##OP##_b##BITS##_u32(uint32_t{0}, static_cast<uint32_t>(limit));  \
  }
HWY_SVE_FOREACH(HWY_SVE_FIRSTN, FirstN, whilelt)
#undef HWY_SVE_FIRSTN

namespace detail {

// All-true mask from a macro
#define HWY_SVE_PTRUE(BITS) svptrue_pat_b##BITS(SV_POW2)

#define HWY_SVE_WRAP_PTRUE(BASE, CHAR, BITS, HALF, NAME, OP)       \
  template <size_t N, int kPow2>                                   \
  HWY_API svbool_t NAME(HWY_SVE_D(BASE, BITS, N, kPow2) /* d */) { \
    return HWY_SVE_PTRUE(BITS);                                    \
  }

HWY_SVE_FOREACH(HWY_SVE_WRAP_PTRUE, PTrue, ptrue)  // return all-true
#undef HWY_SVE_WRAP_PTRUE

HWY_API svbool_t PFalse() { return svpfalse_b(); }

// Returns all-true if d is HWY_FULL or FirstN(N) after capping N.
//
// This is used in functions that load/store memory; other functions (e.g.
// arithmetic) can ignore d and use PTrue instead.
template <class D>
svbool_t MakeMask(D d) {
  return IsFull(d) ? PTrue(d) : FirstN(d, Lanes(d));
}

}  // namespace detail

// ================================================== INIT

// ------------------------------ Set
// vector = f(d, scalar), e.g. Set
#define HWY_SVE_SET(BASE, CHAR, BITS, HALF, NAME, OP)                         \
  template <size_t N, int kPow2>                                              \
  HWY_API HWY_SVE_V(BASE, BITS) NAME(HWY_SVE_D(BASE, BITS, N, kPow2) /* d */, \
                                     HWY_SVE_T(BASE, BITS) arg) {             \
    return sv##OP##_##CHAR##BITS(arg);                                        \
  }

HWY_SVE_FOREACH(HWY_SVE_SET, Set, dup_n)
#undef HWY_SVE_SET

// Required for Zero and VFromD
template <size_t N, int kPow2>
svuint16_t Set(Simd<bfloat16_t, N, kPow2> d, bfloat16_t arg) {
  return Set(RebindToUnsigned<decltype(d)>(), arg.bits);
}

template <class D>
using VFromD = decltype(Set(D(), TFromD<D>()));

// ------------------------------ Zero

template <class D>
VFromD<D> Zero(D d) {
  return Set(d, 0);
}

// ------------------------------ Undefined

#define HWY_SVE_UNDEFINED(BASE, CHAR, BITS, HALF, NAME, OP) \
  template <size_t N, int kPow2>                            \
  HWY_API HWY_SVE_V(BASE, BITS)                             \
      NAME(HWY_SVE_D(BASE, BITS, N, kPow2) /* d */) {       \
    return sv##OP##_##CHAR##BITS();                         \
  }

HWY_SVE_FOREACH(HWY_SVE_UNDEFINED, Undefined, undef)

// ------------------------------ BitCast

namespace detail {

// u8: no change
#define HWY_SVE_CAST_NOP(BASE, CHAR, BITS, HALF, NAME, OP)                \
  HWY_API HWY_SVE_V(BASE, BITS) BitCastToByte(HWY_SVE_V(BASE, BITS) v) {  \
    return v;                                                             \
  }                                                                       \
  template <size_t N, int kPow2>                                          \
  HWY_API HWY_SVE_V(BASE, BITS) BitCastFromByte(                          \
      HWY_SVE_D(BASE, BITS, N, kPow2) /* d */, HWY_SVE_V(BASE, BITS) v) { \
    return v;                                                             \
  }

// All other types
#define HWY_SVE_CAST(BASE, CHAR, BITS, HALF, NAME, OP)                        \
  HWY_INLINE svuint8_t BitCastToByte(HWY_SVE_V(BASE, BITS) v) {               \
    return sv##OP##_u8_##CHAR##BITS(v);                                       \
  }                                                                           \
  template <size_t N, int kPow2>                                              \
  HWY_INLINE HWY_SVE_V(BASE, BITS)                                            \
      BitCastFromByte(HWY_SVE_D(BASE, BITS, N, kPow2) /* d */, svuint8_t v) { \
    return sv##OP##_##CHAR##BITS##_u8(v);                                     \
  }

HWY_SVE_FOREACH_U08(HWY_SVE_CAST_NOP, _, _)
HWY_SVE_FOREACH_I08(HWY_SVE_CAST, _, reinterpret)
HWY_SVE_FOREACH_UI16(HWY_SVE_CAST, _, reinterpret)
HWY_SVE_FOREACH_UI32(HWY_SVE_CAST, _, reinterpret)
HWY_SVE_FOREACH_UI64(HWY_SVE_CAST, _, reinterpret)
HWY_SVE_FOREACH_F(HWY_SVE_CAST, _, reinterpret)

#undef HWY_SVE_CAST_NOP
#undef HWY_SVE_CAST

template <size_t N, int kPow2>
HWY_INLINE svuint16_t BitCastFromByte(Simd<bfloat16_t, N, kPow2> /* d */,
                                      svuint8_t v) {
  return BitCastFromByte(Simd<uint16_t, N, kPow2>(), v);
}

}  // namespace detail

template <class D, class FromV>
HWY_API VFromD<D> BitCast(D d, FromV v) {
  return detail::BitCastFromByte(d, detail::BitCastToByte(v));
}

// ================================================== LOGICAL

// detail::*N() functions accept a scalar argument to avoid extra Set().

// ------------------------------ Not

HWY_SVE_FOREACH_UI(HWY_SVE_RETV_ARGPV, Not, not )

// ------------------------------ And

namespace detail {
HWY_SVE_FOREACH_UI(HWY_SVE_RETV_ARGPVN, AndN, and_n)
}  // namespace detail

HWY_SVE_FOREACH_UI(HWY_SVE_RETV_ARGPVV, And, and)

template <class V, HWY_IF_FLOAT_V(V)>
HWY_API V And(const V a, const V b) {
  const DFromV<V> df;
  const RebindToUnsigned<decltype(df)> du;
  return BitCast(df, And(BitCast(du, a), BitCast(du, b)));
}

// ------------------------------ Or

HWY_SVE_FOREACH_UI(HWY_SVE_RETV_ARGPVV, Or, orr)

template <class V, HWY_IF_FLOAT_V(V)>
HWY_API V Or(const V a, const V b) {
  const DFromV<V> df;
  const RebindToUnsigned<decltype(df)> du;
  return BitCast(df, Or(BitCast(du, a), BitCast(du, b)));
}

// ------------------------------ Xor

namespace detail {
HWY_SVE_FOREACH_UI(HWY_SVE_RETV_ARGPVN, XorN, eor_n)
}  // namespace detail

HWY_SVE_FOREACH_UI(HWY_SVE_RETV_ARGPVV, Xor, eor)

template <class V, HWY_IF_FLOAT_V(V)>
HWY_API V Xor(const V a, const V b) {
  const DFromV<V> df;
  const RebindToUnsigned<decltype(df)> du;
  return BitCast(df, Xor(BitCast(du, a), BitCast(du, b)));
}

// ------------------------------ AndNot

namespace detail {
#define HWY_SVE_RETV_ARGPVN_SWAP(BASE, CHAR, BITS, HALF, NAME, OP) \
  HWY_API HWY_SVE_V(BASE, BITS)                                    \
      NAME(HWY_SVE_T(BASE, BITS) a, HWY_SVE_V(BASE, BITS) b) {     \
    return sv##OP##_##CHAR##BITS##_x(HWY_SVE_PTRUE(BITS), b, a);   \
  }

HWY_SVE_FOREACH_UI(HWY_SVE_RETV_ARGPVN_SWAP, AndNotN, bic_n)
#undef HWY_SVE_RETV_ARGPVN_SWAP
}  // namespace detail

#define HWY_SVE_RETV_ARGPVV_SWAP(BASE, CHAR, BITS, HALF, NAME, OP) \
  HWY_API HWY_SVE_V(BASE, BITS)                                    \
      NAME(HWY_SVE_V(BASE, BITS) a, HWY_SVE_V(BASE, BITS) b) {     \
    return sv##OP##_##CHAR##BITS##_x(HWY_SVE_PTRUE(BITS), b, a);   \
  }
HWY_SVE_FOREACH_UI(HWY_SVE_RETV_ARGPVV_SWAP, AndNot, bic)
#undef HWY_SVE_RETV_ARGPVV_SWAP

template <class V, HWY_IF_FLOAT_V(V)>
HWY_API V AndNot(const V a, const V b) {
  const DFromV<V> df;
  const RebindToUnsigned<decltype(df)> du;
  return BitCast(df, AndNot(BitCast(du, a), BitCast(du, b)));
}

// ------------------------------ OrAnd

template <class V>
HWY_API V OrAnd(const V o, const V a1, const V a2) {
  return Or(o, And(a1, a2));
}

// ------------------------------ PopulationCount

#ifdef HWY_NATIVE_POPCNT
#undef HWY_NATIVE_POPCNT
#else
#define HWY_NATIVE_POPCNT
#endif

// Need to return original type instead of unsigned.
#define HWY_SVE_POPCNT(BASE, CHAR, BITS, HALF, NAME, OP)               \
  HWY_API HWY_SVE_V(BASE, BITS) NAME(HWY_SVE_V(BASE, BITS) v) {        \
    return BitCast(DFromV<decltype(v)>(),                              \
                   sv##OP##_##CHAR##BITS##_x(HWY_SVE_PTRUE(BITS), v)); \
  }
HWY_SVE_FOREACH_UI(HWY_SVE_POPCNT, PopulationCount, cnt)
#undef HWY_SVE_POPCNT

// ================================================== SIGN

// ------------------------------ Neg
HWY_SVE_FOREACH_IF(HWY_SVE_RETV_ARGPV, Neg, neg)

// ------------------------------ Abs
HWY_SVE_FOREACH_IF(HWY_SVE_RETV_ARGPV, Abs, abs)

// ------------------------------ CopySign[ToAbs]

template <class V>
HWY_API V CopySign(const V magn, const V sign) {
  const auto msb = SignBit(DFromV<V>());
  return Or(AndNot(msb, magn), And(msb, sign));
}

template <class V>
HWY_API V CopySignToAbs(const V abs, const V sign) {
  const auto msb = SignBit(DFromV<V>());
  return Or(abs, And(msb, sign));
}

// ================================================== ARITHMETIC

// ------------------------------ Add

namespace detail {
HWY_SVE_FOREACH(HWY_SVE_RETV_ARGPVN, AddN, add_n)
}  // namespace detail

HWY_SVE_FOREACH(HWY_SVE_RETV_ARGPVV, Add, add)

// ------------------------------ Sub

namespace detail {
// Can't use HWY_SVE_RETV_ARGPVN because caller wants to specify pg.
#define HWY_SVE_RETV_ARGPVN_MASK(BASE, CHAR, BITS, HALF, NAME, OP)          \
  HWY_API HWY_SVE_V(BASE, BITS)                                             \
      NAME(svbool_t pg, HWY_SVE_V(BASE, BITS) a, HWY_SVE_T(BASE, BITS) b) { \
    return sv##OP##_##CHAR##BITS##_z(pg, a, b);                             \
  }

HWY_SVE_FOREACH(HWY_SVE_RETV_ARGPVN_MASK, SubN, sub_n)
#undef HWY_SVE_RETV_ARGPVN_MASK
}  // namespace detail

HWY_SVE_FOREACH(HWY_SVE_RETV_ARGPVV, Sub, sub)

// ------------------------------ SumsOf8
HWY_API svuint64_t SumsOf8(const svuint8_t v) {
  const ScalableTag<uint32_t> du32;
  const ScalableTag<uint64_t> du64;
  const svbool_t pg = detail::PTrue(du64);

  const svuint32_t sums_of_4 = svdot_n_u32(Zero(du32), v, 1);
  // Compute pairwise sum of u32 and extend to u64.
  // TODO(janwas): on SVE2, we can instead use svaddp.
  const svuint64_t hi = svlsr_n_u64_x(pg, BitCast(du64, sums_of_4), 32);
  // Isolate the lower 32 bits (to be added to the upper 32 and zero-extended)
  const svuint64_t lo = svextw_u64_x(pg, BitCast(du64, sums_of_4));
  return Add(hi, lo);
}

// ------------------------------ SaturatedAdd

HWY_SVE_FOREACH_UI08(HWY_SVE_RETV_ARGVV, SaturatedAdd, qadd)
HWY_SVE_FOREACH_UI16(HWY_SVE_RETV_ARGVV, SaturatedAdd, qadd)

// ------------------------------ SaturatedSub

HWY_SVE_FOREACH_UI08(HWY_SVE_RETV_ARGVV, SaturatedSub, qsub)
HWY_SVE_FOREACH_UI16(HWY_SVE_RETV_ARGVV, SaturatedSub, qsub)

// ------------------------------ AbsDiff
HWY_SVE_FOREACH_IF(HWY_SVE_RETV_ARGPVV, AbsDiff, abd)

// ------------------------------ ShiftLeft[Same]

#define HWY_SVE_SHIFT_N(BASE, CHAR, BITS, HALF, NAME, OP)               \
  template <int kBits>                                                  \
  HWY_API HWY_SVE_V(BASE, BITS) NAME(HWY_SVE_V(BASE, BITS) v) {         \
    return sv##OP##_##CHAR##BITS##_x(HWY_SVE_PTRUE(BITS), v, kBits);    \
  }                                                                     \
  HWY_API HWY_SVE_V(BASE, BITS)                                         \
      NAME##Same(HWY_SVE_V(BASE, BITS) v, HWY_SVE_T(uint, BITS) bits) { \
    return sv##OP##_##CHAR##BITS##_x(HWY_SVE_PTRUE(BITS), v, bits);     \
  }

HWY_SVE_FOREACH_UI(HWY_SVE_SHIFT_N, ShiftLeft, lsl_n)

// ------------------------------ ShiftRight[Same]

HWY_SVE_FOREACH_U(HWY_SVE_SHIFT_N, ShiftRight, lsr_n)
HWY_SVE_FOREACH_I(HWY_SVE_SHIFT_N, ShiftRight, asr_n)

#undef HWY_SVE_SHIFT_N

// ------------------------------ RotateRight

// TODO(janwas): svxar on SVE2
template <int kBits, class V>
HWY_API V RotateRight(const V v) {
  constexpr size_t kSizeInBits = sizeof(TFromV<V>) * 8;
  static_assert(0 <= kBits && kBits < kSizeInBits, "Invalid shift count");
  if (kBits == 0) return v;
  return Or(ShiftRight<kBits>(v), ShiftLeft<kSizeInBits - kBits>(v));
}

// ------------------------------ Shl/r

#define HWY_SVE_SHIFT(BASE, CHAR, BITS, HALF, NAME, OP)           \
  HWY_API HWY_SVE_V(BASE, BITS)                                   \
      NAME(HWY_SVE_V(BASE, BITS) v, HWY_SVE_V(BASE, BITS) bits) { \
    const RebindToUnsigned<DFromV<decltype(v)>> du;               \
    return sv##OP##_##CHAR##BITS##_x(HWY_SVE_PTRUE(BITS), v,      \
                                     BitCast(du, bits));          \
  }

HWY_SVE_FOREACH_UI(HWY_SVE_SHIFT, Shl, lsl)

HWY_SVE_FOREACH_U(HWY_SVE_SHIFT, Shr, lsr)
HWY_SVE_FOREACH_I(HWY_SVE_SHIFT, Shr, asr)

#undef HWY_SVE_SHIFT

// ------------------------------ Min/Max

HWY_SVE_FOREACH_UI(HWY_SVE_RETV_ARGPVV, Min, min)
HWY_SVE_FOREACH_UI(HWY_SVE_RETV_ARGPVV, Max, max)
HWY_SVE_FOREACH_F(HWY_SVE_RETV_ARGPVV, Min, minnm)
HWY_SVE_FOREACH_F(HWY_SVE_RETV_ARGPVV, Max, maxnm)

namespace detail {
HWY_SVE_FOREACH_UI(HWY_SVE_RETV_ARGPVN, MinN, min_n)
HWY_SVE_FOREACH_UI(HWY_SVE_RETV_ARGPVN, MaxN, max_n)
}  // namespace detail

// ------------------------------ Mul
HWY_SVE_FOREACH_UI16(HWY_SVE_RETV_ARGPVV, Mul, mul)
HWY_SVE_FOREACH_UIF3264(HWY_SVE_RETV_ARGPVV, Mul, mul)

// ------------------------------ MulHigh
HWY_SVE_FOREACH_UI16(HWY_SVE_RETV_ARGPVV, MulHigh, mulh)
namespace detail {
HWY_SVE_FOREACH_UI32(HWY_SVE_RETV_ARGPVV, MulHigh, mulh)
HWY_SVE_FOREACH_U64(HWY_SVE_RETV_ARGPVV, MulHigh, mulh)
}  // namespace detail

// ------------------------------ Div
HWY_SVE_FOREACH_F(HWY_SVE_RETV_ARGPVV, Div, div)

// ------------------------------ ApproximateReciprocal
HWY_SVE_FOREACH_F32(HWY_SVE_RETV_ARGV, ApproximateReciprocal, recpe)

// ------------------------------ Sqrt
HWY_SVE_FOREACH_F(HWY_SVE_RETV_ARGPV, Sqrt, sqrt)

// ------------------------------ ApproximateReciprocalSqrt
HWY_SVE_FOREACH_F32(HWY_SVE_RETV_ARGV, ApproximateReciprocalSqrt, rsqrte)

// ------------------------------ MulAdd
#define HWY_SVE_FMA(BASE, CHAR, BITS, HALF, NAME, OP)                   \
  HWY_API HWY_SVE_V(BASE, BITS)                                         \
      NAME(HWY_SVE_V(BASE, BITS) mul, HWY_SVE_V(BASE, BITS) x,          \
           HWY_SVE_V(BASE, BITS) add) {                                 \
    return sv##OP##_##CHAR##BITS##_x(HWY_SVE_PTRUE(BITS), x, mul, add); \
  }

HWY_SVE_FOREACH_F(HWY_SVE_FMA, MulAdd, mad)

// ------------------------------ NegMulAdd
HWY_SVE_FOREACH_F(HWY_SVE_FMA, NegMulAdd, msb)

// ------------------------------ MulSub
HWY_SVE_FOREACH_F(HWY_SVE_FMA, MulSub, nmsb)

// ------------------------------ NegMulSub
HWY_SVE_FOREACH_F(HWY_SVE_FMA, NegMulSub, nmad)

#undef HWY_SVE_FMA

// ------------------------------ Round etc.

HWY_SVE_FOREACH_F(HWY_SVE_RETV_ARGPV, Round, rintn)
HWY_SVE_FOREACH_F(HWY_SVE_RETV_ARGPV, Floor, rintm)
HWY_SVE_FOREACH_F(HWY_SVE_RETV_ARGPV, Ceil, rintp)
HWY_SVE_FOREACH_F(HWY_SVE_RETV_ARGPV, Trunc, rintz)

// ================================================== MASK

// ------------------------------ RebindMask
template <class D, typename MFrom>
HWY_API svbool_t RebindMask(const D /*d*/, const MFrom mask) {
  return mask;
}

// ------------------------------ Mask logical

HWY_API svbool_t Not(svbool_t m) {
  // We don't know the lane type, so assume 8-bit. For larger types, this will
  // de-canonicalize the predicate, i.e. set bits to 1 even though they do not
  // correspond to the lowest byte in the lane. Per ARM, such bits are ignored.
  return svnot_b_z(HWY_SVE_PTRUE(8), m);
}
HWY_API svbool_t And(svbool_t a, svbool_t b) {
  return svand_b_z(b, b, a);  // same order as AndNot for consistency
}
HWY_API svbool_t AndNot(svbool_t a, svbool_t b) {
  return svbic_b_z(b, b, a);  // reversed order like NEON
}
HWY_API svbool_t Or(svbool_t a, svbool_t b) {
  return svsel_b(a, a, b);  // a ? true : b
}
HWY_API svbool_t Xor(svbool_t a, svbool_t b) {
  return svsel_b(a, svnand_b_z(a, a, b), b);  // a ? !(a & b) : b.
}

// ------------------------------ CountTrue

#define HWY_SVE_COUNT_TRUE(BASE, CHAR, BITS, HALF, NAME, OP)           \
  template <size_t N, int kPow2>                                       \
  HWY_API size_t NAME(HWY_SVE_D(BASE, BITS, N, kPow2) d, svbool_t m) { \
    return sv##OP##_b##BITS(detail::MakeMask(d), m);                   \
  }

HWY_SVE_FOREACH(HWY_SVE_COUNT_TRUE, CountTrue, cntp)
#undef HWY_SVE_COUNT_TRUE

// For 16-bit Compress: full vector, not limited to SV_POW2.
namespace detail {

#define HWY_SVE_COUNT_TRUE_FULL(BASE, CHAR, BITS, HALF, NAME, OP)            \
  template <size_t N, int kPow2>                                             \
  HWY_API size_t NAME(HWY_SVE_D(BASE, BITS, N, kPow2) /* d */, svbool_t m) { \
    return sv##OP##_b##BITS(svptrue_b##BITS(), m);                           \
  }

HWY_SVE_FOREACH(HWY_SVE_COUNT_TRUE_FULL, CountTrueFull, cntp)
#undef HWY_SVE_COUNT_TRUE_FULL

}  // namespace detail

// ------------------------------ AllFalse
template <class D>
HWY_API bool AllFalse(D d, svbool_t m) {
  return !svptest_any(detail::MakeMask(d), m);
}

// ------------------------------ AllTrue
template <class D>
HWY_API bool AllTrue(D d, svbool_t m) {
  return CountTrue(d, m) == Lanes(d);
}

// ------------------------------ FindFirstTrue
template <class D>
HWY_API intptr_t FindFirstTrue(D d, svbool_t m) {
  return AllFalse(d, m) ? intptr_t{-1}
                        : static_cast<intptr_t>(
                              CountTrue(d, svbrkb_b_z(detail::MakeMask(d), m)));
}

// ------------------------------ IfThenElse
#define HWY_SVE_IF_THEN_ELSE(BASE, CHAR, BITS, HALF, NAME, OP)                \
  HWY_API HWY_SVE_V(BASE, BITS)                                               \
      NAME(svbool_t m, HWY_SVE_V(BASE, BITS) yes, HWY_SVE_V(BASE, BITS) no) { \
    return sv##OP##_##CHAR##BITS(m, yes, no);                                 \
  }

HWY_SVE_FOREACH(HWY_SVE_IF_THEN_ELSE, IfThenElse, sel)
#undef HWY_SVE_IF_THEN_ELSE

// ------------------------------ IfThenElseZero
template <class M, class V>
HWY_API V IfThenElseZero(const M mask, const V yes) {
  return IfThenElse(mask, yes, Zero(DFromV<V>()));
}

// ------------------------------ IfThenZeroElse
template <class M, class V>
HWY_API V IfThenZeroElse(const M mask, const V no) {
  return IfThenElse(mask, Zero(DFromV<V>()), no);
}

// ================================================== COMPARE

// mask = f(vector, vector)
#define HWY_SVE_COMPARE(BASE, CHAR, BITS, HALF, NAME, OP)                   \
  HWY_API svbool_t NAME(HWY_SVE_V(BASE, BITS) a, HWY_SVE_V(BASE, BITS) b) { \
    return sv##OP##_##CHAR##BITS(HWY_SVE_PTRUE(BITS), a, b);                \
  }
#define HWY_SVE_COMPARE_N(BASE, CHAR, BITS, HALF, NAME, OP)                 \
  HWY_API svbool_t NAME(HWY_SVE_V(BASE, BITS) a, HWY_SVE_T(BASE, BITS) b) { \
    return sv##OP##_##CHAR##BITS(HWY_SVE_PTRUE(BITS), a, b);                \
  }

// ------------------------------ Eq
HWY_SVE_FOREACH(HWY_SVE_COMPARE, Eq, cmpeq)
namespace detail {
HWY_SVE_FOREACH(HWY_SVE_COMPARE_N, EqN, cmpeq_n)
}  // namespace detail

// ------------------------------ Ne
HWY_SVE_FOREACH(HWY_SVE_COMPARE, Ne, cmpne)
namespace detail {
HWY_SVE_FOREACH(HWY_SVE_COMPARE_N, NeN, cmpne_n)
}  // namespace detail

// ------------------------------ Lt
HWY_SVE_FOREACH(HWY_SVE_COMPARE, Lt, cmplt)
namespace detail {
HWY_SVE_FOREACH(HWY_SVE_COMPARE_N, LtN, cmplt_n)
}  // namespace detail

// ------------------------------ Le
HWY_SVE_FOREACH_F(HWY_SVE_COMPARE, Le, cmple)

#undef HWY_SVE_COMPARE
#undef HWY_SVE_COMPARE_N

// ------------------------------ Gt/Ge (swapped order)

template <class V>
HWY_API svbool_t Gt(const V a, const V b) {
  return Lt(b, a);
}
template <class V>
HWY_API svbool_t Ge(const V a, const V b) {
  return Le(b, a);
}

// ------------------------------ TestBit
template <class V>
HWY_API svbool_t TestBit(const V a, const V bit) {
  return detail::NeN(And(a, bit), 0);
}

// ------------------------------ MaskFromVec (Ne)
template <class V>
HWY_API svbool_t MaskFromVec(const V v) {
  return detail::NeN(v, static_cast<TFromV<V>>(0));
}

// ------------------------------ VecFromMask

template <class D, HWY_IF_NOT_FLOAT_D(D)>
HWY_API VFromD<D> VecFromMask(const D d, svbool_t mask) {
  const auto v0 = Zero(RebindToSigned<decltype(d)>());
  return BitCast(d, detail::SubN(mask, v0, 1));
}

template <class D, HWY_IF_FLOAT_D(D)>
HWY_API VFromD<D> VecFromMask(const D d, svbool_t mask) {
  return BitCast(d, VecFromMask(RebindToUnsigned<D>(), mask));
}

// ------------------------------ IfVecThenElse (MaskFromVec, IfThenElse)

template <class V>
HWY_API V IfVecThenElse(const V mask, const V yes, const V no) {
  // TODO(janwas): use svbsl for SVE2
  return IfThenElse(MaskFromVec(mask), yes, no);
}

// ================================================== MEMORY

// ------------------------------ Load/MaskedLoad/LoadDup128/Store/Stream

#define HWY_SVE_LOAD(BASE, CHAR, BITS, HALF, NAME, OP)     \
  template <size_t N, int kPow2>                           \
  HWY_API HWY_SVE_V(BASE, BITS)                            \
      NAME(HWY_SVE_D(BASE, BITS, N, kPow2) d,              \
           const HWY_SVE_T(BASE, BITS) * HWY_RESTRICT p) { \
    return sv##OP##_##CHAR##BITS(detail::MakeMask(d), p);  \
  }

#define HWY_SVE_MASKED_LOAD(BASE, CHAR, BITS, HALF, NAME, OP)   \
  template <size_t N, int kPow2>                                \
  HWY_API HWY_SVE_V(BASE, BITS)                                 \
      NAME(svbool_t m, HWY_SVE_D(BASE, BITS, N, kPow2) /* d */, \
           const HWY_SVE_T(BASE, BITS) * HWY_RESTRICT p) {      \
    return sv##OP##_##CHAR##BITS(m, p);                         \
  }

#define HWY_SVE_LOAD_DUP128(BASE, CHAR, BITS, HALF, NAME, OP) \
  template <size_t N, int kPow2>                              \
  HWY_API HWY_SVE_V(BASE, BITS)                               \
      NAME(HWY_SVE_D(BASE, BITS, N, kPow2) /* d */,           \
           const HWY_SVE_T(BASE, BITS) * HWY_RESTRICT p) {    \
    /* All-true predicate to load all 128 bits. */            \
    return sv##OP##_##CHAR##BITS(HWY_SVE_PTRUE(8), p);        \
  }

#define HWY_SVE_STORE(BASE, CHAR, BITS, HALF, NAME, OP)       \
  template <size_t N, int kPow2>                              \
  HWY_API void NAME(HWY_SVE_V(BASE, BITS) v,                  \
                    HWY_SVE_D(BASE, BITS, N, kPow2) d,        \
                    HWY_SVE_T(BASE, BITS) * HWY_RESTRICT p) { \
    sv##OP##_##CHAR##BITS(detail::MakeMask(d), p, v);         \
  }

#define HWY_SVE_MASKED_STORE(BASE, CHAR, BITS, HALF, NAME, OP) \
  template <size_t N, int kPow2>                               \
  HWY_API void NAME(svbool_t m, HWY_SVE_V(BASE, BITS) v,       \
                    HWY_SVE_D(BASE, BITS, N, kPow2) /* d */,   \
                    HWY_SVE_T(BASE, BITS) * HWY_RESTRICT p) {  \
    sv##OP##_##CHAR##BITS(m, p, v);                            \
  }

HWY_SVE_FOREACH(HWY_SVE_LOAD, Load, ld1)
HWY_SVE_FOREACH(HWY_SVE_MASKED_LOAD, MaskedLoad, ld1)
HWY_SVE_FOREACH(HWY_SVE_LOAD_DUP128, LoadDup128, ld1rq)
HWY_SVE_FOREACH(HWY_SVE_STORE, Store, st1)
HWY_SVE_FOREACH(HWY_SVE_STORE, Stream, stnt1)
HWY_SVE_FOREACH(HWY_SVE_MASKED_STORE, MaskedStore, st1)

#undef HWY_SVE_LOAD
#undef HWY_SVE_MASKED_LOAD
#undef HWY_SVE_LOAD_DUP128
#undef HWY_SVE_STORE
#undef HWY_SVE_MASKED_STORE

// BF16 is the same as svuint16_t because BF16 is optional before v8.6.
template <size_t N, int kPow2>
HWY_API svuint16_t Load(Simd<bfloat16_t, N, kPow2> d,
                        const bfloat16_t* HWY_RESTRICT p) {
  return Load(RebindToUnsigned<decltype(d)>(),
              reinterpret_cast<const uint16_t * HWY_RESTRICT>(p));
}

template <size_t N, int kPow2>
HWY_API void Store(svuint16_t v, Simd<bfloat16_t, N, kPow2> d,
                   bfloat16_t* HWY_RESTRICT p) {
  Store(v, RebindToUnsigned<decltype(d)>(),
        reinterpret_cast<uint16_t * HWY_RESTRICT>(p));
}

// ------------------------------ Load/StoreU

// SVE only requires lane alignment, not natural alignment of the entire
// vector.
template <class D>
HWY_API VFromD<D> LoadU(D d, const TFromD<D>* HWY_RESTRICT p) {
  return Load(d, p);
}

template <class V, class D>
HWY_API void StoreU(const V v, D d, TFromD<D>* HWY_RESTRICT p) {
  Store(v, d, p);
}

// ------------------------------ ScatterOffset/Index

#define HWY_SVE_SCATTER_OFFSET(BASE, CHAR, BITS, HALF, NAME, OP)             \
  template <size_t N, int kPow2>                                             \
  HWY_API void NAME(HWY_SVE_V(BASE, BITS) v,                                 \
                    HWY_SVE_D(BASE, BITS, N, kPow2) d,                       \
                    HWY_SVE_T(BASE, BITS) * HWY_RESTRICT base,               \
                    HWY_SVE_V(int, BITS) offset) {                           \
    sv##OP##_s##BITS##offset_##CHAR##BITS(detail::MakeMask(d), base, offset, \
                                          v);                                \
  }

#define HWY_SVE_SCATTER_INDEX(BASE, CHAR, BITS, HALF, NAME, OP)                \
  template <size_t N, int kPow2>                                               \
  HWY_API void NAME(                                                           \
      HWY_SVE_V(BASE, BITS) v, HWY_SVE_D(BASE, BITS, N, kPow2) d,              \
      HWY_SVE_T(BASE, BITS) * HWY_RESTRICT base, HWY_SVE_V(int, BITS) index) { \
    sv##OP##_s##BITS##index_##CHAR##BITS(detail::MakeMask(d), base, index, v); \
  }

HWY_SVE_FOREACH_UIF3264(HWY_SVE_SCATTER_OFFSET, ScatterOffset, st1_scatter)
HWY_SVE_FOREACH_UIF3264(HWY_SVE_SCATTER_INDEX, ScatterIndex, st1_scatter)
#undef HWY_SVE_SCATTER_OFFSET
#undef HWY_SVE_SCATTER_INDEX

// ------------------------------ GatherOffset/Index

#define HWY_SVE_GATHER_OFFSET(BASE, CHAR, BITS, HALF, NAME, OP)             \
  template <size_t N, int kPow2>                                            \
  HWY_API HWY_SVE_V(BASE, BITS)                                             \
      NAME(HWY_SVE_D(BASE, BITS, N, kPow2) d,                               \
           const HWY_SVE_T(BASE, BITS) * HWY_RESTRICT base,                 \
           HWY_SVE_V(int, BITS) offset) {                                   \
    return sv##OP##_s##BITS##offset_##CHAR##BITS(detail::MakeMask(d), base, \
                                                 offset);                   \
  }
#define HWY_SVE_GATHER_INDEX(BASE, CHAR, BITS, HALF, NAME, OP)             \
  template <size_t N, int kPow2>                                           \
  HWY_API HWY_SVE_V(BASE, BITS)                                            \
      NAME(HWY_SVE_D(BASE, BITS, N, kPow2) d,                              \
           const HWY_SVE_T(BASE, BITS) * HWY_RESTRICT base,                \
           HWY_SVE_V(int, BITS) index) {                                   \
    return sv##OP##_s##BITS##index_##CHAR##BITS(detail::MakeMask(d), base, \
                                                index);                    \
  }

HWY_SVE_FOREACH_UIF3264(HWY_SVE_GATHER_OFFSET, GatherOffset, ld1_gather)
HWY_SVE_FOREACH_UIF3264(HWY_SVE_GATHER_INDEX, GatherIndex, ld1_gather)
#undef HWY_SVE_GATHER_OFFSET
#undef HWY_SVE_GATHER_INDEX

// ------------------------------ StoreInterleaved3

#define HWY_SVE_STORE3(BASE, CHAR, BITS, HALF, NAME, OP)                      \
  template <size_t N, int kPow2>                                              \
  HWY_API void NAME(HWY_SVE_V(BASE, BITS) v0, HWY_SVE_V(BASE, BITS) v1,       \
                    HWY_SVE_V(BASE, BITS) v2,                                 \
                    HWY_SVE_D(BASE, BITS, N, kPow2) d,                        \
                    HWY_SVE_T(BASE, BITS) * HWY_RESTRICT unaligned) {         \
    const sv##BASE##BITS##x3_t triple = svcreate3##_##CHAR##BITS(v0, v1, v2); \
    sv##OP##_##CHAR##BITS(detail::MakeMask(d), unaligned, triple);            \
  }
HWY_SVE_FOREACH_U08(HWY_SVE_STORE3, StoreInterleaved3, st3)

#undef HWY_SVE_STORE3

// ------------------------------ StoreInterleaved4

#define HWY_SVE_STORE4(BASE, CHAR, BITS, HALF, NAME, OP)                \
  template <size_t N, int kPow2>                                        \
  HWY_API void NAME(HWY_SVE_V(BASE, BITS) v0, HWY_SVE_V(BASE, BITS) v1, \
                    HWY_SVE_V(BASE, BITS) v2, HWY_SVE_V(BASE, BITS) v3, \
                    HWY_SVE_D(BASE, BITS, N, kPow2) d,                  \
                    HWY_SVE_T(BASE, BITS) * HWY_RESTRICT unaligned) {   \
    const sv##BASE##BITS##x4_t quad =                                   \
        svcreate4##_##CHAR##BITS(v0, v1, v2, v3);                       \
    sv##OP##_##CHAR##BITS(detail::MakeMask(d), unaligned, quad);        \
  }
HWY_SVE_FOREACH_U08(HWY_SVE_STORE4, StoreInterleaved4, st4)

#undef HWY_SVE_STORE4

// ================================================== CONVERT

// ------------------------------ PromoteTo

// Same sign
#define HWY_SVE_PROMOTE_TO(BASE, CHAR, BITS, HALF, NAME, OP)                \
  template <size_t N, int kPow2>                                            \
  HWY_API HWY_SVE_V(BASE, BITS) NAME(                                       \
      HWY_SVE_D(BASE, BITS, N, kPow2) /* tag */, HWY_SVE_V(BASE, HALF) v) { \
    return sv##OP##_##CHAR##BITS(v);                                        \
  }

HWY_SVE_FOREACH_UI16(HWY_SVE_PROMOTE_TO, PromoteTo, unpklo)
HWY_SVE_FOREACH_UI32(HWY_SVE_PROMOTE_TO, PromoteTo, unpklo)
HWY_SVE_FOREACH_UI64(HWY_SVE_PROMOTE_TO, PromoteTo, unpklo)

// 2x
template <size_t N, int kPow2>
HWY_API svuint32_t PromoteTo(Simd<uint32_t, N, kPow2> dto, svuint8_t vfrom) {
  const RepartitionToWide<DFromV<decltype(vfrom)>> d2;
  return PromoteTo(dto, PromoteTo(d2, vfrom));
}
template <size_t N, int kPow2>
HWY_API svint32_t PromoteTo(Simd<int32_t, N, kPow2> dto, svint8_t vfrom) {
  const RepartitionToWide<DFromV<decltype(vfrom)>> d2;
  return PromoteTo(dto, PromoteTo(d2, vfrom));
}
template <size_t N, int kPow2>
HWY_API svuint32_t U32FromU8(svuint8_t v) {
  return PromoteTo(Simd<uint32_t, N, kPow2>(), v);
}

// Sign change
template <size_t N, int kPow2>
HWY_API svint16_t PromoteTo(Simd<int16_t, N, kPow2> dto, svuint8_t vfrom) {
  const RebindToUnsigned<decltype(dto)> du;
  return BitCast(dto, PromoteTo(du, vfrom));
}
template <size_t N, int kPow2>
HWY_API svint32_t PromoteTo(Simd<int32_t, N, kPow2> dto, svuint16_t vfrom) {
  const RebindToUnsigned<decltype(dto)> du;
  return BitCast(dto, PromoteTo(du, vfrom));
}
template <size_t N, int kPow2>
HWY_API svint32_t PromoteTo(Simd<int32_t, N, kPow2> dto, svuint8_t vfrom) {
  const Repartition<uint16_t, DFromV<decltype(vfrom)>> du16;
  const Repartition<int16_t, decltype(du16)> di16;
  return PromoteTo(dto, BitCast(di16, PromoteTo(du16, vfrom)));
}

// ------------------------------ PromoteTo F

// svcvt* expects inputs in even lanes, whereas Highway wants lower lanes, so
// first replicate each lane once.
namespace detail {
HWY_SVE_FOREACH(HWY_SVE_RETV_ARGVV, ZipLower, zip1)
// Do not use zip2 to implement PromoteUpperTo or similar because vectors may be
// non-powers of two, so getting the actual "upper half" requires MaskUpperHalf.
}  // namespace detail

template <size_t N, int kPow2>
HWY_API svfloat32_t PromoteTo(Simd<float32_t, N, kPow2> /* d */,
                              const svfloat16_t v) {
  const svfloat16_t vv = detail::ZipLower(v, v);
  return svcvt_f32_f16_x(detail::PTrue(Simd<float16_t, N, kPow2>()), vv);
}

template <size_t N, int kPow2>
HWY_API svfloat64_t PromoteTo(Simd<float64_t, N, kPow2> /* d */,
                              const svfloat32_t v) {
  const svfloat32_t vv = detail::ZipLower(v, v);
  return svcvt_f64_f32_x(detail::PTrue(Simd<float32_t, N, kPow2>()), vv);
}

template <size_t N, int kPow2>
HWY_API svfloat64_t PromoteTo(Simd<float64_t, N, kPow2> /* d */,
                              const svint32_t v) {
  const svint32_t vv = detail::ZipLower(v, v);
  return svcvt_f64_s32_x(detail::PTrue(Simd<int32_t, N, kPow2>()), vv);
}

// For 16-bit Compress
namespace detail {
HWY_SVE_FOREACH_UI32(HWY_SVE_PROMOTE_TO, PromoteUpperTo, unpkhi)
#undef HWY_SVE_PROMOTE_TO

template <size_t N, int kPow2>
HWY_API svfloat32_t PromoteUpperTo(Simd<float, N, kPow2> df, svfloat16_t v) {
  const RebindToUnsigned<decltype(df)> du;
  const RepartitionToNarrow<decltype(du)> dn;
  return BitCast(df, PromoteUpperTo(du, BitCast(dn, v)));
}

}  // namespace detail

// ------------------------------ DemoteTo U

namespace detail {

// Saturates unsigned vectors to half/quarter-width TN.
template <typename TN, class VU>
VU SaturateU(VU v) {
  return detail::MinN(v, static_cast<TFromV<VU>>(LimitsMax<TN>()));
}

// Saturates unsigned vectors to half/quarter-width TN.
template <typename TN, class VI>
VI SaturateI(VI v) {
  return detail::MinN(detail::MaxN(v, LimitsMin<TN>()), LimitsMax<TN>());
}

}  // namespace detail

template <size_t N, int kPow2>
HWY_API svuint8_t DemoteTo(Simd<uint8_t, N, kPow2> dn, const svint16_t v) {
  const DFromV<decltype(v)> di;
  const RebindToUnsigned<decltype(di)> du;
  using TN = TFromD<decltype(dn)>;
  // First clamp negative numbers to zero and cast to unsigned.
  const svuint16_t clamped = BitCast(du, detail::MaxN(v, 0));
  // Saturate to unsigned-max and halve the width.
  const svuint8_t vn = BitCast(dn, detail::SaturateU<TN>(clamped));
  return svuzp1_u8(vn, vn);
}

template <size_t N, int kPow2>
HWY_API svuint16_t DemoteTo(Simd<uint16_t, N, kPow2> dn, const svint32_t v) {
  const DFromV<decltype(v)> di;
  const RebindToUnsigned<decltype(di)> du;
  using TN = TFromD<decltype(dn)>;
  // First clamp negative numbers to zero and cast to unsigned.
  const svuint32_t clamped = BitCast(du, detail::MaxN(v, 0));
  // Saturate to unsigned-max and halve the width.
  const svuint16_t vn = BitCast(dn, detail::SaturateU<TN>(clamped));
  return svuzp1_u16(vn, vn);
}

template <size_t N, int kPow2>
HWY_API svuint8_t DemoteTo(Simd<uint8_t, N, kPow2> dn, const svint32_t v) {
  const DFromV<decltype(v)> di;
  const RebindToUnsigned<decltype(di)> du;
  const RepartitionToNarrow<decltype(du)> d2;
  using TN = TFromD<decltype(dn)>;
  // First clamp negative numbers to zero and cast to unsigned.
  const svuint32_t clamped = BitCast(du, detail::MaxN(v, 0));
  // Saturate to unsigned-max and quarter the width.
  const svuint16_t cast16 = BitCast(d2, detail::SaturateU<TN>(clamped));
  const svuint8_t x2 = BitCast(dn, svuzp1_u16(cast16, cast16));
  return svuzp1_u8(x2, x2);
}

HWY_API svuint8_t U8FromU32(const svuint32_t v) {
  const DFromV<svuint32_t> du32;
  const RepartitionToNarrow<decltype(du32)> du16;
  const RepartitionToNarrow<decltype(du16)> du8;

  const svuint16_t cast16 = BitCast(du16, v);
  const svuint16_t x2 = svuzp1_u16(cast16, cast16);
  const svuint8_t cast8 = BitCast(du8, x2);
  return svuzp1_u8(cast8, cast8);
}

// ------------------------------ DemoteTo I

template <size_t N, int kPow2>
HWY_API svint8_t DemoteTo(Simd<int8_t, N, kPow2> dn, const svint16_t v) {
#if HWY_TARGET == HWY_SVE2
  const svint8_t vn = BitCast(dn, svqxtnb_s16(v));
#else
  using TN = TFromD<decltype(dn)>;
  const svint8_t vn = BitCast(dn, detail::SaturateI<TN>(v));
#endif
  return svuzp1_s8(vn, vn);
}

template <size_t N, int kPow2>
HWY_API svint16_t DemoteTo(Simd<int16_t, N, kPow2> dn, const svint32_t v) {
#if HWY_TARGET == HWY_SVE2
  const svint16_t vn = BitCast(dn, svqxtnb_s32(v));
#else
  using TN = TFromD<decltype(dn)>;
  const svint16_t vn = BitCast(dn, detail::SaturateI<TN>(v));
#endif
  return svuzp1_s16(vn, vn);
}

template <size_t N, int kPow2>
HWY_API svint8_t DemoteTo(Simd<int8_t, N, kPow2> dn, const svint32_t v) {
  const RepartitionToWide<decltype(dn)> d2;
#if HWY_TARGET == HWY_SVE2
  const svint16_t cast16 = BitCast(d2, svqxtnb_s16(svqxtnb_s32(v)));
#else
  using TN = TFromD<decltype(dn)>;
  const svint16_t cast16 = BitCast(d2, detail::SaturateI<TN>(v));
#endif
  const svint8_t v2 = BitCast(dn, svuzp1_s16(cast16, cast16));
  return BitCast(dn, svuzp1_s8(v2, v2));
}

// ------------------------------ ConcatEven/ConcatOdd

// WARNING: the upper half of these needs fixing up (uzp1/uzp2 use the
// full vector length, not rounded down to a power of two as we require).
namespace detail {

#define HWY_SVE_CONCAT_EVERY_SECOND(BASE, CHAR, BITS, HALF, NAME, OP) \
  HWY_INLINE HWY_SVE_V(BASE, BITS)                                    \
      NAME(HWY_SVE_V(BASE, BITS) hi, HWY_SVE_V(BASE, BITS) lo) {      \
    return sv##OP##_##CHAR##BITS(lo, hi);                             \
  }
HWY_SVE_FOREACH(HWY_SVE_CONCAT_EVERY_SECOND, ConcatEven, uzp1)
HWY_SVE_FOREACH(HWY_SVE_CONCAT_EVERY_SECOND, ConcatOdd, uzp2)
#undef HWY_SVE_CONCAT_EVERY_SECOND

// Used to slide up / shift whole register left; mask indicates which range
// to take from lo, and the rest is filled from hi starting at its lowest.
#define HWY_SVE_SPLICE(BASE, CHAR, BITS, HALF, NAME, OP)                   \
  HWY_API HWY_SVE_V(BASE, BITS) NAME(                                      \
      HWY_SVE_V(BASE, BITS) hi, HWY_SVE_V(BASE, BITS) lo, svbool_t mask) { \
    return sv##OP##_##CHAR##BITS(mask, lo, hi);                            \
  }
HWY_SVE_FOREACH(HWY_SVE_SPLICE, Splice, splice)
#undef HWY_SVE_SPLICE

}  // namespace detail

template <class D>
HWY_API VFromD<D> ConcatOdd(D d, VFromD<D> hi, VFromD<D> lo) {
#if 0  // if we could assume VL is a power of two
  return detail::ConcatOdd(hi, lo);
#else
  const VFromD<D> hi_odd = detail::ConcatOdd(hi, hi);
  const VFromD<D> lo_odd = detail::ConcatOdd(lo, lo);
  return detail::Splice(hi_odd, lo_odd, FirstN(d, Lanes(d) / 2));
#endif
}

template <class D>
HWY_API VFromD<D> ConcatEven(D d, VFromD<D> hi, VFromD<D> lo) {
#if 0  // if we could assume VL is a power of two
  return detail::ConcatEven(hi, lo);
#else
  const VFromD<D> hi_odd = detail::ConcatEven(hi, hi);
  const VFromD<D> lo_odd = detail::ConcatEven(lo, lo);
  return detail::Splice(hi_odd, lo_odd, FirstN(d, Lanes(d) / 2));
#endif
}

// ------------------------------ DemoteTo F

template <size_t N, int kPow2>
HWY_API svfloat16_t DemoteTo(Simd<float16_t, N, kPow2> d, const svfloat32_t v) {
  const svfloat16_t in_even = svcvt_f16_f32_x(detail::PTrue(d), v);
  return detail::ConcatEven(in_even, in_even);  // only low 1/2 of result valid
}

template <size_t N, int kPow2>
HWY_API svuint16_t DemoteTo(Simd<bfloat16_t, N, kPow2> /* d */, svfloat32_t v) {
  const svuint16_t in_even = BitCast(ScalableTag<uint16_t>(), v);
  return detail::ConcatOdd(in_even, in_even);  // can ignore upper half of vec
}

template <size_t N, int kPow2>
HWY_API svfloat32_t DemoteTo(Simd<float32_t, N, kPow2> d, const svfloat64_t v) {
  const svfloat32_t in_even = svcvt_f32_f64_x(detail::PTrue(d), v);
  return detail::ConcatEven(in_even, in_even);  // only low 1/2 of result valid
}

template <size_t N, int kPow2>
HWY_API svint32_t DemoteTo(Simd<int32_t, N, kPow2> d, const svfloat64_t v) {
  const svint32_t in_even = svcvt_s32_f64_x(detail::PTrue(d), v);
  return detail::ConcatEven(in_even, in_even);  // only low 1/2 of result valid
}

// ------------------------------ ConvertTo F

#define HWY_SVE_CONVERT(BASE, CHAR, BITS, HALF, NAME, OP)                     \
  template <size_t N, int kPow2>                                              \
  HWY_API HWY_SVE_V(BASE, BITS)                                               \
      NAME(HWY_SVE_D(BASE, BITS, N, kPow2) /* d */, HWY_SVE_V(int, BITS) v) { \
    return sv##OP##_##CHAR##BITS##_s##BITS##_x(HWY_SVE_PTRUE(BITS), v);       \
  }                                                                           \
  /* Truncates (rounds toward zero). */                                       \
  template <size_t N, int kPow2>                                              \
  HWY_API HWY_SVE_V(int, BITS)                                                \
      NAME(HWY_SVE_D(int, BITS, N, kPow2) /* d */, HWY_SVE_V(BASE, BITS) v) { \
    return sv##OP##_s##BITS##_##CHAR##BITS##_x(HWY_SVE_PTRUE(BITS), v);       \
  }

// API only requires f32 but we provide f64 for use by Iota.
HWY_SVE_FOREACH_F(HWY_SVE_CONVERT, ConvertTo, cvt)
#undef HWY_SVE_CONVERT

// ------------------------------ NearestInt (Round, ConvertTo)

template <class VF, class DI = RebindToSigned<DFromV<VF>>>
HWY_API VFromD<DI> NearestInt(VF v) {
  // No single instruction, round then truncate.
  return ConvertTo(DI(), Round(v));
}

// ------------------------------ Iota (Add, ConvertTo)

#define HWY_SVE_IOTA(BASE, CHAR, BITS, HALF, NAME, OP)                        \
  template <size_t N, int kPow2>                                              \
  HWY_API HWY_SVE_V(BASE, BITS) NAME(HWY_SVE_D(BASE, BITS, N, kPow2) /* d */, \
                                     HWY_SVE_T(BASE, BITS) first) {           \
    return sv##OP##_##CHAR##BITS(first, 1);                                   \
  }

HWY_SVE_FOREACH_UI(HWY_SVE_IOTA, Iota, index)
#undef HWY_SVE_IOTA

template <class D, HWY_IF_FLOAT_D(D)>
HWY_API VFromD<D> Iota(const D d, TFromD<D> first) {
  const RebindToSigned<D> di;
  return detail::AddN(ConvertTo(d, Iota(di, 0)), first);
}

// ================================================== COMBINE

namespace detail {

template <class D>
svbool_t MaskLowerHalf(D d) {
  return FirstN(d, Lanes(d) / 2);
}
template <class D>
svbool_t MaskUpperHalf(D d) {
  // For Splice to work as intended, make sure bits above Lanes(d) are zero.
  return AndNot(MaskLowerHalf(d), detail::MakeMask(d));
}

// Right-shift vector pair by constexpr; can be used to slide down (=N) or up
// (=Lanes()-N).
#define HWY_SVE_EXT(BASE, CHAR, BITS, HALF, NAME, OP)            \
  template <size_t kIndex>                                       \
  HWY_API HWY_SVE_V(BASE, BITS)                                  \
      NAME(HWY_SVE_V(BASE, BITS) hi, HWY_SVE_V(BASE, BITS) lo) { \
    return sv##OP##_##CHAR##BITS(lo, hi, kIndex);                \
  }
HWY_SVE_FOREACH(HWY_SVE_EXT, Ext, ext)
#undef HWY_SVE_EXT

}  // namespace detail

// ------------------------------ ConcatUpperLower
template <class D, class V>
HWY_API V ConcatUpperLower(const D d, const V hi, const V lo) {
  return IfThenElse(detail::MaskLowerHalf(d), lo, hi);
}

// ------------------------------ ConcatLowerLower
template <class D, class V>
HWY_API V ConcatLowerLower(const D d, const V hi, const V lo) {
  return detail::Splice(hi, lo, detail::MaskLowerHalf(d));
}

// ------------------------------ ConcatLowerUpper
template <class D, class V>
HWY_API V ConcatLowerUpper(const D d, const V hi, const V lo) {
  return detail::Splice(hi, lo, detail::MaskUpperHalf(d));
}

// ------------------------------ ConcatUpperUpper
template <class D, class V>
HWY_API V ConcatUpperUpper(const D d, const V hi, const V lo) {
  const svbool_t mask_upper = detail::MaskUpperHalf(d);
  const V lo_upper = detail::Splice(lo, lo, mask_upper);
  return IfThenElse(mask_upper, hi, lo_upper);
}

// ------------------------------ Combine
template <class D, class V2>
HWY_API VFromD<D> Combine(const D d, const V2 hi, const V2 lo) {
  return ConcatLowerLower(d, hi, lo);
}

// ------------------------------ ZeroExtendVector

template <class D, class V>
HWY_API V ZeroExtendVector(const D d, const V lo) {
  return Combine(d, Zero(Half<D>()), lo);
}

// ------------------------------ Lower/UpperHalf

template <class D2, class V>
HWY_API V LowerHalf(D2 /* tag */, const V v) {
  return v;
}

template <class V>
HWY_API V LowerHalf(const V v) {
  return v;
}

template <class D2, class V>
HWY_API V UpperHalf(const D2 /* d2 */, const V v) {
  return detail::Splice(v, v, detail::MaskUpperHalf(Twice<D2>()));
}

// ================================================== SWIZZLE

// ------------------------------ GetLane

#define HWY_SVE_GET_LANE(BASE, CHAR, BITS, HALF, NAME, OP)      \
  HWY_API HWY_SVE_T(BASE, BITS) NAME(HWY_SVE_V(BASE, BITS) v) { \
    return sv##OP##_##CHAR##BITS(detail::PFalse(), v);          \
  }

HWY_SVE_FOREACH(HWY_SVE_GET_LANE, GetLane, lasta)
#undef HWY_SVE_GET_LANE

// ------------------------------ DupEven

namespace detail {
HWY_SVE_FOREACH(HWY_SVE_RETV_ARGVV, InterleaveEven, trn1)
}  // namespace detail

template <class V>
HWY_API V DupEven(const V v) {
  return detail::InterleaveEven(v, v);
}

// ------------------------------ DupOdd

namespace detail {
HWY_SVE_FOREACH(HWY_SVE_RETV_ARGVV, InterleaveOdd, trn2)
}  // namespace detail

template <class V>
HWY_API V DupOdd(const V v) {
  return detail::InterleaveOdd(v, v);
}

// ------------------------------ OddEven

namespace detail {
HWY_SVE_FOREACH(HWY_SVE_RETV_ARGVN, Insert, insr_n)
}  // namespace detail

template <class V>
HWY_API V OddEven(const V odd, const V even) {
  const auto even_in_odd = detail::Insert(even, 0);
  return detail::InterleaveOdd(even_in_odd, odd);
}

// ------------------------------ OddEvenBlocks
template <class V>
HWY_API V OddEvenBlocks(const V odd, const V even) {
  const RebindToUnsigned<DFromV<V>> du;
  using TU = TFromD<decltype(du)>;
  constexpr size_t kShift = CeilLog2(16 / sizeof(TU));
  const auto idx_block = ShiftRight<kShift>(Iota(du, 0));
  const auto lsb = detail::AndN(idx_block, static_cast<TU>(1));
  const svbool_t is_even = detail::EqN(lsb, static_cast<TU>(0));
  return IfThenElse(is_even, even, odd);
}

// ------------------------------ TableLookupLanes

template <class D, class VI>
HWY_API VFromD<RebindToUnsigned<D>> IndicesFromVec(D d, VI vec) {
  using TI = TFromV<VI>;
  static_assert(sizeof(TFromD<D>) == sizeof(TI), "Index/lane size mismatch");
  const RebindToUnsigned<D> du;
  const auto indices = BitCast(du, vec);
#if HWY_IS_DEBUG_BUILD
  HWY_DASSERT(AllTrue(du, detail::LtN(indices, static_cast<TI>(Lanes(d)))));
#else
  (void)d;
#endif
  return indices;
}

template <class D, typename TI>
HWY_API VFromD<RebindToUnsigned<D>> SetTableIndices(D d, const TI* idx) {
  static_assert(sizeof(TFromD<D>) == sizeof(TI), "Index size must match lane");
  return IndicesFromVec(d, LoadU(Rebind<TI, D>(), idx));
}

// <32bit are not part of Highway API, but used in Broadcast.
#define HWY_SVE_TABLE(BASE, CHAR, BITS, HALF, NAME, OP)          \
  HWY_API HWY_SVE_V(BASE, BITS)                                  \
      NAME(HWY_SVE_V(BASE, BITS) v, HWY_SVE_V(uint, BITS) idx) { \
    return sv##OP##_##CHAR##BITS(v, idx);                        \
  }

HWY_SVE_FOREACH(HWY_SVE_TABLE, TableLookupLanes, tbl)
#undef HWY_SVE_TABLE

// ------------------------------ SwapAdjacentBlocks (TableLookupLanes)

namespace detail {

template <typename T, size_t N, int kPow2>
constexpr size_t LanesPerBlock(Simd<T, N, kPow2> /* tag */) {
  // We might have a capped vector smaller than a block, so honor that.
  return HWY_MIN(16 / sizeof(T), detail::ScaleByPower(N, kPow2));
}

}  // namespace detail

template <class V>
HWY_API V SwapAdjacentBlocks(const V v) {
  const DFromV<V> d;
  const RebindToUnsigned<decltype(d)> du;
  constexpr auto kLanesPerBlock =
      static_cast<TFromV<V>>(detail::LanesPerBlock(d));
  const VFromD<decltype(du)> idx = detail::XorN(Iota(du, 0), kLanesPerBlock);
  return TableLookupLanes(v, idx);
}

// ------------------------------ Reverse

#if 0  // if we could assume VL is a power of two
#error "Update macro"
#endif
#define HWY_SVE_REVERSE(BASE, CHAR, BITS, HALF, NAME, OP)                \
  template <size_t N, int kPow2>                                         \
  HWY_API HWY_SVE_V(BASE, BITS)                                          \
      NAME(HWY_SVE_D(BASE, BITS, N, kPow2) d, HWY_SVE_V(BASE, BITS) v) { \
    const auto reversed = sv##OP##_##CHAR##BITS(v);                      \
    /* Shift right to remove extra (non-pow2 and remainder) lanes. */    \
    const size_t all_lanes =                                             \
        detail::AllHardwareLanes(hwy::SizeTag<BITS / 8>());              \
    /* TODO(janwas): on SVE2, use whilege. */                            \
    /* Avoids FirstN truncating to the return vector size. */            \
    const ScalableTag<HWY_SVE_T(BASE, BITS)> dfull;                      \
    const svbool_t mask = Not(FirstN(dfull, all_lanes - Lanes(d)));      \
    return detail::Splice(reversed, reversed, mask);                     \
  }

HWY_SVE_FOREACH(HWY_SVE_REVERSE, Reverse, rev)
#undef HWY_SVE_REVERSE

// ------------------------------ Reverse2

template <class D, HWY_IF_LANE_SIZE_D(D, 2)>
HWY_API VFromD<D> Reverse2(D d, const VFromD<D> v) {
  const RebindToUnsigned<decltype(d)> du;
  const RepartitionToWide<decltype(du)> dw;
  return BitCast(d, svrevh_u32_x(detail::PTrue(d), BitCast(dw, v)));
}

template <class D, HWY_IF_LANE_SIZE_D(D, 4)>
HWY_API VFromD<D> Reverse2(D d, const VFromD<D> v) {
  const RebindToUnsigned<decltype(d)> du;
  const RepartitionToWide<decltype(du)> dw;
  return BitCast(d, svrevw_u64_x(detail::PTrue(d), BitCast(dw, v)));
}

template <class D, HWY_IF_LANE_SIZE_D(D, 8)>
HWY_API VFromD<D> Reverse2(D /* tag */, const VFromD<D> v) {  // 3210
  const auto even_in_odd = detail::Insert(v, 0);              // 210z
  return detail::InterleaveOdd(v, even_in_odd);               // 2301
}

// ------------------------------ Reverse4 (TableLookupLanes)

// TODO(janwas): is this approach faster than Shuffle0123?
template <class D>
HWY_API VFromD<D> Reverse4(D d, const VFromD<D> v) {
  const RebindToUnsigned<decltype(d)> du;
  const auto idx = detail::XorN(Iota(du, 0), 3);
  return TableLookupLanes(v, idx);
}

// ------------------------------ Reverse8 (TableLookupLanes)

template <class D>
HWY_API VFromD<D> Reverse8(D d, const VFromD<D> v) {
  const RebindToUnsigned<decltype(d)> du;
  const auto idx = detail::XorN(Iota(du, 0), 7);
  return TableLookupLanes(v, idx);
}

// ------------------------------ Compress (PromoteTo)

#define HWY_SVE_COMPRESS(BASE, CHAR, BITS, HALF, NAME, OP)                     \
  HWY_API HWY_SVE_V(BASE, BITS) NAME(HWY_SVE_V(BASE, BITS) v, svbool_t mask) { \
    return sv##OP##_##CHAR##BITS(mask, v);                                     \
  }

HWY_SVE_FOREACH_UIF3264(HWY_SVE_COMPRESS, Compress, compact)
#undef HWY_SVE_COMPRESS

template <class V, HWY_IF_LANE_SIZE_V(V, 2)>
HWY_API V Compress(V v, svbool_t mask16) {
  static_assert(!IsSame<V, svfloat16_t>(), "Must use overload");
  const DFromV<V> d16;

  // Promote vector and mask to 32-bit
  const RepartitionToWide<decltype(d16)> dw;
  const auto v32L = PromoteTo(dw, v);
  const auto v32H = detail::PromoteUpperTo(dw, v);
  const svbool_t mask32L = svunpklo_b(mask16);
  const svbool_t mask32H = svunpkhi_b(mask16);

  const auto compressedL = Compress(v32L, mask32L);
  const auto compressedH = Compress(v32H, mask32H);

  // Demote to 16-bit (already in range) - separately so we can splice
  const V evenL = BitCast(d16, compressedL);
  const V evenH = BitCast(d16, compressedH);
  const V v16L = detail::ConcatEven(evenL, evenL);  // only lower half needed
  const V v16H = detail::ConcatEven(evenH, evenH);

  // We need to combine two vectors of non-constexpr length, so the only option
  // is Splice, which requires us to synthesize a mask. NOTE: this function uses
  // full vectors (SV_ALL instead of SV_POW2), hence we need unmasked svcnt.
  const size_t countL = detail::CountTrueFull(dw, mask32L);
  const auto compressed_maskL = FirstN(d16, countL);
  return detail::Splice(v16H, v16L, compressed_maskL);
}

// Must treat float16_t as integers so we can ConcatEven.
HWY_API svfloat16_t Compress(svfloat16_t v, svbool_t mask16) {
  const DFromV<decltype(v)> df;
  const RebindToSigned<decltype(df)> di;
  return BitCast(df, Compress(BitCast(di, v), mask16));
}

// ------------------------------ CompressStore

template <class V, class M, class D>
HWY_API size_t CompressStore(const V v, const M mask, const D d,
                             TFromD<D>* HWY_RESTRICT unaligned) {
  StoreU(Compress(v, mask), d, unaligned);
  return CountTrue(d, mask);
}

// ------------------------------ CompressBlendedStore

template <class V, class M, class D>
HWY_API size_t CompressBlendedStore(const V v, const M mask, const D d,
                                    TFromD<D>* HWY_RESTRICT unaligned) {
  const size_t count = CountTrue(d, mask);
  const svbool_t store_mask = FirstN(d, count);
  MaskedStore(store_mask, Compress(v, mask), d, unaligned);
  return count;
}

// ================================================== BLOCKWISE

// ------------------------------ CombineShiftRightBytes

namespace detail {

// For x86-compatible behaviour mandated by Highway API: TableLookupBytes
// offsets are implicitly relative to the start of their 128-bit block.
template <class D, class V>
HWY_INLINE V OffsetsOf128BitBlocks(const D d, const V iota0) {
  using T = MakeUnsigned<TFromD<D>>;
  return detail::AndNotN(static_cast<T>(LanesPerBlock(d) - 1), iota0);
}

template <size_t kLanes, class D>
svbool_t FirstNPerBlock(D d) {
  const RebindToSigned<decltype(d)> di;
  constexpr size_t kLanesPerBlock = detail::LanesPerBlock(di);
  const auto idx_mod = detail::AndN(Iota(di, 0), kLanesPerBlock - 1);
  return detail::LtN(BitCast(di, idx_mod), kLanes);
}

}  // namespace detail

template <size_t kBytes, class D, class V = VFromD<D>>
HWY_API V CombineShiftRightBytes(const D d, const V hi, const V lo) {
  const Repartition<uint8_t, decltype(d)> d8;
  const auto hi8 = BitCast(d8, hi);
  const auto lo8 = BitCast(d8, lo);
  const auto hi_up = detail::Splice(hi8, hi8, FirstN(d8, 16 - kBytes));
  const auto lo_down = detail::Ext<kBytes>(lo8, lo8);
  const svbool_t is_lo = detail::FirstNPerBlock<16 - kBytes>(d8);
  return BitCast(d, IfThenElse(is_lo, lo_down, hi_up));
}

// ------------------------------ Shuffle2301

template <class V>
HWY_API V Shuffle2301(const V v) {
  const DFromV<V> d;
  static_assert(sizeof(TFromD<decltype(d)>) == 4, "Defined for 32-bit types");
  return Reverse2(d, v);
}

// ------------------------------ Shuffle2103
template <class V>
HWY_API V Shuffle2103(const V v) {
  const DFromV<V> d;
  const Repartition<uint8_t, decltype(d)> d8;
  static_assert(sizeof(TFromD<decltype(d)>) == 4, "Defined for 32-bit types");
  const svuint8_t v8 = BitCast(d8, v);
  return BitCast(d, CombineShiftRightBytes<12>(d8, v8, v8));
}

// ------------------------------ Shuffle0321
template <class V>
HWY_API V Shuffle0321(const V v) {
  const DFromV<V> d;
  const Repartition<uint8_t, decltype(d)> d8;
  static_assert(sizeof(TFromD<decltype(d)>) == 4, "Defined for 32-bit types");
  const svuint8_t v8 = BitCast(d8, v);
  return BitCast(d, CombineShiftRightBytes<4>(d8, v8, v8));
}

// ------------------------------ Shuffle1032
template <class V>
HWY_API V Shuffle1032(const V v) {
  const DFromV<V> d;
  const Repartition<uint8_t, decltype(d)> d8;
  static_assert(sizeof(TFromD<decltype(d)>) == 4, "Defined for 32-bit types");
  const svuint8_t v8 = BitCast(d8, v);
  return BitCast(d, CombineShiftRightBytes<8>(d8, v8, v8));
}

// ------------------------------ Shuffle01
template <class V>
HWY_API V Shuffle01(const V v) {
  const DFromV<V> d;
  const Repartition<uint8_t, decltype(d)> d8;
  static_assert(sizeof(TFromD<decltype(d)>) == 8, "Defined for 64-bit types");
  const svuint8_t v8 = BitCast(d8, v);
  return BitCast(d, CombineShiftRightBytes<8>(d8, v8, v8));
}

// ------------------------------ Shuffle0123
template <class V>
HWY_API V Shuffle0123(const V v) {
  return Shuffle2301(Shuffle1032(v));
}

// ------------------------------ ReverseBlocks (Reverse, Shuffle01)
template <class D, class V = VFromD<D>>
HWY_API V ReverseBlocks(D d, V v) {
  const Repartition<uint64_t, D> du64;
  return BitCast(d, Shuffle01(Reverse(du64, BitCast(du64, v))));
}

// ------------------------------ TableLookupBytes

template <class V, class VI>
HWY_API VI TableLookupBytes(const V v, const VI idx) {
  const DFromV<VI> d;
  const Repartition<uint8_t, decltype(d)> du8;
  const auto offsets128 = detail::OffsetsOf128BitBlocks(du8, Iota(du8, 0));
  const auto idx8 = Add(BitCast(du8, idx), offsets128);
  return BitCast(d, TableLookupLanes(BitCast(du8, v), idx8));
}

template <class V, class VI>
HWY_API VI TableLookupBytesOr0(const V v, const VI idx) {
  const DFromV<VI> d;
  // Mask size must match vector type, so cast everything to this type.
  const Repartition<int8_t, decltype(d)> di8;

  auto idx8 = BitCast(di8, idx);
  const auto msb = detail::LtN(idx8, 0);

  const auto lookup = TableLookupBytes(BitCast(di8, v), idx8);
  return BitCast(d, IfThenZeroElse(msb, lookup));
}

// ------------------------------ Broadcast

template <int kLane, class V>
HWY_API V Broadcast(const V v) {
  const DFromV<V> d;
  const RebindToUnsigned<decltype(d)> du;
  constexpr size_t kLanesPerBlock = detail::LanesPerBlock(du);
  static_assert(0 <= kLane && kLane < kLanesPerBlock, "Invalid lane");
  auto idx = detail::OffsetsOf128BitBlocks(du, Iota(du, 0));
  if (kLane != 0) {
    idx = detail::AddN(idx, kLane);
  }
  return TableLookupLanes(v, idx);
}

// ------------------------------ ShiftLeftLanes

template <size_t kLanes, class D, class V = VFromD<D>>
HWY_API V ShiftLeftLanes(D d, const V v) {
  const auto zero = Zero(d);
  const auto shifted = detail::Splice(v, zero, FirstN(d, kLanes));
  // Match x86 semantics by zeroing lower lanes in 128-bit blocks
  return IfThenElse(detail::FirstNPerBlock<kLanes>(d), zero, shifted);
}

template <size_t kLanes, class V>
HWY_API V ShiftLeftLanes(const V v) {
  return ShiftLeftLanes<kLanes>(DFromV<V>(), v);
}

// ------------------------------ ShiftRightLanes
template <size_t kLanes, class D, class V = VFromD<D>>
HWY_API V ShiftRightLanes(D d, V v) {
  // For capped/fractional vectors, clear upper lanes so we shift in zeros.
  if (!detail::IsFull(d)) {
    v = IfThenElseZero(detail::MakeMask(d), v);
  }

  const auto shifted = detail::Ext<kLanes>(v, v);
  // Match x86 semantics by zeroing upper lanes in 128-bit blocks
  constexpr size_t kLanesPerBlock = detail::LanesPerBlock(d);
  const svbool_t mask = detail::FirstNPerBlock<kLanesPerBlock - kLanes>(d);
  return IfThenElseZero(mask, shifted);
}

// ------------------------------ ShiftLeftBytes

template <int kBytes, class D, class V = VFromD<D>>
HWY_API V ShiftLeftBytes(const D d, const V v) {
  const Repartition<uint8_t, decltype(d)> d8;
  return BitCast(d, ShiftLeftLanes<kBytes>(BitCast(d8, v)));
}

template <int kBytes, class V>
HWY_API V ShiftLeftBytes(const V v) {
  return ShiftLeftBytes<kBytes>(DFromV<V>(), v);
}

// ------------------------------ ShiftRightBytes
template <int kBytes, class D, class V = VFromD<D>>
HWY_API V ShiftRightBytes(const D d, const V v) {
  const Repartition<uint8_t, decltype(d)> d8;
  return BitCast(d, ShiftRightLanes<kBytes>(d8, BitCast(d8, v)));
}

// ------------------------------ InterleaveLower

template <class D, class V>
HWY_API V InterleaveLower(D d, const V a, const V b) {
  static_assert(IsSame<TFromD<D>, TFromV<V>>(), "D/V mismatch");
  // Move lower halves of blocks to lower half of vector.
  const Repartition<uint64_t, decltype(d)> d64;
  const auto a64 = BitCast(d64, a);
  const auto b64 = BitCast(d64, b);
  const auto a_blocks = detail::ConcatEven(a64, a64);  // only lower half needed
  const auto b_blocks = detail::ConcatEven(b64, b64);

  return detail::ZipLower(BitCast(d, a_blocks), BitCast(d, b_blocks));
}

template <class V>
HWY_API V InterleaveLower(const V a, const V b) {
  return InterleaveLower(DFromV<V>(), a, b);
}

// ------------------------------ InterleaveUpper

// Full vector: guaranteed to have at least one block
template <class D, class V = VFromD<D>,
          hwy::EnableIf<detail::IsFull(D())>* = nullptr>
HWY_API V InterleaveUpper(D d, const V a, const V b) {
  // Move upper halves of blocks to lower half of vector.
  const Repartition<uint64_t, decltype(d)> d64;
  const auto a64 = BitCast(d64, a);
  const auto b64 = BitCast(d64, b);
  const auto a_blocks = detail::ConcatOdd(a64, a64);  // only lower half needed
  const auto b_blocks = detail::ConcatOdd(b64, b64);
  return detail::ZipLower(BitCast(d, a_blocks), BitCast(d, b_blocks));
}

// Capped/fraction: need runtime check
template <class D, class V = VFromD<D>,
          hwy::EnableIf<!detail::IsFull(D())>* = nullptr>
HWY_API V InterleaveUpper(D d, const V a, const V b) {
  // Less than one block: treat as capped
  if (Lanes(d) * sizeof(TFromD<D>) < 16) {
    const Half<decltype(d)> d2;
    return InterleaveLower(d, UpperHalf(d2, a), UpperHalf(d2, b));
  }
  return InterleaveUpper(DFromV<V>(), a, b);
}

// ------------------------------ ZipLower

template <class V, class DW = RepartitionToWide<DFromV<V>>>
HWY_API VFromD<DW> ZipLower(DW dw, V a, V b) {
  const RepartitionToNarrow<DW> dn;
  static_assert(IsSame<TFromD<decltype(dn)>, TFromV<V>>(), "D/V mismatch");
  return BitCast(dw, InterleaveLower(dn, a, b));
}
template <class V, class D = DFromV<V>, class DW = RepartitionToWide<D>>
HWY_API VFromD<DW> ZipLower(const V a, const V b) {
  return BitCast(DW(), InterleaveLower(D(), a, b));
}

// ------------------------------ ZipUpper
template <class V, class DW = RepartitionToWide<DFromV<V>>>
HWY_API VFromD<DW> ZipUpper(DW dw, V a, V b) {
  const RepartitionToNarrow<DW> dn;
  static_assert(IsSame<TFromD<decltype(dn)>, TFromV<V>>(), "D/V mismatch");
  return BitCast(dw, InterleaveUpper(dn, a, b));
}

// ================================================== REDUCE

#define HWY_SVE_REDUCE(BASE, CHAR, BITS, HALF, NAME, OP)                 \
  template <size_t N, int kPow2>                                         \
  HWY_API HWY_SVE_V(BASE, BITS)                                          \
      NAME(HWY_SVE_D(BASE, BITS, N, kPow2) d, HWY_SVE_V(BASE, BITS) v) { \
    return Set(d, static_cast<HWY_SVE_T(BASE, BITS)>(                    \
                      sv##OP##_##CHAR##BITS(detail::MakeMask(d), v)));   \
  }

HWY_SVE_FOREACH(HWY_SVE_REDUCE, SumOfLanes, addv)
HWY_SVE_FOREACH_UI(HWY_SVE_REDUCE, MinOfLanes, minv)
HWY_SVE_FOREACH_UI(HWY_SVE_REDUCE, MaxOfLanes, maxv)
// NaN if all are
HWY_SVE_FOREACH_F(HWY_SVE_REDUCE, MinOfLanes, minnmv)
HWY_SVE_FOREACH_F(HWY_SVE_REDUCE, MaxOfLanes, maxnmv)

#undef HWY_SVE_REDUCE

// ================================================== Ops with dependencies

// ------------------------------ PromoteTo bfloat16 (ZipLower)

template <size_t N, int kPow2>
HWY_API svfloat32_t PromoteTo(Simd<float32_t, N, kPow2> df32,
                              const svuint16_t v) {
  return BitCast(df32, detail::ZipLower(svdup_n_u16(0), v));
}

// ------------------------------ ReorderDemote2To (OddEven)

template <size_t N, int kPow2>
HWY_API svuint16_t ReorderDemote2To(Simd<bfloat16_t, N, kPow2> dbf16,
                                    svfloat32_t a, svfloat32_t b) {
  const RebindToUnsigned<decltype(dbf16)> du16;
  const Repartition<uint32_t, decltype(dbf16)> du32;
  const svuint32_t b_in_even = ShiftRight<16>(BitCast(du32, b));
  return BitCast(dbf16, OddEven(BitCast(du16, a), BitCast(du16, b_in_even)));
}

// ------------------------------ ZeroIfNegative (Lt, IfThenElse)
template <class V>
HWY_API V ZeroIfNegative(const V v) {
  return IfThenZeroElse(detail::LtN(v, 0), v);
}

// ------------------------------ BroadcastSignBit (ShiftRight)
template <class V>
HWY_API V BroadcastSignBit(const V v) {
  return ShiftRight<sizeof(TFromV<V>) * 8 - 1>(v);
}

// ------------------------------ IfNegativeThenElse (BroadcastSignBit)
template <class V>
HWY_API V IfNegativeThenElse(V v, V yes, V no) {
  static_assert(IsSigned<TFromV<V>>(), "Only works for signed/float");
  const DFromV<V> d;
  const RebindToSigned<decltype(d)> di;

  const svbool_t m = MaskFromVec(BitCast(d, BroadcastSignBit(BitCast(di, v))));
  return IfThenElse(m, yes, no);
}

// ------------------------------ AverageRound (ShiftRight)

#if HWY_TARGET == HWY_SVE2
HWY_SVE_FOREACH_U08(HWY_SVE_RETV_ARGPVV, AverageRound, rhadd)
HWY_SVE_FOREACH_U16(HWY_SVE_RETV_ARGPVV, AverageRound, rhadd)
#else
template <class V>
V AverageRound(const V a, const V b) {
  return ShiftRight<1>(detail::AddN(Add(a, b), 1));
}
#endif  // HWY_TARGET == HWY_SVE2

// ------------------------------ LoadMaskBits (TestBit)

// `p` points to at least 8 readable bytes, not all of which need be valid.
template <class D, HWY_IF_LANE_SIZE_D(D, 1)>
HWY_INLINE svbool_t LoadMaskBits(D d, const uint8_t* HWY_RESTRICT bits) {
  const RebindToUnsigned<D> du;
  const svuint8_t iota = Iota(du, 0);

  // Load correct number of bytes (bits/8) with 7 zeros after each.
  const svuint8_t bytes = BitCast(du, svld1ub_u64(detail::PTrue(d), bits));
  // Replicate bytes 8x such that each byte contains the bit that governs it.
  const svuint8_t rep8 = svtbl_u8(bytes, detail::AndNotN(7, iota));

  // 1, 2, 4, 8, 16, 32, 64, 128,  1, 2 ..
  const svuint8_t bit = Shl(Set(du, 1), detail::AndN(iota, 7));

  return TestBit(rep8, bit);
}

template <class D, HWY_IF_LANE_SIZE_D(D, 2)>
HWY_INLINE svbool_t LoadMaskBits(D /* tag */,
                                 const uint8_t* HWY_RESTRICT bits) {
  const RebindToUnsigned<D> du;
  const Repartition<uint8_t, D> du8;

  // There may be up to 128 bits; avoid reading past the end.
  const svuint8_t bytes = svld1(FirstN(du8, (Lanes(du) + 7) / 8), bits);

  // Replicate bytes 16x such that each lane contains the bit that governs it.
  const svuint8_t rep16 = svtbl_u8(bytes, ShiftRight<4>(Iota(du8, 0)));

  // 1, 2, 4, 8, 16, 32, 64, 128,  1, 2 ..
  const svuint16_t bit = Shl(Set(du, 1), detail::AndN(Iota(du, 0), 7));

  return TestBit(BitCast(du, rep16), bit);
}

template <class D, HWY_IF_LANE_SIZE_D(D, 4)>
HWY_INLINE svbool_t LoadMaskBits(D /* tag */,
                                 const uint8_t* HWY_RESTRICT bits) {
  const RebindToUnsigned<D> du;
  const Repartition<uint8_t, D> du8;

  // Upper bound = 2048 bits / 32 bit = 64 bits; at least 8 bytes are readable,
  // so we can skip computing the actual length (Lanes(du)+7)/8.
  const svuint8_t bytes = svld1(FirstN(du8, 8), bits);

  // Replicate bytes 32x such that each lane contains the bit that governs it.
  const svuint8_t rep32 = svtbl_u8(bytes, ShiftRight<5>(Iota(du8, 0)));

  // 1, 2, 4, 8, 16, 32, 64, 128,  1, 2 ..
  const svuint32_t bit = Shl(Set(du, 1), detail::AndN(Iota(du, 0), 7));

  return TestBit(BitCast(du, rep32), bit);
}

template <class D, HWY_IF_LANE_SIZE_D(D, 8)>
HWY_INLINE svbool_t LoadMaskBits(D /* tag */,
                                 const uint8_t* HWY_RESTRICT bits) {
  const RebindToUnsigned<D> du;

  // Max 2048 bits = 32 lanes = 32 input bits; replicate those into each lane.
  // The "at least 8 byte" guarantee in quick_reference ensures this is safe.
  uint32_t mask_bits;
  CopyBytes<4>(bits, &mask_bits);
  const auto vbits = Set(du, mask_bits);

  // 2 ^ {0,1, .., 31}, will not have more lanes than that.
  const svuint64_t bit = Shl(Set(du, 1), Iota(du, 0));

  return TestBit(vbits, bit);
}

// ------------------------------ StoreMaskBits

namespace detail {

// For each mask lane (governing lane type T), store 1 or 0 in BYTE lanes.
template <class T, HWY_IF_LANE_SIZE(T, 1)>
HWY_INLINE svuint8_t BoolFromMask(svbool_t m) {
  return svdup_n_u8_z(m, 1);
}
template <class T, HWY_IF_LANE_SIZE(T, 2)>
HWY_INLINE svuint8_t BoolFromMask(svbool_t m) {
  const ScalableTag<uint8_t> d8;
  const svuint8_t b16 = BitCast(d8, svdup_n_u16_z(m, 1));
  return detail::ConcatEven(b16, b16);  // only lower half needed
}
template <class T, HWY_IF_LANE_SIZE(T, 4)>
HWY_INLINE svuint8_t BoolFromMask(svbool_t m) {
  return U8FromU32(svdup_n_u32_z(m, 1));
}
template <class T, HWY_IF_LANE_SIZE(T, 8)>
HWY_INLINE svuint8_t BoolFromMask(svbool_t m) {
  const ScalableTag<uint32_t> d32;
  const svuint32_t b64 = BitCast(d32, svdup_n_u64_z(m, 1));
  return U8FromU32(detail::ConcatEven(b64, b64));  // only lower half needed
}

// Compacts groups of 8 u8 into 8 contiguous bits in a 64-bit lane.
HWY_INLINE svuint64_t BitsFromBool(svuint8_t x) {
  const ScalableTag<uint8_t> d8;
  const ScalableTag<uint16_t> d16;
  const ScalableTag<uint32_t> d32;
  const ScalableTag<uint64_t> d64;
  // TODO(janwas): could use SVE2 BDEP, but it's optional.
  x = Or(x, BitCast(d8, ShiftRight<7>(BitCast(d16, x))));
  x = Or(x, BitCast(d8, ShiftRight<14>(BitCast(d32, x))));
  x = Or(x, BitCast(d8, ShiftRight<28>(BitCast(d64, x))));
  return BitCast(d64, x);
}

}  // namespace detail

// `p` points to at least 8 writable bytes.
template <class D>
HWY_API size_t StoreMaskBits(D d, svbool_t m, uint8_t* bits) {
  svuint64_t bits_in_u64 =
      detail::BitsFromBool(detail::BoolFromMask<TFromD<D>>(m));

  const size_t num_bits = Lanes(d);
  const size_t num_bytes = (num_bits + 8 - 1) / 8;  // Round up, see below

  // Truncate each u64 to 8 bits and store to u8.
  svst1b_u64(FirstN(ScalableTag<uint64_t>(), num_bytes), bits, bits_in_u64);

  // Non-full byte, need to clear the undefined upper bits. Can happen for
  // capped/fractional vectors or large T and small hardware vectors.
  if (num_bits < 8) {
    const int mask = (1 << num_bits) - 1;
    bits[0] = static_cast<uint8_t>(bits[0] & mask);
  }
  // Else: we wrote full bytes because num_bits is a power of two >= 8.

  return num_bytes;
}

// ------------------------------ CompressBits, CompressBitsStore (LoadMaskBits)

template <class V>
HWY_INLINE V CompressBits(V v, const uint8_t* HWY_RESTRICT bits) {
  return Compress(v, LoadMaskBits(DFromV<V>(), bits));
}

template <class D>
HWY_API size_t CompressBitsStore(VFromD<D> v, const uint8_t* HWY_RESTRICT bits,
                                 D d, TFromD<D>* HWY_RESTRICT unaligned) {
  return CompressStore(v, LoadMaskBits(d, bits), d, unaligned);
}

// ------------------------------ MulEven (InterleaveEven)

#if HWY_TARGET == HWY_SVE2
namespace detail {
#define HWY_SVE_MUL_EVEN(BASE, CHAR, BITS, HALF, NAME, OP)     \
  HWY_API HWY_SVE_V(BASE, BITS)                                \
      NAME(HWY_SVE_V(BASE, HALF) a, HWY_SVE_V(BASE, HALF) b) { \
    return sv##OP##_##CHAR##BITS(a, b);                        \
  }

HWY_SVE_FOREACH_UI64(HWY_SVE_MUL_EVEN, MulEven, mullb)
#undef HWY_SVE_MUL_EVEN
}  // namespace detail
#endif

template <class V, class DW = RepartitionToWide<DFromV<V>>>
HWY_API VFromD<DW> MulEven(const V a, const V b) {
#if HWY_TARGET == HWY_SVE2
  return BitCast(DW(), detail::MulEven(a, b));
#else
  const auto lo = Mul(a, b);
  const auto hi = detail::MulHigh(a, b);
  return BitCast(DW(), detail::InterleaveEven(lo, hi));
#endif
}

HWY_API svuint64_t MulEven(const svuint64_t a, const svuint64_t b) {
  const auto lo = Mul(a, b);
  const auto hi = detail::MulHigh(a, b);
  return detail::InterleaveEven(lo, hi);
}

HWY_API svuint64_t MulOdd(const svuint64_t a, const svuint64_t b) {
  const auto lo = Mul(a, b);
  const auto hi = detail::MulHigh(a, b);
  return detail::InterleaveOdd(lo, hi);
}

// ------------------------------ ReorderWidenMulAccumulate (MulAdd, ZipLower)

template <size_t N, int kPow2>
HWY_API svfloat32_t ReorderWidenMulAccumulate(Simd<float, N, kPow2> df32,
                                              svuint16_t a, svuint16_t b,
                                              const svfloat32_t sum0,
                                              svfloat32_t& sum1) {
  // TODO(janwas): svbfmlalb_f32 if __ARM_FEATURE_SVE_BF16.
  const Repartition<uint16_t, decltype(df32)> du16;
  const RebindToUnsigned<decltype(df32)> du32;
  const svuint16_t zero = Zero(du16);
  const svuint32_t a0 = ZipLower(du32, zero, BitCast(du16, a));
  const svuint32_t a1 = ZipUpper(du32, zero, BitCast(du16, a));
  const svuint32_t b0 = ZipLower(du32, zero, BitCast(du16, b));
  const svuint32_t b1 = ZipUpper(du32, zero, BitCast(du16, b));
  sum1 = MulAdd(BitCast(df32, a1), BitCast(df32, b1), sum1);
  return MulAdd(BitCast(df32, a0), BitCast(df32, b0), sum0);
}

// ------------------------------ AESRound / CLMul

#if defined(__ARM_FEATURE_SVE2_AES)

// Per-target flag to prevent generic_ops-inl.h from defining AESRound.
#ifdef HWY_NATIVE_AES
#undef HWY_NATIVE_AES
#else
#define HWY_NATIVE_AES
#endif

HWY_API svuint8_t AESRound(svuint8_t state, svuint8_t round_key) {
  // It is not clear whether E and MC fuse like they did on NEON.
  const svuint8_t zero = svdup_n_u8(0);
  return Xor(svaesmc_u8(svaese_u8(state, zero)), round_key);
}

HWY_API svuint8_t AESLastRound(svuint8_t state, svuint8_t round_key) {
  return Xor(svaese_u8(state, svdup_n_u8(0)), round_key);
}

HWY_API svuint64_t CLMulLower(const svuint64_t a, const svuint64_t b) {
  return svpmullb_pair(a, b);
}

HWY_API svuint64_t CLMulUpper(const svuint64_t a, const svuint64_t b) {
  return svpmullt_pair(a, b);
}

#endif  // __ARM_FEATURE_SVE2_AES

// ------------------------------ Lt128

template <class D>
HWY_INLINE svbool_t Lt128(D /* d */, const svuint64_t a, const svuint64_t b) {
  static_assert(!IsSigned<TFromD<D>>() && sizeof(TFromD<D>) == 8, "Use u64");
  // Truth table of Eq and Compare for Hi and Lo u64.
  // (removed lines with (=H && cH) or (=L && cL) - cannot both be true)
  // =H =L cH cL  | out = cH | (=H & cL) = IfThenElse(=H, cL, cH)
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
  const svbool_t eqHL = Eq(a, b);
  const svbool_t ltHL = Lt(a, b);
  // trn (interleave even/odd) allow us to move and copy masks across lanes.
  const svbool_t cmpLL = svtrn1_b64(ltHL, ltHL);
  const svbool_t outHx = svsel_b(eqHL, cmpLL, ltHL);   // See truth table above.
  return svtrn2_b64(outHx, outHx);                     // replicate to HH
}

// ------------------------------ Min128, Max128 (Lt128)

template <class D>
HWY_INLINE svuint64_t Min128(D d, const svuint64_t a, const svuint64_t b) {
  return IfThenElse(Lt128(d, a, b), a, b);
}

template <class D>
HWY_INLINE svuint64_t Max128(D d, const svuint64_t a, const svuint64_t b) {
  return IfThenElse(Lt128(d, a, b), b, a);
}

// ================================================== END MACROS
namespace detail {  // for code folding
#undef HWY_IF_FLOAT_V
#undef HWY_IF_LANE_SIZE_V
#undef HWY_IF_SIGNED_V
#undef HWY_IF_UNSIGNED_V
#undef HWY_SVE_D
#undef HWY_SVE_FOREACH
#undef HWY_SVE_FOREACH_F
#undef HWY_SVE_FOREACH_F16
#undef HWY_SVE_FOREACH_F32
#undef HWY_SVE_FOREACH_F64
#undef HWY_SVE_FOREACH_I
#undef HWY_SVE_FOREACH_I08
#undef HWY_SVE_FOREACH_I16
#undef HWY_SVE_FOREACH_I32
#undef HWY_SVE_FOREACH_I64
#undef HWY_SVE_FOREACH_IF
#undef HWY_SVE_FOREACH_U
#undef HWY_SVE_FOREACH_U08
#undef HWY_SVE_FOREACH_U16
#undef HWY_SVE_FOREACH_U32
#undef HWY_SVE_FOREACH_U64
#undef HWY_SVE_FOREACH_UI
#undef HWY_SVE_FOREACH_UI08
#undef HWY_SVE_FOREACH_UI16
#undef HWY_SVE_FOREACH_UI32
#undef HWY_SVE_FOREACH_UI64
#undef HWY_SVE_FOREACH_UIF3264
#undef HWY_SVE_PTRUE
#undef HWY_SVE_RETV_ARGPV
#undef HWY_SVE_RETV_ARGPVN
#undef HWY_SVE_RETV_ARGPVV
#undef HWY_SVE_RETV_ARGV
#undef HWY_SVE_RETV_ARGVN
#undef HWY_SVE_RETV_ARGVV
#undef HWY_SVE_T
#undef HWY_SVE_UNDEFINED
#undef HWY_SVE_V

}  // namespace detail
// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();
