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
#include "lib/jxl/enc_color_management.h"
#include "lib/jxl/enc_xyb.h"

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

Status GetColorEncoding(const PackedPixelFile& ppf,
                        ColorEncoding* color_encoding) {
  if (!ppf.icc.empty()) {
    PaddedBytes icc;
    icc.assign(ppf.icc.data(), ppf.icc.data() + ppf.icc.size());
    JXL_RETURN_IF_ERROR(color_encoding->SetICC(std::move(icc)));
  } else {
    JXL_RETURN_IF_ERROR(ConvertExternalToInternalColorEncoding(
        ppf.color_encoding, color_encoding));
  }
  if (color_encoding->ICC().empty()) {
    return JXL_FAILURE("Invalid color encoding.");
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

void ToFloatRow(const uint8_t* row_in, JxlPixelFormat format, size_t len,
                float* row_out) {
  bool is_little_endian =
      (format.endianness == JXL_LITTLE_ENDIAN ||
       (format.endianness == JXL_NATIVE_ENDIAN && IsLittleEndian()));
  static constexpr double kMul8 = 1.0 / 255.0;
  static constexpr double kMul16 = 1.0 / 65535.0;
  if (format.data_type == JXL_TYPE_UINT8) {
    for (size_t x = 0; x < len; ++x) {
      row_out[x] = row_in[x] * kMul8;
    }
  } else if (format.data_type == JXL_TYPE_UINT16 && is_little_endian) {
    for (size_t x = 0; x < len; ++x) {
      row_out[x] = LoadLE16(&row_in[2 * x]) * kMul16;
    }
  } else if (format.data_type == JXL_TYPE_UINT16 && !is_little_endian) {
    for (size_t x = 0; x < len; ++x) {
      row_out[x] = LoadBE16(&row_in[2 * x]) * kMul16;
    }
  } else if (format.data_type == JXL_TYPE_FLOAT && is_little_endian) {
    for (size_t x = 0; x < len; ++x) {
      row_out[x] = LoadLEFloat(&row_in[4 * x]);
    }
  } else if (format.data_type == JXL_TYPE_FLOAT && !is_little_endian) {
    for (size_t x = 0; x < len; ++x) {
      row_out[x] = LoadBEFloat(&row_in[4 * x]);
    }
  }
}

}  // namespace

Status EncodeJpeg(const PackedPixelFile& ppf, const JpegSettings& jpeg_settings,
                  ThreadPool* pool, std::vector<uint8_t>* compressed) {
  JXL_RETURN_IF_ERROR(VerifyInput(ppf));

  ColorEncoding color_encoding;
  JXL_RETURN_IF_ERROR(GetColorEncoding(ppf, &color_encoding));

  ColorSpaceTransform c_transform(GetJxlCms());
  ColorEncoding xyb_encoding;
  if (jpeg_settings.xyb) {
    if (ppf.info.num_color_channels != 3) {
      return JXL_FAILURE("Only RGB input is supported in XYB mode.");
    }
    const ColorEncoding& c_desired = ColorEncoding::LinearSRGB(false);
    JXL_RETURN_IF_ERROR(
        c_transform.Init(color_encoding, c_desired, 255.0f, ppf.info.xsize, 1));
    xyb_encoding.SetColorSpace(jxl::ColorSpace::kXYB);
    xyb_encoding.rendering_intent = jxl::RenderingIntent::kPerceptual;
    JXL_RETURN_IF_ERROR(xyb_encoding.CreateICC());
  }
  const ColorEncoding& output_encoding =
      jpeg_settings.xyb ? xyb_encoding : color_encoding;

  // We need to declare all the non-trivial destructor local variables
  // before the call to setjmp().
  std::vector<uint8_t> pixels;
  unsigned char* output_buffer = nullptr;
  unsigned long output_size = 0;
  std::vector<uint8_t> row_bytes;
  size_t rowlen = RoundUpTo(ppf.info.xsize, VectorSize());
  hwy::AlignedFreeUniquePtr<float[]> xyb_tmp =
      hwy::AllocateAligned<float>(6 * rowlen);
  hwy::AlignedFreeUniquePtr<float[]> premul_absorb =
      hwy::AllocateAligned<float>(VectorSize() * 12);
  ComputePremulAbsorb(255.0f, premul_absorb.get());

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
    } else if (jpeg_settings.use_std_quant_tables) {
      jpegli_use_standard_quant_tables(&cinfo);
    }
    jpegli_set_progressive_level(&cinfo, jpeg_settings.progressive_level);
    jpegli_set_defaults(&cinfo);
    if (!jpeg_settings.chroma_subsampling.empty()) {
      if (jpeg_settings.chroma_subsampling == "444") {
        cinfo.comp_info[0].h_samp_factor = 1;
        cinfo.comp_info[0].v_samp_factor = 1;
      } else if (jpeg_settings.chroma_subsampling == "440") {
        cinfo.comp_info[0].h_samp_factor = 1;
        cinfo.comp_info[0].v_samp_factor = 2;
      } else if (jpeg_settings.chroma_subsampling == "422") {
        cinfo.comp_info[0].h_samp_factor = 2;
        cinfo.comp_info[0].v_samp_factor = 1;
      } else if (jpeg_settings.chroma_subsampling == "420") {
        cinfo.comp_info[0].h_samp_factor = 2;
        cinfo.comp_info[0].v_samp_factor = 2;
      } else {
        return false;
      }
      for (int i = 1; i < cinfo.num_components; ++i) {
        cinfo.comp_info[i].h_samp_factor = 1;
        cinfo.comp_info[i].v_samp_factor = 1;
      }
    }
    jpegli_enable_adaptive_quantization(
        &cinfo, jpeg_settings.use_adaptive_quantization);
    jpegli_set_distance(&cinfo, jpeg_settings.distance);
    jpegli_start_compress(&cinfo, TRUE);
    if (!output_encoding.IsSRGB()) {
      jpegli_write_icc_profile(&cinfo, output_encoding.ICC().data(),
                               output_encoding.ICC().size());
    }
    const PackedImage& image = ppf.frames[0].color;
    const uint8_t* pixels = reinterpret_cast<const uint8_t*>(image.pixels());
    if (jpeg_settings.xyb) {
      jpegli_set_input_format(&cinfo, JPEGLI_TYPE_FLOAT, JPEGLI_NATIVE_ENDIAN);
      float* src_buf = c_transform.BufSrc(0);
      float* dst_buf = c_transform.BufDst(0);
      for (size_t y = 0; y < image.ysize; ++y) {
        // convert to float
        ToFloatRow(&pixels[y * image.stride], image.format, 3 * image.xsize,
                   src_buf);
        // convert to linear srgb
        if (!c_transform.Run(0, src_buf, dst_buf)) {
          return false;
        }
        // deinterleave channels
        float* row0 = &xyb_tmp[0];
        float* row1 = &xyb_tmp[rowlen];
        float* row2 = &xyb_tmp[2 * rowlen];
        for (size_t x = 0; x < image.xsize; ++x) {
          row0[x] = dst_buf[3 * x + 0];
          row1[x] = dst_buf[3 * x + 1];
          row2[x] = dst_buf[3 * x + 2];
        }
        // convert to xyb
        LinearRGBRowToXYB(row0, row1, row2, premul_absorb.get(), image.xsize);
        // scale xyb
        ScaleXYBRow(row0, row1, row2, image.xsize);
        // interleave channels
        float* row_out = &xyb_tmp[3 * rowlen];
        for (size_t x = 0; x < image.xsize; ++x) {
          row_out[3 * x + 0] = row0[x];
          row_out[3 * x + 1] = row1[x];
          row_out[3 * x + 2] = row2[x];
        }
        // feed to jpegli as native endian floats
        JSAMPROW row[] = {reinterpret_cast<uint8_t*>(row_out)};
        jpegli_write_scanlines(&cinfo, row, 1);
      }
    } else {
      jpegli_set_input_format(&cinfo, ConvertDataType(image.format.data_type),
                              ConvertEndianness(image.format.endianness));
      row_bytes.resize(image.stride);
      for (size_t y = 0; y < info.ysize; ++y) {
        memcpy(&row_bytes[0], pixels + y * image.stride, image.stride);
        JSAMPROW row[] = {row_bytes.data()};
        jpegli_write_scanlines(&cinfo, row, 1);
      }
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
