// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jpegli/decode.h"

#include <string.h>

#include <vector>

#include "lib/jpegli/decode_internal.h"
#include "lib/jpegli/decode_marker.h"
#include "lib/jpegli/decode_scan.h"
#include "lib/jpegli/error.h"
#include "lib/jpegli/memory_manager.h"
#include "lib/jpegli/render.h"
#include "lib/jpegli/source_manager.h"
#include "lib/jxl/base/status.h"

namespace jpegli {

void InitializeImage(j_decompress_ptr cinfo) {
  cinfo->jpeg_color_space = JCS_UNKNOWN;
  cinfo->restart_interval = 0;
  cinfo->saw_JFIF_marker = FALSE;
  cinfo->JFIF_major_version = 1;
  cinfo->JFIF_minor_version = 1;
  cinfo->density_unit = 0;
  cinfo->X_density = 1;
  cinfo->Y_density = 1;
  cinfo->saw_Adobe_marker = FALSE;
  cinfo->Adobe_transform = 0;
  for (int i = 0; i < NUM_QUANT_TBLS; ++i) {
    cinfo->quant_tbl_ptrs[i] = nullptr;
  }
  for (int i = 0; i < NUM_HUFF_TBLS; ++i) {
    cinfo->dc_huff_tbl_ptrs[i] = nullptr;
    cinfo->ac_huff_tbl_ptrs[i] = nullptr;
  }
}

int ConsumeInput(j_decompress_ptr cinfo) {
  jpeg_source_mgr* src = cinfo->src;
  std::vector<uint8_t> buffer;
  const uint8_t* last_input_byte = src->next_input_byte + src->bytes_in_buffer;
  int status;
  for (;;) {
    if (cinfo->global_state == kProcessScan) {
      status = ProcessScan(cinfo);
    } else {
      status = ProcessMarkers(cinfo);
    }
    if (status != JPEG_SUSPENDED) {
      break;
    }
    if (buffer.size() != src->bytes_in_buffer) {
      // Save the unprocessed bytes in the input to a temporary buffer.
      buffer.assign(src->next_input_byte,
                    src->next_input_byte + src->bytes_in_buffer);
    }
    if (!(*cinfo->src->fill_input_buffer)(cinfo)) {
      return status;
    }
    // Save the end of the current input so that we can restore it after the
    // input processing succeeds.
    last_input_byte = cinfo->src->next_input_byte + src->bytes_in_buffer;
    // Extend the temporary buffer with the new bytes and point the input to it.
    buffer.insert(buffer.end(), src->next_input_byte, last_input_byte);
    src->next_input_byte = buffer.data();
    src->bytes_in_buffer = buffer.size();
  }
  // Restore the input pointer in case we had to change it to a temporary
  // buffer earlier.
  src->next_input_byte = last_input_byte - src->bytes_in_buffer;
  if (status == JPEG_SCAN_COMPLETED) {
    cinfo->global_state = kProcessMarkers;
  } else if (status == JPEG_REACHED_SOS) {
    cinfo->global_state =
        cinfo->global_state == kInHeader ? kHeaderDone : kProcessScan;
  }
  return status;
}

bool IsInputReady(j_decompress_ptr cinfo) {
  if (cinfo->master->found_eoi_) {
    return true;
  }
  if (cinfo->input_scan_number > cinfo->output_scan_number) {
    return true;
  }
  if (cinfo->input_scan_number < cinfo->output_scan_number) {
    return false;
  }
  return cinfo->input_iMCU_row > cinfo->output_iMCU_row;
}

}  // namespace jpegli

void jpegli_CreateDecompress(j_decompress_ptr cinfo, int version,
                             size_t structsize) {
  if (structsize != sizeof(*cinfo)) {
    JPEGLI_ERROR("jpeg_decompress_struct has wrong size.");
  }
  cinfo->master = new jpeg_decomp_master;
  cinfo->mem =
      reinterpret_cast<struct jpeg_memory_mgr*>(new jpegli::MemoryManager);
  cinfo->is_decompressor = TRUE;
  cinfo->src = nullptr;
  cinfo->marker_list = nullptr;
  cinfo->input_scan_number = 0;
  cinfo->quantize_colors = FALSE;
  cinfo->desired_number_of_colors = 0;
  cinfo->master->output_bit_depth_ = 8;
  cinfo->global_state = jpegli::kStart;
  cinfo->buffered_image = FALSE;
  cinfo->raw_data_out = FALSE;
  cinfo->output_scanline = 0;
  cinfo->sample_range_limit = nullptr;  // not used
  cinfo->rec_outbuf_height = 1;         // output works with any buffer height
  cinfo->unread_marker = 0;             // not used
  // TODO(szabadka) Fill this in for progressive mode.
  cinfo->coef_bits = nullptr;
  for (int i = 0; i < 16; ++i) {
    cinfo->master->app_marker_parsers[i] = nullptr;
  }
  cinfo->master->com_marker_parser = nullptr;
}

