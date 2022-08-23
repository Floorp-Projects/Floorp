// Copyright 2021 Google LLC
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

// Target-independent types/functions defined after target-specific ops.

// Relies on the external include guard in highway.h.
HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

// The lane type of a vector type, e.g. float for Vec<ScalableTag<float>>.
template <class V>
using LaneType = decltype(GetLane(V()));

// Vector type, e.g. Vec128<float> for CappedTag<float, 4>. Useful as the return
// type of functions that do not take a vector argument, or as an argument type
// if the function only has a template argument for D, or for explicit type
// names instead of auto. This may be a built-in type.
template <class D>
using Vec = decltype(Zero(D()));

// Mask type. Useful as the return type of functions that do not take a mask
// argument, or as an argument type if the function only has a template argument
// for D, or for explicit type names instead of auto.
template <class D>
using Mask = decltype(MaskFromVec(Zero(D())));

// Returns the closest value to v within [lo, hi].
template <class V>
HWY_API V Clamp(const V v, const V lo, const V hi) {
  return Min(Max(lo, v), hi);
}

// CombineShiftRightBytes (and -Lanes) are not available for the scalar target,
// and RVV has its own implementation of -Lanes.
#if HWY_TARGET != HWY_SCALAR && HWY_TARGET != HWY_RVV

template <size_t kLanes, class D, class V = VFromD<D>>
HWY_API V CombineShiftRightLanes(D d, const V hi, const V lo) {
  constexpr size_t kBytes = kLanes * sizeof(LaneType<V>);
  static_assert(kBytes < 16, "Shift count is per-block");
  return CombineShiftRightBytes<kBytes>(d, hi, lo);
}

#endif

// Returns lanes with the most significant bit set and all other bits zero.
template <class D>
HWY_API Vec<D> SignBit(D d) {
  const RebindToUnsigned<decltype(d)> du;
  return BitCast(d, Set(du, SignMask<TFromD<D>>()));
}

// Returns quiet NaN.
template <class D>
HWY_API Vec<D> NaN(D d) {
  const RebindToSigned<D> di;
  // LimitsMax sets all exponent and mantissa bits to 1. The exponent plus
  // mantissa MSB (to indicate quiet) would be sufficient.
  return BitCast(d, Set(di, LimitsMax<TFromD<decltype(di)>>()));
}

// Returns positive infinity.
template <class D>
HWY_API Vec<D> Inf(D d) {
  const RebindToUnsigned<D> du;
  using T = TFromD<D>;
  using TU = TFromD<decltype(du)>;
  const TU max_x2 = static_cast<TU>(MaxExponentTimes2<T>());
  return BitCast(d, Set(du, max_x2 >> 1));
}

// ------------------------------ SafeFillN

template <class D, typename T = TFromD<D>>
HWY_API void SafeFillN(const size_t num, const T value, D d,
                       T* HWY_RESTRICT to) {
#if HWY_MEM_OPS_MIGHT_FAULT
  (void)d;
  for (size_t i = 0; i < num; ++i) {
    to[i] = value;
  }
#else
  BlendedStore(Set(d, value), FirstN(d, num), d, to);
#endif
}

// ------------------------------ SafeCopyN

template <class D, typename T = TFromD<D>>
HWY_API void SafeCopyN(const size_t num, D d, const T* HWY_RESTRICT from,
                       T* HWY_RESTRICT to) {
#if HWY_MEM_OPS_MIGHT_FAULT
  (void)d;
  for (size_t i = 0; i < num; ++i) {
    to[i] = from[i];
  }
#else
  const Mask<D> mask = FirstN(d, num);
  BlendedStore(MaskedLoad(mask, d, from), mask, d, to);
#endif
}

// "Include guard": skip if native instructions are available. The generic
// implementation is currently shared between x86_* and wasm_*, and is too large
// to duplicate.

#if (defined(HWY_NATIVE_LOAD_STORE_INTERLEAVED) == defined(HWY_TARGET_TOGGLE))
#ifdef HWY_NATIVE_LOAD_STORE_INTERLEAVED
#undef HWY_NATIVE_LOAD_STORE_INTERLEAVED
#else
#define HWY_NATIVE_LOAD_STORE_INTERLEAVED
#endif

// ------------------------------ LoadInterleaved2

template <typename T, size_t N, class V>
HWY_API void LoadInterleaved2(Simd<T, N, 0> d, const T* HWY_RESTRICT unaligned,
                              V& v0, V& v1) {
  const V A = LoadU(d, unaligned + 0 * N);  // v1[1] v0[1] v1[0] v0[0]
  const V B = LoadU(d, unaligned + 1 * N);
  v0 = ConcatEven(d, B, A);
  v1 = ConcatOdd(d, B, A);
}

template <typename T, class V>
HWY_API void LoadInterleaved2(Simd<T, 1, 0> d, const T* HWY_RESTRICT unaligned,
                              V& v0, V& v1) {
  v0 = LoadU(d, unaligned + 0);
  v1 = LoadU(d, unaligned + 1);
}

// ------------------------------ LoadInterleaved3 (CombineShiftRightBytes)

namespace detail {

// Default for <= 128-bit vectors; x86_256 and x86_512 have their own overload.
template <typename T, size_t N, class V, HWY_IF_LE128(T, N)>
HWY_API void LoadTransposedBlocks3(Simd<T, N, 0> d,
                                   const T* HWY_RESTRICT unaligned, V& A, V& B,
                                   V& C) {
  A = LoadU(d, unaligned + 0 * N);
  B = LoadU(d, unaligned + 1 * N);
  C = LoadU(d, unaligned + 2 * N);
}

}  // namespace detail

template <typename T, size_t N, class V, HWY_IF_LANES_PER_BLOCK(T, N, 16)>
HWY_API void LoadInterleaved3(Simd<T, N, 0> d, const T* HWY_RESTRICT unaligned,
                              V& v0, V& v1, V& v2) {
  const RebindToUnsigned<decltype(d)> du;
  // Compact notation so these fit on one line: 12 := v1[2].
  V A;  // 05 24 14 04 23 13 03 22 12 02 21 11 01 20 10 00
  V B;  // 1a 0a 29 19 09 28 18 08 27 17 07 26 16 06 25 15
  V C;  // 2f 1f 0f 2e 1e 0e 2d 1d 0d 2c 1c 0c 2b 1b 0b 2a
  detail::LoadTransposedBlocks3(d, unaligned, A, B, C);
  // Compress all lanes belonging to v0 into consecutive lanes.
  constexpr uint8_t Z = 0x80;
  alignas(16) constexpr uint8_t kIdx_v0A[16] = {0, 3, 6, 9, 12, 15, Z, Z,
                                                Z, Z, Z, Z, Z,  Z,  Z, Z};
  alignas(16) constexpr uint8_t kIdx_v0B[16] = {Z, Z,  Z,  Z, Z, Z, 2, 5,
                                                8, 11, 14, Z, Z, Z, Z, Z};
  alignas(16) constexpr uint8_t kIdx_v0C[16] = {Z, Z, Z, Z, Z, Z, Z,  Z,
                                                Z, Z, Z, 1, 4, 7, 10, 13};
  alignas(16) constexpr uint8_t kIdx_v1A[16] = {1, 4, 7, 10, 13, Z, Z, Z,
                                                Z, Z, Z, Z,  Z,  Z, Z, Z};
  alignas(16) constexpr uint8_t kIdx_v1B[16] = {Z, Z,  Z,  Z, Z, 0, 3, 6,
                                                9, 12, 15, Z, Z, Z, Z, Z};
  alignas(16) constexpr uint8_t kIdx_v1C[16] = {Z, Z, Z, Z, Z, Z, Z,  Z,
                                                Z, Z, Z, 2, 5, 8, 11, 14};
  alignas(16) constexpr uint8_t kIdx_v2A[16] = {2, 5, 8, 11, 14, Z, Z, Z,
                                                Z, Z, Z, Z,  Z,  Z, Z, Z};
  alignas(16) constexpr uint8_t kIdx_v2B[16] = {Z,  Z,  Z, Z, Z, 1, 4, 7,
                                                10, 13, Z, Z, Z, Z, Z, Z};
  alignas(16) constexpr uint8_t kIdx_v2C[16] = {Z, Z, Z, Z, Z, Z, Z,  Z,
                                                Z, Z, 0, 3, 6, 9, 12, 15};
  const V v0L = BitCast(d, TableLookupBytesOr0(A, LoadDup128(du, kIdx_v0A)));
  const V v0M = BitCast(d, TableLookupBytesOr0(B, LoadDup128(du, kIdx_v0B)));
  const V v0U = BitCast(d, TableLookupBytesOr0(C, LoadDup128(du, kIdx_v0C)));
  const V v1L = BitCast(d, TableLookupBytesOr0(A, LoadDup128(du, kIdx_v1A)));
  const V v1M = BitCast(d, TableLookupBytesOr0(B, LoadDup128(du, kIdx_v1B)));
  const V v1U = BitCast(d, TableLookupBytesOr0(C, LoadDup128(du, kIdx_v1C)));
  const V v2L = BitCast(d, TableLookupBytesOr0(A, LoadDup128(du, kIdx_v2A)));
  const V v2M = BitCast(d, TableLookupBytesOr0(B, LoadDup128(du, kIdx_v2B)));
  const V v2U = BitCast(d, TableLookupBytesOr0(C, LoadDup128(du, kIdx_v2C)));
  v0 = Or3(v0L, v0M, v0U);
  v1 = Or3(v1L, v1M, v1U);
  v2 = Or3(v2L, v2M, v2U);
}

