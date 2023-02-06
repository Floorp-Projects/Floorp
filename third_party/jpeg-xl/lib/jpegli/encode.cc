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
#include "lib/jxl/base/byte_order.h"
#include "lib/jxl/color_encoding_internal.h"
#include "lib/jxl/common.h"
#include "lib/jxl/enc_adaptive_quantization.h"
#include "lib/jxl/enc_color_management.h"
#include "lib/jxl/enc_xyb.h"
#include "lib/jxl/jpeg/dec_jpeg_data_writer.h"
#include "lib/jxl/jpeg/enc_jpeg_data.h"

namespace jpegli {

constexpr unsigned char kICCSignature[12] = {
    0x49, 0x43, 0x43, 0x5F, 0x50, 0x52, 0x4F, 0x46, 0x49, 0x4C, 0x45, 0x00};
constexpr int kICCMarker = JPEG_APP0 + 2;
constexpr size_t kMaxBytesInMarker = 65533;

float LinearQualityToDistance(int scale_factor) {
  scale_factor = std::min(5000, std::max(0, scale_factor));
  int quality =
      scale_factor < 100 ? 100 - scale_factor / 2 : 5000 / scale_factor;
  return jpegli_quality_to_distance(quality);
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

void CopyJpegScanInfos(j_compress_ptr cinfo,
                       std::vector<jxl::jpeg::JPEGScanInfo>* scan_infos) {
  for (int i = 0; i < cinfo->num_scans; ++i) {
    const jpeg_scan_info& scan_info = cinfo->scan_info[i];
    jxl::jpeg::JPEGScanInfo si;
    si.Ss = scan_info.Ss;
    si.Se = scan_info.Se;
    si.Ah = scan_info.Ah;
    si.Al = scan_info.Al;
    si.num_components = scan_info.comps_in_scan;
    for (int i = 0; i < scan_info.comps_in_scan; ++i) {
      si.components[i].comp_idx = scan_info.component_index[i];
    }
    scan_infos->push_back(si);
  }
}

}  // namespace jpegli

void jpegli_CreateCompress(j_compress_ptr cinfo, int version,
                           size_t structsize) {
  cinfo->master = nullptr;
  cinfo->mem = nullptr;
  if (structsize != sizeof(*cinfo)) {
    JPEGLI_ERROR("jpegli_compress_struct has wrong size.");
  }
  cinfo->master = new jpeg_comp_master;
  cinfo->mem =
      reinterpret_cast<struct jpeg_memory_mgr*>(new jpegli::MemoryManager);
  cinfo->is_decompressor = FALSE;
  cinfo->dest = nullptr;
  cinfo->master->cur_marker_data = nullptr;
  cinfo->master->distance = 1.0;
  cinfo->master->xyb_mode = false;
  cinfo->master->data_type = JPEGLI_TYPE_UINT8;
  cinfo->master->endianness = JPEGLI_NATIVE_ENDIAN;
}

void jpegli_destroy_compress(j_compress_ptr cinfo) {
  jpegli_destroy(reinterpret_cast<j_common_ptr>(cinfo));
}

void jpegli_set_xyb_mode(j_compress_ptr cinfo) {
  cinfo->master->xyb_mode = true;
}

void jpegli_set_defaults(j_compress_ptr cinfo) {
  cinfo->num_components = cinfo->input_components;
  cinfo->comp_info =
      jpegli::Allocate<jpeg_component_info>(cinfo, cinfo->num_components);
  for (int c = 0; c < cinfo->num_components; ++c) {
    jpeg_component_info* comp = &cinfo->comp_info[c];
    comp->h_samp_factor = 1;
    comp->v_samp_factor = 1;
  }
  cinfo->scan_info = nullptr;
  cinfo->num_scans = 0;
}

void jpegli_default_colorspace(j_compress_ptr cinfo) {}

void jpegli_set_colorspace(j_compress_ptr cinfo, J_COLOR_SPACE colorspace) {
  cinfo->master->jpeg_colorspace = colorspace;
}

void jpegli_set_distance(j_compress_ptr cinfo, float distance) {
  cinfo->master->distance = distance;
}

float jpegli_quality_to_distance(int quality) {
  return (quality >= 100  ? 0.01f
          : quality >= 30 ? 0.1f + (100 - quality) * 0.09f
                          : 53.0f / 3000.0f * quality * quality -
                                23.0f / 20.0f * quality + 25.0f);
}

void jpegli_set_quality(j_compress_ptr cinfo, int quality,
                        boolean force_baseline) {
  cinfo->master->distance = jpegli_quality_to_distance(quality);
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
  if (datalen > jpegli::kMaxBytesInMarker) {
    JPEGLI_ERROR("Invalid marker length %u", datalen);
  }
  std::vector<uint8_t> marker_data(3);
  marker_data[0] = marker;
  marker_data[1] = (datalen + 2) >> 8;
  marker_data[2] = (datalen + 2) & 0xff;
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
  constexpr size_t kMaxIccBytesInMarker =
      jpegli::kMaxBytesInMarker - sizeof jpegli::kICCSignature - 2;
  const int num_markers =
      static_cast<int>(jxl::DivCeil(icc_data_len, kMaxIccBytesInMarker));
  size_t begin = 0;
  for (int current_marker = 0; current_marker < num_markers; ++current_marker) {
    const size_t length = std::min(kMaxIccBytesInMarker, icc_data_len - begin);
    jpegli_write_m_header(
        cinfo, jpegli::kICCMarker,
        static_cast<unsigned int>(length + sizeof jpegli::kICCSignature + 2));
    for (const unsigned char c : jpegli::kICCSignature) {
      jpegli_write_m_byte(cinfo, c);
    }
    jpegli_write_m_byte(cinfo, current_marker + 1);
    jpegli_write_m_byte(cinfo, num_markers);
    for (size_t i = 0; i < length; ++i) {
      jpegli_write_m_byte(cinfo, icc_data_ptr[begin]);
      ++begin;
    }
  }
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
  if (cinfo->num_components != 3) {
    // TODO(szabadka) Remove this restriction.
    JPEGLI_ERROR("Only RGB input is supported.");
  }
  if (num_lines + cinfo->next_scanline > cinfo->image_height) {
    num_lines = cinfo->image_height - cinfo->next_scanline;
  }
  bool is_little_endian =
      (m->endianness == JPEGLI_LITTLE_ENDIAN ||
       (m->endianness == JPEGLI_NATIVE_ENDIAN && IsLittleEndian()));
  static constexpr double kMul8 = 1.0 / 255.0;
  static constexpr double kMul16 = 1.0 / 65535.0;
  for (int c = 0; c < cinfo->num_components; ++c) {
    for (size_t i = 0; i < num_lines; ++i) {
      float* row = m->input.PlaneRow(c, cinfo->next_scanline + i);
      if (m->data_type == JPEGLI_TYPE_UINT8) {
        uint8_t* p = &scanlines[i][c];
        for (size_t x = 0; x < cinfo->image_width; ++x, p += 3) {
          row[x] = p[0] * kMul8;
        }
      } else if (m->data_type == JPEGLI_TYPE_UINT16 && is_little_endian) {
        uint8_t* p = &scanlines[i][c * 2];
        for (size_t x = 0; x < cinfo->image_width; ++x, p += 6) {
          row[x] = LoadLE16(p) * kMul16;
        }
      } else if (m->data_type == JPEGLI_TYPE_UINT16 && !is_little_endian) {
        uint8_t* p = &scanlines[i][c * 2];
        for (size_t x = 0; x < cinfo->image_width; ++x, p += 6) {
          row[x] = LoadBE16(p) * kMul16;
        }
      } else if (m->data_type == JPEGLI_TYPE_FLOAT && is_little_endian) {
        uint8_t* p = &scanlines[i][c * 4];
        for (size_t x = 0; x < cinfo->image_width; ++x, p += 12) {
          row[x] = LoadLEFloat(p);
        }
      } else if (m->data_type == JPEGLI_TYPE_FLOAT && !is_little_endian) {
        uint8_t* p = &scanlines[i][c * 4];
        for (size_t x = 0; x < cinfo->image_width; ++x, p += 12) {
          row[x] = LoadBEFloat(p);
        }
      }
    }
  }
  cinfo->next_scanline += num_lines;
  return num_lines;
}

void jpegli_finish_compress(j_compress_ptr cinfo) {
  jpeg_comp_master* m = cinfo->master;
  if (cinfo->num_components != 3) {
    // TODO(szabadka) Remove this restriction.
    JPEGLI_ERROR("Only RGB input is supported.");
  }
  m->jpeg_data.components.resize(cinfo->num_components);
  jxl::ColorEncoding color_encoding;
  if (!jxl::jpeg::SetColorEncodingFromJpegData(m->jpeg_data, &color_encoding)) {
    JPEGLI_ERROR("Could not parse ICC profile.");
  }
  const bool use_xyb = m->xyb_mode;
  if (use_xyb) {
    jpegli::SetICCAppMarker(jpegli::CreateXybICCAppMarker(), &m->jpeg_data);
  }
  const jxl::Image3F& input = m->input;
  float distance = m->distance;
  std::vector<uint8_t> compressed;

  if (use_xyb) {
    // Subsample blue channel.
    cinfo->comp_info[0].h_samp_factor = cinfo->comp_info[0].v_samp_factor = 2;
    cinfo->comp_info[1].h_samp_factor = cinfo->comp_info[1].v_samp_factor = 2;
    cinfo->comp_info[2].h_samp_factor = cinfo->comp_info[2].v_samp_factor = 1;
  }
  cinfo->max_h_samp_factor = cinfo->max_v_samp_factor = 1;
  for (int c = 0; c < cinfo->num_components; ++c) {
    jpeg_component_info* comp = &cinfo->comp_info[c];
    cinfo->max_h_samp_factor =
        std::max(comp->h_samp_factor, cinfo->max_h_samp_factor);
    cinfo->max_v_samp_factor =
        std::max(comp->v_samp_factor, cinfo->max_v_samp_factor);
  }
  int max_shift = 0;
  for (int c = 0; c < cinfo->num_components; ++c) {
    jpeg_component_info* comp = &cinfo->comp_info[c];
    if (comp->h_samp_factor != comp->v_samp_factor) {
      // TODO(szabadka) Remove this restriction.
      JPEGLI_ERROR(
          "Horizontal- or vertical-only subsampling is not "
          "supported.");
    }
    if (cinfo->max_h_samp_factor % comp->h_samp_factor != 0) {
      JPEGLI_ERROR("Non-integral sampling ratios are not supported.");
    }
    int factor = cinfo->max_h_samp_factor / comp->h_samp_factor;
    bool valid_factor = false;
    int shift = 0;
    for (; shift < 4; ++shift) {
      if (factor == (1 << shift)) {
        valid_factor = true;
        break;
      }
    }
    if (!valid_factor) {
      JPEGLI_ERROR("Invalid sampling factor %d", factor);
    }
    max_shift = std::max(shift, max_shift);
  }

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
  size_t max_samp_factor = 1u << max_shift;
  for (size_t c = 0; c < 3; ++c) {
    const size_t factor =
        (cinfo->max_h_samp_factor / cinfo->comp_info[c].h_samp_factor);
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
  jpegli::ComputeDCTCoefficients(opsin, use_xyb, qf, qm,
                                 &m->jpeg_data.components);

  // DHT (the actual Huffman codes will be added later).
  m->jpeg_data.marker_order.emplace_back(0xc4);

  // SOS
  std::vector<jpegli::ProgressiveScan> progressive_mode = {
      // DC
      {0, 0, 0, 0, max_shift > 0}, {1, 2, 0, 0, false},  {3, 63, 0, 2, false},
      {3, 63, 2, 1, false},        {3, 63, 1, 0, false},
  };
  if (cinfo->scan_info == nullptr) {
    jpegli::AddJpegScanInfos(progressive_mode, &m->jpeg_data.scan_info);
  } else {
    jpegli::CopyJpegScanInfos(cinfo, &m->jpeg_data.scan_info);
  }
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

void jpegli_set_input_format(j_compress_ptr cinfo, JpegliDataType data_type,
                             JpegliEndianness endianness) {
  cinfo->master->data_type = data_type;
  cinfo->master->endianness = endianness;
}
