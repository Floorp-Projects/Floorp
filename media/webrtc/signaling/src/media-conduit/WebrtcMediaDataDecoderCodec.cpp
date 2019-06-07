/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebrtcMediaDataDecoderCodec.h"
#include "ImageContainer.h"
#include "Layers.h"
#include "PDMFactory.h"
#include "VideoUtils.h"
#include "mozilla/layers/ImageBridgeChild.h"
#include "mozilla/media/MediaUtils.h"
#include "webrtc/rtc_base/keep_ref_until_done.h"

namespace mozilla {

WebrtcMediaDataDecoder::WebrtcMediaDataDecoder()
    : mThreadPool(GetMediaThreadPool(MediaThreadType::PLATFORM_DECODER)),
      mTaskQueue(new TaskQueue(do_AddRef(mThreadPool),
                               "WebrtcMediaDataDecoder::mTaskQueue")),
      mImageContainer(layers::LayerManager::CreateImageContainer(
          layers::ImageContainer::ASYNCHRONOUS)),
      mFactory(new PDMFactory()),
      mTrackType(TrackInfo::kUndefinedTrack) {}

WebrtcMediaDataDecoder::~WebrtcMediaDataDecoder() {}

int32_t WebrtcMediaDataDecoder::InitDecode(
    const webrtc::VideoCodec* aCodecSettings, int32_t aNumberOfCores) {
  nsCString codec;
  switch (aCodecSettings->codecType) {
    case webrtc::VideoCodecType::kVideoCodecVP8:
      codec = "video/vp8";
      break;
    case webrtc::VideoCodecType::kVideoCodecVP9:
      codec = "video/vp9";
      break;
    case webrtc::VideoCodecType::kVideoCodecH264:
      codec = "video/avc";
      break;
    default:
      return WEBRTC_VIDEO_CODEC_ERROR;
  }

  mTrackType = TrackInfo::kVideoTrack;

  mInfo = VideoInfo(aCodecSettings->width, aCodecSettings->height);
  mInfo.mMimeType = codec;

  RefPtr<layers::KnowsCompositor> knowsCompositor =
      layers::ImageBridgeChild::GetSingleton();

  if (mDecoder) {
    Release();
  }

  mDecoder = mFactory->CreateDecoder(
      {mInfo, mTaskQueue,
       CreateDecoderParams::OptionSet(
           CreateDecoderParams::Option::LowLatency,
           CreateDecoderParams::Option::FullH264Parsing,
           CreateDecoderParams::Option::ErrorIfNoInitializationData),
       mTrackType, mImageContainer, knowsCompositor});

  if (!mDecoder) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  media::Await(
      do_AddRef(mThreadPool), mDecoder->Init(),
      [&](TrackInfo::TrackType) { mError = NS_OK; },
      [&](const MediaResult& aError) { mError = aError; });

  return NS_SUCCEEDED(mError) ? WEBRTC_VIDEO_CODEC_OK
                              : WEBRTC_VIDEO_CODEC_ERROR;
}

int32_t WebrtcMediaDataDecoder::Decode(
    const webrtc::EncodedImage& aInputImage, bool aMissingFrames,
    const webrtc::RTPFragmentationHeader* aFragmentation,
    const webrtc::CodecSpecificInfo* aCodecSpecificInfo,
    int64_t aRenderTimeMs) {
  if (!mCallback || !mDecoder) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }

  if (!aInputImage._buffer || !aInputImage._length) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }

  // Always start with a complete key frame.
  if (mNeedKeyframe) {
    if (aInputImage._frameType != webrtc::FrameType::kVideoFrameKey)
      return WEBRTC_VIDEO_CODEC_ERROR;
    // We have a key frame - is it complete?
    if (aInputImage._completeFrame) {
      mNeedKeyframe = false;
    } else {
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
  }

  RefPtr<MediaRawData> compressedFrame =
      new MediaRawData(aInputImage._buffer, aInputImage._length);
  if (!compressedFrame->Data()) {
    return WEBRTC_VIDEO_CODEC_MEMORY;
  }

  compressedFrame->mTime =
      media::TimeUnit::FromMicroseconds(aInputImage._timeStamp);
  compressedFrame->mTimecode =
      media::TimeUnit::FromMicroseconds(aRenderTimeMs * 1000);
  compressedFrame->mKeyframe =
      aInputImage._frameType == webrtc::FrameType::kVideoFrameKey;
  {
    media::Await(
        do_AddRef(mThreadPool), mDecoder->Decode(compressedFrame),
        [&](const MediaDataDecoder::DecodedData& aResults) {
          mResults = aResults;
          mError = NS_OK;
        },
        [&](const MediaResult& aError) { mError = aError; });

    for (auto& frame : mResults) {
      MOZ_ASSERT(frame->mType == MediaData::Type::VIDEO_DATA);
      RefPtr<VideoData> video = frame->As<VideoData>();
      MOZ_ASSERT(video);
      if (!video->mImage) {
        // Nothing to display.
        continue;
      }
      rtc::scoped_refptr<ImageBuffer> image(
          new rtc::RefCountedObject<ImageBuffer>(std::move(video->mImage)));

      webrtc::VideoFrame videoFrame(image, frame->mTime.ToMicroseconds(),
                                    frame->mDuration.ToMicroseconds() * 1000,
                                    aInputImage.rotation_);
      mCallback->Decoded(videoFrame);
    }
    mResults.Clear();
  }
  return NS_SUCCEEDED(mError) ? WEBRTC_VIDEO_CODEC_OK
                              : WEBRTC_VIDEO_CODEC_ERROR;
}

int32_t WebrtcMediaDataDecoder::RegisterDecodeCompleteCallback(
    webrtc::DecodedImageCallback* aCallback) {
  mCallback = aCallback;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t WebrtcMediaDataDecoder::Release() {
  RefPtr<MediaDataDecoder> decoder = mDecoder.forget();
  decoder->Flush()->Then(mTaskQueue, __func__,
                         [decoder]() { decoder->Shutdown(); });

  mNeedKeyframe = true;
  mError = NS_OK;

  return WEBRTC_VIDEO_CODEC_OK;
}

bool WebrtcMediaDataDecoder::OnTaskQueue() const {
  return OwnerThread()->IsCurrentThreadIn();
}

}  // namespace mozilla
