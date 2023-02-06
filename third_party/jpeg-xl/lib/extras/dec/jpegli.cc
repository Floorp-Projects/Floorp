// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/extras/dec/jpegli.h"

#include <setjmp.h>
#include <stdint.h>

#include <algorithm>
#include <numeric>
#include <utility>
#include <vector>

#include "lib/jpegli/decode.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/sanitizers.h"

namespace jxl {
namespace extras {

namespace {

constexpr unsigned char kExifSignature[6] = {0x45, 0x78, 0x69,
                                             0x66, 0x00, 0x00};
constexpr int kExifMarker = JPEG_APP0 + 1;
constexpr int kICCMarker = JPEG_APP0 + 2;

static inline bool IsJPG(const std::vector<uint8_t>& bytes) {
  if (bytes.size() < 2) return false;
  if (bytes[0] != 0xFF || bytes[1] != 0xD8) return false;
  return true;
}

bool MarkerIsExif(const jpeg_saved_marker_ptr marker) {
  return marker->marker == kExifMarker &&
         marker->data_length >= sizeof kExifSignature + 2 &&
         std::equal(std::begin(kExifSignature), std::end(kExifSignature),
                    marker->data);
}

Status ReadICCProfile(jpeg_decompress_struct* const cinfo,
                      std::vector<uint8_t>* const icc) {
  uint8_t* icc_data_ptr;
  unsigned int icc_data_len;
  if (jpegli_read_icc_profile(cinfo, &icc_data_ptr, &icc_data_len)) {
    icc->assign(icc_data_ptr, icc_data_ptr + icc_data_len);
    free(icc_data_ptr);
    return true;
  }
  return false;
}

void ReadExif(jpeg_decompress_struct* const cinfo,
              std::vector<uint8_t>* const exif) {
  constexpr size_t kExifSignatureSize = sizeof kExifSignature;
  for (jpeg_saved_marker_ptr marker = cinfo->marker_list; marker != nullptr;
       marker = marker->next) {
    // marker is initialized by libjpeg, which we are not instrumenting with
    // msan.
    msan::UnpoisonMemory(marker, sizeof(*marker));
    msan::UnpoisonMemory(marker->data, marker->data_length);
    if (!MarkerIsExif(marker)) continue;
    size_t marker_length = marker->data_length - kExifSignatureSize;
    exif->resize(marker_length);
    std::copy_n(marker->data + kExifSignatureSize, marker_length, exif->data());
    return;
  }
}

JpegliDataType ConvertDataType(JxlDataType type) {
  switch (type) {
    case JXL_TYPE_UINT8:
      return JPEGLI_TYPE_UINT8;
    case JXL_TYPE_UINT16:
      return JPEGLI_TYPE_UINT16;
    case JXL_TYPE_FLOAT:
      return JPEGLI_TYPE_FLOAT;
    default:
      return JPEGLI_TYPE_UINT8;
  }
}

void MyErrorExit(j_common_ptr cinfo) {
  jmp_buf* env = static_cast<jmp_buf*>(cinfo->client_data);
  (*cinfo->err->output_message)(cinfo);
  jpegli_destroy_decompress(reinterpret_cast<j_decompress_ptr>(cinfo));
  longjmp(*env, 1);
}

void MyOutputMessage(j_common_ptr cinfo) {
#if JXL_DEBUG_WARNING == 1
  char buf[JMSG_LENGTH_MAX + 1];
  (*cinfo->err->format_message)(cinfo, buf);
  buf[JMSG_LENGTH_MAX] = 0;
  JXL_WARNING("%s", buf);
#endif
}

}  // namespace

Status DecodeJpeg(const std::vector<uint8_t>& compressed,
                  JxlDataType output_data_type, ThreadPool* pool,
                  PackedPixelFile* ppf) {
  // Don't do anything for non-JPEG files (no need to report an error)
  if (!IsJPG(compressed)) return false;

  // TODO(veluca): use JPEGData also for pixels?

  // We need to declare all the non-trivial destructor local variables before
  // the call to setjmp().
  std::unique_ptr<JSAMPLE[]> row;

  const auto try_catch_block = [&]() -> bool {
    jpeg_decompress_struct cinfo;
    // cinfo is initialized by libjpeg, which we are not instrumenting with
    // msan, therefore we need to initialize cinfo here.
    msan::UnpoisonMemory(&cinfo, sizeof(cinfo));
    // Setup error handling in jpeg library so we can deal with broken jpegs in
    // the fuzzer.
    jpeg_error_mgr jerr;
    jmp_buf env;
    cinfo.err = jpegli_std_error(&jerr);
    jerr.error_exit = &MyErrorExit;
    jerr.output_message = &MyOutputMessage;
    if (setjmp(env)) {
      return false;
    }
    cinfo.client_data = static_cast<void*>(&env);

    jpegli_create_decompress(&cinfo);
    jpegli_mem_src(&cinfo,
                   reinterpret_cast<const unsigned char*>(compressed.data()),
                   compressed.size());
    jpegli_save_markers(&cinfo, kICCMarker, 0xFFFF);
    jpegli_save_markers(&cinfo, kExifMarker, 0xFFFF);
    const auto failure = [&cinfo](const char* str) -> Status {
      jpegli_abort_decompress(&cinfo);
      jpegli_destroy_decompress(&cinfo);
      return JXL_FAILURE("%s", str);
    };
    jpegli_read_header(&cinfo, TRUE);
    // Might cause CPU-zip bomb.
    if (cinfo.arith_code) {
      return failure("arithmetic code JPEGs are not supported");
    }
    int nbcomp = cinfo.num_components;
    if (nbcomp != 1 && nbcomp != 3) {
      return failure("unsupported number of components in JPEG");
    }
    if (!ReadICCProfile(&cinfo, &ppf->icc)) {
      ppf->icc.clear();
      // Default to SRGB
      // Actually, (cinfo.output_components == nbcomp) will be checked after
      // `jpegli_start_decompress`.
      ppf->color_encoding.color_space =
          (nbcomp == 1) ? JXL_COLOR_SPACE_GRAY : JXL_COLOR_SPACE_RGB;
      ppf->color_encoding.white_point = JXL_WHITE_POINT_D65;
      ppf->color_encoding.primaries = JXL_PRIMARIES_SRGB;
      ppf->color_encoding.transfer_function = JXL_TRANSFER_FUNCTION_SRGB;
      ppf->color_encoding.rendering_intent = JXL_RENDERING_INTENT_PERCEPTUAL;
    }
    ReadExif(&cinfo, &ppf->metadata.exif);

    ppf->info.xsize = cinfo.image_width;
    ppf->info.ysize = cinfo.image_height;
    if (output_data_type == JXL_TYPE_UINT8) {
      ppf->info.bits_per_sample = 8;
    } else if (output_data_type == JXL_TYPE_UINT16) {
      ppf->info.bits_per_sample = 16;
    } else {
      return failure("unsupported data type");
    }
    ppf->info.exponent_bits_per_sample = 0;
    ppf->info.uses_original_profile = true;

    // No alpha in JPG
    ppf->info.alpha_bits = 0;
    ppf->info.alpha_exponent_bits = 0;

    ppf->info.num_color_channels = nbcomp;
    ppf->info.orientation = JXL_ORIENT_IDENTITY;

    jpegli_set_output_format(&cinfo, ConvertDataType(output_data_type),
                             JPEGLI_NATIVE_ENDIAN);
    jpegli_start_decompress(&cinfo);
    JXL_ASSERT(cinfo.output_components == nbcomp);

    const JxlPixelFormat format{
        /*num_channels=*/static_cast<uint32_t>(nbcomp),
        output_data_type,
        /*endianness=*/JXL_NATIVE_ENDIAN,
        /*align=*/0,
    };
    ppf->frames.clear();
    // Allocates the frame buffer.
    ppf->frames.emplace_back(cinfo.image_width, cinfo.image_height, format);
    const auto& frame = ppf->frames.back();
    JXL_ASSERT(sizeof(JSAMPLE) * cinfo.output_components * cinfo.image_width <=
               frame.color.stride);

    for (size_t y = 0; y < cinfo.image_height; ++y) {
      JSAMPROW rows[] = {reinterpret_cast<JSAMPLE*>(
          static_cast<uint8_t*>(frame.color.pixels()) +
          frame.color.stride * y)};
      jpegli_read_scanlines(&cinfo, rows, 1);
      msan::UnpoisonMemory(rows[0], sizeof(JSAMPLE) * cinfo.output_components *
                                        cinfo.image_width);
    }

    jpegli_finish_decompress(&cinfo);
    jpegli_destroy_decompress(&cinfo);
    return true;
  };

  return try_catch_block();
}

}  // namespace extras
}  // namespace jxl
