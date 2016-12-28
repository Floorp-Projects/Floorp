/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/video_coding_impl.h"

#include <algorithm>

#include "webrtc/common_types.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/modules/video_coding/include/video_codec_interface.h"
#include "webrtc/modules/video_coding/encoded_frame.h"
#include "webrtc/modules/video_coding/jitter_buffer.h"
#include "webrtc/modules/video_coding/packet.h"
#include "webrtc/system_wrappers/include/clock.h"

namespace webrtc {
namespace vcm {

int64_t VCMProcessTimer::Period() const {
  return _periodMs;
}

int64_t VCMProcessTimer::TimeUntilProcess() const {
  const int64_t time_since_process = _clock->TimeInMilliseconds() - _latestMs;
  const int64_t time_until_process = _periodMs - time_since_process;
  return std::max<int64_t>(time_until_process, 0);
}

void VCMProcessTimer::Processed() {
  _latestMs = _clock->TimeInMilliseconds();
}
}  // namespace vcm

namespace {
// This wrapper provides a way to modify the callback without the need to expose
// a register method all the way down to the function calling it.
class EncodedImageCallbackWrapper : public EncodedImageCallback {
 public:
  EncodedImageCallbackWrapper()
      : cs_(CriticalSectionWrapper::CreateCriticalSection()), callback_(NULL) {}

  virtual ~EncodedImageCallbackWrapper() {}

  void Register(EncodedImageCallback* callback) {
    CriticalSectionScoped cs(cs_.get());
    callback_ = callback;
  }

  // TODO(andresp): Change to void as return value is ignored.
  virtual int32_t Encoded(const EncodedImage& encoded_image,
                          const CodecSpecificInfo* codec_specific_info,
                          const RTPFragmentationHeader* fragmentation) {
    CriticalSectionScoped cs(cs_.get());
    if (callback_)
      return callback_->Encoded(encoded_image, codec_specific_info,
                                fragmentation);
    return 0;
  }

 private:
  rtc::scoped_ptr<CriticalSectionWrapper> cs_;
  EncodedImageCallback* callback_ GUARDED_BY(cs_);
};

class VideoCodingModuleImpl : public VideoCodingModule {
 public:
  VideoCodingModuleImpl(Clock* clock,
                        EventFactory* event_factory,
                        bool owns_event_factory,
                        VideoEncoderRateObserver* encoder_rate_observer,
                        VCMQMSettingsCallback* qm_settings_callback)
      : VideoCodingModule(),
        sender_(clock,
                &post_encode_callback_,
                encoder_rate_observer,
                qm_settings_callback),
        receiver_(clock, event_factory),
        own_event_factory_(owns_event_factory ? event_factory : NULL) {}

  virtual ~VideoCodingModuleImpl() { own_event_factory_.reset(); }

  int64_t TimeUntilNextProcess() override {
    int64_t sender_time = sender_.TimeUntilNextProcess();
    int64_t receiver_time = receiver_.TimeUntilNextProcess();
    assert(sender_time >= 0);
    assert(receiver_time >= 0);
    return VCM_MIN(sender_time, receiver_time);
  }

  int32_t Process() override {
    int32_t sender_return = sender_.Process();
    int32_t receiver_return = receiver_.Process();
    if (sender_return != VCM_OK)
      return sender_return;
    return receiver_return;
  }

  int32_t RegisterSendCodec(const VideoCodec* sendCodec,
                            uint32_t numberOfCores,
                            uint32_t maxPayloadSize) override {
    return sender_.RegisterSendCodec(sendCodec, numberOfCores, maxPayloadSize);
  }

  int32_t RegisterExternalEncoder(VideoEncoder* externalEncoder,
                                  uint8_t payloadType,
                                  bool internalSource) override {
    sender_.RegisterExternalEncoder(externalEncoder, payloadType,
                                    internalSource);
    return 0;
  }

  int Bitrate(unsigned int* bitrate) const override {
    return sender_.Bitrate(bitrate);
  }

  int FrameRate(unsigned int* framerate) const override {
    return sender_.FrameRate(framerate);
  }

  int32_t SetChannelParameters(uint32_t target_bitrate,  // bits/s.
                               uint8_t lossRate,
                               int64_t rtt) override {
    return sender_.SetChannelParameters(target_bitrate, lossRate, rtt);
  }

  int32_t RegisterTransportCallback(
      VCMPacketizationCallback* transport) override {
    return sender_.RegisterTransportCallback(transport);
  }

  int32_t RegisterSendStatisticsCallback(
      VCMSendStatisticsCallback* sendStats) override {
    return sender_.RegisterSendStatisticsCallback(sendStats);
  }

  int32_t RegisterProtectionCallback(
      VCMProtectionCallback* protection) override {
    return sender_.RegisterProtectionCallback(protection);
  }

  int32_t SetVideoProtection(VCMVideoProtection videoProtection,
                             bool enable) override {
    // TODO(pbos): Remove enable from receive-side protection modes as well.
    if (enable)
      sender_.SetVideoProtection(videoProtection);
    return receiver_.SetVideoProtection(videoProtection, enable);
  }

  int32_t AddVideoFrame(const VideoFrame& videoFrame,
                        const VideoContentMetrics* contentMetrics,
                        const CodecSpecificInfo* codecSpecificInfo) override {
    return sender_.AddVideoFrame(videoFrame, contentMetrics, codecSpecificInfo);
  }

  int32_t IntraFrameRequest(int stream_index) override {
    return sender_.IntraFrameRequest(stream_index);
  }

  int32_t EnableFrameDropper(bool enable) override {
    return sender_.EnableFrameDropper(enable);
  }

