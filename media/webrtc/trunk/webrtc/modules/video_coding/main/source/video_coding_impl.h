/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_VIDEO_CODING_IMPL_H_
#define WEBRTC_MODULES_VIDEO_CODING_VIDEO_CODING_IMPL_H_

#include "webrtc/modules/video_coding/main/interface/video_coding.h"

#include <vector>

#include "webrtc/base/thread_annotations.h"
#include "webrtc/base/thread_checker.h"
#include "webrtc/modules/video_coding/main/source/codec_database.h"
#include "webrtc/modules/video_coding/main/source/frame_buffer.h"
#include "webrtc/modules/video_coding/main/source/generic_decoder.h"
#include "webrtc/modules/video_coding/main/source/generic_encoder.h"
#include "webrtc/modules/video_coding/main/source/jitter_buffer.h"
#include "webrtc/modules/video_coding/main/source/media_optimization.h"
#include "webrtc/modules/video_coding/main/source/receiver.h"
#include "webrtc/modules/video_coding/main/source/timing.h"
#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"

namespace webrtc {

class EncodedFrameObserver;

namespace vcm {

class DebugRecorder;

class VCMProcessTimer {
 public:
  VCMProcessTimer(int64_t periodMs, Clock* clock)
      : _clock(clock),
        _periodMs(periodMs),
        _latestMs(_clock->TimeInMilliseconds()) {}
  int64_t Period() const;
  int64_t TimeUntilProcess() const;
  void Processed();

 private:
  Clock* _clock;
  int64_t _periodMs;
  int64_t _latestMs;
};

class VideoSender {
 public:
  typedef VideoCodingModule::SenderNackMode SenderNackMode;

  VideoSender(Clock* clock,
              EncodedImageCallback* post_encode_callback,
              VideoEncoderRateObserver* encoder_rate_observer);

  ~VideoSender();

  int32_t InitializeSender();

  // Register the send codec to be used.
  // This method must be called on the construction thread.
  int32_t RegisterSendCodec(const VideoCodec* sendCodec,
                            uint32_t numberOfCores,
                            uint32_t maxPayloadSize);
  // Non-blocking access to the currently active send codec configuration.
  // Must be called from the same thread as the VideoSender instance was
  // created on.
  const VideoCodec& GetSendCodec() const;

  // Get a copy of the currently configured send codec.
  // This method acquires a lock to copy the current configuration out,
  // so it can block and the returned information is not guaranteed to be
  // accurate upon return.  Consider using GetSendCodec() instead and make
  // decisions on that thread with regards to the current codec.
  int32_t SendCodecBlocking(VideoCodec* currentSendCodec) const;

  // Same as SendCodecBlocking.  Try to use GetSendCodec() instead.
  VideoCodecType SendCodecBlocking() const;

  int32_t RegisterExternalEncoder(VideoEncoder* externalEncoder,
                                  uint8_t payloadType,
                                  bool internalSource);

  int32_t CodecConfigParameters(uint8_t* buffer, int32_t size) const;
  int32_t SentFrameCount(VCMFrameCount* frameCount);
  int Bitrate(unsigned int* bitrate) const;
  int FrameRate(unsigned int* framerate) const;

  int32_t SetChannelParameters(uint32_t target_bitrate,  // bits/s.
                               uint8_t lossRate,
                               int64_t rtt);

  int32_t RegisterTransportCallback(VCMPacketizationCallback* transport);
  int32_t RegisterSendStatisticsCallback(VCMSendStatisticsCallback* sendStats);
  int32_t RegisterVideoQMCallback(VCMQMSettingsCallback* videoQMSettings);
  int32_t RegisterProtectionCallback(VCMProtectionCallback* protection);
  void SetVideoProtection(bool enable, VCMVideoProtection videoProtection);

  int32_t AddVideoFrame(const I420VideoFrame& videoFrame,
                        const VideoContentMetrics* _contentMetrics,
                        const CodecSpecificInfo* codecSpecificInfo);

  int32_t IntraFrameRequest(int stream_index);
  int32_t EnableFrameDropper(bool enable);

  int StartDebugRecording(const char* file_name_utf8);
  void StopDebugRecording();

  void SuspendBelowMinBitrate();
  bool VideoSuspended() const;

  int64_t TimeUntilNextProcess();
  int32_t Process();

 private:
  Clock* clock_;

  rtc::scoped_ptr<DebugRecorder> recorder_;

  rtc::scoped_ptr<CriticalSectionWrapper> process_crit_sect_;
  CriticalSectionWrapper* _sendCritSect;
  VCMGenericEncoder* _encoder;
  VCMEncodedFrameCallback _encodedFrameCallback;
  std::vector<FrameType> _nextFrameTypes;
  media_optimization::MediaOptimization _mediaOpt;
  VCMSendStatisticsCallback* _sendStatsCallback;
  VCMCodecDataBase _codecDataBase;
  bool frame_dropper_enabled_;
  VCMProcessTimer _sendStatsTimer;

  // Must be accessed on the construction thread of VideoSender.
  VideoCodec current_codec_;
  rtc::ThreadChecker main_thread_;