void jpegli_destroy_decompress(j_decompress_ptr cinfo) {
  jpegli_destroy(reinterpret_cast<j_common_ptr>(cinfo));
}

void jpegli_abort_decompress(j_decompress_ptr cinfo) {
  jpegli_abort(reinterpret_cast<j_common_ptr>(cinfo));
}

void jpegli_save_markers(j_decompress_ptr cinfo, int marker_code,
                         unsigned int length_limit) {
  jpeg_decomp_master* m = cinfo->master;
  m->markers_to_save_.insert(marker_code);
}

void jpegli_set_marker_processor(j_decompress_ptr cinfo, int marker_code,
                                 jpeg_marker_parser_method routine) {
  jpeg_decomp_master* m = cinfo->master;
  if (marker_code == 0xfe) {
    m->com_marker_parser = routine;
  } else if (marker_code >= 0xe0 && marker_code <= 0xef) {
    m->app_marker_parsers[marker_code - 0xe0] = routine;
  } else {
    JPEGLI_ERROR("jpegli_set_marker_processor: invalid marker code %d",
                 marker_code);
  }
}

int jpegli_consume_input(j_decompress_ptr cinfo) {
  if (cinfo->global_state == jpegli::kStart) {
    (*cinfo->src->init_source)(cinfo);
    jpegli::InitializeImage(cinfo);
    cinfo->global_state = jpegli::kInHeader;
  }
  if (cinfo->global_state == jpegli::kHeaderDone) {
    return JPEG_REACHED_SOS;
  }
  if (cinfo->master->found_eoi_) {
    return JPEG_REACHED_EOI;
  }
  if (cinfo->global_state == jpegli::kInHeader ||
      cinfo->global_state == jpegli::kProcessMarkers ||
      cinfo->global_state == jpegli::kProcessScan) {
    return jpegli::ConsumeInput(cinfo);
  }
  JPEGLI_ERROR("Unexpected state %d", cinfo->global_state);
  return JPEG_REACHED_EOI;  // return value does not matter
}

int jpegli_read_header(j_decompress_ptr cinfo, boolean require_image) {
  if (cinfo->global_state != jpegli::kStart &&
      cinfo->global_state != jpegli::kInHeader) {
    JPEGLI_ERROR("jpegli_read_header: unexpected state %d",
                 cinfo->global_state);
  }
  for (;;) {
    int retcode = jpegli_consume_input(cinfo);
    if (retcode == JPEG_SUSPENDED) {
      return retcode;
    } else if (retcode == JPEG_REACHED_SOS) {
      break;
    } else if (retcode == JPEG_REACHED_EOI) {
      JPEGLI_ERROR("jpegli_read_header: unexpected EOI marker.");
    }
  };
  return JPEG_HEADER_OK;
}

boolean jpegli_read_icc_profile(j_decompress_ptr cinfo, JOCTET** icc_data_ptr,
                                unsigned int* icc_data_len) {
  if (cinfo->global_state == jpegli::kStart ||
      cinfo->global_state == jpegli::kInHeader) {
    JPEGLI_ERROR("jpegli_read_icc_profile: unexpected state %d",
                 cinfo->global_state);
  }
  if (icc_data_ptr == nullptr || icc_data_len == nullptr) {
    JPEGLI_ERROR("jpegli_read_icc_profile: invalid output buffer");
  }
  jpeg_decomp_master* m = cinfo->master;
  if (m->icc_profile_.empty()) {
    *icc_data_ptr = nullptr;
    *icc_data_len = 0;
    return FALSE;
  }
  *icc_data_len = m->icc_profile_.size();
  *icc_data_ptr = (JOCTET*)malloc(*icc_data_len);
  if (*icc_data_ptr == nullptr) {
    JPEGLI_ERROR("jpegli_read_icc_profile: Out of memory");
  }
  memcpy(*icc_data_ptr, m->icc_profile_.data(), *icc_data_len);
  return TRUE;
}

