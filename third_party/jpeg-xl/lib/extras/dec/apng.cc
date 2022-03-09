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

#include <stdio.h>
#include <string.h>

#include <string>
#include <utility>
#include <vector>

#include "jxl/encode.h"
#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/printf_macros.h"
#include "lib/jxl/base/scope_guard.h"
#include "lib/jxl/common.h"
#include "lib/jxl/sanitizers.h"
#include "png.h" /* original (unpatched) libpng is ok */

namespace jxl {
namespace extras {

namespace {

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
  return true;
}

Status DecodeGAMA(const unsigned char* payload, const size_t payload_size,
                  JxlColorEncoding* color_encoding) {
  if (payload_size != 4) return JXL_FAILURE("Wrong gAMA size");
  color_encoding->transfer_function = JXL_TRANSFER_FUNCTION_GAMMA;
  color_encoding->gamma = F64FromU32(LoadBE32(payload));
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
  return true;
}

// Retrieves XMP and EXIF/IPTC from itext and text.
class BlobsReaderPNG {
 public:
  static Status Decode(const png_text_struct& info, PackedMetadata* metadata) {
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
  // libpng.
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

constexpr bool isAbc(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

constexpr uint32_t kId_IHDR = 0x52444849;
constexpr uint32_t kId_acTL = 0x4C546361;
constexpr uint32_t kId_fcTL = 0x4C546366;
constexpr uint32_t kId_IDAT = 0x54414449;
constexpr uint32_t kId_fdAT = 0x54416466;
constexpr uint32_t kId_IEND = 0x444E4549;
constexpr uint32_t kId_iCCP = 0x50434369;
constexpr uint32_t kId_sRGB = 0x42475273;
constexpr uint32_t kId_gAMA = 0x414D4167;
constexpr uint32_t kId_cHRM = 0x4D524863;
constexpr uint32_t kId_eXIf = 0x66495865;

struct APNGFrame {
  PaddedBytes pixels;
  std::vector<uint8_t*> rows;
  unsigned int w, h, delay_num, delay_den;
};

struct Reader {
  const uint8_t* next;
  const uint8_t* last;
  bool Read(void* data, size_t len) {
    size_t cap = last - next;
    size_t to_copy = std::min(cap, len);
    memcpy(data, next, to_copy);
    next += to_copy;
    return (len == to_copy);
  }
  bool Eof() { return next == last; }
};

const unsigned long cMaxPNGSize = 1000000UL;
const size_t kMaxPNGChunkSize = 100000000;  // 100 MB

void info_fn(png_structp png_ptr, png_infop info_ptr) {
  png_set_expand(png_ptr);
  png_set_palette_to_rgb(png_ptr);
  png_set_tRNS_to_alpha(png_ptr);
  (void)png_set_interlace_handling(png_ptr);
  png_read_update_info(png_ptr, info_ptr);
}

void row_fn(png_structp png_ptr, png_bytep new_row, png_uint_32 row_num,
            int pass) {
  APNGFrame* frame = (APNGFrame*)png_get_progressive_ptr(png_ptr);
  JXL_CHECK(frame);
  JXL_CHECK(frame->rows[row_num] < frame->pixels.data() + frame->pixels.size());
  png_progressive_combine_row(png_ptr, frame->rows[row_num], new_row);
}

inline unsigned int read_chunk(Reader* r, PaddedBytes* pChunk) {
  unsigned char len[4];
  if (r->Read(&len, 4)) {
    const auto size = png_get_uint_32(len);
    // Check first, to avoid overflow.
    if (size > kMaxPNGChunkSize) {
      JXL_WARNING("APNG chunk size is too big");
      return 0;
    }
    pChunk->resize(size + 12);
    memcpy(pChunk->data(), len, 4);
    if (r->Read(pChunk->data() + 4, pChunk->size() - 4)) {
      return LoadLE32(pChunk->data() + 4);
    }
  }
  return 0;
}

int processing_start(png_structp& png_ptr, png_infop& info_ptr, void* frame_ptr,
                     bool hasInfo, PaddedBytes& chunkIHDR,
                     std::vector<PaddedBytes>& chunksInfo) {
  unsigned char header[8] = {137, 80, 78, 71, 13, 10, 26, 10};

  png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  info_ptr = png_create_info_struct(png_ptr);
  if (!png_ptr || !info_ptr) return 1;

  if (setjmp(png_jmpbuf(png_ptr))) {
    return 1;
  }

  png_set_crc_action(png_ptr, PNG_CRC_QUIET_USE, PNG_CRC_QUIET_USE);
  png_set_progressive_read_fn(png_ptr, frame_ptr, info_fn, row_fn, NULL);

  png_process_data(png_ptr, info_ptr, header, 8);
  png_process_data(png_ptr, info_ptr, chunkIHDR.data(), chunkIHDR.size());

  if (hasInfo) {
    for (unsigned int i = 0; i < chunksInfo.size(); i++) {
      png_process_data(png_ptr, info_ptr, chunksInfo[i].data(),
                       chunksInfo[i].size());
    }
  }
  return 0;
}

int processing_data(png_structp png_ptr, png_infop info_ptr, unsigned char* p,
                    unsigned int size) {
  if (!png_ptr || !info_ptr) return 1;

  if (setjmp(png_jmpbuf(png_ptr))) {
    return 1;
  }

  png_process_data(png_ptr, info_ptr, p, size);
  return 0;
}

int processing_finish(png_structp png_ptr, png_infop info_ptr,
                      PackedMetadata* metadata) {
  unsigned char footer[12] = {0, 0, 0, 0, 73, 69, 78, 68, 174, 66, 96, 130};

  if (!png_ptr || !info_ptr) return 1;

  if (setjmp(png_jmpbuf(png_ptr))) {
    return 1;
  }

  png_process_data(png_ptr, info_ptr, footer, 12);
  // before destroying: check if we encountered any metadata chunks
  png_textp text_ptr;
  int num_text;
  png_get_text(png_ptr, info_ptr, &text_ptr, &num_text);
  for (int i = 0; i < num_text; i++) {
    (void)BlobsReaderPNG::Decode(text_ptr[i], metadata);
  }

  return 0;
}

}  // namespace

Status DecodeImageAPNG(const Span<const uint8_t> bytes,
                       const ColorHints& color_hints,
                       const SizeConstraints& constraints,
                       PackedPixelFile* ppf) {
  Reader r;
  unsigned int id, j, w, h, w0, h0, x0, y0;
  unsigned int delay_num, delay_den, dop, bop, rowbytes, imagesize;
  unsigned char sig[8];
  png_structp png_ptr = nullptr;
  png_infop info_ptr = nullptr;
  PaddedBytes chunk;
  PaddedBytes chunkIHDR;
  std::vector<PaddedBytes> chunksInfo;
  bool isAnimated = false;
  bool hasInfo = false;
  APNGFrame frameRaw = {};
  uint32_t num_channels;
  JxlPixelFormat format;
  unsigned int bytes_per_pixel = 0;

  struct FrameInfo {
    PackedImage data;
    uint32_t duration;
    size_t x0, xsize;
    size_t y0, ysize;
    uint32_t dispose_op;
    uint32_t blend_op;
  };

  std::vector<FrameInfo> frames;

  // Make sure png memory is released in any case.
  auto scope_guard = MakeScopeGuard([&]() {
    png_destroy_read_struct(&png_ptr, &info_ptr, 0);
    // Just in case. Not all versions on libpng wipe-out the pointers.
    png_ptr = nullptr;
    info_ptr = nullptr;
  });

  r = {bytes.data(), bytes.data() + bytes.size()};
  // Not a PNG => not an error
  unsigned char png_signature[8] = {137, 80, 78, 71, 13, 10, 26, 10};
  if (!r.Read(sig, 8) || memcmp(sig, png_signature, 8) != 0) {
    return false;
  }
  id = read_chunk(&r, &chunkIHDR);

  ppf->info.exponent_bits_per_sample = 0;
  ppf->info.alpha_exponent_bits = 0;
  ppf->info.orientation = JXL_ORIENT_IDENTITY;

  ppf->frames.clear();

  bool have_color = false, have_srgb = false;
  bool errorstate = true;
  if (id == kId_IHDR && chunkIHDR.size() == 25) {
    x0 = 0;
    y0 = 0;
    delay_num = 1;
    delay_den = 10;
    dop = 0;
    bop = 0;

    w0 = w = png_get_uint_32(chunkIHDR.data() + 8);
    h0 = h = png_get_uint_32(chunkIHDR.data() + 12);
    if (w > cMaxPNGSize || h > cMaxPNGSize) {
      return false;
    }

    // default settings in case e.g. only gAMA is given
    ppf->color_encoding.color_space = JXL_COLOR_SPACE_RGB;
    ppf->color_encoding.white_point = JXL_WHITE_POINT_D65;
    ppf->color_encoding.primaries = JXL_PRIMARIES_SRGB;
    ppf->color_encoding.transfer_function = JXL_TRANSFER_FUNCTION_SRGB;

    if (!processing_start(png_ptr, info_ptr, (void*)&frameRaw, hasInfo,
                          chunkIHDR, chunksInfo)) {
      while (!r.Eof()) {
        id = read_chunk(&r, &chunk);
        if (!id) break;

        if (id == kId_acTL && !hasInfo && !isAnimated) {
          isAnimated = true;
          ppf->info.have_animation = true;
          ppf->info.animation.tps_numerator = 1000;
          ppf->info.animation.tps_denominator = 1;
        } else if (id == kId_IEND ||
                   (id == kId_fcTL && (!hasInfo || isAnimated))) {
          if (hasInfo) {
            if (!processing_finish(png_ptr, info_ptr, &ppf->metadata)) {
              // Allocates the frame buffer.
              uint32_t duration = delay_num * 1000 / delay_den;
              frames.push_back(FrameInfo{PackedImage(w0, h0, format), duration,
                                         x0, w0, y0, h0, dop, bop});
              auto& frame = frames.back().data;
              for (size_t y = 0; y < h0; ++y) {
                memcpy(static_cast<uint8_t*>(frame.pixels()) + frame.stride * y,
                       frameRaw.rows[y], bytes_per_pixel * w0);
              }
            } else {
              break;
            }
          }

          if (id == kId_IEND) {
            errorstate = false;
            break;
          }
          if (chunk.size() < 34) {
            return JXL_FAILURE("Received a chunk that is too small (%" PRIuS
                               "B)",
                               chunk.size());
          }
          // At this point the old frame is done. Let's start a new one.
          w0 = png_get_uint_32(chunk.data() + 12);
          h0 = png_get_uint_32(chunk.data() + 16);
          x0 = png_get_uint_32(chunk.data() + 20);
          y0 = png_get_uint_32(chunk.data() + 24);
          delay_num = png_get_uint_16(chunk.data() + 28);
          delay_den = png_get_uint_16(chunk.data() + 30);
          dop = chunk[32];
          bop = chunk[33];

          if (!delay_den) delay_den = 100;

          if (w0 > cMaxPNGSize || h0 > cMaxPNGSize || x0 > cMaxPNGSize ||
              y0 > cMaxPNGSize || x0 + w0 > w || y0 + h0 > h || dop > 2 ||
              bop > 1) {
            break;
          }

          if (hasInfo) {
            memcpy(chunkIHDR.data() + 8, chunk.data() + 12, 8);
            if (processing_start(png_ptr, info_ptr, (void*)&frameRaw, hasInfo,
                                 chunkIHDR, chunksInfo)) {
              break;
            }
          }
        } else if (id == kId_IDAT) {
          // First IDAT chunk means we now have all header info
          hasInfo = true;
          JXL_CHECK(w == png_get_image_width(png_ptr, info_ptr));
          JXL_CHECK(h == png_get_image_height(png_ptr, info_ptr));
          int colortype = png_get_color_type(png_ptr, info_ptr);
          ppf->info.bits_per_sample = png_get_bit_depth(png_ptr, info_ptr);
          png_color_8p sigbits = NULL;
          png_get_sBIT(png_ptr, info_ptr, &sigbits);
          if (colortype & 1) {
            // palette will actually be 8-bit regardless of the index bitdepth
            ppf->info.bits_per_sample = 8;
          }
          if (colortype & 2) {
            ppf->info.num_color_channels = 3;
            ppf->color_encoding.color_space = JXL_COLOR_SPACE_RGB;
            if (sigbits && sigbits->red == sigbits->green &&
                sigbits->green == sigbits->blue)
              ppf->info.bits_per_sample = sigbits->red;
          } else {
            ppf->info.num_color_channels = 1;
            ppf->color_encoding.color_space = JXL_COLOR_SPACE_GRAY;
            if (sigbits) ppf->info.bits_per_sample = sigbits->gray;
          }
          if (colortype & 4 ||
              png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
            ppf->info.alpha_bits = ppf->info.bits_per_sample;
            if (sigbits) {
              if (sigbits->alpha &&
                  sigbits->alpha != ppf->info.bits_per_sample) {
                return JXL_FAILURE("Unsupported alpha bit-depth");
              }
              ppf->info.alpha_bits = sigbits->alpha;
            }
          } else {
            ppf->info.alpha_bits = 0;
          }
          ppf->color_encoding.color_space =
              (ppf->info.num_color_channels == 1 ? JXL_COLOR_SPACE_GRAY
                                                 : JXL_COLOR_SPACE_RGB);
          ppf->info.xsize = w;
          ppf->info.ysize = h;
          JXL_RETURN_IF_ERROR(VerifyDimensions(&constraints, w, h));
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
          rowbytes = w * bytes_per_pixel;
          imagesize = h * rowbytes;
          frameRaw.pixels.resize(imagesize);
          frameRaw.rows.resize(h);
          for (j = 0; j < h; j++)
            frameRaw.rows[j] = frameRaw.pixels.data() + j * rowbytes;

          if (processing_data(png_ptr, info_ptr, chunk.data(), chunk.size())) {
            break;
          }
        } else if (id == kId_fdAT && isAnimated) {
          png_save_uint_32(chunk.data() + 4, chunk.size() - 16);
          memcpy(chunk.data() + 8, "IDAT", 4);
          if (processing_data(png_ptr, info_ptr, chunk.data() + 4,
                              chunk.size() - 4)) {
            break;
          }
        } else if (id == kId_iCCP) {
          if (processing_data(png_ptr, info_ptr, chunk.data(), chunk.size())) {
            JXL_WARNING("Corrupt iCCP chunk");
            break;
          }

          // TODO(jon): catch special case of PQ and synthesize color encoding
          // in that case
          int compression_type;
          png_bytep profile;
          png_charp name;
          png_uint_32 proflen;
          png_get_iCCP(png_ptr, info_ptr, &name, &compression_type, &profile,
                       &proflen);
          ppf->icc.resize(proflen);
          memcpy(ppf->icc.data(), profile, proflen);
          have_color = true;
        } else if (id == kId_sRGB) {
          JXL_RETURN_IF_ERROR(DecodeSRGB(chunk.data() + 8, chunk.size() - 12,
                                         &ppf->color_encoding));
          have_srgb = true;
          have_color = true;
        } else if (id == kId_gAMA) {
          JXL_RETURN_IF_ERROR(DecodeGAMA(chunk.data() + 8, chunk.size() - 12,
                                         &ppf->color_encoding));
          have_color = true;
        } else if (id == kId_cHRM) {
          JXL_RETURN_IF_ERROR(DecodeCHRM(chunk.data() + 8, chunk.size() - 12,
                                         &ppf->color_encoding));
          have_color = true;
        } else if (id == kId_eXIf) {
          ppf->metadata.exif.resize(chunk.size() - 12);
          memcpy(ppf->metadata.exif.data(), chunk.data() + 8,
                 chunk.size() - 12);
        } else if (!isAbc(chunk[4]) || !isAbc(chunk[5]) || !isAbc(chunk[6]) ||
                   !isAbc(chunk[7])) {
          break;
        } else {
          if (processing_data(png_ptr, info_ptr, chunk.data(), chunk.size())) {
            break;
          }
          if (!hasInfo) {
            chunksInfo.push_back(chunk);
            continue;
          }
        }
      }
    }

    if (have_srgb) {
      ppf->color_encoding.white_point = JXL_WHITE_POINT_D65;
      ppf->color_encoding.primaries = JXL_PRIMARIES_SRGB;
      ppf->color_encoding.transfer_function = JXL_TRANSFER_FUNCTION_SRGB;
      ppf->color_encoding.rendering_intent = JXL_RENDERING_INTENT_PERCEPTUAL;
    }
    JXL_RETURN_IF_ERROR(ApplyColorHints(
        color_hints, have_color, ppf->info.num_color_channels == 1, ppf));
  }

  if (errorstate) return false;

  bool has_nontrivial_background = false;
  bool previous_frame_should_be_cleared = false;
  enum {
    DISPOSE_OP_NONE = 0,
    DISPOSE_OP_BACKGROUND = 1,
    DISPOSE_OP_PREVIOUS = 2,
  };
  enum {
    BLEND_OP_SOURCE = 0,
    BLEND_OP_OVER = 1,
  };
  for (size_t i = 0; i < frames.size(); i++) {
    auto& frame = frames[i];
    JXL_ASSERT(frame.data.xsize == frame.xsize);
    JXL_ASSERT(frame.data.ysize == frame.ysize);

    // Before encountering a DISPOSE_OP_NONE frame, the canvas is filled with 0,
    // so DISPOSE_OP_BACKGROUND and DISPOSE_OP_PREVIOUS are equivalent.
    if (frame.dispose_op == DISPOSE_OP_NONE) {
      has_nontrivial_background = true;
    }
    bool should_blend = frame.blend_op == BLEND_OP_OVER;
    bool use_for_next_frame =
        has_nontrivial_background && frame.dispose_op != DISPOSE_OP_PREVIOUS;
    size_t x0 = frame.x0;
    size_t y0 = frame.y0;

    if (previous_frame_should_be_cleared) {
      size_t xs = frame.data.xsize;
      size_t ys = frame.data.ysize;
      size_t px0 = frames[i - 1].x0;
      size_t py0 = frames[i - 1].y0;
      size_t pxs = frames[i - 1].xsize;
      size_t pys = frames[i - 1].ysize;
      if (px0 >= x0 && py0 >= y0 && px0 + pxs <= x0 + xs &&
          py0 + pys <= y0 + ys && frame.blend_op == BLEND_OP_SOURCE &&
          use_for_next_frame) {
        // If the previous frame is entirely contained in the current frame and
        // we are using BLEND_OP_SOURCE, nothing special needs to be done.
        ppf->frames.emplace_back(std::move(frame.data));
      } else if (px0 == x0 && py0 == y0 && px0 + pxs == x0 + xs &&
                 py0 + pys == y0 + ys && use_for_next_frame) {
        // If the new frame has the same size as the old one, but we are
        // blending, we can instead just not blend.
        should_blend = false;
        ppf->frames.emplace_back(std::move(frame.data));
      } else if (px0 <= x0 && py0 <= y0 && px0 + pxs >= x0 + xs &&
                 py0 + pys >= y0 + ys && use_for_next_frame) {
        // If the new frame is contained within the old frame, we can pad the
        // new frame with zeros and not blend.
        PackedImage new_data(pxs, pys, frame.data.format);
        memset(new_data.pixels(), 0, new_data.pixels_size);
        for (size_t y = 0; y < ys; y++) {
          size_t bytes_per_pixel =
              PackedImage::BitsPerChannel(new_data.format.data_type) *
              new_data.format.num_channels / 8;
          memcpy(static_cast<uint8_t*>(new_data.pixels()) +
                     new_data.stride * (y + y0 - py0) +
                     bytes_per_pixel * (x0 - px0),
                 static_cast<const uint8_t*>(frame.data.pixels()) +
                     frame.data.stride * y,
                 xs * bytes_per_pixel);
        }

        x0 = px0;
        y0 = py0;
        should_blend = false;
        ppf->frames.emplace_back(std::move(new_data));
      } else {
        // If all else fails, insert a dummy blank frame with kReplace.
        PackedImage blank(pxs, pys, frame.data.format);
        memset(blank.pixels(), 0, blank.pixels_size);
        ppf->frames.emplace_back(std::move(blank));
        auto& pframe = ppf->frames.back();
        pframe.x0 = px0;
        pframe.y0 = py0;
        pframe.frame_info.duration = 0;
        pframe.blend = false;
        pframe.use_for_next_frame = true;

        ppf->frames.emplace_back(std::move(frame.data));
      }
    } else {
      ppf->frames.emplace_back(std::move(frame.data));
    }

    auto& pframe = ppf->frames.back();
    pframe.x0 = x0;
    pframe.y0 = y0;
    pframe.frame_info.duration = frame.duration;
    pframe.blend = should_blend;
    pframe.use_for_next_frame = use_for_next_frame;

    if (has_nontrivial_background &&
        frame.dispose_op == DISPOSE_OP_BACKGROUND) {
      previous_frame_should_be_cleared = true;
    } else {
      previous_frame_should_be_cleared = false;
    }
  }
  if (ppf->frames.empty()) return JXL_FAILURE("No frames decoded");
  ppf->frames.back().frame_info.is_last = true;

  return true;
}

static void PngWrite(png_structp png_ptr, png_bytep data, png_size_t length) {
  PaddedBytes* bytes = static_cast<PaddedBytes*>(png_get_io_ptr(png_ptr));
  bytes->append(data, data + length);
}

// Stores XMP and EXIF/IPTC into key/value strings for PNG
class BlobsWriterPNG {
 public:
  static Status Encode(const Blobs& blobs, std::vector<std::string>* strings) {
    if (!blobs.exif.empty()) {
      JXL_RETURN_IF_ERROR(EncodeBase16("exif", blobs.exif, strings));
    }
    if (!blobs.iptc.empty()) {
      JXL_RETURN_IF_ERROR(EncodeBase16("iptc", blobs.iptc, strings));
    }
    if (!blobs.xmp.empty()) {
      JXL_RETURN_IF_ERROR(EncodeBase16("xmp", blobs.xmp, strings));
    }
    return true;
  }

 private:
  static JXL_INLINE char EncodeNibble(const uint8_t nibble) {
    JXL_ASSERT(nibble < 16);
    return (nibble < 10) ? '0' + nibble : 'a' + nibble - 10;
  }

  static Status EncodeBase16(const std::string& type, const PaddedBytes& bytes,
                             std::vector<std::string>* strings) {
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

    strings->push_back(std::string(key));
    strings->push_back(std::string(header) + base16);
    return true;
  }
};

}  // namespace extras
}  // namespace jxl
