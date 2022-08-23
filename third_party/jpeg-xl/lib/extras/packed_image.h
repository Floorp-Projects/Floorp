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

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "jxl/codestream_header.h"
#include "jxl/encode.h"
#include "jxl/types.h"
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
        pixels_(malloc(std::max<size_t>(1, pixels_size)), free) {}
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

  // Whether the range is determined by format or by JxlBasicInfo
  // e.g. if format is UINT16 and JxlBasicInfo bits_per_sample is 10,
  // then if bitdepth_from_format == true, the range is 0..65535
  // while if bitdepth_from_format == false, the range is 0..1023.
  bool bitdepth_from_format = true;

  // The number of bytes per row.
  size_t stride;

  // Pixel storage format and buffer size of the pixels_ pointer.
  JxlPixelFormat format;
  size_t pixels_size;

  size_t pixel_stride() const {
    return (BitsPerChannel(format.data_type) * format.num_channels /
            jxl::kBitsPerByte);
  }

  static size_t BitsPerChannel(JxlDataType data_type) {
    switch (data_type) {
      case JXL_TYPE_UINT8:
        return 8;
      case JXL_TYPE_UINT16:
        return 16;
      case JXL_TYPE_FLOAT:
        return 32;
      case JXL_TYPE_FLOAT16:
        return 16;
      default:
        JXL_ABORT("Unhandled JxlDataType");
    }
  }

 private:
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
  explicit PackedFrame(Args&&... args) : color(std::forward<Args>(args)...) {}

  // The Frame metadata.
  JxlFrameHeader frame_info = {};
  std::string name;

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
    JxlExtraChannelInfo ec_info;
    size_t index;
    std::string name;
  };
  std::vector<PackedExtraChannel> extra_channels_info;

  // Color information of the decoded pixels.
  // If the icc is empty, the JxlColorEncoding should be used instead.
  std::vector<uint8_t> icc;
  JxlColorEncoding color_encoding = {};
  // The icc profile of the original image.
  std::vector<uint8_t> orig_icc;

  std::unique_ptr<PackedFrame> preview_frame;
  std::vector<PackedFrame> frames;

  PackedMetadata metadata;
  PackedPixelFile() { JxlEncoderInitBasicInfo(&info); };
};

}  // namespace extras
}  // namespace jxl

#endif  // LIB_EXTRAS_PACKED_IMAGE_H_
