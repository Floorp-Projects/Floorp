/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_VIE_RECEIVER_H_
#define WEBRTC_VIDEO_ENGINE_VIE_RECEIVER_H_

#include <list>

#include "webrtc/engine_configurations.h"
#include "webrtc/modules/rtp_rtcp/interface/receive_statistics.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp_defines.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/typedefs.h"
#include "webrtc/video_engine/vie_defines.h"

namespace webrtc {

class CriticalSectionWrapper;
class Encryption;
class FecReceiver;
class ReceiveStatistics;
class RemoteBitrateEstimator;
class RtpDump;
class RtpHeaderParser;
class RTPPayloadRegistry;
class RtpReceiver;
class RtpRtcp;
class VideoCodingModule;

class ViEReceiver : public RtpData {
 public:
  ViEReceiver(const int32_t channel_id, VideoCodingModule* module_vcm,
              RemoteBitrateEstimator* remote_bitrate_estimator,
              RtpFeedback* rtp_feedback);
  ~ViEReceiver();

  bool SetReceiveCodec(const VideoCodec& video_codec);
  bool RegisterPayload(const VideoCodec& video_codec);

  void SetNackStatus(bool enable, int max_nack_reordering_threshold);
  void SetRtxStatus(bool enable, uint32_t ssrc);
  void SetRtxPayloadType(uint32_t payload_type);

  uint32_t GetRemoteSsrc() const;
  int GetCsrcs(uint32_t* csrcs) const;

  int RegisterExternalDecryption(Encryption* decryption);
  int DeregisterExternalDecryption();

  void SetRtpRtcpModule(RtpRtcp* module);

  RtpReceiver* GetRtpReceiver() const;

  void RegisterSimulcastRtpRtcpModules(const std::list<RtpRtcp*>& rtp_modules);

  bool SetReceiveTimestampOffsetStatus(bool enable, int id);
  bool SetReceiveAbsoluteSendTimeStatus(bool enable, int id);

  void StartReceive();
  void StopReceive();

  void StartRTCPReceive();
  void StopRTCPReceive();

  int StartRTPDump(const char file_nameUTF8[1024]);
  int StopRTPDump();

  // Receives packets from external transport.
  int ReceivedRTPPacket(const void* rtp_packet, int rtp_packet_length);
  int ReceivedRTCPPacket(const void* rtcp_packet, int rtcp_packet_length);
  virtual bool OnRecoveredPacket(const uint8_t* packet,
                                 int packet_length) OVERRIDE;

  // Implements RtpData.
  virtual int32_t OnReceivedPayloadData(
      const uint8_t* payload_data,
      const uint16_t payload_size,
      const WebRtcRTPHeader* rtp_header);

  void EstimatedReceiveBandwidth(unsigned int* available_bandwidth) const;

  ReceiveStatistics* GetReceiveStatistics() const;

 private:
  int InsertRTPPacket(const int8_t* rtp_packet, int rtp_packet_length);
  bool ReceivePacket(const uint8_t* packet, int packet_length,
                     const RTPHeader& header, bool in_order);
  // Parses and handles for instance RTX and RED headers.
  // This function assumes that it's being called from only one thread.
  bool ParseAndHandleEncapsulatingHeader(const uint8_t* packet,
                                         int packet_length,
                                         const RTPHeader& header);
  int InsertRTCPPacket(const int8_t* rtcp_packet, int rtcp_packet_length);
  bool IsPacketInOrder(const RTPHeader& header) const;
  bool IsPacketRetransmitted(const RTPHeader& header, bool in_order) const;

  scoped_ptr<CriticalSectionWrapper> receive_cs_;
  const int32_t channel_id_;
  scoped_ptr<RtpHeaderParser> rtp_header_parser_;
  scoped_ptr<RTPPayloadRegistry> rtp_payload_registry_;
  scoped_ptr<RtpReceiver> rtp_receiver_;
  scoped_ptr<ReceiveStatistics> rtp_receive_statistics_;
  scoped_ptr<FecReceiver> fec_receiver_;
  RtpRtcp* rtp_rtcp_;
  std::list<RtpRtcp*> rtp_rtcp_simulcast_;
  VideoCodingModule* vcm_;
  RemoteBitrateEstimator* remote_bitrate_estimator_;

  Encryption* external_decryption_;
  uint8_t* decryption_buffer_;
  RtpDump* rtp_dump_;
  bool receiving_;
  bool receiving_rtcp_;
  uint8_t restored_packet_[kViEMaxMtu];
  bool restored_packet_in_use_;
};

}  // namespace webrt

#endif  // WEBRTC_VIDEO_ENGINE_VIE_RECEIVER_H_
