/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageDecoderSupport.h"

#include "imgINotificationObserver.h"
#include "imgITools.h"
#include "imgINotificationObserver.h"
#include "gfxUtils.h"
#include "AndroidGraphics.h"
#include "JavaExceptions.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/gfx/Swizzle.h"

namespace mozilla {
namespace widget {

namespace {

class ImageCallbackHelper;

HashSet<RefPtr<ImageCallbackHelper>, PointerHasher<ImageCallbackHelper*>>
    gDecodeRequests;

class ImageCallbackHelper : public imgIContainerCallback,
                            public imgINotificationObserver {
 public:
  NS_DECL_ISUPPORTS

  void CompleteExceptionally(const char* aMessage) {
    mResult->CompleteExceptionally(
        java::sdk::IllegalArgumentException::New(aMessage)
            .Cast<jni::Throwable>());
    gDecodeRequests.remove(this);
  }

  void Complete(DataSourceSurface::ScopedMap& aSourceSurface, int32_t width,
                int32_t height) {
    auto pixels = mozilla::jni::ByteBuffer::New(
        reinterpret_cast<int8_t*>(aSourceSurface.GetData()),
        aSourceSurface.GetStride() * height);
    auto bitmap = java::sdk::Bitmap::CreateBitmap(
        width, height, java::sdk::Config::ARGB_8888());
    bitmap->CopyPixelsFromBuffer(pixels);
    mResult->Complete(bitmap);
    gDecodeRequests.remove(this);
  }

  ImageCallbackHelper(java::GeckoResult::Param aResult, int32_t aDesiredLength)
      : mResult(aResult), mDesiredLength(aDesiredLength), mImage(nullptr) {
    MOZ_ASSERT(mResult);
  }

  NS_IMETHOD
  OnImageReady(imgIContainer* aImage, nsresult aStatus) override {
    // Let's make sure we are alive until the request completes
    MOZ_ALWAYS_TRUE(gDecodeRequests.putNew(this));

    if (NS_FAILED(aStatus)) {
      CompleteExceptionally("Could not process image.");
      return aStatus;
    }

    mImage = aImage;
    return mImage->StartDecoding(
        imgIContainer::FLAG_SYNC_DECODE | imgIContainer::FLAG_ASYNC_NOTIFY,
        imgIContainer::FRAME_FIRST);
  }

  // This method assumes that the image is ready to be processed
  nsresult SendBitmap() {
    RefPtr<gfx::SourceSurface> surface;

    if (mDesiredLength > 0) {
      surface = mImage->GetFrameAtSize(
          gfx::IntSize(mDesiredLength, mDesiredLength),
          imgIContainer::FRAME_FIRST,
          imgIContainer::FLAG_SYNC_DECODE | imgIContainer::FLAG_ASYNC_NOTIFY);
    } else {
      surface = mImage->GetFrame(
          imgIContainer::FRAME_FIRST,
          imgIContainer::FLAG_SYNC_DECODE | imgIContainer::FLAG_ASYNC_NOTIFY);
    }

    RefPtr<DataSourceSurface> dataSurface = surface->GetDataSurface();

    NS_ENSURE_TRUE(dataSurface, NS_ERROR_FAILURE);

    int32_t width = dataSurface->GetSize().width;
    int32_t height = dataSurface->GetSize().height;

    DataSourceSurface::ScopedMap sourceMap(dataSurface,
                                           DataSourceSurface::READ);

    // Android's Bitmap only supports R8G8B8A8, so we need to convert the
    // data to the right format
    RefPtr<DataSourceSurface> destDataSurface =
        Factory::CreateDataSourceSurfaceWithStride(dataSurface->GetSize(),
                                                   SurfaceFormat::R8G8B8A8,
                                                   sourceMap.GetStride());
    NS_ENSURE_TRUE(destDataSurface, NS_ERROR_FAILURE);

    DataSourceSurface::ScopedMap destMap(destDataSurface,
                                         DataSourceSurface::READ_WRITE);

    SwizzleData(sourceMap.GetData(), sourceMap.GetStride(),
                surface->GetFormat(), destMap.GetData(), destMap.GetStride(),
                SurfaceFormat::R8G8B8A8, destDataSurface->GetSize());

    Complete(destMap, width, height);

    return NS_OK;
  }

  void Notify(imgIRequest* aRequest, int32_t aType,
              const nsIntRect* aData) override {
    if (aType == imgINotificationObserver::DECODE_COMPLETE) {
      SendBitmap();
      // Breack the cyclic reference between `ImageDecoderListener` (which is a
      // `imgIContainer`) and `ImageCallbackHelper`.
      mImage = nullptr;
    }
  }

 private:
  const java::GeckoResult::GlobalRef mResult;
  int32_t mDesiredLength;
  nsCOMPtr<imgIContainer> mImage;
  virtual ~ImageCallbackHelper() {}
};

NS_IMPL_ISUPPORTS(ImageCallbackHelper, imgIContainerCallback,
                  imgINotificationObserver)

}  // namespace

/* static */ void ImageDecoderSupport::Decode(jni::String::Param aUri,
                                              int32_t aDesiredLength,
                                              jni::Object::Param aResult) {
  auto result = java::GeckoResult::LocalRef(aResult);
  RefPtr<ImageCallbackHelper> helper =
      new ImageCallbackHelper(result, aDesiredLength);

  nsresult rv = DecodeInternal(aUri->ToString(), helper, helper);
  if (NS_FAILED(rv)) {
    helper->OnImageReady(nullptr, rv);
  }
}

/* static */ nsresult ImageDecoderSupport::DecodeInternal(
    const nsAString& aUri, imgIContainerCallback* aCallback,
    imgINotificationObserver* aObserver) {
  nsCOMPtr<imgITools> imgTools = do_GetService("@mozilla.org/image/tools;1");
  if (NS_WARN_IF(!imgTools)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aUri);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_MALFORMED_URI);

  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewChannel(getter_AddRefs(channel), uri,
                     nsContentUtils::GetSystemPrincipal(),
                     nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
                     nsIContentPolicy::TYPE_IMAGE);
  NS_ENSURE_SUCCESS(rv, rv);

  return imgTools->DecodeImageFromChannelAsync(uri, channel, aCallback,
                                               aObserver);
}

}  // namespace widget
}  // namespace mozilla