// 8-bit lanes x8
template <typename T, size_t N, class V, HWY_IF_LANE_SIZE(T, 1),
          HWY_IF_LANES_PER_BLOCK(T, N, 8)>
HWY_API void LoadInterleaved3(Simd<T, N, 0> d, const T* HWY_RESTRICT unaligned,
                              V& v0, V& v1, V& v2) {
  const RebindToUnsigned<decltype(d)> du;
  V A;  // v1[2] v0[2] v2[1] v1[1] v0[1] v2[0] v1[0] v0[0]
  V B;  // v0[5] v2[4] v1[4] v0[4] v2[3] v1[3] v0[3] v2[2]
  V C;  // v2[7] v1[7] v0[7] v2[6] v1[6] v0[6] v2[5] v1[5]
  detail::LoadTransposedBlocks3(d, unaligned, A, B, C);
  // Compress all lanes belonging to v0 into consecutive lanes.
  constexpr uint8_t Z = 0x80;
  alignas(16) constexpr uint8_t kIdx_v0A[16] = {0, 3, 6, Z, Z, Z, Z, Z};
  alignas(16) constexpr uint8_t kIdx_v0B[16] = {Z, Z, Z, 1, 4, 7, Z, Z};
  alignas(16) constexpr uint8_t kIdx_v0C[16] = {Z, Z, Z, Z, Z, Z, 2, 5};
  alignas(16) constexpr uint8_t kIdx_v1A[16] = {1, 4, 7, Z, Z, Z, Z, Z};
  alignas(16) constexpr uint8_t kIdx_v1B[16] = {Z, Z, Z, 2, 5, Z, Z, Z};
  alignas(16) constexpr uint8_t kIdx_v1C[16] = {Z, Z, Z, Z, Z, 0, 3, 6};
  alignas(16) constexpr uint8_t kIdx_v2A[16] = {2, 5, Z, Z, Z, Z, Z, Z};
  alignas(16) constexpr uint8_t kIdx_v2B[16] = {Z, Z, 0, 3, 6, Z, Z, Z};
  alignas(16) constexpr uint8_t kIdx_v2C[16] = {Z, Z, Z, Z, Z, 1, 4, 7};
  const V v0L = BitCast(d, TableLookupBytesOr0(A, LoadDup128(du, kIdx_v0A)));
  const V v0M = BitCast(d, TableLookupBytesOr0(B, LoadDup128(du, kIdx_v0B)));
  const V v0U = BitCast(d, TableLookupBytesOr0(C, LoadDup128(du, kIdx_v0C)));
  const V v1L = BitCast(d, TableLookupBytesOr0(A, LoadDup128(du, kIdx_v1A)));
  const V v1M = BitCast(d, TableLookupBytesOr0(B, LoadDup128(du, kIdx_v1B)));
  const V v1U = BitCast(d, TableLookupBytesOr0(C, LoadDup128(du, kIdx_v1C)));
  const V v2L = BitCast(d, TableLookupBytesOr0(A, LoadDup128(du, kIdx_v2A)));
  const V v2M = BitCast(d, TableLookupBytesOr0(B, LoadDup128(du, kIdx_v2B)));
  const V v2U = BitCast(d, TableLookupBytesOr0(C, LoadDup128(du, kIdx_v2C)));
  v0 = Or3(v0L, v0M, v0U);
  v1 = Or3(v1L, v1M, v1U);
  v2 = Or3(v2L, v2M, v2U);
}

// 16-bit lanes x8
template <typename T, size_t N, class V, HWY_IF_LANE_SIZE(T, 2),
          HWY_IF_LANES_PER_BLOCK(T, N, 8)>
HWY_API void LoadInterleaved3(Simd<T, N, 0> d, const T* HWY_RESTRICT unaligned,
                              V& v0, V& v1, V& v2) {
  const RebindToUnsigned<decltype(d)> du;
  V A;  // v1[2] v0[2] v2[1] v1[1] v0[1] v2[0] v1[0] v0[0]
  V B;  // v0[5] v2[4] v1[4] v0[4] v2[3] v1[3] v0[3] v2[2]
  V C;  // v2[7] v1[7] v0[7] v2[6] v1[6] v0[6] v2[5] v1[5]
  detail::LoadTransposedBlocks3(d, unaligned, A, B, C);
  // Compress all lanes belonging to v0 into consecutive lanes. Same as above,
  // but each element of the array contains two byte indices for a lane.
  constexpr uint16_t Z = 0x8080;
  alignas(16) constexpr uint16_t kIdx_v0A[8] = {0x0100, 0x0706, 0x0D0C, Z,
                                                Z,      Z,      Z,      Z};
  alignas(16) constexpr uint16_t kIdx_v0B[8] = {Z,      Z,      Z, 0x0302,
                                                0x0908, 0x0F0E, Z, Z};
  alignas(16) constexpr uint16_t kIdx_v0C[8] = {Z, Z, Z,      Z,
                                                Z, Z, 0x0504, 0x0B0A};
  alignas(16) constexpr uint16_t kIdx_v1A[8] = {0x0302, 0x0908, 0x0F0E, Z,
                                                Z,      Z,      Z,      Z};
  alignas(16) constexpr uint16_t kIdx_v1B[8] = {Z,      Z, Z, 0x0504,
                                                0x0B0A, Z, Z, Z};
  alignas(16) constexpr uint16_t kIdx_v1C[8] = {Z, Z,      Z,      Z,
                                                Z, 0x0100, 0x0706, 0x0D0C};
  alignas(16) constexpr uint16_t kIdx_v2A[8] = {0x0504, 0x0B0A, Z, Z,
                                                Z,      Z,      Z, Z};
  alignas(16) constexpr uint16_t kIdx_v2B[8] = {Z,      Z, 0x0100, 0x0706,
                                                0x0D0C, Z, Z,      Z};
  alignas(16) constexpr uint16_t kIdx_v2C[8] = {Z, Z,      Z,      Z,
                                                Z, 0x0302, 0x0908, 0x0F0E};
  const V v0L = BitCast(d, TableLookupBytesOr0(A, LoadDup128(du, kIdx_v0A)));
  const V v0M = BitCast(d, TableLookupBytesOr0(B, LoadDup128(du, kIdx_v0B)));
  const V v0U = BitCast(d, TableLookupBytesOr0(C, LoadDup128(du, kIdx_v0C)));
  const V v1L = BitCast(d, TableLookupBytesOr0(A, LoadDup128(du, kIdx_v1A)));
  const V v1M = BitCast(d, TableLookupBytesOr0(B, LoadDup128(du, kIdx_v1B)));
  const V v1U = BitCast(d, TableLookupBytesOr0(C, LoadDup128(du, kIdx_v1C)));
  const V v2L = BitCast(d, TableLookupBytesOr0(A, LoadDup128(du, kIdx_v2A)));
  const V v2M = BitCast(d, TableLookupBytesOr0(B, LoadDup128(du, kIdx_v2B)));
  const V v2U = BitCast(d, TableLookupBytesOr0(C, LoadDup128(du, kIdx_v2C)));
  v0 = Or3(v0L, v0M, v0U);
  v1 = Or3(v1L, v1M, v1U);
  v2 = Or3(v2L, v2M, v2U);
}

template <typename T, size_t N, class V, HWY_IF_LANES_PER_BLOCK(T, N, 4)>
HWY_API void LoadInterleaved3(Simd<T, N, 0> d, const T* HWY_RESTRICT unaligned,
                              V& v0, V& v1, V& v2) {
  V A;  // v0[1] v2[0] v1[0] v0[0]
  V B;  // v1[2] v0[2] v2[1] v1[1]
  V C;  // v2[3] v1[3] v0[3] v2[2]
  detail::LoadTransposedBlocks3(d, unaligned, A, B, C);

  const V vxx_02_03_xx = OddEven(C, B);
  v0 = detail::Shuffle1230(A, vxx_02_03_xx);

  // Shuffle2301 takes the upper/lower halves of the output from one input, so
  // we cannot just combine 13 and 10 with 12 and 11 (similar to v0/v2). Use
  // OddEven because it may have higher throughput than Shuffle.
  const V vxx_xx_10_11 = OddEven(A, B);
  const V v12_13_xx_xx = OddEven(B, C);
  v1 = detail::Shuffle2301(vxx_xx_10_11, v12_13_xx_xx);

  const V vxx_20_21_xx = OddEven(B, A);
  v2 = detail::Shuffle3012(vxx_20_21_xx, C);
}

