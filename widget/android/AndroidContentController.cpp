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
