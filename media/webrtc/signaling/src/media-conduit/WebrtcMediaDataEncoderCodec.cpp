/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebrtcMediaDataEncoderCodec.h"
#include "PEMFactory.h"
#include "VideoUtils.h"
#include "mozilla/media/MediaUtils.h"

namespace mozilla {

WebrtcMediaDataEncoder::WebrtcMediaDataEncoder()
    : mThreadPool(GetMediaThreadPool(MediaThreadType::PLATFORM_ENCODER)),
      mTaskQueue(new TaskQueue(do_AddRef(mThreadPool),
                               "WebrtcMediaDataEncoder::mTaskQueue")),
      mFactory(new PEMFactory()) {}

int32_t WebrtcMediaDataEncoder::InitEncode(
    const webrtc::VideoCodec* aCodecSettings, int32_t aNumberOfCores,
    size_t aMaxPayloadSize) {
  return NS_SUCCEEDED(mError) ? WEBRTC_VIDEO_CODEC_OK
                              : WEBRTC_VIDEO_CODEC_ERROR;
}

int32_t WebrtcMediaDataEncoder::RegisterEncodeCompleteCallback(
    webrtc::EncodedImageCallback* aCallback) {
  mCallback = aCallback;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t WebrtcMediaDataEncoder::Release() {
  mError = NS_OK;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t WebrtcMediaDataEncoder::Encode(
    const webrtc::VideoFrame& aInputFrame,
    const webrtc::CodecSpecificInfo* aCodecSpecificInfo,
    const std::vector<webrtc::FrameType>* aFrameTypes) {
  return NS_SUCCEEDED(mError) ? WEBRTC_VIDEO_CODEC_OK
                              : WEBRTC_VIDEO_CODEC_ERROR;
}

int32_t WebrtcMediaDataEncoder::SetChannelParameters(uint32_t aPacketLoss,
                                                     int64_t aRtt) {
  return WEBRTC_VIDEO_CODEC_OK;
}

}  // namespace mozilla
