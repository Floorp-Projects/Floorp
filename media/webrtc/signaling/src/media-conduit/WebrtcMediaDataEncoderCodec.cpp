/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebrtcMediaDataEncoderCodec.h"

#include "MediaData.h"
#include "PEMFactory.h"
#include "VideoUtils.h"
#include "mozilla/Result.h"
#include "mozilla/media/MediaUtils.h"

namespace mozilla {

extern LazyLogModule sPEMLog;

#undef LOG
#define LOG(msg, ...)               \
  MOZ_LOG(sPEMLog, LogLevel::Debug, \
          ("WebrtcMediaDataEncoder=%p, " msg, this, ##__VA_ARGS__))

using namespace media;
using MimeTypeResult = Result<nsCString, bool>;

static const char* GetModeName(webrtc::H264PacketizationMode aMode) {
  if (aMode == webrtc::H264PacketizationMode::SingleNalUnit) {
    return "SingleNalUnit";
  }
  if (aMode == webrtc::H264PacketizationMode::NonInterleaved) {
    return "NonInterleaved";
  }
  return "Unknown";
}

static MimeTypeResult ConvertWebrtcCodecTypeToMimeType(
    const webrtc::VideoCodecType& aType) {
  switch (aType) {
    case webrtc::VideoCodecType::kVideoCodecVP8:
      return nsCString("video/vp8");
    case webrtc::VideoCodecType::kVideoCodecVP9:
      return nsCString("video/vp9");
    case webrtc::VideoCodecType::kVideoCodecH264:
      return nsCString("video/avc");
    default:
      break;
  }
  return MimeTypeResult(false);
}

static MediaDataEncoder::H264Specific::ProfileLevel ConvertProfileLevel(
    webrtc::H264::Profile aProfile) {
  if (aProfile == webrtc::H264::kProfileConstrainedBaseline ||
      aProfile == webrtc::H264::kProfileBaseline) {
    return MediaDataEncoder::H264Specific::ProfileLevel::BaselineAutoLevel;
  }
  return MediaDataEncoder::H264Specific::ProfileLevel::MainAutoLevel;
}

static MediaDataEncoder::H264Specific GetCodecSpecific(
    const webrtc::VideoCodec* aCodecSettings) {
  return MediaDataEncoder::H264Specific(
      aCodecSettings->H264().keyFrameInterval,
      ConvertProfileLevel(aCodecSettings->H264().profile));
}

WebrtcMediaDataEncoder::WebrtcMediaDataEncoder()
    : mThreadPool(GetMediaThreadPool(MediaThreadType::PLATFORM_ENCODER)),
      mTaskQueue(new TaskQueue(do_AddRef(mThreadPool),
                               "WebrtcMediaDataEncoder::mTaskQueue")),
      mFactory(new PEMFactory()) {}

int32_t WebrtcMediaDataEncoder::InitEncode(
    const webrtc::VideoCodec* aCodecSettings, int32_t aNumberOfCores,
    size_t aMaxPayloadSize) {
  MOZ_ASSERT(
      aCodecSettings->codecType == webrtc::VideoCodecType::kVideoCodecH264,
      "Only support h264 for now.");
  if (!CreateEncoder(aCodecSettings)) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  LOG("Init decode, mimeType %s, mode %s", mInfo.mMimeType.get(),
      GetModeName(mMode));
  return InitEncoder() ? WEBRTC_VIDEO_CODEC_OK : WEBRTC_VIDEO_CODEC_ERROR;
}

bool WebrtcMediaDataEncoder::SetupConfig(
    const webrtc::VideoCodec* aCodecSettings) {
  MimeTypeResult mimeType =
      ConvertWebrtcCodecTypeToMimeType(aCodecSettings->codecType);
  if (mimeType.isErr()) {
    LOG("Get incorrect mime type");
    return false;
  }
  mInfo = VideoInfo(aCodecSettings->width, aCodecSettings->height);
  mInfo.mMimeType = mimeType.unwrap();
  mMode = aCodecSettings->H264().packetizationMode == 1
              ? webrtc::H264PacketizationMode::NonInterleaved
              : webrtc::H264PacketizationMode::SingleNalUnit;
  return true;
}

bool WebrtcMediaDataEncoder::CreateEncoder(
    const webrtc::VideoCodec* aCodecSettings) {
  if (!SetupConfig(aCodecSettings)) {
    return false;
  }
  if (mEncoder) {
    Release();
  }
  LOG("Request platform encoder for %s, bitRate=%u kpbs, frameRate=%u",
      mInfo.mMimeType.get(), aCodecSettings->startBitrate,
      aCodecSettings->maxFramerate);
  mEncoder = mFactory->CreateEncoder(CreateEncoderParams(
      mInfo, MediaDataEncoder::Usage::Realtime, mTaskQueue,
      MediaDataEncoder::PixelFormat::YUV420P, aCodecSettings->maxFramerate,
      aCodecSettings->startBitrate * 1000 /* kpbs to bps*/,
      GetCodecSpecific(aCodecSettings)));
  return !!mEncoder;
}

bool WebrtcMediaDataEncoder::InitEncoder() {
  LOG("Wait until encoder successfully initialize");
  media::Await(
      do_AddRef(mThreadPool), mEncoder->Init(),
      [&](TrackInfo::TrackType) { mError = NS_OK; },
      [&](const MediaResult& aError) { mError = aError; });
  return NS_SUCCEEDED(mError);
}

int32_t WebrtcMediaDataEncoder::RegisterEncodeCompleteCallback(
    webrtc::EncodedImageCallback* aCallback) {
  mCallback = aCallback;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t WebrtcMediaDataEncoder::Shutdown() {
  LOG("Release encoder");
  auto rv = media::Await(do_AddRef(mThreadPool), mEncoder->Shutdown());
  mEncoder = nullptr;
  mCallback = nullptr;
  mError = NS_OK;
  return rv.IsResolve() ? WEBRTC_VIDEO_CODEC_OK : WEBRTC_VIDEO_CODEC_ERROR;
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
