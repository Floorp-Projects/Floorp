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

#include "lib/jxl/image.h"

#include <algorithm>  // swap

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "lib/jxl/image.cc"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

#include "lib/jxl/base/profiler.h"
#include "lib/jxl/common.h"
#include "lib/jxl/image_ops.h"

HWY_BEFORE_NAMESPACE();
namespace jxl {

namespace HWY_NAMESPACE {
size_t GetVectorSize() { return HWY_LANES(uint8_t); }
// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE

}  // namespace jxl
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace jxl {
namespace {

HWY_EXPORT(GetVectorSize);  // Local function.

size_t VectorSize() {
  static size_t bytes = HWY_DYNAMIC_DISPATCH(GetVectorSize)();
  return bytes;
}

// Returns distance [bytes] between the start of two consecutive rows, a
// multiple of vector/cache line size but NOT CacheAligned::kAlias - see below.
size_t BytesPerRow(const size_t xsize, const size_t sizeof_t) {
  const size_t vec_size = VectorSize();
  size_t valid_bytes = xsize * sizeof_t;

  // Allow unaligned accesses starting at the last valid value - this may raise
  // msan errors unless the user calls InitializePaddingForUnalignedAccesses.
  // Skip for the scalar case because no extra lanes will be loaded.
  if (vec_size != 0) {
    valid_bytes += vec_size - sizeof_t;
  }

  // Round up to vector and cache line size.
  const size_t align = std::max(vec_size, CacheAligned::kAlignment);
  size_t bytes_per_row = RoundUpTo(valid_bytes, align);

  // During the lengthy window before writes are committed to memory, CPUs
  // guard against read after write hazards by checking the address, but
  // only the lower 11 bits. We avoid a false dependency between writes to
  // consecutive rows by ensuring their sizes are not multiples of 2 KiB.
  // Avoid2K prevents the same problem for the planes of an Image3.
  if (bytes_per_row % CacheAligned::kAlias == 0) {
    bytes_per_row += align;
  }

  JXL_ASSERT(bytes_per_row % align == 0);
  return bytes_per_row;
}

}  // namespace

PlaneBase::PlaneBase(const size_t xsize, const size_t ysize,
                     const size_t sizeof_t)
    : xsize_(static_cast<uint32_t>(xsize)),
      ysize_(static_cast<uint32_t>(ysize)),
      orig_xsize_(static_cast<uint32_t>(xsize)),
      orig_ysize_(static_cast<uint32_t>(ysize)) {
  // (Can't profile CacheAligned itself because it is used by profiler.h)
  PROFILER_FUNC;

  JXL_CHECK(xsize == xsize_);
  JXL_CHECK(ysize == ysize_);

  JXL_ASSERT(sizeof_t == 1 || sizeof_t == 2 || sizeof_t == 4 || sizeof_t == 8);

  bytes_per_row_ = 0;
  // Dimensions can be zero, e.g. for lazily-allocated images. Only allocate
  // if nonzero, because "zero" bytes still have padding/bookkeeping overhead.
  if (xsize != 0 && ysize != 0) {
    bytes_per_row_ = BytesPerRow(xsize, sizeof_t);
    bytes_ = AllocateArray(bytes_per_row_ * ysize);
    JXL_CHECK(bytes_.get());
    InitializePadding(sizeof_t, Padding::kRoundUp);
  }
}

void PlaneBase::InitializePadding(const size_t sizeof_t, Padding padding) {
#if defined(MEMORY_SANITIZER) || HWY_IDE
  if (xsize_ == 0 || ysize_ == 0) return;

  const size_t vec_size = VectorSize();
  if (vec_size == 0) return;  // Scalar mode: no padding needed

  const size_t valid_size = xsize_ * sizeof_t;
  const size_t initialize_size = padding == Padding::kRoundUp
                                     ? RoundUpTo(valid_size, vec_size)
                                     : valid_size + vec_size - sizeof_t;
  if (valid_size == initialize_size) return;

  for (size_t y = 0; y < ysize_; ++y) {
    uint8_t* JXL_RESTRICT row = static_cast<uint8_t*>(VoidRow(y));
#if defined(__clang__) && (__clang_major__ <= 6)
    // There's a bug in msan in clang-6 when handling AVX2 operations. This
    // workaround allows tests to pass on msan, although it is slower and
    // prevents msan warnings from uninitialized images.
    memset(row, 0, initialize_size);
#else
    memset(row + valid_size, 0, initialize_size - valid_size);
#endif  // clang6
  }
#endif  // MEMORY_SANITIZER
}

void PlaneBase::Swap(PlaneBase& other) {
  std::swap(xsize_, other.xsize_);
  std::swap(ysize_, other.ysize_);
  std::swap(orig_xsize_, other.orig_xsize_);
  std::swap(orig_ysize_, other.orig_ysize_);
  std::swap(bytes_per_row_, other.bytes_per_row_);
  std::swap(bytes_, other.bytes_);
}

ImageB ImageFromPacked(const uint8_t* packed, const size_t xsize,
                       const size_t ysize, const size_t bytes_per_row) {
  JXL_ASSERT(bytes_per_row >= xsize);
  ImageB image(xsize, ysize);
  PROFILER_FUNC;
  for (size_t y = 0; y < ysize; ++y) {
    uint8_t* const JXL_RESTRICT row = image.Row(y);
    const uint8_t* const JXL_RESTRICT packed_row = packed + y * bytes_per_row;
    memcpy(row, packed_row, xsize);
  }
  return image;
}

// Note that using mirroring here gives slightly worse results.
ImageF PadImage(const ImageF& in, const size_t xsize, const size_t ysize) {
  JXL_ASSERT(xsize >= in.xsize());
  JXL_ASSERT(ysize >= in.ysize());
  ImageF out(xsize, ysize);
  size_t y = 0;
  for (; y < in.ysize(); ++y) {
    const float* JXL_RESTRICT row_in = in.ConstRow(y);
    float* JXL_RESTRICT row_out = out.Row(y);
    memcpy(row_out, row_in, in.xsize() * sizeof(row_in[0]));
    const int lastcol = in.xsize() - 1;
    const float lastval = row_out[lastcol];
    for (size_t x = in.xsize(); x < xsize; ++x) {
      row_out[x] = lastval;
    }
  }

  // TODO(janwas): no need to copy if we can 'extend' image: if rows are
  // pointers to any memory? Or allocate larger image before IO?
  const int lastrow = in.ysize() - 1;
  for (; y < ysize; ++y) {
    const float* JXL_RESTRICT row_in = out.ConstRow(lastrow);
    float* JXL_RESTRICT row_out = out.Row(y);
    memcpy(row_out, row_in, xsize * sizeof(row_out[0]));
  }
  return out;
}

Image3F PadImageMirror(const Image3F& in, const size_t xborder,
                       const size_t yborder) {
  size_t xsize = in.xsize();
  size_t ysize = in.ysize();
  Image3F out(xsize + 2 * xborder, ysize + 2 * yborder);
  if (xborder > xsize || yborder > ysize) {
    for (size_t c = 0; c < 3; c++) {
      for (int32_t y = 0; y < static_cast<int32_t>(out.ysize()); y++) {
        float* row_out = out.PlaneRow(c, y);
        const float* row_in = in.PlaneRow(
            c, Mirror(y - static_cast<int32_t>(yborder), in.ysize()));
        for (int32_t x = 0; x < static_cast<int32_t>(out.xsize()); x++) {
          int32_t xin = Mirror(x - static_cast<int32_t>(xborder), in.xsize());
          row_out[x] = row_in[xin];
        }
      }
    }
    return out;
  }
  CopyImageTo(in, Rect(xborder, yborder, xsize, ysize), &out);
  for (size_t c = 0; c < 3; c++) {
    // Horizontal pad.
    for (size_t y = 0; y < ysize; y++) {
      for (size_t x = 0; x < xborder; x++) {
        out.PlaneRow(c, y + yborder)[x] =
            in.ConstPlaneRow(c, y)[xborder - x - 1];
        out.PlaneRow(c, y + yborder)[x + xsize + xborder] =
            in.ConstPlaneRow(c, y)[xsize - 1 - x];
      }
    }
    // Vertical pad.
    for (size_t y = 0; y < yborder; y++) {
      memcpy(out.PlaneRow(c, y), out.ConstPlaneRow(c, 2 * yborder - 1 - y),
             out.xsize() * sizeof(float));
      memcpy(out.PlaneRow(c, y + ysize + yborder),
             out.ConstPlaneRow(c, ysize + yborder - 1 - y),
             out.xsize() * sizeof(float));
    }
  }
  return out;
}

Image3F PadImageToMultiple(const Image3F& in, const size_t N) {
  PROFILER_FUNC;
  const size_t xsize_blocks = DivCeil(in.xsize(), N);
  const size_t ysize_blocks = DivCeil(in.ysize(), N);
  const size_t xsize = N * xsize_blocks;
  const size_t ysize = N * ysize_blocks;
  ImageF out[3];
  for (size_t c = 0; c < 3; ++c) {
    out[c] = PadImage(in.Plane(c), xsize, ysize);
  }
  return Image3F(std::move(out[0]), std::move(out[1]), std::move(out[2]));
}

void PadImageToBlockMultipleInPlace(Image3F* JXL_RESTRICT in) {
  PROFILER_FUNC;
  const size_t xsize_orig = in->xsize();
  const size_t ysize_orig = in->ysize();
  const size_t xsize = RoundUpToBlockDim(xsize_orig);
  const size_t ysize = RoundUpToBlockDim(ysize_orig);
  // Expands image size to the originally-allocated size.
  in->ShrinkTo(xsize, ysize);
  for (size_t c = 0; c < 3; c++) {
    for (size_t y = 0; y < ysize_orig; y++) {
      float* JXL_RESTRICT row = in->PlaneRow(c, y);
      for (size_t x = xsize_orig; x < xsize; x++) {
        row[x] = row[xsize_orig - 1];
      }
    }
    const float* JXL_RESTRICT row_src = in->ConstPlaneRow(c, ysize_orig - 1);
    for (size_t y = ysize_orig; y < ysize; y++) {
      memcpy(in->PlaneRow(c, y), row_src, xsize * sizeof(float));
    }
  }
}

float DotProduct(const ImageF& a, const ImageF& b) {
  double sum = 0.0;
  for (size_t y = 0; y < a.ysize(); ++y) {
    const float* const JXL_RESTRICT row_a = a.ConstRow(y);
    const float* const JXL_RESTRICT row_b = b.ConstRow(y);
    for (size_t x = 0; x < a.xsize(); ++x) {
      sum += row_a[x] * row_b[x];
    }
  }
  return sum;
}

void DownsampleImage(Image3F* opsin, size_t factor) {
  JXL_ASSERT(factor != 1);
  // Allocate extra space to avoid a reallocation when padding.
  Image3F downsampled(DivCeil(opsin->xsize(), factor) + kBlockDim,
                      DivCeil(opsin->ysize(), factor) + kBlockDim);
  downsampled.ShrinkTo(downsampled.xsize() - kBlockDim,
                       downsampled.ysize() - kBlockDim);
  size_t in_stride = opsin->PixelsPerRow();
  for (size_t c = 0; c < 3; c++) {
    for (size_t y = 0; y < downsampled.ysize(); y++) {
      float* row_out = downsampled.PlaneRow(c, y);
      const float* row_in = opsin->PlaneRow(c, factor * y);
      for (size_t x = 0; x < downsampled.xsize(); x++) {
        size_t cnt = 0;
        float sum = 0;
        for (size_t iy = 0; iy < factor && iy + factor * y < opsin->ysize();
             iy++) {
          for (size_t ix = 0; ix < factor && ix + factor * x < opsin->xsize();
               ix++) {
            sum += row_in[iy * in_stride + x * factor + ix];
            cnt++;
          }
        }
        row_out[x] = sum / cnt;
      }
    }
  }
  *opsin = std::move(downsampled);
}

}  // namespace jxl
#endif  // HWY_ONCE
