/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_CHANNEL_PROXY_H_
#define WEBRTC_VOICE_ENGINE_CHANNEL_PROXY_H_

#include "webrtc/base/thread_checker.h"
#include "webrtc/voice_engine/channel_manager.h"
#include "webrtc/voice_engine/include/voe_rtp_rtcp.h"

#include <string>
#include <vector>

namespace webrtc {

class AudioSinkInterface;
class PacketRouter;
class RtpPacketSender;
class TransportFeedbackObserver;

namespace voe {

class Channel;

// This class provides the "view" of a voe::Channel that we need to implement
// webrtc::AudioSendStream and webrtc::AudioReceiveStream. It serves two
// purposes:
//  1. Allow mocking just the interfaces used, instead of the entire
//     voe::Channel class.
//  2. Provide a refined interface for the stream classes, including assumptions
//     on return values and input adaptation.
class ChannelProxy {
 public:
  ChannelProxy();
  explicit ChannelProxy(const ChannelOwner& channel_owner);
  virtual ~ChannelProxy();

  virtual void SetRTCPStatus(bool enable);
  virtual void SetLocalSSRC(uint32_t ssrc);
  virtual void SetRTCP_CNAME(const std::string& c_name);
  virtual void SetSendAbsoluteSenderTimeStatus(bool enable, int id);
  virtual void SetSendAudioLevelIndicationStatus(bool enable, int id);
  virtual void EnableSendTransportSequenceNumber(int id);
  virtual void SetReceiveAbsoluteSenderTimeStatus(bool enable, int id);
  virtual void SetReceiveAudioLevelIndicationStatus(bool enable, int id);
  virtual void SetCongestionControlObjects(
      RtpPacketSender* rtp_packet_sender,
      TransportFeedbackObserver* transport_feedback_observer,
      PacketRouter* packet_router);

  virtual CallStatistics GetRTCPStatistics() const;
  virtual std::vector<ReportBlock> GetRemoteRTCPReportBlocks() const;
  virtual NetworkStatistics GetNetworkStatistics() const;
  virtual AudioDecodingCallStats GetDecodingCallStatistics() const;
  virtual int32_t GetSpeechOutputLevelFullRange() const;
  virtual uint32_t GetDelayEstimate() const;

  virtual bool SetSendTelephoneEventPayloadType(int payload_type);
  virtual bool SendTelephoneEventOutband(uint8_t event, uint32_t duration_ms);

  virtual void SetSink(rtc::scoped_ptr<AudioSinkInterface> sink);

 private:
  Channel* channel() const;

  rtc::ThreadChecker thread_checker_;
  ChannelOwner channel_owner_;
};
}  // namespace voe
}  // namespace webrtc

#endif  // WEBRTC_VOICE_ENGINE_CHANNEL_PROXY_H_
