/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/common_types.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/modules/video_coding/codecs/interface/video_codec_interface.h"
#include "webrtc/modules/video_coding/main/source/encoded_frame.h"
#include "webrtc/modules/video_coding/main/source/jitter_buffer.h"
#include "webrtc/modules/video_coding/main/source/packet.h"
#include "webrtc/modules/video_coding/main/source/video_coding_impl.h"
#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/system_wrappers/interface/trace.h"
#include "webrtc/system_wrappers/interface/trace_event.h"

namespace webrtc {
namespace vcm {

uint32_t
VCMProcessTimer::Period() const {
    return _periodMs;
}

uint32_t
VCMProcessTimer::TimeUntilProcess() const {
    const int64_t time_since_process = _clock->TimeInMilliseconds() -
        static_cast<int64_t>(_latestMs);
    const int64_t time_until_process = static_cast<int64_t>(_periodMs) -
        time_since_process;
    if (time_until_process < 0)
      return 0;
    return time_until_process;
}

void
VCMProcessTimer::Processed() {
    _latestMs = _clock->TimeInMilliseconds();
}
}  // namespace vcm

namespace {
class VideoCodingModuleImpl : public VideoCodingModule {
 public:
  VideoCodingModuleImpl(const int32_t id,
                        Clock* clock,
                        EventFactory* event_factory,
                        bool owns_event_factory)
      : VideoCodingModule(),
        sender_(new vcm::VideoSender(id, clock)),
        receiver_(new vcm::VideoReceiver(id, clock, event_factory)),
        own_event_factory_(owns_event_factory ? event_factory : NULL) {}

  virtual ~VideoCodingModuleImpl() {
    sender_.reset();
    receiver_.reset();
    own_event_factory_.reset();
  }

  virtual int32_t TimeUntilNextProcess() OVERRIDE {
    int32_t sender_time = sender_->TimeUntilNextProcess();
    int32_t receiver_time = receiver_->TimeUntilNextProcess();
    assert(sender_time >= 0);
    assert(receiver_time >= 0);
    return VCM_MIN(sender_time, receiver_time);
  }

  virtual int32_t Process() OVERRIDE {
    int32_t sender_return = sender_->Process();
    int32_t receiver_return = receiver_->Process();
    if (sender_return != VCM_OK)
      return sender_return;
    return receiver_return;
  }

  virtual int32_t InitializeSender() OVERRIDE {
    return sender_->InitializeSender();
  }

  virtual int32_t RegisterSendCodec(const VideoCodec* sendCodec,
                                    uint32_t numberOfCores,
                                    uint32_t maxPayloadSize) OVERRIDE {
    return sender_->RegisterSendCodec(sendCodec, numberOfCores, maxPayloadSize);
  }

  virtual int32_t SendCodec(VideoCodec* currentSendCodec) const OVERRIDE {
    return sender_->SendCodec(currentSendCodec);
  }

  virtual VideoCodecType SendCodec() const OVERRIDE {
    return sender_->SendCodec();
  }

  virtual int32_t RegisterExternalEncoder(VideoEncoder* externalEncoder,
                                          uint8_t payloadType,
                                          bool internalSource) OVERRIDE {
    return sender_->RegisterExternalEncoder(
        externalEncoder, payloadType, internalSource);
  }

  virtual int32_t CodecConfigParameters(uint8_t* buffer, int32_t size)
      OVERRIDE {
    return sender_->CodecConfigParameters(buffer, size);
  }

  virtual int Bitrate(unsigned int* bitrate) const OVERRIDE {
    return sender_->Bitrate(bitrate);
  }

  virtual int FrameRate(unsigned int* framerate) const OVERRIDE {
    return sender_->FrameRate(framerate);
  }

  virtual int32_t SetChannelParameters(uint32_t target_bitrate,  // bits/s.
                                       uint8_t lossRate,
                                       uint32_t rtt) OVERRIDE {
    return sender_->SetChannelParameters(target_bitrate, lossRate, rtt);
  }

  virtual int32_t RegisterTransportCallback(VCMPacketizationCallback* transport)
      OVERRIDE {
    return sender_->RegisterTransportCallback(transport);
  }

  virtual int32_t RegisterSendStatisticsCallback(
      VCMSendStatisticsCallback* sendStats) OVERRIDE {
    return sender_->RegisterSendStatisticsCallback(sendStats);
  }

