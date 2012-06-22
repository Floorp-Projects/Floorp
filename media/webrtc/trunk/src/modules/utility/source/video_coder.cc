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

#include "video_coder.h"

namespace webrtc {
VideoCoder::VideoCoder(WebRtc_UWord32 instanceID)
    : _instanceID( instanceID),
      _vcm(VideoCodingModule::Create(instanceID)),
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

WebRtc_Word32 VideoCoder::ResetDecoder()
{
    _vcm->ResetDecoder();

    _vcm->InitializeSender();
    _vcm->InitializeReceiver();

    _vcm->RegisterTransportCallback(this);
    _vcm->RegisterReceiveCallback(this);
    return 0;
}

WebRtc_Word32 VideoCoder::SetEncodeCodec(VideoCodec& videoCodecInst,
                                         WebRtc_UWord32 numberOfCores,
                                         WebRtc_UWord32 maxPayloadSize)
{
    if(_vcm->RegisterSendCodec(&videoCodecInst, numberOfCores,
                               maxPayloadSize) != VCM_OK)
    {
        return -1;
    }
    return 0;
}


WebRtc_Word32 VideoCoder::SetDecodeCodec(VideoCodec& videoCodecInst,
                                         WebRtc_Word32 numberOfCores)
{
    if (videoCodecInst.plType == 0)
    {
        WebRtc_Word8 plType = DefaultPayloadType(videoCodecInst.plName);
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

WebRtc_Word32 VideoCoder::Decode(VideoFrame& decodedVideo,
                                 const EncodedVideoData& encodedData)
{
    decodedVideo.SetLength(0);
    if(encodedData.payloadSize <= 0)
    {
        return -1;
    }

    _decodedVideo = &decodedVideo;
    if(_vcm->DecodeFromStorage(encodedData) != VCM_OK)
    {
        return -1;
    }
    return 0;
}


WebRtc_Word32 VideoCoder::Encode(const VideoFrame& videoFrame,
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

WebRtc_Word8 VideoCoder::DefaultPayloadType(const char* plName)
{
    VideoCodec tmpCodec;
    WebRtc_Word32 numberOfCodecs = _vcm->NumberOfCodecs();
    for (WebRtc_UWord8 i = 0; i < numberOfCodecs; i++)
    {
        _vcm->Codec(i, &tmpCodec);
        if(strncmp(tmpCodec.plName, plName, kPayloadNameSize) == 0)
        {
            return tmpCodec.plType;
        }
    }
    return -1;
}

WebRtc_Word32 VideoCoder::FrameToRender(VideoFrame& videoFrame)
{
    return _decodedVideo->CopyFrame(videoFrame);
}

WebRtc_Word32 VideoCoder::SendData(
    FrameType frameType,
    WebRtc_UWord8  payloadType,
    WebRtc_UWord32 timeStamp,
    const WebRtc_UWord8* payloadData,
    WebRtc_UWord32 payloadSize,
    const RTPFragmentationHeader& fragmentationHeader,
    const RTPVideoHeader* /*rtpVideoHdr*/)
{
    // Store the data in _videoEncodedData which is a pointer to videoFrame in
    // Encode(..)
    _videoEncodedData->VerifyAndAllocate(payloadSize);
    _videoEncodedData->frameType = frameType;
    _videoEncodedData->payloadType = payloadType;
    _videoEncodedData->timeStamp = timeStamp;
    _videoEncodedData->fragmentationHeader = fragmentationHeader;
    memcpy(_videoEncodedData->payloadData, payloadData,
           sizeof(WebRtc_UWord8) * payloadSize);
    _videoEncodedData->payloadSize = payloadSize;
    return 0;
}
} // namespace webrtc
#endif // WEBRTC_MODULE_UTILITY_VIDEO
