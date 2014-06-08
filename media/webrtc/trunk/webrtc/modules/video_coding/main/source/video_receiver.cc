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
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/modules/video_coding/codecs/interface/video_codec_interface.h"
#include "webrtc/modules/video_coding/main/source/encoded_frame.h"
#include "webrtc/modules/video_coding/main/source/jitter_buffer.h"
#include "webrtc/modules/video_coding/main/source/packet.h"
#include "webrtc/modules/video_coding/main/source/video_coding_impl.h"
#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/system_wrappers/interface/trace.h"
#include "webrtc/system_wrappers/interface/trace_event.h"

// #define DEBUG_DECODER_BIT_STREAM

namespace webrtc {
namespace vcm {

VideoReceiver::VideoReceiver(const int32_t id,
                             Clock* clock,
                             EventFactory* event_factory)
    : _id(id),
      clock_(clock),
      process_crit_sect_(CriticalSectionWrapper::CreateCriticalSection()),
      _receiveCritSect(CriticalSectionWrapper::CreateCriticalSection()),
      _receiverInited(false),
      _receiveState(kReceiveStateInitial),
      _timing(clock_, id, 1),
      _dualTiming(clock_, id, 2, &_timing),
      _receiver(&_timing, clock_, event_factory, id, 1, true),
      _dualReceiver(&_dualTiming, clock_, event_factory, id, 2, false),
      _decodedFrameCallback(_timing, clock_),
      _dualDecodedFrameCallback(_dualTiming, clock_),
      _frameTypeCallback(NULL),
      _receiveStatsCallback(NULL),
      _decoderTimingCallback(NULL),
      _packetRequestCallback(NULL),
      _receiveStateCallback(NULL),
      render_buffer_callback_(NULL),
      _decoder(NULL),
      _dualDecoder(NULL),
#ifdef DEBUG_DECODER_BIT_STREAM
      _bitStreamBeforeDecoder(NULL),
#endif
      _frameFromFile(),
      _keyRequestMode(kKeyOnError),
      _scheduleKeyRequest(false),
      max_nack_list_size_(0),
      pre_decode_image_callback_(NULL),
      _codecDataBase(id),
      _receiveStatsTimer(1000, clock_),
      _retransmissionTimer(10, clock_),
      _keyRequestTimer(500, clock_) {
  assert(clock_);
#ifdef DEBUG_DECODER_BIT_STREAM
  _bitStreamBeforeDecoder = fopen("decoderBitStream.bit", "wb");
#endif
}

VideoReceiver::~VideoReceiver() {
  if (_dualDecoder != NULL) {
    _codecDataBase.ReleaseDecoder(_dualDecoder);
  }
  delete _receiveCritSect;
#ifdef DEBUG_DECODER_BIT_STREAM
  fclose(_bitStreamBeforeDecoder);
#endif
}

int32_t VideoReceiver::Process() {
  int32_t returnValue = VCM_OK;

  // Receive-side statistics
  if (_receiveStatsTimer.TimeUntilProcess() == 0) {
    _receiveStatsTimer.Processed();
    CriticalSectionScoped cs(process_crit_sect_.get());
    if (_receiveStatsCallback != NULL) {
      uint32_t bitRate;
      uint32_t frameRate;
      _receiver.ReceiveStatistics(&bitRate, &frameRate);
      _receiveStatsCallback->OnReceiveStatisticsUpdate(bitRate, frameRate);
    }

    if (_decoderTimingCallback != NULL) {
      int decode_ms;
      int max_decode_ms;
      int current_delay_ms;
      int target_delay_ms;
      int jitter_buffer_ms;
      int min_playout_delay_ms;
      int render_delay_ms;
      _timing.GetTimings(&decode_ms,
                         &max_decode_ms,
                         &current_delay_ms,
                         &target_delay_ms,
                         &jitter_buffer_ms,
                         &min_playout_delay_ms,
                         &render_delay_ms);
      _decoderTimingCallback->OnDecoderTiming(decode_ms,
                                              max_decode_ms,
                                              current_delay_ms,
                                              target_delay_ms,
                                              jitter_buffer_ms,
                                              min_playout_delay_ms,
                                              render_delay_ms);
    }

    // Size of render buffer.
    if (render_buffer_callback_) {
      int buffer_size_ms = _receiver.RenderBufferSizeMs();
      render_buffer_callback_->RenderBufferSizeMs(buffer_size_ms);
    }
  }

  // Key frame requests
  if (_keyRequestTimer.TimeUntilProcess() == 0) {
    _keyRequestTimer.Processed();
    bool request_key_frame = false;
    {
      CriticalSectionScoped cs(process_crit_sect_.get());
      request_key_frame = _scheduleKeyRequest && _frameTypeCallback != NULL;
    }
    if (request_key_frame) {
      const int32_t ret = RequestKeyFrame();
      if (ret != VCM_OK && returnValue == VCM_OK) {
        returnValue = ret;
      }
    }
  }

  // Packet retransmission requests
  // TODO(holmer): Add API for changing Process interval and make sure it's
  // disabled when NACK is off.
  if (_retransmissionTimer.TimeUntilProcess() == 0) {
    _retransmissionTimer.Processed();
    bool callback_registered = false;
    uint16_t length;
    {
      CriticalSectionScoped cs(process_crit_sect_.get());
      length = max_nack_list_size_;
      callback_registered = _packetRequestCallback != NULL;
    }
    if (callback_registered && length > 0) {
      std::vector<uint16_t> nackList(length);
      const int32_t ret = NackList(&nackList[0], &length);
      if (ret != VCM_OK && returnValue == VCM_OK) {
        returnValue = ret;
      }
      if (ret == VCM_OK && length > 0) {
        CriticalSectionScoped cs(process_crit_sect_.get());
        if (_packetRequestCallback != NULL) {
          _packetRequestCallback->ResendPackets(&nackList[0], length);
        }
      }
    }
  }

  return returnValue;
}

void VideoReceiver::SetReceiveState(VideoReceiveState state) {
  if (state == _receiveState) {
    return;
  }
  if (state == kReceiveStatePreemptiveNACK &&
      (_receiveState == kReceiveStateWaitingKey ||
       _receiveState == kReceiveStateDecodingWithErrors)) {
    // invalid state transition - this lets us try to set it on NACK
    // without worrying about the current state
    return;
  }
  _receiveState = state;

  CriticalSectionScoped cs(process_crit_sect_.get());
  if (_receiveStateCallback != NULL) {
    _receiveStateCallback->ReceiveStateChange(_receiveState);
  }
}

int32_t VideoReceiver::TimeUntilNextProcess() {
  uint32_t timeUntilNextProcess = _receiveStatsTimer.TimeUntilProcess();
  if ((_receiver.NackMode() != kNoNack) ||
      (_dualReceiver.State() != kPassive)) {
    // We need a Process call more often if we are relying on
    // retransmissions
    timeUntilNextProcess =
        VCM_MIN(timeUntilNextProcess, _retransmissionTimer.TimeUntilProcess());
  }
  timeUntilNextProcess =
      VCM_MIN(timeUntilNextProcess, _keyRequestTimer.TimeUntilProcess());

  return timeUntilNextProcess;
}

int32_t VideoReceiver::SetReceiveChannelParameters(uint32_t rtt) {
  CriticalSectionScoped receiveCs(_receiveCritSect);
  _receiver.UpdateRtt(rtt);
  return 0;
}

// Enable or disable a video protection method.
// Note: This API should be deprecated, as it does not offer a distinction
// between the protection method and decoding with or without errors. If such a
// behavior is desired, use the following API: SetReceiverRobustnessMode.
int32_t VideoReceiver::SetVideoProtection(VCMVideoProtection videoProtection,
                                          bool enable) {
  // By default, do not decode with errors.
  _receiver.SetDecodeErrorMode(kNoErrors);
  // The dual decoder should always be error free.
  _dualReceiver.SetDecodeErrorMode(kNoErrors);
  switch (videoProtection) {
    case kProtectionNack:
    case kProtectionNackReceiver: {
      CriticalSectionScoped cs(_receiveCritSect);
      if (enable) {
        // Enable NACK and always wait for retransmits.
        _receiver.SetNackMode(kNack, -1, -1);
      } else {
        _receiver.SetNackMode(kNoNack, -1, -1);
      }
      break;
    }

    case kProtectionDualDecoder: {
      CriticalSectionScoped cs(_receiveCritSect);
      if (enable) {
        // Enable NACK but don't wait for retransmissions and don't
        // add any extra delay.
        _receiver.SetNackMode(kNack, 0, 0);
        // Enable NACK and always wait for retransmissions and
        // compensate with extra delay.
        _dualReceiver.SetNackMode(kNack, -1, -1);
        _receiver.SetDecodeErrorMode(kWithErrors);
      } else {
        _dualReceiver.SetNackMode(kNoNack, -1, -1);
      }
      break;
    }

    case kProtectionKeyOnLoss: {
      CriticalSectionScoped cs(_receiveCritSect);
      if (enable) {
        _keyRequestMode = kKeyOnLoss;
        _receiver.SetDecodeErrorMode(kWithErrors);
      } else if (_keyRequestMode == kKeyOnLoss) {
        _keyRequestMode = kKeyOnError;  // default mode
      } else {
        return VCM_PARAMETER_ERROR;
      }
      break;
    }

    case kProtectionKeyOnKeyLoss: {
      CriticalSectionScoped cs(_receiveCritSect);
      if (enable) {
        _keyRequestMode = kKeyOnKeyLoss;
      } else if (_keyRequestMode == kKeyOnKeyLoss) {
        _keyRequestMode = kKeyOnError;  // default mode
      } else {
        return VCM_PARAMETER_ERROR;
      }
      break;
    }

    case kProtectionNackFEC: {
      CriticalSectionScoped cs(_receiveCritSect);
      if (enable) {
        // Enable hybrid NACK/FEC. Always wait for retransmissions
        // and don't add extra delay when RTT is above
        // kLowRttNackMs.
        _receiver.SetNackMode(kNack, media_optimization::kLowRttNackMs, -1);
        _receiver.SetDecodeErrorMode(kNoErrors);
        _receiver.SetDecodeErrorMode(kNoErrors);
      } else {
        _receiver.SetNackMode(kNoNack, -1, -1);
      }
      break;
    }
    case kProtectionNackSender:
    case kProtectionFEC:
    case kProtectionPeriodicKeyFrames:
      // Ignore encoder modes.
      return VCM_OK;
  }
  return VCM_OK;
}

// Initialize receiver, resets codec database etc
int32_t VideoReceiver::InitializeReceiver() {
  CriticalSectionScoped receive_cs(_receiveCritSect);
  CriticalSectionScoped process_cs(process_crit_sect_.get());
  int32_t ret = _receiver.Initialize();
  if (ret < 0) {
    return ret;
  }

  ret = _dualReceiver.Initialize();
  if (ret < 0) {
    return ret;
  }
  _codecDataBase.ResetReceiver();
  _timing.Reset();

  _decoder = NULL;
  _decodedFrameCallback.SetUserReceiveCallback(NULL);
  _receiverInited = true;
  _frameTypeCallback = NULL;
  _receiveStatsCallback = NULL;
  _decoderTimingCallback = NULL;
  _packetRequestCallback = NULL;
  _receiveStateCallback = NULL;
  _keyRequestMode = kKeyOnError;
  _scheduleKeyRequest = false;

  return VCM_OK;
}

// Register a receive callback. Will be called whenever there is a new frame
// ready for rendering.
int32_t VideoReceiver::RegisterReceiveCallback(
    VCMReceiveCallback* receiveCallback) {
  CriticalSectionScoped cs(_receiveCritSect);
  _decodedFrameCallback.SetUserReceiveCallback(receiveCallback);
  return VCM_OK;
}

int32_t VideoReceiver::RegisterReceiveStatisticsCallback(
    VCMReceiveStatisticsCallback* receiveStats) {
  CriticalSectionScoped cs(process_crit_sect_.get());
  _receiveStatsCallback = receiveStats;
  return VCM_OK;
}

int32_t VideoReceiver::RegisterDecoderTimingCallback(
    VCMDecoderTimingCallback* decoderTiming) {
  CriticalSectionScoped cs(process_crit_sect_.get());
  _decoderTimingCallback = decoderTiming;
  return VCM_OK;
}

// Register an externally defined decoder/render object.
// Can be a decoder only or a decoder coupled with a renderer.
int32_t VideoReceiver::RegisterExternalDecoder(VideoDecoder* externalDecoder,
                                               uint8_t payloadType,
                                               bool internalRenderTiming) {
  CriticalSectionScoped cs(_receiveCritSect);
  if (externalDecoder == NULL) {
    // Make sure the VCM updates the decoder next time it decodes.
    _decoder = NULL;
    return _codecDataBase.DeregisterExternalDecoder(payloadType) ? 0 : -1;
  }
  return _codecDataBase.RegisterExternalDecoder(
             externalDecoder, payloadType, internalRenderTiming)
             ? 0
             : -1;
}

// Register a frame type request callback.
int32_t VideoReceiver::RegisterFrameTypeCallback(
    VCMFrameTypeCallback* frameTypeCallback) {
  CriticalSectionScoped cs(process_crit_sect_.get());
  _frameTypeCallback = frameTypeCallback;
  return VCM_OK;
}

int32_t VideoReceiver::RegisterPacketRequestCallback(
    VCMPacketRequestCallback* callback) {
  CriticalSectionScoped cs(process_crit_sect_.get());
  _packetRequestCallback = callback;
  return VCM_OK;
}

int32_t VideoReceiver::RegisterReceiveStateCallback(
    VCMReceiveStateCallback* callback) {
  CriticalSectionScoped cs(process_crit_sect_.get());
  _receiveStateCallback = callback;
  return VCM_OK;
}

int VideoReceiver::RegisterRenderBufferSizeCallback(
    VCMRenderBufferSizeCallback* callback) {
  CriticalSectionScoped cs(process_crit_sect_.get());
  render_buffer_callback_ = callback;
  return VCM_OK;
}

// Decode next frame, blocking.
// Should be called as often as possible to get the most out of the decoder.
int32_t VideoReceiver::Decode(uint16_t maxWaitTimeMs) {
  int64_t nextRenderTimeMs;
  {
    CriticalSectionScoped cs(_receiveCritSect);
    if (!_receiverInited) {
      return VCM_UNINITIALIZED;
    }
    if (!_codecDataBase.DecoderRegistered()) {
      return VCM_NO_CODEC_REGISTERED;
    }
  }

  const bool dualReceiverEnabledNotReceiving = (
      _dualReceiver.State() != kReceiving && _dualReceiver.NackMode() == kNack);

  VCMEncodedFrame* frame =
      _receiver.FrameForDecoding(maxWaitTimeMs,
                                 nextRenderTimeMs,
                                 _codecDataBase.SupportsRenderScheduling(),
                                 &_dualReceiver);

  if (dualReceiverEnabledNotReceiving && _dualReceiver.State() == kReceiving) {
    // Dual receiver is enabled (kNACK enabled), but was not receiving
    // before the call to FrameForDecoding(). After the call the state
    // changed to receiving, and therefore we must copy the primary decoder
    // state to the dual decoder to make it possible for the dual decoder to
    // start decoding retransmitted frames and recover.
    CriticalSectionScoped cs(_receiveCritSect);
    if (_dualDecoder != NULL) {
      _codecDataBase.ReleaseDecoder(_dualDecoder);
    }
    _dualDecoder = _codecDataBase.CreateDecoderCopy();
    if (_dualDecoder != NULL) {
      _dualDecoder->RegisterDecodeCompleteCallback(&_dualDecodedFrameCallback);
      SetReceiveState(kReceiveStateDecodingWithErrors);
    } else {
      _dualReceiver.Reset();
      // presume we have an error, though we might not yet
      SetReceiveState(kReceiveStateWaitingKey);
    }
  } else {
    SetReceiveState(_receiver.ReceiveState());
  }

  if (frame == NULL) {
    return VCM_FRAME_NOT_READY;
  } else {
    CriticalSectionScoped cs(_receiveCritSect);

    // If this frame was too late, we should adjust the delay accordingly
    _timing.UpdateCurrentDelay(frame->RenderTimeMs(),
                               clock_->TimeInMilliseconds());

    if (pre_decode_image_callback_) {
      EncodedImage encoded_image(frame->EncodedImage());
      pre_decode_image_callback_->Encoded(encoded_image);
    }

#ifdef DEBUG_DECODER_BIT_STREAM
    if (_bitStreamBeforeDecoder != NULL) {
      // Write bit stream to file for debugging purposes
      if (fwrite(
              frame->Buffer(), 1, frame->Length(), _bitStreamBeforeDecoder) !=
          frame->Length()) {
        return -1;
      }
    }
#endif
    const int32_t ret = Decode(*frame);
    _receiver.ReleaseFrame(frame);
    frame = NULL;
    if (ret != VCM_OK) {
      return ret;
    }
  }
  return VCM_OK;
}

int32_t VideoReceiver::RequestSliceLossIndication(
    const uint64_t pictureID) const {
  TRACE_EVENT1("webrtc", "RequestSLI", "picture_id", pictureID);
  CriticalSectionScoped cs(process_crit_sect_.get());
  if (_frameTypeCallback != NULL) {
    const int32_t ret =
        _frameTypeCallback->SliceLossIndicationRequest(pictureID);
    if (ret < 0) {
      WEBRTC_TRACE(webrtc::kTraceError,
                   webrtc::kTraceVideoCoding,
                   VCMId(_id),
                   "Failed to request key frame");
      return ret;
    }
  } else {
    WEBRTC_TRACE(webrtc::kTraceWarning,
                 webrtc::kTraceVideoCoding,
                 VCMId(_id),
                 "No frame type request callback registered");
    return VCM_MISSING_CALLBACK;
  }
  return VCM_OK;
}

int32_t VideoReceiver::RequestKeyFrame() {
  TRACE_EVENT0("webrtc", "RequestKeyFrame");
  CriticalSectionScoped process_cs(process_crit_sect_.get());
  if (_frameTypeCallback != NULL) {
    const int32_t ret = _frameTypeCallback->RequestKeyFrame();
    if (ret < 0) {
      WEBRTC_TRACE(webrtc::kTraceError,
                   webrtc::kTraceVideoCoding,
                   VCMId(_id),
                   "Failed to request key frame");
      return ret;
    }
    _scheduleKeyRequest = false;
  } else {
    WEBRTC_TRACE(webrtc::kTraceWarning,
                 webrtc::kTraceVideoCoding,
                 VCMId(_id),
                 "No frame type request callback registered");
    return VCM_MISSING_CALLBACK;
  }
  return VCM_OK;
}

int32_t VideoReceiver::DecodeDualFrame(uint16_t maxWaitTimeMs) {
  CriticalSectionScoped cs(_receiveCritSect);
  if (_dualReceiver.State() != kReceiving ||
      _dualReceiver.NackMode() != kNack) {
    // The dual receiver is currently not receiving or
    // dual decoder mode is disabled.
    return VCM_OK;
  }
  int64_t dummyRenderTime;
  int32_t decodeCount = 0;
  // The dual decoder's state is copied from the main decoder, which may
  // decode with errors. Make sure that the dual decoder does not introduce
  // error.
  _dualReceiver.SetDecodeErrorMode(kNoErrors);
  VCMEncodedFrame* dualFrame =
      _dualReceiver.FrameForDecoding(maxWaitTimeMs, dummyRenderTime);
  if (dualFrame != NULL && _dualDecoder != NULL) {
    WEBRTC_TRACE(webrtc::kTraceStream,
                 webrtc::kTraceVideoCoding,
                 VCMId(_id),
                 "Decoding frame %u with dual decoder",
                 dualFrame->TimeStamp());
    // Decode dualFrame and try to catch up
    int32_t ret =
        _dualDecoder->Decode(*dualFrame, clock_->TimeInMilliseconds());
    if (ret != WEBRTC_VIDEO_CODEC_OK) {
      WEBRTC_TRACE(webrtc::kTraceWarning,
                   webrtc::kTraceVideoCoding,
                   VCMId(_id),
                   "Failed to decode frame with dual decoder");
      _dualReceiver.ReleaseFrame(dualFrame);
      return VCM_CODEC_ERROR;
    }
    if (_receiver.DualDecoderCaughtUp(dualFrame, _dualReceiver)) {
      // Copy the complete decoder state of the dual decoder
      // to the primary decoder.
      WEBRTC_TRACE(webrtc::kTraceStream,
                   webrtc::kTraceVideoCoding,
                   VCMId(_id),
                   "Dual decoder caught up");
      _codecDataBase.CopyDecoder(*_dualDecoder);
      _codecDataBase.ReleaseDecoder(_dualDecoder);
      _dualDecoder = NULL;
    }
    decodeCount++;
  }
  _dualReceiver.ReleaseFrame(dualFrame);
  return decodeCount;
}

// Must be called from inside the receive side critical section.
int32_t VideoReceiver::Decode(const VCMEncodedFrame& frame) {
  TRACE_EVENT_ASYNC_STEP1("webrtc",
                          "Video",
                          frame.TimeStamp(),
                          "Decode",
                          "type",
                          frame.FrameType());
  // Change decoder if payload type has changed
  const bool renderTimingBefore = _codecDataBase.SupportsRenderScheduling();
  _decoder =
      _codecDataBase.GetDecoder(frame.PayloadType(), &_decodedFrameCallback);
  if (renderTimingBefore != _codecDataBase.SupportsRenderScheduling()) {
    // Make sure we reset the decode time estimate since it will
    // be zero for codecs without render timing.
    _timing.ResetDecodeTime();
  }
  if (_decoder == NULL) {
    return VCM_NO_CODEC_REGISTERED;
  }
  // Decode a frame
  int32_t ret = _decoder->Decode(frame, clock_->TimeInMilliseconds());

  // Check for failed decoding, run frame type request callback if needed.
  bool request_key_frame = false;
  if (ret < 0) {
    if (ret == VCM_ERROR_REQUEST_SLI) {
      return RequestSliceLossIndication(
          _decodedFrameCallback.LastReceivedPictureID() + 1);
    } else {
      WEBRTC_TRACE(webrtc::kTraceError,
                   webrtc::kTraceVideoCoding,
                   VCMId(_id),
                   "Failed to decode frame %u, requesting key frame",
                   frame.TimeStamp());
      request_key_frame = true;
    }
  } else if (ret == VCM_REQUEST_SLI) {
    ret = RequestSliceLossIndication(
        _decodedFrameCallback.LastReceivedPictureID() + 1);
  }
  if (!frame.Complete() || frame.MissingFrame()) {
    switch (_keyRequestMode) {
      case kKeyOnKeyLoss: {
        if (frame.FrameType() == kVideoFrameKey) {
          request_key_frame = true;
          ret = VCM_OK;
        }
        break;
      }
      case kKeyOnLoss: {
        request_key_frame = true;
        ret = VCM_OK;
      }
      default:
        break;
    }
  }
  if (request_key_frame) {
    CriticalSectionScoped cs(process_crit_sect_.get());
    _scheduleKeyRequest = true;
  }
  TRACE_EVENT_ASYNC_END0("webrtc", "Video", frame.TimeStamp());
  return ret;
}

// Reset the decoder state
int32_t VideoReceiver::ResetDecoder() {
  bool reset_key_request = false;
  {
    CriticalSectionScoped cs(_receiveCritSect);
    if (_decoder != NULL) {
      _receiver.Initialize();
      _timing.Reset();
      reset_key_request = true;
      _decoder->Reset();
    }
    if (_dualReceiver.State() != kPassive) {
      _dualReceiver.Initialize();
    }
    if (_dualDecoder != NULL) {
      _codecDataBase.ReleaseDecoder(_dualDecoder);
      _dualDecoder = NULL;
    }
  }
  if (reset_key_request) {
    CriticalSectionScoped cs(process_crit_sect_.get());
    _scheduleKeyRequest = false;
  }
  return VCM_OK;
}

// Register possible receive codecs, can be called multiple times
int32_t VideoReceiver::RegisterReceiveCodec(const VideoCodec* receiveCodec,
                                            int32_t numberOfCores,
                                            bool requireKeyFrame) {
  CriticalSectionScoped cs(_receiveCritSect);
  if (receiveCodec == NULL) {
    return VCM_PARAMETER_ERROR;
  }
  if (!_codecDataBase.RegisterReceiveCodec(
          receiveCodec, numberOfCores, requireKeyFrame)) {
    return -1;
  }
  return 0;
}

// Get current received codec
int32_t VideoReceiver::ReceiveCodec(VideoCodec* currentReceiveCodec) const {
  CriticalSectionScoped cs(_receiveCritSect);
  if (currentReceiveCodec == NULL) {
    return VCM_PARAMETER_ERROR;
  }
  return _codecDataBase.ReceiveCodec(currentReceiveCodec) ? 0 : -1;
}

// Get current received codec
VideoCodecType VideoReceiver::ReceiveCodec() const {
  CriticalSectionScoped cs(_receiveCritSect);
  return _codecDataBase.ReceiveCodec();
}

// Incoming packet from network parsed and ready for decode, non blocking.
int32_t VideoReceiver::IncomingPacket(const uint8_t* incomingPayload,
                                      uint32_t payloadLength,
                                      const WebRtcRTPHeader& rtpInfo) {
  if (rtpInfo.frameType == kVideoFrameKey) {
    TRACE_EVENT1("webrtc",
                 "VCM::PacketKeyFrame",
                 "seqnum",
                 rtpInfo.header.sequenceNumber);
  }
  if (incomingPayload == NULL) {
    // The jitter buffer doesn't handle non-zero payload lengths for packets
    // without payload.
    // TODO(holmer): We should fix this in the jitter buffer.
    payloadLength = 0;
  }
  const VCMPacket packet(incomingPayload, payloadLength, rtpInfo);
  int32_t ret;
  if (_dualReceiver.State() != kPassive) {
    ret = _dualReceiver.InsertPacket(
        packet, rtpInfo.type.Video.width, rtpInfo.type.Video.height);
    if (ret == VCM_FLUSH_INDICATOR) {
      RequestKeyFrame();
      ResetDecoder();
      SetReceiveState(kReceiveStateWaitingKey);
    } else if (ret < 0) {
      return ret;
    }
  }
  ret = _receiver.InsertPacket(
      packet, rtpInfo.type.Video.width, rtpInfo.type.Video.height);
  // TODO(holmer): Investigate if this somehow should use the key frame
  // request scheduling to throttle the requests.
  if (ret == VCM_FLUSH_INDICATOR) {
    RequestKeyFrame();
    ResetDecoder();
    SetReceiveState(kReceiveStateWaitingKey);
  } else if (ret < 0) {
    return ret;
  }
  return VCM_OK;
}

// Minimum playout delay (used for lip-sync). This is the minimum delay required
// to sync with audio. Not included in  VideoCodingModule::Delay()
// Defaults to 0 ms.
int32_t VideoReceiver::SetMinimumPlayoutDelay(uint32_t minPlayoutDelayMs) {
  _timing.set_min_playout_delay(minPlayoutDelayMs);
  return VCM_OK;
}

// The estimated delay caused by rendering, defaults to
// kDefaultRenderDelayMs = 10 ms
int32_t VideoReceiver::SetRenderDelay(uint32_t timeMS) {
  _timing.set_render_delay(timeMS);
  return VCM_OK;
}

// Current video delay
int32_t VideoReceiver::Delay() const { return _timing.TargetVideoDelay(); }

// Nack list
int32_t VideoReceiver::NackList(uint16_t* nackList, uint16_t* size) {
  VCMNackStatus nackStatus = kNackOk;
  uint16_t nack_list_length = 0;
  // Collect sequence numbers from the default receiver
  // if in normal nack mode. Otherwise collect them from
  // the dual receiver if the dual receiver is receiving.
  if (_receiver.NackMode() != kNoNack) {
    nackStatus = _receiver.NackList(nackList, *size, &nack_list_length);
  }
  if (nack_list_length == 0 && _dualReceiver.State() != kPassive) {
    nackStatus = _dualReceiver.NackList(nackList, *size, &nack_list_length);
  }
  *size = nack_list_length;

  switch (nackStatus) {
    case kNackNeedMoreMemory: {
      WEBRTC_TRACE(webrtc::kTraceError,
                   webrtc::kTraceVideoCoding,
                   VCMId(_id),
                   "Out of memory");
      return VCM_MEMORY;
    }
    case kNackKeyFrameRequest: {
      WEBRTC_TRACE(webrtc::kTraceWarning,
                   webrtc::kTraceVideoCoding,
                   VCMId(_id),
                   "Failed to get NACK list, requesting key frame");
      SetReceiveState(kReceiveStateWaitingKey);
      return RequestKeyFrame();
    }
    default:
      break;
  }
  if (*size != 0) {
    // Note: not a valid transition from WaitingKey or DecodingWithErrors;
    // will be ignored in that case
    SetReceiveState(kReceiveStatePreemptiveNACK);
  }
  return VCM_OK;
}

int32_t VideoReceiver::ReceivedFrameCount(VCMFrameCount* frameCount) const {
  _receiver.ReceivedFrameCount(frameCount);
  return VCM_OK;
}

uint32_t VideoReceiver::DiscardedPackets() const {
  return _receiver.DiscardedPackets();
}

int VideoReceiver::SetReceiverRobustnessMode(
    ReceiverRobustness robustnessMode,
    VCMDecodeErrorMode decode_error_mode) {
  CriticalSectionScoped cs(_receiveCritSect);
  switch (robustnessMode) {
    case VideoCodingModule::kNone:
      _receiver.SetNackMode(kNoNack, -1, -1);
      _dualReceiver.SetNackMode(kNoNack, -1, -1);
      if (decode_error_mode == kNoErrors) {
        _keyRequestMode = kKeyOnLoss;
      } else {
        _keyRequestMode = kKeyOnError;
      }
      break;
    case VideoCodingModule::kHardNack:
      // Always wait for retransmissions (except when decoding with errors).
      _receiver.SetNackMode(kNack, -1, -1);
      _dualReceiver.SetNackMode(kNoNack, -1, -1);
      _keyRequestMode = kKeyOnError;  // TODO(hlundin): On long NACK list?
      break;
    case VideoCodingModule::kSoftNack:
      assert(false);  // TODO(hlundin): Not completed.
      return VCM_NOT_IMPLEMENTED;
      // Enable hybrid NACK/FEC. Always wait for retransmissions and don't add
      // extra delay when RTT is above kLowRttNackMs.
      _receiver.SetNackMode(kNack, media_optimization::kLowRttNackMs, -1);
      _dualReceiver.SetNackMode(kNoNack, -1, -1);
      _keyRequestMode = kKeyOnError;
      break;
    case VideoCodingModule::kDualDecoder:
      if (decode_error_mode == kNoErrors) {
        return VCM_PARAMETER_ERROR;
      }
      // Enable NACK but don't wait for retransmissions and don't add any extra
      // delay.
      _receiver.SetNackMode(kNack, 0, 0);
      // Enable NACK, compensate with extra delay and wait for retransmissions.
      _dualReceiver.SetNackMode(kNack, -1, -1);
      _keyRequestMode = kKeyOnError;
      break;
    case VideoCodingModule::kReferenceSelection:
      assert(false);  // TODO(hlundin): Not completed.
      return VCM_NOT_IMPLEMENTED;
      if (decode_error_mode == kNoErrors) {
        return VCM_PARAMETER_ERROR;
      }
      _receiver.SetNackMode(kNoNack, -1, -1);
      _dualReceiver.SetNackMode(kNoNack, -1, -1);
      break;
  }
  _receiver.SetDecodeErrorMode(decode_error_mode);
  // The dual decoder should never decode with errors.
  _dualReceiver.SetDecodeErrorMode(kNoErrors);
  return VCM_OK;
}

void VideoReceiver::SetDecodeErrorMode(VCMDecodeErrorMode decode_error_mode) {
  CriticalSectionScoped cs(_receiveCritSect);
  _receiver.SetDecodeErrorMode(decode_error_mode);
}

void VideoReceiver::SetNackSettings(size_t max_nack_list_size,
                                    int max_packet_age_to_nack,
                                    int max_incomplete_time_ms) {
  if (max_nack_list_size != 0) {
    CriticalSectionScoped receive_cs(_receiveCritSect);
    CriticalSectionScoped process_cs(process_crit_sect_.get());
    max_nack_list_size_ = max_nack_list_size;
  }
  _receiver.SetNackSettings(
      max_nack_list_size, max_packet_age_to_nack, max_incomplete_time_ms);
  _dualReceiver.SetNackSettings(
      max_nack_list_size, max_packet_age_to_nack, max_incomplete_time_ms);
}

int VideoReceiver::SetMinReceiverDelay(int desired_delay_ms) {
  return _receiver.SetMinReceiverDelay(desired_delay_ms);
}

void VideoReceiver::RegisterPreDecodeImageCallback(
    EncodedImageCallback* observer) {
  CriticalSectionScoped cs(_receiveCritSect);
  pre_decode_image_callback_ = observer;
}

}  // namespace vcm
}  // namespace webrtc
