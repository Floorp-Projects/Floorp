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

#include "lib/extras/codec_png.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Lodepng library:
#include <lodepng.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "lib/jxl/base/byte_order.h"
#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/file_io.h"
#include "lib/jxl/color_management.h"
#include "lib/jxl/common.h"
#include "lib/jxl/dec_external_image.h"
#include "lib/jxl/enc_external_image.h"
#include "lib/jxl/image.h"
#include "lib/jxl/image_bundle.h"
#include "lib/jxl/luminance.h"

namespace jxl {
namespace {

#define JXL_PNG_VERBOSE 0

// Retrieves XMP and EXIF/IPTC from itext and text.
class BlobsReaderPNG {
 public:
  static Status Decode(const LodePNGInfo& info, Blobs* blobs) {
    for (unsigned idx_itext = 0; idx_itext < info.itext_num; ++idx_itext) {
      // We trust these are properly null-terminated by LodePNG.
      const char* key = info.itext_keys[idx_itext];
      const char* value = info.itext_strings[idx_itext];
      if (strstr(key, "XML:com.adobe.xmp")) {
        blobs->xmp.resize(strlen(value));  // safe, see above
        memcpy(blobs->xmp.data(), value, blobs->xmp.size());
      }
    }

    for (unsigned idx_text = 0; idx_text < info.text_num; ++idx_text) {
      // We trust these are properly null-terminated by LodePNG.
      const char* key = info.text_keys[idx_text];
      const char* value = info.text_strings[idx_text];
      std::string type;
      PaddedBytes bytes;

      // Handle text chunks annotated with key "Raw profile type ####", with
      // #### a type, which may contain metadata.
      const char* kKey = "Raw profile type ";
      if (strncmp(key, kKey, strlen(kKey)) != 0) continue;

      if (!MaybeDecodeBase16(key, value, &type, &bytes)) {
        JXL_WARNING("Couldn't parse 'Raw format type' text chunk");
        continue;
      }
      if (type == "exif") {
        if (!blobs->exif.empty()) {
          JXL_WARNING("overwriting EXIF (%zu bytes) with base16 (%zu bytes)",
                      blobs->exif.size(), bytes.size());
        }
        blobs->exif = std::move(bytes);
      } else if (type == "iptc") {
        // TODO (jon): Deal with IPTC in some way
      } else if (type == "8bim") {
        // TODO (jon): Deal with 8bim in some way
      } else if (type == "xmp") {
        if (!blobs->xmp.empty()) {
          JXL_WARNING("overwriting XMP (%zu bytes) with base16 (%zu bytes)",
                      blobs->xmp.size(), bytes.size());
        }
        blobs->xmp = std::move(bytes);
      } else {
        JXL_WARNING(
            "Unknown type in 'Raw format type' text chunk: %s: %zu bytes",
            type.c_str(), bytes.size());
      }
    }

    return true;
  }

