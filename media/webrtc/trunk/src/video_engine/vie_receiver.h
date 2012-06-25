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

#include "engine_configurations.h"
#include "rtp_rtcp_defines.h"
#include "system_wrappers/interface/scoped_ptr.h"
#include "typedefs.h"
#include "udp_transport.h"
#include "vie_defines.h"

namespace webrtc {

class CriticalSectionWrapper;
class Encryption;
class RtpDump;
class RtpRtcp;
class VideoCodingModule;

class ViEReceiver : public UdpTransportData, public RtpData {
 public:
  ViEReceiver(int engine_id, int channel_id, RtpRtcp& rtp_rtcp,
              VideoCodingModule& module_vcm);
  ~ViEReceiver();

  int RegisterExternalDecryption(Encryption* decryption);
  int DeregisterExternalDecryption();

  void RegisterSimulcastRtpRtcpModules(const std::list<RtpRtcp*>& rtp_modules);

  void StartReceive();
  void StopReceive();

  int StartRTPDump(const char file_nameUTF8[1024]);
  int StopRTPDump();

  // Implements UdpTransportData.
  virtual void IncomingRTPPacket(const WebRtc_Word8* rtp_packet,
                                 const WebRtc_Word32 rtp_packet_length,
                                 const char* from_ip,
                                 const WebRtc_UWord16 from_port);
  virtual void IncomingRTCPPacket(const WebRtc_Word8* rtcp_packet,
                                  const WebRtc_Word32 rtcp_packet_length,
                                  const char* from_ip,
                                  const WebRtc_UWord16 from_port);

  // Receives packets from external transport.
  int ReceivedRTPPacket(const void* rtp_packet, int rtp_packet_length);
  int ReceivedRTCPPacket(const void* rtcp_packet, int rtcp_packet_length);

  // Implements RtpData.
  virtual WebRtc_Word32 OnReceivedPayloadData(
      const WebRtc_UWord8* payload_data,
      const WebRtc_UWord16 payload_size,
      const WebRtcRTPHeader* rtp_header);

 private:
  int InsertRTPPacket(const WebRtc_Word8* rtp_packet, int rtp_packet_length);
  int InsertRTCPPacket(const WebRtc_Word8* rtcp_packet, int rtcp_packet_length);

  scoped_ptr<CriticalSectionWrapper> receive_cs_;
  int engine_id_;
  int channel_id_;
  RtpRtcp& rtp_rtcp_;
  std::list<RtpRtcp*> rtp_rtcp_simulcast_;
  VideoCodingModule& vcm_;

  Encryption* external_decryption_;
  WebRtc_UWord8* decryption_buffer_;
  RtpDump* rtp_dump_;
  bool receiving_;
};

}  // namespace webrt

#endif  // WEBRTC_VIDEO_ENGINE_VIE_RECEIVER_H_