  virtual int32_t RegisterVideoQMCallback(
      VCMQMSettingsCallback* videoQMSettings) OVERRIDE {
    return sender_->RegisterVideoQMCallback(videoQMSettings);
  }

  virtual int32_t RegisterProtectionCallback(VCMProtectionCallback* protection)
      OVERRIDE {
    return sender_->RegisterProtectionCallback(protection);
  }

  virtual int32_t SetVideoProtection(VCMVideoProtection videoProtection,
                                     bool enable) OVERRIDE {
    int32_t sender_return =
        sender_->SetVideoProtection(videoProtection, enable);
    int32_t receiver_return =
        receiver_->SetVideoProtection(videoProtection, enable);
    if (sender_return == VCM_OK)
      return receiver_return;
    return sender_return;
  }

  virtual int32_t AddVideoFrame(const I420VideoFrame& videoFrame,
                                const VideoContentMetrics* contentMetrics,
                                const CodecSpecificInfo* codecSpecificInfo)
      OVERRIDE {
    return sender_->AddVideoFrame(
        videoFrame, contentMetrics, codecSpecificInfo);
  }

  virtual int32_t IntraFrameRequest(int stream_index) OVERRIDE {
    return sender_->IntraFrameRequest(stream_index);
  }

  virtual int32_t EnableFrameDropper(bool enable) OVERRIDE {
    return sender_->EnableFrameDropper(enable);
  }

  virtual int32_t SentFrameCount(VCMFrameCount& frameCount) const OVERRIDE {
    return sender_->SentFrameCount(&frameCount);
  }

  virtual int SetSenderNackMode(SenderNackMode mode) OVERRIDE {
    return sender_->SetSenderNackMode(mode);
  }

  virtual int SetSenderReferenceSelection(bool enable) OVERRIDE {
    return sender_->SetSenderReferenceSelection(enable);
  }

  virtual int SetSenderFEC(bool enable) OVERRIDE {
    return sender_->SetSenderFEC(enable);
  }

  virtual int SetSenderKeyFramePeriod(int periodMs) OVERRIDE {
    return sender_->SetSenderKeyFramePeriod(periodMs);
  }

  virtual int StartDebugRecording(const char* file_name_utf8) OVERRIDE {
    return sender_->StartDebugRecording(file_name_utf8);
  }

  virtual int StopDebugRecording() OVERRIDE {
    return sender_->StopDebugRecording();
  }

  virtual int32_t InitializeReceiver() OVERRIDE {
    return receiver_->InitializeReceiver();
  }

  virtual int32_t RegisterReceiveCodec(const VideoCodec* receiveCodec,
                                       int32_t numberOfCores,
                                       bool requireKeyFrame) OVERRIDE {
    return receiver_->RegisterReceiveCodec(
        receiveCodec, numberOfCores, requireKeyFrame);
  }

  virtual int32_t RegisterExternalDecoder(VideoDecoder* externalDecoder,
                                          uint8_t payloadType,
                                          bool internalRenderTiming) OVERRIDE {
    return receiver_->RegisterExternalDecoder(
        externalDecoder, payloadType, internalRenderTiming);
  }

  virtual int32_t RegisterReceiveCallback(VCMReceiveCallback* receiveCallback)
      OVERRIDE {
    return receiver_->RegisterReceiveCallback(receiveCallback);
  }

  virtual int32_t RegisterReceiveStatisticsCallback(
      VCMReceiveStatisticsCallback* receiveStats) OVERRIDE {
    return receiver_->RegisterReceiveStatisticsCallback(receiveStats);
  }

  virtual int32_t RegisterFrameTypeCallback(
      VCMFrameTypeCallback* frameTypeCallback) OVERRIDE {
    return receiver_->RegisterFrameTypeCallback(frameTypeCallback);
  }

  virtual int32_t RegisterPacketRequestCallback(
      VCMPacketRequestCallback* callback) OVERRIDE {
    return receiver_->RegisterPacketRequestCallback(callback);
  }

  virtual int RegisterRenderBufferSizeCallback(
      VCMRenderBufferSizeCallback* callback) OVERRIDE {
    return receiver_->RegisterRenderBufferSizeCallback(callback);
  }

  virtual int32_t Decode(uint16_t maxWaitTimeMs) OVERRIDE {
    return receiver_->Decode(maxWaitTimeMs);
  }

