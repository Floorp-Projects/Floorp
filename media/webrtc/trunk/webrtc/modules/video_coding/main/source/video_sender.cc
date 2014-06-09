/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/common_types.h"

#include <algorithm>  // std::max

#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/modules/video_coding/codecs/interface/video_codec_interface.h"
#include "webrtc/modules/video_coding/main/source/encoded_frame.h"
#include "webrtc/modules/video_coding/main/source/video_coding_impl.h"
#include "webrtc/system_wrappers/interface/clock.h"

namespace webrtc {
namespace vcm {

class DebugRecorder {
 public:
  DebugRecorder()
      : cs_(CriticalSectionWrapper::CreateCriticalSection()), file_(NULL) {}

  ~DebugRecorder() { Stop(); }

  int Start(const char* file_name_utf8) {
    CriticalSectionScoped cs(cs_.get());
    if (file_)
      fclose(file_);
    file_ = fopen(file_name_utf8, "wb");
    if (!file_)
      return VCM_GENERAL_ERROR;
    return VCM_OK;
  }

  void Stop() {
    CriticalSectionScoped cs(cs_.get());
    if (file_) {
      fclose(file_);
      file_ = NULL;
    }
  }

  void Add(const I420VideoFrame& frame) {
    CriticalSectionScoped cs(cs_.get());
    if (file_)
      PrintI420VideoFrame(frame, file_);
  }

