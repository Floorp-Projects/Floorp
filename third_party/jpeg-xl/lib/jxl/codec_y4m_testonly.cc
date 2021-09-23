// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/codec_y4m_testonly.h"

#include <stddef.h>

namespace jxl {
namespace test {

struct HeaderY4M {
  size_t xsize;
  size_t ysize;
  size_t bits_per_sample;
  int is_yuv;  // Y4M: where 1 = 444, 2 = 422, 3 = 420
};

// Decode Y4M images.
class Y4MParser {
 public:
  explicit Y4MParser(const Span<const uint8_t> bytes)
      : pos_(bytes.data()), end_(pos_ + bytes.size()) {}

  // TODO(jon): support multi-frame y4m
  Status ParseHeader(HeaderY4M* header, const uint8_t** pos) {
    JXL_RETURN_IF_ERROR(ExpectString("YUV4MPEG2", 9));
    header->is_yuv = 3;
    // TODO(jon): check if 4:2:0 is indeed the default
    header->bits_per_sample = 8;
    // TODO(jon): check if there's a y4m convention for higher bit depths
    while (pos_ < end_) {
      char next = 0;
      JXL_RETURN_IF_ERROR(ReadChar(&next));
      if (next == 0x0A) break;
      if (next != ' ') continue;
      char field = 0;
      JXL_RETURN_IF_ERROR(ReadChar(&field));
      switch (field) {
        case 'W':
          JXL_RETURN_IF_ERROR(ParseUnsigned(&header->xsize));
          break;
        case 'H':
          JXL_RETURN_IF_ERROR(ParseUnsigned(&header->ysize));
          break;
        case 'I':
          JXL_RETURN_IF_ERROR(ReadChar(&next));
          if (next != 'p') {
            return JXL_FAILURE(
                "Y4M: only progressive (no frame interlacing) allowed");
          }
          break;
        case 'C': {
          char c1 = 0;
          JXL_RETURN_IF_ERROR(ReadChar(&c1));
          char c2 = 0;
          JXL_RETURN_IF_ERROR(ReadChar(&c2));
          char c3 = 0;
          JXL_RETURN_IF_ERROR(ReadChar(&c3));
          if (c1 != '4') return JXL_FAILURE("Y4M: invalid C param");
          if (c2 == '4') {
            if (c3 != '4') return JXL_FAILURE("Y4M: invalid C param");
            header->is_yuv = 1;  // 444
          } else if (c2 == '2') {
            if (c3 == '2') {
              header->is_yuv = 2;  // 422
            } else if (c3 == '0') {
              header->is_yuv = 3;  // 420
            } else {
              return JXL_FAILURE("Y4M: invalid C param");
            }
          } else {
            return JXL_FAILURE("Y4M: invalid C param");
          }
        }
          [[fallthrough]];
          // no break: fallthrough because this field can have values like
          // "C420jpeg" (we are ignoring the chroma sample location and treat
          // everything like C420jpeg)
        case 'F':  // Framerate in fps as numerator:denominator
                   // TODO(jon): actually read this and set corresponding jxl
                   // metadata
        case 'A':  // Pixel aspect ratio (ignoring it, could perhaps adjust
                   // intrinsic dimensions based on this?)
        case 'X':  // Comment, ignore
          // ignore the field value and go to next one
          while (pos_ < end_) {
            if (pos_[0] == ' ' || pos_[0] == 0x0A) break;
            pos_++;
          }
          break;
        default:
          return JXL_FAILURE("Y4M: parse error");
      }
    }
    JXL_RETURN_IF_ERROR(ExpectString("FRAME", 5));
    while (true) {
      char next = 0;
      JXL_RETURN_IF_ERROR(ReadChar(&next));
      if (next == 0x0A) {
        *pos = pos_;
        return true;
      }
    }
  }