  virtual int32_t DecodeDualFrame(uint16_t maxWaitTimeMs) OVERRIDE {
    return receiver_->DecodeDualFrame(maxWaitTimeMs);
  }

  virtual int32_t ResetDecoder() OVERRIDE { return receiver_->ResetDecoder(); }

  virtual int32_t ReceiveCodec(VideoCodec* currentReceiveCodec) const {
    return receiver_->ReceiveCodec(currentReceiveCodec);
  }

  virtual VideoCodecType ReceiveCodec() const OVERRIDE {
    return receiver_->ReceiveCodec();
  }

  virtual int32_t IncomingPacket(const uint8_t* incomingPayload,
                                 uint32_t payloadLength,
                                 const WebRtcRTPHeader& rtpInfo) OVERRIDE {
    return receiver_->IncomingPacket(incomingPayload, payloadLength, rtpInfo);
  }

  virtual int32_t SetMinimumPlayoutDelay(uint32_t minPlayoutDelayMs) OVERRIDE {
    return receiver_->SetMinimumPlayoutDelay(minPlayoutDelayMs);
  }

  virtual int32_t SetRenderDelay(uint32_t timeMS) OVERRIDE {
    return receiver_->SetRenderDelay(timeMS);
  }

  virtual int32_t Delay() const OVERRIDE { return receiver_->Delay(); }

  virtual int32_t ReceivedFrameCount(VCMFrameCount& frameCount) const OVERRIDE {
    return receiver_->ReceivedFrameCount(&frameCount);
  }

  virtual uint32_t DiscardedPackets() const OVERRIDE {
    return receiver_->DiscardedPackets();
  }

  virtual int SetReceiverRobustnessMode(ReceiverRobustness robustnessMode,
                                        VCMDecodeErrorMode errorMode) OVERRIDE {
    return receiver_->SetReceiverRobustnessMode(robustnessMode, errorMode);
  }

  virtual void SetNackSettings(size_t max_nack_list_size,
                               int max_packet_age_to_nack,
                               int max_incomplete_time_ms) OVERRIDE {
    return receiver_->SetNackSettings(
        max_nack_list_size, max_packet_age_to_nack, max_incomplete_time_ms);
  }

  void SetDecodeErrorMode(VCMDecodeErrorMode decode_error_mode) OVERRIDE {
    return receiver_->SetDecodeErrorMode(decode_error_mode);
  }

  virtual int SetMinReceiverDelay(int desired_delay_ms) OVERRIDE {
    return receiver_->SetMinReceiverDelay(desired_delay_ms);
  }

  virtual int32_t SetReceiveChannelParameters(uint32_t rtt) OVERRIDE {
    return receiver_->SetReceiveChannelParameters(rtt);
  }

 private:
  scoped_ptr<vcm::VideoSender> sender_;
  scoped_ptr<vcm::VideoReceiver> receiver_;
  scoped_ptr<EventFactory> own_event_factory_;
};
}  // namespace

uint8_t VideoCodingModule::NumberOfCodecs() {
  return VCMCodecDataBase::NumberOfCodecs();
}

int32_t VideoCodingModule::Codec(uint8_t listId, VideoCodec* codec) {
  if (codec == NULL) {
    return VCM_PARAMETER_ERROR;
  }
  return VCMCodecDataBase::Codec(listId, codec) ? 0 : -1;
}

int32_t VideoCodingModule::Codec(VideoCodecType codecType, VideoCodec* codec) {
  if (codec == NULL) {
    return VCM_PARAMETER_ERROR;
  }
  return VCMCodecDataBase::Codec(codecType, codec) ? 0 : -1;
}

VideoCodingModule* VideoCodingModule::Create(const int32_t id) {
  return new VideoCodingModuleImpl(
      id, Clock::GetRealTimeClock(), new EventFactoryImpl, true);
}

VideoCodingModule* VideoCodingModule::Create(const int32_t id,
                                             Clock* clock,
                                             EventFactory* event_factory) {
  assert(clock);
  assert(event_factory);
  return new VideoCodingModuleImpl(id, clock, event_factory, false);
}

void VideoCodingModule::Destroy(VideoCodingModule* module) {
  if (module != NULL) {
    delete static_cast<VideoCodingModuleImpl*>(module);
  }
}
}  // namespace webrtc