template <typename T, size_t N, class V, HWY_IF_LANES_PER_BLOCK(T, N, 2)>
HWY_API void LoadInterleaved3(Simd<T, N, 0> d, const T* HWY_RESTRICT unaligned,
                              V& v0, V& v1, V& v2) {
  V A;  // v1[0] v0[0]
  V B;  // v0[1] v2[0]
  V C;  // v2[1] v1[1]
  detail::LoadTransposedBlocks3(d, unaligned, A, B, C);
  v0 = OddEven(B, A);
  v1 = CombineShiftRightBytes<sizeof(T)>(d, C, A);
  v2 = OddEven(C, B);
}

template <typename T, class V>
HWY_API void LoadInterleaved3(Simd<T, 1, 0> d, const T* HWY_RESTRICT unaligned,
                              V& v0, V& v1, V& v2) {
  v0 = LoadU(d, unaligned + 0);
  v1 = LoadU(d, unaligned + 1);
  v2 = LoadU(d, unaligned + 2);
}

// ------------------------------ LoadInterleaved4

namespace detail {

// Default for <= 128-bit vectors; x86_256 and x86_512 have their own overload.
template <typename T, size_t N, class V, HWY_IF_LE128(T, N)>
HWY_API void LoadTransposedBlocks4(Simd<T, N, 0> d,
                                   const T* HWY_RESTRICT unaligned, V& A, V& B,
                                   V& C, V& D) {
  A = LoadU(d, unaligned + 0 * N);
  B = LoadU(d, unaligned + 1 * N);
  C = LoadU(d, unaligned + 2 * N);
  D = LoadU(d, unaligned + 3 * N);
}

}  // namespace detail

template <typename T, size_t N, class V, HWY_IF_LANES_PER_BLOCK(T, N, 16)>
HWY_API void LoadInterleaved4(Simd<T, N, 0> d, const T* HWY_RESTRICT unaligned,
                              V& v0, V& v1, V& v2, V& v3) {
  const Repartition<uint64_t, decltype(d)> d64;
  using V64 = VFromD<decltype(d64)>;
  // 16 lanes per block; the lowest four blocks are at the bottom of A,B,C,D.
  // Here int[i] means the four interleaved values of the i-th 4-tuple and
  // int[3..0] indicates four consecutive 4-tuples (0 = least-significant).
  V A;  // int[13..10] int[3..0]
  V B;  // int[17..14] int[7..4]
  V C;  // int[1b..18] int[b..8]
  V D;  // int[1f..1c] int[f..c]
  detail::LoadTransposedBlocks4(d, unaligned, A, B, C, D);

  // For brevity, the comments only list the lower block (upper = lower + 0x10)
  const V v5140 = InterleaveLower(d, A, B);  // int[5,1,4,0]
  const V vd9c8 = InterleaveLower(d, C, D);  // int[d,9,c,8]
  const V v7362 = InterleaveUpper(d, A, B);  // int[7,3,6,2]
  const V vfbea = InterleaveUpper(d, C, D);  // int[f,b,e,a]

  const V v6420 = InterleaveLower(d, v5140, v7362);  // int[6,4,2,0]
  const V veca8 = InterleaveLower(d, vd9c8, vfbea);  // int[e,c,a,8]
  const V v7531 = InterleaveUpper(d, v5140, v7362);  // int[7,5,3,1]
  const V vfdb9 = InterleaveUpper(d, vd9c8, vfbea);  // int[f,d,b,9]

  const V64 v10L = BitCast(d64, InterleaveLower(d, v6420, v7531));  // v10[7..0]
  const V64 v10U = BitCast(d64, InterleaveLower(d, veca8, vfdb9));  // v10[f..8]
  const V64 v32L = BitCast(d64, InterleaveUpper(d, v6420, v7531));  // v32[7..0]
  const V64 v32U = BitCast(d64, InterleaveUpper(d, veca8, vfdb9));  // v32[f..8]

  v0 = BitCast(d, InterleaveLower(d64, v10L, v10U));
  v1 = BitCast(d, InterleaveUpper(d64, v10L, v10U));
  v2 = BitCast(d, InterleaveLower(d64, v32L, v32U));
  v3 = BitCast(d, InterleaveUpper(d64, v32L, v32U));
}

template <typename T, size_t N, class V, HWY_IF_LANES_PER_BLOCK(T, N, 8)>
HWY_API void LoadInterleaved4(Simd<T, N, 0> d, const T* HWY_RESTRICT unaligned,
                              V& v0, V& v1, V& v2, V& v3) {
  // In the last step, we interleave by half of the block size, which is usually
  // 8 bytes but half that for 8-bit x8 vectors.
  using TW = hwy::UnsignedFromSize<sizeof(T) * N == 8 ? 4 : 8>;
  const Repartition<TW, decltype(d)> dw;
  using VW = VFromD<decltype(dw)>;

  // (Comments are for 256-bit vectors.)
  // 8 lanes per block; the lowest four blocks are at the bottom of A,B,C,D.
  V A;  // v3210[9]v3210[8] v3210[1]v3210[0]
  V B;  // v3210[b]v3210[a] v3210[3]v3210[2]
  V C;  // v3210[d]v3210[c] v3210[5]v3210[4]
  V D;  // v3210[f]v3210[e] v3210[7]v3210[6]
  detail::LoadTransposedBlocks4(d, unaligned, A, B, C, D);

  const V va820 = InterleaveLower(d, A, B);  // v3210[a,8] v3210[2,0]
  const V vec64 = InterleaveLower(d, C, D);  // v3210[e,c] v3210[6,4]
  const V vb931 = InterleaveUpper(d, A, B);  // v3210[b,9] v3210[3,1]
  const V vfd75 = InterleaveUpper(d, C, D);  // v3210[f,d] v3210[7,5]

  const VW v10_b830 =  // v10[b..8] v10[3..0]
      BitCast(dw, InterleaveLower(d, va820, vb931));
  const VW v10_fc74 =  // v10[f..c] v10[7..4]
      BitCast(dw, InterleaveLower(d, vec64, vfd75));
  const VW v32_b830 =  // v32[b..8] v32[3..0]
      BitCast(dw, InterleaveUpper(d, va820, vb931));
  const VW v32_fc74 =  // v32[f..c] v32[7..4]
      BitCast(dw, InterleaveUpper(d, vec64, vfd75));

  v0 = BitCast(d, InterleaveLower(dw, v10_b830, v10_fc74));
  v1 = BitCast(d, InterleaveUpper(dw, v10_b830, v10_fc74));
  v2 = BitCast(d, InterleaveLower(dw, v32_b830, v32_fc74));
  v3 = BitCast(d, InterleaveUpper(dw, v32_b830, v32_fc74));
}

template <typename T, size_t N, class V, HWY_IF_LANES_PER_BLOCK(T, N, 4)>
HWY_API void LoadInterleaved4(Simd<T, N, 0> d, const T* HWY_RESTRICT unaligned,
                              V& v0, V& v1, V& v2, V& v3) {
  V A;  // v3210[4] v3210[0]
  V B;  // v3210[5] v3210[1]
  V C;  // v3210[6] v3210[2]
  V D;  // v3210[7] v3210[3]
  detail::LoadTransposedBlocks4(d, unaligned, A, B, C, D);
  const V v10_ev = InterleaveLower(d, A, C);  // v1[6,4] v0[6,4] v1[2,0] v0[2,0]
  const V v10_od = InterleaveLower(d, B, D);  // v1[7,5] v0[7,5] v1[3,1] v0[3,1]
  const V v32_ev = InterleaveUpper(d, A, C);  // v3[6,4] v2[6,4] v3[2,0] v2[2,0]
  const V v32_od = InterleaveUpper(d, B, D);  // v3[7,5] v2[7,5] v3[3,1] v2[3,1]

  v0 = InterleaveLower(d, v10_ev, v10_od);
  v1 = InterleaveUpper(d, v10_ev, v10_od);
  v2 = InterleaveLower(d, v32_ev, v32_od);
  v3 = InterleaveUpper(d, v32_ev, v32_od);
}

template <typename T, size_t N, class V, HWY_IF_LANES_PER_BLOCK(T, N, 2)>
HWY_API void LoadInterleaved4(Simd<T, N, 0> d, const T* HWY_RESTRICT unaligned,
                              V& v0, V& v1, V& v2, V& v3) {
  V A, B, C, D;
  detail::LoadTransposedBlocks4(d, unaligned, A, B, C, D);
  v0 = InterleaveLower(d, A, C);
  v1 = InterleaveUpper(d, A, C);
  v2 = InterleaveLower(d, B, D);
  v3 = InterleaveUpper(d, B, D);
}

