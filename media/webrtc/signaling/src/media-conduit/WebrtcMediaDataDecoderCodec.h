/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WebrtcMediaDataDecoderCodec_h__
#define WebrtcMediaDataDecoderCodec_h__

#include "MediaConduitInterface.h"
#include "MediaInfo.h"
#include "MediaResult.h"
#include "PlatformDecoderModule.h"
#include "webrtc/common_video/include/video_frame_buffer.h"
#include "webrtc/modules/video_coding/include/video_codec_interface.h"

namespace webrtc {
  class DecodedImageCallback;
}
namespace mozilla {
namespace layers {
  class Image;
  class ImageContainer;
}

class PDMFactory;
class TaskQueue;

class ImageBuffer : public webrtc::NativeHandleBuffer
{
public:
  explicit ImageBuffer(RefPtr<layers::Image>&& aImage);
  rtc::scoped_refptr<VideoFrameBuffer> NativeToI420Buffer() override;

private:
  RefPtr<layers::Image> mImage;
};

class WebrtcMediaDataDecoder : public WebrtcVideoDecoder
{
public:
  WebrtcMediaDataDecoder();

  // Implement VideoDecoder interface.
  uint64_t PluginID() const override { return 0; }

  int32_t InitDecode(const webrtc::VideoCodec* codecSettings,
                     int32_t numberOfCores) override;

  int32_t Decode(const webrtc::EncodedImage& inputImage,
                 bool missingFrames,
                 const webrtc::RTPFragmentationHeader* fragmentation,
                 const webrtc::CodecSpecificInfo* codecSpecificInfo = NULL,
                 int64_t renderTimeMs = -1) override;

  int32_t RegisterDecodeCompleteCallback(
    webrtc::DecodedImageCallback* callback) override;

  int32_t Release() override;

private:
  ~WebrtcMediaDataDecoder();
  void QueueFrame(MediaRawData* aFrame);
  AbstractThread* OwnerThread() const { return mTaskQueue; }
  bool OnTaskQueue() const;

  const RefPtr<TaskQueue> mTaskQueue;
  const RefPtr<layers::ImageContainer> mImageContainer;
  const RefPtr<PDMFactory> mFactory;
  RefPtr<MediaDataDecoder> mDecoder;
  webrtc::DecodedImageCallback* mCallback = nullptr;
  VideoInfo mInfo;
  TrackInfo::TrackType mTrackType;
  bool mNeedKeyframe = true;
  MozPromiseRequestHolder<MediaDataDecoder::DecodePromise> mDecodeRequest;

  Monitor mMonitor;
  // Members below are accessed via mMonitor
  MediaResult mError = NS_OK;
  MediaDataDecoder::DecodedData mResults;
};

} // namespace mozilla

#endif // WebrtcMediaDataDecoderCodec_h__
