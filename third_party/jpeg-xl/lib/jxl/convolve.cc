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

#include "lib/jxl/convolve.h"

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "lib/jxl/convolve.cc"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

#include "lib/jxl/common.h"  // RoundUpTo
#include "lib/jxl/convolve-inl.h"
#include "lib/jxl/image_ops.h"
HWY_BEFORE_NAMESPACE();
namespace jxl {
namespace HWY_NAMESPACE {

// These templates are not found via ADL.
using hwy::HWY_NAMESPACE::Vec;

// Weighted sum of 1x5 pixels around ix, iy with [wx2 wx1 wx0 wx1 wx2].
template <class WrapY>
static float WeightedSumBorder(const ImageF& in, const WrapY wrap_y,
                               const int64_t ix, const int64_t iy,
                               const size_t xsize, const size_t ysize,
                               const float wx0, const float wx1,
                               const float wx2) {
  const WrapMirror wrap_x;
  const float* JXL_RESTRICT row = in.ConstRow(wrap_y(iy, ysize));
  const float in_m2 = row[wrap_x(ix - 2, xsize)];
  const float in_p2 = row[wrap_x(ix + 2, xsize)];
  const float in_m1 = row[wrap_x(ix - 1, xsize)];
  const float in_p1 = row[wrap_x(ix + 1, xsize)];
  const float in_00 = row[ix];
  const float sum_2 = wx2 * (in_m2 + in_p2);
  const float sum_1 = wx1 * (in_m1 + in_p1);
  const float sum_0 = wx0 * in_00;
  return sum_2 + sum_1 + sum_0;
}

template <class WrapY, class V>
static V WeightedSum(const ImageF& in, const WrapY wrap_y, const size_t ix,
                     const int64_t iy, const size_t ysize, const V wx0,
                     const V wx1, const V wx2) {
  const HWY_FULL(float) d;
  const float* JXL_RESTRICT center = in.ConstRow(wrap_y(iy, ysize)) + ix;
  const auto in_m2 = LoadU(d, center - 2);
  const auto in_p2 = LoadU(d, center + 2);
  const auto in_m1 = LoadU(d, center - 1);
  const auto in_p1 = LoadU(d, center + 1);
  const auto in_00 = Load(d, center);
  const auto sum_2 = wx2 * (in_m2 + in_p2);
  const auto sum_1 = wx1 * (in_m1 + in_p1);
  const auto sum_0 = wx0 * in_00;
  return sum_2 + sum_1 + sum_0;
}

// Produces result for one pixel
template <class WrapY>
float Symmetric5Border(const ImageF& in, const Rect& rect, const int64_t ix,
                       const int64_t iy, const WeightsSymmetric5& weights) {
  const float w0 = weights.c[0];
  const float w1 = weights.r[0];
  const float w2 = weights.R[0];
  const float w4 = weights.d[0];
  const float w5 = weights.L[0];
  const float w8 = weights.D[0];

  const size_t xsize = rect.xsize();
  const size_t ysize = rect.ysize();
  const WrapY wrap_y;
  // Unrolled loop over all 5 rows of the kernel.
  float sum0 = WeightedSumBorder(in, wrap_y, ix, iy, xsize, ysize, w0, w1, w2);

  sum0 += WeightedSumBorder(in, wrap_y, ix, iy - 2, xsize, ysize, w2, w5, w8);
  float sum1 =
      WeightedSumBorder(in, wrap_y, ix, iy + 2, xsize, ysize, w2, w5, w8);

  sum0 += WeightedSumBorder(in, wrap_y, ix, iy - 1, xsize, ysize, w1, w4, w5);
  sum1 += WeightedSumBorder(in, wrap_y, ix, iy + 1, xsize, ysize, w1, w4, w5);

  return sum0 + sum1;
}

// Produces result for one vector's worth of pixels
template <class WrapY>
static void Symmetric5Interior(const ImageF& in, const Rect& rect,
                               const int64_t ix, const int64_t iy,
                               const WeightsSymmetric5& weights,
                               float* JXL_RESTRICT row_out) {
  const HWY_FULL(float) d;

  const auto w0 = LoadDup128(d, weights.c);
  const auto w1 = LoadDup128(d, weights.r);
  const auto w2 = LoadDup128(d, weights.R);
  const auto w4 = LoadDup128(d, weights.d);
  const auto w5 = LoadDup128(d, weights.L);
  const auto w8 = LoadDup128(d, weights.D);

  const size_t ysize = rect.ysize();
  const WrapY wrap_y;
  // Unrolled loop over all 5 rows of the kernel.
  auto sum0 = WeightedSum(in, wrap_y, ix, iy, ysize, w0, w1, w2);

  sum0 += WeightedSum(in, wrap_y, ix, iy - 2, ysize, w2, w5, w8);
  auto sum1 = WeightedSum(in, wrap_y, ix, iy + 2, ysize, w2, w5, w8);

  sum0 += WeightedSum(in, wrap_y, ix, iy - 1, ysize, w1, w4, w5);
  sum1 += WeightedSum(in, wrap_y, ix, iy + 1, ysize, w1, w4, w5);

  Store(sum0 + sum1, d, row_out + ix);
}

template <class WrapY>
static void Symmetric5Row(const ImageF& in, const Rect& rect, const int64_t iy,
                          const WeightsSymmetric5& weights,
                          float* JXL_RESTRICT row_out) {
  const int64_t kRadius = 2;
  const size_t xsize = rect.xsize();

  size_t ix = 0;
  const HWY_FULL(float) d;
  const size_t N = Lanes(d);
  const size_t aligned_x = RoundUpTo(kRadius, N);
  for (; ix < std::min(aligned_x, xsize); ++ix) {
    row_out[ix] = Symmetric5Border<WrapY>(in, rect, ix, iy, weights);
  }
  for (; ix + N + kRadius <= xsize; ix += N) {
    Symmetric5Interior<WrapY>(in, rect, ix, iy, weights, row_out);
  }
  for (; ix < xsize; ++ix) {
    row_out[ix] = Symmetric5Border<WrapY>(in, rect, ix, iy, weights);
  }
}

static JXL_NOINLINE void Symmetric5BorderRow(const ImageF& in, const Rect& rect,
                                             const int64_t iy,
                                             const WeightsSymmetric5& weights,
                                             float* JXL_RESTRICT row_out) {
  return Symmetric5Row<WrapMirror>(in, rect, iy, weights, row_out);
}

#if HWY_TARGET != HWY_SCALAR

// Returns indices for SetTableIndices such that TableLookupLanes on the
// rightmost unaligned vector (rightmost sample in its most-significant lane)
// returns the mirrored values, with the mirror outside the last valid sample.
static inline const int32_t* MirrorLanes(const size_t mod) {
  const HWY_CAPPED(float, 16) d;
  constexpr size_t kN = MaxLanes(d);

  // For mod = `image width mod 16` 0..15:
  // last full vec     mirrored (mem order)  loadedVec  mirrorVec  idxVec
  // 0123456789abcdef| fedcba9876543210      fed..210   012..def   012..def
  // 0123456789abcdef|0 0fedcba98765432      0fe..321   234..f00   123..eff
  // 0123456789abcdef|01 10fedcba987654      10f..432   456..110   234..ffe
  // 0123456789abcdef|012 210fedcba9876      210..543   67..2210   34..ffed
  // 0123456789abcdef|0123 3210fedcba98      321..654   8..33210   4..ffedc
  // 0123456789abcdef|01234 43210fedcba
  // 0123456789abcdef|012345 543210fedc
  // 0123456789abcdef|0123456 6543210fe
  // 0123456789abcdef|01234567 76543210
  // 0123456789abcdef|012345678 8765432
  // 0123456789abcdef|0123456789 987654
  // 0123456789abcdef|0123456789A A9876
  // 0123456789abcdef|0123456789AB BA98
  // 0123456789abcdef|0123456789ABC CBA
  // 0123456789abcdef|0123456789ABCD DC
  // 0123456789abcdef|0123456789ABCDE E      EDC..10f   EED..210   ffe..321
#if HWY_CAP_GE512
  HWY_ALIGN static constexpr int32_t idx_lanes[2 * kN - 1] = {
      1,  2,  3,  4,  5,  6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 15,  //
      14, 13, 12, 11, 10, 9, 8, 7, 6, 5,  4,  3,  2,  1,  0};
#elif HWY_CAP_GE256
  HWY_ALIGN static constexpr int32_t idx_lanes[2 * kN - 1] = {
      1, 2, 3, 4, 5, 6, 7, 7,  //
      6, 5, 4, 3, 2, 1, 0};
#else  // 128-bit
  HWY_ALIGN static constexpr int32_t idx_lanes[2 * kN - 1] = {1, 2, 3, 3,  //
                                                              2, 1, 0};
#endif
  return idx_lanes + kN - 1 - mod;
}

#endif  // HWY_TARGET != HWY_SCALAR

namespace strategy {

struct StrategyBase {
  using D = HWY_CAPPED(float, 16);
  using V = Vec<D>;
};

// 3x3 convolution by symmetric kernel with a single scan through the input.
class Symmetric3 : public StrategyBase {
 public:
  static constexpr int64_t kRadius = 1;

