/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/h264_sps_parser.h"

#include "webrtc/base/bitbuffer.h"
#include "webrtc/base/bytebuffer.h"
#include "webrtc/base/logging.h"

#define RETURN_FALSE_ON_FAIL(x) \
  if (!(x)) {                   \
    return false;               \
  }

namespace webrtc {

H264SpsParser::H264SpsParser(const uint8_t* sps, size_t byte_length)
    : sps_(sps), byte_length_(byte_length), width_(), height_() {
}

bool H264SpsParser::Parse() {
  // General note: this is based off the 02/2014 version of the H.264 standard.
  // You can find it on this page:
  // http://www.itu.int/rec/T-REC-H.264

  const char* sps_bytes = reinterpret_cast<const char*>(sps_);
  // First, parse out rbsp, which is basically the source buffer minus emulation
  // bytes (the last byte of a 0x00 0x00 0x03 sequence). RBSP is defined in
  // section 7.3.1 of the H.264 standard.
  rtc::ByteBuffer rbsp_buffer;
  for (size_t i = 0; i < byte_length_;) {
    // Be careful about over/underflow here. byte_length_ - 3 can underflow, and
    // i + 3 can overflow, but byte_length_ - i can't, because i < byte_length_
    // above, and that expression will produce the number of bytes left in
    // the stream including the byte at i.
    if (byte_length_ - i >= 3 && sps_[i] == 0 && sps_[i + 1] == 0 &&
        sps_[i + 2] == 3) {
      // Two rbsp bytes + the emulation byte.
      rbsp_buffer.WriteBytes(sps_bytes + i, 2);
      i += 3;
    } else {
      // Single rbsp byte.
      rbsp_buffer.WriteBytes(sps_bytes + i, 1);
      i++;
    }
  }

  // Now, we need to use a bit buffer to parse through the actual AVC SPS
  // format. See Section 7.3.2.1.1 ("Sequence parameter set data syntax") of the
  // H.264 standard for a complete description.
  // Since we only care about resolution, we ignore the majority of fields, but
  // we still have to actively parse through a lot of the data, since many of
  // the fields have variable size.
  // We're particularly interested in:
  // chroma_format_idc -> affects crop units
  // pic_{width,height}_* -> resolution of the frame in macroblocks (16x16).
  // frame_crop_*_offset -> crop information
  rtc::BitBuffer parser(reinterpret_cast<const uint8_t*>(rbsp_buffer.Data()),
                        rbsp_buffer.Length());

  // The golomb values we have to read, not just consume.
  uint32_t golomb_ignored;

  // separate_colour_plane_flag is optional (assumed 0), but has implications
  // about the ChromaArrayType, which modifies how we treat crop coordinates.
  uint32_t separate_colour_plane_flag = 0;
  // chroma_format_idc will be ChromaArrayType if separate_colour_plane_flag is
  // 0. It defaults to 1, when not specified.
  uint32_t chroma_format_idc = 1;

  // profile_idc: u(8). We need it to determine if we need to read/skip chroma
  // formats.
  uint8_t profile_idc;
  RETURN_FALSE_ON_FAIL(parser.ReadUInt8(&profile_idc));
  // constraint_set0_flag through constraint_set5_flag + reserved_zero_2bits
  // 1 bit each for the flags + 2 bits = 8 bits = 1 byte.
  RETURN_FALSE_ON_FAIL(parser.ConsumeBytes(1));
  // level_idc: u(8)
  RETURN_FALSE_ON_FAIL(parser.ConsumeBytes(1));
  // seq_parameter_set_id: ue(v)
  RETURN_FALSE_ON_FAIL(parser.ReadExponentialGolomb(&golomb_ignored));
  // See if profile_idc has chroma format information.
  if (profile_idc == 100 || profile_idc == 110 || profile_idc == 122 ||
      profile_idc == 244 || profile_idc == 44 || profile_idc == 83 ||
      profile_idc == 86 || profile_idc == 118 || profile_idc == 128 ||
      profile_idc == 138 || profile_idc == 139 || profile_idc == 134) {
    // chroma_format_idc: ue(v)
    RETURN_FALSE_ON_FAIL(parser.ReadExponentialGolomb(&chroma_format_idc));
    if (chroma_format_idc == 3) {
      // separate_colour_plane_flag: u(1)
      RETURN_FALSE_ON_FAIL(parser.ReadBits(&separate_colour_plane_flag, 1));
    }
    // bit_depth_luma_minus8: ue(v)
    RETURN_FALSE_ON_FAIL(parser.ReadExponentialGolomb(&golomb_ignored));
    // bit_depth_chroma_minus8: ue(v)
    RETURN_FALSE_ON_FAIL(parser.ReadExponentialGolomb(&golomb_ignored));
    // qpprime_y_zero_transform_bypass_flag: u(1)
    RETURN_FALSE_ON_FAIL(parser.ConsumeBits(1));
    // seq_scaling_matrix_present_flag: u(1)
    uint32_t seq_scaling_matrix_present_flag;
    RETURN_FALSE_ON_FAIL(parser.ReadBits(&seq_scaling_matrix_present_flag, 1));
    if (seq_scaling_matrix_present_flag) {
      // seq_scaling_list_present_flags. Either 8 or 12, depending on
      // chroma_format_idc.
      uint32_t seq_scaling_list_present_flags;
      if (chroma_format_idc != 3) {
        RETURN_FALSE_ON_FAIL(
            parser.ReadBits(&seq_scaling_list_present_flags, 8));
      } else {
        RETURN_FALSE_ON_FAIL(
            parser.ReadBits(&seq_scaling_list_present_flags, 12));
      }
      // We don't support reading the sequence scaling list, and we don't really
      // see/use them in practice, so we'll just reject the full sps if we see
      // any provided.
      if (seq_scaling_list_present_flags > 0) {
        LOG(LS_WARNING) << "SPS contains scaling lists, which are unsupported.";
        return false;
      }
    }
  }
  // log2_max_frame_num_minus4: ue(v)
  RETURN_FALSE_ON_FAIL(parser.ReadExponentialGolomb(&golomb_ignored));
  // pic_order_cnt_type: ue(v)
  uint32_t pic_order_cnt_type;
  RETURN_FALSE_ON_FAIL(parser.ReadExponentialGolomb(&pic_order_cnt_type));
  if (pic_order_cnt_type == 0) {
    // log2_max_pic_order_cnt_lsb_minus4: ue(v)
    RETURN_FALSE_ON_FAIL(parser.ReadExponentialGolomb(&golomb_ignored));
  } else if (pic_order_cnt_type == 1) {
    // delta_pic_order_always_zero_flag: u(1)
    RETURN_FALSE_ON_FAIL(parser.ConsumeBits(1));
    // offset_for_non_ref_pic: se(v)
    RETURN_FALSE_ON_FAIL(parser.ReadExponentialGolomb(&golomb_ignored));
    // offset_for_top_to_bottom_field: se(v)
    RETURN_FALSE_ON_FAIL(parser.ReadExponentialGolomb(&golomb_ignored));
    // num_ref_frames_in_pic_order_cnt_cycle: ue(v)
    uint32_t num_ref_frames_in_pic_order_cnt_cycle;
    RETURN_FALSE_ON_FAIL(
        parser.ReadExponentialGolomb(&num_ref_frames_in_pic_order_cnt_cycle));
    for (size_t i = 0; i < num_ref_frames_in_pic_order_cnt_cycle; ++i) {
      // offset_for_ref_frame[i]: se(v)
      RETURN_FALSE_ON_FAIL(parser.ReadExponentialGolomb(&golomb_ignored));
    }
  }
  // max_num_ref_frames: ue(v)
  RETURN_FALSE_ON_FAIL(parser.ReadExponentialGolomb(&golomb_ignored));
  // gaps_in_frame_num_value_allowed_flag: u(1)
  RETURN_FALSE_ON_FAIL(parser.ConsumeBits(1));
  //
  // IMPORTANT ONES! Now we're getting to resolution. First we read the pic
  // width/height in macroblocks (16x16), which gives us the base resolution,
  // and then we continue on until we hit the frame crop offsets, which are used
  // to signify resolutions that aren't multiples of 16.
  //
  // pic_width_in_mbs_minus1: ue(v)
  uint32_t pic_width_in_mbs_minus1;
  RETURN_FALSE_ON_FAIL(parser.ReadExponentialGolomb(&pic_width_in_mbs_minus1));
  // pic_height_in_map_units_minus1: ue(v)
  uint32_t pic_height_in_map_units_minus1;
  RETURN_FALSE_ON_FAIL(
      parser.ReadExponentialGolomb(&pic_height_in_map_units_minus1));
  // frame_mbs_only_flag: u(1)
  uint32_t frame_mbs_only_flag;
  RETURN_FALSE_ON_FAIL(parser.ReadBits(&frame_mbs_only_flag, 1));
  if (!frame_mbs_only_flag) {
    // mb_adaptive_frame_field_flag: u(1)
    RETURN_FALSE_ON_FAIL(parser.ConsumeBits(1));
  }
  // direct_8x8_inference_flag: u(1)
  RETURN_FALSE_ON_FAIL(parser.ConsumeBits(1));
  //
  // MORE IMPORTANT ONES! Now we're at the frame crop information.
  //
  // frame_cropping_flag: u(1)
  uint32_t frame_cropping_flag;
  uint32_t frame_crop_left_offset = 0;
  uint32_t frame_crop_right_offset = 0;
  uint32_t frame_crop_top_offset = 0;
  uint32_t frame_crop_bottom_offset = 0;
  RETURN_FALSE_ON_FAIL(parser.ReadBits(&frame_cropping_flag, 1));
  if (frame_cropping_flag) {
    // frame_crop_{left, right, top, bottom}_offset: ue(v)
    RETURN_FALSE_ON_FAIL(parser.ReadExponentialGolomb(&frame_crop_left_offset));
    RETURN_FALSE_ON_FAIL(
        parser.ReadExponentialGolomb(&frame_crop_right_offset));
    RETURN_FALSE_ON_FAIL(parser.ReadExponentialGolomb(&frame_crop_top_offset));
    RETURN_FALSE_ON_FAIL(
        parser.ReadExponentialGolomb(&frame_crop_bottom_offset));
  }

  // Far enough! We don't use the rest of the SPS.

  // Start with the resolution determined by the pic_width/pic_height fields.
  int width = 16 * (pic_width_in_mbs_minus1 + 1);
  int height =
      16 * (2 - frame_mbs_only_flag) * (pic_height_in_map_units_minus1 + 1);

  // Figure out the crop units in pixels. That's based on the chroma format's
  // sampling, which is indicated by chroma_format_idc.
  if (separate_colour_plane_flag || chroma_format_idc == 0) {
    frame_crop_bottom_offset *= (2 - frame_mbs_only_flag);
    frame_crop_top_offset *= (2 - frame_mbs_only_flag);
  } else if (!separate_colour_plane_flag && chroma_format_idc > 0) {
    // Width multipliers for formats 1 (4:2:0) and 2 (4:2:2).
    if (chroma_format_idc == 1 || chroma_format_idc == 2) {
      frame_crop_left_offset *= 2;
      frame_crop_right_offset *= 2;
    }
    // Height multipliers for format 1 (4:2:0).
    if (chroma_format_idc == 1) {
      frame_crop_top_offset *= 2;
      frame_crop_bottom_offset *= 2;
    }
  }
  // Subtract the crop for each dimension.
  width -= (frame_crop_left_offset + frame_crop_right_offset);
  height -= (frame_crop_top_offset + frame_crop_bottom_offset);

  width_ = width;
  height_ = height;
  return true;
}

}  // namespace webrtc
