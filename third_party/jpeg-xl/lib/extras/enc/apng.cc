// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/extras/enc/apng.h"

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
#include <vector>

#include "jxl/encode.h"
#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/printf_macros.h"
#include "lib/jxl/color_encoding_internal.h"
#include "lib/jxl/dec_external_image.h"
#include "lib/jxl/enc_color_management.h"
#include "lib/jxl/enc_image_bundle.h"
#include "lib/jxl/frame_header.h"
#include "lib/jxl/headers.h"
#include "lib/jxl/image.h"
#include "lib/jxl/image_bundle.h"
#include "png.h" /* original (unpatched) libpng is ok */

namespace jxl {
namespace extras {

namespace {

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

}  // namespace

Status EncodeImageAPNG(const CodecInOut* io, const ColorEncoding& c_desired,
                       size_t bits_per_sample, ThreadPool* pool,
                       PaddedBytes* bytes) {
  if (bits_per_sample > 8) {
    bits_per_sample = 16;
  } else if (bits_per_sample < 8) {
    // PNG can also do 4, 2, and 1 bits per sample, but it isn't implemented
    bits_per_sample = 8;
  }

  size_t count = 0;
  bool have_anim = io->metadata.m.have_animation;
  size_t anim_chunks = 0;
  int W = 0, H = 0;

  for (auto& frame : io->frames) {
    png_structp png_ptr;
    png_infop info_ptr;

    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

    if (!png_ptr) return JXL_FAILURE("Could not init png encoder");

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) return JXL_FAILURE("Could not init png info struct");

    png_set_write_fn(png_ptr, bytes, PngWrite, NULL);
    png_set_flush(png_ptr, 0);

    ImageBundle ib = frame.Copy();
    const size_t alpha_bits = ib.HasAlpha() ? bits_per_sample : 0;
    ImageMetadata metadata = io->metadata.m;
    ImageBundle store(&metadata);
    const ImageBundle* transformed;
    JXL_RETURN_IF_ERROR(TransformIfNeeded(ib, c_desired, GetJxlCms(), pool,
                                          &store, &transformed));
    size_t stride = ib.oriented_xsize() *
                    DivCeil(c_desired.Channels() * bits_per_sample + alpha_bits,
                            kBitsPerByte);
    PaddedBytes raw_bytes(stride * ib.oriented_ysize());
    JXL_RETURN_IF_ERROR(ConvertToExternal(
        *transformed, bits_per_sample, /*float_out=*/false,
        c_desired.Channels() + (ib.HasAlpha() ? 1 : 0), JXL_BIG_ENDIAN, stride,
        pool, raw_bytes.data(), raw_bytes.size(), /*out_callback=*/nullptr,
        /*out_opaque=*/nullptr, metadata.GetOrientation()));

    int width = ib.oriented_xsize();
    int height = ib.oriented_ysize();

    png_byte color_type =
        (c_desired.Channels() == 3 ? PNG_COLOR_TYPE_RGB : PNG_COLOR_TYPE_GRAY);
    if (ib.HasAlpha()) color_type |= PNG_COLOR_MASK_ALPHA;
    png_byte bit_depth = bits_per_sample;

    png_set_IHDR(png_ptr, info_ptr, width, height, bit_depth, color_type,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE,
                 PNG_FILTER_TYPE_BASE);
    if (count == 0) {
      W = width;
      H = height;

      // TODO(jon): instead of always setting an iCCP, could try to avoid that
      // have to avoid warnings on the ICC profile becoming fatal
      png_set_benign_errors(png_ptr, 1);
      png_set_iCCP(png_ptr, info_ptr, "1", 0, c_desired.ICC().data(),
                   c_desired.ICC().size());

      std::vector<std::string> textstrings;
      JXL_RETURN_IF_ERROR(BlobsWriterPNG::Encode(io->blobs, &textstrings));
      for (size_t i = 0; i + 1 < textstrings.size(); i += 2) {
        png_text text;
        text.key = const_cast<png_charp>(textstrings[i].c_str());
        text.text = const_cast<png_charp>(textstrings[i + 1].c_str());
        text.compression = PNG_TEXT_COMPRESSION_zTXt;
        png_set_text(png_ptr, info_ptr, &text, 1);
      }

      png_write_info(png_ptr, info_ptr);
    } else {
      // fake writing a header, otherwise libpng gets confused
      size_t pos = bytes->size();
      png_write_info(png_ptr, info_ptr);
      bytes->resize(pos);
    }

    if (have_anim) {
      if (count == 0) {
        png_byte adata[8];
        png_save_uint_32(adata, io->frames.size());
        png_save_uint_32(adata + 4, io->metadata.m.animation.num_loops);
        png_byte actl[5] = "acTL";
        png_write_chunk(png_ptr, actl, adata, 8);
      }
      png_byte fdata[26];
      JXL_ASSERT(W == width);
      JXL_ASSERT(H == height);
      // TODO(jon): also make this work for the non-coalesced case
      png_save_uint_32(fdata, anim_chunks++);
      png_save_uint_32(fdata + 4, width);
      png_save_uint_32(fdata + 8, height);
      png_save_uint_32(fdata + 12, 0);
      png_save_uint_32(fdata + 16, 0);
      png_save_uint_16(
          fdata + 20,
          frame.duration * io->metadata.m.animation.tps_denominator);
      png_save_uint_16(fdata + 22, io->metadata.m.animation.tps_numerator);
      fdata[24] = 1;
      fdata[25] = 0;
      png_byte fctl[5] = "fcTL";
      png_write_chunk(png_ptr, fctl, fdata, 26);
    }

    std::vector<uint8_t*> rows(height);
    for (int y = 0; y < height; ++y) {
      rows[y] = raw_bytes.data() + y * stride;
    }

    png_write_flush(png_ptr);
    const size_t pos = bytes->size();
    png_write_image(png_ptr, &rows[0]);
    png_write_flush(png_ptr);
    if (count > 0) {
      PaddedBytes fdata(4);
      png_save_uint_32(fdata.data(), anim_chunks++);
      size_t p = pos;
      while (p + 8 < bytes->size()) {
        size_t len = png_get_uint_32(bytes->data() + p);
        JXL_ASSERT(bytes->operator[](p + 4) == 'I');
        JXL_ASSERT(bytes->operator[](p + 5) == 'D');
        JXL_ASSERT(bytes->operator[](p + 6) == 'A');
        JXL_ASSERT(bytes->operator[](p + 7) == 'T');
        fdata.append(bytes->data() + p + 8, bytes->data() + p + 8 + len);
        p += len + 12;
      }
      bytes->resize(pos);

      png_byte fdat[5] = "fdAT";
      png_write_chunk(png_ptr, fdat, fdata.data(), fdata.size());
    }

    count++;
    if (count == io->frames.size()) png_write_end(png_ptr, NULL);

    png_destroy_write_struct(&png_ptr, &info_ptr);
  }

  return true;
}

}  // namespace extras
}  // namespace jxl
