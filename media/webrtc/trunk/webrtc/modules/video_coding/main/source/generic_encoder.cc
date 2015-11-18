/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/checks.h"
#include "webrtc/engine_configurations.h"
#include "webrtc/modules/video_coding/main/source/encoded_frame.h"
#include "webrtc/modules/video_coding/main/source/generic_encoder.h"
#include "webrtc/modules/video_coding/main/source/media_optimization.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/logging.h"

namespace webrtc {
namespace {
// Map information from info into rtp. If no relevant information is found
// in info, rtp is set to NULL.
void CopyCodecSpecific(const CodecSpecificInfo* info, RTPVideoHeader* rtp) {
  DCHECK(info);
  switch (info->codecType) {
    case kVideoCodecVP8: {
      rtp->codec = kRtpVideoVp8;
      rtp->codecHeader.VP8.InitRTPVideoHeaderVP8();
      rtp->codecHeader.VP8.pictureId = info->codecSpecific.VP8.pictureId;
      rtp->codecHeader.VP8.nonReference =
          info->codecSpecific.VP8.nonReference;
      rtp->codecHeader.VP8.temporalIdx = info->codecSpecific.VP8.temporalIdx;
      rtp->codecHeader.VP8.layerSync = info->codecSpecific.VP8.layerSync;
      rtp->codecHeader.VP8.tl0PicIdx = info->codecSpecific.VP8.tl0PicIdx;
      rtp->codecHeader.VP8.keyIdx = info->codecSpecific.VP8.keyIdx;
      rtp->simulcastIdx = info->codecSpecific.VP8.simulcastIdx;
      return;
    }
    case kVideoCodecH264:
      rtp->codec = kRtpVideoH264;
      return;
    case kVideoCodecGeneric:
      rtp->codec = kRtpVideoGeneric;
      rtp->simulcastIdx = info->codecSpecific.generic.simulcast_idx;
      return;
    default:
      return;
  }
}
}  // namespace

//#define DEBUG_ENCODER_BIT_STREAM

VCMGenericEncoder::VCMGenericEncoder(VideoEncoder* encoder,
                                     VideoEncoderRateObserver* rate_observer,
                                     bool internalSource)
    : encoder_(encoder),
      rate_observer_(rate_observer),
      vcm_encoded_frame_callback_(nullptr),
      bit_rate_(0),
      frame_rate_(0),
      internal_source_(internalSource),
      rotation_(kVideoRotation_0) {
}

VCMGenericEncoder::~VCMGenericEncoder()
{
}

int32_t VCMGenericEncoder::Release()
{
    {
      rtc::CritScope lock(&rates_lock_);
      bit_rate_ = 0;
      frame_rate_ = 0;
      vcm_encoded_frame_callback_ = nullptr;
    }

    return encoder_->Release();
}

int32_t
VCMGenericEncoder::InitEncode(const VideoCodec* settings,
                              int32_t numberOfCores,
                              size_t maxPayloadSize)
{
    {
      rtc::CritScope lock(&rates_lock_);
      bit_rate_ = settings->startBitrate * 1000;
      frame_rate_ = settings->maxFramerate;
    }

    if (encoder_->InitEncode(settings, numberOfCores, maxPayloadSize) != 0) {
      LOG(LS_ERROR) << "Failed to initialize the encoder associated with "
                       "payload name: " << settings->plName;
      return -1;
    }
    return 0;
}

int32_t
VCMGenericEncoder::Encode(const I420VideoFrame& inputFrame,
                          const CodecSpecificInfo* codecSpecificInfo,
                          const std::vector<FrameType>& frameTypes) {
  std::vector<VideoFrameType> video_frame_types(frameTypes.size(),
                                                kDeltaFrame);
  VCMEncodedFrame::ConvertFrameTypes(frameTypes, &video_frame_types);

  rotation_ = inputFrame.rotation();

  if (vcm_encoded_frame_callback_) {
    // Keep track of the current frame rotation and apply to the output of the
    // encoder. There might not be exact as the encoder could have one frame
    // delay but it should be close enough.
    vcm_encoded_frame_callback_->SetRotation(rotation_);
  }

  return encoder_->Encode(inputFrame, codecSpecificInfo, &video_frame_types);
}

int32_t
VCMGenericEncoder::SetChannelParameters(int32_t packetLoss, int64_t rtt)
{
    return encoder_->SetChannelParameters(packetLoss, rtt);
}

int32_t
VCMGenericEncoder::SetRates(uint32_t newBitRate, uint32_t frameRate)
{
    uint32_t target_bitrate_kbps = (newBitRate + 500) / 1000;
    int32_t ret = encoder_->SetRates(target_bitrate_kbps, frameRate);
    if (ret < 0)
    {
        return ret;
    }

    {
      rtc::CritScope lock(&rates_lock_);
      bit_rate_ = newBitRate;
      frame_rate_ = frameRate;
    }

    if (rate_observer_ != nullptr)
      rate_observer_->OnSetRates(newBitRate, frameRate);
    return VCM_OK;
}

int32_t
VCMGenericEncoder::CodecConfigParameters(uint8_t* buffer, int32_t size)
{
    int32_t ret = encoder_->CodecConfigParameters(buffer, size);
    if (ret < 0)
    {
        return ret;
    }
    return ret;
}

uint32_t VCMGenericEncoder::BitRate() const
{
    rtc::CritScope lock(&rates_lock_);
    return bit_rate_;
}

uint32_t VCMGenericEncoder::FrameRate() const
{
    rtc::CritScope lock(&rates_lock_);
    return frame_rate_;
}

int32_t
VCMGenericEncoder::SetPeriodicKeyFrames(bool enable)
{
    return encoder_->SetPeriodicKeyFrames(enable);
}

int32_t VCMGenericEncoder::RequestFrame(
    const std::vector<FrameType>& frame_types) {
  I420VideoFrame image;
  std::vector<VideoFrameType> video_frame_types(frame_types.size(),
                                                kDeltaFrame);
  VCMEncodedFrame::ConvertFrameTypes(frame_types, &video_frame_types);
  return encoder_->Encode(image, NULL, &video_frame_types);
}

int32_t
VCMGenericEncoder::RegisterEncodeCallback(VCMEncodedFrameCallback* VCMencodedFrameCallback)
{
    VCMencodedFrameCallback->SetInternalSource(internal_source_);
    vcm_encoded_frame_callback_ = VCMencodedFrameCallback;
    return encoder_->RegisterEncodeCompleteCallback(VCMencodedFrameCallback);
}

bool
VCMGenericEncoder::InternalSource() const
{
    return internal_source_;
}

