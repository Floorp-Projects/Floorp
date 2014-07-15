/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 2; -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/unused.h"
#include "nsIAppShell.h"
#include "nsScreenManagerProxy.h"
#include "nsServiceManagerUtils.h"
#include "nsWidgetsCID.h"
#include "ScreenProxy.h"

namespace mozilla {
namespace widget {

using namespace mozilla::dom;

static NS_DEFINE_CID(kAppShellCID, NS_APPSHELL_CID);

ScreenProxy::ScreenProxy(nsScreenManagerProxy* aScreenManager, ScreenDetails aDetails)
  : mScreenManager(aScreenManager)
  , mCacheValid(false)
  , mCacheWillInvalidate(false)
{
  PopulateByDetails(aDetails);
}

NS_IMETHODIMP
ScreenProxy::GetId(uint32_t *outId)
{
  *outId = mId;
  return NS_OK;
}

NS_IMETHODIMP
ScreenProxy::GetRect(int32_t *outLeft,
                     int32_t *outTop,
                     int32_t *outWidth,
                     int32_t *outHeight)
{
  if (!EnsureCacheIsValid()) {
    return NS_ERROR_FAILURE;
  }

  *outLeft = mRect.x;
  *outTop = mRect.y;
  *outWidth = mRect.width;
  *outHeight = mRect.height;
  return NS_OK;
}

NS_IMETHODIMP
ScreenProxy::GetAvailRect(int32_t *outLeft,
                          int32_t *outTop,
                          int32_t *outWidth,
                          int32_t *outHeight)
{
  if (!EnsureCacheIsValid()) {
    return NS_ERROR_FAILURE;
  }

  *outLeft = mAvailRect.x;
  *outTop = mAvailRect.y;
  *outWidth = mAvailRect.width;
  *outHeight = mAvailRect.height;
  return NS_OK;
}

NS_IMETHODIMP
ScreenProxy::GetPixelDepth(int32_t *aPixelDepth)
{
  if (!EnsureCacheIsValid()) {
    return NS_ERROR_FAILURE;
  }

  *aPixelDepth = mPixelDepth;
  return NS_OK;
}

NS_IMETHODIMP
ScreenProxy::GetColorDepth(int32_t *aColorDepth)
{
  if (!EnsureCacheIsValid()) {
    return NS_ERROR_FAILURE;
  }

  *aColorDepth = mColorDepth;
  return NS_OK;
}

void
ScreenProxy::PopulateByDetails(ScreenDetails aDetails)
{
  mId = aDetails.id();
  mRect = nsIntRect(aDetails.rect());
  mAvailRect = nsIntRect(aDetails.availRect());
  mPixelDepth = aDetails.pixelDepth();
  mColorDepth = aDetails.colorDepth();
  mContentsScaleFactor = aDetails.contentsScaleFactor();
}

bool
ScreenProxy::EnsureCacheIsValid()
{
  if (mCacheValid) {
    return true;
  }

  bool success = false;
  // Kick off a synchronous IPC call to the parent to get the
  // most up-to-date information.
  ScreenDetails details;
  unused << mScreenManager->SendScreenRefresh(mId, &details, &success);
  if (!success) {
    NS_WARNING("Updating a ScreenProxy in the child process failed on parent side.");
    return false;
  }

  PopulateByDetails(details);
  mCacheValid = true;

  InvalidateCacheOnNextTick();
  return true;
}

void
ScreenProxy::InvalidateCacheOnNextTick()
{
  if (mCacheWillInvalidate) {
    return;
  }

  mCacheWillInvalidate = true;

  nsCOMPtr<nsIAppShell> appShell = do_GetService(kAppShellCID);
  if (appShell) {
    appShell->RunInStableState(
      NS_NewRunnableMethod(this, &ScreenProxy::InvalidateCache)
    );
  } else {
    // It's pretty bad news if we can't get the appshell. In that case,
    // let's just invalidate the cache right away.
    InvalidateCache();
  }
}

void
ScreenProxy::InvalidateCache()
{
  mCacheValid = false;
  mCacheWillInvalidate = false;
}

} // namespace widget
} // namespace mozilla

