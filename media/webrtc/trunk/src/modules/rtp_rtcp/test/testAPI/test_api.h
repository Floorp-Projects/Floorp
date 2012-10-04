/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_types.h"
#include "rtp_rtcp.h"
#include "rtp_rtcp_defines.h"

namespace webrtc {

class FakeRtpRtcpClock : public RtpRtcpClock {
 public:
  FakeRtpRtcpClock() {
    time_in_ms_ = 123456;
  }
  // Return a timestamp in milliseconds relative to some arbitrary
  // source; the source is fixed for this clock.
  virtual WebRtc_Word64 GetTimeInMS() {
    return time_in_ms_;
  }
  // Retrieve an NTP absolute timestamp.
  virtual void CurrentNTP(WebRtc_UWord32& secs, WebRtc_UWord32& frac) {
    secs = time_in_ms_ / 1000;
    frac = (time_in_ms_ % 1000) * 4294967;
  }
  void IncrementTime(WebRtc_UWord32 time_increment_ms) {
    time_in_ms_ += time_increment_ms;
  }
 private:
  WebRtc_Word64 time_in_ms_;
};

// This class sends all its packet straight to the provided RtpRtcp module.
// with optional packet loss.
class LoopBackTransport : public webrtc::Transport {
 public:
  LoopBackTransport()
    : _count(0),
      _packetLoss(0),
      _rtpRtcpModule(NULL) {
  }
  void SetSendModule(RtpRtcp* rtpRtcpModule) {
    _rtpRtcpModule = rtpRtcpModule;
  }
  void DropEveryNthPacket(int n) {
    _packetLoss = n;
  }
  virtual int SendPacket(int channel, const void *data, int len) {
    _count++;
    if (_packetLoss > 0) {
      if ((_count % _packetLoss) == 0) {
        return len;
      }
    }
    if (_rtpRtcpModule->IncomingPacket((const WebRtc_UWord8*)data, len) == 0) {
      return len;
    }
    return -1;
  }
  virtual int SendRTCPPacket(int channel, const void *data, int len) {
    if (_rtpRtcpModule->IncomingPacket((const WebRtc_UWord8*)data, len) == 0) {
      return len;
    }
    return -1;
  }
 private:
  int _count;
  int _packetLoss;
  RtpRtcp* _rtpRtcpModule;
};

class RtpReceiver : public RtpData {
 public:
   virtual WebRtc_Word32 OnReceivedPayloadData(
       const WebRtc_UWord8* payloadData,
       const WebRtc_UWord16 payloadSize,
       const webrtc::WebRtcRTPHeader* rtpHeader) {
    return 0;
  }
};

}  // namespace webrtc
 