 private:
  // Returns false if invalid.
  static JXL_INLINE Status DecodeNibble(const char c,
                                        uint32_t* JXL_RESTRICT nibble) {
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

  // Parses a PNG text chunk with key of the form "Raw profile type ####", with
  // #### a type.
  // Returns whether it could successfully parse the content.
  // We trust key and encoded are null-terminated because they come from
  // LodePNG.
  static Status MaybeDecodeBase16(const char* key, const char* encoded,
                                  std::string* type, PaddedBytes* bytes) {
    const char* encoded_end = encoded + strlen(encoded);

    const char* kKey = "Raw profile type ";
    if (strncmp(key, kKey, strlen(kKey)) != 0) return false;
    *type = key + strlen(kKey);
    const size_t kMaxTypeLen = 20;
    if (type->length() > kMaxTypeLen) return false;  // Type too long

    // Header: freeform string and number of bytes
    unsigned long bytes_to_decode;
    int header_len;
    std::vector<char> description((encoded_end - encoded) + 1);
    const int fields = sscanf(encoded, "\n%[^\n]\n%8lu%n", description.data(),
                              &bytes_to_decode, &header_len);
    if (fields != 2) return false;  // Failed to decode metadata header
    JXL_ASSERT(bytes->empty());
    bytes->reserve(bytes_to_decode);

    // Encoding: base16 with newline after 72 chars.
    const char* pos = encoded + header_len;
    for (size_t i = 0; i < bytes_to_decode; ++i) {
      if (i % 36 == 0) {
        if (pos + 1 >= encoded_end) return false;  // Truncated base16 1
        if (*pos != '\n') return false;            // Expected newline
        ++pos;
      }

      if (pos + 2 >= encoded_end) return false;  // Truncated base16 2;
      uint32_t nibble0, nibble1;
      JXL_RETURN_IF_ERROR(DecodeNibble(pos[0], &nibble0));
      JXL_RETURN_IF_ERROR(DecodeNibble(pos[1], &nibble1));
      bytes->push_back(static_cast<uint8_t>((nibble0 << 4) + nibble1));
      pos += 2;
    }
    if (pos + 1 != encoded_end) return false;  // Too many encoded bytes
    if (pos[0] != '\n') return false;          // Incorrect metadata terminator
    return true;
  }
};

// Stores XMP and EXIF/IPTC into itext and text.
class BlobsWriterPNG {
 public:
  static Status Encode(const Blobs& blobs, LodePNGInfo* JXL_RESTRICT info) {
    if (!blobs.exif.empty()) {
      JXL_RETURN_IF_ERROR(EncodeBase16("exif", blobs.exif, info));
    }
    if (!blobs.iptc.empty()) {
      JXL_RETURN_IF_ERROR(EncodeBase16("iptc", blobs.iptc, info));
    }

    if (!blobs.xmp.empty()) {
      JXL_RETURN_IF_ERROR(EncodeBase16("xmp", blobs.xmp, info));

      // Below is the official way, but it does not seem to work in ImageMagick.
      // Exiv2 and exiftool are OK with either way of encoding XMP.
      if (/* DISABLES CODE */ (0)) {
        const char* key = "XML:com.adobe.xmp";
        const std::string text(reinterpret_cast<const char*>(blobs.xmp.data()),
                               blobs.xmp.size());
        if (lodepng_add_itext(info, key, "", "", text.c_str()) != 0) {
          return JXL_FAILURE("Failed to add itext");
        }
      }
    }

    return true;
  }

 private:
  static JXL_INLINE char EncodeNibble(const uint8_t nibble) {
    JXL_ASSERT(nibble < 16);
    return (nibble < 10) ? '0' + nibble : 'a' + nibble - 10;
  }

