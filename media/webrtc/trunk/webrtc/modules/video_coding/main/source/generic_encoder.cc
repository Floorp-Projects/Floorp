/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "encoded_frame.h"
#include "generic_encoder.h"
#include "media_optimization.h"
#include "../../../../engine_configurations.h"

namespace webrtc {

//#define DEBUG_ENCODER_BIT_STREAM

VCMGenericEncoder::VCMGenericEncoder(VideoEncoder& encoder, bool internalSource /*= false*/)
:
_encoder(encoder),
_codecType(kVideoCodecUnknown),
_VCMencodedFrameCallback(NULL),
_bitRate(0),
_frameRate(0),
_internalSource(internalSource)
{
}


VCMGenericEncoder::~VCMGenericEncoder()
{
}

int32_t VCMGenericEncoder::Release()
{
    _bitRate = 0;
    _frameRate = 0;
    _VCMencodedFrameCallback = NULL;
    return _encoder.Release();
}

int32_t
VCMGenericEncoder::InitEncode(const VideoCodec* settings,
                              int32_t numberOfCores,
                              uint32_t maxPayloadSize)
{
    _bitRate = settings->startBitrate * 1000;
    _frameRate = settings->maxFramerate;
    _codecType = settings->codecType;
    if (_VCMencodedFrameCallback != NULL)
    {
        _VCMencodedFrameCallback->SetCodecType(_codecType);
    }
    return _encoder.InitEncode(settings, numberOfCores, maxPayloadSize);
}

int32_t
VCMGenericEncoder::Encode(const I420VideoFrame& inputFrame,
                          const CodecSpecificInfo* codecSpecificInfo,
                          const std::vector<FrameType>& frameTypes) {
  std::vector<VideoFrameType> video_frame_types(frameTypes.size(),
                                                kDeltaFrame);
  VCMEncodedFrame::ConvertFrameTypes(frameTypes, &video_frame_types);
  return _encoder.Encode(inputFrame, codecSpecificInfo, &video_frame_types);
}

int32_t
VCMGenericEncoder::SetChannelParameters(int32_t packetLoss, int rtt)
{
    return _encoder.SetChannelParameters(packetLoss, rtt);
}

int32_t
VCMGenericEncoder::SetRates(uint32_t newBitRate, uint32_t frameRate)
{
    uint32_t target_bitrate_kbps = (newBitRate + 500) / 1000;
    int32_t ret = _encoder.SetRates(target_bitrate_kbps, frameRate);
    if (ret < 0)
    {
        return ret;
    }
    _bitRate = newBitRate;
    _frameRate = frameRate;
    return VCM_OK;
}

int32_t
VCMGenericEncoder::CodecConfigParameters(uint8_t* buffer, int32_t size)
{
    int32_t ret = _encoder.CodecConfigParameters(buffer, size);
    if (ret < 0)
    {
        return ret;
    }
    return ret;
}

uint32_t VCMGenericEncoder::BitRate() const
{
    return _bitRate;
}

uint32_t VCMGenericEncoder::FrameRate() const
{
    return _frameRate;
}

int32_t
VCMGenericEncoder::SetPeriodicKeyFrames(bool enable)
{
    return _encoder.SetPeriodicKeyFrames(enable);
}

int32_t VCMGenericEncoder::RequestFrame(
    const std::vector<FrameType>& frame_types) {
  I420VideoFrame image;
  std::vector<VideoFrameType> video_frame_types(frame_types.size(),
                                                kDeltaFrame);
  VCMEncodedFrame::ConvertFrameTypes(frame_types, &video_frame_types);
  return _encoder.Encode(image, NULL, &video_frame_types);
}

int32_t
VCMGenericEncoder::RegisterEncodeCallback(VCMEncodedFrameCallback* VCMencodedFrameCallback)
{
   _VCMencodedFrameCallback = VCMencodedFrameCallback;

   _VCMencodedFrameCallback->SetCodecType(_codecType);
   _VCMencodedFrameCallback->SetInternalSource(_internalSource);
   return _encoder.RegisterEncodeCompleteCallback(_VCMencodedFrameCallback);
}

bool
VCMGenericEncoder::InternalSource() const
{
    return _internalSource;
}