  // Only accesses pixels in [0, xsize).
  template <size_t kSizeModN, class WrapRow>
  static JXL_INLINE void ConvolveRow(const float* const JXL_RESTRICT row_m,
                                     const size_t xsize, const int64_t stride,
                                     const WrapRow& wrap_row,
                                     const WeightsSymmetric3& weights,
                                     float* const JXL_RESTRICT row_out) {
    const D d;
    // t, m, b = top, middle, bottom row;
    const float* const JXL_RESTRICT row_t = wrap_row(row_m - stride, stride);
    const float* const JXL_RESTRICT row_b = wrap_row(row_m + stride, stride);

    // Must load in advance - compiler doesn't understand LoadDup128 and
    // schedules them too late.
    const V w0 = LoadDup128(d, weights.c);
    const V w1 = LoadDup128(d, weights.r);
    const V w2 = LoadDup128(d, weights.d);

    // l, c, r = left, center, right. Leftmost vector: need FirstL1.
    {
      const V tc = LoadU(d, row_t + 0);
      const V mc = LoadU(d, row_m + 0);
      const V bc = LoadU(d, row_b + 0);
      const V tl = Neighbors::FirstL1(tc);
      const V tr = LoadU(d, row_t + 0 + 1);
      const V ml = Neighbors::FirstL1(mc);
      const V mr = LoadU(d, row_m + 0 + 1);
      const V bl = Neighbors::FirstL1(bc);
      const V br = LoadU(d, row_b + 0 + 1);
      const V conv =
          WeightedSum(tl, tc, tr, ml, mc, mr, bl, bc, br, w0, w1, w2);
      Store(conv, d, row_out + 0);
    }

    // Loop as long as we can load enough new values:
    const size_t N = Lanes(d);
    size_t x = N;
    for (; x + N + kRadius <= xsize; x += N) {
      const auto conv = ConvolveValid(row_t, row_m, row_b, x, w0, w1, w2);
      Store(conv, d, row_out + x);
    }

    // For final (partial) vector:
    const V tc = LoadU(d, row_t + x);
    const V mc = LoadU(d, row_m + x);
    const V bc = LoadU(d, row_b + x);

    V tr, mr, br;
#if HWY_TARGET == HWY_SCALAR
    tr = tc;  // Single-lane => mirrored right neighbor = center value.
    mr = mc;
    br = bc;
#else
    if (kSizeModN == 0) {
      // The above loop didn't handle the last vector because it needs an
      // additional right neighbor (generated via mirroring).
      auto mirror = SetTableIndices(d, MirrorLanes(N - 1));
      tr = TableLookupLanes(tc, mirror);
      mr = TableLookupLanes(mc, mirror);
      br = TableLookupLanes(bc, mirror);
    } else {
      auto mirror = SetTableIndices(d, MirrorLanes((xsize % N) - 1));
      // Loads last valid value into uppermost lane and mirrors.
      tr = TableLookupLanes(LoadU(d, row_t + xsize - N), mirror);
      mr = TableLookupLanes(LoadU(d, row_m + xsize - N), mirror);
      br = TableLookupLanes(LoadU(d, row_b + xsize - N), mirror);
    }
#endif

    const V tl = LoadU(d, row_t + x - 1);
    const V ml = LoadU(d, row_m + x - 1);
    const V bl = LoadU(d, row_b + x - 1);
    const V conv = WeightedSum(tl, tc, tr, ml, mc, mr, bl, bc, br, w0, w1, w2);
    Store(conv, d, row_out + x);
  }

 private:
  // Returns sum{x_i * w_i}.
  template <class V>
  static JXL_INLINE V WeightedSum(const V tl, const V tc, const V tr,
                                  const V ml, const V mc, const V mr,
                                  const V bl, const V bc, const V br,
                                  const V w0, const V w1, const V w2) {
    const V sum_tb = tc + bc;

    // Faster than 5 mul + 4 FMA.
    const V mul0 = mc * w0;
    const V sum_lr = ml + mr;

    const V x1 = sum_tb + sum_lr;
    const V mul1 = MulAdd(x1, w1, mul0);

    const V sum_t2 = tl + tr;
    const V sum_b2 = bl + br;
    const V x2 = sum_t2 + sum_b2;
    const V mul2 = MulAdd(x2, w2, mul1);
    return mul2;
  }

  static JXL_INLINE V ConvolveValid(const float* JXL_RESTRICT row_t,
                                    const float* JXL_RESTRICT row_m,
                                    const float* JXL_RESTRICT row_b,
                                    const int64_t x, const V w0, const V w1,
                                    const V w2) {
    const D d;
    const V tc = LoadU(d, row_t + x);
    const V mc = LoadU(d, row_m + x);
    const V bc = LoadU(d, row_b + x);
    const V tl = LoadU(d, row_t + x - 1);
    const V tr = LoadU(d, row_t + x + 1);
    const V ml = LoadU(d, row_m + x - 1);
    const V mr = LoadU(d, row_m + x + 1);
    const V bl = LoadU(d, row_b + x - 1);
    const V br = LoadU(d, row_b + x + 1);
    return WeightedSum(tl, tc, tr, ml, mc, mr, bl, bc, br, w0, w1, w2);
  }
};

// 5x5 convolution by separable kernel with a single scan through the input.
// This is more cache-efficient than separate horizontal/vertical passes, and
// possibly faster (given enough registers) than tiling and/or transposing.
//
// Overview: imagine a 5x5 window around a central pixel. First convolve the
// rows by multiplying the pixels with the corresponding weights from
// WeightsSeparable5.horz[abs(x_offset) * 4]. Then multiply each of these
// intermediate results by the corresponding vertical weight, i.e.
// vert[abs(y_offset) * 4]. Finally, store the sum of these values as the
// convolution result at the position of the central pixel in the output.
//
// Each of these operations uses SIMD vectors. The central pixel and most
// importantly the output are aligned, so neighnoring pixels (e.g. x_offset=1)
// require unaligned loads. Because weights are supplied in identical groups of
// 4, we can use LoadDup128 to load them (slightly faster).
//
// Uses mirrored boundary handling. Until x >= kRadius, the horizontal
// convolution uses Neighbors class to shuffle vectors as if each of its lanes
// had been loaded from the mirrored offset. Similarly, the last full vector to
// write uses mirroring. In the case of scalar vectors, Neighbors is not usable
// and the value is loaded directly. Otherwise, the number of valid pixels
// modulo the vector size enables a small optimization: for smaller offsets,
// a non-mirrored load is sufficient.
class Separable5 : public StrategyBase {
 public:
  static constexpr int64_t kRadius = 2;

