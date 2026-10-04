/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "MacWebAppPresentation.h"
#include "MacWebAppService.h"
#include "MachTransport.h"
#include "Protocol.h"
#include "mozilla/Services.h"
#include "nsIObserverService.h"
#include "nsThreadUtils.h"
#import <Foundation/Foundation.h>
#import <IOSurface/IOSurface.h>

namespace mozilla::widget {
using namespace floorp::shim;

static bool Send(const nsACString& aAppId, MessageType aType,
                 NSDictionary* aPayload, mach_port_t aPort = MACH_PORT_NULL) {
  NSData* bytes = EncodePayload(aPayload);
  if (!bytes) return false;
  RefPtr<MacWebAppService> service = MacWebAppService::GetSingleton();
  return service && service->SendControl(
      aAppId, uint32_t(aType),
      nsDependentCSubstring(static_cast<const char*>(bytes.bytes), bytes.length),
      aPort);
}

MacWebAppPresentation::MacWebAppPresentation(const nsACString& aAppId,
                                             uint32_t aWindowId)
    : mAppId(aAppId), mService(MacWebAppService::GetSingleton()),
      mWindowId(aWindowId) {}

MacWebAppPresentation::~MacWebAppPresentation() = default;

bool MacWebAppPresentation::Present(
    nsTArray<layers::NativeLayerPresentation>&& aLayers, float aBackingScale) {
  MutexAutoLock lock(mMutex);
  if (mStopped) return false;
  mPendingLayers = Some(std::move(aLayers));
  mPendingScale = aBackingScale;
  if (mQueued) return true;
  mQueued = true;
  RefPtr<MacWebAppPresentation> self = this;
  nsresult rv = NS_DispatchToMainThread(NS_NewRunnableFunction(
      "MacWebAppPresentation::SendFrame",
      [self]() {
        nsTArray<layers::NativeLayerPresentation> snapshot;
        float scale;
        {
          MutexAutoLock lock(self->mMutex);
          self->mQueued = false;
          if (self->mStopped) return;
          snapshot = std::move(*self->mPendingLayers);
          self->mPendingLayers.reset();
          scale = self->mPendingScale;
        }
        self->SendFrame(std::move(snapshot), scale);
      }));
  if (NS_FAILED(rv)) mQueued = false;
  return NS_SUCCEEDED(rv);
}

void MacWebAppPresentation::SendFrame(
    nsTArray<layers::NativeLayerPresentation>&& aLayers, float aScale) {
  MOZ_ASSERT(NS_IsMainThread());
  if (mFrameId - mPresentedFrame >= 1) {
    mDeferredLayers = Some(std::move(aLayers));
    mDeferredScale = aScale;
    return;
  }
  if (aLayers.Length() > kMaxLayers || mFrameId == UINT32_MAX ||
      mSurfaceId > UINT32_MAX - kMaxLayers ||
      mLayerId > UINT32_MAX - kMaxLayers || aScale <= 0) {
    Fail();
    return;
  }
  // Validate the whole transaction before sending its first message.
  size_t bytes = 0;
  for (const auto& layer : aLayers) {
    if (!layer.mSurface && !layer.mColor) continue;
    if (layer.mIsDRM || layer.mIsHDR) {
      Fail();
      return;
    }
    if (layer.mColor) continue;
    IOSurfaceRef surface = layer.mSurface->Get();
    size_t allocation = IOSurfaceGetAllocSize(surface);
    if (allocation > kMaxSurfaceBytes ||
        IOSurfaceGetPixelFormat(surface) != 'BGRA' ||
        allocation > kMaxWindowSurfaceBytes / 2 - bytes) {
      Fail();
      return;
    }
    bytes += allocation;
  }
  uint32_t frame = ++mFrameId;
  if (!Send(mAppId, MessageType::BeginFrame,
            @{@"windowId": @(mWindowId), @"frameId": @(frame)})) {
    Fail();
    return;
  }
  std::map<uintptr_t, uint32_t> next;
  uint32_t z = 0;
  for (const auto& layer : aLayers) {
    if ((!layer.mSurface && !layer.mColor) ||
        (!layer.mColor && layer.mDisplayRect.IsEmpty())) continue;
    auto existing = mLayerIds.find(layer.mIdentity);
    uint32_t id = existing == mLayerIds.end() ? ++mLayerId : existing->second;
    next.emplace(layer.mIdentity, id);
    const auto& t = layer.mTransform;
    auto rect = [](const auto& value) -> NSDictionary* {
      return @{@"x": @(value.x), @"y": @(value.y),
               @"width": @(value.width), @"height": @(value.height)};
    };
    NSMutableDictionary* payload = [@{
      @"windowId": @(mWindowId), @"frameId": @(frame), @"layerId": @(id),
      @"positionX": @(int32_t(layer.mPosition.x)), @"positionY": @(int32_t(layer.mPosition.y)),
      @"sizeWidth": @(layer.mSize.width), @"sizeHeight": @(layer.mSize.height),
      @"scale": @(aScale), @"opacity": @1, @"zOrder": @(z++),
      @"transform16": @[@(t._11), @(t._12), @(t._13), @(t._14),
                         @(t._21), @(t._22), @(t._23), @(t._24),
                         @(t._31), @(t._32), @(t._33), @(t._34),
                         @(t._41), @(t._42), @(t._43), @(t._44)],
      @"displayRect": rect(layer.mDisplayRect),
      @"flipped": @(layer.mSurfaceIsFlipped),
      @"sampling": layer.mSamplingFilter == gfx::SamplingFilter::POINT ? @"nearest" : @"linear"
    } mutableCopy];
    if (layer.mClipRect) payload[@"clip"] = rect(*layer.mClipRect);
    if (layer.mRoundedClipRect) {
      const auto& rounded = *layer.mRoundedClipRect;
      NSMutableArray* radii = [NSMutableArray array];
      for (const auto& corner : rounded.corners.radii) {
        [radii addObject:@{@"width": @(corner.width), @"height": @(corner.height)}];
      }
      payload[@"roundedClip"] = @{@"rect": rect(rounded.rect), @"radii": radii};
    }
    if (layer.mColor) {
      auto colorClip = layers::NativeLayerCA::CalculateClipGeometry(
          layer.mSize, layer.mPosition, layer.mTransform, layer.mDisplayRect,
          layer.mClipRect, 1.0f);
      if (!colorClip || CGRectIsEmpty(*colorClip)) {
        next.erase(layer.mIdentity);
        continue;
      }
      payload[@"positionX"] = @(colorClip->origin.x);
      payload[@"positionY"] = @(colorClip->origin.y);
      payload[@"transform16"] = @[@1,@0,@0,@0,@0,@1,@0,@0,@0,@0,@1,@0,@0,@0,@0,@1];
      payload[@"sizeWidth"] = @(colorClip->size.width);
      payload[@"sizeHeight"] = @(colorClip->size.height);
      payload[@"displayRect"] = @{@"x": @0, @"y": @0,
                                  @"width": @(colorClip->size.width), @"height": @(colorClip->size.height)};
      const auto& color = *layer.mColor;
      payload[@"color"] = @{@"r": @(color.r), @"g": @(color.g), @"b": @(color.b), @"a": @(color.a)};
      if (!Send(mAppId, MessageType::SetLayer, payload)) {
        Fail();
        return;
      }
    } else {
      uint32_t surfaceId = ++mSurfaceId;
      mSurfaces.emplace(surfaceId, layer.mSurface);
      payload[@"surfaceId"] = @(surfaceId);
      Port port(IOSurfaceCreateMachPort(layer.mSurface->Get()));
      if (!MACH_PORT_VALID(port.get()) ||
          !Send(mAppId, MessageType::SetLayer, payload, port.get())) {
        Fail();
        return;
      }
    }
  }
  for (const auto& [identity, id] : mLayerIds) {
    if (!next.count(identity) &&
        !Send(mAppId, MessageType::RemoveLayer,
              @{@"windowId": @(mWindowId), @"frameId": @(frame), @"layerId": @(id)})) {
      Fail();
      return;
    }
  }
  mLayerIds = std::move(next);
  if (!Send(mAppId, MessageType::CommitFrame,
            @{@"windowId": @(mWindowId), @"frameId": @(frame)})) Fail();
}

void MacWebAppPresentation::SurfaceReleased(uint32_t aSurfaceId) {
  MOZ_ASSERT(NS_IsMainThread());
  mSurfaces.erase(aSurfaceId);
}

void MacWebAppPresentation::FramePresented(uint32_t aFrameId) {
  MOZ_ASSERT(NS_IsMainThread());
  if (aFrameId > mPresentedFrame && aFrameId <= mFrameId) {
    mPresentedFrame = aFrameId;
    if (mDeferredLayers) {
      auto layers = mDeferredLayers.extract();
      SendFrame(std::move(layers), mDeferredScale);
    }
  }
}

void MacWebAppPresentation::Stop() {
  MOZ_ASSERT(NS_IsMainThread());
  MutexAutoLock lock(mMutex);
  if (mStopped) return;
  mStopped = true;
  mPendingLayers.reset();
  mDeferredLayers.reset();
  if (mService) {
    mService->RetireSurfaces(mAppId, mWindowId, std::move(mSurfaces));
  }
  mLayerIds.clear();
}

void MacWebAppPresentation::Fail() {
  Stop();
  if (nsCOMPtr<nsIObserverService> observers = services::GetObserverService()) {
    NS_ConvertUTF8toUTF16 appId(mAppId);
    observers->NotifyObservers(nullptr, "floorp-web-app-presentation-failed",
                               appId.get());
  }
}
}  // namespace mozilla::widget
