// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/extras/dec/apng.h"

// Parts of this code are taken from apngdis, which has the following license:
/* APNG Disassembler 2.8
 *
 * Deconstructs APNG files into individual frames.
 *
 * http://apngdis.sourceforge.net
 *
 * Copyright (c) 2010-2015 Max Stepin
 * maxst at users.sourceforge.net
 *
 * zlib license
 * ------------
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 */

#include <jxl/codestream_header.h>
#include <jxl/encode.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "lib/extras/packed_image.h"
#include "lib/extras/size_constraints.h"
#include "lib/jxl/base/byte_order.h"
#include "lib/jxl/base/common.h"
#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/printf_macros.h"
#include "lib/jxl/base/rect.h"
#include "lib/jxl/base/span.h"
#include "lib/jxl/base/status.h"
#if JPEGXL_ENABLE_APNG
#include "png.h" /* original (unpatched) libpng is ok */
#endif

namespace jxl {
namespace extras {

#if !JPEGXL_ENABLE_APNG

bool CanDecodeAPNG() { return false; }
Status DecodeImageAPNG(const Span<const uint8_t> bytes,
                       const ColorHints& color_hints, PackedPixelFile* ppf,
                       const SizeConstraints* constraints) {
  return false;
}

#else  // JPEGXL_ENABLE_APNG

namespace {

constexpr std::array<uint8_t, 8> kPngSignature = {137,  'P',  'N', 'G',
                                                  '\r', '\n', 26,  '\n'};

// Returns floating-point value from the PNG encoding (times 10^5).
double F64FromU32(const uint32_t x) { return static_cast<int32_t>(x) * 1E-5; }

/** Extract information from 'sRGB' chunk. */
Status DecodeSrgbChunk(const Bytes payload, JxlColorEncoding* color_encoding) {
  if (payload.size() != 1) return JXL_FAILURE("Wrong sRGB size");
  uint8_t ri = payload[0];
  // (PNG uses the same values as ICC.)
  if (ri >= 4) return JXL_FAILURE("Invalid Rendering Intent");
  color_encoding->white_point = JXL_WHITE_POINT_D65;
  color_encoding->primaries = JXL_PRIMARIES_SRGB;
  color_encoding->transfer_function = JXL_TRANSFER_FUNCTION_SRGB;
  color_encoding->rendering_intent = static_cast<JxlRenderingIntent>(ri);
  return true;
}

/**
 * Extract information from 'cICP' chunk.
 *
 * If the cICP profile is not fully supported, return `false` and leave
 * `color_encoding` unmodified.
 */
Status DecodeCicpChunk(const Bytes payload, JxlColorEncoding* color_encoding) {
  if (payload.size() != 4) return JXL_FAILURE("Wrong cICP size");
  JxlColorEncoding color_enc = *color_encoding;

  // From https://www.itu.int/rec/T-REC-H.273-202107-I/en
  if (payload[0] == 1) {
    // IEC 61966-2-1 sRGB
    color_enc.primaries = JXL_PRIMARIES_SRGB;
    color_enc.white_point = JXL_WHITE_POINT_D65;
  } else if (payload[0] == 4) {
    // Rec. ITU-R BT.470-6 System M
    color_enc.primaries = JXL_PRIMARIES_CUSTOM;
    color_enc.primaries_red_xy[0] = 0.67;
    color_enc.primaries_red_xy[1] = 0.33;
    color_enc.primaries_green_xy[0] = 0.21;
    color_enc.primaries_green_xy[1] = 0.71;
    color_enc.primaries_blue_xy[0] = 0.14;
    color_enc.primaries_blue_xy[1] = 0.08;
    color_enc.white_point = JXL_WHITE_POINT_CUSTOM;
    color_enc.white_point_xy[0] = 0.310;
    color_enc.white_point_xy[1] = 0.316;
  } else if (payload[0] == 5) {
    // Rec. ITU-R BT.1700-0 625 PAL and 625 SECAM
    color_enc.primaries = JXL_PRIMARIES_CUSTOM;
    color_enc.primaries_red_xy[0] = 0.64;
    color_enc.primaries_red_xy[1] = 0.33;
    color_enc.primaries_green_xy[0] = 0.29;
    color_enc.primaries_green_xy[1] = 0.60;
    color_enc.primaries_blue_xy[0] = 0.15;
    color_enc.primaries_blue_xy[1] = 0.06;
    color_enc.white_point = JXL_WHITE_POINT_D65;
  } else if (payload[0] == 6 || payload[0] == 7) {
    // SMPTE ST 170 (2004) / SMPTE ST 240 (1999)
    color_enc.primaries = JXL_PRIMARIES_CUSTOM;
    color_enc.primaries_red_xy[0] = 0.630;
    color_enc.primaries_red_xy[1] = 0.340;
    color_enc.primaries_green_xy[0] = 0.310;
    color_enc.primaries_green_xy[1] = 0.595;
    color_enc.primaries_blue_xy[0] = 0.155;
    color_enc.primaries_blue_xy[1] = 0.070;
    color_enc.white_point = JXL_WHITE_POINT_D65;
  } else if (payload[0] == 8) {
    // Generic film (colour filters using Illuminant C)
    color_enc.primaries = JXL_PRIMARIES_CUSTOM;
    color_enc.primaries_red_xy[0] = 0.681;
    color_enc.primaries_red_xy[1] = 0.319;
    color_enc.primaries_green_xy[0] = 0.243;
    color_enc.primaries_green_xy[1] = 0.692;
    color_enc.primaries_blue_xy[0] = 0.145;
    color_enc.primaries_blue_xy[1] = 0.049;
    color_enc.white_point = JXL_WHITE_POINT_CUSTOM;
    color_enc.white_point_xy[0] = 0.310;
    color_enc.white_point_xy[1] = 0.316;
  } else if (payload[0] == 9) {
    // Rec. ITU-R BT.2100-2
    color_enc.primaries = JXL_PRIMARIES_2100;
    color_enc.white_point = JXL_WHITE_POINT_D65;
  } else if (payload[0] == 10) {
    // CIE 1931 XYZ
    color_enc.primaries = JXL_PRIMARIES_CUSTOM;
    color_enc.primaries_red_xy[0] = 1;
    color_enc.primaries_red_xy[1] = 0;
    color_enc.primaries_green_xy[0] = 0;
    color_enc.primaries_green_xy[1] = 1;
    color_enc.primaries_blue_xy[0] = 0;
    color_enc.primaries_blue_xy[1] = 0;
    color_enc.white_point = JXL_WHITE_POINT_E;
  } else if (payload[0] == 11) {
    // SMPTE RP 431-2 (2011)
    color_enc.primaries = JXL_PRIMARIES_P3;
    color_enc.white_point = JXL_WHITE_POINT_DCI;
  } else if (payload[0] == 12) {
    // SMPTE EG 432-1 (2010)
    color_enc.primaries = JXL_PRIMARIES_P3;
    color_enc.white_point = JXL_WHITE_POINT_D65;
  } else if (payload[0] == 22) {
    color_enc.primaries = JXL_PRIMARIES_CUSTOM;
    color_enc.primaries_red_xy[0] = 0.630;
    color_enc.primaries_red_xy[1] = 0.340;
    color_enc.primaries_green_xy[0] = 0.295;
    color_enc.primaries_green_xy[1] = 0.605;
    color_enc.primaries_blue_xy[0] = 0.155;
    color_enc.primaries_blue_xy[1] = 0.077;
    color_enc.white_point = JXL_WHITE_POINT_D65;
  } else {
    JXL_WARNING("Unsupported primaries specified in cICP chunk: %d",
                static_cast<int>(payload[0]));
    return false;
  }

  if (payload[1] == 1 || payload[1] == 6 || payload[1] == 14 ||
      payload[1] == 15) {
    // Rec. ITU-R BT.709-6
    color_enc.transfer_function = JXL_TRANSFER_FUNCTION_709;
  } else if (payload[1] == 4) {
    // Rec. ITU-R BT.1700-0 625 PAL and 625 SECAM
    color_enc.transfer_function = JXL_TRANSFER_FUNCTION_GAMMA;
    color_enc.gamma = 1 / 2.2;
  } else if (payload[1] == 5) {
    // Rec. ITU-R BT.470-6 System B, G
    color_enc.transfer_function = JXL_TRANSFER_FUNCTION_GAMMA;
    color_enc.gamma = 1 / 2.8;
  } else if (payload[1] == 8 || payload[1] == 13 || payload[1] == 16 ||
             payload[1] == 17 || payload[1] == 18) {
    // These codes all match the corresponding JXL enum values
    color_enc.transfer_function = static_cast<JxlTransferFunction>(payload[1]);
  } else {
    JXL_WARNING("Unsupported transfer function specified in cICP chunk: %d",
                static_cast<int>(payload[1]));
    return false;
  }

  if (payload[2] != 0) {
    JXL_WARNING("Unsupported color space specified in cICP chunk: %d",
                static_cast<int>(payload[2]));
    return false;
  }
  if (payload[3] != 1) {
    JXL_WARNING("Unsupported full-range flag specified in cICP chunk: %d",
                static_cast<int>(payload[3]));
    return false;
  }
  // cICP has no rendering intent, so use the default
  color_enc.rendering_intent = JXL_RENDERING_INTENT_RELATIVE;
  *color_encoding = color_enc;
  return true;
}

/** Extract information from 'gAMA' chunk. */
Status DecodeGamaChunk(Bytes payload, JxlColorEncoding* color_encoding) {
  if (payload.size() != 4) return JXL_FAILURE("Wrong gAMA size");
  color_encoding->transfer_function = JXL_TRANSFER_FUNCTION_GAMMA;
  color_encoding->gamma = F64FromU32(LoadBE32(payload.data()));
  return true;
}

/** Extract information from 'cHTM' chunk. */
Status DecodeChrmChunk(Bytes payload, JxlColorEncoding* color_encoding) {
  if (payload.size() != 32) return JXL_FAILURE("Wrong cHRM size");
  const uint8_t* data = payload.data();
  color_encoding->white_point = JXL_WHITE_POINT_CUSTOM;
  color_encoding->white_point_xy[0] = F64FromU32(LoadBE32(data + 0));
  color_encoding->white_point_xy[1] = F64FromU32(LoadBE32(data + 4));

  color_encoding->primaries = JXL_PRIMARIES_CUSTOM;
  color_encoding->primaries_red_xy[0] = F64FromU32(LoadBE32(data + 8));
  color_encoding->primaries_red_xy[1] = F64FromU32(LoadBE32(data + 12));
  color_encoding->primaries_green_xy[0] = F64FromU32(LoadBE32(data + 16));
  color_encoding->primaries_green_xy[1] = F64FromU32(LoadBE32(data + 20));
  color_encoding->primaries_blue_xy[0] = F64FromU32(LoadBE32(data + 24));
  color_encoding->primaries_blue_xy[1] = F64FromU32(LoadBE32(data + 28));
  return true;
}

/** Returns false if invalid. */
JXL_INLINE Status DecodeHexNibble(const char c, uint32_t* JXL_RESTRICT nibble) {
  if ('a' <= c && c <= 'f') {
    *nibble = 10 + c - 'a';
  } else if ('0' <= c && c <= '9') {
    *nibble = c - '0';
  } else {
    *nibble = 0;
    return JXL_FAILURE("Invalid metadata nibble");
  }
  JXL_ASSERT(*nibble < 16);
  return true;
}

/** Returns false if invalid. */
JXL_INLINE Status DecodeDecimal(const char** pos, const char* end,
                                uint32_t* JXL_RESTRICT value) {
  size_t len = 0;
  *value = 0;
  while (*pos < end) {
    char next = **pos;
    if (next >= '0' && next <= '9') {
      *value = (*value * 10) + static_cast<uint32_t>(next - '0');
      len++;
      if (len > 8) {
        break;
      }
    } else {
      // Do not consume terminator (non-decimal digit).
      break;
    }
    (*pos)++;
  }
  if (len == 0 || len > 8) {
    return JXL_FAILURE("Failed to parse decimal");
  }
  return true;
}

/**
 * Parses a PNG text chunk with key of the form "Raw profile type ####", with
 * #### a type.
 *
 * Returns whether it could successfully parse the content.
 * We trust key and encoded are null-terminated because they come from
 * libpng.
 */
Status MaybeDecodeBase16(const char* key, const char* encoded,
                         std::string* type, std::vector<uint8_t>* bytes) {
  const char* encoded_end = encoded + strlen(encoded);

  const char* kKey = "Raw profile type ";
  if (strncmp(key, kKey, strlen(kKey)) != 0) return false;
  *type = key + strlen(kKey);
  const size_t kMaxTypeLen = 20;
  if (type->length() > kMaxTypeLen) return false;  // Type too long

  // Header: freeform string and number of bytes
  // Expected format is:
  // \n
  // profile name/description\n
  //       40\n               (the number of bytes after hex-decoding)
  // 01234566789abcdef....\n  (72 bytes per line max).
  // 012345667\n              (last line)
  const char* pos = encoded;

  if (*(pos++) != '\n') return false;
  while (pos < encoded_end && *pos != '\n') {
    pos++;
  }
  if (pos == encoded_end) return false;
  // We parsed so far a \n, some number of non \n characters and are now
  // pointing at a \n.
  if (*(pos++) != '\n') return false;
  // Skip leading spaces
  while (pos < encoded_end && *pos == ' ') {
    pos++;
  }
  uint32_t bytes_to_decode = 0;
  JXL_RETURN_IF_ERROR(DecodeDecimal(&pos, encoded_end, &bytes_to_decode));

  // We need 2*bytes for the hex values plus 1 byte every 36 values,
  // plus terminal \n for length.
  size_t tail = static_cast<size_t>(encoded_end - pos);
  bool ok = ((tail / 2) >= bytes_to_decode);
  if (ok) tail -= 2 * static_cast<size_t>(bytes_to_decode);
  ok = ok && (tail == 1 + DivCeil(bytes_to_decode, 36));
  if (!ok) {
    return JXL_FAILURE("Not enough bytes to parse %d bytes in hex",
                       bytes_to_decode);
  }
  JXL_ASSERT(bytes->empty());
  bytes->reserve(bytes_to_decode);

  // Encoding: base16 with newline after 72 chars.
  // pos points to the \n before the first line of hex values.
  for (size_t i = 0; i < bytes_to_decode; ++i) {
    if (i % 36 == 0) {
      if (pos + 1 >= encoded_end) return false;  // Truncated base16 1
      if (*pos != '\n') return false;            // Expected newline
      ++pos;
    }

    if (pos + 2 >= encoded_end) return false;  // Truncated base16 2;
    uint32_t nibble0;
    uint32_t nibble1;
    JXL_RETURN_IF_ERROR(DecodeHexNibble(pos[0], &nibble0));
    JXL_RETURN_IF_ERROR(DecodeHexNibble(pos[1], &nibble1));
    bytes->push_back(static_cast<uint8_t>((nibble0 << 4) + nibble1));
    pos += 2;
  }
  if (pos + 1 != encoded_end) return false;  // Too many encoded bytes
  if (pos[0] != '\n') return false;          // Incorrect metadata terminator
  return true;
}

/** Retrieves XMP and EXIF/IPTC from itext and text. */
Status DecodeBlob(const png_text_struct& info, PackedMetadata* metadata) {
  // We trust these are properly null-terminated by libpng.
  const char* key = info.key;
  const char* value = info.text;
  if (strstr(key, "XML:com.adobe.xmp")) {
    metadata->xmp.resize(strlen(value));  // safe, see above
    memcpy(metadata->xmp.data(), value, metadata->xmp.size());
  }

  std::string type;
  std::vector<uint8_t> bytes;

  // Handle text chunks annotated with key "Raw profile type ####", with
  // #### a type, which may contain metadata.
  const char* kKey = "Raw profile type ";
  if (strncmp(key, kKey, strlen(kKey)) != 0) return false;

  if (!MaybeDecodeBase16(key, value, &type, &bytes)) {
    JXL_WARNING("Couldn't parse 'Raw format type' text chunk");
    return false;
  }
  if (type == "exif") {
    // Remove prefix if present.
    constexpr std::array<uint8_t, 6> kExifPrefix = {'E', 'x', 'i', 'f', 0, 0};
    if (bytes.size() >= kExifPrefix.size() &&
        memcmp(bytes.data(), kExifPrefix.data(), kExifPrefix.size()) == 0) {
      bytes.erase(bytes.begin(), bytes.begin() + kExifPrefix.size());
    }
    if (!metadata->exif.empty()) {
      JXL_DEBUG(2,
                "overwriting EXIF (%" PRIuS " bytes) with base16 (%" PRIuS
                " bytes)",
                metadata->exif.size(), bytes.size());
    }
    metadata->exif = std::move(bytes);
  } else if (type == "iptc") {
    // TODO(jon): Deal with IPTC in some way
  } else if (type == "8bim") {
    // TODO(jon): Deal with 8bim in some way
  } else if (type == "xmp") {
    if (!metadata->xmp.empty()) {
      JXL_DEBUG(2,
                "overwriting XMP (%" PRIuS " bytes) with base16 (%" PRIuS
                " bytes)",
                metadata->xmp.size(), bytes.size());
    }
    metadata->xmp = std::move(bytes);
  } else {
    JXL_DEBUG(
        2, "Unknown type in 'Raw format type' text chunk: %s: %" PRIuS " bytes",
        type.c_str(), bytes.size());
  }
  return true;
}

constexpr bool isAbc(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

/** Wrap 4-char tag name into ID. */
constexpr uint32_t MakeTag(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
  return a | (b << 8) | (c << 16) | (d << 24);
}

/** Reusable image data container. */
struct Pixels {
  // Use array instead of vector to avoid memory initialization.
  std::unique_ptr<uint8_t[]> pixels;
  size_t pixels_size = 0;
  std::vector<uint8_t*> rows;

  Status Resize(size_t row_bytes, size_t num_rows) {
    size_t new_size = row_bytes * num_rows;  // it is assumed size is sane
    if (new_size > pixels_size) {
      pixels.reset(new uint8_t[new_size]);
      if (!pixels) {
        // TODO(szabadka): use specialized OOM error code
        return JXL_FAILURE("Failed to allocate memory for image buffer");
      }
      pixels_size = new_size;
    }
    rows.resize(num_rows);
    for (size_t y = 0; y < num_rows; y++) {
      rows[y] = pixels.get() + y * row_bytes;
    }
    return true;
  }
};

/**
 * Helper that chunks in-memory input.
 */
struct Reader {
  explicit Reader(Span<const uint8_t> data) : data_(data) {}

  const Span<const uint8_t> data_;
  size_t offset_ = 0;

  Bytes Peek(size_t len) const {
    size_t cap = data_.size() - offset_;
    size_t to_copy = std::min(cap, len);
    return {data_.data() + offset_, to_copy};
  }

  Bytes Read(size_t len) {
    Bytes result = Peek(len);
    offset_ += result.size();
    return result;
  }

  /* Returns empty Span on error. */
  Bytes ReadChunk() {
    Bytes len = Peek(4);
    if (len.size() != 4) {
      return Bytes();
    }
    const auto size = png_get_uint_32(len.data());
    // NB: specification allows 2^31 - 1
    constexpr size_t kMaxPNGChunkSize = 1u << 30;  // 1 GB
    // Check first, to avoid overflow.
    if (size > kMaxPNGChunkSize) {
      JXL_WARNING("APNG chunk size is too big");
      return Bytes();
    }
    size_t full_size = size + 12;  // size does not include itself, tag and CRC.
    Bytes result = Read(full_size);
    return (result.size() == full_size) ? result : Bytes();
  }

  bool Eof() const { return offset_ == data_.size(); }
};

void ProgressiveRead_OnInfo(png_structp png_ptr, png_infop info_ptr) {
  png_set_expand(png_ptr);
  png_set_palette_to_rgb(png_ptr);
  png_set_tRNS_to_alpha(png_ptr);
  (void)png_set_interlace_handling(png_ptr);
  png_read_update_info(png_ptr, info_ptr);
}

void ProgressiveRead_OnRow(png_structp png_ptr, png_bytep new_row,
                           png_uint_32 row_num, int pass) {
  Pixels* frame = reinterpret_cast<Pixels*>(png_get_progressive_ptr(png_ptr));
  JXL_CHECK(frame);
  JXL_CHECK(row_num < frame->rows.size());
  png_progressive_combine_row(png_ptr, frame->rows[row_num], new_row);
}

// Holds intermediate state during parsing APNG file.
struct Context {
  ~Context() {
    // Make sure png memory is released in any case.
    ResetPngDecoder();
  }

  bool CreatePngDecoder() {
    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr,
                                     nullptr);
    info_ptr = png_create_info_struct(png_ptr);
    return (png_ptr != nullptr && info_ptr != nullptr);
  }

  /**
   * Initialize PNG decoder.
   *
   * TODO(eustas): add details
   */
  bool InitPngDecoder(const std::vector<Bytes>& chunksInfo,
                      const RectT<uint64_t>& viewport) {
    ResetPngDecoder();

    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr,
                                     nullptr);
    info_ptr = png_create_info_struct(png_ptr);
    if (png_ptr == nullptr || info_ptr == nullptr) {
      return false;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
      return false;
    }

    /* hIST chunk tail is not processed properly; skip this chunk completely;
       see https://github.com/glennrp/libpng/pull/413 */
    constexpr std::array<uint8_t, 5> kIgnoredChunks = {'h', 'I', 'S', 'T', 0};
    png_set_keep_unknown_chunks(png_ptr, 1, kIgnoredChunks.data(),
                                static_cast<int>(kIgnoredChunks.size() / 5));

    png_set_crc_action(png_ptr, PNG_CRC_QUIET_USE, PNG_CRC_QUIET_USE);
    png_set_progressive_read_fn(png_ptr, static_cast<void*>(&frameRaw),
                                ProgressiveRead_OnInfo, ProgressiveRead_OnRow,
                                nullptr);

    png_process_data(png_ptr, info_ptr,
                     const_cast<uint8_t*>(kPngSignature.data()),
                     kPngSignature.size());

    // Patch dimensions.
    png_save_uint_32(ihdr.data() + 8, static_cast<uint32_t>(viewport.xsize()));
    png_save_uint_32(ihdr.data() + 12, static_cast<uint32_t>(viewport.ysize()));
    png_process_data(png_ptr, info_ptr, ihdr.data(), ihdr.size());

    for (const auto& chunk : chunksInfo) {
      png_process_data(png_ptr, info_ptr, const_cast<uint8_t*>(chunk.data()),
                       chunk.size());
    }

    return true;
  }