  template <size_t kSizeModN, class WrapRow>
  static JXL_INLINE void ConvolveRow(const float* const JXL_RESTRICT row_m,
                                     const size_t xsize, const int64_t stride,
                                     const WrapRow& wrap_row,
                                     const WeightsSeparable5& weights,
                                     float* const JXL_RESTRICT row_out) {
    const D d;
    const int64_t neg_stride = -stride;  // allows LEA addressing.
    const float* const JXL_RESTRICT row_t2 =
        wrap_row(row_m + 2 * neg_stride, stride);
    const float* const JXL_RESTRICT row_t1 =
        wrap_row(row_m + 1 * neg_stride, stride);
    const float* const JXL_RESTRICT row_b1 =
        wrap_row(row_m + 1 * stride, stride);
    const float* const JXL_RESTRICT row_b2 =
        wrap_row(row_m + 2 * stride, stride);

    const V wh0 = LoadDup128(d, weights.horz + 0 * 4);
    const V wh1 = LoadDup128(d, weights.horz + 1 * 4);
    const V wh2 = LoadDup128(d, weights.horz + 2 * 4);
    const V wv0 = LoadDup128(d, weights.vert + 0 * 4);
    const V wv1 = LoadDup128(d, weights.vert + 1 * 4);
    const V wv2 = LoadDup128(d, weights.vert + 2 * 4);

    size_t x = 0;

    // More than one iteration for scalars.
    for (; x < kRadius; x += Lanes(d)) {
      const V conv0 = HorzConvolveFirst(row_m, x, xsize, wh0, wh1, wh2) * wv0;

      const V conv1t = HorzConvolveFirst(row_t1, x, xsize, wh0, wh1, wh2);
      const V conv1b = HorzConvolveFirst(row_b1, x, xsize, wh0, wh1, wh2);
      const V conv1 = MulAdd(conv1t + conv1b, wv1, conv0);

      const V conv2t = HorzConvolveFirst(row_t2, x, xsize, wh0, wh1, wh2);
      const V conv2b = HorzConvolveFirst(row_b2, x, xsize, wh0, wh1, wh2);
      const V conv2 = MulAdd(conv2t + conv2b, wv2, conv1);
      Store(conv2, d, row_out + x);
    }

    // Main loop: load inputs without padding
    for (; x + Lanes(d) + kRadius <= xsize; x += Lanes(d)) {
      const V conv0 = HorzConvolve(row_m + x, wh0, wh1, wh2) * wv0;

      const V conv1t = HorzConvolve(row_t1 + x, wh0, wh1, wh2);
      const V conv1b = HorzConvolve(row_b1 + x, wh0, wh1, wh2);
      const V conv1 = MulAdd(conv1t + conv1b, wv1, conv0);

      const V conv2t = HorzConvolve(row_t2 + x, wh0, wh1, wh2);
      const V conv2b = HorzConvolve(row_b2 + x, wh0, wh1, wh2);
      const V conv2 = MulAdd(conv2t + conv2b, wv2, conv1);
      Store(conv2, d, row_out + x);
    }

    // Last full vector to write (the above loop handled mod >= kRadius)
#if HWY_TARGET == HWY_SCALAR
    while (x < xsize) {
#else
    if (kSizeModN < kRadius) {
#endif
      const V conv0 =
          HorzConvolveLast<kSizeModN>(row_m, x, xsize, wh0, wh1, wh2) * wv0;

      const V conv1t =
          HorzConvolveLast<kSizeModN>(row_t1, x, xsize, wh0, wh1, wh2);
      const V conv1b =
          HorzConvolveLast<kSizeModN>(row_b1, x, xsize, wh0, wh1, wh2);
      const V conv1 = MulAdd(conv1t + conv1b, wv1, conv0);

      const V conv2t =
          HorzConvolveLast<kSizeModN>(row_t2, x, xsize, wh0, wh1, wh2);
      const V conv2b =
          HorzConvolveLast<kSizeModN>(row_b2, x, xsize, wh0, wh1, wh2);
      const V conv2 = MulAdd(conv2t + conv2b, wv2, conv1);
      Store(conv2, d, row_out + x);
      x += Lanes(d);
    }

    // If mod = 0, the above vector was the last.
    if (kSizeModN != 0) {
      for (; x < xsize; ++x) {
        float mul = 0.0f;
        for (int64_t dy = -kRadius; dy <= kRadius; ++dy) {
          const float wy = weights.vert[std::abs(dy) * 4];
          const float* clamped_row = wrap_row(row_m + dy * stride, stride);
          for (int64_t dx = -kRadius; dx <= kRadius; ++dx) {
            const float wx = weights.horz[std::abs(dx) * 4];
            const int64_t clamped_x = Mirror(x + dx, xsize);
            mul += clamped_row[clamped_x] * wx * wy;
          }
        }
        row_out[x] = mul;
      }
    }
  }

 private:
  // Same as HorzConvolve for the first/last vector in a row.
  static JXL_INLINE V HorzConvolveFirst(const float* const JXL_RESTRICT row,
                                        const int64_t x, const int64_t xsize,
                                        const V wh0, const V wh1, const V wh2) {
    const D d;
    const V c = LoadU(d, row + x);
    const V mul0 = c * wh0;

#if HWY_TARGET == HWY_SCALAR
    const V l1 = LoadU(d, row + Mirror(x - 1, xsize));
    const V l2 = LoadU(d, row + Mirror(x - 2, xsize));
#else
    (void)xsize;
    const V l1 = Neighbors::FirstL1(c);
    const V l2 = Neighbors::FirstL2(c);
#endif

    const V r1 = LoadU(d, row + x + 1);
    const V r2 = LoadU(d, row + x + 2);

    const V mul1 = MulAdd(l1 + r1, wh1, mul0);
    const V mul2 = MulAdd(l2 + r2, wh2, mul1);
    return mul2;
  }

  template <size_t kSizeModN>
  static JXL_INLINE V HorzConvolveLast(const float* const JXL_RESTRICT row,
                                       const int64_t x, const int64_t xsize,
                                       const V wh0, const V wh1, const V wh2) {
    const D d;
    const V c = LoadU(d, row + x);
    const V mul0 = c * wh0;

    const V l1 = LoadU(d, row + x - 1);
    const V l2 = LoadU(d, row + x - 2);

    V r1, r2;
#if HWY_TARGET == HWY_SCALAR
    r1 = LoadU(d, row + Mirror(x + 1, xsize));
    r2 = LoadU(d, row + Mirror(x + 2, xsize));
#else
    const size_t N = Lanes(d);
    if (kSizeModN == 0) {
      r2 = TableLookupLanes(c, SetTableIndices(d, MirrorLanes(N - 2)));
      r1 = TableLookupLanes(c, SetTableIndices(d, MirrorLanes(N - 1)));
    } else {  // == 1
      const auto last = LoadU(d, row + xsize - N);
      r2 = TableLookupLanes(last, SetTableIndices(d, MirrorLanes(N - 1)));
      r1 = last;
    }
#endif

    // Sum of pixels with Manhattan distance i, multiplied by weights[i].
    const V sum1 = l1 + r1;
    const V mul1 = MulAdd(sum1, wh1, mul0);
    const V sum2 = l2 + r2;
    const V mul2 = MulAdd(sum2, wh2, mul1);
    return mul2;
  }

  // Requires kRadius valid pixels before/after pos.
  static JXL_INLINE V HorzConvolve(const float* const JXL_RESTRICT pos,
                                   const V wh0, const V wh1, const V wh2) {
    const D d;
    const V c = LoadU(d, pos);
    const V mul0 = c * wh0;

    // Loading anew is faster than combining vectors.
    const V l1 = LoadU(d, pos - 1);
    const V r1 = LoadU(d, pos + 1);
    const V l2 = LoadU(d, pos - 2);
    const V r2 = LoadU(d, pos + 2);
    // Sum of pixels with Manhattan distance i, multiplied by weights[i].
    const V sum1 = l1 + r1;
    const V mul1 = MulAdd(sum1, wh1, mul0);
    const V sum2 = l2 + r2;
    const V mul2 = MulAdd(sum2, wh2, mul1);
    return mul2;
  }
};  // namespace strategy

// 7x7 convolution by separable kernel with a single scan through the input.
// Extended version of Separable5, see documentation there.
class Separable7 : public StrategyBase {
 public:
  static constexpr int64_t kRadius = 3;