void jpegli_calc_output_dimensions(j_decompress_ptr cinfo) {
  jpeg_decomp_master* m = cinfo->master;
  if (!m->found_sof_) {
    JPEGLI_ERROR("No SOF marker found.");
  }
  // Resampling is not yet implemented.
  cinfo->output_width = cinfo->image_width;
  cinfo->output_height = cinfo->image_height;
  cinfo->output_components = cinfo->out_color_components;
  cinfo->rec_outbuf_height = 1;
  if (!cinfo->quantize_colors) {
    for (size_t depth = 1; depth <= 16; ++depth) {
      if (cinfo->desired_number_of_colors == (1 << depth)) {
        m->output_bit_depth_ = depth;
      }
    }
    // Signal to the application code that we did set the output bit depth.
    cinfo->desired_number_of_colors = 0;
  }
}

boolean jpegli_has_multiple_scans(j_decompress_ptr cinfo) {
  if (cinfo->input_scan_number == 0) {
    JPEGLI_ERROR("No SOS marker found.");
  }
  return cinfo->master->is_multiscan_;
}

boolean jpegli_input_complete(j_decompress_ptr cinfo) {
  return cinfo->master->found_eoi_;
}

boolean jpegli_start_decompress(j_decompress_ptr cinfo) {
  if (cinfo->global_state == jpegli::kHeaderDone) {
    jpegli_calc_output_dimensions(cinfo);
    cinfo->global_state = jpegli::kProcessScan;
    if (cinfo->buffered_image == TRUE) {
      cinfo->output_scan_number = 0;
      return TRUE;
    }
  } else if (!cinfo->master->is_multiscan_) {
    JPEGLI_ERROR("jpegli_start_decompress: unexpected state %d",
                 cinfo->global_state);
  }
  if (cinfo->master->is_multiscan_) {
    if (cinfo->global_state != jpegli::kProcessScan &&
        cinfo->global_state != jpegli::kProcessMarkers) {
      JPEGLI_ERROR("jpegli_start_decompress: unexpected state %d",
                   cinfo->global_state);
    }
    while (!cinfo->master->found_eoi_) {
      int retcode = jpegli::ConsumeInput(cinfo);
      if (retcode == JPEG_SUSPENDED) {
        return FALSE;
      }
    }
  }
  cinfo->output_scan_number = cinfo->input_scan_number;
  jpegli::PrepareForOutput(cinfo);
  return TRUE;
}

boolean jpegli_start_output(j_decompress_ptr cinfo, int scan_number) {
  if (!cinfo->buffered_image) {
    JPEGLI_ERROR("jpegli_start_output: buffered image mode was not set");
  }
  if (cinfo->global_state != jpegli::kProcessScan) {
    JPEGLI_ERROR("jpegli_start_output: unexpected state %d",
                 cinfo->global_state);
  }
  cinfo->output_scan_number = std::max(1, scan_number);
  if (cinfo->master->found_eoi_) {
    cinfo->output_scan_number =
        std::min(cinfo->output_scan_number, cinfo->input_scan_number);
  }
  // TODO(szabadka): Figure out how much we can reuse.
  jpegli::PrepareForOutput(cinfo);
  return TRUE;
}

boolean jpegli_finish_output(j_decompress_ptr cinfo) {
  if (!cinfo->buffered_image) {
    JPEGLI_ERROR("jpegli_finish_output: buffered image mode was not set");
  }
  if (cinfo->global_state != jpegli::kProcessScan &&
      cinfo->global_state != jpegli::kProcessMarkers) {
    JPEGLI_ERROR("jpegli_finish_output: unexpected state %d",
                 cinfo->global_state);
  }
  // Advance input to the start of the next scan, or to the end of input.
  while (cinfo->input_scan_number <= cinfo->output_scan_number &&
         !cinfo->master->found_eoi_) {
    if (jpegli::ConsumeInput(cinfo) == JPEG_SUSPENDED) {
      return FALSE;
    }
  }
  return TRUE;
}