  /**
   * Pass chunk to PNG decoder.
   */
  bool FeedChunks(const Bytes& chunk1, const Bytes& chunk2 = Bytes()) {
    // TODO(eustas): turn to DCHECK
    if (!png_ptr || !info_ptr) return false;

    if (setjmp(png_jmpbuf(png_ptr))) {
      return false;
    }

    for (const auto& chunk : {chunk1, chunk2}) {
      if (!chunk.empty()) {
        png_process_data(png_ptr, info_ptr, const_cast<uint8_t*>(chunk.data()),
                         chunk.size());
      }
    }
    return true;
  }

  bool FinalizeStream(PackedMetadata* metadata) {
    // TODO(eustas): turn to DCHECK
    if (!png_ptr || !info_ptr) return false;

    if (setjmp(png_jmpbuf(png_ptr))) {
      return false;
    }

    const std::array<uint8_t, 12> kFooter = {0,  0,  0,   0,  73, 69,
                                             78, 68, 174, 66, 96, 130};
    png_process_data(png_ptr, info_ptr, const_cast<uint8_t*>(kFooter.data()),
                     kFooter.size());
    // before destroying: check if we encountered any metadata chunks
    png_textp text_ptr;
    int num_text;
    png_get_text(png_ptr, info_ptr, &text_ptr, &num_text);
    for (int i = 0; i < num_text; i++) {
      Status result = DecodeBlob(text_ptr[i], metadata);
      // Ignore unknown / malformed blob.
      (void)result;
    }

    return true;
  }

