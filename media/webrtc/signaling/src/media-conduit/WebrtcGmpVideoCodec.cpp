/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebrtcGmpVideoCodec.h"

#include <iostream>
#include <vector>

#include "mozilla/Move.h"
#include "mozilla/Scoped.h"
#include "mozilla/SyncRunnable.h"
#include "VideoConduit.h"
#include "AudioConduit.h"
#include "runnable_utils.h"

#include "mozIGeckoMediaPluginService.h"
#include "nsServiceManagerUtils.h"
#include "GMPVideoDecoderProxy.h"
#include "GMPVideoEncoderProxy.h"

#include "gmp-video-host.h"
#include "gmp-video-frame-i420.h"
#include "gmp-video-frame-encoded.h"

#include "webrtc/video_engine/include/vie_external_codec.h"

namespace mozilla {

#ifdef LOG
#undef LOG
#endif

#ifdef PR_LOGGING
#ifdef MOZILLA_INTERNAL_API
extern PRLogModuleInfo* GetGMPLog();
#else
// For CPP unit tests
PRLogModuleInfo*
GetGMPLog()
{
  static PRLogModuleInfo *sLog;
  if (!sLog)
    sLog = PR_NewLogModule("GMP");
  return sLog;
}
#endif
#define LOGD(msg) PR_LOG(GetGMPLog(), PR_LOG_DEBUG, msg)
#define LOG(level, msg) PR_LOG(GetGMPLog(), (level), msg)
#else
#define LOGD(msg)
#define LOG(level, msg)
#endif

// Encoder.
WebrtcGmpVideoEncoder::WebrtcGmpVideoEncoder()
  : mGMP(nullptr)
  , mHost(nullptr)
  , mCallback(nullptr)
  , mCachedPluginId(0)
{}

static void
Encoder_Close_g(GMPVideoEncoderProxy* aGMP)
{
  aGMP->Close();
}

WebrtcGmpVideoEncoder::~WebrtcGmpVideoEncoder()
{
  // Note: we only use SyncRunnables to access mGMP
  // Callbacks may occur at any time until we call Close (or receive
  // Terminated()), so call Close here synchronously.
  // Do NOT touch the refcount of 'this'!
  if (mGMPThread && mGMP) {
    mozilla::SyncRunnable::DispatchToThread(mGMPThread,
                                            WrapRunnableNM(&Encoder_Close_g, mGMP));
    mGMP = nullptr;
  }
}

static int
WebrtcFrameTypeToGmpFrameType(webrtc::VideoFrameType aIn,
                              GMPVideoFrameType *aOut)
{
  MOZ_ASSERT(aOut);
  switch(aIn) {
    case webrtc::kKeyFrame:
      *aOut = kGMPKeyFrame;
      break;
    case webrtc::kDeltaFrame:
      *aOut = kGMPDeltaFrame;
      break;
    case webrtc::kGoldenFrame:
      *aOut = kGMPGoldenFrame;
      break;
    case webrtc::kAltRefFrame:
      *aOut = kGMPAltRefFrame;
      break;
    case webrtc::kSkipFrame:
      *aOut = kGMPSkipFrame;
      break;
    default:
      MOZ_CRASH("Unexpected VideoFrameType");
  }

  return WEBRTC_VIDEO_CODEC_OK;
}

static int
GmpFrameTypeToWebrtcFrameType(GMPVideoFrameType aIn,
                              webrtc::VideoFrameType *aOut)
{
  MOZ_ASSERT(aOut);
  switch(aIn) {
    case kGMPKeyFrame:
      *aOut = webrtc::kKeyFrame;
      break;
    case kGMPDeltaFrame:
      *aOut = webrtc::kDeltaFrame;
      break;
    case kGMPGoldenFrame:
      *aOut = webrtc::kGoldenFrame;
      break;
    case kGMPAltRefFrame:
      *aOut = webrtc::kAltRefFrame;
      break;
    case kGMPSkipFrame:
      *aOut = webrtc::kSkipFrame;
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

  nsCOMPtr<nsIThread> currentThread(do_GetCurrentThread());
  MOZ_ASSERT(currentThread != mGMPThread);

  nsRefPtr<InitDoneRunnable> initDone(new InitDoneRunnable());
  mGMPThread->Dispatch(WrapRunnable(this,
                                    &WebrtcGmpVideoEncoder::InitEncode_g,
                                    aCodecSettings,
                                    aNumberOfCores,
                                    aMaxPayloadSize,
                                    initDone),
                       NS_DISPATCH_NORMAL);


  while (!initDone->IsDone()) {
    NS_ProcessNextEvent(currentThread, true);
  }

  return initDone->Result();
}

void
WebrtcGmpVideoEncoder::InitEncode_g(const webrtc::VideoCodec* aCodecSettings,
                                    int32_t aNumberOfCores,
                                    uint32_t aMaxPayloadSize,
                                    InitDoneRunnable* aInitDone)
{
  nsTArray<nsCString> tags;
  tags.AppendElement(NS_LITERAL_CSTRING("h264"));
  UniquePtr<GetGMPVideoEncoderCallback> callback(
    new InitDoneCallback(this, aInitDone, aCodecSettings, aMaxPayloadSize));
  nsresult rv = mMPS->GetGMPVideoEncoder(&tags,
                                         NS_LITERAL_CSTRING(""),
                                         Move(callback));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    mMPS = nullptr;
    mGMP = nullptr;
    mGMPThread = nullptr;
    mHost = nullptr;
    aInitDone->Dispatch(WEBRTC_VIDEO_CODEC_ERROR);
    return;
  }
}

int32_t
WebrtcGmpVideoEncoder::GmpInitDone(GMPVideoEncoderProxy* aGMP,
                                   GMPVideoHost* aHost,
                                   const webrtc::VideoCodec* aCodecSettings,
                                   uint32_t aMaxPayloadSize)
{
  mGMP = aGMP;
  mHost = aHost;
  if (!mGMP || !mHost) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  // Bug XXXXXX: transfer settings from codecSettings to codec.
  memset(&mCodecParams, 0, sizeof(mCodecParams));

  mCodecParams.mGMPApiVersion = 33;
  mCodecParams.mStartBitrate = aCodecSettings->startBitrate;
  mCodecParams.mMinBitrate = aCodecSettings->minBitrate;
  mCodecParams.mMaxBitrate = aCodecSettings->maxBitrate;
  mCodecParams.mMaxFramerate = aCodecSettings->maxFramerate;
  mMaxPayloadSize = aMaxPayloadSize;
  if (aCodecSettings->codecSpecific.H264.packetizationMode == 1) {
    mMaxPayloadSize = 0; // No limit.
  }

  if (aCodecSettings->mode == webrtc::kScreensharing) {
    mCodecParams.mMode = kGMPScreensharing;
  } else {
    mCodecParams.mMode = kGMPRealtimeVideo;
  }

  return InitEncoderForSize(aCodecSettings->width, aCodecSettings->height);
}

int32_t
WebrtcGmpVideoEncoder::InitEncoderForSize(unsigned short aWidth, unsigned short aHeight)
{
  mCodecParams.mWidth = aWidth;
  mCodecParams.mHeight = aHeight;
  // Pass dummy codecSpecific data for now...
  nsTArray<uint8_t> codecSpecific;

  GMPErr err = mGMP->InitEncode(mCodecParams, codecSpecific, this, 1, mMaxPayloadSize);
  if (err != GMPNoErr) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  return WEBRTC_VIDEO_CODEC_OK;
}


int32_t
WebrtcGmpVideoEncoder::Encode(const webrtc::I420VideoFrame& aInputImage,
                              const webrtc::CodecSpecificInfo* aCodecSpecificInfo,
                              const std::vector<webrtc::VideoFrameType>* aFrameTypes)
{
  MOZ_ASSERT(mHost);
  if (!mGMP) {
    // destroyed via Terminate()
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  MOZ_ASSERT(aInputImage.width() >= 0 && aInputImage.height() >= 0);
  if (static_cast<uint32_t>(aInputImage.width()) != mCodecParams.mWidth ||
      static_cast<uint32_t>(aInputImage.height()) != mCodecParams.mHeight) {
    LOGD(("GMP Encode: resolution change from %ux%u to %dx%d",
          mCodecParams.mWidth, mCodecParams.mHeight, aInputImage.width(), aInputImage.height()));

    nsRefPtr<InitDoneRunnable> initDone(new InitDoneRunnable());
    nsCOMPtr<nsIRunnable> task(
      WrapRunnable(this,
                   &WebrtcGmpVideoEncoder::RegetEncoderForResolutionChange,
                   &aInputImage,
                   initDone));
    mGMPThread->Dispatch(task, NS_DISPATCH_NORMAL);

    nsCOMPtr<nsIThread> currentThread(do_GetCurrentThread());
    while (!initDone->IsDone()) {
      NS_ProcessNextEvent(currentThread, true);
    }

    if (initDone->Result() != WEBRTC_VIDEO_CODEC_OK) {
      return initDone->Result();
    }
  }

  int32_t ret;
  mozilla::SyncRunnable::DispatchToThread(mGMPThread,
                WrapRunnableRet(this,
                                &WebrtcGmpVideoEncoder::Encode_g,
                                &aInputImage,
                                aCodecSpecificInfo,
                                aFrameTypes,
                                &ret));

  return ret;
}

void
WebrtcGmpVideoEncoder::RegetEncoderForResolutionChange(const webrtc::I420VideoFrame* aInputImage,
                                                       InitDoneRunnable* aInitDone)
{
  mGMP->Close();

  UniquePtr<GetGMPVideoEncoderCallback> callback(
    new InitDoneForResolutionChangeCallback(this, aInitDone,
                                            aInputImage->width(),
                                            aInputImage->height()));

  // OpenH264 codec (at least) can't handle dynamic input resolution changes
  // re-init the plugin when the resolution changes
  // XXX allow codec to indicate it doesn't need re-init!
  nsTArray<nsCString> tags;
  tags.AppendElement(NS_LITERAL_CSTRING("h264"));
  if (NS_WARN_IF(NS_FAILED(mMPS->GetGMPVideoEncoder(&tags,
                                                    NS_LITERAL_CSTRING(""),
                                                    Move(callback))))) {
    mGMP = nullptr;
    mHost = nullptr;
    aInitDone->Dispatch(WEBRTC_VIDEO_CODEC_ERROR);
  }
}

int32_t
WebrtcGmpVideoEncoder::Encode_g(const webrtc::I420VideoFrame* aInputImage,
                                const webrtc::CodecSpecificInfo* aCodecSpecificInfo,
                                const std::vector<webrtc::VideoFrameType>* aFrameTypes)
{
  MOZ_ASSERT(mHost);
  MOZ_ASSERT(mGMP);

  GMPVideoFrame* ftmp = nullptr;
  GMPErr err = mHost->CreateFrame(kGMPI420VideoFrame, &ftmp);
  if (err != GMPNoErr) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  GMPUnique<GMPVideoi420Frame> frame(static_cast<GMPVideoi420Frame*>(ftmp));

  err = frame->CreateFrame(aInputImage->allocated_size(webrtc::kYPlane),
                           aInputImage->buffer(webrtc::kYPlane),
                           aInputImage->allocated_size(webrtc::kUPlane),
                           aInputImage->buffer(webrtc::kUPlane),
                           aInputImage->allocated_size(webrtc::kVPlane),
                           aInputImage->buffer(webrtc::kVPlane),
                           aInputImage->width(),
                           aInputImage->height(),
                           aInputImage->stride(webrtc::kYPlane),
                           aInputImage->stride(webrtc::kUPlane),
                           aInputImage->stride(webrtc::kVPlane));
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
  mCallback = aCallback;

  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t
WebrtcGmpVideoEncoder::Release()
{
  LOGD(("GMP Released:"));
  // Note: we only use SyncRunnables to access mGMP
  // Callbacks may occur at any time until we call Close (or receive
  // Terminated()), so call Close here synchronously.
  if (mGMPThread && mGMP) {
    mozilla::SyncRunnable::DispatchToThread(mGMPThread,
                                            WrapRunnableNM(&Encoder_Close_g, mGMP));
  }
  // Now safe to forget things
  mMPS = nullptr;
  mGMP = nullptr;
  mHost = nullptr;
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
  int32_t ret;
  MOZ_ASSERT(mGMPThread);
  mozilla::SyncRunnable::DispatchToThread(mGMPThread,
                WrapRunnableRet(this,
                                &WebrtcGmpVideoEncoder::SetRates_g,
                                aNewBitRate, aFrameRate,
                                &ret));

  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t
WebrtcGmpVideoEncoder::SetRates_g(uint32_t aNewBitRate, uint32_t aFrameRate)
{
  if (!mGMP) {
    // destroyed via Terminate()
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  GMPErr err = mGMP->SetRates(aNewBitRate, aFrameRate);
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
  mCachedPluginId = PluginID();

  // We need to drop our reference to this
  mGMP->Close();
  mGMP = nullptr;
  // Could now notify that it's dead
}

void
WebrtcGmpVideoEncoder::Encoded(GMPVideoEncodedFrame* aEncodedFrame,
                               const nsTArray<uint8_t>& aCodecSpecificInfo)
{
  if (mCallback) { // paranoia
    webrtc::VideoFrameType ft;
    GmpFrameTypeToWebrtcFrameType(aEncodedFrame->FrameType(), &ft);
    uint32_t timestamp = (aEncodedFrame->TimeStamp() * 90ll + 999)/1000;

    LOGD(("GMP Encoded: %llu, type %d, len %d",
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
        LOG(PR_LOG_ERROR,
            ("GMP plugin returned incorrect type (%d)", aEncodedFrame->BufferType()));
        // XXX Bug 1041232 - need a better API for interfacing to the
        // plugin so we can kill it here
        return;
    }

    struct nal_entry {
      uint32_t offset;
      uint32_t size;
    };
    nsAutoTArray<nal_entry, 1> nals;
    uint32_t size;
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
      if (buffer+size > end) {
        // XXX see above - should we kill the plugin for returning extra bytes?  Probably
        LOG(PR_LOG_ERROR,
            ("GMP plugin returned badly formatted encoded data: end is %td bytes past buffer end",
             buffer+size - end));
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
      unit._completeFrame = true;

      mCallback->Encoded(unit, nullptr, &fragmentation);

    }
  }
}

// Decoder.
WebrtcGmpVideoDecoder::WebrtcGmpVideoDecoder() :
  mGMP(nullptr),
  mHost(nullptr),
  mCallback(nullptr),
  mCachedPluginId(0),
  mDecoderStatus(GMPNoErr){}

static void
Decoder_Close_g(GMPVideoDecoderProxy* aGMP)
{
  aGMP->Close();
}

WebrtcGmpVideoDecoder::~WebrtcGmpVideoDecoder()
{
  // Note: we only use SyncRunnables to access mGMP
  // Callbacks may occur at any time until we call Close (or receive
  // Terminated()), so call Close here synchronously.
  // Do NOT touch the refcount of 'this'!
  if (mGMPThread && mGMP) {
    mozilla::SyncRunnable::DispatchToThread(mGMPThread,
                                            WrapRunnableNM(&Decoder_Close_g, mGMP));
    mGMP = nullptr;
  }
}

int32_t
WebrtcGmpVideoDecoder::InitDecode(const webrtc::VideoCodec* aCodecSettings,
                                  int32_t aNumberOfCores)
{
  mMPS = do_GetService("@mozilla.org/gecko-media-plugin-service;1");
  MOZ_ASSERT(mMPS);

  if (!mGMPThread) {
    if (NS_WARN_IF(NS_FAILED(mMPS->GetThread(getter_AddRefs(mGMPThread))))) {
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
  }

  nsRefPtr<InitDoneRunnable> initDone(new InitDoneRunnable());
  mGMPThread->Dispatch(WrapRunnable(this,
                                    &WebrtcGmpVideoDecoder::InitDecode_g,
                                    aCodecSettings,
                                    aNumberOfCores,
                                    initDone.get()),
                       NS_DISPATCH_NORMAL);

  nsCOMPtr<nsIThread> currentThread(do_GetCurrentThread());
  while (!initDone->IsDone()) {
    NS_ProcessNextEvent(currentThread, true);
  }

  return initDone->Result();
}

void
WebrtcGmpVideoDecoder::InitDecode_g(const webrtc::VideoCodec* aCodecSettings,
                                    int32_t aNumberOfCores,
                                    InitDoneRunnable* aInitDone)
{
  nsTArray<nsCString> tags;
  tags.AppendElement(NS_LITERAL_CSTRING("h264"));
  UniquePtr<GetGMPVideoDecoderCallback> callback(
    new InitDoneCallback(this, aInitDone));
  if (NS_WARN_IF(NS_FAILED(mMPS->GetGMPVideoDecoder(&tags,
                                                    NS_LITERAL_CSTRING(""),
                                                    Move(callback))))) {
    mMPS = nullptr;
    mGMP = nullptr;
    mGMPThread = nullptr;
    mHost = nullptr;
    aInitDone->Dispatch(WEBRTC_VIDEO_CODEC_ERROR);
  }
}

int32_t
WebrtcGmpVideoDecoder::GmpInitDone(GMPVideoDecoderProxy* aGMP,
                                   GMPVideoHost* aHost)
{
  mGMP = aGMP;
  mHost = aHost;
  mMPS = nullptr;

  if (!mGMP || !mHost) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

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
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  return WEBRTC_VIDEO_CODEC_OK;
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
  mozilla::SyncRunnable::DispatchToThread(mGMPThread,
                WrapRunnableRet(this,
                                &WebrtcGmpVideoDecoder::Decode_g,
                                aInputImage,
                                aMissingFrames,
                                aFragmentation,
                                aCodecSpecificInfo,
                                aRenderTimeMs,
                                &ret));

  return ret;
}

int32_t
WebrtcGmpVideoDecoder::Decode_g(const webrtc::EncodedImage& aInputImage,
                                bool aMissingFrames,
                                const webrtc::RTPFragmentationHeader* aFragmentation,
                                const webrtc::CodecSpecificInfo* aCodecSpecificInfo,
                                int64_t aRenderTimeMs)
{
  MOZ_ASSERT(mHost);
  if (!mGMP) {
    // destroyed via Terminate()
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  if (!aInputImage._length) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  GMPVideoFrame* ftmp = nullptr;
  GMPErr err = mHost->CreateFrame(kGMPEncodedVideoFrame, &ftmp);
  if (err != GMPNoErr) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  GMPUnique<GMPVideoEncodedFrame> frame(static_cast<GMPVideoEncodedFrame*>(ftmp));
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

  LOGD(("GMP Decode: %llu, len %d", frame->TimeStamp(), aInputImage._length));
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
  mCallback = aCallback;

  return WEBRTC_VIDEO_CODEC_OK;
}


int32_t
WebrtcGmpVideoDecoder::Release()
{
  // Note: we only use SyncRunnables to access mGMP
  // Callbacks may occur at any time until we call Close (or receive
  // Terminated()), so call Close here synchronously.
  if (mGMPThread && mGMP) {
    mozilla::SyncRunnable::DispatchToThread(mGMPThread,
                                            WrapRunnableNM(&Decoder_Close_g, mGMP));
  }
  // Now safe to forget things
  mMPS = nullptr;
  mGMP = nullptr;
  mGMPThread = nullptr;
  mHost = nullptr;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t
WebrtcGmpVideoDecoder::Reset()
{
  // XXX ?
  return WEBRTC_VIDEO_CODEC_OK;
}

void
WebrtcGmpVideoDecoder::Terminated()
{
  LOGD(("GMP Decoder Terminated: %p", (void *)this));
  mCachedPluginId = PluginID();

  mGMP->Close();
  mGMP = nullptr;
  // Could now notify that it's dead
}

void
WebrtcGmpVideoDecoder::Decoded(GMPVideoi420Frame* aDecodedFrame)
{
  if (mCallback) { // paranioa
    webrtc::I420VideoFrame image;
    int ret = image.CreateFrame(aDecodedFrame->AllocatedSize(kGMPYPlane),
                                aDecodedFrame->Buffer(kGMPYPlane),
                                aDecodedFrame->AllocatedSize(kGMPUPlane),
                                aDecodedFrame->Buffer(kGMPUPlane),
                                aDecodedFrame->AllocatedSize(kGMPVPlane),
                                aDecodedFrame->Buffer(kGMPVPlane),
                                aDecodedFrame->Width(),
                                aDecodedFrame->Height(),
                                aDecodedFrame->Stride(kGMPYPlane),
                                aDecodedFrame->Stride(kGMPUPlane),
                                aDecodedFrame->Stride(kGMPVPlane));
    if (ret != 0) {
      return;
    }
    image.set_timestamp((aDecodedFrame->Timestamp() * 90ll + 999)/1000); // round up
    image.set_render_time_ms(0);

    LOGD(("GMP Decoded: %llu", aDecodedFrame->Timestamp()));
    mCallback->Decoded(image);
  }
  aDecodedFrame->Destroy();
}

}
