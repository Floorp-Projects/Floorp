// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/dec_upsample.h"

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "lib/jxl/dec_upsample.cc"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

#include "lib/jxl/base/profiler.h"
#include "lib/jxl/image_ops.h"
#include "lib/jxl/sanitizers.h"

HWY_BEFORE_NAMESPACE();
namespace jxl {
namespace HWY_NAMESPACE {
namespace {

void InitKernel(const float* weights, CacheAlignedUniquePtr* kernel_storage,
                size_t N, size_t x_repeat) {
  const size_t NX = N * x_repeat;
  const size_t N2 = N / 2;
  HWY_FULL(float) df;
  const size_t V = Lanes(df);
  const size_t num_kernels = N * NX;

  constexpr const size_t M = 2 * Upsampler::filter_radius() + 1;
  const size_t MX = M + x_repeat - 1;
  const size_t num_coeffs = M * MX;

  // Pad kernel slices to vector size.
  const size_t stride = RoundUpTo(num_kernels, V);
  *kernel_storage = AllocateArray(stride * sizeof(float) * num_coeffs);
  float* kernels = reinterpret_cast<float*>(kernel_storage->get());
  memset(kernels, 0, stride * sizeof(float) * num_coeffs);

  for (size_t offset = 0; offset < num_coeffs; ++offset) {
    size_t iy = offset / MX;
    size_t ix = offset % MX;
    for (size_t kernel = 0; kernel < num_kernels; ++kernel) {
      size_t ky = kernel / NX;
      size_t kx_ = kernel % NX;
      size_t kx = kx_ % N;
      size_t shift = kx_ / N;
      if ((ix < shift) || (ix - shift >= M)) continue;  // 0 weight from memset.
      // Only weights for top-left 1 / 4 of kernels are specified; other 3 / 4
      // kernels are produced by vertical and horizontal mirroring.
      size_t j = (ky < N2) ? (iy + M * ky) : ((M - 1 - iy) + M * (N - 1 - ky));
      size_t i = (kx < N2) ? (ix - shift + M * kx)
                           : ((M - 1 - (ix - shift)) + M * (N - 1 - kx));
      // (y, x) = sorted(i, j)
      // the matrix built of kernel matrices as blocks is symmetric.
      size_t y = std::min(i, j);
      size_t x = std::max(i, j);
      // Take the weight from "triangle" coordinates.
      float weight =
          weights[M * N2 * y - y * (static_cast<ssize_t>(y) - 1) / 2 + x - y];
      kernels[offset * stride + kernel] = weight;
    }
  }
}

template <size_t N, size_t x_repeat>
void Upsample(const ImageF& src, const Rect& src_rect, ImageF* dst,
              const Rect& dst_rect, const float* kernels,
              ssize_t image_y_offset, size_t image_ysize, float* arena) {
  constexpr const size_t M = 2 * Upsampler::filter_radius() + 1;
  constexpr const size_t M2 = M / 2;
  JXL_DASSERT(src_rect.x0() >= M2);
  const size_t src_x_limit = src_rect.x0() + src_rect.xsize() + M2;
  JXL_DASSERT(src_x_limit <= src.xsize());
  JXL_ASSERT(DivCeil(dst_rect.xsize(), N) <= src_rect.xsize());
  // TODO(eustas): add proper (src|dst) ysize check that accounts for mirroring.

  constexpr const size_t MX = M + x_repeat - 1;
  constexpr const size_t num_coeffs = M * MX;

  constexpr const size_t NX = N * x_repeat;

  HWY_FULL(float) df;
  const size_t V = Lanes(df);
  const size_t num_kernels = N * NX;
  const size_t stride = RoundUpTo(num_kernels, V);

  const size_t rsx = DivCeil(dst_rect.xsize(), N);
  const size_t dsx = rsx + 2 * M2;
  // Round-down to complete vectors.
  const size_t dsx_v = V * (dsx / V);

  float* JXL_RESTRICT in = arena;
  arena += RoundUpTo(num_coeffs, V);
  float* JXL_RESTRICT out = arena;
  arena += stride;
  float* JXL_RESTRICT raw_min_row = arena;
  arena += RoundUpTo(dsx + V, V);
  float* JXL_RESTRICT raw_max_row = arena;
  arena += RoundUpTo(dsx + V, V);
  float* JXL_RESTRICT min_row = arena;
  arena += RoundUpTo(rsx * N + V, V);
  float* JXL_RESTRICT max_row = arena;
  arena += RoundUpTo(rsx * N + V, V);

  memset(raw_min_row + dsx_v, 0, sizeof(float) * (V + dsx - dsx_v));
  memset(raw_max_row + dsx_v, 0, sizeof(float) * (V + dsx - dsx_v));
  memset(min_row + dst_rect.xsize(), 0, sizeof(float) * V);
  memset(max_row + dst_rect.xsize(), 0, sizeof(float) * V);

  // For min/max reduction.
  const size_t span_tail_len = M % V;
  const bool has_span_tail = (span_tail_len != 0);
  JXL_ASSERT(has_span_tail || V <= M);
  const size_t span_start = has_span_tail ? 0 : V;
  const size_t span_tail_start = M - span_tail_len;
  const auto span_tail_mask = Iota(df, 0) < Set(df, span_tail_len);

  // sx and sy correspond to offset in source image.
  // x and y correspond to top-left pixel offset in upsampled output image.
  for (size_t y = 0; y < dst_rect.ysize(); y += N) {
    const float* src_rows[M];
    const size_t sy = y / N;
    const ssize_t top = static_cast<ssize_t>(sy + src_rect.y0() - M2);
    for (size_t iy = 0; iy < M; iy++) {
      const ssize_t image_y = top + iy + image_y_offset;
      src_rows[iy] = src.Row(Mirror(image_y, image_ysize) - image_y_offset);
    }
    const size_t sx0 = src_rect.x0() - M2;
    for (size_t sx = 0; sx < dsx_v; sx += V) {
      static_assert(M == 5, "Filter diameter is expected to be 5");
      const auto r0 = LoadU(df, src_rows[0] + sx0 + sx);
      const auto r1 = LoadU(df, src_rows[1] + sx0 + sx);
      const auto r2 = LoadU(df, src_rows[2] + sx0 + sx);
      const auto r3 = LoadU(df, src_rows[3] + sx0 + sx);
      const auto r4 = LoadU(df, src_rows[4] + sx0 + sx);
      const auto min0 = Min(r0, r1);
      const auto max0 = Max(r0, r1);
      const auto min1 = Min(r2, r3);
      const auto max1 = Max(r2, r3);
      const auto min2 = Min(min0, r4);
      const auto max2 = Max(max0, r4);
      Store(Min(min1, min2), df, raw_min_row + sx);
      Store(Max(max1, max2), df, raw_max_row + sx);
    }
    for (size_t sx = dsx_v; sx < dsx; sx++) {
      static_assert(M == 5, "Filter diameter is expected to be 5");
      const auto r0 = src_rows[0][sx0 + sx];
      const auto r1 = src_rows[1][sx0 + sx];
      const auto r2 = src_rows[2][sx0 + sx];
      const auto r3 = src_rows[3][sx0 + sx];
      const auto r4 = src_rows[4][sx0 + sx];
      const auto min0 = std::min(r0, r1);
      const auto max0 = std::max(r0, r1);
      const auto min1 = std::min(r2, r3);
      const auto max1 = std::max(r2, r3);
      const auto min2 = std::min(min0, r4);
      const auto max2 = std::max(max0, r4);
      raw_min_row[sx] = std::min(min1, min2);
      raw_max_row[sx] = std::max(max1, max2);
    }

    for (size_t sx = 0; sx < rsx; sx++) {
      decltype(Zero(df)) min, max;
      if (has_span_tail) {
        auto dummy = Set(df, raw_min_row[sx]);
        min = IfThenElse(span_tail_mask,
                         LoadU(df, raw_min_row + sx + span_tail_start), dummy);
        max = IfThenElse(span_tail_mask,
                         LoadU(df, raw_max_row + sx + span_tail_start), dummy);
      } else {
        min = LoadU(df, raw_min_row + sx);
        max = LoadU(df, raw_max_row + sx);
      }
      for (size_t fx = span_start; fx < span_tail_start; fx += V) {
        min = Min(LoadU(df, raw_min_row + sx + fx), min);
        max = Max(LoadU(df, raw_max_row + sx + fx), max);
      }
      min = MinOfLanes(df, min);
      max = MaxOfLanes(df, max);
      for (size_t lx = 0; lx < N; lx += V) {
        StoreU(min, df, min_row + N * sx + lx);
        StoreU(max, df, max_row + N * sx + lx);
      }
    }

    for (size_t x = 0; x < dst_rect.xsize(); x += NX) {
      const size_t sx = x / N;
      const size_t xbase = sx + sx0;
      // Copy input pixels for "linearization".
      for (size_t iy = 0; iy < M; iy++) {
        memcpy(in + MX * iy, src_rows[iy] + xbase, MX * sizeof(float));
      }
      if (x_repeat > 1) {
        // Even if filter coeffs contain 0 at "undefined" values, the result
        // might be undefined, because NaN will poison the sum.
        if (JXL_UNLIKELY(xbase + MX > src_x_limit)) {
          for (size_t iy = 0; iy < M; iy++) {
            for (size_t ix = src_x_limit - xbase; ix < MX; ++ix) {
              in[MX * iy + ix] = 0.0f;
            }
          }
        }
      }
      constexpr size_t U = 4;  // Unroll factor.
      constexpr size_t tail = num_coeffs & ~(U - 1);
      constexpr size_t tail_length = num_coeffs - tail;
      for (size_t kernel_idx = 0; kernel_idx < num_kernels; kernel_idx += V) {
        const float* JXL_RESTRICT kernel_base = kernels + kernel_idx;
        decltype(Zero(df)) results[U];
        for (size_t i = 0; i < U; i++) {
          results[i] = Set(df, in[i]) * Load(df, kernel_base + i * stride);
        }
        for (size_t i = U; i < tail; i += U) {
          for (size_t j = 0; j < U; ++j) {
            results[j] =
                MulAdd(Set(df, in[i + j]),
                       Load(df, kernel_base + (i + j) * stride), results[j]);
          }
        }
        for (size_t i = 0; i < tail_length; ++i) {
          results[i] =
              MulAdd(Set(df, in[tail + i]),
                     Load(df, kernel_base + (tail + i) * stride), results[i]);
        }
        auto result = results[0];
        for (size_t i = 1; i < U; ++i) result += results[i];
        Store(result, df, out + kernel_idx);
      }
      const size_t oy_max = std::min<size_t>(dst_rect.ysize(), y + N);
      const size_t ox_max = std::min<size_t>(dst_rect.xsize(), x + NX);
      const size_t copy_len = ox_max - x;
      const size_t copy_last = RoundUpTo(copy_len, V);
      if (JXL_LIKELY(x + copy_last <= dst_rect.xsize())) {
        for (size_t dx = 0; dx < copy_len; dx += V) {
          auto min = LoadU(df, min_row + x + dx);
          auto max = LoadU(df, max_row + x + dx);
          float* pixels = out;
          for (size_t oy = sy * N; oy < oy_max; ++oy, pixels += NX) {
            StoreU(Clamp(LoadU(df, pixels + dx), min, max), df,
                   dst_rect.Row(dst, oy) + x + dx);
          }
        }
      } else {
        for (size_t dx = 0; dx < copy_len; dx++) {
          auto min = min_row[x + dx];
          auto max = max_row[x + dx];
          float* pixels = out;
          for (size_t oy = sy * N; oy < oy_max; ++oy, pixels += NX) {
            dst_rect.Row(dst, oy)[x + dx] = Clamp1(pixels[dx], min, max);
          }
        }
      }
    }
  }
}

}  // namespace

void UpsampleRect(size_t upsampling, const float* kernels, const ImageF& src,
                  const Rect& src_rect, ImageF* dst, const Rect& dst_rect,
                  ssize_t image_y_offset, size_t image_ysize, float* arena,
                  size_t x_repeat) {
  if (upsampling == 1) return;
  if (upsampling == 2) {
    if (x_repeat == 1) {
      Upsample</*N=*/2, /*x_repeat=*/1>(src, src_rect, dst, dst_rect, kernels,
                                        image_y_offset, image_ysize, arena);
    } else if (x_repeat == 2) {
      Upsample</*N=*/2, /*x_repeat=*/2>(src, src_rect, dst, dst_rect, kernels,
                                        image_y_offset, image_ysize, arena);
    } else if (x_repeat == 4) {
      Upsample</*N=*/2, /*x_repeat=*/4>(src, src_rect, dst, dst_rect, kernels,
                                        image_y_offset, image_ysize, arena);
    } else {
      JXL_ABORT("Not implemented");
    }
  } else if (upsampling == 4) {
    JXL_ASSERT(x_repeat == 1);
    Upsample</*N=*/4, /*x_repeat=*/1>(src, src_rect, dst, dst_rect, kernels,
                                      image_y_offset, image_ysize, arena);
  } else if (upsampling == 8) {
    JXL_ASSERT(x_repeat == 1);
    Upsample</*N=*/8, /*x_repeat=*/1>(src, src_rect, dst, dst_rect, kernels,
                                      image_y_offset, image_ysize, arena);
  } else {
    JXL_ABORT("Not implemented");
  }
}

size_t NumLanes() {
  HWY_FULL(float) df;
  return Lanes(df);
}

void Init(size_t upsampling, CacheAlignedUniquePtr* kernel_storage,
          const CustomTransformData& data, size_t x_repeat) {
  if ((upsampling & (upsampling - 1)) != 0 ||
      upsampling > Upsampler::max_upsampling()) {
    JXL_ABORT("Invalid upsample");
  }
  if ((x_repeat & (x_repeat - 1)) != 0 ||
      x_repeat > Upsampler::max_x_repeat()) {
    JXL_ABORT("Invalid x_repeat");
  }

  // No-op upsampling.
  if (upsampling == 1) return;
  const float* weights = (upsampling == 2)   ? data.upsampling2_weights
                         : (upsampling == 4) ? data.upsampling4_weights
                                             : data.upsampling8_weights;
  InitKernel(weights, kernel_storage, upsampling, x_repeat);
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace jxl
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace jxl {

namespace {
HWY_EXPORT(NumLanes);
HWY_EXPORT(Init);
HWY_EXPORT(UpsampleRect);
}  // namespace

void Upsampler::Init(size_t upsampling, const CustomTransformData& data) {
  upsampling_ = upsampling;
  size_t V = HWY_DYNAMIC_DISPATCH(NumLanes)();
  x_repeat_ = 1;
  if (upsampling_ == 2) {
    // 2 * 2 = 4 kernels; repeat cell, if there is more lanes available
    if (V >= 8) x_repeat_ = 2;
    if (V >= 16) x_repeat_ = 4;
  }
  HWY_DYNAMIC_DISPATCH(Init)(upsampling, &kernel_storage_, data, x_repeat_);
}

size_t Upsampler::GetArenaSize(size_t max_dst_xsize) {
  size_t V = HWY_DYNAMIC_DISPATCH(NumLanes)();
  constexpr const size_t M2 = Upsampler::filter_radius();
  constexpr const size_t M = 2 * M2 + 1;
  constexpr size_t X = max_x_repeat();
  constexpr const size_t MX = M + X - 1;
  constexpr const size_t N = max_upsampling();
  // TODO(eustas): raw_(min|max)_row and (min|max)_row could overlap almost
  // completely.
  return RoundUpTo(N * N * X, V) + RoundUpTo(M * MX, V) +
         2 * RoundUpTo(DivCeil(max_dst_xsize, 8) * 4 + 2 * M2 + V, V) +
         2 * RoundUpTo(max_dst_xsize + V, V);
}

void Upsampler::UpsampleRect(const ImageF& src, const Rect& src_rect,
                             ImageF* dst, const Rect& dst_rect,
                             ssize_t image_y_offset, size_t image_ysize,
                             float* arena) const {
  JXL_CHECK(arena);
  JXL_CHECK_IMAGE_INITIALIZED(src, src_rect);
  HWY_DYNAMIC_DISPATCH(UpsampleRect)
  (upsampling_, reinterpret_cast<float*>(kernel_storage_.get()), src, src_rect,
   dst, dst_rect, image_y_offset, image_ysize, arena, x_repeat_);
  JXL_CHECK_IMAGE_INITIALIZED(*dst, dst_rect);
}

void Upsampler::UpsampleRect(const Image3F& src, const Rect& src_rect,
                             Image3F* dst, const Rect& dst_rect,
                             ssize_t image_y_offset, size_t image_ysize,
                             float* arena) const {
  PROFILER_FUNC;
  JXL_CHECK_IMAGE_INITIALIZED(src, src_rect);
  for (size_t c = 0; c < 3; c++) {
    UpsampleRect(src.Plane(c), src_rect, &dst->Plane(c), dst_rect,
                 image_y_offset, image_ysize, arena);
  }
  JXL_CHECK_IMAGE_INITIALIZED(*dst, dst_rect);
}

}  // namespace jxl
#endif  // HWY_ONCE
