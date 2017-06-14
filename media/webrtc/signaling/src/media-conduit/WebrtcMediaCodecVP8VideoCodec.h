/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WebrtcMediaCodecVP8VideoCodec_h__
#define WebrtcMediaCodecVP8VideoCodec_h__

#include <jni.h>

#include "mozilla/Mutex.h"
#include "nsThreadUtils.h"
#include "nsAutoPtr.h"

#include "MediaConduitInterface.h"
#include "AudioConduit.h"
#include "VideoConduit.h"
#include "GeneratedJNIWrappers.h"

#include "webrtc/modules/video_coding/include/video_codec_interface.h"

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
  virtual uint64_t PluginID() const override { return 0; }

  virtual int32_t InitEncode(const webrtc::VideoCodec* codecSettings,
                             int32_t numberOfCores,
                             size_t maxPayloadSize) override;

  virtual int32_t Encode(const webrtc::VideoFrame& inputImage,
                          const webrtc::CodecSpecificInfo* codecSpecificInfo,
                          const std::vector<webrtc::FrameType>* frame_types) override;

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

class WebrtcMediaCodecVP8VideoRemoteEncoder : public WebrtcVideoEncoder {
public:
  WebrtcMediaCodecVP8VideoRemoteEncoder() : mConvertBuf(nullptr), mConvertBufsize(0), mCallback(nullptr) {}

  ~WebrtcMediaCodecVP8VideoRemoteEncoder() override;

  // Implement VideoEncoder interface.
  uint64_t PluginID() const override { return 0; }

  int32_t InitEncode(const webrtc::VideoCodec* codecSettings,
                     int32_t numberOfCores,
                     size_t maxPayloadSize) override;

  int32_t Encode(const webrtc::VideoFrame& inputImage,
                 const webrtc::CodecSpecificInfo* codecSpecificInfo,
                 const std::vector<webrtc::FrameType>* frame_types) override;

  int32_t RegisterEncodeCompleteCallback(webrtc::EncodedImageCallback* callback) override;

  int32_t Release() override;

  int32_t SetChannelParameters(uint32_t packetLoss, int64_t rtt) override { return 0; }

  int32_t SetRates(uint32_t newBitRate, uint32_t frameRate) override;

private:
  java::CodecProxy::GlobalRef mJavaEncoder;
  java::CodecProxy::NativeCallbacks::GlobalRef mJavaCallbacks;
  uint8_t* mConvertBuf;
  uint8_t mConvertBufsize;
  webrtc::EncodedImageCallback* mCallback;
};

class WebrtcMediaCodecVP8VideoDecoder : public WebrtcVideoDecoder {
public:
  WebrtcMediaCodecVP8VideoDecoder();

  virtual ~WebrtcMediaCodecVP8VideoDecoder() override;

  // Implement VideoDecoder interface.
  virtual uint64_t PluginID() const override { return 0; }

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
