/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AndroidContentController.h"

#include "AndroidBridge.h"
#include "base/message_loop.h"
#include "mozilla/layers/APZCCallbackHelper.h"
#include "mozilla/layers/IAPZCTreeManager.h"
#include "nsIObserverService.h"
#include "nsLayoutUtils.h"
#include "nsWindow.h"

using mozilla::layers::IAPZCTreeManager;

namespace mozilla {
namespace widget {

void
AndroidContentController::Destroy()
{
    mAndroidWindow = nullptr;
    ChromeProcessController::Destroy();
}

void
AndroidContentController::NotifyDefaultPrevented(IAPZCTreeManager* aManager,
                                                 uint64_t aInputBlockId,
                                                 bool aDefaultPrevented)
{
    if (!AndroidBridge::IsJavaUiThread()) {
        // The notification must reach the APZ on the Java UI thread (aka the
        // APZ "controller" thread) but we get it from the Gecko thread, so we
        // have to throw it onto the other thread.
        AndroidBridge::Bridge()->PostTaskToUiThread(NewRunnableMethod<uint64_t, bool>(
            aManager, &IAPZCTreeManager::ContentReceivedInputBlock,
            aInputBlockId, aDefaultPrevented), 0);
        return;
    }

    aManager->ContentReceivedInputBlock(aInputBlockId, aDefaultPrevented);
}

void
AndroidContentController::DispatchSingleTapToObservers(const LayoutDevicePoint& aPoint,
                                                       const ScrollableLayerGuid& aGuid) const
{
    nsIContent* content = nsLayoutUtils::FindContentFor(aGuid.mScrollId);
    nsPresContext* context = content
        ? mozilla::layers::APZCCallbackHelper::GetPresContextForContent(content)
        : nullptr;

    if (!context) {
      return;
    }

    CSSPoint point = mozilla::layers::APZCCallbackHelper::ApplyCallbackTransform(
        aPoint / context->CSSToDevPixelScale(), aGuid);

    nsPresContext* rcdContext = context->GetToplevelContentDocumentPresContext();
    if (rcdContext && rcdContext->PresShell()->ScaleToResolution()) {
        // We need to convert from the root document to the root content document,
        // by unapplying the resolution that's on the content document.
        const float resolution = rcdContext->PresShell()->GetResolution();
        point.x /= resolution;
        point.y /= resolution;
    }

    CSSIntPoint rounded = RoundedToInt(point);
    nsAppShell::PostEvent([rounded] {
        nsCOMPtr<nsIObserverService> obsServ =
            mozilla::services::GetObserverService();
        if (!obsServ) {
            return;
        }

        nsPrintfCString data("{\"x\":%d,\"y\":%d}", rounded.x, rounded.y);
        obsServ->NotifyObservers(nullptr, "Gesture:SingleTap",
                                 NS_ConvertASCIItoUTF16(data).get());
    });
}

void
AndroidContentController::HandleTap(TapType aType, const LayoutDevicePoint& aPoint,
                                    Modifiers aModifiers,
                                    const ScrollableLayerGuid& aGuid,
                                    uint64_t aInputBlockId)
{
    // This function will get invoked first on the Java UI thread, and then
    // again on the main thread (because of the code in ChromeProcessController::
    // HandleTap). We want to post the SingleTap message once; it can be
    // done from either thread but we need access to the callback transform
    // so we do it from the main thread.
    if (NS_IsMainThread() && aType == TapType::eSingleTap) {
        DispatchSingleTapToObservers(aPoint, aGuid);
    }

    ChromeProcessController::HandleTap(aType, aPoint, aModifiers, aGuid, aInputBlockId);
}

void
AndroidContentController::PostDelayedTask(already_AddRefed<Runnable> aTask, int aDelayMs)
{
    AndroidBridge::Bridge()->PostTaskToUiThread(Move(aTask), aDelayMs);
}
void
AndroidContentController::UpdateOverscrollVelocity(const float aX, const float aY, const bool aIsRootContent)
{
  if (aIsRootContent && mAndroidWindow) {
    mAndroidWindow->UpdateOverscrollVelocity(aX, aY);
  }
}

void
AndroidContentController::UpdateOverscrollOffset(const float aX, const float aY, const bool aIsRootContent)
{
  if (aIsRootContent && mAndroidWindow) {
    mAndroidWindow->UpdateOverscrollOffset(aX, aY);
  }
}

void
AndroidContentController::SetScrollingRootContent(const bool isRootContent)
{
  if (mAndroidWindow) {
    mAndroidWindow->SetScrollingRootContent(isRootContent);
  }
}

void
AndroidContentController::NotifyAPZStateChange(const ScrollableLayerGuid& aGuid,
                                               APZStateChange aChange,
                                               int aArg)
{
  // This function may get invoked twice, if the first invocation is not on
  // the main thread then the ChromeProcessController version of this function
  // will redispatch to the main thread. We want to make sure that our handling
  // only happens on the main thread.
  ChromeProcessController::NotifyAPZStateChange(aGuid, aChange, aArg);
  if (NS_IsMainThread()) {
    nsCOMPtr<nsIObserverService> observerService = mozilla::services::GetObserverService();
    if (aChange == layers::GeckoContentController::APZStateChange::eTransformEnd) {
      // This is used by tests to determine when the APZ is done doing whatever
      // it's doing. XXX generify this as needed when writing additional tests.
      observerService->NotifyObservers(nullptr, "APZ:TransformEnd", nullptr);
      observerService->NotifyObservers(nullptr, "PanZoom:StateChange", u"NOTHING");
    } else if (aChange == layers::GeckoContentController::APZStateChange::eTransformBegin) {
      observerService->NotifyObservers(nullptr, "PanZoom:StateChange", u"PANNING");
    }
  }
}

} // namespace widget
} // namespace mozilla
