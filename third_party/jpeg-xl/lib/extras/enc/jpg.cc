// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/extras/enc/jpg.h"

#include <jpeglib.h>
#include <setjmp.h>
#include <stdint.h>

#include <algorithm>
#include <iterator>
#include <numeric>
#include <utility>
#include <vector>

#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/color_encoding_internal.h"
#include "lib/jxl/common.h"
#include "lib/jxl/enc_color_management.h"
#include "lib/jxl/enc_image_bundle.h"
#include "lib/jxl/image.h"
#include "lib/jxl/image_bundle.h"
#include "lib/jxl/sanitizers.h"
#if JPEGXL_ENABLE_SJPEG
#include "sjpeg.h"
#endif

namespace jxl {
namespace extras {

namespace {

constexpr float kJPEGSampleMultiplier = MAXJSAMPLE;
constexpr unsigned char kICCSignature[12] = {
    0x49, 0x43, 0x43, 0x5F, 0x50, 0x52, 0x4F, 0x46, 0x49, 0x4C, 0x45, 0x00};
constexpr int kICCMarker = JPEG_APP0 + 2;
constexpr size_t kMaxBytesInMarker = 65533;

constexpr unsigned char kExifSignature[6] = {0x45, 0x78, 0x69,
                                             0x66, 0x00, 0x00};
constexpr int kExifMarker = JPEG_APP0 + 1;

constexpr float kJPEGSampleMin = 0;
constexpr float kJPEGSampleMax = MAXJSAMPLE;

// TODO (jon): take orientation into account when writing jpeg output
// TODO (jon): write Exif blob also in sjpeg encoding
// TODO (jon): overwrite orientation in Exif blob to avoid double orientation

void WriteICCProfile(jpeg_compress_struct* const cinfo,
                     const PaddedBytes& icc) {
  constexpr size_t kMaxIccBytesInMarker =
      kMaxBytesInMarker - sizeof kICCSignature - 2;
  const int num_markers =
      static_cast<int>(DivCeil(icc.size(), kMaxIccBytesInMarker));
  size_t begin = 0;
  for (int current_marker = 0; current_marker < num_markers; ++current_marker) {
    const size_t length = std::min(kMaxIccBytesInMarker, icc.size() - begin);
    jpeg_write_m_header(
        cinfo, kICCMarker,
        static_cast<unsigned int>(length + sizeof kICCSignature + 2));
    for (const unsigned char c : kICCSignature) {
      jpeg_write_m_byte(cinfo, c);
    }
    jpeg_write_m_byte(cinfo, current_marker + 1);
    jpeg_write_m_byte(cinfo, num_markers);
    for (size_t i = 0; i < length; ++i) {
      jpeg_write_m_byte(cinfo, icc[begin]);
      ++begin;
    }
  }
}
void WriteExif(jpeg_compress_struct* const cinfo, const PaddedBytes& exif) {
  if (exif.size() < 4) return;
  jpeg_write_m_header(
      cinfo, kExifMarker,
      static_cast<unsigned int>(exif.size() - 4 + sizeof kExifSignature));
  for (const unsigned char c : kExifSignature) {
    jpeg_write_m_byte(cinfo, c);
  }
  for (size_t i = 4; i < exif.size(); ++i) {
    jpeg_write_m_byte(cinfo, exif[i]);
  }
}

Status SetChromaSubsampling(const YCbCrChromaSubsampling& chroma_subsampling,
                            jpeg_compress_struct* const cinfo) {
  for (size_t i = 0; i < 3; i++) {
    cinfo->comp_info[i].h_samp_factor =
        1 << (chroma_subsampling.MaxHShift() -
              chroma_subsampling.HShift(i < 2 ? i ^ 1 : i));
    cinfo->comp_info[i].v_samp_factor =
        1 << (chroma_subsampling.MaxVShift() -
              chroma_subsampling.VShift(i < 2 ? i ^ 1 : i));
  }
  return true;
}

}  // namespace

Status EncodeWithLibJpeg(const ImageBundle* ib, const CodecInOut* io,
                         size_t quality,
                         const YCbCrChromaSubsampling& chroma_subsampling,
                         PaddedBytes* bytes) {
  jpeg_compress_struct cinfo;
  // cinfo is initialized by libjpeg, which we are not instrumenting with
  // msan.
  msan::UnpoisonMemory(&cinfo, sizeof(cinfo));
  jpeg_error_mgr jerr;
  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_compress(&cinfo);
  unsigned char* buffer = nullptr;
  unsigned long size = 0;
  jpeg_mem_dest(&cinfo, &buffer, &size);
  cinfo.image_width = ib->xsize();
  cinfo.image_height = ib->ysize();
  if (ib->IsGray()) {
    cinfo.input_components = 1;
    cinfo.in_color_space = JCS_GRAYSCALE;
  } else {
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;
  }
  jpeg_set_defaults(&cinfo);
  cinfo.optimize_coding = TRUE;
  if (cinfo.input_components == 3) {
    JXL_RETURN_IF_ERROR(SetChromaSubsampling(chroma_subsampling, &cinfo));
  }
  jpeg_set_quality(&cinfo, quality, TRUE);
  jpeg_start_compress(&cinfo, TRUE);
  if (!ib->IsSRGB()) {
    WriteICCProfile(&cinfo, ib->c_current().ICC());
  }
  WriteExif(&cinfo, io->blobs.exif);
  if (cinfo.input_components > 3 || cinfo.input_components < 0)
    return JXL_FAILURE("invalid numbers of components");

  std::unique_ptr<JSAMPLE[]> row(
      new JSAMPLE[cinfo.input_components * cinfo.image_width]);
  for (size_t y = 0; y < ib->ysize(); ++y) {
    const float* const JXL_RESTRICT input_row[3] = {
        ib->color().ConstPlaneRow(0, y), ib->color().ConstPlaneRow(1, y),
        ib->color().ConstPlaneRow(2, y)};
    for (size_t x = 0; x < ib->xsize(); ++x) {
      for (size_t c = 0; c < static_cast<size_t>(cinfo.input_components); ++c) {
        JXL_RETURN_IF_ERROR(c < 3);
        row[cinfo.input_components * x + c] = static_cast<JSAMPLE>(
            std::max(std::min(kJPEGSampleMultiplier * input_row[c][x] + .5f,
                              kJPEGSampleMax),
                     kJPEGSampleMin));
      }
    }
    JSAMPROW rows[] = {row.get()};
    jpeg_write_scanlines(&cinfo, rows, 1);
  }
  jpeg_finish_compress(&cinfo);
  jpeg_destroy_compress(&cinfo);
  bytes->resize(size);
  // Compressed image data is initialized by libjpeg, which we are not
  // instrumenting with msan.
  msan::UnpoisonMemory(buffer, size);
  std::copy_n(buffer, size, bytes->data());
  std::free(buffer);
  return true;
}

Status EncodeWithSJpeg(const ImageBundle* ib, size_t quality,
                       const YCbCrChromaSubsampling& chroma_subsampling,
                       PaddedBytes* bytes) {
#if !JPEGXL_ENABLE_SJPEG
  return JXL_FAILURE("JPEG XL was built without sjpeg support");
#else
  sjpeg::EncoderParam param(quality);
  if (!ib->IsSRGB()) {
    param.iccp.assign(ib->metadata()->color_encoding.ICC().begin(),
                      ib->metadata()->color_encoding.ICC().end());
  }
  if (chroma_subsampling.Is444()) {
    param.yuv_mode = SJPEG_YUV_444;
  } else if (chroma_subsampling.Is420()) {
    param.yuv_mode = SJPEG_YUV_SHARP;
  } else {
    return JXL_FAILURE("sjpeg does not support this chroma subsampling mode");
  }
  std::vector<uint8_t> rgb;
  rgb.reserve(ib->xsize() * ib->ysize() * 3);
  for (size_t y = 0; y < ib->ysize(); ++y) {
    const float* const rows[] = {
        ib->color().ConstPlaneRow(0, y),
        ib->color().ConstPlaneRow(1, y),
        ib->color().ConstPlaneRow(2, y),
    };
    for (size_t x = 0; x < ib->xsize(); ++x) {
      for (const float* const row : rows) {
        rgb.push_back(static_cast<uint8_t>(
            std::max(0.f, std::min(255.f, roundf(255.f * row[x])))));
      }
    }
  }
  std::string output;
  JXL_RETURN_IF_ERROR(sjpeg::Encode(rgb.data(), ib->xsize(), ib->ysize(),
                                    ib->xsize() * 3, param, &output));
  bytes->assign(
      reinterpret_cast<const uint8_t*>(output.data()),
      reinterpret_cast<const uint8_t*>(output.data() + output.size()));
  return true;
#endif
}

Status EncodeImageJPG(const CodecInOut* io, JpegEncoder encoder, size_t quality,
                      YCbCrChromaSubsampling chroma_subsampling,
                      ThreadPool* pool, PaddedBytes* bytes) {
  if (io->Main().HasAlpha()) {
    return JXL_FAILURE("alpha is not supported");
  }
  if (quality > 100) {
    return JXL_FAILURE("please specify a 0-100 JPEG quality");
  }

  const ImageBundle* ib;
  ImageMetadata metadata = io->metadata.m;
  ImageBundle ib_store(&metadata);
  JXL_RETURN_IF_ERROR(TransformIfNeeded(io->Main(),
                                        io->metadata.m.color_encoding,
                                        GetJxlCms(), pool, &ib_store, &ib));

  switch (encoder) {
    case JpegEncoder::kLibJpeg:
      JXL_RETURN_IF_ERROR(
          EncodeWithLibJpeg(ib, io, quality, chroma_subsampling, bytes));
      break;
    case JpegEncoder::kSJpeg:
      JXL_RETURN_IF_ERROR(
          EncodeWithSJpeg(ib, quality, chroma_subsampling, bytes));
      break;
    default:
      return JXL_FAILURE("tried to use an unknown JPEG encoder");
  }

  return true;
}

}  // namespace extras
}  // namespace jxl
