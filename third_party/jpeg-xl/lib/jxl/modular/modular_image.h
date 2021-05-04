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

#ifndef LIB_JXL_MODULAR_MODULAR_IMAGE_H_
#define LIB_JXL_MODULAR_MODULAR_IMAGE_H_

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <utility>
#include <vector>

#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/image.h"
#include "lib/jxl/image_ops.h"

namespace jxl {

typedef int32_t pixel_type;  // can use int16_t if it's only for 8-bit images.
                             // Need some wiggle room for YCoCg / Squeeze etc

typedef int64_t pixel_type_w;

namespace weighted {
struct Header;
}

class Channel {
 public:
  jxl::Plane<pixel_type> plane;
  size_t w, h;
  int hshift, vshift;  // w ~= image.w >> hshift;  h ~= image.h >> vshift
  int hcshift,
      vcshift;  // cumulative, i.e. when decoding up to this point, we have data
                // available with these shifts (for this component)
  Channel(size_t iw, size_t ih, int hsh = 0, int vsh = 0, int hcsh = 0,
          int vcsh = 0)
      : plane(iw, ih),
        w(iw),
        h(ih),
        hshift(hsh),
        vshift(vsh),
        hcshift(hcsh),
        vcshift(vcsh) {}
  Channel()
      : plane(0, 0), w(0), h(0), hshift(0), vshift(0), hcshift(0), vcshift(0) {}

  Channel(const Channel& other) = delete;
  Channel& operator=(const Channel& other) = delete;

  // Move assignment
  Channel& operator=(Channel&& other) noexcept {
    w = other.w;
    h = other.h;
    hshift = other.hshift;
    vshift = other.vshift;
    hcshift = other.hcshift;
    vcshift = other.vcshift;
    plane = std::move(other.plane);
    return *this;
  }

  // Move constructor
  Channel(Channel&& other) noexcept = default;

  void resize(pixel_type value = 0) {
    if (plane.xsize() == w && plane.ysize() == h) return;
    jxl::Plane<pixel_type> resizedplane(w, h);
    if (plane.xsize() || plane.ysize()) {
      // copy pixels over from old plane to new plane
      size_t y = 0;
      for (; y < plane.ysize() && y < h; y++) {
        const pixel_type* JXL_RESTRICT p = plane.Row(y);
        pixel_type* JXL_RESTRICT rp = resizedplane.Row(y);
        size_t x = 0;
        for (; x < plane.xsize() && x < w; x++) rp[x] = p[x];
        for (; x < w; x++) rp[x] = value;
      }
      for (; y < h; y++) {
        pixel_type* JXL_RESTRICT p = resizedplane.Row(y);
        for (size_t x = 0; x < w; x++) p[x] = value;
      }
    } else if (w && h && value == 0) {
      size_t ppr = resizedplane.bytes_per_row();
      memset(resizedplane.bytes(), 0, ppr * h);
    } else if (w && h) {
      FillImage(value, &resizedplane);
    }
    plane = std::move(resizedplane);
  }
  void resize(int nw, int nh) {
    w = nw;
    h = nh;
    resize();
  }
  bool is_empty() const { return (plane.ysize() == 0); }

  JXL_INLINE pixel_type* Row(const size_t y) { return plane.Row(y); }
  JXL_INLINE const pixel_type* Row(const size_t y) const {
    return plane.Row(y);
  }
  void compute_minmax(pixel_type* min, pixel_type* max) const;
};

class Transform;

class Image {
 public:
  std::vector<Channel>
      channel;  // image data, transforms can dramatically change the number of
                // channels and their semantics
  std::vector<Transform>
      transform;  // keeps track of the transforms that have been applied (and
                  // that have to be undone when rendering the image)

  size_t w, h;  // actual dimensions of the image (channels may have different
                // dimensions due to transforms like chroma subsampling and DCT)
  int minval, maxval;  // actual (largest) range of the channels (actual ranges
                       // might be different due to transforms; after undoing
                       // transforms, might still be different due to lossy)
  size_t nb_channels;  // actual number of distinct channels (after undoing all
                       // transforms except Palette; can be different from
                       // channel.size())
  size_t real_nb_channels;  // real number of channels (after undoing all
                            // transforms)
  size_t nb_meta_channels;  // first few channels might contain things like
                            // palettes or compaction data that are not yet real
                            // image data
  bool error;               // true if a fatal error occurred, false otherwise

  Image(size_t iw, size_t ih, int maxval, int nb_chans);

  Image();
  ~Image();

  Image(const Image& other) = delete;
  Image& operator=(const Image& other) = delete;

  Image& operator=(Image&& other) noexcept;
  Image(Image&& other) noexcept = default;

  bool do_transform(const Transform& t, const weighted::Header& wp_header);
  // undo all except the first 'keep' transforms
  void undo_transforms(const weighted::Header& wp_header, int keep = 0,
                       jxl::ThreadPool* pool = nullptr);
};

}  // namespace jxl

#endif  // LIB_JXL_MODULAR_MODULAR_IMAGE_H_