  static Status EncodeBase16(const std::string& type, const PaddedBytes& bytes,
                             LodePNGInfo* JXL_RESTRICT info) {
    // Encoding: base16 with newline after 72 chars.
    const size_t base16_size =
        2 * bytes.size() + DivCeil(bytes.size(), size_t(36)) + 1;
    std::string base16;
    base16.reserve(base16_size);
    for (size_t i = 0; i < bytes.size(); ++i) {
      if (i % 36 == 0) base16.push_back('\n');
      base16.push_back(EncodeNibble(bytes[i] >> 4));
      base16.push_back(EncodeNibble(bytes[i] & 0x0F));
    }
    base16.push_back('\n');
    JXL_ASSERT(base16.length() == base16_size);

    char key[30];
    snprintf(key, sizeof(key), "Raw profile type %s", type.c_str());

    char header[30];
    snprintf(header, sizeof(header), "\n%s\n%8zu", type.c_str(), bytes.size());

    const std::string& encoded = std::string(header) + base16;
    if (lodepng_add_text(info, key, encoded.c_str()) != 0) {
      return JXL_FAILURE("Failed to add text");
    }

    return true;
  }
};

// Retrieves ColorEncoding from PNG chunks.
class ColorEncodingReaderPNG {
 public:
  // Fills original->color_encoding or returns false.
  Status operator()(const Span<const uint8_t> bytes, const bool is_gray,
                    CodecInOut* io) {
    ColorEncoding* c_original = &io->metadata.m.color_encoding;
    JXL_RETURN_IF_ERROR(Decode(bytes, &io->blobs));

    const ColorSpace color_space =
        is_gray ? ColorSpace::kGray : ColorSpace::kRGB;

    if (have_pq_) {
      c_original->SetColorSpace(color_space);
      c_original->white_point = WhitePoint::kD65;
      c_original->primaries = Primaries::k2100;
      c_original->tf.SetTransferFunction(TransferFunction::kPQ);
      c_original->rendering_intent = RenderingIntent::kRelative;
      if (c_original->CreateICC()) return true;
      JXL_WARNING("Failed to synthesize BT.2100 PQ");
      // Else: try the actual ICC profile.
    }

    // ICC overrides anything else if present.
    if (c_original->SetICC(std::move(icc_))) {
      if (have_srgb_) {
        JXL_WARNING("Invalid PNG with both sRGB and ICC; ignoring sRGB");
      }
      if (is_gray != c_original->IsGray()) {
        return JXL_FAILURE("Mismatch between ICC and PNG header grayscale");
      }
      return true;  // it's fine to ignore gAMA/cHRM.
    }

    // PNG requires that sRGB override gAMA/cHRM.
    if (have_srgb_) {
      return c_original->SetSRGB(color_space, rendering_intent_);
    }

    // Try to create a custom profile:

    c_original->SetColorSpace(color_space);

    // Attempt to set whitepoint and primaries if there is a cHRM chunk, or else
    // use default sRGB (the PNG then is device-dependent).
    // In case of grayscale, do not attempt to set the primaries and ignore the
    // ones the PNG image has (but still set the white point).
    if (!have_chrm_ || !c_original->SetWhitePoint(white_point_) ||
        (!is_gray && !c_original->SetPrimaries(primaries_))) {
#if JXL_PNG_VERBOSE >= 1
      JXL_WARNING("No (valid) cHRM, assuming sRGB");
#endif
      c_original->white_point = WhitePoint::kD65;
      c_original->primaries = Primaries::kSRGB;
    }

    if (!have_gama_ || !c_original->tf.SetGamma(gamma_)) {
#if JXL_PNG_VERBOSE >= 1
      JXL_WARNING("No (valid) gAMA nor sRGB, assuming sRGB");
#endif
      c_original->tf.SetTransferFunction(TransferFunction::kSRGB);
    }

    c_original->rendering_intent = RenderingIntent::kRelative;
    if (c_original->CreateICC()) return true;

    JXL_WARNING(
        "DATA LOSS: unable to create an ICC profile for PNG gAMA/cHRM.\n"
        "Image pixels will be interpreted as sRGB. Please add an ICC \n"
        "profile to the input image");
    return c_original->SetSRGB(color_space);
  }

  // Whether the image has any color profile information (ICC chunk, sRGB
  // chunk, cHRM chunk, and so on), or has no color information chunks at all.
  bool HaveColorProfile() const {
    return have_pq_ || have_srgb_ || have_gama_ || have_chrm_ || have_icc_;
  }

 private:
  Status DecodeICC(const unsigned char* const payload,
                   const size_t payload_size) {
    if (payload_size == 0) return JXL_FAILURE("Empty ICC payload");
    const unsigned char* pos = payload;
    const unsigned char* end = payload + payload_size;

    // Profile name
    if (*pos == '\0') return JXL_FAILURE("Expected ICC name");
    for (size_t i = 0;; ++i) {
      if (i == 80) return JXL_FAILURE("ICC profile name too long");
      if (pos == end) return JXL_FAILURE("Not enough bytes for ICC name");
      if (*pos++ == '\0') break;
    }

    // Special case for BT.2100 PQ (https://w3c.github.io/png-hdr-pq/) - try to
    // synthesize the profile because table-based curves are less accurate.
    // strcmp is safe because we already verified the string is 0-terminated.
    if (!strcmp(reinterpret_cast<const char*>(payload), "ITUR_2100_PQ_FULL")) {
      have_pq_ = true;
    }

    // Skip over compression method (only one is allowed)
    if (pos == end) return JXL_FAILURE("Not enough bytes for ICC method");
    if (*pos++ != 0) return JXL_FAILURE("Unsupported ICC method");

    // Decompress
    unsigned char* icc_buf = nullptr;
    size_t icc_size = 0;
    LodePNGDecompressSettings settings;
    lodepng_decompress_settings_init(&settings);
    const unsigned err = lodepng_zlib_decompress(
        &icc_buf, &icc_size, pos, payload_size - (pos - payload), &settings);
    if (err == 0) {
      icc_.resize(icc_size);
      memcpy(icc_.data(), icc_buf, icc_size);
    }
    free(icc_buf);
    have_icc_ = true;
    return true;
  }

