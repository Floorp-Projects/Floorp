/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebrtcGmpVideoCodec.h"

#include <iostream>
#include <vector>

#include "mozilla/CheckedInt.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/Move.h"
#include "mozilla/SizePrintfMacros.h"
#include "mozilla/SyncRunnable.h"
#include "VideoConduit.h"
#include "AudioConduit.h"
#include "runnable_utils.h"

#include "mozIGeckoMediaPluginService.h"
#include "nsServiceManagerUtils.h"
#include "GMPVideoDecoderProxy.h"
#include "GMPVideoEncoderProxy.h"
#include "MainThreadUtils.h"

#include "gmp-video-host.h"
#include "gmp-video-frame-i420.h"
#include "gmp-video-frame-encoded.h"
#include "webrtc/common_video/include/video_frame_buffer.h"
#include "webrtc/base/bind.h"

namespace mozilla {

#ifdef LOG
#undef LOG
#endif

extern mozilla::LogModule* GetGMPLog();

#define LOGD(msg) MOZ_LOG(GetGMPLog(), mozilla::LogLevel::Debug, msg)
#define LOG(level, msg) MOZ_LOG(GetGMPLog(), (level), msg)

WebrtcGmpPCHandleSetter::WebrtcGmpPCHandleSetter(const std::string& aPCHandle)
{
  if (!NS_IsMainThread()) {
    MOZ_ASSERT(false, "WebrtcGmpPCHandleSetter can only be used on main");
    return;
  }
  MOZ_ASSERT(sCurrentHandle.empty());
  sCurrentHandle = aPCHandle;
}

WebrtcGmpPCHandleSetter::~WebrtcGmpPCHandleSetter()
{
  if (!NS_IsMainThread()) {
    MOZ_ASSERT(false, "WebrtcGmpPCHandleSetter can only be used on main");
    return;
  }

  sCurrentHandle.clear();
}

/* static */ std::string
WebrtcGmpPCHandleSetter::GetCurrentHandle()
{
  if (!NS_IsMainThread()) {
    MOZ_ASSERT(false, "WebrtcGmpPCHandleSetter can only be used on main");
    return "";
  }

  return sCurrentHandle;
}

std::string WebrtcGmpPCHandleSetter::sCurrentHandle = "";

// Encoder.
WebrtcGmpVideoEncoder::WebrtcGmpVideoEncoder()
  : mGMP(nullptr)
  , mInitting(false)
  , mHost(nullptr)
  , mMaxPayloadSize(0)
  , mCallbackMutex("WebrtcGmpVideoEncoder encoded callback mutex")
  , mCallback(nullptr)
  , mCachedPluginId(0)
{
  if (mPCHandle.empty()) {
    mPCHandle = WebrtcGmpPCHandleSetter::GetCurrentHandle();
  }
  MOZ_ASSERT(!mPCHandle.empty());
}

WebrtcGmpVideoEncoder::~WebrtcGmpVideoEncoder()
{
  // We should not have been destroyed if we never closed our GMP
  MOZ_ASSERT(!mGMP);
}

static int
WebrtcFrameTypeToGmpFrameType(webrtc::FrameType aIn,
                              GMPVideoFrameType *aOut)
{
  MOZ_ASSERT(aOut);
  switch(aIn) {
    case webrtc::kVideoFrameKey:
      *aOut = kGMPKeyFrame;
      break;
    case webrtc::kVideoFrameDelta:
      *aOut = kGMPDeltaFrame;
      break;
    case webrtc::kEmptyFrame:
      *aOut = kGMPSkipFrame;
      break;
    default:
      MOZ_CRASH("Unexpected webrtc::FrameType");
  }

  return WEBRTC_VIDEO_CODEC_OK;
}

static int
GmpFrameTypeToWebrtcFrameType(GMPVideoFrameType aIn,
                              webrtc::FrameType *aOut)
{
  MOZ_ASSERT(aOut);
  switch(aIn) {
    case kGMPKeyFrame:
      *aOut = webrtc::kVideoFrameKey;
      break;
    case kGMPDeltaFrame:
      *aOut = webrtc::kVideoFrameDelta;
      break;
    case kGMPSkipFrame:
      *aOut = webrtc::kEmptyFrame;
      break;
    default:
      MOZ_CRASH("Unexpected GMPVideoFrameType");
  }

  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t
WebrtcGmpVideoEncoder::InitEncode(const webrtc::VideoCodec* aCodecSettings,
                                  int32_t aNumberOfCores,
                                  uint32_t aMaxPayloadSize)
{
  if (!mMPS) {
    mMPS = do_GetService("@mozilla.org/gecko-media-plugin-service;1");
  }
  MOZ_ASSERT(mMPS);

  if (!mGMPThread) {
    if (NS_WARN_IF(NS_FAILED(mMPS->GetThread(getter_AddRefs(mGMPThread))))) {
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
  }

  // Bug XXXXXX: transfer settings from codecSettings to codec.
  GMPVideoCodec codecParams;
  memset(&codecParams, 0, sizeof(codecParams));

  codecParams.mGMPApiVersion = 33;
  codecParams.mStartBitrate = aCodecSettings->startBitrate;
  codecParams.mMinBitrate = aCodecSettings->minBitrate;
  codecParams.mMaxBitrate = aCodecSettings->maxBitrate;
  codecParams.mMaxFramerate = aCodecSettings->maxFramerate;
  mMaxPayloadSize = aMaxPayloadSize;

  memset(&mCodecSpecificInfo, 0, sizeof(webrtc::CodecSpecificInfo));
  mCodecSpecificInfo.codecType = webrtc::kVideoCodecH264;
  mCodecSpecificInfo.codecSpecific.H264.packetization_mode =
    aCodecSettings->H264().packetizationMode == 1 ?
    webrtc::H264PacketizationMode::NonInterleaved :
    webrtc::H264PacketizationMode::SingleNalUnit;

  if (mCodecSpecificInfo.codecSpecific.H264.packetization_mode ==
      webrtc::H264PacketizationMode::NonInterleaved) {
    mMaxPayloadSize = 0; // No limit, use FUAs
  }

  if (aCodecSettings->mode == webrtc::kScreensharing) {
    codecParams.mMode = kGMPScreensharing;
  } else {
    codecParams.mMode = kGMPRealtimeVideo;
  }

  codecParams.mWidth = aCodecSettings->width;
  codecParams.mHeight = aCodecSettings->height;

  RefPtr<GmpInitDoneRunnable> initDone(new GmpInitDoneRunnable(mPCHandle));
  mGMPThread->Dispatch(WrapRunnableNM(WebrtcGmpVideoEncoder::InitEncode_g,
                                      RefPtr<WebrtcGmpVideoEncoder>(this),
                                      codecParams,
                                      aNumberOfCores,
                                      aMaxPayloadSize,
                                      initDone),
                       NS_DISPATCH_NORMAL);

  // Since init of the GMP encoder is a multi-step async dispatch (including
  // dispatches to main), and since this function is invoked on main, there's
  // no safe way to block until this init is done. If an error occurs, we'll
  // handle it later.
  return WEBRTC_VIDEO_CODEC_OK;
}

/* static */
void
WebrtcGmpVideoEncoder::InitEncode_g(
    const RefPtr<WebrtcGmpVideoEncoder>& aThis,
    const GMPVideoCodec& aCodecParams,
    int32_t aNumberOfCores,
    uint32_t aMaxPayloadSize,
    const RefPtr<GmpInitDoneRunnable>& aInitDone)
{
  nsTArray<nsCString> tags;
  tags.AppendElement(NS_LITERAL_CSTRING("h264"));
  UniquePtr<GetGMPVideoEncoderCallback> callback(
    new InitDoneCallback(aThis, aInitDone, aCodecParams, aMaxPayloadSize));
  aThis->mInitting = true;
  nsresult rv = aThis->mMPS->GetGMPVideoEncoder(nullptr,
                                                &tags,
                                                NS_LITERAL_CSTRING(""),
                                                Move(callback));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    LOGD(("GMP Encode: GetGMPVideoEncoder failed"));
    aThis->Close_g();
    aInitDone->Dispatch(WEBRTC_VIDEO_CODEC_ERROR,
                        "GMP Encode: GetGMPVideoEncoder failed");
  }
}

int32_t
WebrtcGmpVideoEncoder::GmpInitDone(GMPVideoEncoderProxy* aGMP,
                                   GMPVideoHost* aHost,
                                   std::string* aErrorOut)
{
  if (!mInitting || !aGMP || !aHost) {
    *aErrorOut = "GMP Encode: Either init was aborted, "
                 "or init failed to supply either a GMP Encoder or GMP host.";
    if (aGMP) {
      // This could destroy us, since aGMP may be the last thing holding a ref
      // Return immediately.
      aGMP->Close();
    }
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  mInitting = false;

  if (mGMP && mGMP != aGMP) {
    Close_g();
  }

  mGMP = aGMP;
  mHost = aHost;
  mCachedPluginId = mGMP->GetPluginId();
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t
WebrtcGmpVideoEncoder::GmpInitDone(GMPVideoEncoderProxy* aGMP,
                                   GMPVideoHost* aHost,
                                   const GMPVideoCodec& aCodecParams,
                                   uint32_t aMaxPayloadSize,
                                   std::string* aErrorOut)
{
  int32_t r = GmpInitDone(aGMP, aHost, aErrorOut);
  if (r != WEBRTC_VIDEO_CODEC_OK) {
    // We might have been destroyed if GmpInitDone failed.
    // Return immediately.
    return r;
  }
  mCodecParams = aCodecParams;
  return InitEncoderForSize(aCodecParams.mWidth,
                            aCodecParams.mHeight,
                            aErrorOut);
}

void
WebrtcGmpVideoEncoder::Close_g()
{
  GMPVideoEncoderProxy* gmp(mGMP);
  mGMP = nullptr;
  mHost = nullptr;
  mInitting = false;

  if (gmp) {
    // Do this last, since this could cause us to be destroyed
    gmp->Close();
  }
}

int32_t
WebrtcGmpVideoEncoder::InitEncoderForSize(unsigned short aWidth,
                                          unsigned short aHeight,
                                          std::string* aErrorOut)
{
  mCodecParams.mWidth = aWidth;
  mCodecParams.mHeight = aHeight;
  // Pass dummy codecSpecific data for now...
  nsTArray<uint8_t> codecSpecific;

  GMPErr err = mGMP->InitEncode(mCodecParams, codecSpecific, this, 1, mMaxPayloadSize);
  if (err != GMPNoErr) {
    *aErrorOut = "GMP Encode: InitEncode failed";
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  return WEBRTC_VIDEO_CODEC_OK;
}


int32_t
WebrtcGmpVideoEncoder::Encode(const webrtc::VideoFrame& aInputImage,
                              const webrtc::CodecSpecificInfo* aCodecSpecificInfo,
                              const std::vector<webrtc::FrameType>* aFrameTypes)
{
  MOZ_ASSERT(aInputImage.width() >= 0 && aInputImage.height() >= 0);
  // Would be really nice to avoid this sync dispatch, but it would require a
  // copy of the frame, since it doesn't appear to actually have a refcount.
  // Passing 'this' is safe since this is synchronous.
  mGMPThread->Dispatch(
      WrapRunnable(this,
                   &WebrtcGmpVideoEncoder::Encode_g,
                   &aInputImage,
                   aCodecSpecificInfo,
                   aFrameTypes),
      NS_DISPATCH_SYNC);

  return WEBRTC_VIDEO_CODEC_OK;
}

void
WebrtcGmpVideoEncoder::RegetEncoderForResolutionChange(
    uint32_t aWidth,
    uint32_t aHeight,
    const RefPtr<GmpInitDoneRunnable>& aInitDone)
{
  Close_g();

  UniquePtr<GetGMPVideoEncoderCallback> callback(
    new InitDoneForResolutionChangeCallback(this,
                                            aInitDone,
                                            aWidth,
                                            aHeight));

  // OpenH264 codec (at least) can't handle dynamic input resolution changes
  // re-init the plugin when the resolution changes
  // XXX allow codec to indicate it doesn't need re-init!
  nsTArray<nsCString> tags;
  tags.AppendElement(NS_LITERAL_CSTRING("h264"));
  mInitting = true;
  if (NS_WARN_IF(NS_FAILED(mMPS->GetGMPVideoEncoder(nullptr,
                                                    &tags,
                                                    NS_LITERAL_CSTRING(""),
                                                    Move(callback))))) {
    aInitDone->Dispatch(WEBRTC_VIDEO_CODEC_ERROR,
                        "GMP Encode: GetGMPVideoEncoder failed");
  }
}

int32_t
WebrtcGmpVideoEncoder::Encode_g(const webrtc::VideoFrame* aInputImage,
                                const webrtc::CodecSpecificInfo* aCodecSpecificInfo,
                                const std::vector<webrtc::FrameType>* aFrameTypes)
{
  if (!mGMP) {
    // destroyed via Terminate(), failed to init, or just not initted yet
    LOGD(("GMP Encode: not initted yet"));
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  MOZ_ASSERT(mHost);

  if (static_cast<uint32_t>(aInputImage->width()) != mCodecParams.mWidth ||
      static_cast<uint32_t>(aInputImage->height()) != mCodecParams.mHeight) {
    LOGD(("GMP Encode: resolution change from %ux%u to %dx%d",
          mCodecParams.mWidth, mCodecParams.mHeight, aInputImage->width(), aInputImage->height()));

    RefPtr<GmpInitDoneRunnable> initDone(new GmpInitDoneRunnable(mPCHandle));
    RegetEncoderForResolutionChange(aInputImage->width(),
                                    aInputImage->height(),
                                    initDone);
    if (!mGMP) {
      // We needed to go async to re-get the encoder. Bail.
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
  }

  GMPVideoFrame* ftmp = nullptr;
  GMPErr err = mHost->CreateFrame(kGMPI420VideoFrame, &ftmp);
  if (err != GMPNoErr) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  GMPUniquePtr<GMPVideoi420Frame> frame(static_cast<GMPVideoi420Frame*>(ftmp));
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> input_image = aInputImage->video_frame_buffer();
  // check for overflow of stride * height
  CheckedInt32 ysize = CheckedInt32(input_image->StrideY()) * input_image->height();
  MOZ_RELEASE_ASSERT(ysize.isValid());
  // I will assume that if that doesn't overflow, the others case - YUV
  // 4:2:0 has U/V widths <= Y, even with alignment issues.
  err = frame->CreateFrame(ysize.value(),
                           input_image->DataY(),
                           input_image->StrideU() * ((input_image->height()+1)/2),
                           input_image->DataU(),
                           input_image->StrideV() * ((input_image->height()+1)/2),
                           input_image->DataV(),
                           input_image->width(),
                           input_image->height(),
                           input_image->StrideY(),
                           input_image->StrideU(),
                           input_image->StrideV());
  if (err != GMPNoErr) {
    return err;
  }
  frame->SetTimestamp((aInputImage->timestamp() * 1000ll)/90); // note: rounds down!
  //frame->SetDuration(1000000ll/30); // XXX base duration on measured current FPS - or don't bother

  // Bug XXXXXX: Set codecSpecific info
  GMPCodecSpecificInfo info;
  memset(&info, 0, sizeof(info));
  info.mCodecType = kGMPVideoCodecH264;
  nsTArray<uint8_t> codecSpecificInfo;
  codecSpecificInfo.AppendElements((uint8_t*)&info, sizeof(GMPCodecSpecificInfo));

  nsTArray<GMPVideoFrameType> gmp_frame_types;
  for (auto it = aFrameTypes->begin(); it != aFrameTypes->end(); ++it) {
    GMPVideoFrameType ft;

    int32_t ret = WebrtcFrameTypeToGmpFrameType(*it, &ft);
    if (ret != WEBRTC_VIDEO_CODEC_OK) {
      return ret;
    }

    gmp_frame_types.AppendElement(ft);
  }

  LOGD(("GMP Encode: %llu", (aInputImage->timestamp() * 1000ll)/90));
  err = mGMP->Encode(Move(frame), codecSpecificInfo, gmp_frame_types);
  if (err != GMPNoErr) {
    return err;
  }

  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t
WebrtcGmpVideoEncoder::RegisterEncodeCompleteCallback(webrtc::EncodedImageCallback* aCallback)
{
  MutexAutoLock lock(mCallbackMutex);
  mCallback = aCallback;

  return WEBRTC_VIDEO_CODEC_OK;
}

/* static */ void
WebrtcGmpVideoEncoder::ReleaseGmp_g(RefPtr<WebrtcGmpVideoEncoder>& aEncoder)
{
  aEncoder->Close_g();
}

int32_t
WebrtcGmpVideoEncoder::ReleaseGmp()
{
  LOGD(("GMP Released:"));
  if (mGMPThread) {
    mGMPThread->Dispatch(
        WrapRunnableNM(&WebrtcGmpVideoEncoder::ReleaseGmp_g,
                       RefPtr<WebrtcGmpVideoEncoder>(this)),
        NS_DISPATCH_NORMAL);
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t
WebrtcGmpVideoEncoder::SetChannelParameters(uint32_t aPacketLoss, int aRTT)
{
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t
WebrtcGmpVideoEncoder::SetRates(uint32_t aNewBitRate, uint32_t aFrameRate)
{
  MOZ_ASSERT(mGMPThread);
  if (aFrameRate == 0) {
    aFrameRate = 30; // Assume 30fps if we don't know the rate
  }
  mGMPThread->Dispatch(WrapRunnableNM(&WebrtcGmpVideoEncoder::SetRates_g,
                                      RefPtr<WebrtcGmpVideoEncoder>(this),
                                      aNewBitRate,
                                      aFrameRate),
                       NS_DISPATCH_NORMAL);

  return WEBRTC_VIDEO_CODEC_OK;
}

/* static */ int32_t
WebrtcGmpVideoEncoder::SetRates_g(RefPtr<WebrtcGmpVideoEncoder> aThis,
                                  uint32_t aNewBitRate,
                                  uint32_t aFrameRate)
{
  if (!aThis->mGMP) {
    // destroyed via Terminate()
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  GMPErr err = aThis->mGMP->SetRates(aNewBitRate, aFrameRate);
  if (err != GMPNoErr) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  return WEBRTC_VIDEO_CODEC_OK;
}

// GMPVideoEncoderCallback virtual functions.
void
WebrtcGmpVideoEncoder::Terminated()
{
  LOGD(("GMP Encoder Terminated: %p", (void *)this));

  mGMP->Close();
  mGMP = nullptr;
  mHost = nullptr;
  mInitting = false;
  // Could now notify that it's dead
}

void
WebrtcGmpVideoEncoder::Encoded(GMPVideoEncodedFrame* aEncodedFrame,
                               const nsTArray<uint8_t>& aCodecSpecificInfo)
{
  MutexAutoLock lock(mCallbackMutex);
  if (mCallback) {
    webrtc::FrameType ft;
    GmpFrameTypeToWebrtcFrameType(aEncodedFrame->FrameType(), &ft);
    uint32_t timestamp = (aEncodedFrame->TimeStamp() * 90ll + 999)/1000;

    LOGD(("GMP Encoded: %" PRIu64 ", type %d, len %d",
         aEncodedFrame->TimeStamp(),
         aEncodedFrame->BufferType(),
         aEncodedFrame->Size()));

    // Right now makes one Encoded() callback per unit
    // XXX convert to FragmentationHeader format (array of offsets and sizes plus a buffer) in
    // combination with H264 packetization changes in webrtc/trunk code
    uint8_t *buffer = aEncodedFrame->Buffer();
    uint8_t *end = aEncodedFrame->Buffer() + aEncodedFrame->Size();
    size_t size_bytes;
    switch (aEncodedFrame->BufferType()) {
      case GMP_BufferSingle:
        size_bytes = 0;
        break;
      case GMP_BufferLength8:
        size_bytes = 1;
        break;
      case GMP_BufferLength16:
        size_bytes = 2;
        break;
      case GMP_BufferLength24:
        size_bytes = 3;
        break;
      case GMP_BufferLength32:
        size_bytes = 4;
        break;
      default:
        // Really that it's not in the enum
        LOG(LogLevel::Error,
            ("GMP plugin returned incorrect type (%d)", aEncodedFrame->BufferType()));
        // XXX Bug 1041232 - need a better API for interfacing to the
        // plugin so we can kill it here
        return;
    }

    struct nal_entry {
      uint32_t offset;
      uint32_t size;
    };
    AutoTArray<nal_entry, 1> nals;
    uint32_t size = 0;
    // make sure we don't read past the end of the buffer getting the size
    while (buffer+size_bytes < end) {
      switch (aEncodedFrame->BufferType()) {
        case GMP_BufferSingle:
          size = aEncodedFrame->Size();
          break;
        case GMP_BufferLength8:
          size = *buffer++;
          break;
        case GMP_BufferLength16:
          // presumes we can do unaligned loads
          size = *(reinterpret_cast<uint16_t*>(buffer));
          buffer += 2;
          break;
        case GMP_BufferLength24:
          // 24-bits is a pain, since byte-order issues make things painful
          // I'm going to define 24-bit as little-endian always; big-endian must convert
          size = ((uint32_t) *buffer) |
                 (((uint32_t) *(buffer+1)) << 8) |
                 (((uint32_t) *(buffer+2)) << 16);
          buffer += 3;
          break;
        case GMP_BufferLength32:
          // presumes we can do unaligned loads
          size = *(reinterpret_cast<uint32_t*>(buffer));
          buffer += 4;
          break;
        default:
          MOZ_CRASH("GMP_BufferType already handled in switch above");
      }
      MOZ_ASSERT(size != 0 &&
                 buffer+size <= end); // in non-debug code, don't crash in this case
      if (size == 0 || buffer+size > end) {
        // XXX see above - should we kill the plugin for returning extra bytes?  Probably
        LOG(LogLevel::Error,
            ("GMP plugin returned badly formatted encoded data: buffer=%p, size=%d, end=%p", buffer, size, end));
        return;
      }
      // XXX optimize by making buffer an offset
      nal_entry nal = {((uint32_t) (buffer-aEncodedFrame->Buffer())), (uint32_t) size};
      nals.AppendElement(nal);
      buffer += size;
      // on last one, buffer == end normally
    }
    if (buffer != end) {
      // At most 3 bytes can be left over, depending on buffertype
      LOGD(("GMP plugin returned %td extra bytes", end - buffer));
    }

    size_t num_nals = nals.Length();
    if (num_nals > 0) {
      webrtc::RTPFragmentationHeader fragmentation;
      fragmentation.VerifyAndAllocateFragmentationHeader(num_nals);
      for (size_t i = 0; i < num_nals; i++) {
        fragmentation.fragmentationOffset[i] = nals[i].offset;
        fragmentation.fragmentationLength[i] = nals[i].size;
      }

      webrtc::EncodedImage unit(aEncodedFrame->Buffer(), size, size);
      unit._frameType = ft;
      unit._timeStamp = timestamp;
      // Ensure we ignore this when calculating RTCP timestamps
      unit.capture_time_ms_ = -1;
      unit._completeFrame = true;

      // TODO: Currently the OpenH264 codec does not preserve any codec
      //       specific info passed into it and just returns default values.
      //       If this changes in the future, it would be nice to get rid of
      //       mCodecSpecificInfo.
      mCallback->OnEncodedImage(unit, &mCodecSpecificInfo, &fragmentation);
    }
  }
}

// Decoder.
WebrtcGmpVideoDecoder::WebrtcGmpVideoDecoder() :
  mGMP(nullptr),
  mInitting(false),
  mHost(nullptr),
  mCallbackMutex("WebrtcGmpVideoDecoder decoded callback mutex"),
  mCallback(nullptr),
  mCachedPluginId(0),
  mDecoderStatus(GMPNoErr)
{
  if (mPCHandle.empty()) {
    mPCHandle = WebrtcGmpPCHandleSetter::GetCurrentHandle();
  }
  MOZ_ASSERT(!mPCHandle.empty());
}

WebrtcGmpVideoDecoder::~WebrtcGmpVideoDecoder()
{
  // We should not have been destroyed if we never closed our GMP
  MOZ_ASSERT(!mGMP);
}

int32_t
WebrtcGmpVideoDecoder::InitDecode(const webrtc::VideoCodec* aCodecSettings,
                                  int32_t aNumberOfCores)
{
  if (!mMPS) {
    mMPS = do_GetService("@mozilla.org/gecko-media-plugin-service;1");
  }
  MOZ_ASSERT(mMPS);

  if (!mGMPThread) {
    if (NS_WARN_IF(NS_FAILED(mMPS->GetThread(getter_AddRefs(mGMPThread))))) {
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
  }

  RefPtr<GmpInitDoneRunnable> initDone(new GmpInitDoneRunnable(mPCHandle));
  mGMPThread->Dispatch(WrapRunnableNM(&WebrtcGmpVideoDecoder::InitDecode_g,
                                      RefPtr<WebrtcGmpVideoDecoder>(this),
                                      aCodecSettings,
                                      aNumberOfCores,
                                      initDone),
                       NS_DISPATCH_NORMAL);

  return WEBRTC_VIDEO_CODEC_OK;
}

/* static */ void
WebrtcGmpVideoDecoder::InitDecode_g(
    const RefPtr<WebrtcGmpVideoDecoder>& aThis,
    const webrtc::VideoCodec* aCodecSettings,
    int32_t aNumberOfCores,
    const RefPtr<GmpInitDoneRunnable>& aInitDone)
{
  nsTArray<nsCString> tags;
  tags.AppendElement(NS_LITERAL_CSTRING("h264"));
  UniquePtr<GetGMPVideoDecoderCallback> callback(
    new InitDoneCallback(aThis, aInitDone));
  aThis->mInitting = true;
  nsresult rv = aThis->mMPS->GetGMPVideoDecoder(nullptr,
                                                &tags,
                                                NS_LITERAL_CSTRING(""),
                                                Move(callback));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    LOGD(("GMP Decode: GetGMPVideoDecoder failed"));
    aThis->Close_g();
    aInitDone->Dispatch(WEBRTC_VIDEO_CODEC_ERROR,
                        "GMP Decode: GetGMPVideoDecoder failed.");
  }
}

int32_t
WebrtcGmpVideoDecoder::GmpInitDone(GMPVideoDecoderProxy* aGMP,
                                   GMPVideoHost* aHost,
                                   std::string* aErrorOut)
{
  if (!mInitting || !aGMP || !aHost) {
    *aErrorOut = "GMP Decode: Either init was aborted, "
                 "or init failed to supply either a GMP decoder or GMP host.";
    if (aGMP) {
      // This could destroy us, since aGMP may be the last thing holding a ref
      // Return immediately.
      aGMP->Close();
    }
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  mInitting = false;

  if (mGMP && mGMP != aGMP) {
    Close_g();
  }

  mGMP = aGMP;
  mHost = aHost;
  mCachedPluginId = mGMP->GetPluginId();
  // Bug XXXXXX: transfer settings from codecSettings to codec.
  GMPVideoCodec codec;
  memset(&codec, 0, sizeof(codec));
  codec.mGMPApiVersion = 33;

  // XXX this is currently a hack
  //GMPVideoCodecUnion codecSpecific;
  //memset(&codecSpecific, 0, sizeof(codecSpecific));
  nsTArray<uint8_t> codecSpecific;
  nsresult rv = mGMP->InitDecode(codec, codecSpecific, this, 1);
  if (NS_FAILED(rv)) {
    *aErrorOut = "GMP Decode: InitDecode failed";
    mQueuedFrames.Clear();
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  // now release any frames that got queued waiting for InitDone
  if (!mQueuedFrames.IsEmpty()) {
    // So we're safe to call Decode_g(), which asserts it's empty
    nsTArray<UniquePtr<GMPDecodeData>> temp;
    temp.SwapElements(mQueuedFrames);
    for (auto& queued : temp) {
      int rv = Decode_g(queued->mImage,
                        queued->mMissingFrames,
                        nullptr,
                        nullptr,
                        queued->mRenderTimeMs);
      if (rv) {
        return rv;
      }
    }
  }

  return WEBRTC_VIDEO_CODEC_OK;
}

void
WebrtcGmpVideoDecoder::Close_g()
{
  GMPVideoDecoderProxy* gmp(mGMP);
  mGMP = nullptr;
  mHost = nullptr;
  mInitting = false;

  if (gmp) {
    // Do this last, since this could cause us to be destroyed
    gmp->Close();
  }
}

int32_t
WebrtcGmpVideoDecoder::Decode(const webrtc::EncodedImage& aInputImage,
                              bool aMissingFrames,
                              const webrtc::RTPFragmentationHeader* aFragmentation,
                              const webrtc::CodecSpecificInfo* aCodecSpecificInfo,
                              int64_t aRenderTimeMs)
{
  int32_t ret;
  MOZ_ASSERT(mGMPThread);
  MOZ_ASSERT(!NS_IsMainThread());
  // Would be really nice to avoid this sync dispatch, but it would require a
  // copy of the frame, since it doesn't appear to actually have a refcount.
  // Passing 'this' is safe since this is synchronous.
  mozilla::SyncRunnable::DispatchToThread(mGMPThread,
                WrapRunnableRet(&ret, this,
                                &WebrtcGmpVideoDecoder::Decode_g,
                                aInputImage,
                                aMissingFrames,
                                aFragmentation,
                                aCodecSpecificInfo,
                                aRenderTimeMs));

  return ret;
}

int32_t
WebrtcGmpVideoDecoder::Decode_g(const webrtc::EncodedImage& aInputImage,
                                bool aMissingFrames,
                                const webrtc::RTPFragmentationHeader* aFragmentation,
                                const webrtc::CodecSpecificInfo* aCodecSpecificInfo,
                                int64_t aRenderTimeMs)
{
  if (!mGMP) {
    if (mInitting) {
      // InitDone hasn't been called yet (race)
      GMPDecodeData *data = new GMPDecodeData(aInputImage,
                                              aMissingFrames,
                                              aRenderTimeMs);
      mQueuedFrames.AppendElement(data);
      return WEBRTC_VIDEO_CODEC_OK;
    }
    // destroyed via Terminate(), failed to init, or just not initted yet
    LOGD(("GMP Decode: not initted yet"));
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  MOZ_ASSERT(mQueuedFrames.IsEmpty());
  MOZ_ASSERT(mHost);

  if (!aInputImage._length) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  GMPVideoFrame* ftmp = nullptr;
  GMPErr err = mHost->CreateFrame(kGMPEncodedVideoFrame, &ftmp);
  if (err != GMPNoErr) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  GMPUniquePtr<GMPVideoEncodedFrame> frame(static_cast<GMPVideoEncodedFrame*>(ftmp));
  err = frame->CreateEmptyFrame(aInputImage._length);
  if (err != GMPNoErr) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  // XXX At this point, we only will get mode1 data (a single length and a buffer)
  // Session_info.cc/etc code needs to change to support mode 0.
  *(reinterpret_cast<uint32_t*>(frame->Buffer())) = frame->Size();

  // XXX It'd be wonderful not to have to memcpy the encoded data!
  memcpy(frame->Buffer()+4, aInputImage._buffer+4, frame->Size()-4);

  frame->SetEncodedWidth(aInputImage._encodedWidth);
  frame->SetEncodedHeight(aInputImage._encodedHeight);
  frame->SetTimeStamp((aInputImage._timeStamp * 1000ll)/90); // rounds down
  frame->SetCompleteFrame(aInputImage._completeFrame);
  frame->SetBufferType(GMP_BufferLength32);

  GMPVideoFrameType ft;
  int32_t ret = WebrtcFrameTypeToGmpFrameType(aInputImage._frameType, &ft);
  if (ret != WEBRTC_VIDEO_CODEC_OK) {
    return ret;
  }

  // Bug XXXXXX: Set codecSpecific info
  GMPCodecSpecificInfo info;
  memset(&info, 0, sizeof(info));
  info.mCodecType = kGMPVideoCodecH264;
  info.mCodecSpecific.mH264.mSimulcastIdx = 0;
  nsTArray<uint8_t> codecSpecificInfo;
  codecSpecificInfo.AppendElements((uint8_t*)&info, sizeof(GMPCodecSpecificInfo));

  LOGD(("GMP Decode: %" PRIu64 ", len %" PRIuSIZE "%s", frame->TimeStamp(), aInputImage._length,
        ft == kGMPKeyFrame ? ", KeyFrame" : ""));
  nsresult rv = mGMP->Decode(Move(frame),
                             aMissingFrames,
                             codecSpecificInfo,
                             aRenderTimeMs);
  if (NS_FAILED(rv)) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  if(mDecoderStatus != GMPNoErr){
    mDecoderStatus = GMPNoErr;
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t
WebrtcGmpVideoDecoder::RegisterDecodeCompleteCallback( webrtc::DecodedImageCallback* aCallback)
{
  MutexAutoLock lock(mCallbackMutex);
  mCallback = aCallback;

  return WEBRTC_VIDEO_CODEC_OK;
}


/* static */ void
WebrtcGmpVideoDecoder::ReleaseGmp_g(RefPtr<WebrtcGmpVideoDecoder>& aDecoder)
{
  aDecoder->Close_g();
}

int32_t
WebrtcGmpVideoDecoder::ReleaseGmp()
{
  LOGD(("GMP Released:"));
  if (mGMPThread) {
    mGMPThread->Dispatch(
        WrapRunnableNM(&WebrtcGmpVideoDecoder::ReleaseGmp_g,
                       RefPtr<WebrtcGmpVideoDecoder>(this)),
        NS_DISPATCH_NORMAL);
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

void
WebrtcGmpVideoDecoder::Terminated()
{
  LOGD(("GMP Decoder Terminated: %p", (void *)this));

  mGMP->Close();
  mGMP = nullptr;
  mHost = nullptr;
  mInitting = false;
  // Could now notify that it's dead
}

static void DeleteBuffer(uint8_t* data)
{
  delete[] data;
}

void
WebrtcGmpVideoDecoder::Decoded(GMPVideoi420Frame* aDecodedFrame)
{
  // we have two choices here: wrap the frame with a callback that frees
  // the data later (risking running out of shmems), or copy the data out
  // always.  Also, we can only Destroy() the frame on the gmp thread, so
  // copying is simplest if expensive.
  // I420 size including rounding...
  CheckedInt32 length = (CheckedInt32(aDecodedFrame->Stride(kGMPYPlane)) * aDecodedFrame->Height()) +
                        (aDecodedFrame->Stride(kGMPVPlane) +
                         aDecodedFrame->Stride(kGMPUPlane)) * ((aDecodedFrame->Height()+1)/2);
  int32_t size = length.value();
  MOZ_RELEASE_ASSERT(length.isValid() && size > 0);
  auto buffer = MakeUniqueFallible<uint8_t[]>(size);
  if (buffer) {
    // This is 3 separate buffers currently anyways, no use in trying to
    // see if we can use a single memcpy.
    uint8_t* buffer_y = buffer.get();
    memcpy(buffer_y, aDecodedFrame->Buffer(kGMPYPlane),
           aDecodedFrame->Stride(kGMPYPlane) * aDecodedFrame->Height());
    // Should this be aligned, making it non-contiguous?  Assume no, this is already
    // factored into the strides.
    uint8_t* buffer_u = buffer_y +
                        aDecodedFrame->Stride(kGMPYPlane) * aDecodedFrame->Height();
    memcpy(buffer_u, aDecodedFrame->Buffer(kGMPUPlane),
           aDecodedFrame->Stride(kGMPUPlane) * ((aDecodedFrame->Height()+1)/2));
    uint8_t* buffer_v = buffer_u +
                        aDecodedFrame->Stride(kGMPUPlane) * ((aDecodedFrame->Height()+1)/2);
    memcpy(buffer_v, aDecodedFrame->Buffer(kGMPVPlane),
           aDecodedFrame->Stride(kGMPVPlane) * ((aDecodedFrame->Height()+1)/2));

    MutexAutoLock lock(mCallbackMutex);
    if (mCallback) {
      rtc::scoped_refptr<webrtc::WrappedI420Buffer> video_frame_buffer(
        new rtc::RefCountedObject<webrtc::WrappedI420Buffer>(
          aDecodedFrame->Width(), aDecodedFrame->Height(),
          buffer_y, aDecodedFrame->Stride(kGMPYPlane),
          buffer_u, aDecodedFrame->Stride(kGMPUPlane),
          buffer_v, aDecodedFrame->Stride(kGMPVPlane),
          rtc::Bind(&DeleteBuffer, buffer.release())));

      webrtc::VideoFrame image(video_frame_buffer, 0, 0,
                               webrtc::kVideoRotation_0);
      image.set_timestamp((aDecodedFrame->Timestamp() * 90ll + 999)/1000); // round up
      image.set_render_time_ms(0);

      LOGD(("GMP Decoded: %" PRIu64, aDecodedFrame->Timestamp()));
      mCallback->Decoded(image);
    }
  }
  aDecodedFrame->Destroy();
}

}
