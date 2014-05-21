/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WebrtcExtVideoCodec_h__
#define WebrtcExtVideoCodec_h__

#include "nsThreadUtils.h"
#include "mozilla/Mutex.h"

#include "MediaConduitInterface.h"
#include "AudioConduit.h"
#include "VideoConduit.h"

namespace mozilla {

struct EncodedFrame {
  uint32_t width_;
  uint32_t height_;
  uint32_t timestamp_;
  uint64_t decode_timestamp_;
};

class WebrtcOMX;

class WebrtcExtVideoEncoder : public WebrtcVideoEncoder {
public:
  WebrtcExtVideoEncoder();

  virtual ~WebrtcExtVideoEncoder() MOZ_OVERRIDE;

  // Implement VideoEncoder interface.
  virtual int32_t InitEncode(const webrtc::VideoCodec* codecSettings,
                              int32_t numberOfCores,
                              uint32_t maxPayloadSize) MOZ_OVERRIDE;

  virtual int32_t Encode(const webrtc::I420VideoFrame& inputImage,
                          const webrtc::CodecSpecificInfo* codecSpecificInfo,
                          const std::vector<webrtc::VideoFrameType>* frame_types) MOZ_OVERRIDE;

  virtual int32_t RegisterEncodeCompleteCallback(webrtc::EncodedImageCallback* callback) MOZ_OVERRIDE;

  virtual int32_t Release() MOZ_OVERRIDE;

  virtual int32_t SetChannelParameters(uint32_t packetLoss, int rtt) MOZ_OVERRIDE;

  virtual int32_t SetRates(uint32_t newBitRate, uint32_t frameRate) MOZ_OVERRIDE;

private:
  int32_t VerifyAndAllocate(const uint32_t minimumSize);

  size_t mMaxPayloadSize;
  uint32_t mTimestamp;
  webrtc::EncodedImage mEncodedImage;
  webrtc::EncodedImageCallback* mCallback;

  WebrtcOMX* mOmxEncoder;
};


class WebrtcExtVideoDecoder : public WebrtcVideoDecoder {
public:
  WebrtcExtVideoDecoder();

  virtual ~WebrtcExtVideoDecoder() MOZ_OVERRIDE;

  // Implement VideoDecoder interface.
  virtual int32_t InitDecode(const webrtc::VideoCodec* codecSettings,
                              int32_t numberOfCores) MOZ_OVERRIDE;

  virtual int32_t Decode(const webrtc::EncodedImage& inputImage,
                          bool missingFrames,
                          const webrtc::RTPFragmentationHeader* fragmentation,
                          const webrtc::CodecSpecificInfo*
                          codecSpecificInfo = NULL,
                          int64_t renderTimeMs = -1) MOZ_OVERRIDE;

  virtual int32_t RegisterDecodeCompleteCallback(webrtc::DecodedImageCallback* callback) MOZ_OVERRIDE;

  virtual int32_t Release() MOZ_OVERRIDE;

  virtual int32_t Reset() MOZ_OVERRIDE;

 private:
  void DecodeFrame(EncodedFrame* frame);
  void RunCallback();

  webrtc::DecodedImageCallback* mCallback;
  webrtc::I420VideoFrame mDecodedImage;
  webrtc::I420VideoFrame mVideoFrame;
  uint32_t mFrameWidth;
  uint32_t mFrameHeight;

  WebrtcOMX* mOmxDecoder;
};

}

#endif // WebrtcExtVideoCodec_h__
