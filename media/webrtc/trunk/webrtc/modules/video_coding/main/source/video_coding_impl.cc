/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video_coding_impl.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "common_types.h"
#include "encoded_frame.h"
#include "jitter_buffer.h"
#include "packet.h"
#include "trace.h"
#include "video_codec_interface.h"
#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/system_wrappers/interface/trace_event.h"

namespace webrtc
{

//#define DEBUG_DECODER_BIT_STREAM

uint32_t
VCMProcessTimer::Period() const
{
    return _periodMs;
}

uint32_t
VCMProcessTimer::TimeUntilProcess() const
{
    const int64_t time_since_process = _clock->TimeInMilliseconds() -
        static_cast<int64_t>(_latestMs);
    const int64_t time_until_process = static_cast<int64_t>(_periodMs) -
        time_since_process;
    if (time_until_process < 0)
      return 0;
    return time_until_process;
}

void
VCMProcessTimer::Processed()
{
    _latestMs = _clock->TimeInMilliseconds();
}

VideoCodingModuleImpl::VideoCodingModuleImpl(const int32_t id,
                                             Clock* clock,
                                             EventFactory* event_factory,
                                             bool owns_event_factory)
    : _id(id),
      clock_(clock),
      _receiveCritSect(CriticalSectionWrapper::CreateCriticalSection()),
      _receiverInited(false),
      _timing(clock_, id, 1),
      _dualTiming(clock_, id, 2, &_timing),
      _receiver(&_timing, clock_, event_factory, id, 1, true),
      _dualReceiver(&_dualTiming, clock_, event_factory, id, 2, false),
      _decodedFrameCallback(_timing, clock_),
      _dualDecodedFrameCallback(_dualTiming, clock_),
      _frameTypeCallback(NULL),
      _frameStorageCallback(NULL),
      _receiveStatsCallback(NULL),
      _packetRequestCallback(NULL),
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
      _sendCritSect(CriticalSectionWrapper::CreateCriticalSection()),
      _encoder(),
      _encodedFrameCallback(),
      _nextFrameTypes(1, kVideoFrameDelta),
      _mediaOpt(id, clock_),
      _sendCodecType(kVideoCodecUnknown),
      _sendStatsCallback(NULL),
      _encoderInputFile(NULL),
      _codecDataBase(id),
      _receiveStatsTimer(1000, clock_),
      _sendStatsTimer(1000, clock_),
      _retransmissionTimer(10, clock_),
      _keyRequestTimer(500, clock_),
      event_factory_(event_factory),
      owns_event_factory_(owns_event_factory),
      frame_dropper_enabled_(true) {
  assert(clock_);
#ifdef DEBUG_DECODER_BIT_STREAM
  _bitStreamBeforeDecoder = fopen("decoderBitStream.bit", "wb");
#endif
}

VideoCodingModuleImpl::~VideoCodingModuleImpl()
{
    if (_dualDecoder != NULL)
    {
        _codecDataBase.ReleaseDecoder(_dualDecoder);
    }
    delete _receiveCritSect;
    delete _sendCritSect;
    if (owns_event_factory_) {
      delete event_factory_;
    }
#ifdef DEBUG_DECODER_BIT_STREAM
    fclose(_bitStreamBeforeDecoder);
#endif
    if (_encoderInputFile != NULL)
    {
        fclose(_encoderInputFile);
    }
}

VideoCodingModule*
VideoCodingModule::Create(const int32_t id)
{
    return new VideoCodingModuleImpl(id, Clock::GetRealTimeClock(),
                                     new EventFactoryImpl, true);
}

VideoCodingModule*
VideoCodingModule::Create(const int32_t id, Clock* clock,
                          EventFactory* event_factory)
{
    assert(clock);
    assert(event_factory);
    return new VideoCodingModuleImpl(id, clock, event_factory, false);
}

void
VideoCodingModule::Destroy(VideoCodingModule* module)
{
    if (module != NULL)
    {
        delete static_cast<VideoCodingModuleImpl*>(module);
    }
}

int32_t
VideoCodingModuleImpl::Process()
{
    int32_t returnValue = VCM_OK;

    // Receive-side statistics
    if (_receiveStatsTimer.TimeUntilProcess() == 0)
    {
        _receiveStatsTimer.Processed();
        if (_receiveStatsCallback != NULL)
        {
            uint32_t bitRate;
            uint32_t frameRate;
            _receiver.ReceiveStatistics(&bitRate, &frameRate);
            _receiveStatsCallback->ReceiveStatistics(bitRate, frameRate);
        }

        // Size of render buffer.
        if (render_buffer_callback_) {
          int buffer_size_ms = _receiver.RenderBufferSizeMs();
          render_buffer_callback_->RenderBufferSizeMs(buffer_size_ms);
      }
    }

    // Send-side statistics
    if (_sendStatsTimer.TimeUntilProcess() == 0)
    {
        _sendStatsTimer.Processed();
        if (_sendStatsCallback != NULL)
        {
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

    // Packet retransmission requests
    // TODO(holmer): Add API for changing Process interval and make sure it's
    // disabled when NACK is off.
    if (_retransmissionTimer.TimeUntilProcess() == 0)
    {
        _retransmissionTimer.Processed();
        if (_packetRequestCallback != NULL)
        {
            uint16_t length;
            {
                CriticalSectionScoped cs(_receiveCritSect);
                length = max_nack_list_size_;
            }
            std::vector<uint16_t> nackList(length);
            const int32_t ret = NackList(&nackList[0], length);
            if (ret != VCM_OK && returnValue == VCM_OK)
            {
                returnValue = ret;
            }
            if (length > 0)
            {
                _packetRequestCallback->ResendPackets(&nackList[0], length);
            }
        }
    }

    // Key frame requests
    if (_keyRequestTimer.TimeUntilProcess() == 0)
    {
        _keyRequestTimer.Processed();
        if (_scheduleKeyRequest && _frameTypeCallback != NULL)
        {
            const int32_t ret = RequestKeyFrame();
            if (ret != VCM_OK && returnValue == VCM_OK)
            {
                returnValue = ret;
            }
        }
    }

    return returnValue;
}

int32_t
VideoCodingModuleImpl::Id() const
{
    CriticalSectionScoped receiveCs(_receiveCritSect);
    {
        CriticalSectionScoped sendCs(_sendCritSect);
        return _id;
    }
}

//  Change the unique identifier of this object
int32_t
VideoCodingModuleImpl::ChangeUniqueId(const int32_t id)
{
    CriticalSectionScoped receiveCs(_receiveCritSect);
    {
        CriticalSectionScoped sendCs(_sendCritSect);
        _id = id;
        return VCM_OK;
    }
}

// Returns the number of milliseconds until the module wants a worker thread to
// call Process
int32_t
VideoCodingModuleImpl::TimeUntilNextProcess()
{
    uint32_t timeUntilNextProcess = VCM_MIN(
                                    _receiveStatsTimer.TimeUntilProcess(),
                                    _sendStatsTimer.TimeUntilProcess());
    if ((_receiver.NackMode() != kNoNack) ||
        (_dualReceiver.State() != kPassive))
    {
        // We need a Process call more often if we are relying on
        // retransmissions
        timeUntilNextProcess = VCM_MIN(timeUntilNextProcess,
                                       _retransmissionTimer.TimeUntilProcess());
    }
    timeUntilNextProcess = VCM_MIN(timeUntilNextProcess,
                                   _keyRequestTimer.TimeUntilProcess());

    return timeUntilNextProcess;
}

// Get number of supported codecs
uint8_t
VideoCodingModule::NumberOfCodecs()
{
    return VCMCodecDataBase::NumberOfCodecs();
}

// Get supported codec with id
int32_t
VideoCodingModule::Codec(uint8_t listId, VideoCodec* codec)
{
    if (codec == NULL)
    {
        return VCM_PARAMETER_ERROR;
    }
    return VCMCodecDataBase::Codec(listId, codec) ? 0 : -1;
}

// Get supported codec with type
int32_t
VideoCodingModule::Codec(VideoCodecType codecType, VideoCodec* codec)
{
    if (codec == NULL)
    {
        return VCM_PARAMETER_ERROR;
    }
    return VCMCodecDataBase::Codec(codecType, codec) ? 0 : -1;
}

/*
*   Sender
*/

// Reset send side to initial state - all components
int32_t
VideoCodingModuleImpl::InitializeSender()
{
    CriticalSectionScoped cs(_sendCritSect);
    _codecDataBase.ResetSender();
    _encoder = NULL;
    _encodedFrameCallback.SetTransportCallback(NULL);
    // setting default bitRate and frameRate to 0
    _mediaOpt.SetEncodingData(kVideoCodecUnknown, 0, 0, 0, 0, 0, 0);
    _mediaOpt.Reset(); // Resetting frame dropper
    return VCM_OK;
}

// Register the send codec to be used.
int32_t
VideoCodingModuleImpl::RegisterSendCodec(const VideoCodec* sendCodec,
                                         uint32_t numberOfCores,
                                         uint32_t maxPayloadSize)
{
    CriticalSectionScoped cs(_sendCritSect);
    if (sendCodec == NULL) {
        return VCM_PARAMETER_ERROR;
    }

    bool ret = _codecDataBase.SetSendCodec(sendCodec, numberOfCores,
                                           maxPayloadSize,
                                           &_encodedFrameCallback);
    if (!ret) {
        WEBRTC_TRACE(webrtc::kTraceError,
                     webrtc::kTraceVideoCoding,
                     VCMId(_id),
                     "Failed to initialize encoder");
        return VCM_CODEC_ERROR;
    }

    _encoder = _codecDataBase.GetEncoder();
    _sendCodecType = sendCodec->codecType;
    int numLayers = (_sendCodecType != kVideoCodecVP8) ? 1 :
                        sendCodec->codecSpecific.VP8.numberOfTemporalLayers;
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

    _mediaOpt.SetEncodingData(_sendCodecType,
                              sendCodec->maxBitrate * 1000,
                              sendCodec->maxFramerate * 1000,
                              sendCodec->startBitrate * 1000,
                              sendCodec->width,
                              sendCodec->height,
                              numLayers);
    _mediaOpt.SetMtu(maxPayloadSize);

    return VCM_OK;
}

// Get current send codec
int32_t
VideoCodingModuleImpl::SendCodec(VideoCodec* currentSendCodec) const
{
    CriticalSectionScoped cs(_sendCritSect);

    if (currentSendCodec == NULL)
    {
        return VCM_PARAMETER_ERROR;
    }
    return _codecDataBase.SendCodec(currentSendCodec) ? 0 : -1;
}

// Get the current send codec type
VideoCodecType
VideoCodingModuleImpl::SendCodec() const
{
    CriticalSectionScoped cs(_sendCritSect);

    return _codecDataBase.SendCodec();
}

// Register an external decoder object.
// This can not be used together with external decoder callbacks.
int32_t
VideoCodingModuleImpl::RegisterExternalEncoder(VideoEncoder* externalEncoder,
                                               uint8_t payloadType,
                                               bool internalSource /*= false*/)
{
    CriticalSectionScoped cs(_sendCritSect);

    if (externalEncoder == NULL)
    {
        bool wasSendCodec = false;
        const bool ret = _codecDataBase.DeregisterExternalEncoder(
            payloadType, &wasSendCodec);
        if (wasSendCodec)
        {
            // Make sure the VCM doesn't use the de-registered codec
            _encoder = NULL;
        }
        return ret ? 0 : -1;
    }
    _codecDataBase.RegisterExternalEncoder(externalEncoder, payloadType,
                                           internalSource);
    return 0;
}

// Get codec config parameters
int32_t
VideoCodingModuleImpl::CodecConfigParameters(uint8_t* buffer,
                                             int32_t size)
{
    CriticalSectionScoped cs(_sendCritSect);
    if (_encoder != NULL)
    {
        return _encoder->CodecConfigParameters(buffer, size);
    }
    return VCM_UNINITIALIZED;
}

// Get encode bitrate
int VideoCodingModuleImpl::Bitrate(unsigned int* bitrate) const
{
  CriticalSectionScoped cs(_sendCritSect);
  // return the bit rate which the encoder is set to
  if (!_encoder) {
    return VCM_UNINITIALIZED;
  }
  *bitrate = _encoder->BitRate();
  return 0;
}

// Get encode frame rate
int VideoCodingModuleImpl::FrameRate(unsigned int* framerate) const
{
  CriticalSectionScoped cs(_sendCritSect);
  // input frame rate, not compensated
  if (!_encoder) {
    return VCM_UNINITIALIZED;
  }
  *framerate = _encoder->FrameRate();
  return 0;
}

// Set channel parameters
int32_t
VideoCodingModuleImpl::SetChannelParameters(uint32_t target_bitrate,
                                            uint8_t lossRate,
                                            uint32_t rtt)
{
    int32_t ret = 0;
    {
        CriticalSectionScoped sendCs(_sendCritSect);
        uint32_t targetRate = _mediaOpt.SetTargetRates(target_bitrate,
                                                             lossRate,
                                                             rtt);
        if (_encoder != NULL)
        {
            ret = _encoder->SetChannelParameters(lossRate, rtt);
            if (ret < 0 )
            {
                return ret;
            }
            ret = (int32_t)_encoder->SetRates(targetRate,
                                                    _mediaOpt.InputFrameRate());
            if (ret < 0)
            {
                return ret;
            }
        }
        else
        {
            return VCM_UNINITIALIZED;
        } // encoder
    }// send side
    return VCM_OK;
}

int32_t
VideoCodingModuleImpl::SetReceiveChannelParameters(uint32_t rtt)
{
    CriticalSectionScoped receiveCs(_receiveCritSect);
    _receiver.UpdateRtt(rtt);
    return 0;
}

// Register a transport callback which will be called to deliver the encoded
// buffers
int32_t
VideoCodingModuleImpl::RegisterTransportCallback(
    VCMPacketizationCallback* transport)
{
    CriticalSectionScoped cs(_sendCritSect);
    _encodedFrameCallback.SetMediaOpt(&_mediaOpt);
    _encodedFrameCallback.SetTransportCallback(transport);
    return VCM_OK;
}

// Register video output information callback which will be called to deliver
// information about the video stream produced by the encoder, for instance the
// average frame rate and bit rate.
int32_t
VideoCodingModuleImpl::RegisterSendStatisticsCallback(
    VCMSendStatisticsCallback* sendStats)
{
    CriticalSectionScoped cs(_sendCritSect);
    _sendStatsCallback = sendStats;
    return VCM_OK;
}

// Register a video quality settings callback which will be called when frame
// rate/dimensions need to be updated for video quality optimization
int32_t
VideoCodingModuleImpl::RegisterVideoQMCallback(
    VCMQMSettingsCallback* videoQMSettings)
{
    CriticalSectionScoped cs(_sendCritSect);
    return _mediaOpt.RegisterVideoQMCallback(videoQMSettings);
}


// Register a video protection callback which will be called to deliver the
// requested FEC rate and NACK status (on/off).
int32_t
VideoCodingModuleImpl::RegisterProtectionCallback(
    VCMProtectionCallback* protection)
{
    CriticalSectionScoped cs(_sendCritSect);
    _mediaOpt.RegisterProtectionCallback(protection);
    return VCM_OK;
}

// Enable or disable a video protection method.
// Note: This API should be deprecated, as it does not offer a distinction
// between the protection method and decoding with or without errors. If such a
// behavior is desired, use the following API: SetReceiverRobustnessMode.
int32_t
VideoCodingModuleImpl::SetVideoProtection(VCMVideoProtection videoProtection,
                                          bool enable)
{
    // By default, do not decode with errors.
    _receiver.SetDecodeWithErrors(false);
    // The dual decoder should always be error free.
    _dualReceiver.SetDecodeWithErrors(false);
    switch (videoProtection)
    {

    case kProtectionNack:
        {
            // Both send-side and receive-side
            SetVideoProtection(kProtectionNackSender, enable);
            SetVideoProtection(kProtectionNackReceiver, enable);
            break;
        }

    case kProtectionNackSender:
        {
            CriticalSectionScoped cs(_sendCritSect);
            _mediaOpt.EnableProtectionMethod(enable, media_optimization::kNack);
            break;
        }

    case kProtectionNackReceiver:
        {
            CriticalSectionScoped cs(_receiveCritSect);
            if (enable)
            {
              // Enable NACK and always wait for retransmits.
                _receiver.SetNackMode(kNack, -1, -1);
            }
            else
            {
                _receiver.SetNackMode(kNoNack, -1, -1);
            }
            break;
        }

    case kProtectionDualDecoder:
        {
            CriticalSectionScoped cs(_receiveCritSect);
            if (enable)
            {
                // Enable NACK but don't wait for retransmissions and don't
                // add any extra delay.
                _receiver.SetNackMode(kNack, 0, 0);
                // Enable NACK and always wait for retransmissions and
                // compensate with extra delay.
                _dualReceiver.SetNackMode(kNack, -1, -1);
                _receiver.SetDecodeWithErrors(true);
            }
            else
            {
                _dualReceiver.SetNackMode(kNoNack, -1, -1);
            }
            break;
        }

    case kProtectionKeyOnLoss:
        {
            CriticalSectionScoped cs(_receiveCritSect);
            if (enable)
            {
                _keyRequestMode = kKeyOnLoss;
                _receiver.SetDecodeWithErrors(true);
            }
            else if (_keyRequestMode == kKeyOnLoss)
            {
                _keyRequestMode = kKeyOnError; // default mode
            }
            else
            {
                return VCM_PARAMETER_ERROR;
            }
            break;
        }

    case kProtectionKeyOnKeyLoss:
        {
            CriticalSectionScoped cs(_receiveCritSect);
            if (enable)
            {
                _keyRequestMode = kKeyOnKeyLoss;
            }
            else if (_keyRequestMode == kKeyOnKeyLoss)
            {
                _keyRequestMode = kKeyOnError; // default mode
            }
            else
            {
                return VCM_PARAMETER_ERROR;
            }
            break;
        }

    case kProtectionNackFEC:
        {
            {
              // Receive side
                CriticalSectionScoped cs(_receiveCritSect);
                if (enable)
                {
                    // Enable hybrid NACK/FEC. Always wait for retransmissions
                    // and don't add extra delay when RTT is above
                    // kLowRttNackMs.
                    _receiver.SetNackMode(kNack,
                                          media_optimization::kLowRttNackMs,
                                          -1);
                    _receiver.SetDecodeWithErrors(false);
                    _receiver.SetDecodeWithErrors(false);
                }
                else
                {
                    _receiver.SetNackMode(kNoNack, -1, -1);
                }
            }
            // Send Side
            {
                CriticalSectionScoped cs(_sendCritSect);
                _mediaOpt.EnableProtectionMethod(enable,
                                                 media_optimization::kNackFec);
            }
            break;
        }

    case kProtectionFEC:
        {
            CriticalSectionScoped cs(_sendCritSect);
            _mediaOpt.EnableProtectionMethod(enable, media_optimization::kFec);
            break;
        }

    case kProtectionPeriodicKeyFrames:
        {
            CriticalSectionScoped cs(_sendCritSect);
            return _codecDataBase.SetPeriodicKeyFrames(enable) ? 0 : -1;
            break;
        }
    }
    return VCM_OK;
}

// Add one raw video frame to the encoder, blocking.
int32_t
VideoCodingModuleImpl::AddVideoFrame(const I420VideoFrame& videoFrame,
                                     const VideoContentMetrics* contentMetrics,
                                     const CodecSpecificInfo* codecSpecificInfo)
{
    CriticalSectionScoped cs(_sendCritSect);
    if (_encoder == NULL)
    {
        return VCM_UNINITIALIZED;
    }
    // TODO(holmer): Add support for dropping frames per stream. Currently we
    // only have one frame dropper for all streams.
    if (_nextFrameTypes[0] == kFrameEmpty)
    {
        return VCM_OK;
    }
    _mediaOpt.UpdateIncomingFrameRate();

    if (_mediaOpt.DropFrame())
    {
        WEBRTC_TRACE(webrtc::kTraceStream,
                     webrtc::kTraceVideoCoding,
                     VCMId(_id),
                     "Drop frame due to bitrate");
    }
    else
    {
        _mediaOpt.UpdateContentData(contentMetrics);
        int32_t ret = _encoder->Encode(videoFrame,
                                             codecSpecificInfo,
                                             _nextFrameTypes);
        if (_encoderInputFile != NULL)
        {
            if (PrintI420VideoFrame(videoFrame, _encoderInputFile) < 0)
            {
                return -1;
            }
        }
        if (ret < 0)
        {
            WEBRTC_TRACE(webrtc::kTraceError,
                         webrtc::kTraceVideoCoding,
                         VCMId(_id),
                         "Encode error: %d", ret);
            return ret;
        }
        for (size_t i = 0; i < _nextFrameTypes.size(); ++i) {
          _nextFrameTypes[i] = kVideoFrameDelta;  // Default frame type.
        }
    }
    return VCM_OK;
}

int32_t VideoCodingModuleImpl::IntraFrameRequest(int stream_index) {
  CriticalSectionScoped cs(_sendCritSect);
  if (stream_index < 0 ||
      static_cast<unsigned int>(stream_index) >= _nextFrameTypes.size()) {
    return -1;
  }
  _nextFrameTypes[stream_index] = kVideoFrameKey;
  if (_encoder != NULL && _encoder->InternalSource()) {
    // Try to request the frame if we have an external encoder with
    // internal source since AddVideoFrame never will be called.
    if (_encoder->RequestFrame(_nextFrameTypes) ==
        WEBRTC_VIDEO_CODEC_OK) {
      _nextFrameTypes[stream_index] = kVideoFrameDelta;
    }
  }
  return VCM_OK;
}

int32_t
VideoCodingModuleImpl::EnableFrameDropper(bool enable)
{
    CriticalSectionScoped cs(_sendCritSect);
    frame_dropper_enabled_ = enable;
    _mediaOpt.EnableFrameDropper(enable);
    return VCM_OK;
}


int32_t
VideoCodingModuleImpl::SentFrameCount(VCMFrameCount &frameCount) const
{
    CriticalSectionScoped cs(_sendCritSect);
    return _mediaOpt.SentFrameCount(frameCount);
}

// Initialize receiver, resets codec database etc
int32_t
VideoCodingModuleImpl::InitializeReceiver()
{
    CriticalSectionScoped cs(_receiveCritSect);
    int32_t ret = _receiver.Initialize();
    if (ret < 0)
    {
        return ret;
    }

    ret = _dualReceiver.Initialize();
    if (ret < 0)
    {
        return ret;
    }
    _codecDataBase.ResetReceiver();
    _timing.Reset();

    _decoder = NULL;
    _decodedFrameCallback.SetUserReceiveCallback(NULL);
    _receiverInited = true;
    _frameTypeCallback = NULL;
    _frameStorageCallback = NULL;
    _receiveStatsCallback = NULL;
    _packetRequestCallback = NULL;
    _keyRequestMode = kKeyOnError;
    _scheduleKeyRequest = false;

    return VCM_OK;
}

// Register a receive callback. Will be called whenever there is a new frame
// ready for rendering.
int32_t
VideoCodingModuleImpl::RegisterReceiveCallback(
    VCMReceiveCallback* receiveCallback)
{
    CriticalSectionScoped cs(_receiveCritSect);
    _decodedFrameCallback.SetUserReceiveCallback(receiveCallback);
    return VCM_OK;
}

int32_t
VideoCodingModuleImpl::RegisterReceiveStatisticsCallback(
                                     VCMReceiveStatisticsCallback* receiveStats)
{
    CriticalSectionScoped cs(_receiveCritSect);
    _receiveStatsCallback = receiveStats;
    return VCM_OK;
}

// Register an externally defined decoder/render object.
// Can be a decoder only or a decoder coupled with a renderer.
int32_t
VideoCodingModuleImpl::RegisterExternalDecoder(VideoDecoder* externalDecoder,
                                               uint8_t payloadType,
                                               bool internalRenderTiming)
{
    CriticalSectionScoped cs(_receiveCritSect);
    if (externalDecoder == NULL)
    {
        // Make sure the VCM updates the decoder next time it decodes.
        _decoder = NULL;
        return _codecDataBase.DeregisterExternalDecoder(payloadType) ? 0 : -1;
    }
    return _codecDataBase.RegisterExternalDecoder(
        externalDecoder, payloadType, internalRenderTiming) ? 0 : -1;
}

// Register a frame type request callback.
int32_t
VideoCodingModuleImpl::RegisterFrameTypeCallback(
    VCMFrameTypeCallback* frameTypeCallback)
{
    CriticalSectionScoped cs(_receiveCritSect);
    _frameTypeCallback = frameTypeCallback;
    return VCM_OK;
}

int32_t
VideoCodingModuleImpl::RegisterFrameStorageCallback(
    VCMFrameStorageCallback* frameStorageCallback)
{
    CriticalSectionScoped cs(_receiveCritSect);
    _frameStorageCallback = frameStorageCallback;
    return VCM_OK;
}

int32_t
VideoCodingModuleImpl::RegisterPacketRequestCallback(
    VCMPacketRequestCallback* callback)
{
    CriticalSectionScoped cs(_receiveCritSect);
    _packetRequestCallback = callback;
    return VCM_OK;
}

int VideoCodingModuleImpl::RegisterRenderBufferSizeCallback(
  VCMRenderBufferSizeCallback* callback) {
  CriticalSectionScoped cs(_receiveCritSect);
  render_buffer_callback_ = callback;
  return VCM_OK;
}

// Decode next frame, blocking.
// Should be called as often as possible to get the most out of the decoder.
int32_t
VideoCodingModuleImpl::Decode(uint16_t maxWaitTimeMs)
{
    TRACE_EVENT1("webrtc", "VCM::Decode", "max_wait", maxWaitTimeMs);
    int64_t nextRenderTimeMs;
    {
        CriticalSectionScoped cs(_receiveCritSect);
        if (!_receiverInited)
        {
            return VCM_UNINITIALIZED;
        }
        if (!_codecDataBase.DecoderRegistered())
        {
            return VCM_NO_CODEC_REGISTERED;
        }
    }

    const bool dualReceiverEnabledNotReceiving =
        (_dualReceiver.State() != kReceiving &&
         _dualReceiver.NackMode() == kNack);

    VCMEncodedFrame* frame = _receiver.FrameForDecoding(
        maxWaitTimeMs,
        nextRenderTimeMs,
        _codecDataBase.SupportsRenderScheduling(),
        &_dualReceiver);

    if (dualReceiverEnabledNotReceiving && _dualReceiver.State() == kReceiving)
    {
        // Dual receiver is enabled (kNACK enabled), but was not receiving
        // before the call to FrameForDecoding(). After the call the state
        // changed to receiving, and therefore we must copy the primary decoder
        // state to the dual decoder to make it possible for the dual decoder to
        // start decoding retransmitted frames and recover.
        CriticalSectionScoped cs(_receiveCritSect);
        if (_dualDecoder != NULL)
        {
            _codecDataBase.ReleaseDecoder(_dualDecoder);
        }
        _dualDecoder = _codecDataBase.CreateDecoderCopy();
        if (_dualDecoder != NULL)
        {
            _dualDecoder->RegisterDecodeCompleteCallback(
                &_dualDecodedFrameCallback);
        }
        else
        {
            _dualReceiver.Reset();
        }
    }

    if (frame == NULL)
      return VCM_FRAME_NOT_READY;
    else
    {
        CriticalSectionScoped cs(_receiveCritSect);

        // If this frame was too late, we should adjust the delay accordingly
        _timing.UpdateCurrentDelay(frame->RenderTimeMs(),
                                   clock_->TimeInMilliseconds());

#ifdef DEBUG_DECODER_BIT_STREAM
        if (_bitStreamBeforeDecoder != NULL)
        {
          // Write bit stream to file for debugging purposes
          if (fwrite(frame->Buffer(), 1, frame->Length(),
                     _bitStreamBeforeDecoder) !=  frame->Length()) {
            return -1;
          }
        }
#endif
        if (_frameStorageCallback != NULL)
        {
            int32_t ret = frame->Store(*_frameStorageCallback);
            if (ret < 0)
            {
                return ret;
            }
        }

        const int32_t ret = Decode(*frame);
        _receiver.ReleaseFrame(frame);
        frame = NULL;
        if (ret != VCM_OK)
        {
            return ret;
        }
    }
    return VCM_OK;
}

int32_t
VideoCodingModuleImpl::RequestSliceLossIndication(
    const uint64_t pictureID) const
{
    TRACE_EVENT1("webrtc", "RequestSLI", "picture_id", pictureID);
    if (_frameTypeCallback != NULL)
    {
        const int32_t ret =
            _frameTypeCallback->SliceLossIndicationRequest(pictureID);
        if (ret < 0)
        {
            WEBRTC_TRACE(webrtc::kTraceError,
                         webrtc::kTraceVideoCoding,
                         VCMId(_id),
                         "Failed to request key frame");
            return ret;
        }
    } else
    {
        WEBRTC_TRACE(webrtc::kTraceWarning,
                     webrtc::kTraceVideoCoding,
                     VCMId(_id),
                     "No frame type request callback registered");
        return VCM_MISSING_CALLBACK;
    }
    return VCM_OK;
}

int32_t
VideoCodingModuleImpl::RequestKeyFrame()
{
    TRACE_EVENT0("webrtc", "RequestKeyFrame");
    if (_frameTypeCallback != NULL)
    {
        const int32_t ret = _frameTypeCallback->RequestKeyFrame();
        if (ret < 0)
        {
            WEBRTC_TRACE(webrtc::kTraceError,
                         webrtc::kTraceVideoCoding,
                         VCMId(_id),
                         "Failed to request key frame");
            return ret;
        }
        _scheduleKeyRequest = false;
    }
    else
    {
        WEBRTC_TRACE(webrtc::kTraceWarning,
                     webrtc::kTraceVideoCoding,
                     VCMId(_id),
                     "No frame type request callback registered");
        return VCM_MISSING_CALLBACK;
    }
    return VCM_OK;
}

int32_t
VideoCodingModuleImpl::DecodeDualFrame(uint16_t maxWaitTimeMs)
{
    CriticalSectionScoped cs(_receiveCritSect);
    if (_dualReceiver.State() != kReceiving ||
        _dualReceiver.NackMode() != kNack)
    {
        // The dual receiver is currently not receiving or
        // dual decoder mode is disabled.
        return VCM_OK;
    }
    int64_t dummyRenderTime;
    int32_t decodeCount = 0;
    // The dual decoder's state is copied from the main decoder, which may
    // decode with errors. Make sure that the dual decoder does not introduce
    // error.
    _dualReceiver.SetDecodeWithErrors(false);
    VCMEncodedFrame* dualFrame = _dualReceiver.FrameForDecoding(
                                                            maxWaitTimeMs,
                                                            dummyRenderTime);
    if (dualFrame != NULL && _dualDecoder != NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceStream,
                     webrtc::kTraceVideoCoding,
                     VCMId(_id),
                     "Decoding frame %u with dual decoder",
                     dualFrame->TimeStamp());
        // Decode dualFrame and try to catch up
        int32_t ret = _dualDecoder->Decode(*dualFrame,
                                                 clock_->TimeInMilliseconds());
        if (ret != WEBRTC_VIDEO_CODEC_OK)
        {
            WEBRTC_TRACE(webrtc::kTraceWarning,
                         webrtc::kTraceVideoCoding,
                         VCMId(_id),
                         "Failed to decode frame with dual decoder");
            _dualReceiver.ReleaseFrame(dualFrame);
            return VCM_CODEC_ERROR;
        }
        if (_receiver.DualDecoderCaughtUp(dualFrame, _dualReceiver))
        {
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
int32_t
VideoCodingModuleImpl::Decode(const VCMEncodedFrame& frame)
{
    TRACE_EVENT2("webrtc", "Decode",
                 "timestamp", frame.TimeStamp(),
                 "type", frame.FrameType());
    // Change decoder if payload type has changed
    const bool renderTimingBefore = _codecDataBase.SupportsRenderScheduling();
    _decoder = _codecDataBase.GetDecoder(frame.PayloadType(),
                                         &_decodedFrameCallback);
    if (renderTimingBefore != _codecDataBase.SupportsRenderScheduling())
    {
        // Make sure we reset the decode time estimate since it will
        // be zero for codecs without render timing.
        _timing.ResetDecodeTime();
    }
    if (_decoder == NULL)
    {
        return VCM_NO_CODEC_REGISTERED;
    }
    // Decode a frame
    int32_t ret = _decoder->Decode(frame, clock_->TimeInMilliseconds());

    // Check for failed decoding, run frame type request callback if needed.
    if (ret < 0)
    {
        if (ret == VCM_ERROR_REQUEST_SLI)
        {
            return RequestSliceLossIndication(
                    _decodedFrameCallback.LastReceivedPictureID() + 1);
        }
        else
        {
            WEBRTC_TRACE(webrtc::kTraceError,
                         webrtc::kTraceVideoCoding,
                         VCMId(_id),
                         "Failed to decode frame %u, requesting key frame",
                         frame.TimeStamp());
            ret = RequestKeyFrame();
        }
    }
    else if (ret == VCM_REQUEST_SLI)
    {
        ret = RequestSliceLossIndication(
            _decodedFrameCallback.LastReceivedPictureID() + 1);
    }
    if (!frame.Complete() || frame.MissingFrame())
    {
        switch (_keyRequestMode)
        {
            case kKeyOnKeyLoss:
            {
                if (frame.FrameType() == kVideoFrameKey)
                {
                    _scheduleKeyRequest = true;
                    return VCM_OK;
                }
                break;
            }
            case kKeyOnLoss:
            {
                _scheduleKeyRequest = true;
                return VCM_OK;
            }
            default:
                break;
        }
    }
    return ret;
}

int32_t
VideoCodingModuleImpl::DecodeFromStorage(
    const EncodedVideoData& frameFromStorage)
{
    CriticalSectionScoped cs(_receiveCritSect);
    int32_t ret = _frameFromFile.ExtractFromStorage(frameFromStorage);
    if (ret < 0)
    {
        return ret;
    }
    return Decode(_frameFromFile);
}

// Reset the decoder state
int32_t
VideoCodingModuleImpl::ResetDecoder()
{
    CriticalSectionScoped cs(_receiveCritSect);
    if (_decoder != NULL)
    {
        _receiver.Initialize();
        _timing.Reset();
        _scheduleKeyRequest = false;
        _decoder->Reset();
    }
    if (_dualReceiver.State() != kPassive)
    {
        _dualReceiver.Initialize();
    }
    if (_dualDecoder != NULL)
    {
        _codecDataBase.ReleaseDecoder(_dualDecoder);
        _dualDecoder = NULL;
    }
    return VCM_OK;
}

// Register possible receive codecs, can be called multiple times
int32_t
VideoCodingModuleImpl::RegisterReceiveCodec(const VideoCodec* receiveCodec,
                                                int32_t numberOfCores,
                                                bool requireKeyFrame)
{
    CriticalSectionScoped cs(_receiveCritSect);
    if (receiveCodec == NULL)
    {
        return VCM_PARAMETER_ERROR;
    }
    if (!_codecDataBase.RegisterReceiveCodec(receiveCodec, numberOfCores,
                                             requireKeyFrame)) {
      return -1;
    }
    return 0;
}

// Get current received codec
int32_t
VideoCodingModuleImpl::ReceiveCodec(VideoCodec* currentReceiveCodec) const
{
    CriticalSectionScoped cs(_receiveCritSect);
    if (currentReceiveCodec == NULL)
    {
        return VCM_PARAMETER_ERROR;
    }
    return _codecDataBase.ReceiveCodec(currentReceiveCodec) ? 0 : -1;
}

// Get current received codec
VideoCodecType
VideoCodingModuleImpl::ReceiveCodec() const
{
    CriticalSectionScoped cs(_receiveCritSect);
    return _codecDataBase.ReceiveCodec();
}

// Incoming packet from network parsed and ready for decode, non blocking.
int32_t
VideoCodingModuleImpl::IncomingPacket(const uint8_t* incomingPayload,
                                    uint32_t payloadLength,
                                    const WebRtcRTPHeader& rtpInfo)
{
    if (rtpInfo.frameType == kVideoFrameKey) {
      TRACE_EVENT1("webrtc", "VCM::PacketKeyFrame",
                   "seqnum", rtpInfo.header.sequenceNumber);
    } else {
      TRACE_EVENT2("webrtc", "VCM::Packet",
                   "seqnum", rtpInfo.header.sequenceNumber,
                   "type", rtpInfo.frameType);
    }
    if (incomingPayload == NULL) {
      // The jitter buffer doesn't handle non-zero payload lengths for packets
      // without payload.
      // TODO(holmer): We should fix this in the jitter buffer.
      payloadLength = 0;
    }
    const VCMPacket packet(incomingPayload, payloadLength, rtpInfo);
    int32_t ret;
    if (_dualReceiver.State() != kPassive)
    {
        ret = _dualReceiver.InsertPacket(packet,
                                         rtpInfo.type.Video.width,
                                         rtpInfo.type.Video.height);
        if (ret == VCM_FLUSH_INDICATOR) {
          RequestKeyFrame();
          ResetDecoder();
        } else if (ret < 0) {
          return ret;
        }
    }
    ret = _receiver.InsertPacket(packet,
                                 rtpInfo.type.Video.width,
                                 rtpInfo.type.Video.height);
    // TODO(holmer): Investigate if this somehow should use the key frame
    // request scheduling to throttle the requests.
    if (ret == VCM_FLUSH_INDICATOR) {
      RequestKeyFrame();
      ResetDecoder();
    } else if (ret < 0) {
      return ret;
    }
    return VCM_OK;
}

// Minimum playout delay (used for lip-sync). This is the minimum delay required
// to sync with audio. Not included in  VideoCodingModule::Delay()
// Defaults to 0 ms.
int32_t
VideoCodingModuleImpl::SetMinimumPlayoutDelay(uint32_t minPlayoutDelayMs)
{
    _timing.set_min_playout_delay(minPlayoutDelayMs);
    return VCM_OK;
}

// The estimated delay caused by rendering, defaults to
// kDefaultRenderDelayMs = 10 ms
int32_t
VideoCodingModuleImpl::SetRenderDelay(uint32_t timeMS)
{
    _timing.set_render_delay(timeMS);
    return VCM_OK;
}

// Current video delay
int32_t
VideoCodingModuleImpl::Delay() const
{
    return _timing.TargetVideoDelay();
}

// Nack list
int32_t
VideoCodingModuleImpl::NackList(uint16_t* nackList, uint16_t& size)
{
    VCMNackStatus nackStatus = kNackOk;
    uint16_t nack_list_length = 0;
    // Collect sequence numbers from the default receiver
    // if in normal nack mode. Otherwise collect them from
    // the dual receiver if the dual receiver is receiving.
    if (_receiver.NackMode() != kNoNack)
    {
        nackStatus = _receiver.NackList(nackList, size, &nack_list_length);
    }
    if (nack_list_length == 0 && _dualReceiver.State() != kPassive)
    {
        nackStatus = _dualReceiver.NackList(nackList, size, &nack_list_length);
    }
    size = nack_list_length;

    switch (nackStatus)
    {
    case kNackNeedMoreMemory:
        {
            WEBRTC_TRACE(webrtc::kTraceError,
                         webrtc::kTraceVideoCoding,
                         VCMId(_id),
                         "Out of memory");
            return VCM_MEMORY;
        }
    case kNackKeyFrameRequest:
        {
            CriticalSectionScoped cs(_receiveCritSect);
            WEBRTC_TRACE(webrtc::kTraceWarning,
                         webrtc::kTraceVideoCoding,
                         VCMId(_id),
                         "Failed to get NACK list, requesting key frame");
            return RequestKeyFrame();
        }
    default:
        break;
    }
    return VCM_OK;
}

int32_t
VideoCodingModuleImpl::ReceivedFrameCount(VCMFrameCount& frameCount) const
{
    _receiver.ReceivedFrameCount(&frameCount);
    return VCM_OK;
}

uint32_t VideoCodingModuleImpl::DiscardedPackets() const {
  return _receiver.DiscardedPackets();
}

int VideoCodingModuleImpl::SetSenderNackMode(SenderNackMode mode) {
  CriticalSectionScoped cs(_sendCritSect);

  switch (mode) {
    case kNackNone:
      _mediaOpt.EnableProtectionMethod(false, media_optimization::kNack);
      break;
    case kNackAll:
      _mediaOpt.EnableProtectionMethod(true, media_optimization::kNack);
      break;
    case kNackSelective:
      return VCM_NOT_IMPLEMENTED;
      break;
  }
  return VCM_OK;
}

int VideoCodingModuleImpl::SetSenderReferenceSelection(bool enable) {
  return VCM_NOT_IMPLEMENTED;
}

int VideoCodingModuleImpl::SetSenderFEC(bool enable) {
  CriticalSectionScoped cs(_sendCritSect);
  _mediaOpt.EnableProtectionMethod(enable, media_optimization::kFec);
  return VCM_OK;
}

int VideoCodingModuleImpl::SetSenderKeyFramePeriod(int periodMs) {
  return VCM_NOT_IMPLEMENTED;
}

int VideoCodingModuleImpl::SetReceiverRobustnessMode(
    ReceiverRobustness robustnessMode,
    DecodeErrors errorMode) {
  CriticalSectionScoped cs(_receiveCritSect);
  switch (robustnessMode) {
    case kNone:
      _receiver.SetNackMode(kNoNack, -1, -1);
      _dualReceiver.SetNackMode(kNoNack, -1, -1);
      if (errorMode == kNoDecodeErrors) {
        _keyRequestMode = kKeyOnLoss;
      } else {
        _keyRequestMode = kKeyOnError;
      }
      break;
    case kHardNack:
      if (errorMode == kAllowDecodeErrors) {
        return VCM_PARAMETER_ERROR;
      }
      // Always wait for retransmissions.
      _receiver.SetNackMode(kNack, -1, -1);
      _dualReceiver.SetNackMode(kNoNack, -1, -1);
      _keyRequestMode = kKeyOnError;  // TODO(hlundin): On long NACK list?
      break;
    case kSoftNack:
      assert(false); // TODO(hlundin): Not completed.
      return VCM_NOT_IMPLEMENTED;
      // Enable hybrid NACK/FEC. Always wait for retransmissions and don't add
      // extra delay when RTT is above kLowRttNackMs.
      _receiver.SetNackMode(kNack, media_optimization::kLowRttNackMs, -1);
      _dualReceiver.SetNackMode(kNoNack, -1, -1);
      _keyRequestMode = kKeyOnError;
      break;
    case kDualDecoder:
      if (errorMode == kNoDecodeErrors) {
        return VCM_PARAMETER_ERROR;
      }
      // Enable NACK but don't wait for retransmissions and don't add any extra
      // delay.
      _receiver.SetNackMode(kNack, 0, 0);
      // Enable NACK, compensate with extra delay and wait for retransmissions.
      _dualReceiver.SetNackMode(kNack, -1, -1);
      _keyRequestMode = kKeyOnError;
      break;
    case kReferenceSelection:
      assert(false); // TODO(hlundin): Not completed.
      return VCM_NOT_IMPLEMENTED;
      if (errorMode == kNoDecodeErrors) {
        return VCM_PARAMETER_ERROR;
      }
      _receiver.SetNackMode(kNoNack, -1, -1);
      _dualReceiver.SetNackMode(kNoNack, -1, -1);
      break;
  }
  _receiver.SetDecodeWithErrors(errorMode == kAllowDecodeErrors);
  // The dual decoder should never decode with errors.
  _dualReceiver.SetDecodeWithErrors(false);
  return VCM_OK;
}

void VideoCodingModuleImpl::SetNackSettings(size_t max_nack_list_size,
                                            int max_packet_age_to_nack,
                                            int max_incomplete_time_ms) {
  if (max_nack_list_size != 0) {
    CriticalSectionScoped cs(_receiveCritSect);
    max_nack_list_size_ = max_nack_list_size;
  }
  _receiver.SetNackSettings(max_nack_list_size, max_packet_age_to_nack,
                            max_incomplete_time_ms);
  _dualReceiver.SetNackSettings(max_nack_list_size, max_packet_age_to_nack,
                                max_incomplete_time_ms);
}

int VideoCodingModuleImpl::SetMinReceiverDelay(int desired_delay_ms) {
  return _receiver.SetMinReceiverDelay(desired_delay_ms);
}

int VideoCodingModuleImpl::StartDebugRecording(const char* file_name_utf8) {
  CriticalSectionScoped cs(_sendCritSect);
  _encoderInputFile = fopen(file_name_utf8, "wb");
  if (_encoderInputFile == NULL)
    return VCM_GENERAL_ERROR;
  return VCM_OK;
}

int VideoCodingModuleImpl::StopDebugRecording(){
  CriticalSectionScoped cs(_sendCritSect);
  if (_encoderInputFile != NULL) {
    fclose(_encoderInputFile);
    _encoderInputFile = NULL;
  }
  return VCM_OK;
}

}  // namespace webrtc
