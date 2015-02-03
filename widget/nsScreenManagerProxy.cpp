/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/unused.h"
#include "mozilla/dom/ContentChild.h"
#include "nsScreenManagerProxy.h"
#include "nsServiceManagerUtils.h"
#include "nsIAppShell.h"
#include "nsIScreen.h"
#include "nsIScreenManager.h"
#include "nsWidgetsCID.h"

static NS_DEFINE_CID(kAppShellCID, NS_APPSHELL_CID);

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::widget;

NS_IMPL_ISUPPORTS(nsScreenManagerProxy, nsIScreenManager)

nsScreenManagerProxy::nsScreenManagerProxy()
  : mNumberOfScreens(-1)
  , mSystemDefaultScale(1.0)
  , mCacheValid(true)
  , mCacheWillInvalidate(false)
{
  bool success = false;
  unused << ContentChild::GetSingleton()->SendPScreenManagerConstructor(
                                            this,
                                            &mNumberOfScreens,
                                            &mSystemDefaultScale,
                                            &success);

  if (!success) {
    // We're in bad shape. We'll return the default values, but we'll basically
    // be lying.
    NS_WARNING("Setting up communications with the parent nsIScreenManager failed.");
  }

  InvalidateCacheOnNextTick();

  // nsScreenManagerProxy is a service, which will always have a reference
  // held to it by the Component Manager once the service is requested.
  // However, nsScreenManagerProxy also implements PScreenManagerChild, and
  // that means that the manager of the PScreenManager protocol (PContent
  // in this case) needs to know how to deallocate this actor. We AddRef here
  // so that in the event that PContent tries to deallocate us either before
  // or after process shutdown, we don't try to do a double-free.
  AddRef();
}

/**
 * nsIScreenManager
 **/

NS_IMETHODIMP
nsScreenManagerProxy::GetPrimaryScreen(nsIScreen** outScreen)
{
  InvalidateCacheOnNextTick();

  if (!mPrimaryScreen) {
    ScreenDetails details;
    bool success = false;
    unused << SendGetPrimaryScreen(&details, &success);
    if (!success) {
      return NS_ERROR_FAILURE;
    }

    mPrimaryScreen = new ScreenProxy(this, details);
  }
  NS_ADDREF(*outScreen = mPrimaryScreen);
  return NS_OK;
}

NS_IMETHODIMP
nsScreenManagerProxy::ScreenForId(uint32_t aId, nsIScreen** outScreen)
{
  // At this time, there's no need for child processes to query for
  // screens by ID.
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsScreenManagerProxy::ScreenForRect(int32_t inLeft,
                                    int32_t inTop,
                                    int32_t inWidth,
                                    int32_t inHeight,
                                    nsIScreen** outScreen)
{
  bool success = false;
  ScreenDetails details;
  unused << SendScreenForRect(inLeft, inTop, inWidth, inHeight, &details, &success);
  if (!success) {
    return NS_ERROR_FAILURE;
  }

  nsRefPtr<ScreenProxy> screen = new ScreenProxy(this, details);
  NS_ADDREF(*outScreen = screen);

  return NS_OK;
}

NS_IMETHODIMP
nsScreenManagerProxy::ScreenForNativeWidget(void* aWidget,
                                            nsIScreen** outScreen)
{
  // Because ScreenForNativeWidget can be called numerous times
  // indirectly from content via the DOM Screen API, we cache the
  // results for this tick of the event loop.
  TabChild* tabChild = static_cast<TabChild*>(aWidget);

  // Enumerate the cached screen array, looking for one that has
  // the TabChild that we're looking for...
  for (uint32_t i = 0; i < mScreenCache.Length(); ++i) {
      ScreenCacheEntry& curr = mScreenCache[i];
      if (curr.mTabChild == aWidget) {
          NS_ADDREF(*outScreen = static_cast<nsIScreen*>(curr.mScreenProxy));
          return NS_OK;
      }
  }

  // Never cached this screen, so we have to ask the parent process
  // for it.
  bool success = false;
  ScreenDetails details;
  unused << SendScreenForBrowser(tabChild->GetTabId(), &details, &success);
  if (!success) {
    return NS_ERROR_FAILURE;
  }

  ScreenCacheEntry newEntry;
  nsRefPtr<ScreenProxy> screen = new ScreenProxy(this, details);

  newEntry.mScreenProxy = screen;
  newEntry.mTabChild = tabChild;

  mScreenCache.AppendElement(newEntry);

  NS_ADDREF(*outScreen = screen);

  InvalidateCacheOnNextTick();
  return NS_OK;
}

NS_IMETHODIMP
nsScreenManagerProxy::GetNumberOfScreens(uint32_t* aNumberOfScreens)
{
  if (!EnsureCacheIsValid()) {
    return NS_ERROR_FAILURE;
  }

  *aNumberOfScreens = mNumberOfScreens;
  return NS_OK;
}

NS_IMETHODIMP
nsScreenManagerProxy::GetSystemDefaultScale(float *aSystemDefaultScale)
{
  if (!EnsureCacheIsValid()) {
    return NS_ERROR_FAILURE;
  }

  *aSystemDefaultScale = mSystemDefaultScale;
  return NS_OK;
}

bool
nsScreenManagerProxy::EnsureCacheIsValid()
{
  if (mCacheValid) {
    return true;
  }

  bool success = false;
  // Kick off a synchronous IPC call to the parent to get the
  // most up-to-date information.
  unused << SendRefresh(&mNumberOfScreens, &mSystemDefaultScale, &success);
  if (!success) {
    NS_WARNING("Refreshing nsScreenManagerProxy failed in the parent process.");
    return false;
  }

  mCacheValid = true;

  InvalidateCacheOnNextTick();
  return true;
}

void
nsScreenManagerProxy::InvalidateCacheOnNextTick()
{
  if (mCacheWillInvalidate) {
    return;
  }

  mCacheWillInvalidate = true;

  nsCOMPtr<nsIAppShell> appShell = do_GetService(kAppShellCID);
  if (appShell) {
    appShell->RunInStableState(
      NS_NewRunnableMethod(this, &nsScreenManagerProxy::InvalidateCache)
    );
  } else {
    // It's pretty bad news if we can't get the appshell. In that case,
    // let's just invalidate the cache right away.
    InvalidateCache();
  }
}

void
nsScreenManagerProxy::InvalidateCache()
{
  mCacheValid = false;
  mCacheWillInvalidate = false;

  if (mPrimaryScreen) {
    mPrimaryScreen = nullptr;
  }
  for (int32_t i = mScreenCache.Length() - 1; i >= 0; --i) {
    mScreenCache.RemoveElementAt(i);
  }
}