  // Returns floating-point value from the PNG encoding (times 10^5).
  static double F64FromU32(const uint32_t x) {
    return static_cast<int32_t>(x) * 1E-5;
  }

  Status DecodeSRGB(const unsigned char* payload, const size_t payload_size) {
    if (payload_size != 1) return JXL_FAILURE("Wrong sRGB size");
    // (PNG uses the same values as ICC.)
    rendering_intent_ = static_cast<RenderingIntent>(payload[0]);
    have_srgb_ = true;
    return true;
  }

  Status DecodeGAMA(const unsigned char* payload, const size_t payload_size) {
    if (payload_size != 4) return JXL_FAILURE("Wrong gAMA size");
    gamma_ = F64FromU32(LoadBE32(payload));
    have_gama_ = true;
    return true;
  }

  Status DecodeCHRM(const unsigned char* payload, const size_t payload_size) {
    if (payload_size != 32) return JXL_FAILURE("Wrong cHRM size");
    white_point_.x = F64FromU32(LoadBE32(payload + 0));
    white_point_.y = F64FromU32(LoadBE32(payload + 4));
    primaries_.r.x = F64FromU32(LoadBE32(payload + 8));
    primaries_.r.y = F64FromU32(LoadBE32(payload + 12));
    primaries_.g.x = F64FromU32(LoadBE32(payload + 16));
    primaries_.g.y = F64FromU32(LoadBE32(payload + 20));
    primaries_.b.x = F64FromU32(LoadBE32(payload + 24));
    primaries_.b.y = F64FromU32(LoadBE32(payload + 28));
    have_chrm_ = true;
    return true;
  }

  Status DecodeEXIF(const unsigned char* payload, const size_t payload_size,
                    Blobs* blobs) {
    // If we already have EXIF, keep the larger one.
    if (blobs->exif.size() > payload_size) return true;
    blobs->exif.resize(payload_size);
    memcpy(blobs->exif.data(), payload, payload_size);
    return true;
  }

  Status Decode(const Span<const uint8_t> bytes, Blobs* blobs) {
    // Look for colorimetry and text chunks in the PNG image. The PNG chunks
    // begin after the PNG magic header of 8 bytes.
    const unsigned char* chunk = bytes.data() + 8;
    const unsigned char* end = bytes.data() + bytes.size();
    for (;;) {
      // chunk points to the first field of a PNG chunk. The chunk has
      // respectively 4 bytes of length, 4 bytes type, length bytes of data,
      // 4 bytes CRC.
      if (chunk + 4 >= end) {
        break;  // Regular end reached.
      }

      char type_char[5];
      if (chunk + 8 >= end) {
        JXL_NOTIFY_ERROR("PNG: malformed chunk");
        break;
      }
      lodepng_chunk_type(type_char, chunk);
      std::string type = type_char;

      if (type == "acTL" || type == "fcTL" || type == "fdAT") {
        // this is an APNG file, without proper handling we would just return
        // the first frame, so for now codec_apng handles animation until the
        // animation chunk handling is added here
        return false;
      }
      if (type == "eXIf" || type == "iCCP" || type == "sRGB" ||
          type == "gAMA" || type == "cHRM") {
        const unsigned char* payload = lodepng_chunk_data_const(chunk);
        const size_t payload_size = lodepng_chunk_length(chunk);
        // The entire chunk needs also 4 bytes of CRC after the payload.
        if (payload + payload_size + 4 >= end) {
          JXL_NOTIFY_ERROR("PNG: truncated chunk");
          break;
        }
        if (lodepng_chunk_check_crc(chunk) != 0) {
          JXL_NOTIFY_ERROR("CRC mismatch in unknown PNG chunk");
          chunk = lodepng_chunk_next_const(chunk, end);
          continue;
        }

        if (type == "eXIf") {
          JXL_RETURN_IF_ERROR(DecodeEXIF(payload, payload_size, blobs));
        } else if (type == "iCCP") {
          JXL_RETURN_IF_ERROR(DecodeICC(payload, payload_size));
        } else if (type == "sRGB") {
          JXL_RETURN_IF_ERROR(DecodeSRGB(payload, payload_size));
        } else if (type == "gAMA") {
          JXL_RETURN_IF_ERROR(DecodeGAMA(payload, payload_size));
        } else if (type == "cHRM") {
          JXL_RETURN_IF_ERROR(DecodeCHRM(payload, payload_size));
        }
      }

      chunk = lodepng_chunk_next_const(chunk, end);
    }
    return true;
  }

