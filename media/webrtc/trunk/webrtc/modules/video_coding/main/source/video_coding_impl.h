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
  VCMProcessTimer(uint32_t periodMs, Clock* clock)
      : _clock(clock),
        _periodMs(periodMs),
        _latestMs(_clock->TimeInMilliseconds()) {}
  uint32_t Period() const;
  uint32_t TimeUntilProcess() const;
  void Processed();

 private:
  Clock* _clock;
  uint32_t _periodMs;
  int64_t _latestMs;
};

class VideoSender {
 public:
  typedef VideoCodingModule::SenderNackMode SenderNackMode;

  VideoSender(const int32_t id,
              Clock* clock,
              EncodedImageCallback* post_encode_callback);

  ~VideoSender();

  int32_t InitializeSender();

  // Register the send codec to be used.
  int32_t RegisterSendCodec(const VideoCodec* sendCodec,
                            uint32_t numberOfCores,
                            uint32_t maxPayloadSize);

  int32_t SendCodec(VideoCodec* currentSendCodec) const;
  VideoCodecType SendCodec() const;
  int32_t RegisterExternalEncoder(VideoEncoder* externalEncoder,
                                  uint8_t payloadType,
                                  bool internalSource);

  int32_t CodecConfigParameters(uint8_t* buffer, int32_t size) const;
  int32_t SentFrameCount(VCMFrameCount* frameCount);
  int Bitrate(unsigned int* bitrate) const;
  int FrameRate(unsigned int* framerate) const;

  int32_t SetChannelParameters(uint32_t target_bitrate,  // bits/s.
                               uint8_t lossRate,
                               uint32_t rtt);

  int32_t RegisterTransportCallback(VCMPacketizationCallback* transport);
  int32_t RegisterSendStatisticsCallback(VCMSendStatisticsCallback* sendStats);
  int32_t RegisterVideoQMCallback(VCMQMSettingsCallback* videoQMSettings);
  int32_t RegisterProtectionCallback(VCMProtectionCallback* protection);
  int32_t SetVideoProtection(VCMVideoProtection videoProtection, bool enable);

  int32_t AddVideoFrame(const I420VideoFrame& videoFrame,
                        const VideoContentMetrics* _contentMetrics,
                        const CodecSpecificInfo* codecSpecificInfo);

  int32_t IntraFrameRequest(int stream_index);
  int32_t EnableFrameDropper(bool enable);

  int SetSenderNackMode(SenderNackMode mode);
  int SetSenderReferenceSelection(bool enable);
  int SetSenderFEC(bool enable);
  int SetSenderKeyFramePeriod(int periodMs);

  void SetCPULoadState(CPULoadState state);

  int StartDebugRecording(const char* file_name_utf8);
  void StopDebugRecording();

  void SuspendBelowMinBitrate();
  bool VideoSuspended() const;

  int32_t TimeUntilNextProcess();
  int32_t Process();

 private:
  int32_t _id;
  Clock* clock_;

  scoped_ptr<DebugRecorder> recorder_;

  scoped_ptr<CriticalSectionWrapper> process_crit_sect_;
  CriticalSectionWrapper* _sendCritSect;
  VCMGenericEncoder* _encoder;
  VCMEncodedFrameCallback _encodedFrameCallback;
  std::vector<FrameType> _nextFrameTypes;
  media_optimization::MediaOptimization _mediaOpt;
  VCMSendStatisticsCallback* _sendStatsCallback;
  VCMCodecDataBase _codecDataBase;
  bool frame_dropper_enabled_;
  VCMProcessTimer _sendStatsTimer;

  VCMQMSettingsCallback* qm_settings_callback_;
  VCMProtectionCallback* protection_callback_;
};

class VideoReceiver {
 public:
  typedef VideoCodingModule::ReceiverRobustness ReceiverRobustness;

  VideoReceiver(const int32_t id, Clock* clock, EventFactory* event_factory);
  ~VideoReceiver();

  int32_t InitializeReceiver();
  void SetReceiveState(VideoReceiveState state);
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
  int32_t RegisterReceiveStateCallback(VCMReceiveStateCallback* callback);
  int RegisterRenderBufferSizeCallback(VCMRenderBufferSizeCallback* callback);

  int32_t Decode(uint16_t maxWaitTimeMs);
  int32_t DecodeDualFrame(uint16_t maxWaitTimeMs);
  int32_t ResetDecoder();

  int32_t ReceiveCodec(VideoCodec* currentReceiveCodec) const;
  VideoCodecType ReceiveCodec() const;

  int32_t IncomingPacket(const uint8_t* incomingPayload,
                         uint32_t payloadLength,
                         const WebRtcRTPHeader& rtpInfo);
  int32_t SetMinimumPlayoutDelay(uint32_t minPlayoutDelayMs);
  int32_t SetRenderDelay(uint32_t timeMS);
  int32_t Delay() const;
  int32_t ReceivedFrameCount(VCMFrameCount* frameCount) const;
  uint32_t DiscardedPackets() const;

  int SetReceiverRobustnessMode(ReceiverRobustness robustnessMode,
                                VCMDecodeErrorMode errorMode);
  void SetNackSettings(size_t max_nack_list_size,
                       int max_packet_age_to_nack,
                       int max_incomplete_time_ms);

  void SetDecodeErrorMode(VCMDecodeErrorMode decode_error_mode);
  int SetMinReceiverDelay(int desired_delay_ms);

  int32_t SetReceiveChannelParameters(uint32_t rtt);
  int32_t SetVideoProtection(VCMVideoProtection videoProtection, bool enable);

  int32_t TimeUntilNextProcess();
  int32_t Process();

  void RegisterPreDecodeImageCallback(EncodedImageCallback* observer);

 protected:
  int32_t Decode(const webrtc::VCMEncodedFrame& frame);
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

  int32_t _id;
  Clock* clock_;
  scoped_ptr<CriticalSectionWrapper> process_crit_sect_;
  CriticalSectionWrapper* _receiveCritSect;
  bool _receiverInited;
  VideoReceiveState _receiveState;
  VCMTiming _timing;
  VCMTiming _dualTiming;
  VCMReceiver _receiver;
  VCMReceiver _dualReceiver;
  VCMDecodedFrameCallback _decodedFrameCallback;
  VCMDecodedFrameCallback _dualDecodedFrameCallback;
  VCMFrameTypeCallback* _frameTypeCallback;
  VCMReceiveStatisticsCallback* _receiveStatsCallback;
  VCMDecoderTimingCallback* _decoderTimingCallback;
  VCMPacketRequestCallback* _packetRequestCallback;
  VCMReceiveStateCallback* _receiveStateCallback;
  VCMRenderBufferSizeCallback* render_buffer_callback_;
  VCMGenericDecoder* _decoder;
  VCMGenericDecoder* _dualDecoder;
#ifdef DEBUG_DECODER_BIT_STREAM
  FILE* _bitStreamBeforeDecoder;
#endif
  VCMFrameBuffer _frameFromFile;
  VCMKeyRequestMode _keyRequestMode;
  bool _scheduleKeyRequest;
  size_t max_nack_list_size_;
  EncodedImageCallback* pre_decode_image_callback_;

  VCMCodecDataBase _codecDataBase;
  VCMProcessTimer _receiveStatsTimer;
  VCMProcessTimer _retransmissionTimer;
  VCMProcessTimer _keyRequestTimer;
};

}  // namespace vcm
}  // namespace webrtc
#endif  // WEBRTC_MODULES_VIDEO_CODING_VIDEO_CODING_IMPL_H_
