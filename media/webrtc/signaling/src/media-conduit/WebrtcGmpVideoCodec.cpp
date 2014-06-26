/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebrtcGmpVideoCodec.h"

#include <iostream>
#include <vector>

#include "mozilla/Scoped.h"
#include "VideoConduit.h"
#include "AudioConduit.h"
#include "runnable_utils.h"

#include "mozIGeckoMediaPluginService.h"
#include "nsServiceManagerUtils.h"

#include "gmp-video-host.h"
#include "gmp-video-frame-i420.h"
#include "gmp-video-frame-encoded.h"

#include "webrtc/video_engine/include/vie_external_codec.h"

namespace mozilla {

// Encoder.
WebrtcGmpVideoEncoder::WebrtcGmpVideoEncoder()
  : mGMPThread(nullptr)
  , mGMP(nullptr)
  , mHost(nullptr)
  , mCallback(nullptr)
{}

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
      MOZ_CRASH();
      return WEBRTC_VIDEO_CODEC_ERROR;
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
      MOZ_CRASH();
      return WEBRTC_VIDEO_CODEC_ERROR;
  }

  return WEBRTC_VIDEO_CODEC_OK;
}


int32_t
WebrtcGmpVideoEncoder::InitEncode(const webrtc::VideoCodec* aCodecSettings,
                                  int32_t aNumberOfCores,
                                  uint32_t aMaxPayloadSize)
{
  mMPS = do_GetService("@mozilla.org/gecko-media-plugin-service;1");
  MOZ_ASSERT(mMPS);

  nsresult rv = mMPS->GetThread(&mGMPThread);
  if (NS_FAILED(rv)) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  int32_t ret;
  RUN_ON_THREAD(mGMPThread,
                WrapRunnableRet(this,
                                &WebrtcGmpVideoEncoder::InitEncode_g,
                                aCodecSettings,
                                aNumberOfCores,
                                aMaxPayloadSize,
                                &ret));

  return ret;
}

int32_t
WebrtcGmpVideoEncoder::InitEncode_g(const webrtc::VideoCodec* aCodecSettings,
                                    int32_t aNumberOfCores,
                                    uint32_t aMaxPayloadSize)
{
  GMPVideoHost* host = nullptr;
  GMPVideoEncoder* gmp = nullptr;

  nsresult rv = mMPS->GetGMPVideoEncoderVP8(&host, &gmp);
  if (NS_FAILED(rv)) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  mGMP = gmp;
  mHost = host;

  if (!gmp || !host) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  // Bug XXXXXX: transfer settings from codecSettings to codec.
  GMPVideoCodec codec;
  memset(&codec, 0, sizeof(codec));

  codec.mWidth = aCodecSettings->width;
  codec.mHeight = aCodecSettings->height;
  codec.mStartBitrate = aCodecSettings->startBitrate;
  codec.mMinBitrate = aCodecSettings->minBitrate;
  codec.mMaxBitrate = aCodecSettings->maxBitrate;
  codec.mMaxFramerate = aCodecSettings->maxFramerate;

  GMPVideoErr err = mGMP->InitEncode(codec, this, 1, aMaxPayloadSize);
  if (err != GMPVideoNoErr) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  return WEBRTC_VIDEO_CODEC_OK;
}


int32_t
WebrtcGmpVideoEncoder::Encode(const webrtc::I420VideoFrame& aInputImage,
                              const webrtc::CodecSpecificInfo* aCodecSpecificInfo,
                              const std::vector<webrtc::VideoFrameType>* aFrameTypes)
{
  int32_t ret;
  MOZ_ASSERT(mGMPThread);
  RUN_ON_THREAD(mGMPThread,
                WrapRunnableRet(this,
                                &WebrtcGmpVideoEncoder::Encode_g,
                                &aInputImage,
                                aCodecSpecificInfo,
                                aFrameTypes,
                                &ret));

  return ret;
}