  PaddedBytes icc_;

  bool have_pq_ = false;
  bool have_srgb_ = false;
  bool have_gama_ = false;
  bool have_chrm_ = false;
  bool have_icc_ = false;

  // Only valid if have_srgb_:
  RenderingIntent rendering_intent_;

  // Only valid if have_gama_:
  double gamma_;

  // Only valid if have_chrm_:
  CIExy white_point_;
  PrimariesCIExy primaries_;
};

Status ApplyHints(const bool is_gray, CodecInOut* io) {
  bool got_color_space = false;

  JXL_RETURN_IF_ERROR(io->dec_hints.Foreach(
      [is_gray, io, &got_color_space](const std::string& key,
                                      const std::string& value) -> Status {
        ColorEncoding* c_original = &io->metadata.m.color_encoding;
        if (key == "color_space") {
          if (!ParseDescription(value, c_original) ||
              !c_original->CreateICC()) {
            return JXL_FAILURE("PNG: Failed to apply color_space");
          }

          if (is_gray != io->metadata.m.color_encoding.IsGray()) {
            return JXL_FAILURE(
                "PNG: mismatch between file and color_space hint");
          }

          got_color_space = true;
        } else if (key == "icc_pathname") {
          PaddedBytes icc;
          JXL_RETURN_IF_ERROR(ReadFile(value, &icc));
          JXL_RETURN_IF_ERROR(c_original->SetICC(std::move(icc)));
          got_color_space = true;
        } else {
          JXL_WARNING("PNG decoder ignoring %s hint", key.c_str());
        }
        return true;
      }));

  if (!got_color_space) {
    JXL_WARNING("PNG: no color_space/icc_pathname given, assuming sRGB");
    JXL_RETURN_IF_ERROR(io->metadata.m.color_encoding.SetSRGB(
        is_gray ? ColorSpace::kGray : ColorSpace::kRGB));
  }

  return true;
}

// Stores ColorEncoding into PNG chunks.
class ColorEncodingWriterPNG {
 public:
  static Status Encode(const ColorEncoding& c, LodePNGInfo* JXL_RESTRICT info) {
    // Prefer to only write sRGB - smaller.
    if (c.IsSRGB()) {
      JXL_RETURN_IF_ERROR(AddSRGB(c, info));
      // PNG recommends not including both sRGB and iCCP, so skip the latter.
    } else {
      JXL_ASSERT(!c.ICC().empty());
      JXL_RETURN_IF_ERROR(AddICC(c.ICC(), info));
    }

    // gAMA and cHRM are always allowed but will be overridden by sRGB/iCCP.
    JXL_RETURN_IF_ERROR(MaybeAddGAMA(c, info));
    JXL_RETURN_IF_ERROR(MaybeAddCHRM(c, info));
    return true;
  }

 private:
  static Status AddChunk(const char* type, const PaddedBytes& payload,
                         LodePNGInfo* JXL_RESTRICT info) {
    // Ignore original location/order of chunks; place them in the first group.
    if (lodepng_chunk_create(&info->unknown_chunks_data[0],
                             &info->unknown_chunks_size[0], payload.size(),
                             type, payload.data()) != 0) {
      return JXL_FAILURE("Failed to add chunk");
    }
    return true;
  }

  static Status AddICC(const PaddedBytes& icc, LodePNGInfo* JXL_RESTRICT info) {
    LodePNGCompressSettings settings;
    lodepng_compress_settings_init(&settings);
    unsigned char* out = nullptr;
    size_t out_size = 0;
    if (lodepng_zlib_compress(&out, &out_size, icc.data(), icc.size(),
                              &settings) != 0) {
      return JXL_FAILURE("Failed to compress ICC");
    }

    PaddedBytes payload;
    payload.resize(3 + out_size);
    // TODO(janwas): use special name if PQ
    payload[0] = '1';  // profile name
    payload[1] = '\0';
    payload[2] = 0;  // compression method (zlib)
    memcpy(&payload[3], out, out_size);
    free(out);

    return AddChunk("iCCP", payload, info);
  }

