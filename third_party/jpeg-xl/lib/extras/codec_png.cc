// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

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
#include "lib/jxl/enc_image_bundle.h"
#include "lib/jxl/image.h"
#include "lib/jxl/image_bundle.h"
#include "lib/jxl/luminance.h"

namespace jxl {
namespace extras {
namespace {

#define JXL_PNG_VERBOSE 0

// Retrieves XMP and EXIF/IPTC from itext and text.
class BlobsReaderPNG {
 public:
  static Status Decode(const LodePNGInfo& info, PackedMetadata* metadata) {
    for (unsigned idx_itext = 0; idx_itext < info.itext_num; ++idx_itext) {
      // We trust these are properly null-terminated by LodePNG.
      const char* key = info.itext_keys[idx_itext];
      const char* value = info.itext_strings[idx_itext];
      if (strstr(key, "XML:com.adobe.xmp")) {
        metadata->xmp.resize(strlen(value));  // safe, see above
        memcpy(metadata->xmp.data(), value, metadata->xmp.size());
      }
    }

    for (unsigned idx_text = 0; idx_text < info.text_num; ++idx_text) {
      // We trust these are properly null-terminated by LodePNG.
      const char* key = info.text_keys[idx_text];
      const char* value = info.text_strings[idx_text];
      std::string type;
      std::vector<uint8_t> bytes;

      // Handle text chunks annotated with key "Raw profile type ####", with
      // #### a type, which may contain metadata.
      const char* kKey = "Raw profile type ";
      if (strncmp(key, kKey, strlen(kKey)) != 0) continue;

      if (!MaybeDecodeBase16(key, value, &type, &bytes)) {
        JXL_WARNING("Couldn't parse 'Raw format type' text chunk");
        continue;
      }
      if (type == "exif") {
        if (!metadata->exif.empty()) {
          JXL_WARNING("overwriting EXIF (%" PRIuS " bytes) with base16 (%" PRIuS
                      " bytes)",
                      metadata->exif.size(), bytes.size());
        }
        metadata->exif = std::move(bytes);
      } else if (type == "iptc") {
        // TODO (jon): Deal with IPTC in some way
      } else if (type == "8bim") {
        // TODO (jon): Deal with 8bim in some way
      } else if (type == "xmp") {
        if (!metadata->xmp.empty()) {
          JXL_WARNING("overwriting XMP (%" PRIuS " bytes) with base16 (%" PRIuS
                      " bytes)",
                      metadata->xmp.size(), bytes.size());
        }
        metadata->xmp = std::move(bytes);
      } else {
        JXL_WARNING("Unknown type in 'Raw format type' text chunk: %s: %" PRIuS
                    " bytes",
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
                                  std::string* type,
                                  std::vector<uint8_t>* bytes) {
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
    unsigned long bytes_to_decode;
    const int fields = sscanf(pos, "%8lu", &bytes_to_decode);
    if (fields != 1) return false;  // Failed to decode metadata header
    JXL_ASSERT(pos + 8 <= encoded_end);
    pos += 8;  // read %8lu

    // We need 2*bytes for the hex values plus 1 byte every 36 values.
    const unsigned long needed_bytes =
        bytes_to_decode * 2 + 1 + DivCeil(bytes_to_decode, 36);
    if (needed_bytes != static_cast<size_t>(encoded_end - pos)) {
      return JXL_FAILURE("Not enough bytes to parse %lu bytes in hex",
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
    snprintf(header, sizeof(header), "\n%s\n%8" PRIuS, type.c_str(),
             bytes.size());

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
                    PackedPixelFile* ppf) {
    JXL_RETURN_IF_ERROR(Decode(bytes, &ppf->metadata, &ppf->color_encoding));

    const JxlColorSpace color_space =
        is_gray ? JXL_COLOR_SPACE_GRAY : JXL_COLOR_SPACE_RGB;
    ppf->color_encoding.color_space = color_space;

    if (have_pq_) {
      // Synthesize the ICC with these parameters instead.
      ppf->color_encoding.white_point = JXL_WHITE_POINT_D65;
      ppf->color_encoding.primaries = JXL_PRIMARIES_2100;
      ppf->color_encoding.transfer_function = JXL_TRANSFER_FUNCTION_PQ;
      ppf->color_encoding.rendering_intent = JXL_RENDERING_INTENT_RELATIVE;
      return true;
    }

    // ICC overrides anything else if present.
    ppf->icc = std::move(icc_);

    // PNG requires that sRGB override gAMA/cHRM.
    if (have_srgb_) {
      ppf->icc.clear();
      ppf->color_encoding.white_point = JXL_WHITE_POINT_D65;
      ppf->color_encoding.primaries = JXL_PRIMARIES_SRGB;
      ppf->color_encoding.transfer_function = JXL_TRANSFER_FUNCTION_SRGB;
      ppf->color_encoding.rendering_intent = JXL_RENDERING_INTENT_PERCEPTUAL;
      return true;
    }

    // Try to create a custom profile:

    // Attempt to set whitepoint and primaries if there is a cHRM chunk, or else
    // use default sRGB (the PNG then is device-dependent).
    // In case of grayscale, do not attempt to set the primaries and ignore the
    // ones the PNG image has (but still set the white point).
    if (!have_chrm_) {
#if JXL_PNG_VERBOSE >= 1
      JXL_WARNING("No cHRM, assuming sRGB");
#endif
      ppf->color_encoding.white_point = JXL_WHITE_POINT_D65;
      ppf->color_encoding.primaries = JXL_PRIMARIES_SRGB;
    }

    if (!have_gama_) {
#if JXL_PNG_VERBOSE >= 1
      JXL_WARNING("No gAMA nor sRGB, assuming sRGB");
#endif
      ppf->color_encoding.transfer_function = JXL_TRANSFER_FUNCTION_SRGB;
    }

    ppf->color_encoding.rendering_intent = JXL_RENDERING_INTENT_RELATIVE;
    return true;
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

  Status DecodeSRGB(const unsigned char* payload, const size_t payload_size,
                    JxlColorEncoding* color_encoding) {
    if (payload_size != 1) return JXL_FAILURE("Wrong sRGB size");
    // (PNG uses the same values as ICC.)
    if (payload[0] >= 4) return JXL_FAILURE("Invalid Rendering Intent");
    color_encoding->rendering_intent =
        static_cast<JxlRenderingIntent>(payload[0]);
    have_srgb_ = true;
    return true;
  }

  Status DecodeGAMA(const unsigned char* payload, const size_t payload_size,
                    JxlColorEncoding* color_encoding) {
    if (payload_size != 4) return JXL_FAILURE("Wrong gAMA size");
    color_encoding->transfer_function = JXL_TRANSFER_FUNCTION_GAMMA;
    color_encoding->gamma = F64FromU32(LoadBE32(payload));
    have_gama_ = true;
    return true;
  }

  Status DecodeCHRM(const unsigned char* payload, const size_t payload_size,
                    JxlColorEncoding* color_encoding) {
    if (payload_size != 32) return JXL_FAILURE("Wrong cHRM size");

    color_encoding->white_point = JXL_WHITE_POINT_CUSTOM;
    color_encoding->white_point_xy[0] = F64FromU32(LoadBE32(payload + 0));
    color_encoding->white_point_xy[1] = F64FromU32(LoadBE32(payload + 4));

    color_encoding->primaries = JXL_PRIMARIES_CUSTOM;
    color_encoding->primaries_red_xy[0] = F64FromU32(LoadBE32(payload + 8));
    color_encoding->primaries_red_xy[1] = F64FromU32(LoadBE32(payload + 12));
    color_encoding->primaries_green_xy[0] = F64FromU32(LoadBE32(payload + 16));
    color_encoding->primaries_green_xy[1] = F64FromU32(LoadBE32(payload + 20));
    color_encoding->primaries_blue_xy[0] = F64FromU32(LoadBE32(payload + 24));
    color_encoding->primaries_blue_xy[1] = F64FromU32(LoadBE32(payload + 28));

    have_chrm_ = true;
    return true;
  }

  Status DecodeEXIF(const unsigned char* payload, const size_t payload_size,
                    PackedMetadata* metadata) {
    // If we already have EXIF, keep the larger one.
    if (metadata->exif.size() > payload_size) return true;
    metadata->exif.resize(payload_size);
    memcpy(metadata->exif.data(), payload, payload_size);
    return true;
  }

  Status Decode(const Span<const uint8_t> bytes, PackedMetadata* metadata,
                JxlColorEncoding* color_encoding) {
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
          JXL_RETURN_IF_ERROR(DecodeEXIF(payload, payload_size, metadata));
        } else if (type == "iCCP") {
          JXL_RETURN_IF_ERROR(DecodeICC(payload, payload_size));
        } else if (type == "sRGB") {
          JXL_RETURN_IF_ERROR(
              DecodeSRGB(payload, payload_size, color_encoding));
        } else if (type == "gAMA") {
          JXL_RETURN_IF_ERROR(
              DecodeGAMA(payload, payload_size, color_encoding));
        } else if (type == "cHRM") {
          JXL_RETURN_IF_ERROR(
              DecodeCHRM(payload, payload_size, color_encoding));
        }
      }

      chunk = lodepng_chunk_next_const(chunk, end);
    }
    return true;
  }

  std::vector<uint8_t> icc_;

  bool have_pq_ = false;
  bool have_srgb_ = false;
  bool have_gama_ = false;
  bool have_chrm_ = false;
  bool have_icc_ = false;
};

// Stores ColorEncoding into PNG chunks.
class ColorEncodingWriterPNG {
 public:
  static Status Encode(const ColorEncoding& c, LodePNGInfo* JXL_RESTRICT info) {
    // Prefer to only write sRGB - smaller.
    if (c.IsSRGB()) {
      JXL_RETURN_IF_ERROR(AddSRGB(c, info));
      // PNG recommends not including both sRGB and iCCP, so skip the latter.
    } else if (!c.HaveFields() || !c.tf.IsGamma()) {
      // Having a gamma value means that the source was a PNG with gAMA and
      // without iCCP.
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
    double gamma;
    if (c.tf.IsGamma()) {
      gamma = c.tf.GetGamma();
    } else if (c.tf.IsLinear()) {
      gamma = 1;
    } else if (c.tf.IsSRGB()) {
      gamma = 0.45455;
    } else {
      return true;
    }

    PaddedBytes payload(4);
    StoreBE32(U32FromF64(gamma), payload.data());
    return AddChunk("gAMA", payload, info);
  }

  static Status MaybeAddCHRM(const ColorEncoding& c,
                             LodePNGInfo* JXL_RESTRICT info) {
    CIExy white_point = c.GetWhitePoint();
    // A PNG image stores both whitepoint and primaries in the cHRM chunk, but
    // for grayscale images we don't have primaries. It does not matter what
    // values are stored in the PNG though (all colors are a multiple of the
    // whitepoint), so choose default ones. See
    // http://www.libpng.org/pub/png/spec/1.2/PNG-Chunks.html section 4.2.2.1.
    PrimariesCIExy primaries =
        c.IsGray() ? ColorEncoding().GetPrimaries() : c.GetPrimaries();

    if (c.primaries == Primaries::kSRGB && c.white_point == WhitePoint::kD65) {
      // For sRGB, the cHRM chunk is supposed to have very specific values which
      // don't quite match the pre-quantized ones we have (red is off by
      // 0.00010). Technically, this is only required for full sRGB, but for
      // consistency, we might as well use them whenever the primaries and white
      // point are sRGB's.
      white_point.x = 0.31270;
      white_point.y = 0.32900;
      primaries.r.x = 0.64000;
      primaries.r.y = 0.33000;
      primaries.g.x = 0.30000;
      primaries.g.y = 0.60000;
      primaries.b.x = 0.15000;
      primaries.b.y = 0.06000;
    }

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
        // palette to be interpreted as RGB colored, not grayscale, so we must
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

Status DecodeImagePNG(const Span<const uint8_t> bytes,
                      const ColorHints& color_hints,
                      const SizeConstraints& constraints,
                      PackedPixelFile* ppf) {
  unsigned w, h;
  PNGState state;
  if (lodepng_inspect(&w, &h, &state.s, bytes.data(), bytes.size()) != 0) {
    return false;  // not an error - just wrong format
  }
  JXL_RETURN_IF_ERROR(VerifyDimensions(&constraints, w, h));

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

  if (!BlobsReaderPNG::Decode(state.s.info_png, &ppf->metadata)) {
    JXL_WARNING("PNG metadata may be incomplete");
  }
  ColorEncodingReaderPNG reader;
  JXL_RETURN_IF_ERROR(reader(bytes, is_gray, ppf));

  const uint32_t num_channels = (is_gray ? 1 : 3) + has_alpha;

  ppf->info.xsize = w;
  ppf->info.ysize = h;
  // Original data is uint, so exponent_bits_per_sample = 0.
  ppf->info.bits_per_sample = bits_per_sample;
  ppf->info.exponent_bits_per_sample = 0;
  ppf->info.uses_original_profile = true;
  ppf->info.have_preview = false;
  ppf->info.have_animation = false;

  ppf->info.alpha_bits = has_alpha ? bits_per_sample : 0;
  ppf->info.num_color_channels = is_gray ? 1 : 3;

  const JxlPixelFormat format{
      /*num_channels=*/num_channels,
      /*data_type=*/bits_per_sample == 16 ? JXL_TYPE_UINT16 : JXL_TYPE_UINT8,
      /*endianness=*/JXL_BIG_ENDIAN,  // PNG requirement
      /*align=*/0,
  };
  const size_t out_size = static_cast<size_t>(w) * h * num_channels *
                          bits_per_sample / kBitsPerByte;
  ppf->frames.emplace_back(w, h, format, out_ptr.release(), out_size);

  JXL_RETURN_IF_ERROR(
      ApplyColorHints(color_hints, reader.HaveColorProfile(), is_gray, ppf));
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

}  // namespace extras
}  // namespace jxl
