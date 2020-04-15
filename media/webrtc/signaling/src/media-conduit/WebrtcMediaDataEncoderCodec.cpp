/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebrtcMediaDataEncoderCodec.h"

#include "AnnexB.h"
#include "ImageContainer.h"
#include "MediaData.h"
#include "PEMFactory.h"
#include "VideoUtils.h"
#include "mozilla/Result.h"
#include "mozilla/Span.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/media/MediaUtils.h"
#include "webrtc/media/base/mediaconstants.h"
#include "webrtc/system_wrappers/include/clock.h"

namespace mozilla {

extern LazyLogModule sPEMLog;

#undef LOG
#define LOG(msg, ...)               \
  MOZ_LOG(sPEMLog, LogLevel::Debug, \
          ("WebrtcMediaDataEncoder=%p, " msg, this, ##__VA_ARGS__))

#undef LOG_V
#define LOG_V(msg, ...)               \
  MOZ_LOG(sPEMLog, LogLevel::Verbose, \
          ("WebrtcMediaDataEncoder=%p, " msg, this, ##__VA_ARGS__))

using namespace media;
using namespace layers;
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
    : mCallbackMutex("WebrtcMediaDataEncoderCodec encoded callback mutex"),
      mThreadPool(GetMediaThreadPool(MediaThreadType::PLATFORM_ENCODER)),
      mTaskQueue(new TaskQueue(do_AddRef(mThreadPool),
                               "WebrtcMediaDataEncoder::mTaskQueue")),
      mFactory(new PEMFactory()),
      // Use the same lower and upper bound as h264_video_toolbox_encoder which
      // is an encoder from webrtc's upstream codebase.
      // 0.5 is set as a mininum to prevent overcompensating for large temporary
      // overshoots. We don't want to degrade video quality too badly.
      // 0.95 is set to prevent oscillations. When a lower bitrate is set on the
      // encoder than previously set, its output seems to have a brief period of
      // drastically reduced bitrate, so we want to avoid that. In steady state
      // conditions, 0.95 seems to give us better overall bitrate over long
      // periods of time.
      mBitrateAdjuster(webrtc::Clock::GetRealTimeClock(), 0.5, 0.95) {}

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
  mMaxFrameRate = aCodecSettings->maxFramerate;
  // Those bitrates in codec setting are all kbps, so we have to covert them to
  // bps.
  mMaxBitrateBps = aCodecSettings->maxBitrate * 1000;
  mMinBitrateBps = aCodecSettings->minBitrate * 1000;
  mBitrateAdjuster.SetTargetBitrateBps(aCodecSettings->startBitrate * 1000);
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
  LOG("Request platform encoder for %s, bitRate=%u bps, frameRate=%u",
      mInfo.mMimeType.get(), mBitrateAdjuster.GetTargetBitrateBps(),
      aCodecSettings->maxFramerate);
  mEncoder = mFactory->CreateEncoder(CreateEncoderParams(
      mInfo, MediaDataEncoder::Usage::Realtime, mTaskQueue,
      MediaDataEncoder::PixelFormat::YUV420P, aCodecSettings->maxFramerate,
      mBitrateAdjuster.GetTargetBitrateBps(),
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
  MutexAutoLock lock(mCallbackMutex);
  mCallback = aCallback;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t WebrtcMediaDataEncoder::Shutdown() {
  LOG("Release encoder");
  {
    MutexAutoLock lock(mCallbackMutex);
    mCallback = nullptr;
  }
  mThreadPool->Dispatch(NS_NewRunnableFunction(
      "WebrtcMediaDataEncoder::Shutdown",
      [self = RefPtr<WebrtcMediaDataEncoder>(this),
       encoder = RefPtr<MediaDataEncoder>(std::move(mEncoder))]() {
        self->mError = NS_OK;
        encoder->Shutdown();
      }));
  return WEBRTC_VIDEO_CODEC_OK;
}

RefPtr<MediaData> WebrtcMediaDataEncoder::CreateVideoDataFromWebrtcVideoFrame(
    const webrtc::VideoFrame& aFrame, bool aIsKeyFrame) {
  MOZ_ASSERT(aFrame.video_frame_buffer()->type() ==
                 webrtc::VideoFrameBuffer::Type::kI420,
             "Only support YUV420!");
  rtc::scoped_refptr<webrtc::I420BufferInterface> i420 =
      aFrame.video_frame_buffer()->GetI420();

  PlanarYCbCrData yCbCrData;
  yCbCrData.mYChannel = const_cast<uint8_t*>(i420->DataY());
  yCbCrData.mYSize = gfx::IntSize(i420->width(), i420->height());
  yCbCrData.mYStride = i420->StrideY();
  yCbCrData.mCbChannel = const_cast<uint8_t*>(i420->DataU());
  yCbCrData.mCrChannel = const_cast<uint8_t*>(i420->DataV());
  yCbCrData.mCbCrSize = gfx::IntSize(i420->ChromaWidth(), i420->ChromaHeight());
  MOZ_ASSERT(i420->StrideU() == i420->StrideV());
  yCbCrData.mCbCrStride = i420->StrideU();
  yCbCrData.mPicSize = gfx::IntSize(i420->width(), i420->height());

  RefPtr<PlanarYCbCrImage> image =
      new RecyclingPlanarYCbCrImage(new BufferRecycleBin());
  image->CopyData(yCbCrData);

  RefPtr<MediaData> data = VideoData::CreateFromImage(
      image->GetSize(), 0, TimeUnit::FromMicroseconds(aFrame.timestamp_us()),
      TimeUnit::FromSeconds(1.0 / mMaxFrameRate), image, aIsKeyFrame,
      TimeUnit::FromMicroseconds(aFrame.timestamp()));
  return data;
}

int32_t WebrtcMediaDataEncoder::Encode(
    const webrtc::VideoFrame& aInputFrame,
    const webrtc::CodecSpecificInfo* aCodecSpecificInfo,
    const std::vector<webrtc::FrameType>* aFrameTypes) {
  if (!mCallback || !mEncoder) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }

  if (!aInputFrame.size() || !aInputFrame.video_frame_buffer() ||
      aFrameTypes->empty()) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }

  if (NS_FAILED(mError)) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  LOG_V("Encode frame, type %d size %u", (*aFrameTypes)[0], aInputFrame.size());
  MOZ_ASSERT(aInputFrame.video_frame_buffer()->type() ==
             webrtc::VideoFrameBuffer::Type::kI420);
  RefPtr<MediaData> data = CreateVideoDataFromWebrtcVideoFrame(
      aInputFrame, (*aFrameTypes)[0] == webrtc::FrameType::kVideoFrameKey);
  OwnerThread()->Dispatch(NS_NewRunnableFunction(
      "WebrtcMediaDataEncoder::Encode",
      [self = RefPtr<WebrtcMediaDataEncoder>(this), data]() {
        if (!self->mEncoder) {
          return;
        }
        self->ProcessEncode(data);
      }));
  return WEBRTC_VIDEO_CODEC_OK;
}

void WebrtcMediaDataEncoder::ProcessEncode(
    const RefPtr<MediaData>& aInputData) {
  const gfx::IntSize display = aInputData->As<VideoData>()->mDisplay;
  mEncoder->Encode(aInputData)
      ->Then(
          OwnerThread(), __func__,
          [display, self = RefPtr<WebrtcMediaDataEncoder>(this),
           // capture this for printing address in LOG.
           this](const MediaDataEncoder::EncodedData& aData) {
            MutexAutoLock lock(mCallbackMutex);
            // Callback has been unregistered.
            if (!mCallback) {
              return;
            }
            // The encoder haven't finished encoding yet.
            if (aData.IsEmpty()) {
              return;
            }

            LOG_V("Received encoded frame, nums %zu width %d height %d",
                  aData.Length(), display.width, display.height);
            for (auto& frame : aData) {
              webrtc::EncodedImage image(const_cast<uint8_t*>(frame->Data()),
                                         frame->Size(), frame->Size());
              image._encodedWidth = display.width;
              image._encodedHeight = display.height;
              CheckedInt64 time =
                  TimeUnitToFrames(frame->mTime, cricket::kVideoCodecClockrate);
              if (!time.isValid()) {
                mError = MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                                     "invalid timestamp from encoder");
                return;
              }
              image._timeStamp = time.value();
              image._frameType = frame->mKeyframe
                                     ? webrtc::FrameType::kVideoFrameKey
                                     : webrtc::FrameType::kVideoFrameDelta;
              image._completeFrame = true;

              nsTArray<AnnexB::NALEntry> entries;
              AnnexB::ParseNALEntries(
                  MakeSpan<const uint8_t>(frame->Data(), frame->Size()),
                  entries);
              const size_t nalNums = entries.Length();
              LOG_V("NAL nums %zu", nalNums);
              MOZ_ASSERT(nalNums,
                         "Should have at least 1 NALU in encoded frame!");

              webrtc::RTPFragmentationHeader header;
              header.VerifyAndAllocateFragmentationHeader(nalNums);
              for (size_t idx = 0; idx < nalNums; idx++) {
                header.fragmentationOffset[idx] = entries[idx].mOffset;
                header.fragmentationLength[idx] = entries[idx].mSize;
                LOG_V("NAL offset %" PRId64 " size %" PRId64,
                      entries[idx].mOffset, entries[idx].mSize);
              }

              webrtc::CodecSpecificInfo codecSpecific;
              codecSpecific.codecType = webrtc::kVideoCodecH264;
              codecSpecific.codecSpecific.H264.packetization_mode = mMode;

              LOG_V("Send encoded image");
              mCallback->OnEncodedImage(image, &codecSpecific, &header);
              mBitrateAdjuster.Update(image._size);
            }
          },
          [self = RefPtr<WebrtcMediaDataEncoder>(this)](
              const MediaResult& aError) { self->mError = aError; });
}

int32_t WebrtcMediaDataEncoder::SetChannelParameters(uint32_t aPacketLoss,
                                                     int64_t aRtt) {
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t WebrtcMediaDataEncoder::SetRates(uint32_t aNewBitrateKbps,
                                         uint32_t aFrameRate) {
  if (!mEncoder) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }

  if (!aFrameRate) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }

  if (NS_FAILED(mError)) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  const uint32_t newBitrateBps = aNewBitrateKbps * 1000;
  if (newBitrateBps < mMinBitrateBps || newBitrateBps > mMaxBitrateBps) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }

  // We have already been in this bitrate.
  if (mBitrateAdjuster.GetAdjustedBitrateBps() == newBitrateBps) {
    return WEBRTC_VIDEO_CODEC_OK;
  }
  mBitrateAdjuster.SetTargetBitrateBps(newBitrateBps);
  LOG("Set bitrate %u bps, minBitrate %u bps, maxBitrate %u bps", newBitrateBps,
      mMinBitrateBps, mMaxBitrateBps);
  auto rv =
      media::Await(do_AddRef(mThreadPool), mEncoder->SetBitrate(newBitrateBps));
  return rv.IsResolve() ? WEBRTC_VIDEO_CODEC_OK : WEBRTC_VIDEO_CODEC_ERROR;
}

}  // namespace mozilla