  template <size_t kSizeModN, class WrapRow>
  static JXL_INLINE void ConvolveRow(const float* const JXL_RESTRICT row_m,
                                     const size_t xsize, const int64_t stride,
                                     const WrapRow& wrap_row,
                                     const WeightsSeparable7& weights,
                                     float* const JXL_RESTRICT row_out) {
    const D d;
    const int64_t neg_stride = -stride;  // allows LEA addressing.
    const float* const JXL_RESTRICT row_t3 =
        wrap_row(row_m + 3 * neg_stride, stride);
    const float* const JXL_RESTRICT row_t2 =
        wrap_row(row_m + 2 * neg_stride, stride);
    const float* const JXL_RESTRICT row_t1 =
        wrap_row(row_m + 1 * neg_stride, stride);
    const float* const JXL_RESTRICT row_b1 =
        wrap_row(row_m + 1 * stride, stride);
    const float* const JXL_RESTRICT row_b2 =
        wrap_row(row_m + 2 * stride, stride);
    const float* const JXL_RESTRICT row_b3 =
        wrap_row(row_m + 3 * stride, stride);

    const V wh0 = LoadDup128(d, weights.horz + 0 * 4);
    const V wh1 = LoadDup128(d, weights.horz + 1 * 4);
    const V wh2 = LoadDup128(d, weights.horz + 2 * 4);
    const V wh3 = LoadDup128(d, weights.horz + 3 * 4);
    const V wv0 = LoadDup128(d, weights.vert + 0 * 4);
    const V wv1 = LoadDup128(d, weights.vert + 1 * 4);
    const V wv2 = LoadDup128(d, weights.vert + 2 * 4);
    const V wv3 = LoadDup128(d, weights.vert + 3 * 4);

    size_t x = 0;

    // More than one iteration for scalars.
    for (; x < kRadius; x += Lanes(d)) {
      const V conv0 =
          HorzConvolveFirst(row_m, x, xsize, wh0, wh1, wh2, wh3) * wv0;

      const V conv1t = HorzConvolveFirst(row_t1, x, xsize, wh0, wh1, wh2, wh3);
      const V conv1b = HorzConvolveFirst(row_b1, x, xsize, wh0, wh1, wh2, wh3);
      const V conv1 = MulAdd(conv1t + conv1b, wv1, conv0);

      const V conv2t = HorzConvolveFirst(row_t2, x, xsize, wh0, wh1, wh2, wh3);
      const V conv2b = HorzConvolveFirst(row_b2, x, xsize, wh0, wh1, wh2, wh3);
      const V conv2 = MulAdd(conv2t + conv2b, wv2, conv1);

      const V conv3t = HorzConvolveFirst(row_t3, x, xsize, wh0, wh1, wh2, wh3);
      const V conv3b = HorzConvolveFirst(row_b3, x, xsize, wh0, wh1, wh2, wh3);
      const V conv3 = MulAdd(conv3t + conv3b, wv3, conv2);

      Store(conv3, d, row_out + x);
    }

    // Main loop: load inputs without padding
    for (; x + Lanes(d) + kRadius <= xsize; x += Lanes(d)) {
      const V conv0 = HorzConvolve(row_m + x, wh0, wh1, wh2, wh3) * wv0;

      const V conv1t = HorzConvolve(row_t1 + x, wh0, wh1, wh2, wh3);
      const V conv1b = HorzConvolve(row_b1 + x, wh0, wh1, wh2, wh3);
      const V conv1 = MulAdd(conv1t + conv1b, wv1, conv0);

      const V conv2t = HorzConvolve(row_t2 + x, wh0, wh1, wh2, wh3);
      const V conv2b = HorzConvolve(row_b2 + x, wh0, wh1, wh2, wh3);
      const V conv2 = MulAdd(conv2t + conv2b, wv2, conv1);

      const V conv3t = HorzConvolve(row_t3 + x, wh0, wh1, wh2, wh3);
      const V conv3b = HorzConvolve(row_b3 + x, wh0, wh1, wh2, wh3);
      const V conv3 = MulAdd(conv3t + conv3b, wv3, conv2);

      Store(conv3, d, row_out + x);
    }

    // Last full vector to write (the above loop handled mod >= kRadius)
#if HWY_TARGET == HWY_SCALAR
    while (x < xsize) {
#else
    if (kSizeModN < kRadius) {
#endif
      const V conv0 =
          HorzConvolveLast<kSizeModN>(row_m, x, xsize, wh0, wh1, wh2, wh3) *
          wv0;

      const V conv1t =
          HorzConvolveLast<kSizeModN>(row_t1, x, xsize, wh0, wh1, wh2, wh3);
      const V conv1b =
          HorzConvolveLast<kSizeModN>(row_b1, x, xsize, wh0, wh1, wh2, wh3);
      const V conv1 = MulAdd(conv1t + conv1b, wv1, conv0);

      const V conv2t =
          HorzConvolveLast<kSizeModN>(row_t2, x, xsize, wh0, wh1, wh2, wh3);
      const V conv2b =
          HorzConvolveLast<kSizeModN>(row_b2, x, xsize, wh0, wh1, wh2, wh3);
      const V conv2 = MulAdd(conv2t + conv2b, wv2, conv1);

      const V conv3t =
          HorzConvolveLast<kSizeModN>(row_t3, x, xsize, wh0, wh1, wh2, wh3);
      const V conv3b =
          HorzConvolveLast<kSizeModN>(row_b3, x, xsize, wh0, wh1, wh2, wh3);
      const V conv3 = MulAdd(conv3t + conv3b, wv3, conv2);

      Store(conv3, d, row_out + x);
      x += Lanes(d);
    }

    // If mod = 0, the above vector was the last.
    if (kSizeModN != 0) {
      for (; x < xsize; ++x) {
        float mul = 0.0f;
        for (int64_t dy = -kRadius; dy <= kRadius; ++dy) {
          const float wy = weights.vert[std::abs(dy) * 4];
          const float* clamped_row = wrap_row(row_m + dy * stride, stride);
          for (int64_t dx = -kRadius; dx <= kRadius; ++dx) {
            const float wx = weights.horz[std::abs(dx) * 4];
            const int64_t clamped_x = Mirror(x + dx, xsize);
            mul += clamped_row[clamped_x] * wx * wy;
          }
        }
        row_out[x] = mul;
      }
    }
  }

 private:
  // Same as HorzConvolve for the first/last vector in a row.
  static JXL_INLINE V HorzConvolveFirst(const float* const JXL_RESTRICT row,
                                        const int64_t x, const int64_t xsize,
                                        const V wh0, const V wh1, const V wh2,
                                        const V wh3) {
    const D d;
    const V c = LoadU(d, row + x);
    const V mul0 = c * wh0;

#if HWY_TARGET == HWY_SCALAR
    const V l1 = LoadU(d, row + Mirror(x - 1, xsize));
    const V l2 = LoadU(d, row + Mirror(x - 2, xsize));
    const V l3 = LoadU(d, row + Mirror(x - 3, xsize));
#else
    (void)xsize;
    const V l1 = Neighbors::FirstL1(c);
    const V l2 = Neighbors::FirstL2(c);
    const V l3 = Neighbors::FirstL3(c);
#endif

    const V r1 = LoadU(d, row + x + 1);
    const V r2 = LoadU(d, row + x + 2);
    const V r3 = LoadU(d, row + x + 3);

    const V mul1 = MulAdd(l1 + r1, wh1, mul0);
    const V mul2 = MulAdd(l2 + r2, wh2, mul1);
    const V mul3 = MulAdd(l3 + r3, wh3, mul2);
    return mul3;
  }

  template <size_t kSizeModN>
  static JXL_INLINE V HorzConvolveLast(const float* const JXL_RESTRICT row,
                                       const int64_t x, const int64_t xsize,
                                       const V wh0, const V wh1, const V wh2,
                                       const V wh3) {
    const D d;
    const V c = LoadU(d, row + x);
    const V mul0 = c * wh0;

    const V l1 = LoadU(d, row + x - 1);
    const V l2 = LoadU(d, row + x - 2);
    const V l3 = LoadU(d, row + x - 3);

    V r1, r2, r3;
#if HWY_TARGET == HWY_SCALAR
    r1 = LoadU(d, row + Mirror(x + 1, xsize));
    r2 = LoadU(d, row + Mirror(x + 2, xsize));
    r3 = LoadU(d, row + Mirror(x + 3, xsize));
#else
    const size_t N = Lanes(d);
    if (kSizeModN == 0) {
      r3 = TableLookupLanes(c, SetTableIndices(d, MirrorLanes(N - 3)));
      r2 = TableLookupLanes(c, SetTableIndices(d, MirrorLanes(N - 2)));
      r1 = TableLookupLanes(c, SetTableIndices(d, MirrorLanes(N - 1)));
    } else if (kSizeModN == 1) {
      const auto last = LoadU(d, row + xsize - N);
      r3 = TableLookupLanes(last, SetTableIndices(d, MirrorLanes(N - 2)));
      r2 = TableLookupLanes(last, SetTableIndices(d, MirrorLanes(N - 1)));
      r1 = last;
    } else /* kSizeModN >= 2 */ {
      const auto last = LoadU(d, row + xsize - N);
      r3 = TableLookupLanes(last, SetTableIndices(d, MirrorLanes(N - 1)));
      r2 = last;
      r1 = LoadU(d, row + x + 1);
    }
#endif

    // Sum of pixels with Manhattan distance i, multiplied by weights[i].
    const V sum1 = l1 + r1;
    const V mul1 = MulAdd(sum1, wh1, mul0);
    const V sum2 = l2 + r2;
    const V mul2 = MulAdd(sum2, wh2, mul1);
    const V sum3 = l3 + r3;
    const V mul3 = MulAdd(sum3, wh3, mul2);
    return mul3;
  }

  // Returns one vector of horizontal convolution results; lane i is the result
  // for pixel pos + i. This is the fast path for interior pixels, i.e. kRadius
  // valid pixels before/after pos.
  static JXL_INLINE V HorzConvolve(const float* const JXL_RESTRICT pos,
                                   const V wh0, const V wh1, const V wh2,
                                   const V wh3) {
    const D d;
    const V c = LoadU(d, pos);
    const V mul0 = c * wh0;

    // TODO(janwas): better to Combine
    const V l1 = LoadU(d, pos - 1);
    const V r1 = LoadU(d, pos + 1);
    const V l2 = LoadU(d, pos - 2);
    const V r2 = LoadU(d, pos + 2);
    const V l3 = LoadU(d, pos - 3);
    const V r3 = LoadU(d, pos + 3);
    // Sum of pixels with Manhattan distance i, multiplied by weights[i].
    const V sum1 = l1 + r1;
    const V mul1 = MulAdd(sum1, wh1, mul0);
    const V sum2 = l2 + r2;
    const V mul2 = MulAdd(sum2, wh2, mul1);
    const V sum3 = l3 + r3;
    const V mul3 = MulAdd(sum3, wh3, mul2);
    return mul3;
  }
};  // namespace HWY_NAMESPACE

}  // namespace strategy

// Single entry point for convolution.
// "Strategy" (Direct*/Separable*) decides kernel size and how to evaluate it.
template <class Strategy>
class ConvolveT {
  static constexpr int64_t kRadius = Strategy::kRadius;
  using Simd = HWY_CAPPED(float, 16);