int32_t
WebrtcGmpVideoEncoder::Encode_g(const webrtc::I420VideoFrame* aInputImage,
                                const webrtc::CodecSpecificInfo* aCodecSpecificInfo,
                                const std::vector<webrtc::VideoFrameType>* aFrameTypes)
{
  MOZ_ASSERT(mHost);
  MOZ_ASSERT(mGMP);

  GMPVideoFrame* ftmp = nullptr;
  GMPVideoErr err = mHost->CreateFrame(kGMPI420VideoFrame, &ftmp);
  if (err != GMPVideoNoErr) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  GMPVideoi420Frame* frame = static_cast<GMPVideoi420Frame*>(ftmp);

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
  if (err != GMPVideoNoErr) {
    return err;
  }
  frame->SetTimestamp(aInputImage->timestamp());
  frame->SetRenderTime_ms(aInputImage->render_time_ms());

  // Bug XXXXXX: Set codecSpecific info
  GMPCodecSpecificInfo info;
  memset(&info, 0, sizeof(info));

  std::vector<GMPVideoFrameType> gmp_frame_types;
  for (auto it = aFrameTypes->begin(); it != aFrameTypes->end(); ++it) {
    GMPVideoFrameType ft;

    int32_t ret = WebrtcFrameTypeToGmpFrameType(*it, &ft);
    if (ret != WEBRTC_VIDEO_CODEC_OK) {
      return ret;
    }

    gmp_frame_types.push_back(ft);
  }

  err = mGMP->Encode(frame, info, gmp_frame_types);
  if (err != GMPVideoNoErr) {
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
  RUN_ON_THREAD(mGMPThread,
                WrapRunnableRet(this,
                                &WebrtcGmpVideoEncoder::SetRates_g,
                                aNewBitRate, aFrameRate,
                                &ret));

  return ret;
}

int32_t
WebrtcGmpVideoEncoder::SetRates_g(uint32_t aNewBitRate, uint32_t aFrameRate)
{
  MOZ_ASSERT(mGMP);
  GMPVideoErr err = mGMP->SetRates(aNewBitRate, aFrameRate);
  if (err != GMPVideoNoErr) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  return WEBRTC_VIDEO_CODEC_OK;
}

#define GMP_ENCODE_HAS_START_CODES 1
#ifdef GMP_ENCODE_HAS_START_CODES
// Temporary until inside-sandbox-code switches from start codes to the API here
static int GetNextNALUnit(const uint8_t **aData,
                          const uint8_t *aEnd, // at first byte past end
                          size_t *aNalSize)
{
  const uint8_t *data = *aData;
  uint8_t zeros = 0;

  MOZ_ASSERT(data);
  // Don't assume we start with a start code (paranoia)
  while (data < aEnd) {
    if (*data == 0) {
      zeros++;
      if (zeros > 3) {
        // internal format error; keep going anyways
        zeros = 3;
      }
    } else {
      if (*data == 0x01) {
        if (zeros >= 2) {
          // Found start code 0x000001 or 0x00000001
          MOZ_ASSERT(zeros == 3); // current temp code only handles 4-byte codes
          // now find the length of the NAL
          *aData = ++data; // start of actual data

          while (data < aEnd) {
            if (*data == 0) {
              zeros++;
              if (zeros > 3) {
                // internal format error; keep going anyways
                zeros = 3;
              }
            } else {
              if (*data == 0x01) {
                if (zeros >= 2) {
                  // Found start code 0x000001 or 0x00000001
                  *aNalSize = (data - *aData) - zeros;
                  return 0;
                }
              }
              zeros = 0;
            }
            data++;
          }
          // NAL ends at the end of the buffer
          *aNalSize = (data - *aData);
          return 0;
        }
      }
      zeros = 0;
    }
    data++;
  }
  return -1; // no nals
}

#endif

// GMPEncoderCallback virtual functions.
void
WebrtcGmpVideoEncoder::Encoded(GMPVideoEncodedFrame* aEncodedFrame,
                               const GMPCodecSpecificInfo& aCodecSpecificInfo)
{
  if (mCallback) { // paranoia
    webrtc::VideoFrameType ft;
    GmpFrameTypeToWebrtcFrameType(aEncodedFrame->FrameType(), &ft);
    GMPBufferType type = aCodecSpecificInfo.mBufferType;

#ifdef GMP_ENCODE_HAS_START_CODES
    {
      // This code will be removed when the code inside the plugin is updated
      // Break input encoded data into NALUs and convert to length+data format
      const uint8_t* data = aEncodedFrame->Buffer();
      const uint8_t* end  = data + aEncodedFrame->Size(); // at first byte past end
      size_t nalSize = 0;
      while (GetNextNALUnit(&data, end, &nalSize) == 0) {
        // Assumes 4-byte start codes (0x00000001)
        MOZ_ASSERT(data >= aEncodedFrame->Buffer() + 4);
        uint8_t *start_code = const_cast<uint8_t*>(data-sizeof(uint32_t));
        if (*start_code == 0x00 && *(start_code+1) == 0x00 &&
            *(start_code+2) == 0x00 && *(start_code+3) == 0x01) {
          *(reinterpret_cast<uint32_t*>(start_code)) = nalSize;
        }
        data += nalSize;
      }
      type = GMP_BufferLength32;
    }
#endif

    // Right now makes one Encoded() callback per unit
    // XXX convert to FragmentationHeader format (array of offsets and sizes plus a buffer) in
    // combination with H264 packetization changes in webrtc/trunk code
    uint8_t *buffer = aEncodedFrame->Buffer();
    uint8_t *end = aEncodedFrame->Buffer() + aEncodedFrame->Size();
    uint32_t size;
    while (buffer < end) {
      switch (type) {
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
          // really that it's not in the enum; gives more readable error
          MOZ_ASSERT(aCodecSpecificInfo.mBufferType != GMP_BufferSingle);
          aEncodedFrame->Destroy();
          return;
      }
      webrtc::EncodedImage unit(buffer, size, size);
      unit._frameType = ft;
      unit._timeStamp = aEncodedFrame->TimeStamp();
      unit._completeFrame = true;

      mCallback->Encoded(unit, nullptr, nullptr);

      buffer += size;
    }
  }
  aEncodedFrame->Destroy();
}


// Decoder.
WebrtcGmpVideoDecoder::WebrtcGmpVideoDecoder() :
  mGMPThread(nullptr),
  mGMP(nullptr),
  mHost(nullptr),
  mCallback(nullptr) {}

int32_t
WebrtcGmpVideoDecoder::InitDecode(const webrtc::VideoCodec* aCodecSettings,
                                  int32_t aNumberOfCores)
{
  mMPS = do_GetService("@mozilla.org/gecko-media-plugin-service;1");
  MOZ_ASSERT(mMPS);

  if (NS_WARN_IF(NS_FAILED(mMPS->GetThread(&mGMPThread))))
  {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  int32_t ret;
  RUN_ON_THREAD(mGMPThread,
                WrapRunnableRet(this,
                                &WebrtcGmpVideoDecoder::InitDecode_g,
                                aCodecSettings,
                                aNumberOfCores,
                                &ret));

  return ret;
}

int32_t
WebrtcGmpVideoDecoder::InitDecode_g(const webrtc::VideoCodec* aCodecSettings,
                                    int32_t aNumberOfCores)
{
  GMPVideoHost* host = nullptr;
  GMPVideoDecoder* gmp = nullptr;

  if (NS_WARN_IF(NS_FAILED(mMPS->GetGMPVideoDecoderVP8(&host, &gmp))))
  {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  mGMP = gmp;
  mHost = host;
  if (!gmp || !host) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  // Bug XXXXXX: transfer settings from codecSettings to codec.
  GMPVideoCodec codec;
  memset(&codec, 0, sizeof(codec));

  GMPVideoErr err = mGMP->InitDecode(codec, this, 1);
  if (err != GMPVideoNoErr) {
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
  RUN_ON_THREAD(mGMPThread,
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
  MOZ_ASSERT(mGMP);

  GMPVideoFrame* ftmp = nullptr;
  GMPVideoErr err = mHost->CreateFrame(kGMPEncodedVideoFrame, &ftmp);
  if (err != GMPVideoNoErr) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  GMPVideoEncodedFrame* frame = static_cast<GMPVideoEncodedFrame*>(ftmp);
  err = frame->CreateEmptyFrame(aInputImage._length);
  if (err != GMPVideoNoErr) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  // XXX It'd be wonderful not to have to memcpy the encoded data!
  memcpy(frame->Buffer(), aInputImage._buffer, frame->Size());

  frame->SetEncodedWidth(aInputImage._encodedWidth);
  frame->SetEncodedHeight(aInputImage._encodedHeight);
  frame->SetTimeStamp(aInputImage._timeStamp);
  frame->SetCompleteFrame(aInputImage._completeFrame);

  GMPVideoFrameType ft;
  int32_t ret = WebrtcFrameTypeToGmpFrameType(aInputImage._frameType, &ft);
  if (ret != WEBRTC_VIDEO_CODEC_OK) {
    return ret;
  }

  // Bug XXXXXX: Set codecSpecific info
  GMPCodecSpecificInfo info;
  memset(&info, 0, sizeof(info));

  err = mGMP->Decode(frame, aMissingFrames, info, aRenderTimeMs);
  if (err != GMPVideoNoErr) {
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
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t
WebrtcGmpVideoDecoder::Reset()
{
  return WEBRTC_VIDEO_CODEC_OK;
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
    image.set_timestamp(aDecodedFrame->Timestamp());
    image.set_render_time_ms(0);

    mCallback->Decoded(image);
  }
  aDecodedFrame->Destroy();
}

}
