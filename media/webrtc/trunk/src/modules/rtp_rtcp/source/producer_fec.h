/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_PRODUCER_FEC_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_PRODUCER_FEC_H_

#include <list>

#include "modules/rtp_rtcp/source/forward_error_correction.h"

namespace webrtc {

struct RtpPacket;

class RedPacket {
 public:
  explicit RedPacket(int length);
  ~RedPacket();
  void CreateHeader(const uint8_t* rtp_header, int header_length,
                    int red_pl_type, int pl_type);
  void SetSeqNum(int seq_num);
  void AssignPayload(const uint8_t* payload, int length);
  void ClearMarkerBit();
  uint8_t* data() const;
  int length() const;

 private:
  uint8_t* data_;
  int length_;
  int header_length_;
};

class ProducerFec {
 public:
  explicit ProducerFec(ForwardErrorCorrection* fec);
  ~ProducerFec();

  void SetFecParameters(const FecProtectionParams* params,
                        int max_fec_frames);

  RedPacket* BuildRedPacket(const uint8_t* data_buffer,
                            int payload_length,
                            int rtp_header_length,
                            int red_pl_type);

  int AddRtpPacketAndGenerateFec(const uint8_t* data_buffer,
                                 int payload_length,
                                 int rtp_header_length);

  bool ExcessOverheadBelowMax();

  bool MinimumMediaPacketsReached();

  bool FecAvailable() const;

  RedPacket* GetFecPacket(int red_pl_type,
                          int fec_pl_type,
                          uint16_t seq_num,
                          int rtp_header_length);

 private:
  void DeletePackets();
  int Overhead() const;
  ForwardErrorCorrection* fec_;
  std::list<ForwardErrorCorrection::Packet*> media_packets_fec_;
  std::list<ForwardErrorCorrection::Packet*> fec_packets_;
  int num_frames_;
  bool incomplete_frame_;
  int num_first_partition_;
  int minimum_media_packets_fec_;
  FecProtectionParams params_;
  FecProtectionParams new_params_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_PRODUCER_FEC_H_
