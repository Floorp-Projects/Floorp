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

#include "webrtc/modules/video_coding/include/video_coding.h"

#include <vector>

#include "webrtc/base/thread_annotations.h"
#include "webrtc/base/thread_checker.h"
#include "webrtc/modules/video_coding/codec_database.h"
#include "webrtc/modules/video_coding/frame_buffer.h"
#include "webrtc/modules/video_coding/generic_decoder.h"
#include "webrtc/modules/video_coding/generic_encoder.h"
#include "webrtc/modules/video_coding/jitter_buffer.h"
#include "webrtc/modules/video_coding/media_optimization.h"
#include "webrtc/modules/video_coding/receiver.h"
#include "webrtc/modules/video_coding/timing.h"
#include "webrtc/modules/video_coding/utility/qp_parser.h"
#include "webrtc/system_wrappers/include/clock.h"
#include "webrtc/system_wrappers/include/critical_section_wrapper.h"

namespace webrtc {

class EncodedFrameObserver;

namespace vcm {

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
              VideoEncoderRateObserver* encoder_rate_observer,
              VCMQMSettingsCallback* qm_settings_callback);

  ~VideoSender();

  // Register the send codec to be used.
  // This method must be called on the construction thread.
  int32_t RegisterSendCodec(const VideoCodec* sendCodec,
                            uint32_t numberOfCores,
                            uint32_t maxPayloadSize);

  void RegisterExternalEncoder(VideoEncoder* externalEncoder,
                               uint8_t payloadType,
                               bool internalSource);

  int Bitrate(unsigned int* bitrate) const;
  int FrameRate(unsigned int* framerate) const;

  int32_t SetChannelParameters(uint32_t target_bitrate,  // bits/s.
                               uint8_t lossRate,
                               int64_t rtt);

  int32_t RegisterTransportCallback(VCMPacketizationCallback* transport);
  int32_t RegisterSendStatisticsCallback(VCMSendStatisticsCallback* sendStats);
  int32_t RegisterProtectionCallback(VCMProtectionCallback* protection);
  void SetVideoProtection(VCMVideoProtection videoProtection);

  int32_t AddVideoFrame(const VideoFrame& videoFrame,
                        const VideoContentMetrics* _contentMetrics,
                        const CodecSpecificInfo* codecSpecificInfo);

  int32_t IntraFrameRequest(int stream_index);
  int32_t EnableFrameDropper(bool enable);

  void SetCPULoadState(CPULoadState state);

  void SuspendBelowMinBitrate();
  bool VideoSuspended() const;

  int64_t TimeUntilNextProcess();
  int32_t Process();

 private:
  void SetEncoderParameters(EncoderParameters params)
      EXCLUSIVE_LOCKS_REQUIRED(send_crit_);

  Clock* const clock_;

  rtc::scoped_ptr<CriticalSectionWrapper> process_crit_sect_;
  mutable rtc::CriticalSection send_crit_;
  VCMGenericEncoder* _encoder;
  VCMEncodedFrameCallback _encodedFrameCallback;
  std::vector<FrameType> _nextFrameTypes;
  media_optimization::MediaOptimization _mediaOpt;
  VCMSendStatisticsCallback* _sendStatsCallback GUARDED_BY(process_crit_sect_);
  VCMCodecDataBase _codecDataBase GUARDED_BY(send_crit_);
  bool frame_dropper_enabled_ GUARDED_BY(send_crit_);
  VCMProcessTimer _sendStatsTimer;

  // Must be accessed on the construction thread of VideoSender.
  VideoCodec current_codec_;
  rtc::ThreadChecker main_thread_;

  VCMQMSettingsCallback* const qm_settings_callback_;
  VCMProtectionCallback* protection_callback_;

  rtc::CriticalSection params_lock_;
  EncoderParameters encoder_params_ GUARDED_BY(params_lock_);
};

class VideoReceiver {
 public:
  typedef VideoCodingModule::ReceiverRobustness ReceiverRobustness;

  VideoReceiver(Clock* clock, EventFactory* event_factory);
  ~VideoReceiver();

  void SetReceiveState(VideoReceiveState state);
  int32_t RegisterReceiveCodec(const VideoCodec* receiveCodec,
                               int32_t numberOfCores,
                               bool requireKeyFrame);

  void RegisterExternalDecoder(VideoDecoder* externalDecoder,
                               uint8_t payloadType);
  int32_t RegisterReceiveCallback(VCMReceiveCallback* receiveCallback);
  int32_t RegisterReceiveStatisticsCallback(
      VCMReceiveStatisticsCallback* receiveStats);
  int32_t RegisterDecoderTimingCallback(
      VCMDecoderTimingCallback* decoderTiming);
  int32_t RegisterFrameTypeCallback(VCMFrameTypeCallback* frameTypeCallback);
  int32_t RegisterPacketRequestCallback(VCMPacketRequestCallback* callback);
  int32_t RegisterReceiveStateCallback(VCMReceiveStateCallback* callback);
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

 private:
  Clock* const clock_;
  rtc::scoped_ptr<CriticalSectionWrapper> process_crit_sect_;
  CriticalSectionWrapper* _receiveCritSect;
  VideoReceiveState _receiveState;
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
  VCMReceiveStateCallback* _receiveStateCallback
      GUARDED_BY(process_crit_sect_);
  VCMRenderBufferSizeCallback* render_buffer_callback_
      GUARDED_BY(process_crit_sect_);
  VCMGenericDecoder* _decoder;
#ifdef DEBUG_DECODER_BIT_STREAM
  FILE* _bitStreamBeforeDecoder;
#endif
  VCMFrameBuffer _frameFromFile;
  bool _scheduleKeyRequest GUARDED_BY(process_crit_sect_);
  size_t max_nack_list_size_ GUARDED_BY(process_crit_sect_);
  EncodedImageCallback* pre_decode_image_callback_ GUARDED_BY(_receiveCritSect);

  VCMCodecDataBase _codecDataBase GUARDED_BY(_receiveCritSect);
  VCMProcessTimer _receiveStatsTimer;
  VCMProcessTimer _retransmissionTimer;
  VCMProcessTimer _keyRequestTimer;
  QpParser qp_parser_;
};

}  // namespace vcm
}  // namespace webrtc
#endif  // WEBRTC_MODULES_VIDEO_CODING_VIDEO_CODING_IMPL_H_
