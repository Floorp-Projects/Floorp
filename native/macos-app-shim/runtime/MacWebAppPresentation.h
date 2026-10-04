/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_widget_MacWebAppPresentation_h
#define mozilla_widget_MacWebAppPresentation_h

#include <map>
#include "mozilla/Mutex.h"
#include "mozilla/layers/NativeLayerCA.h"
#include "nsString.h"

namespace mozilla::widget {

class MacWebAppService;
class MacWebAppPresentation final : public layers::NativeLayerPresentationSink {
 public:
  MacWebAppPresentation(const nsACString& aAppId, uint32_t aWindowId);
  bool Present(nsTArray<layers::NativeLayerPresentation>&& aLayers,
               float aBackingScale) override;
  void SurfaceReleased(uint32_t aSurfaceId);
  void FramePresented(uint32_t aFrameId);
  void Stop();

 private:
  ~MacWebAppPresentation() override;
  void SendFrame(nsTArray<layers::NativeLayerPresentation>&& aLayers,
                 float aBackingScale);
  void Fail();
  nsCString mAppId;
  RefPtr<MacWebAppService> mService;
  uint32_t mWindowId;
  Mutex mMutex{"MacWebAppPresentation"};
  bool mStopped MOZ_GUARDED_BY(mMutex) = false;
  bool mQueued MOZ_GUARDED_BY(mMutex) = false;
  Maybe<nsTArray<layers::NativeLayerPresentation>> mPendingLayers MOZ_GUARDED_BY(mMutex);
  float mPendingScale MOZ_GUARDED_BY(mMutex) = 1;
  Maybe<nsTArray<layers::NativeLayerPresentation>> mDeferredLayers;
  float mDeferredScale = 1;
  uint32_t mFrameId = 0;
  uint32_t mSurfaceId = 0;
  uint32_t mLayerId = 0;
  uint32_t mPresentedFrame = 0;
  std::map<uintptr_t, uint32_t> mLayerIds;
  std::map<uint32_t, RefPtr<layers::NativeLayerSurface>> mSurfaces;
};

}  // namespace mozilla::widget
#endif
