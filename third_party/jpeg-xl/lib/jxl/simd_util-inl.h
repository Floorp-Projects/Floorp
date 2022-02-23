// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

// Misc utilities for SIMD operations

#if defined(LIB_JXL_SIMD_UTIL_INL_H_) == defined(HWY_TARGET_TOGGLE)
#ifdef LIB_JXL_SIMD_UTIL_INL_H_
#undef LIB_JXL_SIMD_UTIL_INL_H_
#else
#define LIB_JXL_SIMD_UTIL_INL_H_
#endif

#include <hwy/highway.h>

HWY_BEFORE_NAMESPACE();
namespace jxl {
namespace HWY_NAMESPACE {

#if HWY_CAP_GE512
using hwy::HWY_NAMESPACE::Half;
using hwy::HWY_NAMESPACE::Vec;
template <size_t i, class DF, class V>
HWY_INLINE Vec<Half<Half<DF>>> Quarter(const DF df, V v) {
  using HF = Half<DF>;
  using HHF = Half<HF>;
  auto half = i >= 2 ? UpperHalf(HF(), v) : LowerHalf(HF(), v);
  return i & 1 ? UpperHalf(HHF(), half) : LowerHalf(HHF(), half);
}

template <class DF, class V>
HWY_INLINE Vec<DF> Concat4(const DF df, V v0, V v1, V v2, V v3) {
  using HF = Half<DF>;
  return Combine(DF(), Combine(HF(), v3, v2), Combine(HF(), v1, v0));
}

#endif

// Stores v0[0], v1[0], v0[1], v1[1], ... to mem, in this order. Mem must be
// aligned.
template <class DF, class V>
void StoreInterleaved(const DF df, V v0, V v1, float* mem) {
#if HWY_TARGET == HWY_SCALAR
  Store(v0, df, mem);
  Store(v1, df, mem + 1);
#elif !HWY_CAP_GE256
  Store(InterleaveLower(df, v0, v1), df, mem);
  Store(InterleaveUpper(df, v0, v1), df, mem + Lanes(df));
#elif !HWY_CAP_GE512
  auto t0 = InterleaveLower(df, v0, v1);
  auto t1 = InterleaveUpper(df, v0, v1);
  Store(ConcatLowerLower(df, t1, t0), df, mem);
  Store(ConcatUpperUpper(df, t1, t0), df, mem + Lanes(df));
#else
  auto t0 = InterleaveLower(df, v0, v1);
  auto t1 = InterleaveUpper(df, v0, v1);
  Store(Concat4(df, Quarter<0>(df, t0), Quarter<0>(df, t1), Quarter<1>(df, t0),
                Quarter<1>(df, t1)),
        df, mem);
  Store(Concat4(df, Quarter<2>(df, t0), Quarter<2>(df, t1), Quarter<3>(df, t0),
                Quarter<3>(df, t1)),
        df, mem + Lanes(df));
#endif
}

// Stores v0[0], v1[0], v2[0], v3[0], v0[1] ... to mem, in this order. Mem must
// be aligned.
template <class DF, class V>
void StoreInterleaved(const DF df, V v0, V v1, V v2, V v3, float* mem) {
#if HWY_TARGET == HWY_SCALAR
  Store(v0, df, mem);
  Store(v1, df, mem + 1);
  Store(v2, df, mem + 2);
  Store(v3, df, mem + 3);
#elif !HWY_CAP_GE256
  auto t0 = InterleaveLower(df, v0, v2);
  auto t1 = InterleaveLower(df, v1, v3);
  auto t2 = InterleaveUpper(df, v0, v2);
  auto t3 = InterleaveUpper(df, v1, v3);
  Store(InterleaveLower(df, t0, t1), df, mem);
  Store(InterleaveUpper(df, t0, t1), df, mem + Lanes(df));
  Store(InterleaveLower(df, t2, t3), df, mem + 2 * Lanes(df));
  Store(InterleaveUpper(df, t2, t3), df, mem + 3 * Lanes(df));
#elif !HWY_CAP_GE512
  auto t0 = InterleaveLower(df, v0, v2);
  auto t1 = InterleaveLower(df, v1, v3);
  auto t2 = InterleaveUpper(df, v0, v2);
  auto t3 = InterleaveUpper(df, v1, v3);

  auto m0 = InterleaveLower(df, t0, t1);
  auto m1 = InterleaveUpper(df, t0, t1);
  auto m2 = InterleaveLower(df, t2, t3);
  auto m3 = InterleaveUpper(df, t2, t3);

  Store(ConcatLowerLower(df, m1, m0), df, mem);
  Store(ConcatLowerLower(df, m3, m2), df, mem + Lanes(df));
  Store(ConcatUpperUpper(df, m1, m0), df, mem + 2 * Lanes(df));
  Store(ConcatUpperUpper(df, m3, m2), df, mem + 3 * Lanes(df));
#else
  auto t0 = InterleaveLower(df, v0, v2);
  auto t1 = InterleaveLower(df, v1, v3);
  auto t2 = InterleaveUpper(df, v0, v2);
  auto t3 = InterleaveUpper(df, v1, v3);

  auto m0 = InterleaveLower(df, t0, t1);
  auto m1 = InterleaveUpper(df, t0, t1);
  auto m2 = InterleaveLower(df, t2, t3);
  auto m3 = InterleaveUpper(df, t2, t3);

  Store(Concat4(df, Quarter<0>(df, m0), Quarter<0>(df, m1), Quarter<0>(df, m2),
                Quarter<0>(df, m3)),
        df, mem);
  Store(Concat4(df, Quarter<1>(df, m0), Quarter<1>(df, m1), Quarter<1>(df, m2),
                Quarter<1>(df, m3)),
        df, mem + Lanes(df));
  Store(Concat4(df, Quarter<2>(df, m0), Quarter<2>(df, m1), Quarter<2>(df, m2),
                Quarter<2>(df, m3)),
        df, mem + 2 * Lanes(df));
  Store(Concat4(df, Quarter<3>(df, m0), Quarter<3>(df, m1), Quarter<3>(df, m2),
                Quarter<3>(df, m3)),
        df, mem + 3 * Lanes(df));
#endif
}

// Stores v0[0], v1[0], v2[0], v3[0], v4[0], v5[0], v6[0], v7[0], v0[1] ... to
// mem, in this order. Mem must be aligned.
template <class DF, class V>
void StoreInterleaved(const DF df, V v0, V v1, V v2, V v3, V v4, V v5, V v6,
                      V v7, float* mem) {
#if HWY_TARGET == HWY_SCALAR
  Store(v0, df, mem);
  Store(v1, df, mem + 1);
  Store(v2, df, mem + 2);
  Store(v3, df, mem + 3);
  Store(v4, df, mem + 4);
  Store(v5, df, mem + 5);
  Store(v6, df, mem + 6);
  Store(v7, df, mem + 7);
#elif !HWY_CAP_GE256
  auto t0 = InterleaveLower(df, v0, v4);
  auto t1 = InterleaveLower(df, v1, v5);
  auto t2 = InterleaveLower(df, v2, v6);
  auto t3 = InterleaveLower(df, v3, v7);
  auto t4 = InterleaveUpper(df, v0, v4);
  auto t5 = InterleaveUpper(df, v1, v5);
  auto t6 = InterleaveUpper(df, v2, v6);
  auto t7 = InterleaveUpper(df, v3, v7);

  auto w0 = InterleaveLower(df, t0, t2);
  auto w1 = InterleaveLower(df, t1, t3);
  auto w2 = InterleaveUpper(df, t0, t2);
  auto w3 = InterleaveUpper(df, t1, t3);
  auto w4 = InterleaveLower(df, t4, t6);
  auto w5 = InterleaveLower(df, t5, t7);
  auto w6 = InterleaveUpper(df, t4, t6);
  auto w7 = InterleaveUpper(df, t5, t7);

  Store(InterleaveLower(df, w0, w1), df, mem);
  Store(InterleaveUpper(df, w0, w1), df, mem + Lanes(df));
  Store(InterleaveLower(df, w2, w3), df, mem + 2 * Lanes(df));
  Store(InterleaveUpper(df, w2, w3), df, mem + 3 * Lanes(df));
  Store(InterleaveLower(df, w4, w5), df, mem + 4 * Lanes(df));
  Store(InterleaveUpper(df, w4, w5), df, mem + 5 * Lanes(df));
  Store(InterleaveLower(df, w6, w7), df, mem + 6 * Lanes(df));
  Store(InterleaveUpper(df, w6, w7), df, mem + 7 * Lanes(df));
#elif !HWY_CAP_GE512
  auto t0 = InterleaveLower(df, v0, v4);
  auto t1 = InterleaveLower(df, v1, v5);
  auto t2 = InterleaveLower(df, v2, v6);
  auto t3 = InterleaveLower(df, v3, v7);
  auto t4 = InterleaveUpper(df, v0, v4);
  auto t5 = InterleaveUpper(df, v1, v5);
  auto t6 = InterleaveUpper(df, v2, v6);
  auto t7 = InterleaveUpper(df, v3, v7);

  auto w0 = InterleaveLower(df, t0, t2);
  auto w1 = InterleaveLower(df, t1, t3);
  auto w2 = InterleaveUpper(df, t0, t2);
  auto w3 = InterleaveUpper(df, t1, t3);
  auto w4 = InterleaveLower(df, t4, t6);
  auto w5 = InterleaveLower(df, t5, t7);
  auto w6 = InterleaveUpper(df, t4, t6);
  auto w7 = InterleaveUpper(df, t5, t7);

  auto m0 = InterleaveLower(df, w0, w1);
  auto m1 = InterleaveUpper(df, w0, w1);
  auto m2 = InterleaveLower(df, w2, w3);
  auto m3 = InterleaveUpper(df, w2, w3);
  auto m4 = InterleaveLower(df, w4, w5);
  auto m5 = InterleaveUpper(df, w4, w5);
  auto m6 = InterleaveLower(df, w6, w7);
  auto m7 = InterleaveUpper(df, w6, w7);

  Store(ConcatLowerLower(df, m1, m0), df, mem);
  Store(ConcatLowerLower(df, m3, m2), df, mem + Lanes(df));
  Store(ConcatLowerLower(df, m5, m4), df, mem + 2 * Lanes(df));
  Store(ConcatLowerLower(df, m7, m6), df, mem + 3 * Lanes(df));
  Store(ConcatUpperUpper(df, m1, m0), df, mem + 4 * Lanes(df));
  Store(ConcatUpperUpper(df, m3, m2), df, mem + 5 * Lanes(df));
  Store(ConcatUpperUpper(df, m5, m4), df, mem + 6 * Lanes(df));
  Store(ConcatUpperUpper(df, m7, m6), df, mem + 7 * Lanes(df));
#else
  auto t0 = InterleaveLower(df, v0, v4);
  auto t1 = InterleaveLower(df, v1, v5);
  auto t2 = InterleaveLower(df, v2, v6);
  auto t3 = InterleaveLower(df, v3, v7);
  auto t4 = InterleaveUpper(df, v0, v4);
  auto t5 = InterleaveUpper(df, v1, v5);
  auto t6 = InterleaveUpper(df, v2, v6);
  auto t7 = InterleaveUpper(df, v3, v7);

  auto w0 = InterleaveLower(df, t0, t2);
  auto w1 = InterleaveLower(df, t1, t3);
  auto w2 = InterleaveUpper(df, t0, t2);
  auto w3 = InterleaveUpper(df, t1, t3);
  auto w4 = InterleaveLower(df, t4, t6);
  auto w5 = InterleaveLower(df, t5, t7);
  auto w6 = InterleaveUpper(df, t4, t6);
  auto w7 = InterleaveUpper(df, t5, t7);

  auto m0 = InterleaveLower(df, w0, w1);
  auto m1 = InterleaveUpper(df, w0, w1);
  auto m2 = InterleaveLower(df, w2, w3);
  auto m3 = InterleaveUpper(df, w2, w3);
  auto m4 = InterleaveLower(df, w4, w5);
  auto m5 = InterleaveUpper(df, w4, w5);
  auto m6 = InterleaveLower(df, w6, w7);
  auto m7 = InterleaveUpper(df, w6, w7);

  Store(Concat4(df, Quarter<0>(df, m0), Quarter<0>(df, m1), Quarter<0>(df, m2),
                Quarter<0>(df, m3)),
        df, mem);
  Store(Concat4(df, Quarter<0>(df, m4), Quarter<0>(df, m5), Quarter<0>(df, m6),
                Quarter<0>(df, m7)),
        df, mem + Lanes(df));
  Store(Concat4(df, Quarter<1>(df, m0), Quarter<1>(df, m1), Quarter<1>(df, m2),
                Quarter<1>(df, m3)),
        df, mem + 2 * Lanes(df));
  Store(Concat4(df, Quarter<1>(df, m4), Quarter<1>(df, m5), Quarter<1>(df, m6),
                Quarter<1>(df, m7)),
        df, mem + 3 * Lanes(df));
  Store(Concat4(df, Quarter<2>(df, m0), Quarter<2>(df, m1), Quarter<2>(df, m2),
                Quarter<2>(df, m3)),
        df, mem + 4 * Lanes(df));
  Store(Concat4(df, Quarter<2>(df, m4), Quarter<2>(df, m5), Quarter<2>(df, m6),
                Quarter<2>(df, m7)),
        df, mem + 5 * Lanes(df));
  Store(Concat4(df, Quarter<3>(df, m0), Quarter<3>(df, m1), Quarter<3>(df, m2),
                Quarter<3>(df, m3)),
        df, mem + 6 * Lanes(df));
  Store(Concat4(df, Quarter<3>(df, m4), Quarter<3>(df, m5), Quarter<3>(df, m6),
                Quarter<3>(df, m7)),
        df, mem + 7 * Lanes(df));
#endif
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace jxl
HWY_AFTER_NAMESPACE();

#endif  // LIB_JXL_SIMD_UTIL_INL_H_