 /***************************
  * Callback Implementation
  ***************************/
VCMEncodedFrameCallback::VCMEncodedFrameCallback():
_sendCallback(),
_mediaOpt(NULL),
_encodedBytes(0),
_payloadType(0),
_codecType(kVideoCodecUnknown),
_internalSource(false)
#ifdef DEBUG_ENCODER_BIT_STREAM
, _bitStreamAfterEncoder(NULL)
#endif
{
#ifdef DEBUG_ENCODER_BIT_STREAM
    _bitStreamAfterEncoder = fopen("encoderBitStream.bit", "wb");
#endif
}

VCMEncodedFrameCallback::~VCMEncodedFrameCallback()
{
#ifdef DEBUG_ENCODER_BIT_STREAM
    fclose(_bitStreamAfterEncoder);
#endif
}

int32_t
VCMEncodedFrameCallback::SetTransportCallback(VCMPacketizationCallback* transport)
{
    _sendCallback = transport;
    return VCM_OK;
}

int32_t
VCMEncodedFrameCallback::Encoded(
    EncodedImage &encodedImage,
    const CodecSpecificInfo* codecSpecificInfo,
    const RTPFragmentationHeader* fragmentationHeader)
{
    FrameType frameType = VCMEncodedFrame::ConvertFrameType(encodedImage._frameType);

    uint32_t encodedBytes = 0;
    if (_sendCallback != NULL)
    {
        encodedBytes = encodedImage._length;

#ifdef DEBUG_ENCODER_BIT_STREAM
        if (_bitStreamAfterEncoder != NULL)
        {
            fwrite(encodedImage._buffer, 1, encodedImage._length, _bitStreamAfterEncoder);
        }
#endif

        RTPVideoHeader rtpVideoHeader;
        RTPVideoHeader* rtpVideoHeaderPtr = &rtpVideoHeader;
        if (codecSpecificInfo)
        {
            CopyCodecSpecific(*codecSpecificInfo, &rtpVideoHeaderPtr);
        }
        else
        {
            rtpVideoHeaderPtr = NULL;
        }

        int32_t callbackReturn = _sendCallback->SendData(
            frameType,
            _payloadType,
            encodedImage._timeStamp,
            encodedImage.capture_time_ms_,
            encodedImage._buffer,
            encodedBytes,
            *fragmentationHeader,
            rtpVideoHeaderPtr);
       if (callbackReturn < 0)
       {
           return callbackReturn;
       }
    }
    else
    {
        return VCM_UNINITIALIZED;
    }
    _encodedBytes = encodedBytes;
    if (_mediaOpt != NULL) {
      _mediaOpt->UpdateWithEncodedData(_encodedBytes, encodedImage._timeStamp,
                                       frameType);
      if (_internalSource)
      {
          return _mediaOpt->DropFrame(); // Signal to encoder to drop next frame
      }
    }
    return VCM_OK;
}

uint32_t
VCMEncodedFrameCallback::EncodedBytes()
{
    return _encodedBytes;
}

void
VCMEncodedFrameCallback::SetMediaOpt(
    media_optimization::VCMMediaOptimization *mediaOpt)
{
    _mediaOpt = mediaOpt;
}

void VCMEncodedFrameCallback::CopyCodecSpecific(const CodecSpecificInfo& info,
                                                RTPVideoHeader** rtp) {
    switch (info.codecType) {
        case kVideoCodecVP8: {
            (*rtp)->codecHeader.VP8.InitRTPVideoHeaderVP8();
            (*rtp)->codecHeader.VP8.pictureId =
                info.codecSpecific.VP8.pictureId;
            (*rtp)->codecHeader.VP8.nonReference =
                info.codecSpecific.VP8.nonReference;
            (*rtp)->codecHeader.VP8.temporalIdx =
                info.codecSpecific.VP8.temporalIdx;
            (*rtp)->codecHeader.VP8.layerSync =
                info.codecSpecific.VP8.layerSync;
            (*rtp)->codecHeader.VP8.tl0PicIdx =
                info.codecSpecific.VP8.tl0PicIdx;
            (*rtp)->codecHeader.VP8.keyIdx =
                info.codecSpecific.VP8.keyIdx;
            (*rtp)->simulcastIdx = info.codecSpecific.VP8.simulcastIdx;
            return;
        }
        default: {
            // No codec specific info. Change RTP header pointer to NULL.
            *rtp = NULL;
            return;
        }
    }
}
} // namespace webrtc