 public:
  static size_t MinWidth() {
#if HWY_TARGET == HWY_SCALAR
    // First/Last use mirrored loads of up to +/- kRadius.
    return 2 * kRadius;
#else
    return Lanes(Simd()) + kRadius;
#endif
  }

  // "Image" is ImageF or Image3F.
  template <class Image, class Weights>
  static void Run(const Image& in, const Rect& rect, const Weights& weights,
                  ThreadPool* pool, Image* out) {
    PROFILER_ZONE("ConvolveT::Run");
    JXL_CHECK(SameSize(rect, *out));
    JXL_CHECK(rect.xsize() >= MinWidth());

    static_assert(int64_t(kRadius) <= 3,
                  "Must handle [0, kRadius) and >= kRadius");
    switch (rect.xsize() % Lanes(Simd())) {
      case 0:
        return RunRows<0>(in, rect, weights, pool, out);
      case 1:
        return RunRows<1>(in, rect, weights, pool, out);
      case 2:
        return RunRows<2>(in, rect, weights, pool, out);
      default:
        return RunRows<3>(in, rect, weights, pool, out);
    }
  }

 private:
  template <size_t kSizeModN, class WrapRow, class Weights>
  static JXL_INLINE void RunRow(const float* JXL_RESTRICT in,
                                const size_t xsize, const int64_t stride,
                                const WrapRow& wrap_row, const Weights& weights,
                                float* JXL_RESTRICT out) {
    Strategy::template ConvolveRow<kSizeModN>(in, xsize, stride, wrap_row,
                                              weights, out);
  }

