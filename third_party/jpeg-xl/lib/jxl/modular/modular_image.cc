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

#include "lib/jxl/modular/modular_image.h"

#include "lib/jxl/base/status.h"
#include "lib/jxl/common.h"
#include "lib/jxl/modular/transform/transform.h"

namespace jxl {

void Channel::compute_minmax(pixel_type *min, pixel_type *max) const {
  pixel_type realmin = std::numeric_limits<pixel_type>::max();
  pixel_type realmax = std::numeric_limits<pixel_type>::min();
  for (size_t y = 0; y < h; y++) {
    const pixel_type *JXL_RESTRICT p = plane.Row(y);
    for (size_t x = 0; x < w; x++) {
      if (p[x] < realmin) realmin = p[x];
      if (p[x] > realmax) realmax = p[x];
    }
  }

  if (min) *min = realmin;
  if (max) *max = realmax;
}

void Image::undo_transforms(const weighted::Header &wp_header, int keep,
                            jxl::ThreadPool *pool) {
  if (keep == -2) return;
  while ((int)transform.size() > keep && transform.size() > 0) {
    Transform t = transform.back();
    JXL_DEBUG_V(4, "Undoing transform %s", t.Name());
    Status result = t.Inverse(*this, wp_header, pool);
    if (result == false) {
      JXL_NOTIFY_ERROR("Error while undoing transform %s.", t.Name());
      error = true;
      return;
    }
    JXL_DEBUG_V(8, "Undoing transform %s: done", t.Name());
    transform.pop_back();
  }
  if (!keep) {  // clamp the values to the valid range (lossy
                // compression can produce values outside the range)
    for (size_t i = 0; i < channel.size(); i++) {
      for (size_t y = 0; y < channel[i].h; y++) {
        pixel_type *JXL_RESTRICT p = channel[i].plane.Row(y);
        for (size_t x = 0; x < channel[i].w; x++, p++) {
          *p = Clamp1(*p, minval, maxval);
        }
      }
    }
  }
}

bool Image::do_transform(const Transform &tr,
                         const weighted::Header &wp_header) {
  Transform t = tr;
  bool did_it = t.Forward(*this, wp_header);
  if (did_it) transform.push_back(t);
  return did_it;
}

Image::Image(size_t iw, size_t ih, int maxval, int nb_chans)
    : w(iw),
      h(ih),
      minval(0),
      maxval(maxval),
      nb_channels(nb_chans),
      real_nb_channels(nb_chans),
      nb_meta_channels(0),
      error(false) {
  for (int i = 0; i < nb_chans; i++) channel.emplace_back(Channel(iw, ih));
}
Image::Image()
    : w(0),
      h(0),
      minval(0),
      maxval(255),
      nb_channels(0),
      real_nb_channels(0),
      nb_meta_channels(0),
      error(true) {}

Image::~Image() = default;

Image &Image::operator=(Image &&other) noexcept {
  w = other.w;
  h = other.h;
  minval = other.minval;
  maxval = other.maxval;
  nb_channels = other.nb_channels;
  real_nb_channels = other.real_nb_channels;
  nb_meta_channels = other.nb_meta_channels;
  error = other.error;
  channel = std::move(other.channel);
  transform = std::move(other.transform);
  return *this;
}

}  // namespace jxl
