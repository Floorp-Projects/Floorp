/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBRTC_GONK
#pragma error WebrtcOMXH264VideoCodec works only on B2G.
#endif

#ifndef WEBRTC_OMX_H264_CODEC_H_
#define WEBRTC_OMX_H264_CODEC_H_

#include "AudioConduit.h"
#include "VideoConduit.h"
#include <foundation/ABase.h>
#include <utils/RefBase.h>
#include "OMXCodecWrapper.h"

namespace android {
  class OMXVideoEncoder;
}

namespace mozilla {

class WebrtcOMXDecoder;
class OMXOutputDrain;

// XXX see if we can reduce this
#define WEBRTC_OMX_H264_MIN_DECODE_BUFFERS 10
#define OMX_IDR_NEEDED_FOR_BITRATE 0

class WebrtcOMXH264VideoEncoder : public WebrtcVideoEncoder
{
public:
  WebrtcOMXH264VideoEncoder();

  virtual ~WebrtcOMXH264VideoEncoder();

  // Implement VideoEncoder interface.
  virtual const uint64_t PluginID() override { return 0; }

  virtual int32_t InitEncode(const webrtc::VideoCodec* aCodecSettings,
                             int32_t aNumOfCores,
                             uint32_t aMaxPayloadSize) override;

  virtual int32_t Encode(const webrtc::I420VideoFrame& aInputImage,
                         const webrtc::CodecSpecificInfo* aCodecSpecificInfo,
                         const std::vector<webrtc::VideoFrameType>* aFrameTypes) override;

  virtual int32_t RegisterEncodeCompleteCallback(webrtc::EncodedImageCallback* aCallback) override;

  virtual int32_t Release() override;

  virtual int32_t SetChannelParameters(uint32_t aPacketLossRate,
                                       int64_t aRoundTripTimeMs) override;

  virtual int32_t SetRates(uint32_t aBitRate, uint32_t aFrameRate) override;

private:
  nsAutoPtr<android::OMXVideoEncoder> mOMX;
  android::sp<android::OMXCodecReservation> mReservation;

  webrtc::EncodedImageCallback* mCallback;
  RefPtr<OMXOutputDrain> mOutputDrain;
  uint32_t mWidth;
  uint32_t mHeight;
  uint32_t mFrameRate;
  uint32_t mBitRateKbps;
#ifdef OMX_IDR_NEEDED_FOR_BITRATE
  uint32_t mBitRateAtLastIDR;
  TimeStamp mLastIDRTime;
#endif
  bool mOMXConfigured;
  bool mOMXReconfigure;
  webrtc::EncodedImage mEncodedImage;
};

class WebrtcOMXH264VideoDecoder : public WebrtcVideoDecoder
{
public:
  WebrtcOMXH264VideoDecoder();

  virtual ~WebrtcOMXH264VideoDecoder();

  // Implement VideoDecoder interface.
  virtual const uint64_t PluginID() override { return 0; }

  virtual int32_t InitDecode(const webrtc::VideoCodec* aCodecSettings,
                             int32_t aNumOfCores) override;
  virtual int32_t Decode(const webrtc::EncodedImage& aInputImage,
                         bool aMissingFrames,
                         const webrtc::RTPFragmentationHeader* aFragmentation,
                         const webrtc::CodecSpecificInfo* aCodecSpecificInfo = nullptr,
                         int64_t aRenderTimeMs = -1) override;
  virtual int32_t RegisterDecodeCompleteCallback(webrtc::DecodedImageCallback* callback) override;

  virtual int32_t Release() override;

  virtual int32_t Reset() override;

private:
  webrtc::DecodedImageCallback* mCallback;
  RefPtr<WebrtcOMXDecoder> mOMX;
  android::sp<android::OMXCodecReservation> mReservation;
};

}

#endif // WEBRTC_OMX_H264_CODEC_H_
