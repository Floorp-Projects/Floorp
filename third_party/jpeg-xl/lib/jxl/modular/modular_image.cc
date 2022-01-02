// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/modular/modular_image.h"

#include "lib/jxl/base/status.h"
#include "lib/jxl/common.h"
#include "lib/jxl/modular/transform/transform.h"

namespace jxl {

void Image::undo_transforms(const weighted::Header &wp_header,
                            jxl::ThreadPool *pool) {
  while (!transform.empty()) {
    Transform t = transform.back();
    JXL_DEBUG_V(4, "Undoing transform");
    Status result = t.Inverse(*this, wp_header, pool);
    if (result == false) {
      JXL_NOTIFY_ERROR("Error while undoing transform.");
      error = true;
      return;
    }
    JXL_DEBUG_V(8, "Undoing transform: done");
    transform.pop_back();
  }
}

Image::Image(size_t iw, size_t ih, int bd, int nb_chans)
    : w(iw), h(ih), bitdepth(bd), nb_meta_channels(0), error(false) {
  for (int i = 0; i < nb_chans; i++) channel.emplace_back(Channel(iw, ih));
}

Image::Image() : w(0), h(0), bitdepth(8), nb_meta_channels(0), error(true) {}

Image &Image::operator=(Image &&other) noexcept {
  w = other.w;
  h = other.h;
  bitdepth = other.bitdepth;
  nb_meta_channels = other.nb_meta_channels;
  error = other.error;
  channel = std::move(other.channel);
  transform = std::move(other.transform);
  return *this;
}

Image Image::clone() {
  Image c(w, h, bitdepth, 0);
  c.nb_meta_channels = nb_meta_channels;
  c.error = error;
  c.transform = transform;
  for (Channel &ch : channel) {
    Channel a(ch.w, ch.h, ch.hshift, ch.vshift);
    CopyImageTo(ch.plane, &a.plane);
    c.channel.push_back(std::move(a));
  }
  return c;
}

}  // namespace jxl
