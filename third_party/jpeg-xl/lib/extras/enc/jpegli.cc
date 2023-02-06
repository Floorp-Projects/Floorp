// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/extras/enc/jpegli.h"

#include <setjmp.h>
#include <stdint.h>

#include "jxl/codestream_header.h"
#include "lib/extras/enc/encode.h"
#include "lib/jpegli/encode.h"

namespace jxl {
namespace extras {

namespace {

void MyErrorExit(j_common_ptr cinfo) {
  jmp_buf* env = static_cast<jmp_buf*>(cinfo->client_data);
  (*cinfo->err->output_message)(cinfo);
  jpegli_destroy_compress(reinterpret_cast<j_compress_ptr>(cinfo));
  longjmp(*env, 1);
}

bool IsSRGBEncoding(const JxlColorEncoding& c) {
  return ((c.color_space == JXL_COLOR_SPACE_RGB ||
           c.color_space == JXL_COLOR_SPACE_GRAY) &&
          c.primaries == JXL_PRIMARIES_SRGB &&
          c.white_point == JXL_WHITE_POINT_D65 &&
          c.transfer_function == JXL_TRANSFER_FUNCTION_SRGB);
}

Status VerifyInput(const PackedPixelFile& ppf) {
  const JxlBasicInfo& info = ppf.info;
  JXL_RETURN_IF_ERROR(Encoder::VerifyBasicInfo(info));
  if (info.alpha_bits > 0) {
    return JXL_FAILURE("Alpha is not supported for JPEG output.");
  }
  if (ppf.frames.size() != 1) {
    return JXL_FAILURE("JPEG input must have exactly one frame.");
  }
  const PackedImage& image = ppf.frames[0].color;
  JXL_RETURN_IF_ERROR(Encoder::VerifyImageSize(image, info));
  if (image.format.data_type == JXL_TYPE_FLOAT16) {
    return JXL_FAILURE("FLOAT16 input is not supprted.");
  }
  JXL_RETURN_IF_ERROR(Encoder::VerifyBitDepth(image.format.data_type,
                                              info.bits_per_sample,
                                              info.exponent_bits_per_sample));
  if ((image.format.data_type == JXL_TYPE_UINT8 && info.bits_per_sample != 8) ||
      (image.format.data_type == JXL_TYPE_UINT16 &&
       info.bits_per_sample != 16)) {
    return JXL_FAILURE("Only full bit depth unsigned types are supported.");
  }
  return true;
}

Status GetICC(const PackedPixelFile& ppf, std::vector<uint8_t>* icc) {
  if (!ppf.icc.empty()) {
    icc->assign(ppf.icc.begin(), ppf.icc.end());
  } else {
    ColorEncoding c_enc;
    JXL_RETURN_IF_ERROR(
        ConvertExternalToInternalColorEncoding(ppf.color_encoding, &c_enc));
    if (c_enc.ICC().empty()) {
      return JXL_FAILURE("Failed to serialize ICC");
    }
    icc->assign(c_enc.ICC().begin(), c_enc.ICC().end());
  }
  return true;
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

JpegliEndianness ConvertEndianness(JxlEndianness endianness) {
  switch (endianness) {
    case JXL_NATIVE_ENDIAN:
      return JPEGLI_NATIVE_ENDIAN;
    case JXL_LITTLE_ENDIAN:
      return JPEGLI_LITTLE_ENDIAN;
    case JXL_BIG_ENDIAN:
      return JPEGLI_BIG_ENDIAN;
    default:
      return JPEGLI_NATIVE_ENDIAN;
  }
}
}  // namespace

Status EncodeJpeg(const PackedPixelFile& ppf, const JpegSettings& jpeg_settings,
                  ThreadPool* pool, std::vector<uint8_t>* compressed) {
  JXL_RETURN_IF_ERROR(VerifyInput(ppf));
  std::vector<uint8_t> icc;
  JXL_RETURN_IF_ERROR(GetICC(ppf, &icc));

  // We need to declare all the non-trivial destructor local variables
  // before the call to setjmp().
  std::vector<uint8_t> pixels;
  unsigned char* output_buffer = nullptr;
  unsigned long output_size = 0;

  const auto try_catch_block = [&]() -> bool {
    jpeg_compress_struct cinfo;
    jpeg_error_mgr jerr;
    jmp_buf env;
    cinfo.err = jpegli_std_error(&jerr);
    jerr.error_exit = &MyErrorExit;
    if (setjmp(env)) {
      if (output_buffer) free(output_buffer);
      return false;
    }
    cinfo.client_data = static_cast<void*>(&env);
    jpegli_create_compress(&cinfo);
    jpegli_mem_dest(&cinfo, &output_buffer, &output_size);
    const JxlBasicInfo& info = ppf.info;
    cinfo.image_width = info.xsize;
    cinfo.image_height = info.ysize;
    cinfo.input_components = info.num_color_channels;
    cinfo.in_color_space =
        cinfo.input_components == 1 ? JCS_GRAYSCALE : JCS_RGB;
    if (jpeg_settings.xyb) {
      jpegli_set_xyb_mode(&cinfo);
    }
    jpegli_set_defaults(&cinfo);
    jpegli_set_distance(&cinfo, jpeg_settings.distance);
    jpegli_start_compress(&cinfo, TRUE);
    if (!IsSRGBEncoding(ppf.color_encoding)) {
      jpegli_write_icc_profile(&cinfo, icc.data(), icc.size());
    }
    const PackedImage& image = ppf.frames[0].color;
    const uint8_t* pixels = reinterpret_cast<const uint8_t*>(image.pixels());
    std::vector<uint8_t> row_bytes(image.stride);
    jpegli_set_input_format(&cinfo, ConvertDataType(image.format.data_type),
                            ConvertEndianness(image.format.endianness));
    for (size_t y = 0; y < info.ysize; ++y) {
      memcpy(&row_bytes[0], pixels + y * image.stride, image.stride);
      JSAMPROW row[] = {row_bytes.data()};
      jpegli_write_scanlines(&cinfo, row, 1);
    }
    jpegli_finish_compress(&cinfo);
    jpegli_destroy_compress(&cinfo);
    compressed->resize(output_size);
    std::copy_n(output_buffer, output_size, compressed->data());
    std::free(output_buffer);
    return true;
  };
  return try_catch_block();
}

}  // namespace extras
}  // namespace jxl
