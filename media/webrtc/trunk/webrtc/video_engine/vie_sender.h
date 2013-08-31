/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// ViESender is responsible for encrypting, if enabled, packets and send to
// network.

#ifndef WEBRTC_VIDEO_ENGINE_VIE_SENDER_H_
#define WEBRTC_VIDEO_ENGINE_VIE_SENDER_H_

#include "webrtc/common_types.h"
#include "webrtc/engine_configurations.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/typedefs.h"
#include "webrtc/video_engine/vie_defines.h"

namespace webrtc {

class CriticalSectionWrapper;
class RtpDump;
class Transport;
class VideoCodingModule;

class ViESender: public Transport {
 public:
  explicit ViESender(const int32_t channel_id);
  ~ViESender();

  // Registers an encryption class to use before sending packets.
  int RegisterExternalEncryption(Encryption* encryption);
  int DeregisterExternalEncryption();

  // Registers transport to use for sending RTP and RTCP.
  int RegisterSendTransport(Transport* transport);
  int DeregisterSendTransport();

  // Stores all incoming packets to file.
  int StartRTPDump(const char file_nameUTF8[1024]);
  int StopRTPDump();

  // Implements Transport.
  virtual int SendPacket(int vie_id, const void* data, int len);
  virtual int SendRTCPPacket(int vie_id, const void* data, int len);

 private:
  const int32_t channel_id_;

  scoped_ptr<CriticalSectionWrapper> critsect_;

  Encryption* external_encryption_;
  uint8_t* encryption_buffer_;
  Transport* transport_;
  RtpDump* rtp_dump_;
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_VIE_SENDER_H_