 /***************************
  * Callback Implementation
  ***************************/
VCMEncodedFrameCallback::VCMEncodedFrameCallback(
    EncodedImageCallback* post_encode_callback)
    : _sendCallback(),
      _mediaOpt(NULL),
      _payloadType(0),
      _internalSource(false),
      _rotation(kVideoRotation_0),
      post_encode_callback_(post_encode_callback)
#ifdef DEBUG_ENCODER_BIT_STREAM
      ,
      _bitStreamAfterEncoder(NULL)
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

int32_t VCMEncodedFrameCallback::Encoded(
    const EncodedImage& encodedImage,
    const CodecSpecificInfo* codecSpecificInfo,
    const RTPFragmentationHeader* fragmentationHeader) {
  post_encode_callback_->Encoded(encodedImage, NULL, NULL);

  if (_sendCallback == NULL) {
    return VCM_UNINITIALIZED;
  }

#ifdef DEBUG_ENCODER_BIT_STREAM
  if (_bitStreamAfterEncoder != NULL) {
    fwrite(encodedImage._buffer, 1, encodedImage._length,
           _bitStreamAfterEncoder);
  }
#endif

  RTPVideoHeader rtpVideoHeader;
  memset(&rtpVideoHeader, 0, sizeof(RTPVideoHeader));
  RTPVideoHeader* rtpVideoHeaderPtr = &rtpVideoHeader;
  if (codecSpecificInfo) {
    CopyCodecSpecific(codecSpecificInfo, rtpVideoHeaderPtr);
  }
  rtpVideoHeader.rotation = _rotation;

  int32_t callbackReturn = _sendCallback->SendData(
      _payloadType, encodedImage, *fragmentationHeader, rtpVideoHeaderPtr);
  if (callbackReturn < 0) {
    return callbackReturn;
  }

  if (_mediaOpt != NULL) {
    _mediaOpt->UpdateWithEncodedData(encodedImage);
    if (_internalSource)
      return _mediaOpt->DropFrame();  // Signal to encoder to drop next frame.
  }
  return VCM_OK;
}

void
VCMEncodedFrameCallback::SetMediaOpt(
    media_optimization::MediaOptimization *mediaOpt)
{
    _mediaOpt = mediaOpt;
}

}  // namespace webrtc
