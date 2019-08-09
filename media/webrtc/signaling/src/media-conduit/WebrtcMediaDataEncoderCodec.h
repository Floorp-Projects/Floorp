/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WebrtcMediaDataEncoderCodec_h__
#define WebrtcMediaDataEncoderCodec_h__

#include "MediaConduitInterface.h"
#include "MediaInfo.h"
#include "MediaResult.h"
#include "PlatformEncoderModule.h"
#include "webrtc/modules/video_coding/include/video_codec_interface.h"

namespace mozilla {

class PEMFactory;
class SharedThreadPool;
class TaskQueue;

class WebrtcMediaDataEncoder : public WebrtcVideoEncoder {
 public:
  WebrtcMediaDataEncoder();

  uint64_t PluginID() const override { return 0; }

  int32_t InitEncode(const webrtc::VideoCodec* aCodecSettings,
                     int32_t aNumberOfCores, size_t aMaxPayloadSize) override;

  int32_t RegisterEncodeCompleteCallback(
      webrtc::EncodedImageCallback* aCallback) override;

  int32_t Release() override;

  int32_t Encode(const webrtc::VideoFrame& aFrame,
                 const webrtc::CodecSpecificInfo* aCodecSpecificInfo,
                 const std::vector<webrtc::FrameType>* aFrameTypes) override;

  int32_t SetChannelParameters(uint32_t aPacketLoss, int64_t aRtt) override;

 private:
  ~WebrtcMediaDataEncoder() = default;

  AbstractThread* OwnerThread() const { return mTaskQueue; }
  bool OnTaskQueue() const { return OwnerThread()->IsCurrentThreadIn(); };

  const RefPtr<SharedThreadPool> mThreadPool;
  const RefPtr<TaskQueue> mTaskQueue;
  const RefPtr<PEMFactory> mFactory;
  RefPtr<MediaDataEncoder> mEncoder;
  webrtc::EncodedImageCallback* mCallback = nullptr;

  VideoInfo mInfo;
  MediaResult mError = NS_OK;
};

}  // namespace mozilla

#endif  // WebrtcMediaDataEncoderCodec_h__
