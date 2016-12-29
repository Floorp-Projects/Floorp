/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_AUDIO_AUDIO_RECEIVE_STREAM_H_
#define WEBRTC_AUDIO_AUDIO_RECEIVE_STREAM_H_

#include "webrtc/audio_receive_stream.h"
#include "webrtc/audio_state.h"
#include "webrtc/base/thread_checker.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_header_parser.h"

namespace webrtc {
class CongestionController;
class RemoteBitrateEstimator;

namespace voe {
class ChannelProxy;
}  // namespace voe

namespace internal {

class AudioReceiveStream final : public webrtc::AudioReceiveStream {
 public:
  AudioReceiveStream(CongestionController* congestion_controller,
                     const webrtc::AudioReceiveStream::Config& config,
                     const rtc::scoped_refptr<webrtc::AudioState>& audio_state);
  ~AudioReceiveStream() override;

  // webrtc::ReceiveStream implementation.
  void Start() override;
  void Stop() override;
  void SignalNetworkState(NetworkState state) override;
  bool DeliverRtcp(const uint8_t* packet, size_t length) override;
  bool DeliverRtp(const uint8_t* packet,
                  size_t length,
                  const PacketTime& packet_time) override;

  // webrtc::AudioReceiveStream implementation.
  webrtc::AudioReceiveStream::Stats GetStats() const override;

  void SetSink(rtc::scoped_ptr<AudioSinkInterface> sink) override;

  const webrtc::AudioReceiveStream::Config& config() const;

 private:
  VoiceEngine* voice_engine() const;

  rtc::ThreadChecker thread_checker_;
  RemoteBitrateEstimator* remote_bitrate_estimator_ = nullptr;
  const webrtc::AudioReceiveStream::Config config_;
  rtc::scoped_refptr<webrtc::AudioState> audio_state_;
  rtc::scoped_ptr<RtpHeaderParser> rtp_header_parser_;
  rtc::scoped_ptr<voe::ChannelProxy> channel_proxy_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(AudioReceiveStream);
};
}  // namespace internal
}  // namespace webrtc

#endif  // WEBRTC_AUDIO_AUDIO_RECEIVE_STREAM_H_
