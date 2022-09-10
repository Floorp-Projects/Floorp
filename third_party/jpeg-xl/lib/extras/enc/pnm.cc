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

Status EncodeHeader(const PackedImage& image, size_t bits_per_sample,
                    bool little_endian, char* header, int* chars_written) {
  size_t num_channels = image.format.num_channels;
  bool is_gray = num_channels <= 2;
  bool has_alpha = num_channels == 2 || num_channels == 4;
  if (has_alpha) {  // PAM
    if (bits_per_sample > 16) return JXL_FAILURE("PNM cannot have > 16 bits");
    const uint32_t max_val = (1U << bits_per_sample) - 1;
    *chars_written =
        snprintf(header, kMaxHeaderSize,
                 "P7\nWIDTH %" PRIuS "\nHEIGHT %" PRIuS
                 "\nDEPTH %u\nMAXVAL %u\nTUPLTYPE %s\nENDHDR\n",
                 image.xsize, image.ysize, is_gray ? 2 : 4, max_val,
                 is_gray ? "GRAYSCALE_ALPHA" : "RGB_ALPHA");
    JXL_RETURN_IF_ERROR(static_cast<unsigned int>(*chars_written) <
                        kMaxHeaderSize);
  } else if (bits_per_sample == 32) {  // PFM
    const char type = is_gray ? 'f' : 'F';
    const double scale = little_endian ? -1.0 : 1.0;
    *chars_written =
        snprintf(header, kMaxHeaderSize, "P%c\n%" PRIuS " %" PRIuS "\n%.1f\n",
                 type, image.xsize, image.ysize, scale);
    JXL_RETURN_IF_ERROR(static_cast<unsigned int>(*chars_written) <
                        kMaxHeaderSize);
  } else {  // PGM/PPM
    if (bits_per_sample > 16) return JXL_FAILURE("PNM cannot have > 16 bits");
    const uint32_t max_val = (1U << bits_per_sample) - 1;
    const char type = is_gray ? '5' : '6';
    *chars_written =
        snprintf(header, kMaxHeaderSize, "P%c\n%" PRIuS " %" PRIuS "\n%u\n",
                 type, image.xsize, image.ysize, max_val);
    JXL_RETURN_IF_ERROR(static_cast<unsigned int>(*chars_written) <
                        kMaxHeaderSize);
  }
  return true;
}

Status EncodeImagePNM(const PackedImage& image, size_t bits_per_sample,
                      std::vector<uint8_t>* bytes) {
  if (bits_per_sample <= 16 && image.format.endianness != JXL_BIG_ENDIAN) {
    return JXL_FAILURE("PPM/PGM requires big-endian pixel format.");
  }
  bool is_little_endian =
      (image.format.endianness == JXL_LITTLE_ENDIAN ||
       (image.format.endianness == JXL_NATIVE_ENDIAN && IsLittleEndian()));
  char header[kMaxHeaderSize];
  int header_size = 0;
  JXL_RETURN_IF_ERROR(EncodeHeader(image, bits_per_sample, is_little_endian,
                                   header, &header_size));
  bytes->resize(static_cast<size_t>(header_size) + image.pixels_size);
  memcpy(bytes->data(), header, static_cast<size_t>(header_size));
  const bool flipped_y = bits_per_sample == 32;  // PFMs are flipped
  const uint8_t* in = reinterpret_cast<const uint8_t*>(image.pixels());
  uint8_t* out = bytes->data() + header_size;
  for (size_t y = 0; y < image.ysize; ++y) {
    size_t y_out = flipped_y ? image.ysize - 1 - y : y;
    const uint8_t* row_in = &in[y * image.stride];
    uint8_t* row_out = &out[y_out * image.stride];
    memcpy(row_out, row_in, image.stride);
  }
  return true;
}

class PNMEncoder : public Encoder {
 public:
  Status Encode(const PackedPixelFile& ppf, EncodedImage* encoded_image,
                ThreadPool* pool = nullptr) const override {
    JXL_RETURN_IF_ERROR(VerifyBasicInfo(ppf.info));
    if (!ppf.metadata.exif.empty() || !ppf.metadata.iptc.empty() ||
        !ppf.metadata.jumbf.empty() || !ppf.metadata.xmp.empty()) {
      JXL_WARNING("PNM encoder ignoring metadata - use a different codec");
    }
    encoded_image->icc = ppf.icc;
    encoded_image->bitstreams.clear();
    encoded_image->bitstreams.reserve(ppf.frames.size());
    for (const auto& frame : ppf.frames) {
      JXL_RETURN_IF_ERROR(VerifyPackedImage(frame.color, ppf.info));
      encoded_image->bitstreams.emplace_back();
      JXL_RETURN_IF_ERROR(EncodeImagePNM(frame.color, ppf.info.bits_per_sample,
                                         &encoded_image->bitstreams.back()));
    }
    for (size_t i = 0; i < ppf.extra_channels_info.size(); ++i) {
      const auto& ec_info = ppf.extra_channels_info[i].ec_info;
      encoded_image->extra_channel_bitstreams.emplace_back();
      auto& ec_bitstreams = encoded_image->extra_channel_bitstreams.back();
      for (const auto& frame : ppf.frames) {
        ec_bitstreams.emplace_back();
        JXL_RETURN_IF_ERROR(EncodeImagePNM(frame.extra_channels[i],
                                           ec_info.bits_per_sample,
                                           &ec_bitstreams.back()));
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
        for (JxlEndianness endianness : {JXL_BIG_ENDIAN}) {
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

Span<const uint8_t> MakeSpan(const char* str) {
  return Span<const uint8_t>(reinterpret_cast<const uint8_t*>(str),
                             strlen(str));
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

}  // namespace extras
}  // namespace jxl
