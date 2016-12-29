/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_H264_BITSTREAM_PARSER_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_H264_BITSTREAM_PARSER_H_

#include <stddef.h>
#include <stdint.h>

namespace rtc {
class BitBuffer;
}

namespace webrtc {

// Stateful H264 bitstream parser (due to SPS/PPS). Used to parse out QP values
// from the bitstream.
// TODO(pbos): Unify with RTP SPS parsing and only use one H264 parser.
// TODO(pbos): If/when this gets used on the receiver side CHECKs must be
// removed and gracefully abort as we have no control over receive-side
// bitstreams.
class H264BitstreamParser {
 public:
  // Parse an additional chunk of H264 bitstream.
  void ParseBitstream(const uint8_t* bitstream, size_t length);

  // Get the last extracted QP value from the parsed bitstream.
  bool GetLastSliceQp(int* qp) const;

 private:
  // Captured in SPS and used when parsing slice NALUs.
  struct SpsState {
    SpsState();

    uint32_t delta_pic_order_always_zero_flag = 0;
    uint32_t separate_colour_plane_flag = 0;
    uint32_t frame_mbs_only_flag = 0;
    uint32_t log2_max_frame_num_minus4 = 0;
    uint32_t log2_max_pic_order_cnt_lsb_minus4 = 0;
    uint32_t pic_order_cnt_type = 0;
  };

  struct PpsState {
    PpsState();

    bool bottom_field_pic_order_in_frame_present_flag = false;
    bool weighted_pred_flag = false;
    uint32_t weighted_bipred_idc = false;
    uint32_t redundant_pic_cnt_present_flag = 0;
    int pic_init_qp_minus26 = 0;
  };

  void ParseSlice(const uint8_t* slice, size_t length);
  bool ParseSpsNalu(const uint8_t* sps_nalu, size_t length);
  bool ParsePpsNalu(const uint8_t* pps_nalu, size_t length);
  bool ParseNonParameterSetNalu(const uint8_t* source,
                                size_t source_length,
                                uint8_t nalu_type);

  // SPS/PPS state, updated when parsing new SPS/PPS, used to parse slices.
  bool sps_parsed_ = false;
  SpsState sps_;
  bool pps_parsed_ = false;
  PpsState pps_;

  // Last parsed slice QP.
  bool last_slice_qp_delta_parsed_ = false;
  int32_t last_slice_qp_delta_ = 0;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_H264_BITSTREAM_PARSER_H_