// Any T x1
template <typename T, class V>
HWY_API void LoadInterleaved4(Simd<T, 1, 0> d, const T* HWY_RESTRICT unaligned,
                              V& v0, V& v1, V& v2, V& v3) {
  v0 = LoadU(d, unaligned + 0);
  v1 = LoadU(d, unaligned + 1);
  v2 = LoadU(d, unaligned + 2);
  v3 = LoadU(d, unaligned + 3);
}

// ------------------------------ StoreInterleaved2

namespace detail {

// Default for <= 128-bit vectors; x86_256 and x86_512 have their own overload.
template <typename T, size_t N, class V, HWY_IF_LE128(T, N)>
HWY_API void StoreTransposedBlocks2(const V A, const V B, Simd<T, N, 0> d,
                                    T* HWY_RESTRICT unaligned) {
  StoreU(A, d, unaligned + 0 * N);
  StoreU(B, d, unaligned + 1 * N);
}

}  // namespace detail

// >= 128 bit vector
template <typename T, size_t N, class V, HWY_IF_GE128(T, N)>
HWY_API void StoreInterleaved2(const V v0, const V v1, Simd<T, N, 0> d,
                               T* HWY_RESTRICT unaligned) {
  const auto v10L = InterleaveLower(d, v0, v1);  // .. v1[0] v0[0]
  const auto v10U = InterleaveUpper(d, v0, v1);  // .. v1[N/2] v0[N/2]
  detail::StoreTransposedBlocks2(v10L, v10U, d, unaligned);
}

// 64 bits
template <typename T>
HWY_API void StoreInterleaved2(const Vec64<T> part0, const Vec64<T> part1,
                               Full64<T> /*tag*/, T* HWY_RESTRICT unaligned) {
  // Use full vectors to reduce the number of stores.
  const Full128<T> d_full;
  const Vec128<T> v0{part0.raw};
  const Vec128<T> v1{part1.raw};
  const auto v10 = InterleaveLower(d_full, v0, v1);
  StoreU(v10, d_full, unaligned);
}

// <= 32 bits
template <typename T, size_t N, HWY_IF_LE32(T, N)>
HWY_API void StoreInterleaved2(const Vec128<T, N> part0,
                               const Vec128<T, N> part1, Simd<T, N, 0> /*tag*/,
                               T* HWY_RESTRICT unaligned) {
  // Use full vectors to reduce the number of stores.
  const Full128<T> d_full;
  const Vec128<T> v0{part0.raw};
  const Vec128<T> v1{part1.raw};
  const auto v10 = InterleaveLower(d_full, v0, v1);
  alignas(16) T buf[16 / sizeof(T)];
  StoreU(v10, d_full, buf);
  CopyBytes<2 * N * sizeof(T)>(buf, unaligned);
}

// ------------------------------ StoreInterleaved3 (CombineShiftRightBytes,
// TableLookupBytes)

namespace detail {

// Default for <= 128-bit vectors; x86_256 and x86_512 have their own overload.
template <typename T, size_t N, class V, HWY_IF_LE128(T, N)>
HWY_API void StoreTransposedBlocks3(const V A, const V B, const V C,
                                    Simd<T, N, 0> d,
                                    T* HWY_RESTRICT unaligned) {
  StoreU(A, d, unaligned + 0 * N);
  StoreU(B, d, unaligned + 1 * N);
  StoreU(C, d, unaligned + 2 * N);
}

}  // namespace detail

// >= 128-bit vector, 8-bit lanes
template <typename T, size_t N, class V, HWY_IF_LANE_SIZE(T, 1),
          HWY_IF_GE128(T, N)>
HWY_API void StoreInterleaved3(const V v0, const V v1, const V v2,
                               Simd<T, N, 0> d, T* HWY_RESTRICT unaligned) {
  const RebindToUnsigned<decltype(d)> du;
  const auto k5 = Set(du, 5);
  const auto k6 = Set(du, 6);

  // Interleave (v0,v1,v2) to (MSB on left, lane 0 on right):
  // v0[5], v2[4],v1[4],v0[4] .. v2[0],v1[0],v0[0]. We're expanding v0 lanes
  // to their place, with 0x80 so lanes to be filled from other vectors are 0
  // to enable blending by ORing together.
  alignas(16) static constexpr uint8_t tbl_v0[16] = {
      0, 0x80, 0x80, 1, 0x80, 0x80, 2, 0x80, 0x80,  //
      3, 0x80, 0x80, 4, 0x80, 0x80, 5};
  alignas(16) static constexpr uint8_t tbl_v1[16] = {
      0x80, 0, 0x80, 0x80, 1, 0x80,  //
      0x80, 2, 0x80, 0x80, 3, 0x80, 0x80, 4, 0x80, 0x80};
  // The interleaved vectors will be named A, B, C; temporaries with suffix
  // 0..2 indicate which input vector's lanes they hold.
  const auto shuf_A0 = LoadDup128(du, tbl_v0);
  const auto shuf_A1 = LoadDup128(du, tbl_v1);  // cannot reuse shuf_A0 (has 5)
  const auto shuf_A2 = CombineShiftRightBytes<15>(du, shuf_A1, shuf_A1);
  const auto A0 = TableLookupBytesOr0(v0, shuf_A0);  // 5..4..3..2..1..0
  const auto A1 = TableLookupBytesOr0(v1, shuf_A1);  // ..4..3..2..1..0.
  const auto A2 = TableLookupBytesOr0(v2, shuf_A2);  // .4..3..2..1..0..
  const V A = BitCast(d, A0 | A1 | A2);

  // B: v1[10],v0[10], v2[9],v1[9],v0[9] .. , v2[6],v1[6],v0[6], v2[5],v1[5]
  const auto shuf_B0 = shuf_A2 + k6;  // .A..9..8..7..6..
  const auto shuf_B1 = shuf_A0 + k5;  // A..9..8..7..6..5
  const auto shuf_B2 = shuf_A1 + k5;  // ..9..8..7..6..5.
  const auto B0 = TableLookupBytesOr0(v0, shuf_B0);
  const auto B1 = TableLookupBytesOr0(v1, shuf_B1);
  const auto B2 = TableLookupBytesOr0(v2, shuf_B2);
  const V B = BitCast(d, B0 | B1 | B2);

  // C: v2[15],v1[15],v0[15], v2[11],v1[11],v0[11], v2[10]
  const auto shuf_C0 = shuf_B2 + k6;  // ..F..E..D..C..B.
  const auto shuf_C1 = shuf_B0 + k5;  // .F..E..D..C..B..
  const auto shuf_C2 = shuf_B1 + k5;  // F..E..D..C..B..A
  const auto C0 = TableLookupBytesOr0(v0, shuf_C0);
  const auto C1 = TableLookupBytesOr0(v1, shuf_C1);
  const auto C2 = TableLookupBytesOr0(v2, shuf_C2);
  const V C = BitCast(d, C0 | C1 | C2);

  detail::StoreTransposedBlocks3(A, B, C, d, unaligned);
}

// >= 128-bit vector, 16-bit lanes
template <typename T, size_t N, class V, HWY_IF_LANE_SIZE(T, 2),
          HWY_IF_GE128(T, N)>
HWY_API void StoreInterleaved3(const V v0, const V v1, const V v2,
                               Simd<T, N, 0> d, T* HWY_RESTRICT unaligned) {
  const Repartition<uint8_t, decltype(d)> du8;
  const auto k2 = Set(du8, 2 * sizeof(T));
  const auto k3 = Set(du8, 3 * sizeof(T));

  // Interleave (v0,v1,v2) to (MSB on left, lane 0 on right):
  // v1[2],v0[2], v2[1],v1[1],v0[1], v2[0],v1[0],v0[0]. 0x80 so lanes to be
  // filled from other vectors are 0 for blending. Note that these are byte
  // indices for 16-bit lanes.
  alignas(16) static constexpr uint8_t tbl_v1[16] = {
      0x80, 0x80, 0,    1,    0x80, 0x80, 0x80, 0x80,
      2,    3,    0x80, 0x80, 0x80, 0x80, 4,    5};
  alignas(16) static constexpr uint8_t tbl_v2[16] = {
      0x80, 0x80, 0x80, 0x80, 0,    1,    0x80, 0x80,
      0x80, 0x80, 2,    3,    0x80, 0x80, 0x80, 0x80};

  // The interleaved vectors will be named A, B, C; temporaries with suffix
  // 0..2 indicate which input vector's lanes they hold.
  const auto shuf_A1 = LoadDup128(du8, tbl_v1);  // 2..1..0.
                                                 // .2..1..0
  const auto shuf_A0 = CombineShiftRightBytes<2>(du8, shuf_A1, shuf_A1);
  const auto shuf_A2 = LoadDup128(du8, tbl_v2);  // ..1..0..

  const auto A0 = TableLookupBytesOr0(v0, shuf_A0);
  const auto A1 = TableLookupBytesOr0(v1, shuf_A1);
  const auto A2 = TableLookupBytesOr0(v2, shuf_A2);
  const V A = BitCast(d, A0 | A1 | A2);

  // B: v0[5] v2[4],v1[4],v0[4], v2[3],v1[3],v0[3], v2[2]
  const auto shuf_B0 = shuf_A1 + k3;  // 5..4..3.
  const auto shuf_B1 = shuf_A2 + k3;  // ..4..3..
  const auto shuf_B2 = shuf_A0 + k2;  // .4..3..2
  const auto B0 = TableLookupBytesOr0(v0, shuf_B0);
  const auto B1 = TableLookupBytesOr0(v1, shuf_B1);
  const auto B2 = TableLookupBytesOr0(v2, shuf_B2);
  const V B = BitCast(d, B0 | B1 | B2);

  // C: v2[7],v1[7],v0[7], v2[6],v1[6],v0[6], v2[5],v1[5]
  const auto shuf_C0 = shuf_B1 + k3;  // ..7..6..
  const auto shuf_C1 = shuf_B2 + k3;  // .7..6..5
  const auto shuf_C2 = shuf_B0 + k2;  // 7..6..5.
  const auto C0 = TableLookupBytesOr0(v0, shuf_C0);
  const auto C1 = TableLookupBytesOr0(v1, shuf_C1);
  const auto C2 = TableLookupBytesOr0(v2, shuf_C2);
  const V C = BitCast(d, C0 | C1 | C2);

  detail::StoreTransposedBlocks3(A, B, C, d, unaligned);
}