  void ResetPngDecoder() {
    png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
    // Just in case. Not all versions on libpng wipe-out the pointers.
    png_ptr = nullptr;
    info_ptr = nullptr;
  }

  std::array<uint8_t, 25> ihdr;  // (modified) copy of file IHDR chunk
  png_structp png_ptr = nullptr;
  png_infop info_ptr = nullptr;
  Pixels frameRaw = {};
};

enum class DisposeOp : uint8_t { NONE = 0, BACKGROUND = 1, PREVIOUS = 2 };

constexpr uint8_t kLastDisposeOp = static_cast<uint32_t>(DisposeOp::PREVIOUS);

enum class BlendOp : uint8_t { SOURCE = 0, OVER = 1 };

constexpr uint8_t kLastBlendOp = static_cast<uint32_t>(BlendOp::OVER);

// fcTL
struct FrameControl {
  uint32_t delay_num;
  uint32_t delay_den;
  RectT<uint64_t> viewport;
  DisposeOp dispose_op;
  BlendOp blend_op;
};

struct Frame {
  PackedImage pixels;
  FrameControl metadata;
};

bool ValidateViewport(const RectT<uint64_t>& r) {
  constexpr uint32_t kMaxPngDim = 1000000UL;
  return (r.xsize() <= kMaxPngDim) && (r.ysize() <= kMaxPngDim);
}

/**
 * Setup #channels, bpp, colorspace, etc. from PNG values.
 */
void SetColorData(PackedPixelFile* ppf, uint8_t color_type, uint8_t bit_depth,
                  png_color_8p sig_bits, uint32_t has_transparency) {
  bool palette_used = ((color_type & 1) != 0);
  bool color_used = ((color_type & 2) != 0);
  bool alpha_channel_used = ((color_type & 4) != 0);
  if (palette_used) {
    if (!color_used || alpha_channel_used) {
      JXL_DEBUG(2, "Unexpected PNG color type");
    }
  }

  ppf->info.bits_per_sample = bit_depth;

  if (palette_used) {
    // palette will actually be 8-bit regardless of the index bitdepth
    ppf->info.bits_per_sample = 8;
  }
  if (color_used) {
    ppf->info.num_color_channels = 3;
    ppf->color_encoding.color_space = JXL_COLOR_SPACE_RGB;
    if (sig_bits) {
      if (sig_bits->red == sig_bits->green &&
          sig_bits->green == sig_bits->blue) {
        ppf->info.bits_per_sample = sig_bits->red;
      } else {
        int maxbps =
            std::max(sig_bits->red, std::max(sig_bits->green, sig_bits->blue));
        JXL_DEBUG(2,
                  "sBIT chunk: bit depths for R, G, and B are not the same (%i "
                  "%i %i), while in JPEG XL they have to be the same. Setting "
                  "RGB bit depth to %i.",
                  sig_bits->red, sig_bits->green, sig_bits->blue, maxbps);
        ppf->info.bits_per_sample = maxbps;
      }
    }
  } else {
    ppf->info.num_color_channels = 1;
    ppf->color_encoding.color_space = JXL_COLOR_SPACE_GRAY;
    if (sig_bits) ppf->info.bits_per_sample = sig_bits->gray;
  }
  if (alpha_channel_used || has_transparency) {
    ppf->info.alpha_bits = ppf->info.bits_per_sample;
    if (sig_bits && sig_bits->alpha != ppf->info.bits_per_sample) {
      JXL_DEBUG(2,
                "sBIT chunk: bit depths for RGBA are inconsistent "
                "(%i %i %i %i). Setting A bitdepth to %i.",
                sig_bits->red, sig_bits->green, sig_bits->blue, sig_bits->alpha,
                ppf->info.bits_per_sample);
    }
  } else {
    ppf->info.alpha_bits = 0;
  }
  ppf->color_encoding.color_space = (ppf->info.num_color_channels == 1)
                                        ? JXL_COLOR_SPACE_GRAY
                                        : JXL_COLOR_SPACE_RGB;
}

// Color profile chunks: cICP has the highest priority, followed by
// iCCP and sRGB (which shouldn't co-exist, but if they do, we use
// iCCP), followed finally by gAMA and cHRM.
enum class ColorInfoType {
  NONE = 0,
  GAMA_OR_CHRM = 1,
  ICCP_OR_SRGB = 2,
  CICP = 3
};

}  // namespace

bool CanDecodeAPNG() { return true; }

/**
 * Parse and decode PNG file.
 *
 * Useful PNG chunks:
 *   acTL : animation control (#frames, loop count)
 *   fcTL : frame control (seq#, viewport, delay, disposal blending)
 *   bKGD : preferable background
 *   IDAT : "default image"
 *          if single fcTL goes before IDAT, then it is also first frame
 *   fdAT : seq# + IDAT-like content
 *   PLTE : palette
 *   cICP : coding-independent code points for video signal type identification
 *   iCCP : embedded ICC profile
 *   sRGB : standard RGB colour space
 *   eXIf : exchangeable image file profile
 *   gAMA : image gamma
 *   cHRM : primary chromaticities and white point
 *   tRNS : transparency
 *
 * PNG chunk ordering:
 *  - IHDR first
 *  - IEND last
 *  - acTL, cHRM, cICP, gAMA, iCCP, sRGB, bKGD, eXIf, PLTE before IDAT
 *  - fdAT after IDAT
 *
 * More rules:
 *  - iCCP and sRGB are exclusive
 *  - fcTL and fdAT seq# must be in order fro 0, with no gaps or duplicates
 *  - fcTL before corresponding IDAT / fdAT
 */
Status DecodeImageAPNG(const Span<const uint8_t> bytes,
                       const ColorHints& color_hints, PackedPixelFile* ppf,
                       const SizeConstraints* constraints) {
  // Initialize output (default settings in case e.g. only gAMA is given).
  ppf->frames.clear();
  ppf->info.exponent_bits_per_sample = 0;
  ppf->info.alpha_exponent_bits = 0;
  ppf->info.orientation = JXL_ORIENT_IDENTITY;
  ppf->color_encoding.color_space = JXL_COLOR_SPACE_RGB;
  ppf->color_encoding.white_point = JXL_WHITE_POINT_D65;
  ppf->color_encoding.primaries = JXL_PRIMARIES_SRGB;
  ppf->color_encoding.transfer_function = JXL_TRANSFER_FUNCTION_SRGB;
  ppf->color_encoding.rendering_intent = JXL_RENDERING_INTENT_RELATIVE;

  Reader input(bytes);

  // Check signature.
  Bytes sig = input.Read(kPngSignature.size());
  if (sig.size() != 8 ||
      memcmp(sig.data(), kPngSignature.data(), kPngSignature.size()) != 0) {
    return false;  // Return silently if it is not a PNG
  }

  // Check IHDR chunk.
  Context ctx;
  Bytes ihdr = input.ReadChunk();
  if (ihdr.size() != ctx.ihdr.size()) {
    return JXL_FAILURE("Unexpected first chunk payload size");
  }
  memcpy(ctx.ihdr.data(), ihdr.data(), ihdr.size());
  uint32_t id = LoadLE32(ihdr.data() + 4);
  if (id != MakeTag('I', 'H', 'D', 'R')) {
    return JXL_FAILURE("First chunk is not IHDR");
  }
  const RectT<uint64_t> image_rect(0, 0, png_get_uint_32(ihdr.data() + 8),
                                   png_get_uint_32(ihdr.data() + 12));
  if (!ValidateViewport(image_rect)) {
    return JXL_FAILURE("PNG image dimensions are too large");
  }

  // Chunks we supply to PNG decoder for every animation frame.
  std::vector<Bytes> passthrough_chunks;
  if (!ctx.InitPngDecoder(passthrough_chunks, image_rect)) {
    return JXL_FAILURE("Failed to initialize PNG decoder");
  }

  // Marker that this PNG is animated.
  bool seen_actl = false;
  // First IDAT is a very important milestone; at this moment we freeze
  // gathered metadata.
  bool seen_idat = false;
  // fCTL can occur multiple times, but only once before IDAT.
  bool seen_fctl = false;
  // Logical EOF.
  bool seen_iend = false;

  ColorInfoType color_info_type = ColorInfoType::NONE;

  // Flag that we processed some IDAT / fDAT after image / frame start.
  bool seen_pixel_data = false;

  uint32_t num_channels;
  JxlPixelFormat format = {};
  size_t bytes_per_pixel = 0;
  std::vector<Frame> frames;
  FrameControl current_frame = {/*delay_num=*/1, /*delay_den=*/10, image_rect,
                                DisposeOp::NONE, BlendOp::SOURCE};

  // Copies frame pixels / metadata from temporary storage.
  // TODO(eustas): avoid copying.
  const auto finalize_frame = [&]() -> Status {
    if (!seen_pixel_data) {
      return JXL_FAILURE("Frame / image without fdAT / IDAT chunks");
    }
    if (!ctx.FinalizeStream(&ppf->metadata)) {
      return JXL_FAILURE("Failed to finalize PNG substream");
    }
    // Allocates the frame buffer.
    const RectT<uint64_t>& vp = current_frame.viewport;
    size_t xsize = static_cast<size_t>(vp.xsize());
    size_t ysize = static_cast<size_t>(vp.ysize());
    JXL_ASSIGN_OR_RETURN(PackedImage image,
                         PackedImage::Create(xsize, ysize, format));
    for (size_t y = 0; y < ysize; ++y) {
      // TODO(eustas): ensure multiplication is safe
      memcpy(static_cast<uint8_t*>(image.pixels()) + image.stride * y,
             ctx.frameRaw.rows[y], bytes_per_pixel * xsize);
    }
    frames.push_back(Frame{std::move(image), current_frame});
    seen_pixel_data = false;
    return true;
  };

  while (!input.Eof()) {
    if (seen_iend) {
      return JXL_FAILURE("Exuberant input after IEND chunk");
    }
    Bytes chunk = input.ReadChunk();
    if (chunk.empty()) {
      return JXL_FAILURE("Malformed chunk");
    }
    Bytes type(chunk.data() + 4, 4);
    id = LoadLE32(type.data());
    // Cut 'size' and 'type' at front and 'CRC' at the end.
    Bytes payload(chunk.data() + 8, chunk.size() - 12);

    if (!isAbc(type[0]) || !isAbc(type[1]) || !isAbc(type[2]) ||
        !isAbc(type[3])) {
      return JXL_FAILURE("Exotic PNG chunk");
    }

    switch (id) {
      case MakeTag('a', 'c', 'T', 'L'):
        if (seen_idat) {
          JXL_DEBUG(2, "aCTL after IDAT ignored");
          continue;
        }
        if (seen_actl) {
          JXL_DEBUG(2, "Duplicate aCTL chunk ignored");
          continue;
        }
        seen_actl = true;
        ppf->info.have_animation = JXL_TRUE;
        // TODO(eustas): decode from chunk?
        ppf->info.animation.tps_numerator = 1000;
        ppf->info.animation.tps_denominator = 1;
        continue;

      case MakeTag('I', 'E', 'N', 'D'):
        seen_iend = true;
        JXL_RETURN_IF_ERROR(finalize_frame());
        continue;

      case MakeTag('f', 'c', 'T', 'L'): {
        if (payload.size() != 26) {
          return JXL_FAILURE("Unexpected fcTL payload size: %u",
                             static_cast<uint32_t>(payload.size()));
        }
        if (seen_fctl && !seen_idat) {
          return JXL_FAILURE("More than one fcTL before IDAT");
        }
        if (seen_idat && !seen_actl) {
          return JXL_FAILURE("fcTL after IDAT, but without acTL");
        }
        seen_fctl = true;

        // TODO(eustas): check order?
        // sequence_number = png_get_uint_32(payload.data());
        RectT<uint64_t> raw_viewport(png_get_uint_32(payload.data() + 12),
                                     png_get_uint_32(payload.data() + 16),
                                     png_get_uint_32(payload.data() + 4),
                                     png_get_uint_32(payload.data() + 8));
        uint8_t dispose_op = payload[24];
        if (dispose_op > kLastDisposeOp) {
          return JXL_FAILURE("Invalid DisposeOp");
        }
        uint8_t blend_op = payload[25];
        if (blend_op > kLastBlendOp) {
          return JXL_FAILURE("Invalid BlendOp");
        }
        FrameControl next_frame = {
            /*delay_num=*/png_get_uint_16(payload.data() + 20),
            /*delay_den=*/png_get_uint_16(payload.data() + 22), raw_viewport,
            static_cast<DisposeOp>(dispose_op), static_cast<BlendOp>(blend_op)};

        if (!raw_viewport.Intersection(image_rect).IsSame(raw_viewport)) {
          // Cropping happened.
          return JXL_FAILURE("PNG frame is outside of image rect");
        }

        if (!seen_idat) {
          // "Default" image is the first animation frame. Its viewport must
          // cover the whole image area.
          if (!raw_viewport.IsSame(image_rect)) {
            return JXL_FAILURE(
                "If the first animation frame is default image, its viewport "
                "must cover full image");
          }
        } else {
          JXL_RETURN_IF_ERROR(finalize_frame());
          if (!ctx.InitPngDecoder(passthrough_chunks, next_frame.viewport)) {
            return JXL_FAILURE("Failed to initialize PNG decoder");
          }
        }
        current_frame = next_frame;
        continue;
      }

      case MakeTag('I', 'D', 'A', 'T'): {
        if (!frames.empty()) {
          return JXL_FAILURE("IDAT after default image is over");
        }
        if (!seen_idat) {
          // First IDAT means that all metadata is ready.
          seen_idat = true;
          JXL_CHECK(image_rect.xsize() ==
                    png_get_image_width(ctx.png_ptr, ctx.info_ptr));
          JXL_CHECK(image_rect.ysize() ==
                    png_get_image_height(ctx.png_ptr, ctx.info_ptr));
          JXL_RETURN_IF_ERROR(VerifyDimensions(constraints, image_rect.xsize(),
                                               image_rect.ysize()));
          ppf->info.xsize = image_rect.xsize();
          ppf->info.ysize = image_rect.ysize();

          png_color_8p sig_bits = nullptr;
          // Error is OK -> sig_bits remains nullptr.
          png_get_sBIT(ctx.png_ptr, ctx.info_ptr, &sig_bits);
          SetColorData(ppf, png_get_color_type(ctx.png_ptr, ctx.info_ptr),
                       png_get_bit_depth(ctx.png_ptr, ctx.info_ptr), sig_bits,
                       png_get_valid(ctx.png_ptr, ctx.info_ptr, PNG_INFO_tRNS));
          num_channels =
              ppf->info.num_color_channels + (ppf->info.alpha_bits ? 1 : 0);
          format = {
              /*num_channels=*/num_channels,
              /*data_type=*/ppf->info.bits_per_sample > 8 ? JXL_TYPE_UINT16
                                                          : JXL_TYPE_UINT8,
              /*endianness=*/JXL_BIG_ENDIAN,
              /*align=*/0,
          };
          bytes_per_pixel =
              num_channels * (format.data_type == JXL_TYPE_UINT16 ? 2 : 1);
          // TODO(eustas): ensure multiplication is safe
          uint64_t row_bytes =
              static_cast<uint64_t>(image_rect.xsize()) * bytes_per_pixel;
          uint64_t max_rows = std::numeric_limits<size_t>::max() / row_bytes;
          if (image_rect.ysize() > max_rows) {
            return JXL_FAILURE("Image too big.");
          }
          // TODO(eustas): drop frameRaw
          JXL_RETURN_IF_ERROR(
              ctx.frameRaw.Resize(row_bytes, image_rect.ysize()));
        }

        if (!ctx.FeedChunks(chunk)) {
          return JXL_FAILURE("Decoding IDAT failed");
        }
        seen_pixel_data = true;
        continue;
      }

      case MakeTag('f', 'd', 'A', 'T'): {
        if (!seen_idat) {
          return JXL_FAILURE("fdAT chunk before IDAT");
        }
        if (!seen_actl) {
          return JXL_FAILURE("fdAT chunk before acTL");
        }
        /* The 'fdAT' chunk has... the same structure as an 'IDAT' chunk,
         * except preceded by a sequence number. */
        if (payload.size() < 4) {
          return JXL_FAILURE("Corrupted fdAT chunk");
        }
        // Turn 'fdAT' to 'IDAT' by cutting sequence number and replacing tag.
        std::array<uint8_t, 8> preamble;
        png_save_uint_32(preamble.data(), payload.size() - 4);
        memcpy(preamble.data() + 4, "IDAT", 4);
        // Cut-off 'size', 'type' and 'sequence_number'
        Bytes chunk_tail(chunk.data() + 12, chunk.size() - 12);
        if (!ctx.FeedChunks(Bytes(preamble), chunk_tail)) {
          return JXL_FAILURE("Decoding fdAT failed");
        }
        seen_pixel_data = true;
        continue;
      }

      case MakeTag('c', 'I', 'C', 'P'):
        if (color_info_type == ColorInfoType::CICP) {
          JXL_DEBUG(2, "Excessive colorspace definition; cICP chunk ignored");
          continue;
        }
        JXL_RETURN_IF_ERROR(DecodeCicpChunk(payload, &ppf->color_encoding));
        ppf->icc.clear();
        ppf->primary_color_representation =
            PackedPixelFile::kColorEncodingIsPrimary;
        color_info_type = ColorInfoType::CICP;
        continue;

      case MakeTag('i', 'C', 'C', 'P'): {
        if (color_info_type == ColorInfoType::ICCP_OR_SRGB) {
          return JXL_FAILURE("Repeated iCCP / sRGB chunk");
        }
        if (color_info_type > ColorInfoType::ICCP_OR_SRGB) {
          JXL_DEBUG(2, "Excessive colorspace definition; iCCP chunk ignored");
          continue;
        }
        // Let PNG decoder deal with chunk processing.
        if (!ctx.FeedChunks(chunk)) {
          return JXL_FAILURE("Corrupt iCCP chunk");
        }

        // TODO(jon): catch special case of PQ and synthesize color encoding
        // in that case
        int compression_type = 0;
        png_bytep profile = nullptr;
        png_charp name = nullptr;
        png_uint_32 profile_len = 0;
        png_uint_32 ok =
            png_get_iCCP(ctx.png_ptr, ctx.info_ptr, &name, &compression_type,
                         &profile, &profile_len);
        if (!ok || !profile_len) {
          return JXL_FAILURE("Malformed / incomplete iCCP chunk");
        }
        ppf->icc.assign(profile, profile + profile_len);
        ppf->primary_color_representation = PackedPixelFile::kIccIsPrimary;
        color_info_type = ColorInfoType::ICCP_OR_SRGB;
        continue;
      }

      case MakeTag('s', 'R', 'G', 'B'):
        if (color_info_type == ColorInfoType::ICCP_OR_SRGB) {
          return JXL_FAILURE("Repeated iCCP / sRGB chunk");
        }
        if (color_info_type > ColorInfoType::ICCP_OR_SRGB) {
          JXL_DEBUG(2, "Excessive colorspace definition; sRGB chunk ignored");
          continue;
        }
        JXL_RETURN_IF_ERROR(DecodeSrgbChunk(payload, &ppf->color_encoding));
        color_info_type = ColorInfoType::ICCP_OR_SRGB;
        continue;

      case MakeTag('g', 'A', 'M', 'A'):
        if (color_info_type >= ColorInfoType::GAMA_OR_CHRM) {
          JXL_DEBUG(2, "Excessive colorspace definition; gAMA chunk ignored");
          continue;
        }
        JXL_RETURN_IF_ERROR(DecodeGamaChunk(payload, &ppf->color_encoding));
        color_info_type = ColorInfoType::GAMA_OR_CHRM;
        continue;

      case MakeTag('c', 'H', 'R', 'M'):
        if (color_info_type >= ColorInfoType::GAMA_OR_CHRM) {
          JXL_DEBUG(2, "Excessive colorspace definition; cHRM chunk ignored");
          continue;
        }
        JXL_RETURN_IF_ERROR(DecodeChrmChunk(payload, &ppf->color_encoding));
        color_info_type = ColorInfoType::GAMA_OR_CHRM;
        continue;

      case MakeTag('e', 'X', 'I', 'f'):
        // TODO(eustas): next eXIF chunk overwrites current; is it ok?
        ppf->metadata.exif.resize(payload.size());
        memcpy(ppf->metadata.exif.data(), payload.data(), payload.size());
        continue;

      default:
        // We don't know what is that, just pass through.
        if (!ctx.FeedChunks(chunk)) {
          return JXL_FAILURE("PNG decoder failed to process chunk");
        }
        // If it happens before IDAT, we consider it metadata and pass to all
        // sub-decoders.
        if (!seen_idat) {
          passthrough_chunks.push_back(chunk);
        }
        continue;
    }
  }

  bool color_is_already_set = (color_info_type != ColorInfoType::NONE);
  bool is_gray = (ppf->info.num_color_channels == 1);
  JXL_RETURN_IF_ERROR(
      ApplyColorHints(color_hints, color_is_already_set, is_gray, ppf));

  bool has_nontrivial_background = false;
  bool previous_frame_should_be_cleared = false;
  for (size_t i = 0; i < frames.size(); i++) {
    Frame& frame = frames[i];
    const FrameControl& fc = frame.metadata;
    const RectT<uint64_t> vp = fc.viewport;
    const auto& pixels = frame.pixels;
    size_t xsize = pixels.xsize;
    size_t ysize = pixels.ysize;
    JXL_ASSERT(xsize == vp.xsize());
    JXL_ASSERT(ysize == vp.ysize());

    // Before encountering a DISPOSE_OP_NONE frame, the canvas is filled with
    // 0, so DISPOSE_OP_BACKGROUND and DISPOSE_OP_PREVIOUS are equivalent.
    if (fc.dispose_op == DisposeOp::NONE) {
      has_nontrivial_background = true;
    }
    bool should_blend = fc.blend_op == BlendOp::OVER;
    bool use_for_next_frame =
        has_nontrivial_background && fc.dispose_op != DisposeOp::PREVIOUS;
    size_t x0 = vp.x0();
    size_t y0 = vp.y0();
    if (previous_frame_should_be_cleared) {
      const auto& pvp = frames[i - 1].metadata.viewport;
      size_t px0 = pvp.x0();
      size_t py0 = pvp.y0();
      size_t pxs = pvp.xsize();
      size_t pys = pvp.ysize();
      if (px0 >= x0 && py0 >= y0 && px0 + pxs <= x0 + xsize &&
          py0 + pys <= y0 + ysize && fc.blend_op == BlendOp::SOURCE &&
          use_for_next_frame) {
        // If the previous frame is entirely contained in the current frame
        // and we are using BLEND_OP_SOURCE, nothing special needs to be done.
        ppf->frames.emplace_back(std::move(frame.pixels));
      } else if (px0 == x0 && py0 == y0 && px0 + pxs == x0 + xsize &&
                 py0 + pys == y0 + ysize && use_for_next_frame) {
        // If the new frame has the same size as the old one, but we are
        // blending, we can instead just not blend.
        should_blend = false;
        ppf->frames.emplace_back(std::move(frame.pixels));
      } else if (px0 <= x0 && py0 <= y0 && px0 + pxs >= x0 + xsize &&
                 py0 + pys >= y0 + ysize && use_for_next_frame) {
        // If the new frame is contained within the old frame, we can pad the
        // new frame with zeros and not blend.
        JXL_ASSIGN_OR_RETURN(PackedImage new_data,
                             PackedImage::Create(pxs, pys, pixels.format));
        memset(new_data.pixels(), 0, new_data.pixels_size);
        for (size_t y = 0; y < ysize; y++) {
          size_t bytes_per_pixel =
              PackedImage::BitsPerChannel(new_data.format.data_type) *
              new_data.format.num_channels / 8;
          memcpy(
              static_cast<uint8_t*>(new_data.pixels()) +
                  new_data.stride * (y + y0 - py0) +
                  bytes_per_pixel * (x0 - px0),
              static_cast<const uint8_t*>(pixels.pixels()) + pixels.stride * y,
              xsize * bytes_per_pixel);
        }

        x0 = px0;
        y0 = py0;
        xsize = pxs;
        ysize = pys;
        should_blend = false;
        ppf->frames.emplace_back(std::move(new_data));
      } else {
        // If all else fails, insert a placeholder blank frame with kReplace.
        JXL_ASSIGN_OR_RETURN(PackedImage blank,
                             PackedImage::Create(pxs, pys, pixels.format));
        memset(blank.pixels(), 0, blank.pixels_size);
        ppf->frames.emplace_back(std::move(blank));
        auto& pframe = ppf->frames.back();
        pframe.frame_info.layer_info.crop_x0 = px0;
        pframe.frame_info.layer_info.crop_y0 = py0;
        pframe.frame_info.layer_info.xsize = pxs;
        pframe.frame_info.layer_info.ysize = pys;
        pframe.frame_info.duration = 0;
        bool is_full_size = px0 == 0 && py0 == 0 && pxs == ppf->info.xsize &&
                            pys == ppf->info.ysize;
        pframe.frame_info.layer_info.have_crop = is_full_size ? 0 : 1;
        pframe.frame_info.layer_info.blend_info.blendmode = JXL_BLEND_REPLACE;
        pframe.frame_info.layer_info.blend_info.source = 1;
        pframe.frame_info.layer_info.save_as_reference = 1;
        ppf->frames.emplace_back(std::move(frame.pixels));
      }
    } else {
      ppf->frames.emplace_back(std::move(frame.pixels));
    }

    auto& pframe = ppf->frames.back();
    pframe.frame_info.layer_info.crop_x0 = x0;
    pframe.frame_info.layer_info.crop_y0 = y0;
    pframe.frame_info.layer_info.xsize = xsize;
    pframe.frame_info.layer_info.ysize = ysize;
    pframe.frame_info.duration =
        fc.delay_num * 1000 / (fc.delay_den ? fc.delay_den : 100);
    pframe.frame_info.layer_info.blend_info.blendmode =
        should_blend ? JXL_BLEND_BLEND : JXL_BLEND_REPLACE;
    bool is_full_size = x0 == 0 && y0 == 0 && xsize == ppf->info.xsize &&
                        ysize == ppf->info.ysize;
    pframe.frame_info.layer_info.have_crop = is_full_size ? 0 : 1;
    pframe.frame_info.layer_info.blend_info.source = 1;
    pframe.frame_info.layer_info.blend_info.alpha = 0;
    pframe.frame_info.layer_info.save_as_reference = use_for_next_frame ? 1 : 0;

    previous_frame_should_be_cleared =
        has_nontrivial_background && (fc.dispose_op == DisposeOp::BACKGROUND);
  }

  if (ppf->frames.empty()) return JXL_FAILURE("No frames decoded");
  ppf->frames.back().frame_info.is_last = JXL_TRUE;

  return true;
}

#endif  // JPEGXL_ENABLE_APNG

}  // namespace extras
}  // namespace jxl
