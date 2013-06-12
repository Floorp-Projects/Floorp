/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video_coding.h"
#include "trace.h"
#include "trace_event.h"
#include "generic_decoder.h"
#include "internal_defines.h"
#include "webrtc/system_wrappers/interface/clock.h"

namespace webrtc {

VCMDecodedFrameCallback::VCMDecodedFrameCallback(VCMTiming& timing,
                                                 Clock* clock)
:
_critSect(CriticalSectionWrapper::CreateCriticalSection()),
_clock(clock),
_receiveCallback(NULL),
_timing(timing),
_timestampMap(kDecoderFrameMemoryLength),
_lastReceivedPictureID(0)
{
}

VCMDecodedFrameCallback::~VCMDecodedFrameCallback()
{
    delete _critSect;
}

void VCMDecodedFrameCallback::SetUserReceiveCallback(
    VCMReceiveCallback* receiveCallback)
{
    CriticalSectionScoped cs(_critSect);
    _receiveCallback = receiveCallback;
}

int32_t VCMDecodedFrameCallback::Decoded(I420VideoFrame& decodedImage)
{
    // TODO(holmer): We should improve this so that we can handle multiple
    // callbacks from one call to Decode().
    CriticalSectionScoped cs(_critSect);
    VCMFrameInformation* frameInfo = static_cast<VCMFrameInformation*>(
        _timestampMap.Pop(decodedImage.timestamp()));
    if (frameInfo == NULL)
    {
        // The map should never be empty or full if this callback is called.
        return WEBRTC_VIDEO_CODEC_ERROR;
    }

    _timing.StopDecodeTimer(
        decodedImage.timestamp(),
        frameInfo->decodeStartTimeMs,
        _clock->TimeInMilliseconds());

    if (_receiveCallback != NULL)
    {
        _frame.SwapFrame(&decodedImage);
        _frame.set_render_time_ms(frameInfo->renderTimeMs);
        int32_t callbackReturn = _receiveCallback->FrameToRender(_frame);
        if (callbackReturn < 0)
        {
            WEBRTC_TRACE(webrtc::kTraceDebug,
                         webrtc::kTraceVideoCoding,
                         -1,
                         "Render callback returned error: %d", callbackReturn);
        }
    }
    return WEBRTC_VIDEO_CODEC_OK;
}

int32_t
VCMDecodedFrameCallback::ReceivedDecodedReferenceFrame(
    const uint64_t pictureId)
{
    CriticalSectionScoped cs(_critSect);
    if (_receiveCallback != NULL)
    {
        return _receiveCallback->ReceivedDecodedReferenceFrame(pictureId);
    }
    return -1;
}

int32_t
VCMDecodedFrameCallback::ReceivedDecodedFrame(const uint64_t pictureId)
{
    _lastReceivedPictureID = pictureId;
    return 0;
}

uint64_t VCMDecodedFrameCallback::LastReceivedPictureID() const
{
    return _lastReceivedPictureID;
}

int32_t VCMDecodedFrameCallback::Map(uint32_t timestamp, VCMFrameInformation* frameInfo)
{
    CriticalSectionScoped cs(_critSect);
    return _timestampMap.Add(timestamp, frameInfo);
}

int32_t VCMDecodedFrameCallback::Pop(uint32_t timestamp)
{
    CriticalSectionScoped cs(_critSect);
    if (_timestampMap.Pop(timestamp) == NULL)
    {
        return VCM_GENERAL_ERROR;
    }
    return VCM_OK;
}

VCMGenericDecoder::VCMGenericDecoder(VideoDecoder& decoder, int32_t id, bool isExternal)
:
_id(id),
_callback(NULL),
_frameInfos(),
_nextFrameInfoIdx(0),
_decoder(decoder),
_codecType(kVideoCodecUnknown),
_isExternal(isExternal),
_requireKeyFrame(false),
_keyFrameDecoded(false)
{
}

VCMGenericDecoder::~VCMGenericDecoder()
{
}

int32_t VCMGenericDecoder::InitDecode(const VideoCodec* settings,
                                            int32_t numberOfCores,
                                            bool requireKeyFrame)
{
    _requireKeyFrame = requireKeyFrame;
    _keyFrameDecoded = false;
    _codecType = settings->codecType;

    return _decoder.InitDecode(settings, numberOfCores);
}

int32_t VCMGenericDecoder::Decode(const VCMEncodedFrame& frame,
                                        int64_t nowMs)
{
    if (_requireKeyFrame &&
        !_keyFrameDecoded &&
        frame.FrameType() != kVideoFrameKey &&
        frame.FrameType() != kVideoFrameGolden)
    {
        // Require key frame is enabled, meaning that one key frame must be decoded
        // before we can decode delta frames.
        return VCM_CODEC_ERROR;
    }
    _frameInfos[_nextFrameInfoIdx].decodeStartTimeMs = nowMs;
    _frameInfos[_nextFrameInfoIdx].renderTimeMs = frame.RenderTimeMs();
    _callback->Map(frame.TimeStamp(), &_frameInfos[_nextFrameInfoIdx]);

    WEBRTC_TRACE(webrtc::kTraceDebug,
                 webrtc::kTraceVideoCoding,
                 VCMId(_id),
                 "Decoding timestamp %u", frame.TimeStamp());

    _nextFrameInfoIdx = (_nextFrameInfoIdx + 1) % kDecoderFrameMemoryLength;
    int32_t ret = _decoder.Decode(frame.EncodedImage(),
                                        frame.MissingFrame(),
                                        frame.FragmentationHeader(),
                                        frame.CodecSpecific(),
                                        frame.RenderTimeMs());

    if (ret < WEBRTC_VIDEO_CODEC_OK)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCoding, VCMId(_id), "Decoder error: %d\n", ret);
        _callback->Pop(frame.TimeStamp());
        return ret;
    }
    else if (ret == WEBRTC_VIDEO_CODEC_NO_OUTPUT ||
             ret == WEBRTC_VIDEO_CODEC_REQUEST_SLI)
    {
        // No output
        _callback->Pop(frame.TimeStamp());
    }
    // Update the key frame decoded variable so that we know whether or not we've decoded a key frame since reset.
    _keyFrameDecoded = (_keyFrameDecoded ||
        frame.FrameType() == kVideoFrameKey);
    return ret;
}

int32_t
VCMGenericDecoder::Release()
{
    _keyFrameDecoded = false;
    return _decoder.Release();
}

int32_t VCMGenericDecoder::Reset()
{
    _keyFrameDecoded = false;
    return _decoder.Reset();
}

int32_t VCMGenericDecoder::SetCodecConfigParameters(const uint8_t* buffer, int32_t size)
{
    return _decoder.SetCodecConfigParameters(buffer, size);
}

int32_t VCMGenericDecoder::RegisterDecodeCompleteCallback(VCMDecodedFrameCallback* callback)
{
    _callback = callback;
    return _decoder.RegisterDecodeCompleteCallback(callback);
}

bool VCMGenericDecoder::External() const
{
    return _isExternal;
}

} // namespace
