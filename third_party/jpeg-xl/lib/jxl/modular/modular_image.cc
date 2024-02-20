// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/modular/modular_image.h"

#include <sstream>

#include "lib/jxl/base/status.h"
#include "lib/jxl/image_ops.h"
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

Image::Image(size_t iw, size_t ih, int bitdepth)
    : w(iw), h(ih), bitdepth(bitdepth), nb_meta_channels(0), error(false) {}

StatusOr<Image> Image::Create(size_t iw, size_t ih, int bitdepth,
                              int nb_chans) {
  Image result(iw, ih, bitdepth);
  for (int i = 0; i < nb_chans; i++) {
    StatusOr<Channel> channel_or = Channel::Create(iw, ih);
    JXL_RETURN_IF_ERROR(channel_or.status());
    result.channel.emplace_back(std::move(channel_or).value());
  }
  return result;
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

StatusOr<Image> Image::Clone(const Image &that) {
  Image clone(that.w, that.h, that.bitdepth);
  clone.nb_meta_channels = that.nb_meta_channels;
  clone.error = that.error;
  clone.transform = that.transform;
  for (const Channel &ch : that.channel) {
    JXL_ASSIGN_OR_RETURN(Channel a,
                         Channel::Create(ch.w, ch.h, ch.hshift, ch.vshift));
    CopyImageTo(ch.plane, &a.plane);
    clone.channel.push_back(std::move(a));
  }
  return clone;
}

#if JXL_DEBUG_V_LEVEL >= 1
std::string Image::DebugString() const {
  std::ostringstream os;
  os << w << "x" << h << ", depth: " << bitdepth;
  if (!channel.empty()) {
    os << ", channels:";
    for (size_t i = 0; i < channel.size(); ++i) {
      os << " " << channel[i].w << "x" << channel[i].h
         << "(shift: " << channel[i].hshift << "," << channel[i].vshift << ")";
      if (i < nb_meta_channels) os << "*";
    }
  }
  return os.str();
}
#endif

}  // namespace jxl
