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

#include "lib/extras/codec_pnm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "lib/jxl/base/bits.h"
#include "lib/jxl/base/byte_order.h"
#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/file_io.h"
#include "lib/jxl/color_management.h"
#include "lib/jxl/dec_external_image.h"
#include "lib/jxl/enc_external_image.h"
#include "lib/jxl/fields.h"  // AllDefault
#include "lib/jxl/image.h"
#include "lib/jxl/image_bundle.h"
#include "lib/jxl/image_ops.h"
#include "lib/jxl/luminance.h"

namespace jxl {
namespace {

struct HeaderPNM {
  size_t xsize;
  size_t ysize;
  bool is_bit;   // PBM
  bool is_gray;  // PGM
  size_t bits_per_sample;
  bool floating_point;
  bool big_endian;
};

class Parser {
 public:
  explicit Parser(const Span<const uint8_t> bytes)
      : pos_(bytes.data()), end_(pos_ + bytes.size()) {}

  // Sets "pos" to the first non-header byte/pixel on success.
  Status ParseHeader(HeaderPNM* header, const uint8_t** pos) {
    // codec.cc ensures we have at least two bytes => no range check here.
    if (pos_[0] != 'P') return false;
    const uint8_t type = pos_[1];
    pos_ += 2;

    header->is_bit = false;

    switch (type) {
      case '4':
        header->is_bit = true;
        header->is_gray = true;
        header->bits_per_sample = 1;
        return ParseHeaderPNM(header, pos);

      case '5':
        header->is_gray = true;
        return ParseHeaderPNM(header, pos);

      case '6':
        header->is_gray = false;
        return ParseHeaderPNM(header, pos);

      case 'F':
        header->is_gray = false;
        return ParseHeaderPFM(header, pos);

      case 'f':
        header->is_gray = true;
        return ParseHeaderPFM(header, pos);
    }
    return false;
  }

  // Exposed for testing
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

  Status ParseSigned(double* number) {
    if (pos_ == end_) return JXL_FAILURE("PNM: reached end before signed");

    if (*pos_ != '-' && *pos_ != '+' && !IsDigit(*pos_)) {
      return JXL_FAILURE("PNM: expected signed number");
    }

    // Skip sign
    const bool is_neg = *pos_ == '-';
    if (is_neg || *pos_ == '+') {
      ++pos_;
      if (pos_ == end_) return JXL_FAILURE("PNM: reached end before digits");
    }

    // Leading digits
    *number = 0.0;
    while (pos_ < end_ && *pos_ >= '0' && *pos_ <= '9') {
      *number *= 10;
      *number += *pos_ - '0';
      ++pos_;
    }

    // Decimal places?
    if (pos_ < end_ && *pos_ == '.') {
      ++pos_;
      double place = 0.1;
      while (pos_ < end_ && *pos_ >= '0' && *pos_ <= '9') {
        *number += (*pos_ - '0') * place;
        place *= 0.1;
        ++pos_;
      }
    }

    if (is_neg) *number = -*number;
    return true;
  }

 private:
  static bool IsDigit(const uint8_t c) { return '0' <= c && c <= '9'; }
  static bool IsLineBreak(const uint8_t c) { return c == '\r' || c == '\n'; }
  static bool IsWhitespace(const uint8_t c) {
    return IsLineBreak(c) || c == '\t' || c == ' ';
  }

  Status SkipBlank() {
    if (pos_ == end_) return JXL_FAILURE("PNM: reached end before blank");
    const uint8_t c = *pos_;
    if (c != ' ' && c != '\n') return JXL_FAILURE("PNM: expected blank");
    ++pos_;
    return true;
  }

  Status SkipSingleWhitespace() {
    if (pos_ == end_) return JXL_FAILURE("PNM: reached end before whitespace");
    if (!IsWhitespace(*pos_)) return JXL_FAILURE("PNM: expected whitespace");
    ++pos_;
    return true;
  }

  Status SkipWhitespace() {
    if (pos_ == end_) return JXL_FAILURE("PNM: reached end before whitespace");
    if (!IsWhitespace(*pos_) && *pos_ != '#') {
      return JXL_FAILURE("PNM: expected whitespace/comment");
    }

    while (pos_ < end_ && IsWhitespace(*pos_)) {
      ++pos_;
    }

    // Comment(s)
    while (pos_ != end_ && *pos_ == '#') {
      while (pos_ != end_ && !IsLineBreak(*pos_)) {
        ++pos_;
      }
      // Newline(s)
      while (pos_ != end_ && IsLineBreak(*pos_)) pos_++;
    }

    while (pos_ < end_ && IsWhitespace(*pos_)) {
      ++pos_;
    }
    return true;
  }