  static Status AddSRGB(const ColorEncoding& c,
                        LodePNGInfo* JXL_RESTRICT info) {
    PaddedBytes payload;
    payload.push_back(static_cast<uint8_t>(c.rendering_intent));
    return AddChunk("sRGB", payload, info);
  }

  // Returns PNG encoding of floating-point value (times 10^5).
  static uint32_t U32FromF64(const double x) {
    return static_cast<int32_t>(roundf(x * 1E5));
  }

  static Status MaybeAddGAMA(const ColorEncoding& c,
                             LodePNGInfo* JXL_RESTRICT info) {
    if (!c.tf.IsGamma()) return true;
    const double gamma = c.tf.GetGamma();

    PaddedBytes payload(4);
    StoreBE32(U32FromF64(gamma), payload.data());
    return AddChunk("gAMA", payload, info);
  }

  static Status MaybeAddCHRM(const ColorEncoding& c,
                             LodePNGInfo* JXL_RESTRICT info) {
    // TODO(lode): remove this, PNG can also have cHRM for P3, sRGB, ...
    if (c.white_point != WhitePoint::kCustom &&
        c.primaries != Primaries::kCustom) {
      return true;
    }

    const CIExy white_point = c.GetWhitePoint();
    // A PNG image stores both whitepoint and primaries in the cHRM chunk, but
    // for grayscale images we don't have primaries. It does not matter what
    // values are stored in the PNG though (all colors are a multiple of the
    // whitepoint), so choose default ones. See
    // http://www.libpng.org/pub/png/spec/1.2/PNG-Chunks.html section 4.2.2.1.
    const PrimariesCIExy primaries =
        c.IsGray() ? ColorEncoding().GetPrimaries() : c.GetPrimaries();

    PaddedBytes payload(32);
    StoreBE32(U32FromF64(white_point.x), &payload[0]);
    StoreBE32(U32FromF64(white_point.y), &payload[4]);
    StoreBE32(U32FromF64(primaries.r.x), &payload[8]);
    StoreBE32(U32FromF64(primaries.r.y), &payload[12]);
    StoreBE32(U32FromF64(primaries.g.x), &payload[16]);
    StoreBE32(U32FromF64(primaries.g.y), &payload[20]);
    StoreBE32(U32FromF64(primaries.b.x), &payload[24]);
    StoreBE32(U32FromF64(primaries.b.y), &payload[28]);
    return AddChunk("cHRM", payload, info);
  }
};

// RAII - ensures state is freed even if returning early.
struct PNGState {
  PNGState() { lodepng_state_init(&s); }
  ~PNGState() { lodepng_state_cleanup(&s); }