 private:
  Status ExpectString(const char* str, size_t len) {
    // Unlikely to happen.
    if (pos_ + len < pos_) return JXL_FAILURE("Y4M: overflow");

    if (pos_ + len > end_ || strncmp(str, (const char*)pos_, len) != 0) {
      return JXL_FAILURE("Y4M: expected %s", str);
    }
    pos_ += len;
    return true;
  }

  Status ReadChar(char* out) {
    // Unlikely to happen.
    if (pos_ + 1 < pos_) return JXL_FAILURE("Y4M: overflow");

    if (pos_ >= end_) {
      return JXL_FAILURE("Y4M: unexpected end of input");
    }
    *out = *pos_;
    pos_++;
    return true;
  }

  static bool IsDigit(const uint8_t c) { return '0' <= c && c <= '9'; }

  Status ParseUnsigned(size_t* number) {
    if (pos_ == end_) return JXL_FAILURE("PNM: reached end before number");
    if (!IsDigit(*pos_)) return JXL_FAILURE("PNM: expected unsigned number");

    *number = 0;
    while (pos_ < end_ && *pos_ >= '0' && *pos_ <= '9') {
      *number *= 10;
      *number += *pos_ - '0';
      ++pos_;
    }

    return true;
  }

  const uint8_t* pos_;
  const uint8_t* const end_;
};

Status DecodeImageY4M(const Span<const uint8_t> bytes, CodecInOut* io) {
  Y4MParser parser(bytes);
  HeaderY4M header = {};
  const uint8_t* pos = nullptr;
  JXL_RETURN_IF_ERROR(parser.ParseHeader(&header, &pos));

  Image3F yuvdata(header.xsize, header.ysize);
  ImageBundle bundle(&io->metadata.m);
  const int hshift[3][3] = {{0, 0, 0}, {0, 1, 1}, {0, 1, 1}};
  const int vshift[3][3] = {{0, 0, 0}, {0, 0, 0}, {0, 1, 1}};

  for (size_t c = 0; c < 3; c++) {
    for (size_t y = 0; y < header.ysize >> vshift[header.is_yuv - 1][c]; ++y) {
      float* const JXL_RESTRICT row = yuvdata.PlaneRow((c == 2 ? 2 : 1 - c), y);
      if (pos + (header.xsize >> hshift[header.is_yuv - 1][c]) >
          bytes.data() + bytes.size())
        return JXL_FAILURE("Not enough image data");
      for (size_t x = 0; x < header.xsize >> hshift[header.is_yuv - 1][c];
           ++x) {
        row[x] = (1.f / 255.f) * ((*pos++) - 128.f);
      }
    }
  }
  bundle.SetFromImage(std::move(yuvdata), io->metadata.m.color_encoding);
  bundle.color_transform = ColorTransform::kYCbCr;

  YCbCrChromaSubsampling subsampling;
  uint8_t cssh[3] = {
      2, static_cast<uint8_t>(hshift[header.is_yuv - 1][1] ? 1 : 2),
      static_cast<uint8_t>(hshift[header.is_yuv - 1][2] ? 1 : 2)};
  uint8_t cssv[3] = {
      2, static_cast<uint8_t>(vshift[header.is_yuv - 1][1] ? 1 : 2),
      static_cast<uint8_t>(vshift[header.is_yuv - 1][2] ? 1 : 2)};

  JXL_RETURN_IF_ERROR(subsampling.Set(cssh, cssv));
  bundle.chroma_subsampling = subsampling;
  io->Main() = std::move(bundle);

  JXL_RETURN_IF_ERROR(io->metadata.m.color_encoding.SetSRGB(ColorSpace::kRGB));
  io->metadata.m.SetUintSamples(header.bits_per_sample);
  io->metadata.m.SetAlphaBits(0);
  io->dec_pixels = header.xsize * header.ysize;

  io->metadata.m.bit_depth.bits_per_sample = io->Main().DetectRealBitdepth();
  io->SetSize(header.xsize, header.ysize);
  SetIntensityTarget(io);
  return true;
}

}  // namespace test
}  // namespace jxl