  VCMQMSettingsCallback* qm_settings_callback_;
  VCMProtectionCallback* protection_callback_;
};

class VideoReceiver {
 public:
  typedef VideoCodingModule::ReceiverRobustness ReceiverRobustness;

  VideoReceiver(Clock* clock, EventFactory* event_factory);
  ~VideoReceiver();

  int32_t InitializeReceiver();
  int32_t RegisterReceiveCodec(const VideoCodec* receiveCodec,
                               int32_t numberOfCores,
                               bool requireKeyFrame);

  int32_t RegisterExternalDecoder(VideoDecoder* externalDecoder,
                                  uint8_t payloadType,
                                  bool internalRenderTiming);
  int32_t RegisterReceiveCallback(VCMReceiveCallback* receiveCallback);
  int32_t RegisterReceiveStatisticsCallback(
      VCMReceiveStatisticsCallback* receiveStats);
  int32_t RegisterDecoderTimingCallback(
      VCMDecoderTimingCallback* decoderTiming);
  int32_t RegisterFrameTypeCallback(VCMFrameTypeCallback* frameTypeCallback);
  int32_t RegisterPacketRequestCallback(VCMPacketRequestCallback* callback);
  int RegisterRenderBufferSizeCallback(VCMRenderBufferSizeCallback* callback);

  int32_t Decode(uint16_t maxWaitTimeMs);
  int32_t ResetDecoder();

  int32_t ReceiveCodec(VideoCodec* currentReceiveCodec) const;
  VideoCodecType ReceiveCodec() const;

  int32_t IncomingPacket(const uint8_t* incomingPayload,
                         size_t payloadLength,
                         const WebRtcRTPHeader& rtpInfo);
  int32_t SetMinimumPlayoutDelay(uint32_t minPlayoutDelayMs);
  int32_t SetRenderDelay(uint32_t timeMS);
  int32_t Delay() const;
  uint32_t DiscardedPackets() const;

  int SetReceiverRobustnessMode(ReceiverRobustness robustnessMode,
                                VCMDecodeErrorMode errorMode);
  void SetNackSettings(size_t max_nack_list_size,
                       int max_packet_age_to_nack,
                       int max_incomplete_time_ms);

  void SetDecodeErrorMode(VCMDecodeErrorMode decode_error_mode);
  int SetMinReceiverDelay(int desired_delay_ms);

  int32_t SetReceiveChannelParameters(int64_t rtt);
  int32_t SetVideoProtection(VCMVideoProtection videoProtection, bool enable);

  int64_t TimeUntilNextProcess();
  int32_t Process();

  void RegisterPreDecodeImageCallback(EncodedImageCallback* observer);
  void TriggerDecoderShutdown();

 protected:
  int32_t Decode(const webrtc::VCMEncodedFrame& frame)
      EXCLUSIVE_LOCKS_REQUIRED(_receiveCritSect);
  int32_t RequestKeyFrame();
  int32_t RequestSliceLossIndication(const uint64_t pictureID) const;
  int32_t NackList(uint16_t* nackList, uint16_t* size);

 private:
  enum VCMKeyRequestMode {
    kKeyOnError,    // Normal mode, request key frames on decoder error
    kKeyOnKeyLoss,  // Request key frames on decoder error and on packet loss
                    // in key frames.
    kKeyOnLoss,     // Request key frames on decoder error and on packet loss
                    // in any frame
  };

  Clock* const clock_;
  rtc::scoped_ptr<CriticalSectionWrapper> process_crit_sect_;
  CriticalSectionWrapper* _receiveCritSect;
  VCMTiming _timing;
  VCMReceiver _receiver;
  VCMDecodedFrameCallback _decodedFrameCallback;
  VCMFrameTypeCallback* _frameTypeCallback GUARDED_BY(process_crit_sect_);
  VCMReceiveStatisticsCallback* _receiveStatsCallback
      GUARDED_BY(process_crit_sect_);
  VCMDecoderTimingCallback* _decoderTimingCallback
      GUARDED_BY(process_crit_sect_);
  VCMPacketRequestCallback* _packetRequestCallback
      GUARDED_BY(process_crit_sect_);
  VCMRenderBufferSizeCallback* render_buffer_callback_
      GUARDED_BY(process_crit_sect_);
  VCMGenericDecoder* _decoder;
#ifdef DEBUG_DECODER_BIT_STREAM
  FILE* _bitStreamBeforeDecoder;
#endif
  VCMFrameBuffer _frameFromFile;
  VCMKeyRequestMode _keyRequestMode;
  bool _scheduleKeyRequest GUARDED_BY(process_crit_sect_);
  size_t max_nack_list_size_ GUARDED_BY(process_crit_sect_);
  EncodedImageCallback* pre_decode_image_callback_ GUARDED_BY(_receiveCritSect);

  VCMCodecDataBase _codecDataBase GUARDED_BY(_receiveCritSect);
  VCMProcessTimer _receiveStatsTimer;
  VCMProcessTimer _retransmissionTimer;
  VCMProcessTimer _keyRequestTimer;
};

}  // namespace vcm
}  // namespace webrtc
#endif  // WEBRTC_MODULES_VIDEO_CODING_VIDEO_CODING_IMPL_H_