  LodePNGState s;
};

Status CheckGray(const LodePNGColorMode& mode, bool has_icc, bool* is_gray) {
  switch (mode.colortype) {
    case LCT_GREY:
    case LCT_GREY_ALPHA:
      *is_gray = true;
      return true;

    case LCT_RGB:
    case LCT_RGBA:
      *is_gray = false;
      return true;

    case LCT_PALETTE: {
      if (has_icc) {
        // If an ICC profile is present, the PNG specification requires
        // palette to be intepreted as RGB colored, not grayscale, so we must
        // output color in that case and unfortunately can't optimize it to
        // gray if the palette only has gray entries.
        *is_gray = false;
        return true;
      } else {
        *is_gray = true;
        for (size_t i = 0; i < mode.palettesize; i++) {
          if (mode.palette[i * 4] != mode.palette[i * 4 + 1] ||
              mode.palette[i * 4] != mode.palette[i * 4 + 2]) {
            *is_gray = false;
            break;
          }
        }
        return true;
      }
    }

    default:
      *is_gray = false;
      return JXL_FAILURE("Unexpected PNG color type");
  }
}

Status CheckAlpha(const LodePNGColorMode& mode, bool* has_alpha) {
  if (mode.key_defined) {
    // Color key marks a single color as transparent.
    *has_alpha = true;
    return true;
  }

  switch (mode.colortype) {
    case LCT_GREY:
    case LCT_RGB:
      *has_alpha = false;
      return true;

    case LCT_GREY_ALPHA:
    case LCT_RGBA:
      *has_alpha = true;
      return true;

    case LCT_PALETTE: {
      *has_alpha = false;
      for (size_t i = 0; i < mode.palettesize; i++) {
        // PNG palettes are always 8-bit.
        if (mode.palette[i * 4 + 3] != 255) {
          *has_alpha = true;
          break;
        }
      }
      return true;
    }

    default:
      *has_alpha = false;
      return JXL_FAILURE("Unexpected PNG color type");
  }
}

LodePNGColorType MakeType(const bool is_gray, const bool has_alpha) {
  if (is_gray) {
    return has_alpha ? LCT_GREY_ALPHA : LCT_GREY;
  }
  return has_alpha ? LCT_RGBA : LCT_RGB;
}

// Inspects first chunk of the given type and updates state with the information
// when the chunk is relevant and present in the file.
Status InspectChunkType(const Span<const uint8_t> bytes,
                        const std::string& type, LodePNGState* state) {
  const unsigned char* chunk = lodepng_chunk_find_const(
      bytes.data(), bytes.data() + bytes.size(), type.c_str());
  if (chunk && lodepng_inspect_chunk(state, chunk - bytes.data(), bytes.data(),
                                     bytes.size()) != 0) {
    return JXL_FAILURE("Invalid chunk \"%s\" in PNG image", type.c_str());
  }
  return true;
}

}  // namespace

Status DecodeImagePNG(const Span<const uint8_t> bytes, ThreadPool* pool,
                      CodecInOut* io) {
  unsigned w, h;
  PNGState state;
  if (lodepng_inspect(&w, &h, &state.s, bytes.data(), bytes.size()) != 0) {
    return false;  // not an error - just wrong format
  }
  JXL_RETURN_IF_ERROR(VerifyDimensions(&io->constraints, w, h));
  io->SetSize(w, h);
  // Palette RGB values
  if (!InspectChunkType(bytes, "PLTE", &state.s)) {
    return false;
  }
  // Transparent color key, or palette transparency
  if (!InspectChunkType(bytes, "tRNS", &state.s)) {
    return false;
  }
  // ICC profile
  if (!InspectChunkType(bytes, "iCCP", &state.s)) {
    return false;
  }
  const LodePNGColorMode& color_mode = state.s.info_png.color;
  bool has_icc = state.s.info_png.iccp_defined;

  bool is_gray, has_alpha;
  JXL_RETURN_IF_ERROR(CheckGray(color_mode, has_icc, &is_gray));
  JXL_RETURN_IF_ERROR(CheckAlpha(color_mode, &has_alpha));
  // We want LodePNG to promote 1/2/4 bit pixels to 8.
  size_t bits_per_sample = std::max(color_mode.bitdepth, 8u);
  if (bits_per_sample != 8 && bits_per_sample != 16) {
    return JXL_FAILURE("Unexpected PNG bit depth");
  }
  io->metadata.m.SetUintSamples(static_cast<uint32_t>(bits_per_sample));
  io->metadata.m.SetAlphaBits(
      has_alpha ? io->metadata.m.bit_depth.bits_per_sample : 0);

  // Always decode to 8/16-bit RGB/RGBA, not LCT_PALETTE.
  state.s.info_raw.bitdepth = static_cast<unsigned>(bits_per_sample);
  state.s.info_raw.colortype = MakeType(is_gray, has_alpha);
  unsigned char* out = nullptr;
  const unsigned err =
      lodepng_decode(&out, &w, &h, &state.s, bytes.data(), bytes.size());
  // Automatically call free(out) on return.
  std::unique_ptr<unsigned char, void (*)(void*)> out_ptr{out, free};
  if (err != 0) {
    return JXL_FAILURE("PNG decode failed: %s", lodepng_error_text(err));
  }

  if (!BlobsReaderPNG::Decode(state.s.info_png, &io->blobs)) {
    JXL_WARNING("PNG metadata may be incomplete");
  }
  ColorEncodingReaderPNG reader;
  JXL_RETURN_IF_ERROR(reader(bytes, is_gray, io));
#if JXL_PNG_VERBOSE >= 1
  printf("PNG read %s\n", Description(io->metadata.m.color_encoding).c_str());
#endif

  const size_t num_channels = (is_gray ? 1 : 3) + has_alpha;
  const size_t out_size = w * h * num_channels * bits_per_sample / kBitsPerByte;

  const JxlEndianness endianness = JXL_BIG_ENDIAN;  // PNG requirement
  const Span<const uint8_t> span(out, out_size);
  const bool ok =
      ConvertFromExternal(span, w, h, io->metadata.m.color_encoding, has_alpha,
                          /*alpha_is_premultiplied=*/false,
                          io->metadata.m.bit_depth.bits_per_sample, endianness,
                          /*flipped_y=*/false, pool, &io->Main());
  JXL_RETURN_IF_ERROR(ok);
  io->dec_pixels = w * h;
  io->metadata.m.bit_depth.bits_per_sample = io->Main().DetectRealBitdepth();
  SetIntensityTarget(io);
  if (!reader.HaveColorProfile()) {
    JXL_RETURN_IF_ERROR(ApplyHints(is_gray, io));
  } else {
    (void)io->dec_hints.Foreach(
        [](const std::string& key, const std::string& /*value*/) {
          JXL_WARNING("PNG decoder ignoring %s hint", key.c_str());
          return true;
        });
  }
  return true;
}

Status EncodeImagePNG(const CodecInOut* io, const ColorEncoding& c_desired,
                      size_t bits_per_sample, ThreadPool* pool,
                      PaddedBytes* bytes) {
  if (bits_per_sample > 8) {
    bits_per_sample = 16;
  } else if (bits_per_sample < 8) {
    // PNG can also do 4, 2, and 1 bits per sample, but it isn't implemented
    bits_per_sample = 8;
  }
  ImageBundle ib = io->Main().Copy();
  const size_t alpha_bits = ib.HasAlpha() ? bits_per_sample : 0;
  ImageMetadata metadata = io->metadata.m;
  ImageBundle store(&metadata);
  const ImageBundle* transformed;
  JXL_RETURN_IF_ERROR(
      TransformIfNeeded(ib, c_desired, pool, &store, &transformed));
  size_t stride = ib.oriented_xsize() *
                  DivCeil(c_desired.Channels() * bits_per_sample + alpha_bits,
                          kBitsPerByte);
  PaddedBytes raw_bytes(stride * ib.oriented_ysize());
  JXL_RETURN_IF_ERROR(ConvertToExternal(
      *transformed, bits_per_sample, /*float_out=*/false,
      c_desired.Channels() + (ib.HasAlpha() ? 1 : 0), JXL_BIG_ENDIAN, stride,
      pool, raw_bytes.data(), raw_bytes.size(), /*out_callback=*/nullptr,
      /*out_opaque=*/nullptr, metadata.GetOrientation()));

  PNGState state;
  // For maximum compatibility, still store 8-bit even if pixels are all zero.
  state.s.encoder.auto_convert = 0;

  LodePNGInfo* info = &state.s.info_png;
  info->color.bitdepth = bits_per_sample;
  info->color.colortype = MakeType(ib.IsGray(), ib.HasAlpha());
  state.s.info_raw = info->color;

  JXL_RETURN_IF_ERROR(ColorEncodingWriterPNG::Encode(c_desired, info));
  JXL_RETURN_IF_ERROR(BlobsWriterPNG::Encode(io->blobs, info));

  unsigned char* out = nullptr;
  size_t out_size = 0;
  const unsigned err =
      lodepng_encode(&out, &out_size, raw_bytes.data(), ib.oriented_xsize(),
                     ib.oriented_ysize(), &state.s);
  // Automatically call free(out) on return.
  std::unique_ptr<unsigned char, void (*)(void*)> out_ptr{out, free};
  if (err != 0) {
    return JXL_FAILURE("Failed to encode PNG: %s", lodepng_error_text(err));
  }
  bytes->resize(out_size);
  memcpy(bytes->data(), out, out_size);
  return true;
}

}  // namespace jxl
