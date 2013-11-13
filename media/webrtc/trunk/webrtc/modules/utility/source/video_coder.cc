/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifdef WEBRTC_MODULE_UTILITY_VIDEO

#include "webrtc/modules/utility/source/video_coder.h"

namespace webrtc {
VideoCoder::VideoCoder(uint32_t instanceID)
    : _vcm(VideoCodingModule::Create(instanceID)),
      _decodedVideo(0)
{
    _vcm->InitializeSender();
    _vcm->InitializeReceiver();

    _vcm->RegisterTransportCallback(this);
    _vcm->RegisterReceiveCallback(this);
}

VideoCoder::~VideoCoder()
{
    VideoCodingModule::Destroy(_vcm);
}

int32_t VideoCoder::SetEncodeCodec(VideoCodec& videoCodecInst,
                                   uint32_t numberOfCores,
                                   uint32_t maxPayloadSize)
{
    if(_vcm->RegisterSendCodec(&videoCodecInst, numberOfCores,
                               maxPayloadSize) != VCM_OK)
    {
        return -1;
    }
    return 0;
}


int32_t VideoCoder::SetDecodeCodec(VideoCodec& videoCodecInst,
                                   int32_t numberOfCores)
{
    if (videoCodecInst.plType == 0)
    {
        int8_t plType = DefaultPayloadType(videoCodecInst.plName);
        if (plType == -1)
        {
            return -1;
        }
        videoCodecInst.plType = plType;
    }

    if(_vcm->RegisterReceiveCodec(&videoCodecInst, numberOfCores) != VCM_OK)
    {
        return -1;
    }
    return 0;
}

int32_t VideoCoder::Decode(I420VideoFrame& decodedVideo,
                           const EncodedVideoData& encodedData)
{
    decodedVideo.ResetSize();
    if(encodedData.payloadSize <= 0)
    {
        return -1;
    }

    _decodedVideo = &decodedVideo;
    return 0;
}


int32_t VideoCoder::Encode(const I420VideoFrame& videoFrame,
                           EncodedVideoData& videoEncodedData)
{
    // The AddVideoFrame(..) call will (indirectly) call SendData(). Store a
    // pointer to videoFrame so that it can be updated.
    _videoEncodedData = &videoEncodedData;
    videoEncodedData.payloadSize = 0;
    if(_vcm->AddVideoFrame(videoFrame) != VCM_OK)
    {
        return -1;
    }
    return 0;
}

int8_t VideoCoder::DefaultPayloadType(const char* plName)
{
    VideoCodec tmpCodec;
    int32_t numberOfCodecs = _vcm->NumberOfCodecs();
    for (uint8_t i = 0; i < numberOfCodecs; i++)
    {
        _vcm->Codec(i, &tmpCodec);
        if(strncmp(tmpCodec.plName, plName, kPayloadNameSize) == 0)
        {
            return tmpCodec.plType;
        }
    }
    return -1;
}

int32_t VideoCoder::FrameToRender(I420VideoFrame& videoFrame)
{
    return _decodedVideo->CopyFrame(videoFrame);
}

int32_t VideoCoder::SendData(
    const FrameType frameType,
    const uint8_t  payloadType,
    const uint32_t timeStamp,
    int64_t capture_time_ms,
    const uint8_t* payloadData,
    uint32_t payloadSize,
    const RTPFragmentationHeader& fragmentationHeader,
    const RTPVideoHeader* /*rtpVideoHdr*/)
{
    // Store the data in _videoEncodedData which is a pointer to videoFrame in
    // Encode(..)
    _videoEncodedData->VerifyAndAllocate(payloadSize);
    _videoEncodedData->frameType = frameType;
    _videoEncodedData->payloadType = payloadType;
    _videoEncodedData->timeStamp = timeStamp;
    _videoEncodedData->fragmentationHeader.CopyFrom(fragmentationHeader);
    memcpy(_videoEncodedData->payloadData, payloadData,
           sizeof(uint8_t) * payloadSize);
    _videoEncodedData->payloadSize = payloadSize;
    return 0;
}
}  // namespace webrtc
#endif // WEBRTC_MODULE_UTILITY_VIDEO