  void SuspendBelowMinBitrate() override {
    return sender_.SuspendBelowMinBitrate();
  }

  bool VideoSuspended() const override { return sender_.VideoSuspended(); }

  int32_t RegisterReceiveCodec(const VideoCodec* receiveCodec,
                               int32_t numberOfCores,
                               bool requireKeyFrame) override {
    return receiver_.RegisterReceiveCodec(receiveCodec, numberOfCores,
                                          requireKeyFrame);
  }

  void RegisterExternalDecoder(VideoDecoder* externalDecoder,
                               uint8_t payloadType) override {
    receiver_.RegisterExternalDecoder(externalDecoder, payloadType);
  }

  int32_t RegisterReceiveCallback(
      VCMReceiveCallback* receiveCallback) override {
    return receiver_.RegisterReceiveCallback(receiveCallback);
  }

  int32_t RegisterReceiveStatisticsCallback(
      VCMReceiveStatisticsCallback* receiveStats) override {
    return receiver_.RegisterReceiveStatisticsCallback(receiveStats);
  }

  int32_t RegisterDecoderTimingCallback(
      VCMDecoderTimingCallback* decoderTiming) override {
    return receiver_.RegisterDecoderTimingCallback(decoderTiming);
  }

  int32_t RegisterFrameTypeCallback(
      VCMFrameTypeCallback* frameTypeCallback) override {
    return receiver_.RegisterFrameTypeCallback(frameTypeCallback);
  }

  int32_t RegisterPacketRequestCallback(
      VCMPacketRequestCallback* callback) override {
    return receiver_.RegisterPacketRequestCallback(callback);
  }

  int RegisterRenderBufferSizeCallback(
      VCMRenderBufferSizeCallback* callback) override {
    return receiver_.RegisterRenderBufferSizeCallback(callback);
  }

  int32_t Decode(uint16_t maxWaitTimeMs) override {
    return receiver_.Decode(maxWaitTimeMs);
  }

  int32_t ResetDecoder() override { return receiver_.ResetDecoder(); }

  int32_t ReceiveCodec(VideoCodec* currentReceiveCodec) const override {
    return receiver_.ReceiveCodec(currentReceiveCodec);
  }

  VideoCodecType ReceiveCodec() const override {
    return receiver_.ReceiveCodec();
  }

  int32_t IncomingPacket(const uint8_t* incomingPayload,
                         size_t payloadLength,
                         const WebRtcRTPHeader& rtpInfo) override {
    return receiver_.IncomingPacket(incomingPayload, payloadLength, rtpInfo);
  }

  int32_t SetMinimumPlayoutDelay(uint32_t minPlayoutDelayMs) override {
    return receiver_.SetMinimumPlayoutDelay(minPlayoutDelayMs);
  }

  int32_t SetRenderDelay(uint32_t timeMS) override {
    return receiver_.SetRenderDelay(timeMS);
  }

  int32_t Delay() const override { return receiver_.Delay(); }

  uint32_t DiscardedPackets() const override {
    return receiver_.DiscardedPackets();
  }

  int SetReceiverRobustnessMode(ReceiverRobustness robustnessMode,
                                VCMDecodeErrorMode errorMode) override {
    return receiver_.SetReceiverRobustnessMode(robustnessMode, errorMode);
  }

  void SetNackSettings(size_t max_nack_list_size,
                       int max_packet_age_to_nack,
                       int max_incomplete_time_ms) override {
    return receiver_.SetNackSettings(max_nack_list_size, max_packet_age_to_nack,
                                     max_incomplete_time_ms);
  }

  void SetDecodeErrorMode(VCMDecodeErrorMode decode_error_mode) override {
    return receiver_.SetDecodeErrorMode(decode_error_mode);
  }

  int SetMinReceiverDelay(int desired_delay_ms) override {
    return receiver_.SetMinReceiverDelay(desired_delay_ms);
  }

  int32_t SetReceiveChannelParameters(int64_t rtt) override {
    return receiver_.SetReceiveChannelParameters(rtt);
  }

  void RegisterPreDecodeImageCallback(EncodedImageCallback* observer) override {
    receiver_.RegisterPreDecodeImageCallback(observer);
  }

  void RegisterPostEncodeImageCallback(
      EncodedImageCallback* observer) override {
    post_encode_callback_.Register(observer);
  }

  void TriggerDecoderShutdown() override { receiver_.TriggerDecoderShutdown(); }

 private:
  EncodedImageCallbackWrapper post_encode_callback_;
  vcm::VideoSender sender_;
  vcm::VideoReceiver receiver_;
  rtc::scoped_ptr<EventFactory> own_event_factory_;
};
}  // namespace

void VideoCodingModule::Codec(VideoCodecType codecType, VideoCodec* codec) {
  VCMCodecDataBase::Codec(codecType, codec);
}

VideoCodingModule* VideoCodingModule::Create(
    Clock* clock,
    VideoEncoderRateObserver* encoder_rate_observer,
    VCMQMSettingsCallback* qm_settings_callback) {
  return new VideoCodingModuleImpl(clock, new EventFactoryImpl, true,
                                   encoder_rate_observer, qm_settings_callback);
}

VideoCodingModule* VideoCodingModule::Create(Clock* clock,
                                             EventFactory* event_factory) {
  assert(clock);
  assert(event_factory);
  return new VideoCodingModuleImpl(clock, event_factory, false, nullptr,
                                   nullptr);
}

void VideoCodingModule::Destroy(VideoCodingModule* module) {
  if (module != NULL) {
    delete static_cast<VideoCodingModuleImpl*>(module);
  }
}
}  // namespace webrtc
