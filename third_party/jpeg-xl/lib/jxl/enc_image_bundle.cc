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

#include "lib/jxl/enc_image_bundle.h"

#include <limits>
#include <utility>

#include "lib/jxl/alpha.h"
#include "lib/jxl/base/byte_order.h"
#include "lib/jxl/base/padded_bytes.h"
#include "lib/jxl/base/profiler.h"
#include "lib/jxl/codec_in_out.h"
#include "lib/jxl/enc_color_management.h"
#include "lib/jxl/fields.h"
#include "lib/jxl/image_bundle.h"
#include "lib/jxl/luminance.h"

namespace jxl {

namespace {

// Copies ib:rect, converts, and copies into out.
template <typename T>
Status CopyToT(const ImageMetadata* metadata, const ImageBundle* ib,
               const Rect& rect, const ColorEncoding& c_desired,
               ThreadPool* pool, Image3<T>* out) {
  PROFILER_FUNC;
  static_assert(
      std::is_same<T, float>::value || std::numeric_limits<T>::min() == 0,
      "CopyToT implemented only for float and unsigned types");
  ColorSpaceTransform c_transform;
  // Changing IsGray is probably a bug.
  JXL_CHECK(ib->IsGray() == c_desired.IsGray());
#if JPEGXL_ENABLE_SKCMS
  bool is_gray = false;
#else
  bool is_gray = ib->IsGray();
#endif
  if (out->xsize() < rect.xsize() || out->ysize() < rect.ysize()) {
    *out = Image3<T>(rect.xsize(), rect.ysize());
  } else {
    out->ShrinkTo(rect.xsize(), rect.ysize());
  }
  RunOnPool(
      pool, 0, rect.ysize(),
      [&](size_t num_threads) {
        return c_transform.Init(ib->c_current(), c_desired,
                                metadata->IntensityTarget(), rect.xsize(),
                                num_threads);
      },
      [&](const int y, const int thread) {
        float* mutable_src_buf = c_transform.BufSrc(thread);
        const float* src_buf = mutable_src_buf;
        // Interleave input.
        if (is_gray) {
          src_buf = rect.ConstPlaneRow(ib->color(), 0, y);
        } else {
          const float* JXL_RESTRICT row_in0 =
              rect.ConstPlaneRow(ib->color(), 0, y);
          const float* JXL_RESTRICT row_in1 =
              rect.ConstPlaneRow(ib->color(), 1, y);
          const float* JXL_RESTRICT row_in2 =
              rect.ConstPlaneRow(ib->color(), 2, y);
          for (size_t x = 0; x < rect.xsize(); x++) {
            mutable_src_buf[3 * x + 0] = row_in0[x];
            mutable_src_buf[3 * x + 1] = row_in1[x];
            mutable_src_buf[3 * x + 2] = row_in2[x];
          }
        }
        float* JXL_RESTRICT dst_buf = c_transform.BufDst(thread);
        DoColorSpaceTransform(&c_transform, thread, src_buf, dst_buf);
        T* JXL_RESTRICT row_out0 = out->PlaneRow(0, y);
        T* JXL_RESTRICT row_out1 = out->PlaneRow(1, y);
        T* JXL_RESTRICT row_out2 = out->PlaneRow(2, y);
        // De-interleave output and convert type.
        if (std::is_same<float, T>::value) {  // deinterleave to float.
          if (is_gray) {
            for (size_t x = 0; x < rect.xsize(); x++) {
              row_out0[x] = dst_buf[x];
              row_out1[x] = dst_buf[x];
              row_out2[x] = dst_buf[x];
            }
          } else {
            for (size_t x = 0; x < rect.xsize(); x++) {
              row_out0[x] = dst_buf[3 * x + 0];
              row_out1[x] = dst_buf[3 * x + 1];
              row_out2[x] = dst_buf[3 * x + 2];
            }
          }
        } else {
          // Convert to T, doing clamping.
          float max = std::numeric_limits<T>::max();
          auto cvt = [max](float in) {
            float v = std::max(0.0f, std::min(max, in * max));
            return static_cast<T>(v < 0 ? v - 0.5f : v + 0.5f);
          };
          if (is_gray) {
            for (size_t x = 0; x < rect.xsize(); x++) {
              row_out0[x] = cvt(dst_buf[x]);
              row_out1[x] = cvt(dst_buf[x]);
              row_out2[x] = cvt(dst_buf[x]);
            }
          } else {
            for (size_t x = 0; x < rect.xsize(); x++) {
              row_out0[x] = cvt(dst_buf[3 * x + 0]);
              row_out1[x] = cvt(dst_buf[3 * x + 1]);
              row_out2[x] = cvt(dst_buf[3 * x + 2]);
            }
          }
        }
      },
      "Colorspace transform");
  return true;
}

}  // namespace

Status ImageBundle::TransformTo(const ColorEncoding& c_desired,
                                ThreadPool* pool) {
  PROFILER_FUNC;
  JXL_RETURN_IF_ERROR(CopyTo(Rect(color_), c_desired, &color_, pool));
  c_current_ = c_desired;
  return true;
}

Status ImageBundle::CopyTo(const Rect& rect, const ColorEncoding& c_desired,
                           Image3B* out, ThreadPool* pool) const {
  return CopyToT(metadata_, this, rect, c_desired, pool, out);
}
Status ImageBundle::CopyTo(const Rect& rect, const ColorEncoding& c_desired,
                           Image3F* out, ThreadPool* pool) const {
  return CopyToT(metadata_, this, rect, c_desired, pool, out);
}

Status ImageBundle::CopyToSRGB(const Rect& rect, Image3B* out,
                               ThreadPool* pool) const {
  return CopyTo(rect, ColorEncoding::SRGB(IsGray()), out, pool);
}

Status TransformIfNeeded(const ImageBundle& in, const ColorEncoding& c_desired,
                         ThreadPool* pool, ImageBundle* store,
                         const ImageBundle** out) {
  if (in.c_current().SameColorEncoding(c_desired)) {
    *out = &in;
    return true;
  }
  // TODO(janwas): avoid copying via createExternal+copyBackToIO
  // instead of copy+createExternal+copyBackToIO
  store->SetFromImage(CopyImage(in.color()), in.c_current());

  // Must at least copy the alpha channel for use by external_image.
  if (in.HasExtraChannels()) {
    std::vector<ImageF> extra_channels;
    for (const ImageF& extra_channel : in.extra_channels()) {
      extra_channels.emplace_back(CopyImage(extra_channel));
    }
    store->SetExtraChannels(std::move(extra_channels));
  }

  if (!store->TransformTo(c_desired, pool)) {
    return false;
  }
  *out = store;
  return true;
}

}  // namespace jxl
