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

#include "lib/jxl/image_bundle.h"

#include <limits>
#include <utility>

#include "lib/jxl/alpha.h"
#include "lib/jxl/base/byte_order.h"
#include "lib/jxl/base/padded_bytes.h"
#include "lib/jxl/base/profiler.h"
#include "lib/jxl/codec_in_out.h"
#include "lib/jxl/color_management.h"
#include "lib/jxl/fields.h"
#include "lib/jxl/luminance.h"

namespace jxl {

void ImageBundle::ShrinkTo(size_t xsize, size_t ysize) {
  if (HasColor()) color_.ShrinkTo(xsize, ysize);
  for (size_t i = 0; i < extra_channels_.size(); ++i) {
    const auto& eci = metadata_->extra_channel_info[i];
    extra_channels_[i].ShrinkTo(eci.Size(xsize), eci.Size(ysize));
  }
}

// Called by all other SetFrom*.
void ImageBundle::SetFromImage(Image3F&& color,
                               const ColorEncoding& c_current) {
  JXL_CHECK(color.xsize() != 0 && color.ysize() != 0);
  JXL_CHECK(metadata_->color_encoding.IsGray() == c_current.IsGray());
  color_ = std::move(color);
  c_current_ = c_current;
  VerifySizes();
}

void ImageBundle::VerifyMetadata() const {
  JXL_CHECK(!c_current_.ICC().empty());
  JXL_CHECK(metadata_->color_encoding.IsGray() == IsGray());

  if (metadata_->HasAlpha() && alpha().xsize() == 0) {
    JXL_ABORT("MD alpha_bits %u IB alpha %zu x %zu\n",
              metadata_->GetAlphaBits(), alpha().xsize(), alpha().ysize());
  }
  const uint32_t alpha_bits = metadata_->GetAlphaBits();
  JXL_CHECK(alpha_bits <= 32);

  // metadata_->num_extra_channels may temporarily differ from
  // extra_channels_.size(), e.g. after SetAlpha. They are synced by the next
  // call to VisitFields.
}

void ImageBundle::VerifySizes() const {
  const size_t xs = xsize();
  const size_t ys = ysize();

  if (HasExtraChannels()) {
    JXL_CHECK(xs != 0 && ys != 0);
    for (size_t ec = 0; ec < metadata_->extra_channel_info.size(); ++ec) {
      const ExtraChannelInfo& eci = metadata_->extra_channel_info[ec];
      JXL_CHECK(extra_channels_[ec].xsize() == eci.Size(xs));
      JXL_CHECK(extra_channels_[ec].ysize() == eci.Size(ys));
    }
  }
}

size_t ImageBundle::DetectRealBitdepth() const {
  return metadata_->bit_depth.bits_per_sample;

  // TODO(lode): let this function return lower bit depth if possible, e.g.
  // return 8 bits in case the original image came from a 16-bit PNG that
  // was in fact representable as 8-bit PNG. Ensure that the implementation
  // returns 16 if e.g. two consecutive 16-bit values appeared in the original
  // image (such as 32768 and 32769), take into account that e.g. the values
  // 3-bit can represent is not a superset of the values 2-bit can represent,
  // and there may be slight imprecisions in the floating point image.
}

const ImageF& ImageBundle::alpha() const {
  JXL_ASSERT(HasAlpha());
  const size_t ec = metadata_->Find(ExtraChannel::kAlpha) -
                    metadata_->extra_channel_info.data();
  JXL_ASSERT(ec < extra_channels_.size());
  return extra_channels_[ec];
}
ImageF* ImageBundle::alpha() {
  JXL_ASSERT(HasAlpha());
  const size_t ec = metadata_->Find(ExtraChannel::kAlpha) -
                    metadata_->extra_channel_info.data();
  JXL_ASSERT(ec < extra_channels_.size());
  return &extra_channels_[ec];
}

const ImageF& ImageBundle::depth() const {
  JXL_ASSERT(HasDepth());
  const size_t ec = metadata_->Find(ExtraChannel::kDepth) -
                    metadata_->extra_channel_info.data();
  JXL_ASSERT(ec < extra_channels_.size());
  return extra_channels_[ec];
}

void ImageBundle::SetAlpha(ImageF&& alpha, bool alpha_is_premultiplied) {
  const ExtraChannelInfo* eci = metadata_->Find(ExtraChannel::kAlpha);
  // Must call SetAlphaBits first, otherwise we don't know which channel index
  JXL_CHECK(eci != nullptr);
  JXL_CHECK(alpha.xsize() != 0 && alpha.ysize() != 0);
  JXL_CHECK(eci->alpha_associated == alpha_is_premultiplied);
  extra_channels_.insert(
      extra_channels_.begin() + (eci - metadata_->extra_channel_info.data()),
      std::move(alpha));
  // num_extra_channels is automatically set in visitor
  VerifySizes();
}

void ImageBundle::SetExtraChannels(std::vector<ImageF>&& extra_channels) {
  for (const ImageF& plane : extra_channels) {
    JXL_CHECK(plane.xsize() != 0 && plane.ysize() != 0);
  }
  extra_channels_ = std::move(extra_channels);
  VerifySizes();
}
}  // namespace jxl
