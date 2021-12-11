// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/extras/codec_jpg.h"

#include <stddef.h>
#include <stdio.h>

#if JPEGXL_ENABLE_JPEG
// After stddef/stdio
#include <jpeglib.h>
#include <setjmp.h>
#include <stdint.h>
#endif  // JPEGXL_ENABLE_JPEG

#include <algorithm>
#include <iterator>
#include <numeric>
#include <utility>
#include <vector>

#include "lib/extras/time.h"
#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/color_encoding_internal.h"
#include "lib/jxl/color_management.h"
#include "lib/jxl/common.h"
#include "lib/jxl/enc_image_bundle.h"
#include "lib/jxl/image.h"
#include "lib/jxl/image_bundle.h"
#include "lib/jxl/image_ops.h"
#include "lib/jxl/jpeg/dec_jpeg_data_writer.h"
#include "lib/jxl/jpeg/enc_jpeg_data.h"
#include "lib/jxl/jpeg/enc_jpeg_data_reader.h"
#include "lib/jxl/luminance.h"
#include "lib/jxl/sanitizers.h"
#if JPEGXL_ENABLE_SJPEG
#include "sjpeg.h"
#endif

namespace jxl {
namespace extras {

#if JPEGXL_ENABLE_JPEG
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

bool MarkerIsICC(const jpeg_saved_marker_ptr marker) {
  return marker->marker == kICCMarker &&
         marker->data_length >= sizeof kICCSignature + 2 &&
         std::equal(std::begin(kICCSignature), std::end(kICCSignature),
                    marker->data);
}
bool MarkerIsExif(const jpeg_saved_marker_ptr marker) {
  return marker->marker == kExifMarker &&
         marker->data_length >= sizeof kExifSignature + 2 &&
         std::equal(std::begin(kExifSignature), std::end(kExifSignature),
                    marker->data);
}

Status ReadICCProfile(jpeg_decompress_struct* const cinfo,
                      std::vector<uint8_t>* const icc) {
  constexpr size_t kICCSignatureSize = sizeof kICCSignature;
  // ICC signature + uint8_t index + uint8_t max_index.
  constexpr size_t kICCHeadSize = kICCSignatureSize + 2;
  // Markers are 1-indexed, and we keep them that way in this vector to get a
  // convenient 0 at the front for when we compute the offsets later.
  std::vector<size_t> marker_lengths;
  int num_markers = 0;
  int seen_markers_count = 0;
  bool has_num_markers = false;
  for (jpeg_saved_marker_ptr marker = cinfo->marker_list; marker != nullptr;
       marker = marker->next) {
    // marker is initialized by libjpeg, which we are not instrumenting with
    // msan.
    msan::UnpoisonMemory(marker, sizeof(*marker));
    msan::UnpoisonMemory(marker->data, marker->data_length);
    if (!MarkerIsICC(marker)) continue;

    const int current_marker = marker->data[kICCSignatureSize];
    if (current_marker == 0) {
      return JXL_FAILURE("inconsistent JPEG ICC marker numbering");
    }
    const int current_num_markers = marker->data[kICCSignatureSize + 1];
    if (current_marker > current_num_markers) {
      return JXL_FAILURE("inconsistent JPEG ICC marker numbering");
    }
    if (has_num_markers) {
      if (current_num_markers != num_markers) {
        return JXL_FAILURE("inconsistent numbers of JPEG ICC markers");
      }
    } else {
      num_markers = current_num_markers;
      has_num_markers = true;
      marker_lengths.resize(num_markers + 1);
    }

    size_t marker_length = marker->data_length - kICCHeadSize;

    if (marker_length == 0) {
      // NB: if we allow empty chunks, then the next check is incorrect.
      return JXL_FAILURE("Empty ICC chunk");
    }

    if (marker_lengths[current_marker] != 0) {
      return JXL_FAILURE("duplicate JPEG ICC marker number");
    }
    marker_lengths[current_marker] = marker_length;
    seen_markers_count++;
  }

  if (marker_lengths.empty()) {
    // Not an error.
    return false;
  }

  if (seen_markers_count != num_markers) {
    JXL_DASSERT(has_num_markers);
    return JXL_FAILURE("Incomplete set of ICC chunks");
  }

  std::vector<size_t> offsets = std::move(marker_lengths);
  std::partial_sum(offsets.begin(), offsets.end(), offsets.begin());
  icc->resize(offsets.back());

  for (jpeg_saved_marker_ptr marker = cinfo->marker_list; marker != nullptr;
       marker = marker->next) {
    if (!MarkerIsICC(marker)) continue;
    const uint8_t* first = marker->data + kICCHeadSize;
    uint8_t current_marker = marker->data[kICCSignatureSize];
    size_t offset = offsets[current_marker - 1];
    size_t marker_length = offsets[current_marker] - offset;
    std::copy_n(first, marker_length, icc->data() + offset);
  }

  return true;
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

void MyErrorExit(j_common_ptr cinfo) {
  jmp_buf* env = static_cast<jmp_buf*>(cinfo->client_data);
  (*cinfo->err->output_message)(cinfo);
  jpeg_destroy_decompress(reinterpret_cast<j_decompress_ptr>(cinfo));
  longjmp(*env, 1);
}

void MyOutputMessage(j_common_ptr cinfo) {
#if JXL_DEBUG_WARNING == 1
  char buf[JMSG_LENGTH_MAX];
  (*cinfo->err->format_message)(cinfo, buf);
  JXL_WARNING("%s", buf);
#endif
}

}  // namespace
#endif  // JPEGXL_ENABLE_JPEG

Status DecodeImageJPGCoefficients(Span<const uint8_t> bytes, CodecInOut* io) {
  if (!IsJPG(bytes)) return false;
  // Use brunsli JPEG decoder to read quantized coefficients.
  if (!jpeg::DecodeImageJPG(bytes, io)) {
    fprintf(stderr, "Corrupt or CMYK JPEG.\n");
    return false;
  }
  return true;
}

Status DecodeImageJPG(const Span<const uint8_t> bytes,
                      const ColorHints& color_hints,
                      const SizeConstraints& constraints,
                      PackedPixelFile* ppf) {
  // Don't do anything for non-JPEG files (no need to report an error)
  if (!IsJPG(bytes)) return false;

#if JPEGXL_ENABLE_JPEG
  // TODO(veluca): use JPEGData also for pixels?

  // We need to declare all the non-trivial destructor local variables before
  // the call to setjmp().
  ColorEncoding color_encoding;
  PaddedBytes icc;
  Image3F image;
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
    cinfo.err = jpeg_std_error(&jerr);
    jerr.error_exit = &MyErrorExit;
    jerr.output_message = &MyOutputMessage;
    if (setjmp(env)) {
      return false;
    }
    cinfo.client_data = static_cast<void*>(&env);

    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, reinterpret_cast<const unsigned char*>(bytes.data()),
                 bytes.size());
    jpeg_save_markers(&cinfo, kICCMarker, 0xFFFF);
    jpeg_save_markers(&cinfo, kExifMarker, 0xFFFF);
    jpeg_read_header(&cinfo, TRUE);
    const auto failure = [&cinfo](const char* str) -> Status {
      jpeg_abort_decompress(&cinfo);
      jpeg_destroy_decompress(&cinfo);
      return JXL_FAILURE("%s", str);
    };
    if (!VerifyDimensions(&constraints, cinfo.image_width,
                          cinfo.image_height)) {
      return failure("image too big");
    }
    // Might cause CPU-zip bomb.
    if (cinfo.arith_code) {
      return failure("arithmetic code JPEGs are not supported");
    }
    if (!ReadICCProfile(&cinfo, &ppf->icc)) {
      ppf->icc.clear();
      // Default to SRGB
      ppf->color_encoding.color_space = cinfo.output_components == 1
                                            ? JXL_COLOR_SPACE_GRAY
                                            : JXL_COLOR_SPACE_RGB;
      ppf->color_encoding.white_point = JXL_WHITE_POINT_D65;
      ppf->color_encoding.primaries = JXL_PRIMARIES_SRGB;
      ppf->color_encoding.transfer_function = JXL_TRANSFER_FUNCTION_SRGB;
      ppf->color_encoding.rendering_intent = JXL_RENDERING_INTENT_PERCEPTUAL;
    }
    ReadExif(&cinfo, &ppf->metadata.exif);
    int nbcomp = cinfo.num_components;
    if (nbcomp != 1 && nbcomp != 3) {
      return failure("unsupported number of components in JPEG");
    }
    if (!ApplyColorHints(color_hints, /*color_already_set=*/true,
                         /*is_gray=*/false, ppf)) {
      return failure("ApplyColorHints failed");
    }