  template <size_t kSizeModN, class Weights>
  static JXL_INLINE void RunBorderRows(const ImageF& in, const Rect& rect,
                                       const int64_t ybegin, const int64_t yend,
                                       const Weights& weights, ImageF* out) {
    const int64_t stride = in.PixelsPerRow();
    const WrapRowMirror wrap_row(in, rect.ysize());
    for (int64_t y = ybegin; y < yend; ++y) {
      RunRow<kSizeModN>(rect.ConstRow(in, y), rect.xsize(), stride, wrap_row,
                        weights, out->Row(y));
    }
  }

  // Image3F.
  template <size_t kSizeModN, class Weights>
  static JXL_INLINE void RunBorderRows(const Image3F& in, const Rect& rect,
                                       const int64_t ybegin, const int64_t yend,
                                       const Weights& weights, Image3F* out) {
    const int64_t stride = in.PixelsPerRow();
    for (int64_t y = ybegin; y < yend; ++y) {
      for (size_t c = 0; c < 3; ++c) {
        const WrapRowMirror wrap_row(in.Plane(c), rect.ysize());
        RunRow<kSizeModN>(rect.ConstPlaneRow(in, c, y), rect.xsize(), stride,
                          wrap_row, weights, out->PlaneRow(c, y));
      }
    }
  }

  template <size_t kSizeModN, class Weights>
  static JXL_INLINE void RunInteriorRows(const ImageF& in, const Rect& rect,
                                         const int64_t ybegin,
                                         const int64_t yend,
                                         const Weights& weights,
                                         ThreadPool* pool, ImageF* out) {
    const int64_t stride = in.PixelsPerRow();
    RunOnPool(
        pool, ybegin, yend, ThreadPool::SkipInit(),
        [&](const int y, int /*thread*/) HWY_ATTR {
          RunRow<kSizeModN>(rect.ConstRow(in, y), rect.xsize(), stride,
                            WrapRowUnchanged(), weights, out->Row(y));
        },
        "Convolve");
  }

  // Image3F.
  template <size_t kSizeModN, class Weights>
  static JXL_INLINE void RunInteriorRows(const Image3F& in, const Rect& rect,
                                         const int64_t ybegin,
                                         const int64_t yend,
                                         const Weights& weights,
                                         ThreadPool* pool, Image3F* out) {
    const int64_t stride = in.PixelsPerRow();
    RunOnPool(
        pool, ybegin, yend, ThreadPool::SkipInit(),
        [&](const int y, int /*thread*/) HWY_ATTR {
          for (size_t c = 0; c < 3; ++c) {
            RunRow<kSizeModN>(rect.ConstPlaneRow(in, c, y), rect.xsize(),
                              stride, WrapRowUnchanged(), weights,
                              out->PlaneRow(c, y));
          }
        },
        "Convolve3");
  }

