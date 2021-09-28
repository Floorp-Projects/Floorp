// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/extras/codec_apng.h"

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

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/color_encoding_internal.h"
#include "lib/jxl/color_management.h"
#include "lib/jxl/frame_header.h"
#include "lib/jxl/headers.h"
#include "lib/jxl/image.h"
#include "lib/jxl/image_bundle.h"
#include "lib/jxl/luminance.h"
#include "png.h" /* original (unpatched) libpng is ok */

namespace jxl {
namespace extras {

namespace {

constexpr bool isAbc(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}
#define notabc(c) ((c) < 65 || (c) > 122 || ((c) > 90 && (c) < 97))

constexpr uint32_t kId_IHDR = 0x52444849;
constexpr uint32_t kId_acTL = 0x4C546361;
constexpr uint32_t kId_fcTL = 0x4C546366;
constexpr uint32_t kId_IDAT = 0x54414449;
constexpr uint32_t kId_fdAT = 0x54416466;
constexpr uint32_t kId_IEND = 0x444E4549;

struct CHUNK {
  unsigned char* p;
  unsigned int size;
};

struct APNGFrame {
  unsigned char *p, **rows;
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
  png_set_strip_16(png_ptr);
  png_set_gray_to_rgb(png_ptr);
  png_set_palette_to_rgb(png_ptr);
  png_set_add_alpha(png_ptr, 0xff, PNG_FILLER_AFTER);
  (void)png_set_interlace_handling(png_ptr);
  png_read_update_info(png_ptr, info_ptr);
}

void row_fn(png_structp png_ptr, png_bytep new_row, png_uint_32 row_num,
            int pass) {
  APNGFrame* frame = (APNGFrame*)png_get_progressive_ptr(png_ptr);
  png_progressive_combine_row(png_ptr, frame->rows[row_num], new_row);
}

inline unsigned int read_chunk(Reader* r, CHUNK* pChunk) {
  unsigned char len[4];
  pChunk->size = 0;
  pChunk->p = 0;
  if (r->Read(&len, 4)) {
    const auto size = png_get_uint_32(len);
    // Check first, to avoid overflow.
    if (size > kMaxPNGChunkSize) {
      JXL_WARNING("APNG chunk size is too big");
      return 0;
    }
    pChunk->size = size + 12;
    pChunk->p = new unsigned char[pChunk->size];
    memcpy(pChunk->p, len, 4);
    if (r->Read(pChunk->p + 4, pChunk->size - 4)) {
      return *(unsigned int*)(pChunk->p + 4);
    }
  }
  return 0;
}

int processing_start(png_structp& png_ptr, png_infop& info_ptr, void* frame_ptr,
                     bool hasInfo, CHUNK& chunkIHDR,
                     std::vector<CHUNK>& chunksInfo) {
  unsigned char header[8] = {137, 80, 78, 71, 13, 10, 26, 10};

  png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  info_ptr = png_create_info_struct(png_ptr);
  if (!png_ptr || !info_ptr) return 1;

  if (setjmp(png_jmpbuf(png_ptr))) {
    png_destroy_read_struct(&png_ptr, &info_ptr, 0);
    return 1;
  }

  png_set_crc_action(png_ptr, PNG_CRC_QUIET_USE, PNG_CRC_QUIET_USE);
  png_set_progressive_read_fn(png_ptr, frame_ptr, info_fn, row_fn, NULL);

  png_process_data(png_ptr, info_ptr, header, 8);
  png_process_data(png_ptr, info_ptr, chunkIHDR.p, chunkIHDR.size);

  if (hasInfo) {
    for (unsigned int i = 0; i < chunksInfo.size(); i++) {
      png_process_data(png_ptr, info_ptr, chunksInfo[i].p, chunksInfo[i].size);
    }
  }
  return 0;
}

int processing_data(png_structp png_ptr, png_infop info_ptr, unsigned char* p,
                    unsigned int size) {
  if (!png_ptr || !info_ptr) return 1;

  if (setjmp(png_jmpbuf(png_ptr))) {
    png_destroy_read_struct(&png_ptr, &info_ptr, 0);
    return 1;
  }

  png_process_data(png_ptr, info_ptr, p, size);
  return 0;
}

int processing_finish(png_structp png_ptr, png_infop info_ptr) {
  unsigned char footer[12] = {0, 0, 0, 0, 73, 69, 78, 68, 174, 66, 96, 130};

  if (!png_ptr || !info_ptr) return 1;

  if (setjmp(png_jmpbuf(png_ptr))) {
    png_destroy_read_struct(&png_ptr, &info_ptr, 0);
    return 1;
  }

  png_process_data(png_ptr, info_ptr, footer, 12);
  png_destroy_read_struct(&png_ptr, &info_ptr, 0);

  return 0;
}

}  // namespace

Status DecodeImageAPNG(Span<const uint8_t> bytes, const ColorHints& color_hints,
                       ThreadPool* pool, CodecInOut* io) {
  Reader r;
  unsigned int id, i, j, w, h, w0, h0, x0, y0;
  unsigned int delay_num, delay_den, dop, bop, rowbytes, imagesize;
  unsigned char sig[8];
  png_structp png_ptr;
  png_infop info_ptr;
  CHUNK chunk;
  CHUNK chunkIHDR;
  std::vector<CHUNK> chunksInfo;
  bool isAnimated = false;
  bool skipFirst = false;
  bool hasInfo = false;
  bool all_dispose_bg = true;
  APNGFrame frameRaw = {};

  r = {bytes.data(), bytes.data() + bytes.size()};
  // Not an aPNG => not an error
  unsigned char png_signature[8] = {137, 80, 78, 71, 13, 10, 26, 10};
  if (!r.Read(sig, 8) || memcmp(sig, png_signature, 8) != 0) {
    return false;
  }
  id = read_chunk(&r, &chunkIHDR);

  io->frames.clear();
  io->dec_pixels = 0;
  io->metadata.m.SetUintSamples(8);
  io->metadata.m.SetAlphaBits(8);
  io->metadata.m.color_encoding =
      ColorEncoding::SRGB();  // todo: get data from png metadata
  JXL_RETURN_IF_ERROR(ApplyColorHints(color_hints, /*color_already_set=*/true,
                                      /*is_gray=*/false, io));

  bool errorstate = true;
  if (id == kId_IHDR && chunkIHDR.size == 25) {
    w0 = w = png_get_uint_32(chunkIHDR.p + 8);
    h0 = h = png_get_uint_32(chunkIHDR.p + 12);

    if (w > cMaxPNGSize || h > cMaxPNGSize) {
      return false;
    }

    x0 = 0;
    y0 = 0;
    delay_num = 1;
    delay_den = 10;
    dop = 0;
    bop = 0;
    rowbytes = w * 4;
    imagesize = h * rowbytes;

    frameRaw.p = new unsigned char[imagesize];
    frameRaw.rows = new png_bytep[h * sizeof(png_bytep)];
    for (j = 0; j < h; j++) frameRaw.rows[j] = frameRaw.p + j * rowbytes;

    if (!processing_start(png_ptr, info_ptr, (void*)&frameRaw, hasInfo,
                          chunkIHDR, chunksInfo)) {
      bool last_base_was_none = true;
      while (!r.Eof()) {
        id = read_chunk(&r, &chunk);
        if (!id) break;
        JXL_ASSERT(chunk.p != nullptr);

        if (id == kId_acTL && !hasInfo && !isAnimated) {
          isAnimated = true;
          skipFirst = true;
          io->metadata.m.have_animation = true;
          io->metadata.m.animation.tps_numerator = 1000;
        } else if (id == kId_IEND ||
                   (id == kId_fcTL && (!hasInfo || isAnimated))) {
          if (hasInfo) {
            if (!processing_finish(png_ptr, info_ptr)) {
              ImageBundle bundle(&io->metadata.m);
              bundle.duration = delay_num * 1000 / delay_den;
              bundle.origin.x0 = x0;
              bundle.origin.y0 = y0;
              // TODO(veluca): this could in principle be implemented.
              if (last_base_was_none && !all_dispose_bg &&
                  (x0 != 0 || y0 != 0 || w0 != w || h0 != h || bop != 0)) {
                return JXL_FAILURE(
                    "APNG with dispose-to-0 is not supported for non-full or "
                    "blended frames");
              }
              switch (dop) {
                case 0:
                  bundle.use_for_next_frame = true;
                  last_base_was_none = false;
                  all_dispose_bg = false;
                  break;
                case 2:
                  bundle.use_for_next_frame = false;
                  all_dispose_bg = false;
                  break;
                default:
                  bundle.use_for_next_frame = false;
                  last_base_was_none = true;
              }
              bundle.blend = bop != 0;
              io->dec_pixels += w0 * h0;

              Image3F sub_frame(w0, h0);
              ImageF sub_frame_alpha(w0, h0);
              for (size_t y = 0; y < h0; ++y) {
                float* const JXL_RESTRICT row_r = sub_frame.PlaneRow(0, y);
                float* const JXL_RESTRICT row_g = sub_frame.PlaneRow(1, y);
                float* const JXL_RESTRICT row_b = sub_frame.PlaneRow(2, y);
                float* const JXL_RESTRICT row_alpha = sub_frame_alpha.Row(y);
                uint8_t* const f = frameRaw.rows[y];
                for (size_t x = 0; x < w0; ++x) {
                  if (f[4 * x + 3] == 0) {
                    row_alpha[x] = 0;
                    row_r[x] = 0;
                    row_g[x] = 0;
                    row_b[x] = 0;
                    continue;
                  }
                  row_r[x] = f[4 * x + 0] * (1.f / 255);
                  row_g[x] = f[4 * x + 1] * (1.f / 255);
                  row_b[x] = f[4 * x + 2] * (1.f / 255);
                  row_alpha[x] = f[4 * x + 3] * (1.f / 255);
                }
              }
              bundle.SetFromImage(std::move(sub_frame), ColorEncoding::SRGB());
              bundle.SetAlpha(std::move(sub_frame_alpha),
                              /*alpha_is_premultiplied=*/false);
              io->frames.push_back(std::move(bundle));
            } else {
              delete[] chunk.p;
              break;
            }
          }

          if (id == kId_IEND) {
            errorstate = false;
            break;
          }
          // At this point the old frame is done. Let's start a new one.
          w0 = png_get_uint_32(chunk.p + 12);
          h0 = png_get_uint_32(chunk.p + 16);
          x0 = png_get_uint_32(chunk.p + 20);
          y0 = png_get_uint_32(chunk.p + 24);
          delay_num = png_get_uint_16(chunk.p + 28);
          delay_den = png_get_uint_16(chunk.p + 30);
          dop = chunk.p[32];
          bop = chunk.p[33];

          if (!delay_den) delay_den = 100;

          if (w0 > cMaxPNGSize || h0 > cMaxPNGSize || x0 > cMaxPNGSize ||
              y0 > cMaxPNGSize || x0 + w0 > w || y0 + h0 > h || dop > 2 ||
              bop > 1) {
            delete[] chunk.p;
            break;
          }

          if (hasInfo) {
            memcpy(chunkIHDR.p + 8, chunk.p + 12, 8);
            if (processing_start(png_ptr, info_ptr, (void*)&frameRaw, hasInfo,
                                 chunkIHDR, chunksInfo)) {
              delete[] chunk.p;
              break;
            }
          } else
            skipFirst = false;

          if (io->frames.size() == (skipFirst ? 1 : 0)) {
            bop = 0;
            if (dop == 2) dop = 1;
          }
        } else if (id == kId_IDAT) {
          hasInfo = true;
          if (processing_data(png_ptr, info_ptr, chunk.p, chunk.size)) {
            delete[] chunk.p;
            break;
          }
        } else if (id == kId_fdAT && isAnimated) {
          png_save_uint_32(chunk.p + 4, chunk.size - 16);
          memcpy(chunk.p + 8, "IDAT", 4);
          if (processing_data(png_ptr, info_ptr, chunk.p + 4, chunk.size - 4)) {
            delete[] chunk.p;
            break;
          }
        } else if (!isAbc(chunk.p[4]) || !isAbc(chunk.p[5]) ||
                   !isAbc(chunk.p[6]) || !isAbc(chunk.p[7])) {
          delete[] chunk.p;
          break;
        } else if (!hasInfo) {
          if (processing_data(png_ptr, info_ptr, chunk.p, chunk.size)) {
            delete[] chunk.p;
            break;
          }
          chunksInfo.push_back(chunk);
          continue;
        }
        delete[] chunk.p;
      }
    }
    delete[] frameRaw.rows;
    delete[] frameRaw.p;
  }

  for (i = 0; i < chunksInfo.size(); i++) delete[] chunksInfo[i].p;

  chunksInfo.clear();
  delete[] chunkIHDR.p;

  if (errorstate) return false;
  SetIntensityTarget(io);
  return true;
}

}  // namespace extras
}  // namespace jxl
