/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WebrtcImageBuffer_h__
#define WebrtcImageBuffer_h__

#include "webrtc/common_video/include/video_frame_buffer.h"
#include "webrtc/rtc_base/keep_ref_until_done.h"

namespace mozilla {
namespace layers {
class Image;
}

class ImageBuffer : public webrtc::VideoFrameBuffer {
 public:
  explicit ImageBuffer(RefPtr<layers::Image>&& aImage)
      : mImage(std::move(aImage)) {}

  rtc::scoped_refptr<webrtc::I420BufferInterface> ToI420() override {
    RefPtr<layers::PlanarYCbCrImage> image = mImage->AsPlanarYCbCrImage();
    MOZ_ASSERT(image);
    if (!image) {
      // TODO. YUV420 ReadBack, Image only provides a RGB readback.
      return nullptr;
    }
    const layers::PlanarYCbCrData* data = image->GetData();
    rtc::scoped_refptr<webrtc::I420BufferInterface> buf(
        new rtc::RefCountedObject<webrtc::WrappedI420Buffer>(
            data->mPicSize.width, data->mPicSize.height, data->mYChannel,
            data->mYStride, data->mCbChannel, data->mCbCrStride,
            data->mCrChannel, data->mCbCrStride,
            rtc::KeepRefUntilDone(image.get())));
    return buf;
  }

  Type type() const override { return Type::kNative; }

  int width() const override { return mImage->GetSize().width; }

  int height() const override { return mImage->GetSize().height; }

  RefPtr<layers::Image> GetNativeImage() const { return mImage; }

 private:
  const RefPtr<layers::Image> mImage;
};

}  // namespace mozilla

#endif  // WebrtcImageBuffer_h__
