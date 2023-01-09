// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jpegli/encode.h"

#include "lib/jpegli/dct.h"
#include "lib/jpegli/encode_internal.h"
#include "lib/jpegli/entropy_coding.h"
#include "lib/jpegli/error.h"
#include "lib/jpegli/memory_manager.h"
#include "lib/jpegli/quant.h"
#include "lib/jxl/color_encoding_internal.h"
#include "lib/jxl/enc_adaptive_quantization.h"
#include "lib/jxl/enc_color_management.h"
#include "lib/jxl/enc_xyb.h"
#include "lib/jxl/jpeg/dec_jpeg_data_writer.h"
#include "lib/jxl/jpeg/enc_jpeg_data.h"

namespace jpegli {

double QualityToDistance(int quality) {
  return (quality >= 100  ? 0.0
          : quality >= 30 ? 0.1 + (100 - quality) * 0.09
                          : 53.0 / 3000.0 * quality * quality -
                                23.0 / 20.0 * quality + 25.0);
}

double LinearQualityToDistance(int scale_factor) {
  scale_factor = std::min(5000, std::max(0, scale_factor));
  int quality =
      scale_factor < 100 ? 100 - scale_factor / 2 : 5000 / scale_factor;
  return QualityToDistance(quality);
}

struct ProgressiveScan {
  int Ss, Se, Ah, Al;
  bool interleaved;
};

template <typename T>
std::vector<uint8_t> CreateICCAppMarker(const T& icc) {
  std::vector<uint8_t> icc_marker(17 + icc.size());
  // See the APP2 marker format for embedded ICC profile at
  // https://www.color.org/technotes/ICC-Technote-ProfileEmbedding.pdf
  icc_marker[0] = 0xe2;  // APP2 marker
  // ICC marker size (excluding the marker bytes).
  icc_marker[1] = (icc_marker.size() - 1) >> 8;
  icc_marker[2] = (icc_marker.size() - 1) & 0xFF;
  // Byte sequence identifying an APP2 marker containing an icc profile.
  memcpy(&icc_marker[3], "ICC_PROFILE", 12);
  icc_marker[15] = 1;  // Sequence number
  icc_marker[16] = 1;  // Number of chunks.
  memcpy(&icc_marker[17], icc.data(), icc.size());
  return icc_marker;
}

std::vector<uint8_t> CreateXybICCAppMarker() {
  jxl::ColorEncoding c_xyb;
  c_xyb.SetColorSpace(jxl::ColorSpace::kXYB);
  c_xyb.rendering_intent = jxl::RenderingIntent::kPerceptual;
  JXL_CHECK(c_xyb.CreateICC());
  return CreateICCAppMarker(c_xyb.ICC());
}

void SetICCAppMarker(const std::vector<uint8_t>& icc,
                     jxl::jpeg::JPEGData* jpg) {
  std::vector<std::vector<uint8_t>> app_data;
  bool icc_added = false;
  for (auto& v : jpg->app_data) {
    JXL_DASSERT(!v.empty());
    if (v[0] != 0xe2) {
      app_data.emplace_back(std::move(v));
    } else if (!icc_added) {
      // TODO(szabadka) Handle too big icc data.
      app_data.push_back(icc);
      icc_added = true;
    }
  }
  if (!icc_added) {
    app_data.push_back(icc);
  }
  std::swap(jpg->app_data, app_data);
}

void AddJpegScanInfos(const std::vector<ProgressiveScan>& scans,
                      std::vector<jxl::jpeg::JPEGScanInfo>* scan_infos) {
  for (const auto& scan : scans) {
    jxl::jpeg::JPEGScanInfo si;
    si.Ss = scan.Ss;
    si.Se = scan.Se;
    si.Ah = scan.Ah;
    si.Al = scan.Al;
    if (scan.interleaved) {
      si.num_components = 3;
      for (uint32_t c = 0; c < 3; ++c) {
        si.components[c].comp_idx = c;
      }
      scan_infos->push_back(si);
    } else {
      for (uint32_t c = 0; c < 3; ++c) {
        si.num_components = 1;
        si.components[0].comp_idx = c;
        scan_infos->push_back(si);
      }
    }
  }
}

}  // namespace jpegli

