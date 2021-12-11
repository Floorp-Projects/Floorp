/*
 *  Copyright (c) 2010 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MEDIA_ENGINE_FAKEWEBRTCVOICEENGINE_H_
#define MEDIA_ENGINE_FAKEWEBRTCVOICEENGINE_H_

#include <map>
#include <vector>

#include "media/engine/webrtcvoe.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace voe {
class TransmitMixer;
}  // namespace voe
}  // namespace webrtc

namespace cricket {

#define WEBRTC_CHECK_CHANNEL(channel) \
  if (channels_.find(channel) == channels_.end()) return -1;

#define WEBRTC_STUB(method, args) \
  int method args override { return 0; }

#define WEBRTC_FUNC(method, args) int method args override

class FakeWebRtcVoiceEngine : public webrtc::VoEBase {
 public:
  struct Channel {
    std::vector<webrtc::CodecInst> recv_codecs;
    size_t neteq_capacity = 0;
    bool neteq_fast_accelerate = false;
  };

  explicit FakeWebRtcVoiceEngine(webrtc::voe::TransmitMixer* transmit_mixer)
      : transmit_mixer_(transmit_mixer) {}
  ~FakeWebRtcVoiceEngine() override {
    RTC_CHECK(channels_.empty());
  }

  bool IsInited() const { return inited_; }
  int GetLastChannel() const { return last_channel_; }
  int GetNumChannels() const { return static_cast<int>(channels_.size()); }
  void set_fail_create_channel(bool fail_create_channel) {
    fail_create_channel_ = fail_create_channel;
  }

  WEBRTC_STUB(Release, ());

  // webrtc::VoEBase
  WEBRTC_FUNC(Init,
              (webrtc::AudioDeviceModule* adm,
               webrtc::AudioProcessing* audioproc,
               const rtc::scoped_refptr<webrtc::AudioDecoderFactory>&
                   decoder_factory)) {
    inited_ = true;
    return 0;
  }
  void Terminate() override {
    inited_ = false;
  }
  webrtc::voe::TransmitMixer* transmit_mixer() override {
    return transmit_mixer_;
  }
  WEBRTC_FUNC(CreateChannel, ()) {
    return CreateChannel(webrtc::VoEBase::ChannelConfig());
  }
  WEBRTC_FUNC(CreateChannel, (const webrtc::VoEBase::ChannelConfig& config)) {
    if (fail_create_channel_) {
      return -1;
    }
    Channel* ch = new Channel();
    ch->neteq_capacity = config.acm_config.neteq_config.max_packets_in_buffer;
    ch->neteq_fast_accelerate =
        config.acm_config.neteq_config.enable_fast_accelerate;
    channels_[++last_channel_] = ch;
    return last_channel_;
  }
  WEBRTC_FUNC(DeleteChannel, (int channel)) {
    WEBRTC_CHECK_CHANNEL(channel);
    delete channels_[channel];
    channels_.erase(channel);
    return 0;
  }
  WEBRTC_STUB(StartPlayout, (int channel));
  WEBRTC_STUB(StartSend, (int channel));
  WEBRTC_STUB(StopPlayout, (int channel));
  WEBRTC_STUB(StopSend, (int channel));
  WEBRTC_STUB(SetPlayout, (bool enable));
  WEBRTC_STUB(SetRecording, (bool enable));

  size_t GetNetEqCapacity() const {
    auto ch = channels_.find(last_channel_);
    RTC_DCHECK(ch != channels_.end());
    return ch->second->neteq_capacity;
  }
  bool GetNetEqFastAccelerate() const {
    auto ch = channels_.find(last_channel_);
    RTC_CHECK(ch != channels_.end());
    return ch->second->neteq_fast_accelerate;
  }

 private:
  bool inited_ = false;
  int last_channel_ = -1;
  std::map<int, Channel*> channels_;
  bool fail_create_channel_ = false;
  webrtc::voe::TransmitMixer* transmit_mixer_ = nullptr;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(FakeWebRtcVoiceEngine);
};

}  // namespace cricket

#endif  // MEDIA_ENGINE_FAKEWEBRTCVOICEENGINE_H_
