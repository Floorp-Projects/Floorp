// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/extras/enc/pnm.h"

#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "lib/extras/packed_image.h"
#include "lib/jxl/base/byte_order.h"
#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/file_io.h"
#include "lib/jxl/base/printf_macros.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/color_management.h"
#include "lib/jxl/dec_external_image.h"
#include "lib/jxl/enc_color_management.h"
#include "lib/jxl/enc_external_image.h"
#include "lib/jxl/enc_image_bundle.h"
#include "lib/jxl/fields.h"  // AllDefault
#include "lib/jxl/image.h"
#include "lib/jxl/image_bundle.h"

namespace jxl {
namespace extras {
namespace {

constexpr size_t kMaxHeaderSize = 200;

class PNMEncoder : public Encoder {
 public:
  Status Encode(const PackedPixelFile& ppf, EncodedImage* encoded_image,
                ThreadPool* pool = nullptr) const override {
    if (!ppf.metadata.exif.empty() || !ppf.metadata.iptc.empty() ||
        !ppf.metadata.jumbf.empty() || !ppf.metadata.xmp.empty()) {
      JXL_WARNING("PNM encoder ignoring metadata - use a different codec");
    }
    encoded_image->icc = ppf.icc;
    encoded_image->bitstreams.clear();
    encoded_image->bitstreams.reserve(ppf.frames.size());
    for (const auto& frame : ppf.frames) {
      encoded_image->bitstreams.emplace_back();
      JXL_RETURN_IF_ERROR(EncodeImagePNM(frame.color, ppf.info.orientation,
                                         ppf.info.bits_per_sample,
                                         &encoded_image->bitstreams.back()));
    }
    for (size_t i = 0; i < ppf.extra_channels_info.size(); ++i) {
      encoded_image->extra_channel_bitstreams.emplace_back();
      auto& ec_bitstreams = encoded_image->extra_channel_bitstreams.back();
      for (const auto& frame : ppf.frames) {
        ec_bitstreams.emplace_back();
        JXL_RETURN_IF_ERROR(
            EncodeImagePNM(frame.extra_channels[i], ppf.info.orientation,
                           ppf.info.bits_per_sample, &ec_bitstreams.back()));
      }
    }
    return true;
  }
};

class PPMEncoder : public PNMEncoder {
 public:
  std::vector<JxlPixelFormat> AcceptedFormats() const override {
    std::vector<JxlPixelFormat> formats;
    for (const uint32_t num_channels : {1, 2, 3, 4}) {
      for (const JxlDataType data_type : {JXL_TYPE_UINT8, JXL_TYPE_UINT16}) {
        for (JxlEndianness endianness : {JXL_BIG_ENDIAN, JXL_LITTLE_ENDIAN}) {
          formats.push_back(JxlPixelFormat{/*num_channels=*/num_channels,
                                           /*data_type=*/data_type,
                                           /*endianness=*/endianness,
                                           /*align=*/0});
        }
      }
    }
    return formats;
  }
};

class PFMEncoder : public PNMEncoder {
 public:
  std::vector<JxlPixelFormat> AcceptedFormats() const override {
    std::vector<JxlPixelFormat> formats;
    for (const uint32_t num_channels : {1, 3}) {
      for (const JxlDataType data_type : {JXL_TYPE_FLOAT16, JXL_TYPE_FLOAT}) {
        for (JxlEndianness endianness : {JXL_BIG_ENDIAN, JXL_LITTLE_ENDIAN}) {
          formats.push_back(JxlPixelFormat{/*num_channels=*/num_channels,
                                           /*data_type=*/data_type,
                                           /*endianness=*/endianness,
                                           /*align=*/0});
        }
      }
    }
    return formats;
  }
};

class PGMEncoder : public PPMEncoder {
 public:
  std::vector<JxlPixelFormat> AcceptedFormats() const override {
    std::vector<JxlPixelFormat> formats = PPMEncoder::AcceptedFormats();
    for (auto it = formats.begin(); it != formats.end();) {
      if (it->num_channels > 2) {
        it = formats.erase(it);
      } else {
        ++it;
      }
    }
    return formats;
  }
};

class PAMEncoder : public PPMEncoder {
 public:
  std::vector<JxlPixelFormat> AcceptedFormats() const override {
    std::vector<JxlPixelFormat> formats = PPMEncoder::AcceptedFormats();
    for (auto it = formats.begin(); it != formats.end();) {
      if (it->num_channels != 2 && it->num_channels != 4) {
        it = formats.erase(it);
      } else {
        ++it;
      }
    }
    return formats;
  }
};

Status EncodeHeader(const PackedImage& image, JxlOrientation orientation,
                    size_t bits_per_sample, bool little_endian, char* header,
                    int* chars_written) {
  size_t num_channels = image.format.num_channels;
  bool is_gray = num_channels <= 2;
  bool has_alpha = num_channels == 2 || num_channels == 4;
  size_t oriented_xsize = orientation <= 4 ? image.xsize : image.ysize;
  size_t oriented_ysize = orientation <= 4 ? image.ysize : image.xsize;
  if (has_alpha) {  // PAM
    if (bits_per_sample > 16) return JXL_FAILURE("PNM cannot have > 16 bits");
    const uint32_t max_val = (1U << bits_per_sample) - 1;
    *chars_written =
        snprintf(header, kMaxHeaderSize,
                 "P7\nWIDTH %" PRIuS "\nHEIGHT %" PRIuS
                 "\nDEPTH %u\nMAXVAL %u\nTUPLTYPE %s\nENDHDR\n",
                 oriented_xsize, oriented_ysize, is_gray ? 2 : 4, max_val,
                 is_gray ? "GRAYSCALE_ALPHA" : "RGB_ALPHA");
    JXL_RETURN_IF_ERROR(static_cast<unsigned int>(*chars_written) <
                        kMaxHeaderSize);
  } else if (bits_per_sample == 32) {  // PFM
    const char type = is_gray ? 'f' : 'F';
    const double scale = little_endian ? -1.0 : 1.0;
    *chars_written =
        snprintf(header, kMaxHeaderSize, "P%c\n%" PRIuS " %" PRIuS "\n%.1f\n",
                 type, oriented_xsize, oriented_ysize, scale);
    JXL_RETURN_IF_ERROR(static_cast<unsigned int>(*chars_written) <
                        kMaxHeaderSize);
  } else {  // PGM/PPM
    if (bits_per_sample > 16) return JXL_FAILURE("PNM cannot have > 16 bits");
    const uint32_t max_val = (1U << bits_per_sample) - 1;
    const char type = is_gray ? '5' : '6';
    *chars_written =
        snprintf(header, kMaxHeaderSize, "P%c\n%" PRIuS " %" PRIuS "\n%u\n",
                 type, oriented_xsize, oriented_ysize, max_val);
    JXL_RETURN_IF_ERROR(static_cast<unsigned int>(*chars_written) <
                        kMaxHeaderSize);
  }
  return true;
}

Span<const uint8_t> MakeSpan(const char* str) {
  return Span<const uint8_t>(reinterpret_cast<const uint8_t*>(str),
                             strlen(str));
}

// Flip the image vertically for loading/saving PFM files which have the
// scanlines inverted.
void VerticallyFlipImage(float* const float_image, const size_t xsize,
                         const size_t ysize, const size_t num_channels) {
  for (size_t y = 0; y < ysize / 2; y++) {
    float* first_row = &float_image[y * num_channels * xsize];
    float* other_row = &float_image[(ysize - y - 1) * num_channels * xsize];
    for (size_t c = 0; c < num_channels; c++) {
      for (size_t x = 0; x < xsize; ++x) {
        float tmp = first_row[x * num_channels + c];
        first_row[x * num_channels + c] = other_row[x * num_channels + c];
        other_row[x * num_channels + c] = tmp;
      }
    }
  }
}

}  // namespace

std::unique_ptr<Encoder> GetPPMEncoder() {
  return jxl::make_unique<PPMEncoder>();
}

std::unique_ptr<Encoder> GetPFMEncoder() {
  return jxl::make_unique<PFMEncoder>();
}

std::unique_ptr<Encoder> GetPGMEncoder() {
  return jxl::make_unique<PGMEncoder>();
}

std::unique_ptr<Encoder> GetPAMEncoder() {
  return jxl::make_unique<PAMEncoder>();
}

Status EncodeImagePNM(const PackedImage& image, JxlOrientation orientation,
                      size_t bits_per_sample, std::vector<uint8_t>* bytes) {
  // Choose native for PFM; PGM/PPM require big-endian
  bool is_little_endian = bits_per_sample > 16 && IsLittleEndian();
  char header[kMaxHeaderSize];
  int header_size = 0;
  JXL_RETURN_IF_ERROR(EncodeHeader(image, orientation, bits_per_sample,
                                   is_little_endian, header, &header_size));
  bytes->resize(static_cast<size_t>(header_size) + image.pixels_size);
  memcpy(bytes->data(), header, static_cast<size_t>(header_size));
  memcpy(bytes->data() + header_size, image.pixels(), image.pixels_size);
  if (bits_per_sample > 16) {
    VerticallyFlipImage(reinterpret_cast<float*>(bytes->data() + header_size),
                        image.xsize, image.ysize, image.format.num_channels);
  }

  return true;
}

}  // namespace extras
}  // namespace jxl