void jpegli_CreateCompress(j_compress_ptr cinfo, int version,
                           size_t structsize) {
  if (structsize != sizeof(*cinfo)) {
    JPEGLI_ERROR("jpegli_compress_struct has wrong size.");
  }
  cinfo->master = new jpeg_comp_master;
  cinfo->mem =
      reinterpret_cast<struct jpeg_memory_mgr*>(new jpegli::MemoryManager);
  cinfo->is_decompressor = FALSE;
  cinfo->dest = nullptr;
  cinfo->master->cur_marker_data = nullptr;
}

void jpegli_destroy_compress(j_compress_ptr cinfo) {
  jpegli_destroy(reinterpret_cast<j_common_ptr>(cinfo));
}

void jpegli_set_defaults(j_compress_ptr cinfo) {
  cinfo->num_components = cinfo->input_components;
}

void jpegli_default_colorspace(j_compress_ptr cinfo) {}

void jpegli_set_colorspace(j_compress_ptr cinfo, J_COLOR_SPACE colorspace) {
  cinfo->master->jpeg_colorspace = colorspace;
}

void jpegli_set_quality(j_compress_ptr cinfo, int quality,
                        boolean force_baseline) {
  cinfo->master->distance = jpegli::QualityToDistance(quality);
  cinfo->master->force_baseline = force_baseline;
}

void jpegli_set_linear_quality(j_compress_ptr cinfo, int scale_factor,
                               boolean force_baseline) {
  cinfo->master->distance = jpegli::LinearQualityToDistance(scale_factor);
  cinfo->master->force_baseline = force_baseline;
}

int jpegli_quality_scaling(int quality) {
  quality = std::min(100, std::max(1, quality));
  return quality < 50 ? 5000 / quality : 200 - 2 * quality;
}

void jpegli_add_quant_table(j_compress_ptr cinfo, int which_tbl,
                            const unsigned int* basic_table, int scale_factor,
                            boolean force_baseline) {}

void jpegli_simple_progression(j_compress_ptr cinfo) {}

void jpegli_suppress_tables(j_compress_ptr cinfo, boolean suppress) {}

void jpegli_write_m_header(j_compress_ptr cinfo, int marker,
                           unsigned int datalen) {
  jpeg_comp_master* m = cinfo->master;
  if (datalen > 65533u) {
    JPEGLI_ERROR("Invalid marker length %u", datalen);
  }
  std::vector<uint8_t> marker_data(3);
  marker_data[0] = marker;
  marker_data[1] = datalen >> 8;
  marker_data[2] = datalen & 0xff;
  if (marker >= 0xe0 && marker <= 0xef) {
    m->jpeg_data.app_data.emplace_back(std::move(marker_data));
    m->cur_marker_data = &m->jpeg_data.app_data.back();
  } else if (marker == 0xfe) {
    m->jpeg_data.com_data.emplace_back(std::move(marker_data));
    m->cur_marker_data = &m->jpeg_data.com_data.back();
  } else {
    JPEGLI_ERROR(
        "jpegli_write_m_header: "
        "Only APP and COM markers are supported.");
  }
}

void jpegli_write_m_byte(j_compress_ptr cinfo, int val) {
  jpeg_comp_master* m = cinfo->master;
  if (m->cur_marker_data == nullptr) {
    JPEGLI_ERROR("Marker header missing.");
  }
  m->cur_marker_data->push_back(val);
}

void jpegli_write_icc_profile(j_compress_ptr cinfo, const JOCTET* icc_data_ptr,
                              unsigned int icc_data_len) {
  if (icc_data_len > 65517u) {
    // TODO(szabadka) Handle chunked ICC profiles.
    JPEGLI_ERROR("ICC data is too long.");
  }
  jpeg_comp_master* m = cinfo->master;
  m->jpeg_data.app_data.push_back(jpegli::CreateICCAppMarker(
      jxl::Span<const uint8_t>(icc_data_ptr, icc_data_len)));
}