JDIMENSION jpegli_read_scanlines(j_decompress_ptr cinfo, JSAMPARRAY scanlines,
                                 JDIMENSION max_lines) {
  jpeg_decomp_master* m = cinfo->master;
  if (cinfo->global_state != jpegli::kProcessScan &&
      cinfo->global_state != jpegli::kProcessMarkers) {
    JPEGLI_ERROR("jpegli_read_scanlines: unexpected state %d",
                 cinfo->global_state);
  }
  if (cinfo->buffered_image) {
    if (cinfo->output_scan_number == 0) {
      JPEGLI_ERROR(
          "jpegli_read_scanlines: "
          "jpegli_start_output() was not called");
    }
  } else if (m->is_multiscan_ && !m->found_eoi_) {
    JPEGLI_ERROR(
        "jpegli_read_scanlines: "
        "jpegli_start_decompress() did not finish");
  }
  if (cinfo->output_scanline + max_lines > cinfo->output_height) {
    max_lines = cinfo->output_height - cinfo->output_scanline;
  }
  size_t num_output_rows = 0;
  while (num_output_rows < max_lines) {
    if (jpegli::IsInputReady(cinfo)) {
      jpegli::ProcessOutput(cinfo, &num_output_rows, scanlines, max_lines);
    } else if (jpegli::ConsumeInput(cinfo) == JPEG_SUSPENDED) {
      break;
    }
  }
  return num_output_rows;
}

JDIMENSION jpegli_skip_scanlines(j_decompress_ptr cinfo, JDIMENSION num_lines) {
  // TODO(szabadka) Skip the IDCT for skipped over blocks.
  return jpegli_read_scanlines(cinfo, nullptr, num_lines);
}

void jpegli_crop_scanline(j_decompress_ptr cinfo, JDIMENSION* xoffset,
                          JDIMENSION* width) {
  if ((cinfo->global_state != jpegli::kProcessScan &&
       cinfo->global_state != jpegli::kProcessMarkers) ||
      cinfo->output_scanline != 0) {
    JPEGLI_ERROR("jpegli_crop_decompress: unexpected state %d",
                 cinfo->global_state);
  }
  if (xoffset == nullptr || width == nullptr || *width == 0 ||
      *xoffset + *width > cinfo->output_width) {
    JPEGLI_ERROR("jpegli_crop_scanline: Invalid arguments");
  }
  // TODO(szabadka) Skip the IDCT for skipped over blocks.
  size_t xend = *xoffset + *width;
  *xoffset = (*xoffset / DCTSIZE) * DCTSIZE;
  *width = xend - *xoffset;
  cinfo->master->xoffset_ = *xoffset;
  cinfo->output_width = *width;
}

JDIMENSION jpegli_read_raw_data(j_decompress_ptr cinfo, JSAMPIMAGE data,
                                JDIMENSION max_lines) {
  if ((cinfo->global_state != jpegli::kProcessScan &&
       cinfo->global_state != jpegli::kProcessMarkers) ||
      !cinfo->raw_data_out) {
    JPEGLI_ERROR("jpegli_read_raw_data: unexpected state %d",
                 cinfo->global_state);
  }
  size_t iMCU_height = cinfo->max_v_samp_factor * DCTSIZE;
  if (max_lines < iMCU_height) {
    JPEGLI_ERROR("jpegli_read_raw_data: output buffer too small");
  }
  while (!jpegli::IsInputReady(cinfo)) {
    if (jpegli::ConsumeInput(cinfo) == JPEG_SUSPENDED) {
      return 0;
    }
  }
  if (cinfo->output_iMCU_row < cinfo->total_iMCU_rows) {
    jpegli::ProcessRawOutput(cinfo, data);
    return iMCU_height;
  }
  return 0;
}

boolean jpegli_finish_decompress(j_decompress_ptr cinfo) {
  if (cinfo->global_state != jpegli::kProcessScan &&
      cinfo->global_state != jpegli::kProcessMarkers) {
    JPEGLI_ERROR("jpegli_finish_decompress: unexpected state %d",
                 cinfo->global_state);
  }
  while (!cinfo->master->found_eoi_) {
    int retcode = jpegli::ConsumeInput(cinfo);
    if (retcode == JPEG_SUSPENDED) {
      return FALSE;
    }
  }
  jpegli_abort_decompress(cinfo);
  return TRUE;
}

boolean jpegli_resync_to_restart(j_decompress_ptr cinfo, int desired) {
  // The default resync_to_restart will just throw an error.
  JPEGLI_ERROR("Invalid restart marker found.");
  return TRUE;
}