  template <size_t kSizeModN, class Image, class Weights>
  static JXL_INLINE void RunRows(const Image& in, const Rect& rect,
                                 const Weights& weights, ThreadPool* pool,
                                 Image* out) {
    const int64_t ysize = rect.ysize();
    RunBorderRows<kSizeModN>(in, rect, 0, std::min(int64_t(kRadius), ysize),
                             weights, out);
    if (ysize > 2 * int64_t(kRadius)) {
      RunInteriorRows<kSizeModN>(in, rect, int64_t(kRadius),
                                 ysize - int64_t(kRadius), weights, pool, out);
    }
    if (ysize > int64_t(kRadius)) {
      RunBorderRows<kSizeModN>(in, rect, ysize - int64_t(kRadius), ysize,
                               weights, out);
    }
  }
};

void Symmetric3(const ImageF& in, const Rect& rect,
                const WeightsSymmetric3& weights, ThreadPool* pool,
                ImageF* out) {
  using Conv = ConvolveT<strategy::Symmetric3>;
  if (rect.xsize() >= Conv::MinWidth()) {
    return Conv::Run(in, rect, weights, pool, out);
  }

  return SlowSymmetric3(in, rect, weights, pool, out);
}

// Symmetric5 is implemented above without ConvolveT.

void Separable5(const ImageF& in, const Rect& rect,
                const WeightsSeparable5& weights, ThreadPool* pool,
                ImageF* out) {
  using Conv = ConvolveT<strategy::Separable5>;
  if (rect.xsize() >= Conv::MinWidth()) {
    return Conv::Run(in, rect, weights, pool, out);
  }

  return SlowSeparable5(in, rect, weights, pool, out);
}
void Separable5_3(const Image3F& in, const Rect& rect,
                  const WeightsSeparable5& weights, ThreadPool* pool,
                  Image3F* out) {
  using Conv = ConvolveT<strategy::Separable5>;
  if (rect.xsize() >= Conv::MinWidth()) {
    return Conv::Run(in, rect, weights, pool, out);
  }

  return SlowSeparable5(in, rect, weights, pool, out);
}

void Separable7(const ImageF& in, const Rect& rect,
                const WeightsSeparable7& weights, ThreadPool* pool,
                ImageF* out) {
  using Conv = ConvolveT<strategy::Separable7>;
  if (rect.xsize() >= Conv::MinWidth()) {
    return Conv::Run(in, rect, weights, pool, out);
  }

  return SlowSeparable7(in, rect, weights, pool, out);
}
void Separable7_3(const Image3F& in, const Rect& rect,
                  const WeightsSeparable7& weights, ThreadPool* pool,
                  Image3F* out) {
  using Conv = ConvolveT<strategy::Separable7>;
  if (rect.xsize() >= Conv::MinWidth()) {
    return Conv::Run(in, rect, weights, pool, out);
  }

  return SlowSeparable7(in, rect, weights, pool, out);
}

// Semi-vectorized (interior pixels Fonly); called directly like slow::, unlike
// the fully vectorized strategies below.
void Symmetric5(const ImageF& in, const Rect& rect,
                const WeightsSymmetric5& weights, ThreadPool* pool,
                ImageF* JXL_RESTRICT out) {
  PROFILER_FUNC;

  const size_t ysize = rect.ysize();
  RunOnPool(
      pool, 0, static_cast<uint32_t>(ysize), ThreadPool::SkipInit(),
      [&](const int task, int /*thread*/) {
        const int64_t iy = task;

        if (iy < 2 || iy >= static_cast<ssize_t>(ysize) - 2) {
          Symmetric5BorderRow(in, rect, iy, weights, out->Row(iy));
        } else {
          Symmetric5Row<WrapUnchanged>(in, rect, iy, weights, out->Row(iy));
        }
      },
      "Symmetric5x5Convolution");
}

void Symmetric5_3(const Image3F& in, const Rect& rect,
                  const WeightsSymmetric5& weights, ThreadPool* pool,
                  Image3F* JXL_RESTRICT out) {
  PROFILER_FUNC;

  const size_t ysize = rect.ysize();
  RunOnPool(
      pool, 0, static_cast<uint32_t>(ysize), ThreadPool::SkipInit(),
      [&](const int task, int /*thread*/) {
        const size_t iy = task;

        if (iy < 2 || iy >= ysize - 2) {
          for (size_t c = 0; c < 3; ++c) {
            Symmetric5BorderRow(in.Plane(c), rect, iy, weights,
                                out->PlaneRow(c, iy));
          }
        } else {
          for (size_t c = 0; c < 3; ++c) {
            Symmetric5Row<WrapUnchanged>(in.Plane(c), rect, iy, weights,
                                         out->PlaneRow(c, iy));
          }
        }
      },
      "Symmetric5x5Convolution3");
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace jxl
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace jxl {

HWY_EXPORT(Symmetric3);
void Symmetric3(const ImageF& in, const Rect& rect,
                const WeightsSymmetric3& weights, ThreadPool* pool,
                ImageF* out) {
  return HWY_DYNAMIC_DISPATCH(Symmetric3)(in, rect, weights, pool, out);
}

HWY_EXPORT(Symmetric5);
void Symmetric5(const ImageF& in, const Rect& rect,
                const WeightsSymmetric5& weights, ThreadPool* pool,
                ImageF* JXL_RESTRICT out) {
  return HWY_DYNAMIC_DISPATCH(Symmetric5)(in, rect, weights, pool, out);
}

HWY_EXPORT(Symmetric5_3);
void Symmetric5_3(const Image3F& in, const Rect& rect,
                  const WeightsSymmetric5& weights, ThreadPool* pool,
                  Image3F* JXL_RESTRICT out) {
  return HWY_DYNAMIC_DISPATCH(Symmetric5_3)(in, rect, weights, pool, out);
}

HWY_EXPORT(Separable5);
void Separable5(const ImageF& in, const Rect& rect,
                const WeightsSeparable5& weights, ThreadPool* pool,
                ImageF* out) {
  return HWY_DYNAMIC_DISPATCH(Separable5)(in, rect, weights, pool, out);
}

HWY_EXPORT(Separable5_3);
void Separable5_3(const Image3F& in, const Rect& rect,
                  const WeightsSeparable5& weights, ThreadPool* pool,
                  Image3F* out) {
  return HWY_DYNAMIC_DISPATCH(Separable5_3)(in, rect, weights, pool, out);
}

HWY_EXPORT(Separable7);
void Separable7(const ImageF& in, const Rect& rect,
                const WeightsSeparable7& weights, ThreadPool* pool,
                ImageF* out) {
  return HWY_DYNAMIC_DISPATCH(Separable7)(in, rect, weights, pool, out);
}

HWY_EXPORT(Separable7_3);
void Separable7_3(const Image3F& in, const Rect& rect,
                  const WeightsSeparable7& weights, ThreadPool* pool,
                  Image3F* out) {
  return HWY_DYNAMIC_DISPATCH(Separable7_3)(in, rect, weights, pool, out);
}

//------------------------------------------------------------------------------
// Kernels

// Concentrates energy in low-frequency components (e.g. for antialiasing).
const WeightsSymmetric3& WeightsSymmetric3Lowpass() {
  // Computed by research/convolve_weights.py's cubic spline approximations of
  // prolate spheroidal wave functions.
  constexpr float w0 = 0.36208932f;
  constexpr float w1 = 0.12820096f;
  constexpr float w2 = 0.03127668f;
  static constexpr WeightsSymmetric3 weights = {
      {HWY_REP4(w0)}, {HWY_REP4(w1)}, {HWY_REP4(w2)}};
  return weights;
}

const WeightsSeparable5& WeightsSeparable5Lowpass() {
  constexpr float w0 = 0.41714928f;
  constexpr float w1 = 0.25539268f;
  constexpr float w2 = 0.03603267f;
  static constexpr WeightsSeparable5 weights = {
      {HWY_REP4(w0), HWY_REP4(w1), HWY_REP4(w2)},
      {HWY_REP4(w0), HWY_REP4(w1), HWY_REP4(w2)}};
  return weights;
}

const WeightsSymmetric5& WeightsSymmetric5Lowpass() {
  static constexpr WeightsSymmetric5 weights = {
      {HWY_REP4(0.1740135f)}, {HWY_REP4(0.1065369f)}, {HWY_REP4(0.0150310f)},
      {HWY_REP4(0.0652254f)}, {HWY_REP4(0.0012984f)}, {HWY_REP4(0.0092025f)}};
  return weights;
}

const WeightsSeparable5& WeightsSeparable5Gaussian1() {
  constexpr float w0 = 0.38774f;
  constexpr float w1 = 0.24477f;
  constexpr float w2 = 0.06136f;
  static constexpr WeightsSeparable5 weights = {
      {HWY_REP4(w0), HWY_REP4(w1), HWY_REP4(w2)},
      {HWY_REP4(w0), HWY_REP4(w1), HWY_REP4(w2)}};
  return weights;
}

const WeightsSeparable5& WeightsSeparable5Gaussian2() {
  constexpr float w0 = 0.250301f;
  constexpr float w1 = 0.221461f;
  constexpr float w2 = 0.153388f;
  static constexpr WeightsSeparable5 weights = {
      {HWY_REP4(w0), HWY_REP4(w1), HWY_REP4(w2)},
      {HWY_REP4(w0), HWY_REP4(w1), HWY_REP4(w2)}};
  return weights;
}

//------------------------------------------------------------------------------
// Slow

namespace {

template <class WrapX, class WrapY>
float SlowSymmetric3Pixel(const ImageF& in, const int64_t ix, const int64_t iy,
                          const int64_t xsize, const int64_t ysize,
                          const WeightsSymmetric3& weights) {
  float sum = 0.0f;

  // ix: image; kx: kernel
  for (int64_t ky = -1; ky <= 1; ky++) {
    const int64_t y = WrapY()(iy + ky, ysize);
    const float* JXL_RESTRICT row_in = in.ConstRow(static_cast<size_t>(y));

    const float wc = ky == 0 ? weights.c[0] : weights.r[0];
    const float wlr = ky == 0 ? weights.r[0] : weights.d[0];

    const int64_t xm1 = WrapX()(ix - 1, xsize);
    const int64_t xp1 = WrapX()(ix + 1, xsize);
    sum += row_in[ix] * wc + (row_in[xm1] + row_in[xp1]) * wlr;
  }
  return sum;
}

template <class WrapY>
void SlowSymmetric3Row(const ImageF& in, const int64_t iy, const int64_t xsize,
                       const int64_t ysize, const WeightsSymmetric3& weights,
                       float* JXL_RESTRICT row_out) {
  row_out[0] =
      SlowSymmetric3Pixel<WrapMirror, WrapY>(in, 0, iy, xsize, ysize, weights);
  for (int64_t ix = 1; ix < xsize - 1; ix++) {
    row_out[ix] = SlowSymmetric3Pixel<WrapUnchanged, WrapY>(in, ix, iy, xsize,
                                                            ysize, weights);
  }
  {
    const int64_t ix = xsize - 1;
    row_out[ix] = SlowSymmetric3Pixel<WrapMirror, WrapY>(in, ix, iy, xsize,
                                                         ysize, weights);
  }
}

}  // namespace

void SlowSymmetric3(const ImageF& in, const Rect& rect,
                    const WeightsSymmetric3& weights, ThreadPool* pool,
                    ImageF* JXL_RESTRICT out) {
  PROFILER_FUNC;

  const int64_t xsize = static_cast<int64_t>(rect.xsize());
  const int64_t ysize = static_cast<int64_t>(rect.ysize());
  const int64_t kRadius = 1;

  RunOnPool(
      pool, 0, static_cast<uint32_t>(ysize), ThreadPool::SkipInit(),
      [&](const int task, int /*thread*/) {
        const int64_t iy = task;
        float* JXL_RESTRICT out_row = out->Row(static_cast<size_t>(iy));

        if (iy < kRadius || iy >= ysize - kRadius) {
          SlowSymmetric3Row<WrapMirror>(in, iy, xsize, ysize, weights, out_row);
        } else {
          SlowSymmetric3Row<WrapUnchanged>(in, iy, xsize, ysize, weights,
                                           out_row);
        }
      },
      "SlowSymmetric3");
}

void SlowSymmetric3(const Image3F& in, const Rect& rect,
                    const WeightsSymmetric3& weights, ThreadPool* pool,
                    Image3F* JXL_RESTRICT out) {
  PROFILER_FUNC;

  const int64_t xsize = static_cast<int64_t>(rect.xsize());
  const int64_t ysize = static_cast<int64_t>(rect.ysize());
  const int64_t kRadius = 1;

  RunOnPool(
      pool, 0, static_cast<uint32_t>(ysize), ThreadPool::SkipInit(),
      [&](const int task, int /*thread*/) {
        const int64_t iy = task;
        const size_t oy = static_cast<size_t>(iy);

        if (iy < kRadius || iy >= ysize - kRadius) {
          for (size_t c = 0; c < 3; ++c) {
            SlowSymmetric3Row<WrapMirror>(in.Plane(c), iy, xsize, ysize,
                                          weights, out->PlaneRow(c, oy));
          }
        } else {
          for (size_t c = 0; c < 3; ++c) {
            SlowSymmetric3Row<WrapUnchanged>(in.Plane(c), iy, xsize, ysize,
                                             weights, out->PlaneRow(c, oy));
          }
        }
      },
      "SlowSymmetric3");
}

namespace {

// Separable kernels, any radius.
float SlowSeparablePixel(const ImageF& in, const Rect& rect, const int64_t x,
                         const int64_t y, const int64_t radius,
                         const float* JXL_RESTRICT horz_weights,
                         const float* JXL_RESTRICT vert_weights) {
  const size_t xsize = rect.xsize();
  const size_t ysize = rect.ysize();
  const WrapMirror wrap;

  float mul = 0.0f;
  for (int dy = -radius; dy <= radius; ++dy) {
    const float wy = vert_weights[std::abs(dy) * 4];
    const size_t sy = wrap(y + dy, ysize);
    JXL_CHECK(sy < ysize);
    const float* const JXL_RESTRICT row = rect.ConstRow(in, sy);
    for (int dx = -radius; dx <= radius; ++dx) {
      const float wx = horz_weights[std::abs(dx) * 4];
      const size_t sx = wrap(x + dx, xsize);
      JXL_CHECK(sx < xsize);
      mul += row[sx] * wx * wy;
    }
  }
  return mul;
}

}  // namespace

void SlowSeparable5(const ImageF& in, const Rect& rect,
                    const WeightsSeparable5& weights, ThreadPool* pool,
                    ImageF* out) {
  PROFILER_FUNC;
  const float* horz_weights = &weights.horz[0];
  const float* vert_weights = &weights.vert[0];

  const size_t ysize = rect.ysize();
  RunOnPool(
      pool, 0, static_cast<uint32_t>(ysize), ThreadPool::SkipInit(),
      [&](const int task, int /*thread*/) {
        const int64_t y = task;

        float* const JXL_RESTRICT row_out = out->Row(y);
        for (size_t x = 0; x < rect.xsize(); ++x) {
          row_out[x] = SlowSeparablePixel(in, rect, x, y, /*radius=*/2,
                                          horz_weights, vert_weights);
        }
      },
      "SlowSeparable5");
}

void SlowSeparable5(const Image3F& in, const Rect& rect,
                    const WeightsSeparable5& weights, ThreadPool* pool,
                    Image3F* out) {
  for (size_t c = 0; c < 3; ++c) {
    SlowSeparable5(in.Plane(c), rect, weights, pool, &out->Plane(c));
  }
}

void SlowSeparable7(const ImageF& in, const Rect& rect,
                    const WeightsSeparable7& weights, ThreadPool* pool,
                    ImageF* out) {
  PROFILER_FUNC;
  const float* horz_weights = &weights.horz[0];
  const float* vert_weights = &weights.vert[0];

  const size_t ysize = rect.ysize();
  RunOnPool(
      pool, 0, static_cast<uint32_t>(ysize), ThreadPool::SkipInit(),
      [&](const int task, int /*thread*/) {
        const int64_t y = task;

        float* const JXL_RESTRICT row_out = out->Row(y);
        for (size_t x = 0; x < rect.xsize(); ++x) {
          row_out[x] = SlowSeparablePixel(in, rect, x, y, /*radius=*/3,
                                          horz_weights, vert_weights);
        }
      },
      "SlowSeparable7");
}

void SlowSeparable7(const Image3F& in, const Rect& rect,
                    const WeightsSeparable7& weights, ThreadPool* pool,
                    Image3F* out) {
  for (size_t c = 0; c < 3; ++c) {
    SlowSeparable7(in.Plane(c), rect, weights, pool, &out->Plane(c));
  }
}

void SlowLaplacian5(const ImageF& in, const Rect& rect, ThreadPool* pool,
                    ImageF* out) {
  PROFILER_FUNC;
  JXL_CHECK(SameSize(rect, *out));

  const size_t xsize = rect.xsize();
  const size_t ysize = rect.ysize();
  const WrapMirror wrap;

  RunOnPool(
      pool, 0, static_cast<uint32_t>(ysize), ThreadPool::SkipInit(),
      [&](const int task, int /*thread*/) {
        const int64_t y = task;

        const float* const JXL_RESTRICT row_t =
            rect.ConstRow(in, wrap(y - 2, ysize));
        const float* const JXL_RESTRICT row_m = rect.ConstRow(in, y);
        const float* const JXL_RESTRICT row_b =
            rect.ConstRow(in, wrap(y + 2, ysize));
        float* const JXL_RESTRICT row_out = out->Row(y);

        for (int64_t x = 0; static_cast<size_t>(x) < xsize; ++x) {
          const int64_t xm2 = wrap(x - 2, xsize);
          const int64_t xp2 = wrap(x + 2, xsize);
          float r = 0.0f;
          r += /*               */ 1.0f * row_t[x];
          r += 1.0f * row_m[xm2] - 4.0f * row_m[x] + 1.0f * row_m[xp2];
          r += /*               */ 1.0f * row_b[x];
          row_out[x] = r;
        }
      },
      "SlowLaplacian5");
}

void SlowLaplacian5(const Image3F& in, const Rect& rect, ThreadPool* pool,
                    Image3F* out) {
  for (size_t c = 0; c < 3; ++c) {
    SlowLaplacian5(in.Plane(c), rect, pool, &out->Plane(c));
  }
}

}  // namespace jxl
#endif  // HWY_ONCE