void jpegli_start_compress(j_compress_ptr cinfo, boolean write_all_tables) {
  jpeg_comp_master* m = cinfo->master;
  m->input = jxl::Image3F(cinfo->image_width, cinfo->image_height);
  cinfo->next_scanline = 0;
}

JDIMENSION jpegli_write_scanlines(j_compress_ptr cinfo, JSAMPARRAY scanlines,
                                  JDIMENSION num_lines) {
  jpeg_comp_master* m = cinfo->master;
  // TODO(szabadka) Handle CMYK component input images.
  if (cinfo->num_components > 3) {
    JPEGLI_ERROR("Only at most 3 image components are supported.");
  }
  if (num_lines + cinfo->next_scanline > cinfo->image_height) {
    num_lines = cinfo->image_height - cinfo->next_scanline;
  }
  static constexpr double kMul = 1.0 / 255.0;
  for (int c = 0; c < cinfo->num_components; ++c) {
    for (size_t i = 0; i < num_lines; ++i) {
      float* row = m->input.PlaneRow(c, cinfo->next_scanline + i);
      for (size_t x = 0; x < cinfo->image_width; ++x) {
        row[x] = scanlines[i][3 * x + c] * kMul;
      }
    }
  }
  cinfo->next_scanline += num_lines;
  return num_lines;
}

