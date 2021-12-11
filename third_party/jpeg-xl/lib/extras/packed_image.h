// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_EXTRAS_PACKED_IMAGE_H_
#define LIB_EXTRAS_PACKED_IMAGE_H_

// Helper class for storing external (int or float, interleaved) images. This is
// the common format used by other libraries and in the libjxl API.

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <memory>
#include <string>
#include <vector>

#include "jxl/codestream_header.h"
#include "jxl/types.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/common.h"

namespace jxl {
namespace extras {

// Class representing an interleaved image with a bunch of channels.
class PackedImage {
 public:
  PackedImage(size_t xsize, size_t ysize, const JxlPixelFormat& format)
      : PackedImage(xsize, ysize, format, CalcStride(format, xsize)) {}
  PackedImage(size_t xsize, size_t ysize, const JxlPixelFormat& format,
              size_t stride)
      : xsize(xsize),
        ysize(ysize),
        stride(stride),
        format(format),
        pixels_size(ysize * stride),
        pixels_(malloc(pixels_size), free) {}
  // Construct the image using the passed pixel buffer. The buffer is owned by
  // this object and released with free().
  PackedImage(size_t xsize, size_t ysize, const JxlPixelFormat& format,
              void* pixels, size_t pixels_size)
      : xsize(xsize),
        ysize(ysize),
        stride(CalcStride(format, xsize)),
        format(format),
        pixels_size(pixels_size),
        pixels_(pixels, free) {
    JXL_ASSERT(pixels_size >= stride * ysize);
  }

  // The interleaved pixels as defined in the storage format.
  void* pixels() const { return pixels_.get(); }

  // The image size in pixels.
  size_t xsize;
  size_t ysize;

  // Whether the y coordinate is flipped (y=0 is the last row).
  bool flipped_y = false;

  // The number of bytes per row.
  size_t stride;

  // Pixel storage format and buffer size of the pixels_ pointer.
  JxlPixelFormat format;
  size_t pixels_size;

 private:
  static size_t BitsPerChannel(JxlDataType data_type) {
    switch (data_type) {
      case JXL_TYPE_BOOLEAN:
        return 1;
      case JXL_TYPE_UINT8:
        return 8;
      case JXL_TYPE_UINT16:
        return 16;
      case JXL_TYPE_UINT32:
        return 32;
      case JXL_TYPE_FLOAT:
        return 32;
      case JXL_TYPE_FLOAT16:
        return 16;
        // No default, give compiler error if new type not handled.
    }
    return 0;  // Indicate invalid data type.
  }

  static size_t CalcStride(const JxlPixelFormat& format, size_t xsize) {
    size_t stride = xsize * (BitsPerChannel(format.data_type) *
                             format.num_channels / jxl::kBitsPerByte);
    if (format.align > 1) {
      stride = jxl::DivCeil(stride, format.align) * format.align;
    }
    return stride;
  }

  std::unique_ptr<void, decltype(free)*> pixels_;
};

// Helper class representing a frame, as seen from the API. Animations will have
// multiple frames, but a single frame can have a color/grayscale channel and
// multiple extra channels. The order of the extra channels should be the same
// as all other frames in the same image.
class PackedFrame {
 public:
  template <typename... Args>
  PackedFrame(Args... args) : color(args...) {}

  // The Frame metadata.
  JxlFrameHeader frame_info = {};
  std::string name;

  // Offset of the frame in the image.
  // TODO(deymo): Add support in the API for this.
  size_t x0 = 0;
  size_t y0 = 0;

  // Whether this frame should be blended with the previous one.
  // TODO(deymo): Maybe add support for this in the API.
  bool blend = false;
  bool use_for_next_frame = false;

  // The pixel data for the color (or grayscale) channels.
  PackedImage color;
  // Extra channel image data.
  std::vector<PackedImage> extra_channels;
};

// Optional metadata associated with a file
class PackedMetadata {
 public:
  std::vector<uint8_t> exif;
  std::vector<uint8_t> iptc;
  std::vector<uint8_t> jumbf;
  std::vector<uint8_t> xmp;
};

// Helper class representing a JXL image file as decoded to pixels from the API.
class PackedPixelFile {
 public:
  JxlBasicInfo info = {};

  // The extra channel metadata information.
  struct PackedExtraChannel {
    PackedExtraChannel(const JxlExtraChannelInfo& ec_info,
                       const std::string& name)
        : ec_info(ec_info), name(name) {}

    JxlExtraChannelInfo ec_info;
    std::string name;
  };
  std::vector<PackedExtraChannel> extra_channels_info;

  // Color information. If the icc is empty, the JxlColorEncoding should be used
  // instead.
  std::vector<uint8_t> icc;
  JxlColorEncoding color_encoding = {};

  std::vector<PackedFrame> frames;

  PackedMetadata metadata;
};

}  // namespace extras
}  // namespace jxl

#endif  // LIB_EXTRAS_PACKED_IMAGE_H_
