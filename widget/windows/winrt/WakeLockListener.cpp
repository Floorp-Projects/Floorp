/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WakeLockListener.h"
#include "MetroUtils.h"

using namespace mozilla::widget::winrt;

NS_IMPL_ISUPPORTS(WakeLockListener, nsIDOMMozWakeLockListener)

NS_IMETHODIMP
WakeLockListener::Callback(const nsAString& aTopic, const nsAString& aState)
{
  if (!mDisplayRequest) {
    if (FAILED(ActivateGenericInstance(RuntimeClass_Windows_System_Display_DisplayRequest, mDisplayRequest))) {
      NS_WARNING("Failed to instantiate IDisplayRequest, wakelocks will be broken!");
      return NS_OK;
    }
  }

  if (aState.EqualsLiteral("locked-foreground")) {
    mDisplayRequest->RequestActive();
  } else {
    mDisplayRequest->RequestRelease();
  }

  return NS_OK;
}
