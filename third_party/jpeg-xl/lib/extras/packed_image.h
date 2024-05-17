// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_EXTRAS_PACKED_IMAGE_H_
#define LIB_EXTRAS_PACKED_IMAGE_H_

// Helper class for storing external (int or float, interleaved) images. This is
// the common format used by other libraries and in the libjxl API.

#include <jxl/codestream_header.h>
#include <jxl/encode.h>
#include <jxl/types.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "lib/jxl/base/byte_order.h"
#include "lib/jxl/base/common.h"
#include "lib/jxl/base/status.h"

namespace jxl {
namespace extras {

// Class representing an interleaved image with a bunch of channels.
class PackedImage {
 public:
  static StatusOr<PackedImage> Create(size_t xsize, size_t ysize,
                                      const JxlPixelFormat& format) {
    PackedImage image(xsize, ysize, format, CalcStride(format, xsize));
    if (!image.pixels()) {
      // TODO(szabadka): use specialized OOM error code
      return JXL_FAILURE("Failed to allocate memory for image");
    }
    return image;
  }

  PackedImage Copy() const {
    PackedImage copy(xsize, ysize, format, CalcStride(format, xsize));
    memcpy(reinterpret_cast<uint8_t*>(copy.pixels()),
           reinterpret_cast<const uint8_t*>(pixels()), pixels_size);
    return copy;
  }

  // The interleaved pixels as defined in the storage format.
  void* pixels() const { return pixels_.get(); }

  uint8_t* pixels(size_t y, size_t x, size_t c) const {
    return (reinterpret_cast<uint8_t*>(pixels_.get()) + y * stride +
            x * pixel_stride_ + c * bytes_per_channel_);
  }

  const uint8_t* const_pixels(size_t y, size_t x, size_t c) const {
    return (reinterpret_cast<const uint8_t*>(pixels_.get()) + y * stride +
            x * pixel_stride_ + c * bytes_per_channel_);
  }

  // The image size in pixels.
  size_t xsize;
  size_t ysize;

  // The number of bytes per row.
  size_t stride;

  // Pixel storage format and buffer size of the pixels_ pointer.
  JxlPixelFormat format;
  size_t pixels_size;

  size_t pixel_stride() const { return pixel_stride_; }

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

  float GetPixelValue(size_t y, size_t x, size_t c) const {
    const uint8_t* data = const_pixels(y, x, c);
    switch (format.data_type) {
      case JXL_TYPE_UINT8:
        return data[0] * (1.0f / 255);
      case JXL_TYPE_UINT16: {
        uint16_t val;
        memcpy(&val, data, 2);
        return (swap_endianness_ ? JXL_BSWAP16(val) : val) * (1.0f / 65535);
      }
      case JXL_TYPE_FLOAT: {
        float val;
        memcpy(&val, data, 4);
        return swap_endianness_ ? BSwapFloat(val) : val;
      }
      default:
        JXL_ABORT("Unhandled JxlDataType");
    }
  }

  void SetPixelValue(size_t y, size_t x, size_t c, float val) const {
    uint8_t* data = pixels(y, x, c);
    switch (format.data_type) {
      case JXL_TYPE_UINT8:
        data[0] = Clamp1(std::round(val * 255), 0.0f, 255.0f);
        break;
      case JXL_TYPE_UINT16: {
        uint16_t val16 = Clamp1(std::round(val * 65535), 0.0f, 65535.0f);
        if (swap_endianness_) {
          val16 = JXL_BSWAP16(val16);
        }
        memcpy(data, &val16, 2);
        break;
      }
      case JXL_TYPE_FLOAT: {
        if (swap_endianness_) {
          val = BSwapFloat(val);
        }
        memcpy(data, &val, 4);
        break;
      }
      default:
        JXL_ABORT("Unhandled JxlDataType");
    }
  }

 private:
  PackedImage(size_t xsize, size_t ysize, const JxlPixelFormat& format,
              size_t stride)
      : xsize(xsize),
        ysize(ysize),
        stride(stride),
        format(format),
        pixels_size(ysize * stride),
        pixels_(malloc(std::max<size_t>(1, pixels_size)), free) {
    bytes_per_channel_ = BitsPerChannel(format.data_type) / jxl::kBitsPerByte;
    pixel_stride_ = format.num_channels * bytes_per_channel_;
    swap_endianness_ = SwapEndianness(format.endianness);
  }

  static size_t CalcStride(const JxlPixelFormat& format, size_t xsize) {
    size_t stride = xsize * (BitsPerChannel(format.data_type) *
                             format.num_channels / jxl::kBitsPerByte);
    if (format.align > 1) {
      stride = jxl::DivCeil(stride, format.align) * format.align;
    }
    return stride;
  }

