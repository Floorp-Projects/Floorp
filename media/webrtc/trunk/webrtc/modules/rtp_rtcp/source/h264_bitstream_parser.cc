/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "webrtc/modules/rtp_rtcp/source/h264_bitstream_parser.h"

#include <vector>

#include "webrtc/base/bitbuffer.h"
#include "webrtc/base/bytebuffer.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/scoped_ptr.h"

namespace webrtc {
namespace {
// The size of a NALU header {0 0 0 1}.
static const size_t kNaluHeaderSize = 4;

// The size of a NALU header plus the type byte.
static const size_t kNaluHeaderAndTypeSize = kNaluHeaderSize + 1;

// The NALU type.
static const uint8_t kNaluSps = 0x7;
static const uint8_t kNaluPps = 0x8;
static const uint8_t kNaluIdr = 0x5;
static const uint8_t kNaluTypeMask = 0x1F;

static const uint8_t kSliceTypeP = 0x0;
static const uint8_t kSliceTypeB = 0x1;
static const uint8_t kSliceTypeSp = 0x3;

// Returns a vector of the NALU start sequences (0 0 0 1) in the given buffer.
std::vector<size_t> FindNaluStartSequences(const uint8_t* buffer,
                                           size_t buffer_size) {
  std::vector<size_t> sequences;
  // This is sorta like Boyer-Moore, but with only the first optimization step:
  // given a 4-byte sequence we're looking at, if the 4th byte isn't 1 or 0,
  // skip ahead to the next 4-byte sequence. 0s and 1s are relatively rare, so
  // this will skip the majority of reads/checks.
  const uint8_t* end = buffer + buffer_size - 4;
  for (const uint8_t* head = buffer; head < end;) {
    if (head[3] > 1) {
      head += 4;
    } else if (head[3] == 1 && head[2] == 0 && head[1] == 0 && head[0] == 0) {
      sequences.push_back(static_cast<size_t>(head - buffer));
      head += 4;
    } else {
      head++;
    }
  }

  return sequences;
}
}  // namespace

// Parses RBSP from source bytes. Removes emulation bytes, but leaves the
// rbsp_trailing_bits() in the stream, since none of the parsing reads all the
// way to the end of a parsed RBSP sequence. When writing, that means the
// rbsp_trailing_bits() should be preserved and don't need to be restored (i.e.
// the rbsp_stop_one_bit, which is just a 1, then zero padded), and alignment
// should "just work".
// TODO(pbos): Make parsing RBSP something that can be integrated into BitBuffer
// so we don't have to copy the entire frames when only interested in the
// headers.
rtc::ByteBuffer* ParseRbsp(const uint8_t* bytes, size_t length) {
  // Copied from webrtc::H264SpsParser::Parse.
  rtc::ByteBuffer* rbsp_buffer = new rtc::ByteBuffer;
  for (size_t i = 0; i < length;) {
    if (length - i >= 3 && bytes[i] == 0 && bytes[i + 1] == 0 &&
        bytes[i + 2] == 3) {
      rbsp_buffer->WriteBytes(reinterpret_cast<const char*>(bytes) + i, 2);
      i += 3;
    } else {
      rbsp_buffer->WriteBytes(reinterpret_cast<const char*>(bytes) + i, 1);
      i++;
    }
  }
  return rbsp_buffer;
}

#define RETURN_FALSE_ON_FAIL(x)       \
  if (!(x)) {                         \
    LOG_F(LS_ERROR) << "FAILED: " #x; \
    return false;                     \
  }

H264BitstreamParser::PpsState::PpsState() {}

H264BitstreamParser::SpsState::SpsState() {}

// These functions are similar to webrtc::H264SpsParser::Parse, and based on the
// same version of the H.264 standard. You can find it here:
// http://www.itu.int/rec/T-REC-H.264
bool H264BitstreamParser::ParseSpsNalu(const uint8_t* sps, size_t length) {
  // Reset SPS state.
  sps_ = SpsState();
  sps_parsed_ = false;
  // Parse out the SPS RBSP. It should be small, so it's ok that we create a
  // copy. We'll eventually write this back.
  rtc::scoped_ptr<rtc::ByteBuffer> sps_rbsp(
      ParseRbsp(sps + kNaluHeaderAndTypeSize, length - kNaluHeaderAndTypeSize));
  rtc::BitBuffer sps_parser(reinterpret_cast<const uint8_t*>(sps_rbsp->Data()),
                            sps_rbsp->Length());

  uint8_t byte_tmp;
  uint32_t golomb_tmp;
  uint32_t bits_tmp;

  // profile_idc: u(8).
  uint8_t profile_idc;
  RETURN_FALSE_ON_FAIL(sps_parser.ReadUInt8(&profile_idc));
  // constraint_set0_flag through constraint_set5_flag + reserved_zero_2bits
  // 1 bit each for the flags + 2 bits = 8 bits = 1 byte.
  RETURN_FALSE_ON_FAIL(sps_parser.ReadUInt8(&byte_tmp));
  // level_idc: u(8)
  RETURN_FALSE_ON_FAIL(sps_parser.ReadUInt8(&byte_tmp));
  // seq_parameter_set_id: ue(v)
  RETURN_FALSE_ON_FAIL(sps_parser.ReadExponentialGolomb(&golomb_tmp));
  sps_.separate_colour_plane_flag = 0;
  // See if profile_idc has chroma format information.
  if (profile_idc == 100 || profile_idc == 110 || profile_idc == 122 ||
      profile_idc == 244 || profile_idc == 44 || profile_idc == 83 ||
      profile_idc == 86 || profile_idc == 118 || profile_idc == 128 ||
      profile_idc == 138 || profile_idc == 139 || profile_idc == 134) {
    // chroma_format_idc: ue(v)
    uint32_t chroma_format_idc;
    RETURN_FALSE_ON_FAIL(sps_parser.ReadExponentialGolomb(&chroma_format_idc));
    if (chroma_format_idc == 3) {
      // separate_colour_plane_flag: u(1)
      RETURN_FALSE_ON_FAIL(
          sps_parser.ReadBits(&sps_.separate_colour_plane_flag, 1));
    }
    // bit_depth_luma_minus8: ue(v)
    RETURN_FALSE_ON_FAIL(sps_parser.ReadExponentialGolomb(&golomb_tmp));
    // bit_depth_chroma_minus8: ue(v)
    RETURN_FALSE_ON_FAIL(sps_parser.ReadExponentialGolomb(&golomb_tmp));
    // qpprime_y_zero_transform_bypass_flag: u(1)
    RETURN_FALSE_ON_FAIL(sps_parser.ReadBits(&bits_tmp, 1));
    // seq_scaling_matrix_present_flag: u(1)
    uint32_t seq_scaling_matrix_present_flag;
    RETURN_FALSE_ON_FAIL(
        sps_parser.ReadBits(&seq_scaling_matrix_present_flag, 1));
    if (seq_scaling_matrix_present_flag) {
      // seq_scaling_list_present_flags. Either 8 or 12, depending on
      // chroma_format_idc.
      uint32_t seq_scaling_list_present_flags;
      if (chroma_format_idc != 3) {
        RETURN_FALSE_ON_FAIL(
            sps_parser.ReadBits(&seq_scaling_list_present_flags, 8));
      } else {
        RETURN_FALSE_ON_FAIL(
            sps_parser.ReadBits(&seq_scaling_list_present_flags, 12));
      }
      // TODO(pbos): Support parsing scaling lists if they're seen in practice.
      RTC_CHECK(seq_scaling_list_present_flags == 0)
          << "SPS contains scaling lists, which are unsupported.";
    }
  }
  // log2_max_frame_num_minus4: ue(v)
  RETURN_FALSE_ON_FAIL(
      sps_parser.ReadExponentialGolomb(&sps_.log2_max_frame_num_minus4));
  // pic_order_cnt_type: ue(v)
  RETURN_FALSE_ON_FAIL(
      sps_parser.ReadExponentialGolomb(&sps_.pic_order_cnt_type));

  if (sps_.pic_order_cnt_type == 0) {
    // log2_max_pic_order_cnt_lsb_minus4: ue(v)
    RETURN_FALSE_ON_FAIL(sps_parser.ReadExponentialGolomb(
        &sps_.log2_max_pic_order_cnt_lsb_minus4));
  } else if (sps_.pic_order_cnt_type == 1) {
    // delta_pic_order_always_zero_flag: u(1)
    RETURN_FALSE_ON_FAIL(
        sps_parser.ReadBits(&sps_.delta_pic_order_always_zero_flag, 1));
    // offset_for_non_ref_pic: se(v)
    RETURN_FALSE_ON_FAIL(sps_parser.ReadExponentialGolomb(&golomb_tmp));
    // offset_for_top_to_bottom_field: se(v)
    RETURN_FALSE_ON_FAIL(sps_parser.ReadExponentialGolomb(&golomb_tmp));
    uint32_t num_ref_frames_in_pic_order_cnt_cycle;
    // num_ref_frames_in_pic_order_cnt_cycle: ue(v)
    RETURN_FALSE_ON_FAIL(sps_parser.ReadExponentialGolomb(
        &num_ref_frames_in_pic_order_cnt_cycle));
    for (uint32_t i = 0; i < num_ref_frames_in_pic_order_cnt_cycle; i++) {
      // offset_for_ref_frame[i]: se(v)
      RETURN_FALSE_ON_FAIL(sps_parser.ReadExponentialGolomb(&golomb_tmp));
    }
  }
  // max_num_ref_frames: ue(v)
  RETURN_FALSE_ON_FAIL(sps_parser.ReadExponentialGolomb(&golomb_tmp));
  // gaps_in_frame_num_value_allowed_flag: u(1)
  RETURN_FALSE_ON_FAIL(sps_parser.ReadBits(&bits_tmp, 1));
  // pic_width_in_mbs_minus1: ue(v)
  RETURN_FALSE_ON_FAIL(sps_parser.ReadExponentialGolomb(&golomb_tmp));
  // pic_height_in_map_units_minus1: ue(v)
  RETURN_FALSE_ON_FAIL(sps_parser.ReadExponentialGolomb(&golomb_tmp));
  // frame_mbs_only_flag: u(1)
  RETURN_FALSE_ON_FAIL(sps_parser.ReadBits(&sps_.frame_mbs_only_flag, 1));
  sps_parsed_ = true;
  return true;
}

bool H264BitstreamParser::ParsePpsNalu(const uint8_t* pps, size_t length) {
  RTC_CHECK(sps_parsed_);
  // We're starting a new stream, so reset picture type rewriting values.
  pps_ = PpsState();
  pps_parsed_ = false;
  rtc::scoped_ptr<rtc::ByteBuffer> buffer(
      ParseRbsp(pps + kNaluHeaderAndTypeSize, length - kNaluHeaderAndTypeSize));
  rtc::BitBuffer parser(reinterpret_cast<const uint8_t*>(buffer->Data()),
                        buffer->Length());

  uint32_t bits_tmp;
  uint32_t golomb_ignored;
  // pic_parameter_set_id: ue(v)
  RETURN_FALSE_ON_FAIL(parser.ReadExponentialGolomb(&golomb_ignored));
  // seq_parameter_set_id: ue(v)
  RETURN_FALSE_ON_FAIL(parser.ReadExponentialGolomb(&golomb_ignored));
  // entropy_coding_mode_flag: u(1)
  uint32_t entropy_coding_mode_flag;
  RETURN_FALSE_ON_FAIL(parser.ReadBits(&entropy_coding_mode_flag, 1));
  // TODO(pbos): Implement CABAC support if spotted in the wild.
  RTC_CHECK(entropy_coding_mode_flag == 0)
      << "Don't know how to parse CABAC streams.";
  // bottom_field_pic_order_in_frame_present_flag: u(1)
  uint32_t bottom_field_pic_order_in_frame_present_flag;
  RETURN_FALSE_ON_FAIL(
      parser.ReadBits(&bottom_field_pic_order_in_frame_present_flag, 1));
  pps_.bottom_field_pic_order_in_frame_present_flag =
      bottom_field_pic_order_in_frame_present_flag != 0;

  // num_slice_groups_minus1: ue(v)
  uint32_t num_slice_groups_minus1;
  RETURN_FALSE_ON_FAIL(parser.ReadExponentialGolomb(&num_slice_groups_minus1));
  if (num_slice_groups_minus1 > 0) {
    uint32_t slice_group_map_type;
    // slice_group_map_type: ue(v)
    RETURN_FALSE_ON_FAIL(parser.ReadExponentialGolomb(&slice_group_map_type));
    if (slice_group_map_type == 0) {
      for (uint32_t i_group = 0; i_group <= num_slice_groups_minus1;
           ++i_group) {
        // run_length_minus1[iGroup]: ue(v)
        RETURN_FALSE_ON_FAIL(parser.ReadExponentialGolomb(&golomb_ignored));
      }
    } else if (slice_group_map_type == 2) {
      for (uint32_t i_group = 0; i_group <= num_slice_groups_minus1;
           ++i_group) {
        // top_left[iGroup]: ue(v)
        RETURN_FALSE_ON_FAIL(parser.ReadExponentialGolomb(&golomb_ignored));
        // bottom_right[iGroup]: ue(v)
        RETURN_FALSE_ON_FAIL(parser.ReadExponentialGolomb(&golomb_ignored));
      }
    } else if (slice_group_map_type == 3 || slice_group_map_type == 4 ||
               slice_group_map_type == 5) {
      // slice_group_change_direction_flag: u(1)
      RETURN_FALSE_ON_FAIL(parser.ReadBits(&bits_tmp, 1));
      // slice_group_change_rate_minus1: ue(v)
      RETURN_FALSE_ON_FAIL(parser.ReadExponentialGolomb(&golomb_ignored));
    } else if (slice_group_map_type == 6) {
      // pic_size_in_map_units_minus1: ue(v)
      uint32_t pic_size_in_map_units_minus1;
      RETURN_FALSE_ON_FAIL(
          parser.ReadExponentialGolomb(&pic_size_in_map_units_minus1));
      uint32_t slice_group_id_bits = 0;
      uint32_t num_slice_groups = num_slice_groups_minus1 + 1;
      // If num_slice_groups is not a power of two an additional bit is required
      // to account for the ceil() of log2() below.
      if ((num_slice_groups & (num_slice_groups - 1)) != 0)
        ++slice_group_id_bits;
      while (num_slice_groups > 0) {
        num_slice_groups >>= 1;
        ++slice_group_id_bits;
      }
      for (uint32_t i = 0; i <= pic_size_in_map_units_minus1; i++) {
        // slice_group_id[i]: u(v)
        // Represented by ceil(log2(num_slice_groups_minus1 + 1)) bits.
        RETURN_FALSE_ON_FAIL(parser.ReadBits(&bits_tmp, slice_group_id_bits));
      }
    }
  }
  // num_ref_idx_l0_default_active_minus1: ue(v)
  RETURN_FALSE_ON_FAIL(parser.ReadExponentialGolomb(&golomb_ignored));
  // num_ref_idx_l1_default_active_minus1: ue(v)
  RETURN_FALSE_ON_FAIL(parser.ReadExponentialGolomb(&golomb_ignored));
  // weighted_pred_flag: u(1)
  uint32_t weighted_pred_flag;
  RETURN_FALSE_ON_FAIL(parser.ReadBits(&weighted_pred_flag, 1));
  pps_.weighted_pred_flag = weighted_pred_flag != 0;
  // weighted_bipred_idc: u(2)
  RETURN_FALSE_ON_FAIL(parser.ReadBits(&pps_.weighted_bipred_idc, 2));

  // pic_init_qp_minus26: se(v)
  RETURN_FALSE_ON_FAIL(
      parser.ReadSignedExponentialGolomb(&pps_.pic_init_qp_minus26));
  // pic_init_qs_minus26: se(v)
  RETURN_FALSE_ON_FAIL(parser.ReadExponentialGolomb(&golomb_ignored));
  // chroma_qp_index_offset: se(v)
  RETURN_FALSE_ON_FAIL(parser.ReadExponentialGolomb(&golomb_ignored));
  // deblocking_filter_control_present_flag: u(1)
  // constrained_intra_pred_flag: u(1)
  RETURN_FALSE_ON_FAIL(parser.ReadBits(&bits_tmp, 2));
  // redundant_pic_cnt_present_flag: u(1)
  RETURN_FALSE_ON_FAIL(
      parser.ReadBits(&pps_.redundant_pic_cnt_present_flag, 1));

  pps_parsed_ = true;
  return true;
}

bool H264BitstreamParser::ParseNonParameterSetNalu(const uint8_t* source,
                                                   size_t source_length,
                                                   uint8_t nalu_type) {
  RTC_CHECK(sps_parsed_);
  RTC_CHECK(pps_parsed_);
  last_slice_qp_delta_parsed_ = false;
  rtc::scoped_ptr<rtc::ByteBuffer> slice_rbsp(ParseRbsp(
      source + kNaluHeaderAndTypeSize, source_length - kNaluHeaderAndTypeSize));
  rtc::BitBuffer slice_reader(
      reinterpret_cast<const uint8_t*>(slice_rbsp->Data()),
      slice_rbsp->Length());
  // Check to see if this is an IDR slice, which has an extra field to parse
  // out.
  bool is_idr = (source[kNaluHeaderSize] & 0x0F) == kNaluIdr;
  uint8_t nal_ref_idc = (source[kNaluHeaderSize] & 0x60) >> 5;
  uint32_t golomb_tmp;
  uint32_t bits_tmp;

  // first_mb_in_slice: ue(v)
  RETURN_FALSE_ON_FAIL(slice_reader.ReadExponentialGolomb(&golomb_tmp));
  // slice_type: ue(v)
  uint32_t slice_type;
  RETURN_FALSE_ON_FAIL(slice_reader.ReadExponentialGolomb(&slice_type));
  // slice_type's 5..9 range is used to indicate that all slices of a picture
  // have the same value of slice_type % 5, we don't care about that, so we map
  // to the corresponding 0..4 range.
  slice_type %= 5;
  // pic_parameter_set_id: ue(v)
  RETURN_FALSE_ON_FAIL(slice_reader.ReadExponentialGolomb(&golomb_tmp));
  if (sps_.separate_colour_plane_flag == 1) {
    // colour_plane_id
    RETURN_FALSE_ON_FAIL(slice_reader.ReadBits(&bits_tmp, 2));
  }
  // frame_num: u(v)
  // Represented by log2_max_frame_num_minus4 + 4 bits.
  RETURN_FALSE_ON_FAIL(
      slice_reader.ReadBits(&bits_tmp, sps_.log2_max_frame_num_minus4 + 4));
  uint32_t field_pic_flag = 0;
  if (sps_.frame_mbs_only_flag == 0) {
    // field_pic_flag: u(1)
    RETURN_FALSE_ON_FAIL(slice_reader.ReadBits(&field_pic_flag, 1));
    if (field_pic_flag != 0) {
      // bottom_field_flag: u(1)
      RETURN_FALSE_ON_FAIL(slice_reader.ReadBits(&bits_tmp, 1));
    }
  }
  if (is_idr) {
    // idr_pic_id: ue(v)
    RETURN_FALSE_ON_FAIL(slice_reader.ReadExponentialGolomb(&golomb_tmp));
  }
  // pic_order_cnt_lsb: u(v)
  // Represented by sps_.log2_max_pic_order_cnt_lsb_minus4 + 4 bits.
  if (sps_.pic_order_cnt_type == 0) {
    RETURN_FALSE_ON_FAIL(slice_reader.ReadBits(
        &bits_tmp, sps_.log2_max_pic_order_cnt_lsb_minus4 + 4));
    if (pps_.bottom_field_pic_order_in_frame_present_flag &&
        field_pic_flag == 0) {
      // delta_pic_order_cnt_bottom: se(v)
      RETURN_FALSE_ON_FAIL(slice_reader.ReadExponentialGolomb(&golomb_tmp));
    }
  }
  if (sps_.pic_order_cnt_type == 1 && !sps_.delta_pic_order_always_zero_flag) {
    // delta_pic_order_cnt[0]: se(v)
    RETURN_FALSE_ON_FAIL(slice_reader.ReadExponentialGolomb(&golomb_tmp));
    if (pps_.bottom_field_pic_order_in_frame_present_flag && !field_pic_flag) {
      // delta_pic_order_cnt[1]: se(v)
      RETURN_FALSE_ON_FAIL(slice_reader.ReadExponentialGolomb(&golomb_tmp));
    }
  }
  if (pps_.redundant_pic_cnt_present_flag) {
    // redundant_pic_cnt: ue(v)
    RETURN_FALSE_ON_FAIL(slice_reader.ReadExponentialGolomb(&golomb_tmp));
  }
  if (slice_type == kSliceTypeB) {
    // direct_spatial_mv_pred_flag: u(1)
    RETURN_FALSE_ON_FAIL(slice_reader.ReadBits(&bits_tmp, 1));
  }
  if (slice_type == kSliceTypeP || slice_type == kSliceTypeSp ||
      slice_type == kSliceTypeB) {
    uint32_t num_ref_idx_active_override_flag;
    // num_ref_idx_active_override_flag: u(1)
    RETURN_FALSE_ON_FAIL(
        slice_reader.ReadBits(&num_ref_idx_active_override_flag, 1));
    if (num_ref_idx_active_override_flag != 0) {
      // num_ref_idx_l0_active_minus1: ue(v)
      RETURN_FALSE_ON_FAIL(slice_reader.ReadExponentialGolomb(&golomb_tmp));
      if (slice_type == kSliceTypeB) {
        // num_ref_idx_l1_active_minus1: ue(v)
        RETURN_FALSE_ON_FAIL(slice_reader.ReadExponentialGolomb(&golomb_tmp));
      }
    }
  }
  // assume nal_unit_type != 20 && nal_unit_type != 21:
  RTC_CHECK_NE(nalu_type, 20);
  RTC_CHECK_NE(nalu_type, 21);
  // if (nal_unit_type == 20 || nal_unit_type == 21)
  //   ref_pic_list_mvc_modification()
  // else
  {
    // ref_pic_list_modification():
    // |slice_type| checks here don't use named constants as they aren't named
    // in the spec for this segment. Keeping them consistent makes it easier to
    // verify that they are both the same.
    if (slice_type % 5 != 2 && slice_type % 5 != 4) {
      // ref_pic_list_modification_flag_l0: u(1)
      uint32_t ref_pic_list_modification_flag_l0;
      RETURN_FALSE_ON_FAIL(
          slice_reader.ReadBits(&ref_pic_list_modification_flag_l0, 1));
      if (ref_pic_list_modification_flag_l0) {
        uint32_t modification_of_pic_nums_idc;
        do {
          // modification_of_pic_nums_idc: ue(v)
          RETURN_FALSE_ON_FAIL(slice_reader.ReadExponentialGolomb(
              &modification_of_pic_nums_idc));
          if (modification_of_pic_nums_idc == 0 ||
              modification_of_pic_nums_idc == 1) {
            // abs_diff_pic_num_minus1: ue(v)
            RETURN_FALSE_ON_FAIL(
                slice_reader.ReadExponentialGolomb(&golomb_tmp));
          } else if (modification_of_pic_nums_idc == 2) {
            // long_term_pic_num: ue(v)
            RETURN_FALSE_ON_FAIL(
                slice_reader.ReadExponentialGolomb(&golomb_tmp));
          }
        } while (modification_of_pic_nums_idc != 3);
      }
    }
    if (slice_type % 5 == 1) {
      // ref_pic_list_modification_flag_l1: u(1)
      uint32_t ref_pic_list_modification_flag_l1;
      RETURN_FALSE_ON_FAIL(
          slice_reader.ReadBits(&ref_pic_list_modification_flag_l1, 1));
      if (ref_pic_list_modification_flag_l1) {
        uint32_t modification_of_pic_nums_idc;
        do {
          // modification_of_pic_nums_idc: ue(v)
          RETURN_FALSE_ON_FAIL(slice_reader.ReadExponentialGolomb(
              &modification_of_pic_nums_idc));
          if (modification_of_pic_nums_idc == 0 ||
              modification_of_pic_nums_idc == 1) {
            // abs_diff_pic_num_minus1: ue(v)
            RETURN_FALSE_ON_FAIL(
                slice_reader.ReadExponentialGolomb(&golomb_tmp));
          } else if (modification_of_pic_nums_idc == 2) {
            // long_term_pic_num: ue(v)
            RETURN_FALSE_ON_FAIL(
                slice_reader.ReadExponentialGolomb(&golomb_tmp));
          }
        } while (modification_of_pic_nums_idc != 3);
      }
    }
  }
  // TODO(pbos): Do we need support for pred_weight_table()?
  RTC_CHECK(!((pps_.weighted_pred_flag &&
               (slice_type == kSliceTypeP || slice_type == kSliceTypeSp)) ||
              (pps_.weighted_bipred_idc != 0 && slice_type == kSliceTypeB)))
      << "Missing support for pred_weight_table().";
  // if ((weighted_pred_flag && (slice_type == P || slice_type == SP)) ||
  //    (weighted_bipred_idc == 1 && slice_type == B)) {
  //  pred_weight_table()
  // }
  if (nal_ref_idc != 0) {
    // dec_ref_pic_marking():
    if (is_idr) {
      // no_output_of_prior_pics_flag: u(1)
      // long_term_reference_flag: u(1)
      RETURN_FALSE_ON_FAIL(slice_reader.ReadBits(&bits_tmp, 2));
    } else {
      // adaptive_ref_pic_marking_mode_flag: u(1)
      uint32_t adaptive_ref_pic_marking_mode_flag;
      RETURN_FALSE_ON_FAIL(
          slice_reader.ReadBits(&adaptive_ref_pic_marking_mode_flag, 1));
      if (adaptive_ref_pic_marking_mode_flag) {
        uint32_t memory_management_control_operation;
        do {
          // memory_management_control_operation: ue(v)
          RETURN_FALSE_ON_FAIL(slice_reader.ReadExponentialGolomb(
              &memory_management_control_operation));
          if (memory_management_control_operation == 1 ||
              memory_management_control_operation == 3) {
            // difference_of_pic_nums_minus1: ue(v)
            RETURN_FALSE_ON_FAIL(
                slice_reader.ReadExponentialGolomb(&golomb_tmp));
          }
          if (memory_management_control_operation == 2) {
            // long_term_pic_num: ue(v)
            RETURN_FALSE_ON_FAIL(
                slice_reader.ReadExponentialGolomb(&golomb_tmp));
          }
          if (memory_management_control_operation == 3 ||
              memory_management_control_operation == 6) {
            // long_term_frame_idx: ue(v)
            RETURN_FALSE_ON_FAIL(
                slice_reader.ReadExponentialGolomb(&golomb_tmp));
          }
          if (memory_management_control_operation == 4) {
            // max_long_term_frame_idx_plus1: ue(v)
            RETURN_FALSE_ON_FAIL(
                slice_reader.ReadExponentialGolomb(&golomb_tmp));
          }
        } while (memory_management_control_operation != 0);
      }
    }
  }
  // cabac not supported: entropy_coding_mode_flag == 0 asserted above.
  // if (entropy_coding_mode_flag && slice_type != I && slice_type != SI)
  //   cabac_init_idc
  RETURN_FALSE_ON_FAIL(
      slice_reader.ReadSignedExponentialGolomb(&last_slice_qp_delta_));
  last_slice_qp_delta_parsed_ = true;
  return true;
}

void H264BitstreamParser::ParseSlice(const uint8_t* slice, size_t length) {
  uint8_t nalu_type = slice[4] & kNaluTypeMask;
  switch (nalu_type) {
    case kNaluSps:
      RTC_CHECK(ParseSpsNalu(slice, length))
          << "Failed to parse bitstream SPS.";
      break;
    case kNaluPps:
      RTC_CHECK(ParsePpsNalu(slice, length))
          << "Failed to parse bitstream PPS.";
      break;
    default:
      RTC_CHECK(ParseNonParameterSetNalu(slice, length, nalu_type))
          << "Failed to parse picture slice.";
      break;
  }
}

void H264BitstreamParser::ParseBitstream(const uint8_t* bitstream,
                                         size_t length) {
  RTC_CHECK_GE(length, 4u);
  std::vector<size_t> slice_markers = FindNaluStartSequences(bitstream, length);
  RTC_CHECK(!slice_markers.empty());
  for (size_t i = 0; i < slice_markers.size() - 1; ++i) {
    ParseSlice(bitstream + slice_markers[i],
               slice_markers[i + 1] - slice_markers[i]);
  }
  // Parse the last slice.
  ParseSlice(bitstream + slice_markers.back(), length - slice_markers.back());
}

bool H264BitstreamParser::GetLastSliceQp(int* qp) const {
  if (!last_slice_qp_delta_parsed_)
    return false;
  *qp = 26 + pps_.pic_init_qp_minus26 + last_slice_qp_delta_;
  return true;
}

}  // namespace webrtc