void jpegli_finish_compress(j_compress_ptr cinfo) {
  jpeg_comp_master* m = cinfo->master;
  m->jpeg_data.components.resize(cinfo->num_components);
  jxl::ColorEncoding color_encoding;
  if (!jxl::jpeg::SetColorEncodingFromJpegData(m->jpeg_data, &color_encoding)) {
    JPEGLI_ERROR("Could not parse ICC profile.");
  }
  const bool use_xyb = false;
  if (use_xyb) {
    jpegli::SetICCAppMarker(jpegli::CreateXybICCAppMarker(), &m->jpeg_data);
  }
  const jxl::Image3F& input = m->input;
  float distance = m->distance;
  std::vector<uint8_t> compressed;

  const bool subsample_blue = use_xyb;
  const size_t max_shift = subsample_blue ? 1 : 0;
  jxl::FrameDimensions frame_dim;
  frame_dim.Set(input.xsize(), input.ysize(), 1, max_shift, max_shift, false,
                1);

  // Convert input to XYB colorspace.
  jxl::Image3F opsin(frame_dim.xsize_padded, frame_dim.ysize_padded);
  opsin.ShrinkTo(frame_dim.xsize, frame_dim.ysize);
  jxl::Image3FToXYB(input, color_encoding, 255.0, nullptr, &opsin,
                    jxl::GetJxlCms());
  PadImageToBlockMultipleInPlace(&opsin, 8 << max_shift);

  // Compute adaptive quant field.
  jxl::ImageF mask;
  jxl::ImageF qf =
      jxl::InitialQuantField(distance, opsin, frame_dim, nullptr, 1.0, &mask);
  if (use_xyb) {
    ScaleXYB(&opsin);
  } else {
    opsin.ShrinkTo(input.xsize(), input.ysize());
    JXL_CHECK(RgbToYcbcr(input.Plane(0), input.Plane(1), input.Plane(2),
                         &opsin.Plane(0), &opsin.Plane(1), &opsin.Plane(2),
                         nullptr));
    PadImageToBlockMultipleInPlace(&opsin, 8 << max_shift);
  }

  // Create jpeg data and optimize Huffman codes.
  float global_scale = 0.66f;
  if (!use_xyb) {
    global_scale /= 500;
    if (color_encoding.tf.IsPQ()) {
      global_scale *= .4f;
    } else if (color_encoding.tf.IsHLG()) {
      global_scale *= .5f;
    }
  }
  float dc_quant = jxl::InitialQuantDC(distance);

  // APPn
  for (const auto& v : m->jpeg_data.app_data) {
    JXL_DASSERT(!v.empty());
    uint8_t marker = v[0];
    m->jpeg_data.marker_order.push_back(marker);
  }

  // DQT
  m->jpeg_data.marker_order.emplace_back(0xdb);
  float qm[3 * jxl::kDCTBlockSize];
  jpegli::AddJpegQuantMatrices(qf, use_xyb, dc_quant, global_scale,
                               &m->jpeg_data.quant, qm);

  // SOF
  m->jpeg_data.marker_order.emplace_back(0xc2);
  m->jpeg_data.components.resize(3);
  m->jpeg_data.height = frame_dim.ysize;
  m->jpeg_data.width = frame_dim.xsize;
  if (use_xyb) {
    m->jpeg_data.components[0].id = 'R';
    m->jpeg_data.components[1].id = 'G';
    m->jpeg_data.components[2].id = 'B';
  } else {
    m->jpeg_data.components[0].id = 1;
    m->jpeg_data.components[1].id = 2;
    m->jpeg_data.components[2].id = 3;
  }
  size_t max_samp_factor = subsample_blue ? 2 : 1;
  for (size_t c = 0; c < 3; ++c) {
    const size_t factor = (subsample_blue && c == 2) ? 2 : 1;
    m->jpeg_data.components[c].h_samp_factor = max_samp_factor / factor;
    m->jpeg_data.components[c].v_samp_factor = max_samp_factor / factor;
    JXL_ASSERT(frame_dim.xsize_blocks % factor == 0);
    JXL_ASSERT(frame_dim.ysize_blocks % factor == 0);
    m->jpeg_data.components[c].width_in_blocks =
        frame_dim.xsize_blocks / factor;
    m->jpeg_data.components[c].height_in_blocks =
        frame_dim.ysize_blocks / factor;
    m->jpeg_data.components[c].quant_idx = c;
  }
  jpegli::ComputeDCTCoefficients(opsin, use_xyb, qf, frame_dim, qm,
                                 &m->jpeg_data.components);

  // DHT (the actual Huffman codes will be added later).
  m->jpeg_data.marker_order.emplace_back(0xc4);

  // SOS
  std::vector<jpegli::ProgressiveScan> progressive_mode = {
      // DC
      {0, 0, 0, 0, !subsample_blue}, {1, 2, 0, 0, false},  {3, 63, 0, 2, false},
      {3, 63, 2, 1, false},          {3, 63, 1, 0, false},
  };
  jpegli::AddJpegScanInfos(progressive_mode, &m->jpeg_data.scan_info);
  for (size_t i = 0; i < m->jpeg_data.scan_info.size(); i++) {
    m->jpeg_data.marker_order.emplace_back(0xda);
  }

  // EOI
  m->jpeg_data.marker_order.push_back(0xd9);

  jpegli::OptimizeHuffmanCodes(&m->jpeg_data);

  // Write jpeg data to compressed stream.
  auto write = [&compressed](const uint8_t* buf, size_t len) {
    compressed.insert(compressed.end(), buf, buf + len);
    return len;
  };
  if (!jxl::jpeg::WriteJpeg(m->jpeg_data, write)) {
    JPEGLI_ERROR("Writing jpeg data failed.");
  }

  (*cinfo->dest->init_destination)(cinfo);
  size_t pos = 0;
  while (pos < compressed.size()) {
    if (cinfo->dest->free_in_buffer == 0 &&
        !(*cinfo->dest->empty_output_buffer)(cinfo)) {
      JPEGLI_ERROR("Destination suspension is not supported.");
    }
    size_t len =
        std::min<size_t>(cinfo->dest->free_in_buffer, compressed.size() - pos);
    memcpy(cinfo->dest->next_output_byte, &compressed[pos], len);
    pos += len;
    cinfo->dest->free_in_buffer -= len;
    cinfo->dest->next_output_byte += len;
  }
  (*cinfo->dest->term_destination)(cinfo);
}
