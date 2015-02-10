/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Class templates copied from WebRTC:
/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef WEBRTCGMPVIDEOCODEC_H_
#define WEBRTCGMPVIDEOCODEC_H_

#include <iostream>
#include <queue>

#include "nsThreadUtils.h"
#include "mozilla/Monitor.h"
#include "mozilla/Mutex.h"

#include "mozIGeckoMediaPluginService.h"
#include "MediaConduitInterface.h"
#include "AudioConduit.h"
#include "VideoConduit.h"
#include "webrtc/modules/video_coding/codecs/interface/video_codec_interface.h"

#include "gmp-video-host.h"
#include "GMPVideoDecoderProxy.h"
#include "GMPVideoEncoderProxy.h"

namespace mozilla {

class WebrtcGmpVideoEncoder : public WebrtcVideoEncoder,
                              public GMPVideoEncoderCallbackProxy
{
public:
  WebrtcGmpVideoEncoder();
  virtual ~WebrtcGmpVideoEncoder();

  // Implement VideoEncoder interface.
  virtual const uint64_t PluginID() override
  {
    return mGMP ? mGMP->ParentID() : mCachedPluginId;
  }

  virtual void Terminated() override;

  virtual int32_t InitEncode(const webrtc::VideoCodec* aCodecSettings,
                             int32_t aNumberOfCores,
                             uint32_t aMaxPayloadSize) override;

  virtual int32_t Encode(const webrtc::I420VideoFrame& aInputImage,
                         const webrtc::CodecSpecificInfo* aCodecSpecificInfo,
                         const std::vector<webrtc::VideoFrameType>* aFrameTypes) override;

  virtual int32_t RegisterEncodeCompleteCallback(
    webrtc::EncodedImageCallback* aCallback) override;

  virtual int32_t Release() override;

  virtual int32_t SetChannelParameters(uint32_t aPacketLoss,
                                       int aRTT) override;

  virtual int32_t SetRates(uint32_t aNewBitRate,
                           uint32_t aFrameRate) override;

  // GMPVideoEncoderCallback virtual functions.
  virtual void Encoded(GMPVideoEncodedFrame* aEncodedFrame,
                       const nsTArray<uint8_t>& aCodecSpecificInfo) override;

  virtual void Error(GMPErr aError) override {
  }

private:
  class InitDoneRunnable : public nsRunnable
  {
  public:
    InitDoneRunnable()
      : mInitDone(false),
        mResult(WEBRTC_VIDEO_CODEC_OK),
        mThread(do_GetCurrentThread())
    {
    }

    NS_IMETHOD Run()
    {
      MOZ_ASSERT(mThread == nsCOMPtr<nsIThread>(do_GetCurrentThread()));
      mInitDone = true;
      return NS_OK;
    }

    void Dispatch(int32_t aResult)
    {
      mResult = aResult;
      mThread->Dispatch(this, NS_DISPATCH_NORMAL);
    }

    bool IsDone()
    {
      MOZ_ASSERT(nsCOMPtr<nsIThread>(do_GetCurrentThread()) == mThread);
      return mInitDone;
    }

    int32_t Result()
    {
      return mResult;
    }

  private:
    bool mInitDone;
    int32_t mResult;
    nsCOMPtr<nsIThread> mThread;
  };

  void InitEncode_g(const webrtc::VideoCodec* aCodecSettings,
                    int32_t aNumberOfCores,
                    uint32_t aMaxPayloadSize,
                    InitDoneRunnable* aInitDone);
  int32_t GmpInitDone(GMPVideoEncoderProxy* aGMP, GMPVideoHost* aHost,
                      const webrtc::VideoCodec* aCodecSettings,
                      uint32_t aMaxPayloadSize);
  int32_t InitEncoderForSize(unsigned short aWidth, unsigned short aHeight);

  class InitDoneCallback : public GetGMPVideoEncoderCallback
  {
  public:
    InitDoneCallback(WebrtcGmpVideoEncoder* aEncoder,
                     InitDoneRunnable* aInitDone,
                     const webrtc::VideoCodec* aCodecSettings,
                     uint32_t aMaxPayloadSize)
      : mEncoder(aEncoder),
        mInitDone(aInitDone),
        mCodecSettings(aCodecSettings),
        mMaxPayloadSize(aMaxPayloadSize)
    {
    }

    virtual void Done(GMPVideoEncoderProxy* aGMP, GMPVideoHost* aHost) override
    {
      mEncoder->mGMP = aGMP;
      mEncoder->mHost = aHost;
      int32_t result;
      if (aGMP || aHost) {
        result = mEncoder->GmpInitDone(aGMP, aHost, mCodecSettings,
                                       mMaxPayloadSize);
      } else {
        result = WEBRTC_VIDEO_CODEC_ERROR;
      }

      mInitDone->Dispatch(result);
    }

  private:
    WebrtcGmpVideoEncoder* mEncoder;
    nsRefPtr<InitDoneRunnable> mInitDone;
    const webrtc::VideoCodec* mCodecSettings;
    uint32_t mMaxPayloadSize;
  };

  int32_t Encode_g(const webrtc::I420VideoFrame* aInputImage,
                   const webrtc::CodecSpecificInfo* aCodecSpecificInfo,
                   const std::vector<webrtc::VideoFrameType>* aFrameTypes);
  void RegetEncoderForResolutionChange(const webrtc::I420VideoFrame* aInputImage,
                                       InitDoneRunnable* aInitDone);

  class InitDoneForResolutionChangeCallback : public GetGMPVideoEncoderCallback
  {
  public:
    InitDoneForResolutionChangeCallback(WebrtcGmpVideoEncoder* aEncoder,
                                        InitDoneRunnable* aInitDone,
                                        uint32_t aWidth,
                                        uint32_t aHeight)
      : mEncoder(aEncoder),
        mInitDone(aInitDone),
        mWidth(aWidth),
        mHeight(aHeight)
    {
    }

    virtual void Done(GMPVideoEncoderProxy* aGMP, GMPVideoHost* aHost) override
    {
      mEncoder->mGMP = aGMP;
      mEncoder->mHost = aHost;
      int32_t result;
      if (aGMP && aHost) {
        result = mEncoder->InitEncoderForSize(mWidth, mHeight);
      } else {
        result = WEBRTC_VIDEO_CODEC_ERROR;
      }

      mInitDone->Dispatch(result);
    }

  private:
    WebrtcGmpVideoEncoder* mEncoder;
    nsRefPtr<InitDoneRunnable> mInitDone;
    uint32_t mWidth;
    uint32_t mHeight;
  };

  virtual int32_t SetRates_g(uint32_t aNewBitRate,
                             uint32_t aFrameRate);

  nsCOMPtr<mozIGeckoMediaPluginService> mMPS;
  nsCOMPtr<nsIThread> mGMPThread;
  GMPVideoEncoderProxy* mGMP;
  GMPVideoHost* mHost;
  GMPVideoCodec mCodecParams;
  uint32_t mMaxPayloadSize;
  webrtc::EncodedImageCallback* mCallback;
  uint64_t mCachedPluginId;
};


class WebrtcGmpVideoDecoder : public WebrtcVideoDecoder,
                              public GMPVideoDecoderCallbackProxy
{
public:
  WebrtcGmpVideoDecoder();
  virtual ~WebrtcGmpVideoDecoder();

  // Implement VideoDecoder interface.
  virtual const uint64_t PluginID() override
  {
    return mGMP ? mGMP->ParentID() : mCachedPluginId;
  }

  virtual void Terminated() override;

  virtual int32_t InitDecode(const webrtc::VideoCodec* aCodecSettings,
                             int32_t aNumberOfCores) override;
  virtual int32_t Decode(const webrtc::EncodedImage& aInputImage,
                         bool aMissingFrames,
                         const webrtc::RTPFragmentationHeader* aFragmentation,
                         const webrtc::CodecSpecificInfo* aCodecSpecificInfo = nullptr,
                         int64_t aRenderTimeMs = -1) override;
  virtual int32_t RegisterDecodeCompleteCallback(webrtc::DecodedImageCallback* aCallback) override;

  virtual int32_t Release() override;

  virtual int32_t Reset() override;

  virtual void Decoded(GMPVideoi420Frame* aDecodedFrame) override;

  virtual void ReceivedDecodedReferenceFrame(const uint64_t aPictureId) override {
    MOZ_CRASH();
  }

  virtual void ReceivedDecodedFrame(const uint64_t aPictureId) override {
    MOZ_CRASH();
  }

  virtual void InputDataExhausted() override {
  }

  virtual void DrainComplete() override {
  }

  virtual void ResetComplete() override {
  }

  virtual void Error(GMPErr aError) override {
     mDecoderStatus = aError;
  }

private:
  class InitDoneRunnable : public nsRunnable
  {
  public:
    InitDoneRunnable()
      : mInitDone(false),
        mResult(WEBRTC_VIDEO_CODEC_OK),
        mThread(do_GetCurrentThread())
    {
    }

    NS_IMETHOD Run()
    {
      MOZ_ASSERT(mThread == nsCOMPtr<nsIThread>(do_GetCurrentThread()));
      mInitDone = true;
      return NS_OK;
    }

    void Dispatch(int32_t aResult)
    {
      mResult = aResult;
      mThread->Dispatch(this, NS_DISPATCH_NORMAL);
    }

    bool IsDone()
    {
      MOZ_ASSERT(nsCOMPtr<nsIThread>(do_GetCurrentThread()) == mThread);
      return mInitDone;
    }

    int32_t Result()
    {
      return mResult;
    }

  private:
    bool mInitDone;
    int32_t mResult;
    nsCOMPtr<nsIThread> mThread;
  };

  void InitDecode_g(const webrtc::VideoCodec* aCodecSettings,
                    int32_t aNumberOfCores,
                    InitDoneRunnable* aInitDone);
  int32_t GmpInitDone(GMPVideoDecoderProxy* aGMP, GMPVideoHost* aHost);

  class InitDoneCallback : public GetGMPVideoDecoderCallback
  {
  public:
    explicit InitDoneCallback(WebrtcGmpVideoDecoder* aDecoder,
                              InitDoneRunnable* aInitDone)
      : mDecoder(aDecoder),
        mInitDone(aInitDone)
    {
    }

    virtual void Done(GMPVideoDecoderProxy* aGMP, GMPVideoHost* aHost)
    {
      int32_t result = mDecoder->GmpInitDone(aGMP, aHost);

      mInitDone->Dispatch(result);
    }

  private:
    WebrtcGmpVideoDecoder* mDecoder;
    nsRefPtr<InitDoneRunnable> mInitDone;
};

  virtual int32_t Decode_g(const webrtc::EncodedImage& aInputImage,
                           bool aMissingFrames,
                           const webrtc::RTPFragmentationHeader* aFragmentation,
                           const webrtc::CodecSpecificInfo* aCodecSpecificInfo,
                           int64_t aRenderTimeMs);

  nsCOMPtr<mozIGeckoMediaPluginService> mMPS;
  nsCOMPtr<nsIThread> mGMPThread;
  GMPVideoDecoderProxy* mGMP; // Addref is held for us
  GMPVideoHost* mHost;
  webrtc::DecodedImageCallback* mCallback;
  uint64_t mCachedPluginId;
  GMPErr mDecoderStatus;
};

}

#endif