  Status ParseHeaderPNM(HeaderPNM* header, const uint8_t** pos) {
    JXL_RETURN_IF_ERROR(SkipWhitespace());
    JXL_RETURN_IF_ERROR(ParseUnsigned(&header->xsize));

    JXL_RETURN_IF_ERROR(SkipWhitespace());
    JXL_RETURN_IF_ERROR(ParseUnsigned(&header->ysize));

    if (!header->is_bit) {
      JXL_RETURN_IF_ERROR(SkipWhitespace());
      size_t max_val;
      JXL_RETURN_IF_ERROR(ParseUnsigned(&max_val));
      if (max_val == 0 || max_val >= 65536) {
        return JXL_FAILURE("PNM: bad MaxVal");
      }
      header->bits_per_sample = CeilLog2Nonzero(max_val);
    }
    header->floating_point = false;
    header->big_endian = true;

    JXL_RETURN_IF_ERROR(SkipSingleWhitespace());

    *pos = pos_;
    return true;
  }

  Status ParseHeaderPFM(HeaderPNM* header, const uint8_t** pos) {
    JXL_RETURN_IF_ERROR(SkipSingleWhitespace());
    JXL_RETURN_IF_ERROR(ParseUnsigned(&header->xsize));

    JXL_RETURN_IF_ERROR(SkipBlank());
    JXL_RETURN_IF_ERROR(ParseUnsigned(&header->ysize));

    JXL_RETURN_IF_ERROR(SkipSingleWhitespace());
    // The scale has no meaning as multiplier, only its sign is used to
    // indicate endianness. All software expects nominal range 0..1.
    double scale;
    JXL_RETURN_IF_ERROR(ParseSigned(&scale));
    header->big_endian = scale >= 0.0;
    header->bits_per_sample = 32;
    header->floating_point = true;

    JXL_RETURN_IF_ERROR(SkipSingleWhitespace());

    *pos = pos_;
    return true;
  }