// >= 128-bit vector, 32-bit lanes
template <typename T, size_t N, class V, HWY_IF_LANE_SIZE(T, 4),
          HWY_IF_GE128(T, N)>
HWY_API void StoreInterleaved3(const V v0, const V v1, const V v2,
                               Simd<T, N, 0> d, T* HWY_RESTRICT unaligned) {
  const RepartitionToWide<decltype(d)> dw;

  const V v10_v00 = InterleaveLower(d, v0, v1);
  const V v01_v20 = OddEven(v0, v2);
  // A: v0[1], v2[0],v1[0],v0[0] (<- lane 0)
  const V A = BitCast(
      d, InterleaveLower(dw, BitCast(dw, v10_v00), BitCast(dw, v01_v20)));

  const V v1_321 = ShiftRightLanes<1>(d, v1);
  const V v0_32 = ShiftRightLanes<2>(d, v0);
  const V v21_v11 = OddEven(v2, v1_321);
  const V v12_v02 = OddEven(v1_321, v0_32);
  // B: v1[2],v0[2], v2[1],v1[1]
  const V B = BitCast(
      d, InterleaveLower(dw, BitCast(dw, v21_v11), BitCast(dw, v12_v02)));

  // Notation refers to the upper 2 lanes of the vector for InterleaveUpper.
  const V v23_v13 = OddEven(v2, v1_321);
  const V v03_v22 = OddEven(v0, v2);
  // C: v2[3],v1[3],v0[3], v2[2]
  const V C = BitCast(
      d, InterleaveUpper(dw, BitCast(dw, v03_v22), BitCast(dw, v23_v13)));

  detail::StoreTransposedBlocks3(A, B, C, d, unaligned);
}

// >= 128-bit vector, 64-bit lanes
template <typename T, size_t N, class V, HWY_IF_LANE_SIZE(T, 8),
          HWY_IF_GE128(T, N)>
HWY_API void StoreInterleaved3(const V v0, const V v1, const V v2,
                               Simd<T, N, 0> d, T* HWY_RESTRICT unaligned) {
  const V A = InterleaveLower(d, v0, v1);
  const V B = OddEven(v0, v2);
  const V C = InterleaveUpper(d, v1, v2);
  detail::StoreTransposedBlocks3(A, B, C, d, unaligned);
}

// 64-bit vector, 8-bit lanes
template <typename T, HWY_IF_LANE_SIZE(T, 1)>
HWY_API void StoreInterleaved3(const Vec64<T> part0, const Vec64<T> part1,
                               const Vec64<T> part2, Full64<T> d,
                               T* HWY_RESTRICT unaligned) {
  constexpr size_t N = 16 / sizeof(T);
  // Use full vectors for the shuffles and first result.
  const Full128<uint8_t> du;
  const Full128<T> d_full;
  const auto k5 = Set(du, 5);
  const auto k6 = Set(du, 6);

  const Vec128<T> v0{part0.raw};
  const Vec128<T> v1{part1.raw};
  const Vec128<T> v2{part2.raw};

  // Interleave (v0,v1,v2) to (MSB on left, lane 0 on right):
  // v1[2],v0[2], v2[1],v1[1],v0[1], v2[0],v1[0],v0[0]. 0x80 so lanes to be
  // filled from other vectors are 0 for blending.
  alignas(16) static constexpr uint8_t tbl_v0[16] = {
      0, 0x80, 0x80, 1, 0x80, 0x80, 2, 0x80, 0x80,  //
      3, 0x80, 0x80, 4, 0x80, 0x80, 5};
  alignas(16) static constexpr uint8_t tbl_v1[16] = {
      0x80, 0, 0x80, 0x80, 1, 0x80,  //
      0x80, 2, 0x80, 0x80, 3, 0x80, 0x80, 4, 0x80, 0x80};
  // The interleaved vectors will be named A, B, C; temporaries with suffix
  // 0..2 indicate which input vector's lanes they hold.
  const auto shuf_A0 = Load(du, tbl_v0);
  const auto shuf_A1 = Load(du, tbl_v1);  // cannot reuse shuf_A0 (5 in MSB)
  const auto shuf_A2 = CombineShiftRightBytes<15>(du, shuf_A1, shuf_A1);
  const auto A0 = TableLookupBytesOr0(v0, shuf_A0);  // 5..4..3..2..1..0
  const auto A1 = TableLookupBytesOr0(v1, shuf_A1);  // ..4..3..2..1..0.
  const auto A2 = TableLookupBytesOr0(v2, shuf_A2);  // .4..3..2..1..0..
  const auto A = BitCast(d_full, A0 | A1 | A2);
  StoreU(A, d_full, unaligned + 0 * N);

  // Second (HALF) vector: v2[7],v1[7],v0[7], v2[6],v1[6],v0[6], v2[5],v1[5]
  const auto shuf_B0 = shuf_A2 + k6;  // ..7..6..
  const auto shuf_B1 = shuf_A0 + k5;  // .7..6..5
  const auto shuf_B2 = shuf_A1 + k5;  // 7..6..5.
  const auto B0 = TableLookupBytesOr0(v0, shuf_B0);
  const auto B1 = TableLookupBytesOr0(v1, shuf_B1);
  const auto B2 = TableLookupBytesOr0(v2, shuf_B2);
  const Vec64<T> B{(B0 | B1 | B2).raw};
  StoreU(B, d, unaligned + 1 * N);
}

// 64-bit vector, 16-bit lanes
template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_API void StoreInterleaved3(const Vec64<T> part0, const Vec64<T> part1,
                               const Vec64<T> part2, Full64<T> dh,
                               T* HWY_RESTRICT unaligned) {
  const Full128<T> d;
  const Full128<uint8_t> du8;
  constexpr size_t N = 16 / sizeof(T);
  const auto k2 = Set(du8, 2 * sizeof(T));
  const auto k3 = Set(du8, 3 * sizeof(T));

  const Vec128<T> v0{part0.raw};
  const Vec128<T> v1{part1.raw};
  const Vec128<T> v2{part2.raw};

  // Interleave part (v0,v1,v2) to full (MSB on left, lane 0 on right):
  // v1[2],v0[2], v2[1],v1[1],v0[1], v2[0],v1[0],v0[0]. We're expanding v0 lanes
  // to their place, with 0x80 so lanes to be filled from other vectors are 0
  // to enable blending by ORing together.
  alignas(16) static constexpr uint8_t tbl_v1[16] = {
      0x80, 0x80, 0,    1,    0x80, 0x80, 0x80, 0x80,
      2,    3,    0x80, 0x80, 0x80, 0x80, 4,    5};
  alignas(16) static constexpr uint8_t tbl_v2[16] = {
      0x80, 0x80, 0x80, 0x80, 0,    1,    0x80, 0x80,
      0x80, 0x80, 2,    3,    0x80, 0x80, 0x80, 0x80};

  // The interleaved vectors will be named A, B; temporaries with suffix
  // 0..2 indicate which input vector's lanes they hold.
  const auto shuf_A1 = Load(du8, tbl_v1);  // 2..1..0.
                                           // .2..1..0
  const auto shuf_A0 = CombineShiftRightBytes<2>(du8, shuf_A1, shuf_A1);
  const auto shuf_A2 = Load(du8, tbl_v2);  // ..1..0..

  const auto A0 = TableLookupBytesOr0(v0, shuf_A0);
  const auto A1 = TableLookupBytesOr0(v1, shuf_A1);
  const auto A2 = TableLookupBytesOr0(v2, shuf_A2);
  const Vec128<T> A = BitCast(d, A0 | A1 | A2);
  StoreU(A, d, unaligned + 0 * N);

  // Second (HALF) vector: v2[3],v1[3],v0[3], v2[2]
  const auto shuf_B0 = shuf_A1 + k3;  // ..3.
  const auto shuf_B1 = shuf_A2 + k3;  // .3..
  const auto shuf_B2 = shuf_A0 + k2;  // 3..2
  const auto B0 = TableLookupBytesOr0(v0, shuf_B0);
  const auto B1 = TableLookupBytesOr0(v1, shuf_B1);
  const auto B2 = TableLookupBytesOr0(v2, shuf_B2);
  const Vec128<T> B = BitCast(d, B0 | B1 | B2);
  StoreU(Vec64<T>{B.raw}, dh, unaligned + 1 * N);
}

