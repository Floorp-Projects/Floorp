/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WebrtcMediaCodecVP8VideoCodec_h__
#define WebrtcMediaCodecVP8VideoCodec_h__

#include "mozilla/Mutex.h"
#include "nsThreadUtils.h"
#include "nsAutoPtr.h"

#include "MediaConduitInterface.h"
#include "AudioConduit.h"
#include "VideoConduit.h"

namespace mozilla {

struct EncodedFrame {
  uint32_t width_;
  uint32_t height_;
  uint32_t timeStamp_;
  uint64_t decode_timestamp_;
};

class WebrtcAndroidMediaCodec;

class WebrtcMediaCodecVP8VideoEncoder : public WebrtcVideoEncoder {
public:
  WebrtcMediaCodecVP8VideoEncoder();

  virtual ~WebrtcMediaCodecVP8VideoEncoder() override;

  // Implement VideoEncoder interface.
  virtual const uint64_t PluginID() override { return 0; }

  virtual int32_t InitEncode(const webrtc::VideoCodec* codecSettings,
                              int32_t numberOfCores,
                              uint32_t maxPayloadSize) override;

  virtual int32_t Encode(const webrtc::I420VideoFrame& inputImage,
                          const webrtc::CodecSpecificInfo* codecSpecificInfo,
                          const std::vector<webrtc::VideoFrameType>* frame_types) override;

  virtual int32_t RegisterEncodeCompleteCallback(webrtc::EncodedImageCallback* callback) override;

  virtual int32_t Release() override;

  virtual int32_t SetChannelParameters(uint32_t packetLoss, int64_t rtt) override;

  virtual int32_t SetRates(uint32_t newBitRate, uint32_t frameRate) override;

private:
  int32_t VerifyAndAllocate(const uint32_t minimumSize);
  bool ResetInputBuffers();
  bool ResetOutputBuffers();

  size_t mMaxPayloadSize;
  uint32_t mTimestamp;
  webrtc::EncodedImage mEncodedImage;
  webrtc::EncodedImageCallback* mCallback;
  uint32_t mFrameWidth;
  uint32_t mFrameHeight;

  WebrtcAndroidMediaCodec* mMediaCodecEncoder;

  jobjectArray mInputBuffers;
  jobjectArray mOutputBuffers;
};

class WebrtcMediaCodecVP8VideoDecoder : public WebrtcVideoDecoder {
public:
  WebrtcMediaCodecVP8VideoDecoder();

  virtual ~WebrtcMediaCodecVP8VideoDecoder() override;

  // Implement VideoDecoder interface.
  virtual const uint64_t PluginID() override { return 0; }

  virtual int32_t InitDecode(const webrtc::VideoCodec* codecSettings,
                              int32_t numberOfCores) override;

  virtual int32_t Decode(const webrtc::EncodedImage& inputImage,
                          bool missingFrames,
                          const webrtc::RTPFragmentationHeader* fragmentation,
                          const webrtc::CodecSpecificInfo*
                          codecSpecificInfo = NULL,
                          int64_t renderTimeMs = -1) override;

  virtual int32_t RegisterDecodeCompleteCallback(webrtc::DecodedImageCallback* callback) override;

  virtual int32_t Release() override;

  virtual int32_t Reset() override;

private:
  void DecodeFrame(EncodedFrame* frame);
  void RunCallback();
  bool ResetInputBuffers();
  bool ResetOutputBuffers();

  webrtc::DecodedImageCallback* mCallback;

  uint32_t mFrameWidth;
  uint32_t mFrameHeight;

  WebrtcAndroidMediaCodec* mMediaCodecDecoder;
  jobjectArray mInputBuffers;
  jobjectArray mOutputBuffers;

};

}

#endif // WebrtcMediaCodecVP8VideoCodec_h__