  const uint8_t* pos_;
  const uint8_t* const end_;
};

constexpr size_t kMaxHeaderSize = 200;

Status EncodeHeader(const ImageBundle& ib, const size_t bits_per_sample,
                    const bool little_endian, char* header,
                    int* JXL_RESTRICT chars_written) {
  if (ib.HasAlpha()) return JXL_FAILURE("PNM: can't store alpha");

  if (bits_per_sample == 32) {  // PFM
    const char type = ib.IsGray() ? 'f' : 'F';
    const double scale = little_endian ? -1.0 : 1.0;
    snprintf(header, kMaxHeaderSize, "P%c\n%zu %zu\n%.1f\n%n", type,
             ib.oriented_xsize(), ib.oriented_ysize(), scale, chars_written);
  } else if (bits_per_sample == 1) {  // PBM
    if (!ib.IsGray()) {
      return JXL_FAILURE("Cannot encode color as PBM");
    }
    snprintf(header, kMaxHeaderSize, "P4\n%zu %zu\n%n", ib.oriented_xsize(),
             ib.oriented_ysize(), chars_written);
  } else {  // PGM/PPM
    const uint32_t max_val = (1U << bits_per_sample) - 1;
    if (max_val >= 65536) return JXL_FAILURE("PNM cannot have > 16 bits");
    const char type = ib.IsGray() ? '5' : '6';
    snprintf(header, kMaxHeaderSize, "P%c\n%zu %zu\n%u\n%n", type,
             ib.oriented_xsize(), ib.oriented_ysize(), max_val, chars_written);
  }
  return true;
}

Status ApplyHints(const bool is_gray, CodecInOut* io) {
  bool got_color_space = false;

  JXL_RETURN_IF_ERROR(io->dec_hints.Foreach(
      [is_gray, io, &got_color_space](const std::string& key,
                                      const std::string& value) -> Status {
        ColorEncoding* c_original = &io->metadata.m.color_encoding;
        if (key == "color_space") {
          if (!ParseDescription(value, c_original) ||
              !c_original->CreateICC()) {
            return JXL_FAILURE("PNM: Failed to apply color_space");
          }

          if (is_gray != io->metadata.m.color_encoding.IsGray()) {
            return JXL_FAILURE(
                "PNM: mismatch between file and color_space hint");
          }

          got_color_space = true;
        } else if (key == "icc_pathname") {
          PaddedBytes icc;
          JXL_RETURN_IF_ERROR(ReadFile(value, &icc));
          JXL_RETURN_IF_ERROR(c_original->SetICC(std::move(icc)));
          got_color_space = true;
        } else {
          JXL_WARNING("PNM decoder ignoring %s hint", key.c_str());
        }
        return true;
      }));

  if (!got_color_space) {
    JXL_WARNING("PNM: no color_space/icc_pathname given, assuming sRGB");
    JXL_RETURN_IF_ERROR(io->metadata.m.color_encoding.SetSRGB(
        is_gray ? ColorSpace::kGray : ColorSpace::kRGB));
  }

  return true;
}

Span<const uint8_t> MakeSpan(const char* str) {
  return Span<const uint8_t>(reinterpret_cast<const uint8_t*>(str),
                             strlen(str));
}

// Flip the image vertically for loading/saving PFM files which have the
// scanlines inverted.
void VerticallyFlipImage(Image3F* const image) {
  for (int c = 0; c < 3; c++) {
    for (size_t y = 0; y < image->ysize() / 2; y++) {
      float* first_row = image->PlaneRow(c, y);
      float* other_row = image->PlaneRow(c, image->ysize() - y - 1);
      for (size_t x = 0; x < image->xsize(); ++x) {
        float tmp = first_row[x];
        first_row[x] = other_row[x];
        other_row[x] = tmp;
      }
    }
  }
}

}  // namespace

Status DecodeImagePNM(const Span<const uint8_t> bytes, ThreadPool* pool,
                      CodecInOut* io) {
  Parser parser(bytes);
  HeaderPNM header = {};
  const uint8_t* pos = nullptr;
  if (!parser.ParseHeader(&header, &pos)) return false;
  JXL_RETURN_IF_ERROR(
      VerifyDimensions(&io->constraints, header.xsize, header.ysize));

  if (header.bits_per_sample == 0 || header.bits_per_sample > 32) {
    return JXL_FAILURE("PNM: bits_per_sample invalid");
  }

  JXL_RETURN_IF_ERROR(ApplyHints(header.is_gray, io));
  if (header.floating_point) {
    io->metadata.m.SetFloat32Samples();
  } else {
    io->metadata.m.SetUintSamples(header.bits_per_sample);
  }
  io->metadata.m.SetAlphaBits(0);
  io->dec_pixels = header.xsize * header.ysize;

  const bool flipped_y = header.bits_per_sample == 32;  // PFMs are flipped
  const Span<const uint8_t> span(pos, bytes.data() + bytes.size() - pos);
  JXL_RETURN_IF_ERROR(ConvertFromExternal(
      span, header.xsize, header.ysize, io->metadata.m.color_encoding,
      /*has_alpha=*/false, /*alpha_is_premultiplied=*/false,
      io->metadata.m.bit_depth.bits_per_sample,
      header.big_endian ? JXL_BIG_ENDIAN : JXL_LITTLE_ENDIAN, flipped_y, pool,
      &io->Main()));
  if (!header.floating_point) {
    io->metadata.m.bit_depth.bits_per_sample = io->Main().DetectRealBitdepth();
  }
  io->SetSize(header.xsize, header.ysize);
  SetIntensityTarget(io);
  return true;
}

Status EncodeImagePNM(const CodecInOut* io, const ColorEncoding& c_desired,
                      size_t bits_per_sample, ThreadPool* pool,
                      PaddedBytes* bytes) {
  const bool floating_point = bits_per_sample > 16;
  // Choose native for PFM; PGM/PPM require big-endian (N/A for PBM)
  const JxlEndianness endianness =
      floating_point ? JXL_NATIVE_ENDIAN : JXL_BIG_ENDIAN;

  ImageMetadata metadata_copy = io->metadata.m;
  // AllDefault sets all_default, which can cause a race condition.
  if (!Bundle::AllDefault(metadata_copy)) {
    JXL_WARNING("PNM encoder ignoring metadata - use a different codec");
  }
  if (!c_desired.IsSRGB()) {
    JXL_WARNING(
        "PNM encoder cannot store custom ICC profile; decoder\n"
        "will need hint key=color_space to get the same values");
  }

  ImageBundle ib = io->Main().Copy();
  // In case of PFM the image must be flipped upside down since that format
  // is designed that way.
  const ImageBundle* to_color_transform = &ib;
  ImageBundle flipped;
  if (floating_point) {
    flipped = ib.Copy();
    VerticallyFlipImage(flipped.color());
    to_color_transform = &flipped;
  }
  ImageMetadata metadata = io->metadata.m;
  ImageBundle store(&metadata);
  const ImageBundle* transformed;
  JXL_RETURN_IF_ERROR(TransformIfNeeded(*to_color_transform, c_desired, pool,
                                        &store, &transformed));
  size_t stride = ib.oriented_xsize() *
                  (c_desired.Channels() * bits_per_sample) / kBitsPerByte;
  PaddedBytes pixels(stride * ib.oriented_ysize());
  JXL_RETURN_IF_ERROR(ConvertToExternal(
      *transformed, bits_per_sample, floating_point, c_desired.Channels(),
      endianness, stride, pool, pixels.data(), pixels.size(),
      /*out_callback=*/nullptr, /*out_opaque=*/nullptr,
      metadata.GetOrientation()));

  char header[kMaxHeaderSize];
  int header_size = 0;
  bool is_little_endian = endianness == JXL_LITTLE_ENDIAN ||
                          (endianness == JXL_NATIVE_ENDIAN && IsLittleEndian());
  JXL_RETURN_IF_ERROR(EncodeHeader(*transformed, bits_per_sample,
                                   is_little_endian, header, &header_size));

  bytes->resize(static_cast<size_t>(header_size) + pixels.size());
  memcpy(bytes->data(), header, static_cast<size_t>(header_size));
  memcpy(bytes->data() + header_size, pixels.data(), pixels.size());

  return true;
}

void TestCodecPNM() {
  size_t u = 77777;  // Initialized to wrong value.
  double d = 77.77;
// Failing to parse invalid strings results in a crash if `JXL_CRASH_ON_ERROR`
// is defined and hence the tests fail. Therefore we only run these tests if
// `JXL_CRASH_ON_ERROR` is not defined.
#ifndef JXL_CRASH_ON_ERROR
  JXL_CHECK(false == Parser(MakeSpan("")).ParseUnsigned(&u));
  JXL_CHECK(false == Parser(MakeSpan("+")).ParseUnsigned(&u));
  JXL_CHECK(false == Parser(MakeSpan("-")).ParseUnsigned(&u));
  JXL_CHECK(false == Parser(MakeSpan("A")).ParseUnsigned(&u));

  JXL_CHECK(false == Parser(MakeSpan("")).ParseSigned(&d));
  JXL_CHECK(false == Parser(MakeSpan("+")).ParseSigned(&d));
  JXL_CHECK(false == Parser(MakeSpan("-")).ParseSigned(&d));
  JXL_CHECK(false == Parser(MakeSpan("A")).ParseSigned(&d));
#endif
  JXL_CHECK(true == Parser(MakeSpan("1")).ParseUnsigned(&u));
  JXL_CHECK(u == 1);

  JXL_CHECK(true == Parser(MakeSpan("32")).ParseUnsigned(&u));
  JXL_CHECK(u == 32);

  JXL_CHECK(true == Parser(MakeSpan("1")).ParseSigned(&d));
  JXL_CHECK(d == 1.0);
  JXL_CHECK(true == Parser(MakeSpan("+2")).ParseSigned(&d));
  JXL_CHECK(d == 2.0);
  JXL_CHECK(true == Parser(MakeSpan("-3")).ParseSigned(&d));
  JXL_CHECK(std::abs(d - -3.0) < 1E-15);
  JXL_CHECK(true == Parser(MakeSpan("3.141592")).ParseSigned(&d));
  JXL_CHECK(std::abs(d - 3.141592) < 1E-15);
  JXL_CHECK(true == Parser(MakeSpan("-3.141592")).ParseSigned(&d));
  JXL_CHECK(std::abs(d - -3.141592) < 1E-15);
}

}  // namespace jxl