// 64-bit vector, 32-bit lanes
template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API void StoreInterleaved3(const Vec64<T> v0, const Vec64<T> v1,
                               const Vec64<T> v2, Full64<T> d,
                               T* HWY_RESTRICT unaligned) {
  // (same code as 128-bit vector, 64-bit lanes)
  constexpr size_t N = 2;
  const Vec64<T> v10_v00 = InterleaveLower(d, v0, v1);
  const Vec64<T> v01_v20 = OddEven(v0, v2);
  const Vec64<T> v21_v11 = InterleaveUpper(d, v1, v2);
  StoreU(v10_v00, d, unaligned + 0 * N);
  StoreU(v01_v20, d, unaligned + 1 * N);
  StoreU(v21_v11, d, unaligned + 2 * N);
}

// 64-bit lanes are handled by the N=1 case below.

// <= 32-bit vector, 8-bit lanes
template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 1), HWY_IF_LE32(T, N)>
HWY_API void StoreInterleaved3(const Vec128<T, N> part0,
                               const Vec128<T, N> part1,
                               const Vec128<T, N> part2, Simd<T, N, 0> /*tag*/,
                               T* HWY_RESTRICT unaligned) {
  // Use full vectors for the shuffles and result.
  const Full128<uint8_t> du;
  const Full128<T> d_full;

  const Vec128<T> v0{part0.raw};
  const Vec128<T> v1{part1.raw};
  const Vec128<T> v2{part2.raw};

  // Interleave (v0,v1,v2). We're expanding v0 lanes to their place, with 0x80
  // so lanes to be filled from other vectors are 0 to enable blending by ORing
  // together.
  alignas(16) static constexpr uint8_t tbl_v0[16] = {
      0,    0x80, 0x80, 1,    0x80, 0x80, 2,    0x80,
      0x80, 3,    0x80, 0x80, 0x80, 0x80, 0x80, 0x80};
  // The interleaved vector will be named A; temporaries with suffix
  // 0..2 indicate which input vector's lanes they hold.
  const auto shuf_A0 = Load(du, tbl_v0);
  const auto shuf_A1 = CombineShiftRightBytes<15>(du, shuf_A0, shuf_A0);
  const auto shuf_A2 = CombineShiftRightBytes<14>(du, shuf_A0, shuf_A0);
  const auto A0 = TableLookupBytesOr0(v0, shuf_A0);  // ......3..2..1..0
  const auto A1 = TableLookupBytesOr0(v1, shuf_A1);  // .....3..2..1..0.
  const auto A2 = TableLookupBytesOr0(v2, shuf_A2);  // ....3..2..1..0..
  const Vec128<T> A = BitCast(d_full, A0 | A1 | A2);
  alignas(16) T buf[16 / sizeof(T)];
  StoreU(A, d_full, buf);
  CopyBytes<N * 3 * sizeof(T)>(buf, unaligned);
}

// 32-bit vector, 16-bit lanes
template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_API void StoreInterleaved3(const Vec128<T, 2> part0,
                               const Vec128<T, 2> part1,
                               const Vec128<T, 2> part2, Simd<T, 2, 0> /*tag*/,
                               T* HWY_RESTRICT unaligned) {
  constexpr size_t N = 4 / sizeof(T);
  // Use full vectors for the shuffles and result.
  const Full128<uint8_t> du8;
  const Full128<T> d_full;

  const Vec128<T> v0{part0.raw};
  const Vec128<T> v1{part1.raw};
  const Vec128<T> v2{part2.raw};

  // Interleave (v0,v1,v2). We're expanding v0 lanes to their place, with 0x80
  // so lanes to be filled from other vectors are 0 to enable blending by ORing
  // together.
  alignas(16) static constexpr uint8_t tbl_v2[16] = {
      0x80, 0x80, 0x80, 0x80, 0,    1,    0x80, 0x80,
      0x80, 0x80, 2,    3,    0x80, 0x80, 0x80, 0x80};
  // The interleaved vector will be named A; temporaries with suffix
  // 0..2 indicate which input vector's lanes they hold.
  const auto shuf_A2 =  // ..1..0..
      Load(du8, tbl_v2);
  const auto shuf_A1 =  // ...1..0.
      CombineShiftRightBytes<2>(du8, shuf_A2, shuf_A2);
  const auto shuf_A0 =  // ....1..0
      CombineShiftRightBytes<4>(du8, shuf_A2, shuf_A2);
  const auto A0 = TableLookupBytesOr0(v0, shuf_A0);  // ..1..0
  const auto A1 = TableLookupBytesOr0(v1, shuf_A1);  // .1..0.
  const auto A2 = TableLookupBytesOr0(v2, shuf_A2);  // 1..0..
  const auto A = BitCast(d_full, A0 | A1 | A2);
  alignas(16) T buf[16 / sizeof(T)];
  StoreU(A, d_full, buf);
  CopyBytes<N * 3 * sizeof(T)>(buf, unaligned);
}

// Single-element vector, any lane size: just store directly
template <typename T>
HWY_API void StoreInterleaved3(const Vec128<T, 1> v0, const Vec128<T, 1> v1,
                               const Vec128<T, 1> v2, Simd<T, 1, 0> d,
                               T* HWY_RESTRICT unaligned) {
  StoreU(v0, d, unaligned + 0);
  StoreU(v1, d, unaligned + 1);
  StoreU(v2, d, unaligned + 2);
}

// ------------------------------ StoreInterleaved4

namespace detail {

// Default for <= 128-bit vectors; x86_256 and x86_512 have their own overload.
template <typename T, size_t N, class V, HWY_IF_LE128(T, N)>
HWY_API void StoreTransposedBlocks4(const V A, const V B, const V C, const V D,
                                    Simd<T, N, 0> d,
                                    T* HWY_RESTRICT unaligned) {
  StoreU(A, d, unaligned + 0 * N);
  StoreU(B, d, unaligned + 1 * N);
  StoreU(C, d, unaligned + 2 * N);
  StoreU(D, d, unaligned + 3 * N);
}

}  // namespace detail

// >= 128-bit vector, 8..32-bit lanes
template <typename T, size_t N, class V, HWY_IF_NOT_LANE_SIZE(T, 8),
          HWY_IF_GE128(T, N)>
HWY_API void StoreInterleaved4(const V v0, const V v1, const V v2, const V v3,
                               Simd<T, N, 0> d, T* HWY_RESTRICT unaligned) {
  const RepartitionToWide<decltype(d)> dw;
  const auto v10L = ZipLower(dw, v0, v1);  // .. v1[0] v0[0]
  const auto v32L = ZipLower(dw, v2, v3);
  const auto v10U = ZipUpper(dw, v0, v1);
  const auto v32U = ZipUpper(dw, v2, v3);
  // The interleaved vectors are A, B, C, D.
  const auto A = BitCast(d, InterleaveLower(dw, v10L, v32L));  // 3210
  const auto B = BitCast(d, InterleaveUpper(dw, v10L, v32L));
  const auto C = BitCast(d, InterleaveLower(dw, v10U, v32U));
  const auto D = BitCast(d, InterleaveUpper(dw, v10U, v32U));
  detail::StoreTransposedBlocks4(A, B, C, D, d, unaligned);
}

// >= 128-bit vector, 64-bit lanes
template <typename T, size_t N, class V, HWY_IF_LANE_SIZE(T, 8),
          HWY_IF_GE128(T, N)>
HWY_API void StoreInterleaved4(const V v0, const V v1, const V v2, const V v3,
                               Simd<T, N, 0> d, T* HWY_RESTRICT unaligned) {
  // The interleaved vectors are A, B, C, D.
  const auto A = InterleaveLower(d, v0, v1);  // v1[0] v0[0]
  const auto B = InterleaveLower(d, v2, v3);
  const auto C = InterleaveUpper(d, v0, v1);
  const auto D = InterleaveUpper(d, v2, v3);
  detail::StoreTransposedBlocks4(A, B, C, D, d, unaligned);
}