  size_t bytes_per_channel_;
  size_t pixel_stride_;
  bool swap_endianness_;
  std::unique_ptr<void, decltype(free)*> pixels_;
};

// Helper class representing a frame, as seen from the API. Animations will have
// multiple frames, but a single frame can have a color/grayscale channel and
// multiple extra channels. The order of the extra channels should be the same
// as all other frames in the same image.
class PackedFrame {
 public:
  explicit PackedFrame(PackedImage&& image) : color(std::move(image)) {}

  static StatusOr<PackedFrame> Create(size_t xsize, size_t ysize,
                                      const JxlPixelFormat& format) {
    JXL_ASSIGN_OR_RETURN(PackedImage image,
                         PackedImage::Create(xsize, ysize, format));
    PackedFrame frame(std::move(image));
    return frame;
  }

  StatusOr<PackedFrame> Copy() const {
    JXL_ASSIGN_OR_RETURN(
        PackedFrame copy,
        PackedFrame::Create(color.xsize, color.ysize, color.format));
    copy.frame_info = frame_info;
    copy.name = name;
    copy.color = color.Copy();
    for (const auto& ec : extra_channels) {
      copy.extra_channels.emplace_back(ec.Copy());
    }
    return copy;
  }

  // The Frame metadata.
  JxlFrameHeader frame_info = {};
  std::string name;

  // The pixel data for the color (or grayscale) channels.
  PackedImage color;
  // Extra channel image data.
  std::vector<PackedImage> extra_channels;
};

class ChunkedPackedFrame {
 public:
  ChunkedPackedFrame(
      size_t xsize, size_t ysize,
      std::function<JxlChunkedFrameInputSource()> get_input_source)
      : xsize(xsize),
        ysize(ysize),
        get_input_source_(std::move(get_input_source)) {
    const auto input_source = get_input_source_();
    input_source.get_color_channels_pixel_format(input_source.opaque, &format);
  }

  JxlChunkedFrameInputSource GetInputSource() { return get_input_source_(); }

  // The Frame metadata.
  JxlFrameHeader frame_info = {};
  std::string name;

  size_t xsize;
  size_t ysize;
  JxlPixelFormat format;

 private:
  std::function<JxlChunkedFrameInputSource()> get_input_source_;
};

// Optional metadata associated with a file
class PackedMetadata {
 public:
  std::vector<uint8_t> exif;
  std::vector<uint8_t> iptc;
  std::vector<uint8_t> jumbf;
  std::vector<uint8_t> xmp;
};

// The extra channel metadata information.
struct PackedExtraChannel {
  JxlExtraChannelInfo ec_info;
  size_t index;
  std::string name;
};

// Helper class representing a JXL image file as decoded to pixels from the API.
class PackedPixelFile {
 public:
  JxlBasicInfo info = {};

  std::vector<PackedExtraChannel> extra_channels_info;

  // Color information of the decoded pixels.
  // `primary_color_representation` indicates whether `color_encoding` or `icc`
  // is the “authoritative” encoding of the colorspace, as opposed to a fallback
  // encoding. For example, if `color_encoding` is the primary one, as would
  // occur when decoding a jxl file with such a representation, then `enc/jxl`
  // will use it and ignore the ICC profile, whereas `enc/png` will include the
  // ICC profile for compatibility.
  // If `icc` is the primary representation, `enc/jxl` will preserve it when
  // compressing losslessly, but *may* encode it as a color_encoding when
  // compressing lossily.
  enum {
    kColorEncodingIsPrimary,
    kIccIsPrimary
  } primary_color_representation = kColorEncodingIsPrimary;
  JxlColorEncoding color_encoding = {};
  std::vector<uint8_t> icc;
  // The icc profile of the original image.
  std::vector<uint8_t> orig_icc;

  JxlBitDepth input_bitdepth = {JXL_BIT_DEPTH_FROM_PIXEL_FORMAT, 0, 0};

  std::unique_ptr<PackedFrame> preview_frame;
  std::vector<PackedFrame> frames;
  mutable std::vector<ChunkedPackedFrame> chunked_frames;

  PackedMetadata metadata;
  PackedPixelFile() { JxlEncoderInitBasicInfo(&info); };

  size_t num_frames() const {
    return chunked_frames.empty() ? frames.size() : chunked_frames.size();
  }
  size_t xsize() const { return info.xsize; }
  size_t ysize() const { return info.ysize; }
};

}  // namespace extras
}  // namespace jxl

#endif  // LIB_EXTRAS_PACKED_IMAGE_H_
