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
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp_defines.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/typedefs.h"
#include "webrtc/video_engine/vie_defines.h"

namespace webrtc {

class CriticalSectionWrapper;
class Encryption;
class RemoteBitrateEstimator;
class RtpDump;
class RtpHeaderParser;
class RtpRtcp;
class VideoCodingModule;

class ViEReceiver : public RtpData {
 public:
  ViEReceiver(const int32_t channel_id, VideoCodingModule* module_vcm,
              RemoteBitrateEstimator* remote_bitrate_estimator);
  ~ViEReceiver();

  int RegisterExternalDecryption(Encryption* decryption);
  int DeregisterExternalDecryption();

  void SetRtpRtcpModule(RtpRtcp* module);

  void RegisterSimulcastRtpRtcpModules(const std::list<RtpRtcp*>& rtp_modules);

  bool SetReceiveTimestampOffsetStatus(bool enable, int id);
  bool SetReceiveAbsoluteSendTimeStatus(bool enable, int id);

  void StartReceive();
  void StopReceive();

  int StartRTPDump(const char file_nameUTF8[1024]);
  int StopRTPDump();

  // Receives packets from external transport.
  int ReceivedRTPPacket(const void* rtp_packet, int rtp_packet_length);
  int ReceivedRTCPPacket(const void* rtcp_packet, int rtcp_packet_length);

  // Implements RtpData.
  virtual int32_t OnReceivedPayloadData(
      const uint8_t* payload_data,
      const uint16_t payload_size,
      const WebRtcRTPHeader* rtp_header);

  void OnSendReportReceived(const int32_t id,
                            const uint32_t senderSSRC,
                            uint32_t ntp_secs,
                            uint32_t ntp_frac,
                            uint32_t timestamp);

  void EstimatedReceiveBandwidth(unsigned int* available_bandwidth) const;

 private:
  int InsertRTPPacket(const int8_t* rtp_packet, int rtp_packet_length);
  int InsertRTCPPacket(const int8_t* rtcp_packet, int rtcp_packet_length);

  scoped_ptr<CriticalSectionWrapper> receive_cs_;
  const int32_t channel_id_;
  scoped_ptr<RtpHeaderParser> rtp_header_parser_;
  RtpRtcp* rtp_rtcp_;
  std::list<RtpRtcp*> rtp_rtcp_simulcast_;
  VideoCodingModule* vcm_;
  RemoteBitrateEstimator* remote_bitrate_estimator_;

  Encryption* external_decryption_;
  uint8_t* decryption_buffer_;
  RtpDump* rtp_dump_;
  bool receiving_;
};

}  // namespace webrt

#endif  // WEBRTC_VIDEO_ENGINE_VIE_RECEIVER_H_