// 64-bit vector, 8..32-bit lanes
template <typename T, HWY_IF_NOT_LANE_SIZE(T, 8)>
HWY_API void StoreInterleaved4(const Vec64<T> part0, const Vec64<T> part1,
                               const Vec64<T> part2, const Vec64<T> part3,
                               Full64<T> /*tag*/, T* HWY_RESTRICT unaligned) {
  constexpr size_t N = 16 / sizeof(T);
  // Use full vectors to reduce the number of stores.
  const Full128<T> d_full;
  const RepartitionToWide<decltype(d_full)> dw;
  const Vec128<T> v0{part0.raw};
  const Vec128<T> v1{part1.raw};
  const Vec128<T> v2{part2.raw};
  const Vec128<T> v3{part3.raw};
  const auto v10 = ZipLower(dw, v0, v1);  // v1[0] v0[0]
  const auto v32 = ZipLower(dw, v2, v3);
  const auto A = BitCast(d_full, InterleaveLower(dw, v10, v32));
  const auto B = BitCast(d_full, InterleaveUpper(dw, v10, v32));
  StoreU(A, d_full, unaligned + 0 * N);
  StoreU(B, d_full, unaligned + 1 * N);
}

// 64-bit vector, 64-bit lane
template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_API void StoreInterleaved4(const Vec64<T> part0, const Vec64<T> part1,
                               const Vec64<T> part2, const Vec64<T> part3,
                               Full64<T> /*tag*/, T* HWY_RESTRICT unaligned) {
  constexpr size_t N = 16 / sizeof(T);
  // Use full vectors to reduce the number of stores.
  const Full128<T> d_full;
  const Vec128<T> v0{part0.raw};
  const Vec128<T> v1{part1.raw};
  const Vec128<T> v2{part2.raw};
  const Vec128<T> v3{part3.raw};
  const auto A = InterleaveLower(d_full, v0, v1);  // v1[0] v0[0]
  const auto B = InterleaveLower(d_full, v2, v3);
  StoreU(A, d_full, unaligned + 0 * N);
  StoreU(B, d_full, unaligned + 1 * N);
}

// <= 32-bit vectors
template <typename T, size_t N, HWY_IF_LE32(T, N)>
HWY_API void StoreInterleaved4(const Vec128<T, N> part0,
                               const Vec128<T, N> part1,
                               const Vec128<T, N> part2,
                               const Vec128<T, N> part3, Simd<T, N, 0> /*tag*/,
                               T* HWY_RESTRICT unaligned) {
  // Use full vectors to reduce the number of stores.
  const Full128<T> d_full;
  const RepartitionToWide<decltype(d_full)> dw;
  const Vec128<T> v0{part0.raw};
  const Vec128<T> v1{part1.raw};
  const Vec128<T> v2{part2.raw};
  const Vec128<T> v3{part3.raw};
  const auto v10 = ZipLower(dw, v0, v1);  // .. v1[0] v0[0]
  const auto v32 = ZipLower(dw, v2, v3);
  const auto v3210 = BitCast(d_full, InterleaveLower(dw, v10, v32));
  alignas(16) T buf[16 / sizeof(T)];
  StoreU(v3210, d_full, buf);
  CopyBytes<4 * N * sizeof(T)>(buf, unaligned);
}

#endif  // HWY_NATIVE_LOAD_STORE_INTERLEAVED

// ------------------------------ AESRound

// Cannot implement on scalar: need at least 16 bytes for TableLookupBytes.
#if HWY_TARGET != HWY_SCALAR

// Define for white-box testing, even if native instructions are available.
namespace detail {

// Constant-time: computes inverse in GF(2^4) based on "Accelerating AES with
// Vector Permute Instructions" and the accompanying assembly language
// implementation: https://crypto.stanford.edu/vpaes/vpaes.tgz. See also Botan:
// https://botan.randombit.net/doxygen/aes__vperm_8cpp_source.html .
//
// A brute-force 256 byte table lookup can also be made constant-time, and
// possibly competitive on NEON, but this is more performance-portable
// especially for x86 and large vectors.
template <class V>  // u8
HWY_INLINE V SubBytes(V state) {
  const DFromV<V> du;
  const auto mask = Set(du, 0xF);

  // Change polynomial basis to GF(2^4)
  {
    alignas(16) static constexpr uint8_t basisL[16] = {
        0x00, 0x70, 0x2A, 0x5A, 0x98, 0xE8, 0xB2, 0xC2,
        0x08, 0x78, 0x22, 0x52, 0x90, 0xE0, 0xBA, 0xCA};
    alignas(16) static constexpr uint8_t basisU[16] = {
        0x00, 0x4D, 0x7C, 0x31, 0x7D, 0x30, 0x01, 0x4C,
        0x81, 0xCC, 0xFD, 0xB0, 0xFC, 0xB1, 0x80, 0xCD};
    const auto sL = And(state, mask);
    const auto sU = ShiftRight<4>(state);  // byte shift => upper bits are zero
    const auto gf4L = TableLookupBytes(LoadDup128(du, basisL), sL);
    const auto gf4U = TableLookupBytes(LoadDup128(du, basisU), sU);
    state = Xor(gf4L, gf4U);
  }

  // Inversion in GF(2^4). Elements 0 represent "infinity" (division by 0) and
  // cause TableLookupBytesOr0 to return 0.
  alignas(16) static constexpr uint8_t kZetaInv[16] = {
      0x80, 7, 11, 15, 6, 10, 4, 1, 9, 8, 5, 2, 12, 14, 13, 3};
  alignas(16) static constexpr uint8_t kInv[16] = {
      0x80, 1, 8, 13, 15, 6, 5, 14, 2, 12, 11, 10, 9, 3, 7, 4};
  const auto tbl = LoadDup128(du, kInv);
  const auto sL = And(state, mask);      // L=low nibble, U=upper
  const auto sU = ShiftRight<4>(state);  // byte shift => upper bits are zero
  const auto sX = Xor(sU, sL);
  const auto invL = TableLookupBytes(LoadDup128(du, kZetaInv), sL);
  const auto invU = TableLookupBytes(tbl, sU);
  const auto invX = TableLookupBytes(tbl, sX);
  const auto outL = Xor(sX, TableLookupBytesOr0(tbl, Xor(invL, invU)));
  const auto outU = Xor(sU, TableLookupBytesOr0(tbl, Xor(invL, invX)));

  // Linear skew (cannot bake 0x63 bias into the table because out* indices
  // may have the infinity flag set).
  alignas(16) static constexpr uint8_t kAffineL[16] = {
      0x00, 0xC7, 0xBD, 0x6F, 0x17, 0x6D, 0xD2, 0xD0,
      0x78, 0xA8, 0x02, 0xC5, 0x7A, 0xBF, 0xAA, 0x15};
  alignas(16) static constexpr uint8_t kAffineU[16] = {
      0x00, 0x6A, 0xBB, 0x5F, 0xA5, 0x74, 0xE4, 0xCF,
      0xFA, 0x35, 0x2B, 0x41, 0xD1, 0x90, 0x1E, 0x8E};
  const auto affL = TableLookupBytesOr0(LoadDup128(du, kAffineL), outL);
  const auto affU = TableLookupBytesOr0(LoadDup128(du, kAffineU), outU);
  return Xor(Xor(affL, affU), Set(du, 0x63));
}

}  // namespace detail

#endif  // HWY_TARGET != HWY_SCALAR

// "Include guard": skip if native AES instructions are available.
#if (defined(HWY_NATIVE_AES) == defined(HWY_TARGET_TOGGLE))
#ifdef HWY_NATIVE_AES
#undef HWY_NATIVE_AES
#else
#define HWY_NATIVE_AES
#endif

// (Must come after HWY_TARGET_TOGGLE, else we don't reset it for scalar)
#if HWY_TARGET != HWY_SCALAR

namespace detail {

template <class V>  // u8
HWY_API V ShiftRows(const V state) {
  const DFromV<V> du;
  alignas(16) static constexpr uint8_t kShiftRow[16] = {
      0,  5,  10, 15,  // transposed: state is column major
      4,  9,  14, 3,   //
      8,  13, 2,  7,   //
      12, 1,  6,  11};
  const auto shift_row = LoadDup128(du, kShiftRow);
  return TableLookupBytes(state, shift_row);
}

template <class V>  // u8
HWY_API V MixColumns(const V state) {
  const DFromV<V> du;
  // For each column, the rows are the sum of GF(2^8) matrix multiplication by:
  // 2 3 1 1  // Let s := state*1, d := state*2, t := state*3.
  // 1 2 3 1  // d are on diagonal, no permutation needed.
  // 1 1 2 3  // t1230 indicates column indices of threes for the 4 rows.
  // 3 1 1 2  // We also need to compute s2301 and s3012 (=1230 o 2301).
  alignas(16) static constexpr uint8_t k2301[16] = {
      2, 3, 0, 1, 6, 7, 4, 5, 10, 11, 8, 9, 14, 15, 12, 13};
  alignas(16) static constexpr uint8_t k1230[16] = {
      1, 2, 3, 0, 5, 6, 7, 4, 9, 10, 11, 8, 13, 14, 15, 12};
  const RebindToSigned<decltype(du)> di;  // can only do signed comparisons
  const auto msb = Lt(BitCast(di, state), Zero(di));
  const auto overflow = BitCast(du, IfThenElseZero(msb, Set(di, 0x1B)));
  const auto d = Xor(Add(state, state), overflow);  // = state*2 in GF(2^8).
  const auto s2301 = TableLookupBytes(state, LoadDup128(du, k2301));
  const auto d_s2301 = Xor(d, s2301);
  const auto t_s2301 = Xor(state, d_s2301);  // t(s*3) = XOR-sum {s, d(s*2)}
  const auto t1230_s3012 = TableLookupBytes(t_s2301, LoadDup128(du, k1230));
  return Xor(d_s2301, t1230_s3012);  // XOR-sum of 4 terms
}

}  // namespace detail