    ppf->info.xsize = cinfo.image_width;
    ppf->info.ysize = cinfo.image_height;
    // Original data is uint, so exponent_bits_per_sample = 0.
    ppf->info.bits_per_sample = BITS_IN_JSAMPLE;
    JXL_ASSERT(BITS_IN_JSAMPLE == 8 || BITS_IN_JSAMPLE == 16);
    ppf->info.exponent_bits_per_sample = 0;
    ppf->info.uses_original_profile = true;

    // No alpha in JPG
    ppf->info.alpha_bits = 0;
    ppf->info.alpha_exponent_bits = 0;

    ppf->info.num_color_channels = nbcomp;
    ppf->info.orientation = JXL_ORIENT_IDENTITY;

    jpeg_start_decompress(&cinfo);
    JXL_ASSERT(cinfo.output_components == nbcomp);

    const JxlPixelFormat format{
        /*num_channels=*/static_cast<uint32_t>(nbcomp),
        /*data_type=*/BITS_IN_JSAMPLE == 8 ? JXL_TYPE_UINT8 : JXL_TYPE_UINT16,
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
      jpeg_read_scanlines(&cinfo, rows, 1);
      msan::UnpoisonMemory(rows[0], sizeof(JSAMPLE) * cinfo.output_components *
                                        cinfo.image_width);
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    return true;
  };

  return try_catch_block();
#else   // JPEGXL_ENABLE_JPEG
  return JXL_FAILURE("JPEG decoding not enabled at build time.");
#endif  // JPEGXL_ENABLE_JPEG
}

#if JPEGXL_ENABLE_JPEG
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
#endif  // JPEGXL_ENABLE_JPEG

Status EncodeImageJPGCoefficients(const CodecInOut* io, PaddedBytes* bytes) {
  auto write = [&bytes](const uint8_t* buf, size_t len) {
    bytes->append(buf, buf + len);
    return len;
  };
  return jpeg::WriteJpeg(*io->Main().jpeg_data, write);
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

#if JPEGXL_ENABLE_JPEG
  const ImageBundle* ib;
  ImageMetadata metadata = io->metadata.m;
  ImageBundle ib_store(&metadata);
  JXL_RETURN_IF_ERROR(TransformIfNeeded(
      io->Main(), io->metadata.m.color_encoding, pool, &ib_store, &ib));

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
#else   // JPEGXL_ENABLE_JPEG
  return JXL_FAILURE("JPEG pixel encoding not enabled at build time");
#endif  // JPEGXL_ENABLE_JPEG
}

}  // namespace extras
}  // namespace jxl