 private:
  scoped_ptr<CriticalSectionWrapper> cs_;
  FILE* file_ GUARDED_BY(cs_);
};

VideoSender::VideoSender(const int32_t id,
                         Clock* clock,
                         EncodedImageCallback* post_encode_callback)
    : _id(id),
      clock_(clock),
      recorder_(new DebugRecorder()),
      process_crit_sect_(CriticalSectionWrapper::CreateCriticalSection()),
      _sendCritSect(CriticalSectionWrapper::CreateCriticalSection()),
      _encoder(),
      _encodedFrameCallback(post_encode_callback),
      _nextFrameTypes(1, kVideoFrameDelta),
      _mediaOpt(id, clock_),
      _sendStatsCallback(NULL),
      _codecDataBase(id),
      frame_dropper_enabled_(true),
      _sendStatsTimer(1000, clock_),
      qm_settings_callback_(NULL),
      protection_callback_(NULL) {}

VideoSender::~VideoSender() {
  delete _sendCritSect;
}

int32_t VideoSender::Process() {
  int32_t returnValue = VCM_OK;

  if (_sendStatsTimer.TimeUntilProcess() == 0) {
    _sendStatsTimer.Processed();
    CriticalSectionScoped cs(process_crit_sect_.get());
    if (_sendStatsCallback != NULL) {
      uint32_t bitRate;
      uint32_t frameRate;
      {
        CriticalSectionScoped cs(_sendCritSect);
        bitRate = _mediaOpt.SentBitRate();
        frameRate = _mediaOpt.SentFrameRate();
      }
      _sendStatsCallback->SendStatistics(bitRate, frameRate);
    }
  }

  return returnValue;
}

// Reset send side to initial state - all components
int32_t VideoSender::InitializeSender() {
  CriticalSectionScoped cs(_sendCritSect);
  _codecDataBase.ResetSender();
  _encoder = NULL;
  _encodedFrameCallback.SetTransportCallback(NULL);
  _mediaOpt.Reset();  // Resetting frame dropper
  return VCM_OK;
}

int32_t VideoSender::TimeUntilNextProcess() {
  return _sendStatsTimer.TimeUntilProcess();
}

// Register the send codec to be used.
int32_t VideoSender::RegisterSendCodec(const VideoCodec* sendCodec,
                                       uint32_t numberOfCores,
                                       uint32_t maxPayloadSize) {
  CriticalSectionScoped cs(_sendCritSect);
  if (sendCodec == NULL) {
    return VCM_PARAMETER_ERROR;
  }

  bool ret = _codecDataBase.SetSendCodec(
      sendCodec, numberOfCores, maxPayloadSize, &_encodedFrameCallback);

  // Update encoder regardless of result to make sure that we're not holding on
  // to a deleted instance.
  _encoder = _codecDataBase.GetEncoder();

  if (!ret) {
    WEBRTC_TRACE(webrtc::kTraceError,
                 webrtc::kTraceVideoCoding,
                 VCMId(_id),
                 "Failed to initialize encoder");
    return VCM_CODEC_ERROR;
  }

  int numLayers = (sendCodec->codecType != kVideoCodecVP8)
                      ? 1
                      : sendCodec->codecSpecific.VP8.numberOfTemporalLayers;
  // If we have screensharing and we have layers, we disable frame dropper.
  bool disable_frame_dropper =
      numLayers > 1 && sendCodec->mode == kScreensharing;
  if (disable_frame_dropper) {
    _mediaOpt.EnableFrameDropper(false);
  } else if (frame_dropper_enabled_) {
    _mediaOpt.EnableFrameDropper(true);
  }
  _nextFrameTypes.clear();
  _nextFrameTypes.resize(VCM_MAX(sendCodec->numberOfSimulcastStreams, 1),
                         kVideoFrameDelta);

  _mediaOpt.SetEncodingData(sendCodec->codecType,
                            sendCodec->maxBitrate * 1000,
                            sendCodec->maxFramerate * 1000,
                            sendCodec->startBitrate * 1000,
                            sendCodec->width,
                            sendCodec->height,
                            numLayers,
                            maxPayloadSize);
  return VCM_OK;
}

// Get current send codec
int32_t VideoSender::SendCodec(VideoCodec* currentSendCodec) const {
  CriticalSectionScoped cs(_sendCritSect);

  if (currentSendCodec == NULL) {
    return VCM_PARAMETER_ERROR;
  }
  return _codecDataBase.SendCodec(currentSendCodec) ? 0 : -1;
}

// Get the current send codec type
VideoCodecType VideoSender::SendCodec() const {
  CriticalSectionScoped cs(_sendCritSect);

  return _codecDataBase.SendCodec();
}

// Register an external decoder object.
// This can not be used together with external decoder callbacks.
int32_t VideoSender::RegisterExternalEncoder(VideoEncoder* externalEncoder,
                                             uint8_t payloadType,
                                             bool internalSource /*= false*/) {
  CriticalSectionScoped cs(_sendCritSect);

  if (externalEncoder == NULL) {
    bool wasSendCodec = false;
    const bool ret =
        _codecDataBase.DeregisterExternalEncoder(payloadType, &wasSendCodec);
    if (wasSendCodec) {
      // Make sure the VCM doesn't use the de-registered codec
      _encoder = NULL;
    }
    return ret ? 0 : -1;
  }
  _codecDataBase.RegisterExternalEncoder(
      externalEncoder, payloadType, internalSource);
  return 0;
}

// Get codec config parameters
int32_t VideoSender::CodecConfigParameters(uint8_t* buffer,
                                           int32_t size) const {
  CriticalSectionScoped cs(_sendCritSect);
  if (_encoder != NULL) {
    return _encoder->CodecConfigParameters(buffer, size);
  }
  return VCM_UNINITIALIZED;
}

// TODO(andresp): Make const once media_opt is thread-safe and this has a
// pointer to it.
int32_t VideoSender::SentFrameCount(VCMFrameCount* frameCount) {
  CriticalSectionScoped cs(_sendCritSect);
  *frameCount = _mediaOpt.SentFrameCount();
  return VCM_OK;
}

// Get encode bitrate
int VideoSender::Bitrate(unsigned int* bitrate) const {
  CriticalSectionScoped cs(_sendCritSect);
  // return the bit rate which the encoder is set to
  if (!_encoder) {
    return VCM_UNINITIALIZED;
  }
  *bitrate = _encoder->BitRate();
  return 0;
}

// Get encode frame rate
int VideoSender::FrameRate(unsigned int* framerate) const {
  CriticalSectionScoped cs(_sendCritSect);
  // input frame rate, not compensated
  if (!_encoder) {
    return VCM_UNINITIALIZED;
  }
  *framerate = _encoder->FrameRate();
  return 0;
}

// Set channel parameters
int32_t VideoSender::SetChannelParameters(uint32_t target_bitrate,
                                          uint8_t lossRate,
                                          uint32_t rtt) {
  int32_t ret = 0;
  {
    CriticalSectionScoped sendCs(_sendCritSect);
    uint32_t targetRate = _mediaOpt.SetTargetRates(target_bitrate,
                                                   lossRate,
                                                   rtt,
                                                   protection_callback_,
                                                   qm_settings_callback_);
    if (_encoder != NULL) {
      ret = _encoder->SetChannelParameters(lossRate, rtt);
      if (ret < 0) {
        return ret;
      }
      ret = (int32_t)_encoder->SetRates(targetRate, _mediaOpt.InputFrameRate());
      if (ret < 0) {
        return ret;
      }
    } else {
      return VCM_UNINITIALIZED;
    }  // encoder
  }    // send side
  return VCM_OK;
}

int32_t VideoSender::RegisterTransportCallback(
    VCMPacketizationCallback* transport) {
  CriticalSectionScoped cs(_sendCritSect);
  _encodedFrameCallback.SetMediaOpt(&_mediaOpt);
  _encodedFrameCallback.SetTransportCallback(transport);
  return VCM_OK;
}

// Register video output information callback which will be called to deliver
// information about the video stream produced by the encoder, for instance the
// average frame rate and bit rate.
int32_t VideoSender::RegisterSendStatisticsCallback(
    VCMSendStatisticsCallback* sendStats) {
  CriticalSectionScoped cs(process_crit_sect_.get());
  _sendStatsCallback = sendStats;
  return VCM_OK;
}

// Register a video quality settings callback which will be called when frame
// rate/dimensions need to be updated for video quality optimization
int32_t VideoSender::RegisterVideoQMCallback(
    VCMQMSettingsCallback* qm_settings_callback) {
  CriticalSectionScoped cs(_sendCritSect);
  qm_settings_callback_ = qm_settings_callback;
  _mediaOpt.EnableQM(qm_settings_callback_ != NULL);
  return VCM_OK;
}

// Register a video protection callback which will be called to deliver the
// requested FEC rate and NACK status (on/off).
int32_t VideoSender::RegisterProtectionCallback(
    VCMProtectionCallback* protection_callback) {
  CriticalSectionScoped cs(_sendCritSect);
  protection_callback_ = protection_callback;
  return VCM_OK;
}

// Enable or disable a video protection method.
// Note: This API should be deprecated, as it does not offer a distinction
// between the protection method and decoding with or without errors. If such a
// behavior is desired, use the following API: SetReceiverRobustnessMode.
int32_t VideoSender::SetVideoProtection(VCMVideoProtection videoProtection,
                                        bool enable) {
  switch (videoProtection) {
    case kProtectionNack:
    case kProtectionNackSender: {
      CriticalSectionScoped cs(_sendCritSect);
      _mediaOpt.EnableProtectionMethod(enable, media_optimization::kNack);
      break;
    }

    case kProtectionNackFEC: {
      CriticalSectionScoped cs(_sendCritSect);
      _mediaOpt.EnableProtectionMethod(enable, media_optimization::kNackFec);
      break;
    }

    case kProtectionFEC: {
      CriticalSectionScoped cs(_sendCritSect);
      _mediaOpt.EnableProtectionMethod(enable, media_optimization::kFec);
      break;
    }

    case kProtectionPeriodicKeyFrames: {
      CriticalSectionScoped cs(_sendCritSect);
      return _codecDataBase.SetPeriodicKeyFrames(enable) ? 0 : -1;
      break;
    }
    case kProtectionNackReceiver:
    case kProtectionDualDecoder:
    case kProtectionKeyOnLoss:
    case kProtectionKeyOnKeyLoss:
      // Ignore decoder modes.
      return VCM_OK;
  }
  return VCM_OK;
}
// Add one raw video frame to the encoder, blocking.
int32_t VideoSender::AddVideoFrame(const I420VideoFrame& videoFrame,
                                   const VideoContentMetrics* contentMetrics,
                                   const CodecSpecificInfo* codecSpecificInfo) {
  CriticalSectionScoped cs(_sendCritSect);
  if (_encoder == NULL) {
    return VCM_UNINITIALIZED;
  }
  // TODO(holmer): Add support for dropping frames per stream. Currently we
  // only have one frame dropper for all streams.
  if (_nextFrameTypes[0] == kFrameEmpty) {
    return VCM_OK;
  }
  if (_mediaOpt.DropFrame()) {
    WEBRTC_TRACE(webrtc::kTraceStream,
                 webrtc::kTraceVideoCoding,
                 VCMId(_id),
                 "Drop frame due to bitrate");
  } else {
    _mediaOpt.UpdateContentData(contentMetrics);
    int32_t ret =
        _encoder->Encode(videoFrame, codecSpecificInfo, _nextFrameTypes);
    recorder_->Add(videoFrame);
    if (ret < 0) {
      WEBRTC_TRACE(webrtc::kTraceError,
                   webrtc::kTraceVideoCoding,
                   VCMId(_id),
                   "Encode error: %d",
                   ret);
      return ret;
    }
    for (size_t i = 0; i < _nextFrameTypes.size(); ++i) {
      _nextFrameTypes[i] = kVideoFrameDelta;  // Default frame type.
    }
  }
  return VCM_OK;
}

int32_t VideoSender::IntraFrameRequest(int stream_index) {
  CriticalSectionScoped cs(_sendCritSect);
  if (stream_index < 0 ||
      static_cast<unsigned int>(stream_index) >= _nextFrameTypes.size()) {
    return -1;
  }
  _nextFrameTypes[stream_index] = kVideoFrameKey;
  if (_encoder != NULL && _encoder->InternalSource()) {
    // Try to request the frame if we have an external encoder with
    // internal source since AddVideoFrame never will be called.
    if (_encoder->RequestFrame(_nextFrameTypes) == WEBRTC_VIDEO_CODEC_OK) {
      _nextFrameTypes[stream_index] = kVideoFrameDelta;
    }
  }
  return VCM_OK;
}

int32_t VideoSender::EnableFrameDropper(bool enable) {
  CriticalSectionScoped cs(_sendCritSect);
  frame_dropper_enabled_ = enable;
  _mediaOpt.EnableFrameDropper(enable);
  return VCM_OK;
}

int VideoSender::SetSenderNackMode(SenderNackMode mode) {
  CriticalSectionScoped cs(_sendCritSect);

  switch (mode) {
    case VideoCodingModule::kNackNone:
      _mediaOpt.EnableProtectionMethod(false, media_optimization::kNack);
      break;
    case VideoCodingModule::kNackAll:
      _mediaOpt.EnableProtectionMethod(true, media_optimization::kNack);
      break;
    case VideoCodingModule::kNackSelective:
      return VCM_NOT_IMPLEMENTED;
      break;
  }
  return VCM_OK;
}

int VideoSender::SetSenderReferenceSelection(bool enable) {
  return VCM_NOT_IMPLEMENTED;
}

int VideoSender::SetSenderFEC(bool enable) {
  CriticalSectionScoped cs(_sendCritSect);
  _mediaOpt.EnableProtectionMethod(enable, media_optimization::kFec);
  return VCM_OK;
}

int VideoSender::SetSenderKeyFramePeriod(int periodMs) {
  return VCM_NOT_IMPLEMENTED;
}

int VideoSender::StartDebugRecording(const char* file_name_utf8) {
  return recorder_->Start(file_name_utf8);
}

void VideoSender::StopDebugRecording() {
  recorder_->Stop();
}

void VideoSender::SuspendBelowMinBitrate() {
  CriticalSectionScoped cs(_sendCritSect);
  VideoCodec current_send_codec;
  if (SendCodec(&current_send_codec) != 0) {
    assert(false);  // Must set a send codec before SuspendBelowMinBitrate.
    return;
  }
  int threshold_bps;
  if (current_send_codec.numberOfSimulcastStreams == 0) {
    threshold_bps = current_send_codec.minBitrate * 1000;
  } else {
    threshold_bps = current_send_codec.simulcastStream[0].minBitrate * 1000;
  }
  // Set the hysteresis window to be at 10% of the threshold, but at least
  // 10 kbps.
  int window_bps = std::max(threshold_bps / 10, 10000);
  _mediaOpt.SuspendBelowMinBitrate(threshold_bps, window_bps);
}

bool VideoSender::VideoSuspended() const {
  CriticalSectionScoped cs(_sendCritSect);
  return _mediaOpt.IsVideoSuspended();
}

void VideoSender::SetCPULoadState(CPULoadState state) {
  CriticalSectionScoped cs(_sendCritSect);
  _mediaOpt.SetCPULoadState(state);
}

}  // namespace vcm
}  // namespace webrtc