template <class V>  // u8
HWY_API V AESRound(V state, const V round_key) {
  // Intel docs swap the first two steps, but it does not matter because
  // ShiftRows is a permutation and SubBytes is independent of lane index.
  state = detail::SubBytes(state);
  state = detail::ShiftRows(state);
  state = detail::MixColumns(state);
  state = Xor(state, round_key);  // AddRoundKey
  return state;
}

template <class V>  // u8
HWY_API V AESLastRound(V state, const V round_key) {
  // LIke AESRound, but without MixColumns.
  state = detail::SubBytes(state);
  state = detail::ShiftRows(state);
  state = Xor(state, round_key);  // AddRoundKey
  return state;
}

// Constant-time implementation inspired by
// https://www.bearssl.org/constanttime.html, but about half the cost because we
// use 64x64 multiplies and 128-bit XORs.
template <class V>
HWY_API V CLMulLower(V a, V b) {
  const DFromV<V> d;
  static_assert(IsSame<TFromD<decltype(d)>, uint64_t>(), "V must be u64");
  const auto k1 = Set(d, 0x1111111111111111ULL);
  const auto k2 = Set(d, 0x2222222222222222ULL);
  const auto k4 = Set(d, 0x4444444444444444ULL);
  const auto k8 = Set(d, 0x8888888888888888ULL);
  const auto a0 = And(a, k1);
  const auto a1 = And(a, k2);
  const auto a2 = And(a, k4);
  const auto a3 = And(a, k8);
  const auto b0 = And(b, k1);
  const auto b1 = And(b, k2);
  const auto b2 = And(b, k4);
  const auto b3 = And(b, k8);

  auto m0 = Xor(MulEven(a0, b0), MulEven(a1, b3));
  auto m1 = Xor(MulEven(a0, b1), MulEven(a1, b0));
  auto m2 = Xor(MulEven(a0, b2), MulEven(a1, b1));
  auto m3 = Xor(MulEven(a0, b3), MulEven(a1, b2));
  m0 = Xor(m0, Xor(MulEven(a2, b2), MulEven(a3, b1)));
  m1 = Xor(m1, Xor(MulEven(a2, b3), MulEven(a3, b2)));
  m2 = Xor(m2, Xor(MulEven(a2, b0), MulEven(a3, b3)));
  m3 = Xor(m3, Xor(MulEven(a2, b1), MulEven(a3, b0)));
  return Or(Or(And(m0, k1), And(m1, k2)), Or(And(m2, k4), And(m3, k8)));
}

template <class V>
HWY_API V CLMulUpper(V a, V b) {
  const DFromV<V> d;
  static_assert(IsSame<TFromD<decltype(d)>, uint64_t>(), "V must be u64");
  const auto k1 = Set(d, 0x1111111111111111ULL);
  const auto k2 = Set(d, 0x2222222222222222ULL);
  const auto k4 = Set(d, 0x4444444444444444ULL);
  const auto k8 = Set(d, 0x8888888888888888ULL);
  const auto a0 = And(a, k1);
  const auto a1 = And(a, k2);
  const auto a2 = And(a, k4);
  const auto a3 = And(a, k8);
  const auto b0 = And(b, k1);
  const auto b1 = And(b, k2);
  const auto b2 = And(b, k4);
  const auto b3 = And(b, k8);

  auto m0 = Xor(MulOdd(a0, b0), MulOdd(a1, b3));
  auto m1 = Xor(MulOdd(a0, b1), MulOdd(a1, b0));
  auto m2 = Xor(MulOdd(a0, b2), MulOdd(a1, b1));
  auto m3 = Xor(MulOdd(a0, b3), MulOdd(a1, b2));
  m0 = Xor(m0, Xor(MulOdd(a2, b2), MulOdd(a3, b1)));
  m1 = Xor(m1, Xor(MulOdd(a2, b3), MulOdd(a3, b2)));
  m2 = Xor(m2, Xor(MulOdd(a2, b0), MulOdd(a3, b3)));
  m3 = Xor(m3, Xor(MulOdd(a2, b1), MulOdd(a3, b0)));
  return Or(Or(And(m0, k1), And(m1, k2)), Or(And(m2, k4), And(m3, k8)));
}

#endif  // HWY_NATIVE_AES
#endif  // HWY_TARGET != HWY_SCALAR

// "Include guard": skip if native POPCNT-related instructions are available.
#if (defined(HWY_NATIVE_POPCNT) == defined(HWY_TARGET_TOGGLE))
#ifdef HWY_NATIVE_POPCNT
#undef HWY_NATIVE_POPCNT
#else
#define HWY_NATIVE_POPCNT
#endif

#undef HWY_MIN_POW2_FOR_128
#if HWY_TARGET == HWY_RVV
#define HWY_MIN_POW2_FOR_128 1
#else
// All other targets except HWY_SCALAR (which is excluded by HWY_IF_GE128_D)
// guarantee 128 bits anyway.
#define HWY_MIN_POW2_FOR_128 0
#endif

// This algorithm requires vectors to be at least 16 bytes, which is the case
// for LMUL >= 2. If not, use the fallback below.
template <typename V, class D = DFromV<V>, HWY_IF_LANE_SIZE_D(D, 1),
          HWY_IF_GE128_D(D), HWY_IF_POW2_GE(D, HWY_MIN_POW2_FOR_128)>
HWY_API V PopulationCount(V v) {
  static_assert(IsSame<TFromD<D>, uint8_t>(), "V must be u8");
  const D d;
  HWY_ALIGN constexpr uint8_t kLookup[16] = {
      0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
  };
  const auto lo = And(v, Set(d, 0xF));
  const auto hi = ShiftRight<4>(v);
  const auto lookup = LoadDup128(d, kLookup);
  return Add(TableLookupBytes(lookup, hi), TableLookupBytes(lookup, lo));
}

// RVV has a specialization that avoids the Set().
#if HWY_TARGET != HWY_RVV
// Slower fallback for capped vectors.
template <typename V, class D = DFromV<V>, HWY_IF_LANE_SIZE_D(D, 1), HWY_IF_LT128_D(D)>
HWY_API V PopulationCount(V v) {
  static_assert(IsSame<TFromD<D>, uint8_t>(), "V must be u8");
  const D d;
  // See https://arxiv.org/pdf/1611.07612.pdf, Figure 3
  v = Sub(v, And(ShiftRight<1>(v), Set(d, 0x55)));
  v = Add(And(ShiftRight<2>(v), Set(d, 0x33)), And(v, Set(d, 0x33)));
  return And(Add(v, ShiftRight<4>(v)), Set(d, 0x0F));
}
#endif  // HWY_TARGET != HWY_RVV

template <typename V, class D = DFromV<V>, HWY_IF_LANE_SIZE_D(D, 2)>
HWY_API V PopulationCount(V v) {
  static_assert(IsSame<TFromD<D>, uint16_t>(), "V must be u16");
  const D d;
  const Repartition<uint8_t, decltype(d)> d8;
  const auto vals = BitCast(d, PopulationCount(BitCast(d8, v)));
  return Add(ShiftRight<8>(vals), And(vals, Set(d, 0xFF)));
}

template <typename V, class D = DFromV<V>, HWY_IF_LANE_SIZE_D(D, 4)>
HWY_API V PopulationCount(V v) {
  static_assert(IsSame<TFromD<D>, uint32_t>(), "V must be u32");
  const D d;
  Repartition<uint16_t, decltype(d)> d16;
  auto vals = BitCast(d, PopulationCount(BitCast(d16, v)));
  return Add(ShiftRight<16>(vals), And(vals, Set(d, 0xFF)));
}

#if HWY_HAVE_INTEGER64
template <typename V, class D = DFromV<V>, HWY_IF_LANE_SIZE_D(D, 8)>
HWY_API V PopulationCount(V v) {
  static_assert(IsSame<TFromD<D>, uint64_t>(), "V must be u64");
  const D d;
  Repartition<uint32_t, decltype(d)> d32;
  auto vals = BitCast(d, PopulationCount(BitCast(d32, v)));
  return Add(ShiftRight<32>(vals), And(vals, Set(d, 0xFF)));
}
#endif

#endif  // HWY_NATIVE_POPCNT

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();
