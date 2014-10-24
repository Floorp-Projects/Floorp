/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsScreenManagerProxy_h
#define nsScreenManagerProxy_h

#include "nsIScreenManager.h"
#include "mozilla/dom/PScreenManagerChild.h"
#include "mozilla/dom/TabChild.h"
#include "ScreenProxy.h"

/**
 * The nsScreenManagerProxy is used by the content process to get
 * information about system screens. It uses the PScreenManager protocol
 * to communicate with a PScreenManagerParent to get this information,
 * and also caches the information it gets back.
 *
 * We cache both the system screen information that nsIScreenManagers
 * provide, as well as the nsIScreens that callers can query for.
 *
 * Both of these caches are invalidated on the next tick of the event
 * loop.
 */
class nsScreenManagerProxy MOZ_FINAL : public nsIScreenManager,
                                       public mozilla::dom::PScreenManagerChild
{
public:
  nsScreenManagerProxy();

  NS_DECL_ISUPPORTS
  NS_DECL_NSISCREENMANAGER

private:
  ~nsScreenManagerProxy() {};

  bool EnsureCacheIsValid();
  void InvalidateCacheOnNextTick();
  void InvalidateCache();

  uint32_t mNumberOfScreens;
  float mSystemDefaultScale;
  bool mCacheValid;
  bool mCacheWillInvalidate;

  nsRefPtr<mozilla::widget::ScreenProxy> mPrimaryScreen;

  // nsScreenManagerProxy caches the results to repeated calls to
  // ScreenForNativeWidget, which can be triggered indirectly by
  // web content using the DOM Screen API. This allows us to bypass
  // a lot of IPC traffic.
  //
  // The cache stores ScreenProxy's mapped to the TabChild that
  // asked for the ScreenForNativeWidget was called with via
  // ScreenCacheEntry's. The cache is cleared on the next tick of
  // the event loop.
  struct ScreenCacheEntry
  {
    nsRefPtr<mozilla::widget::ScreenProxy> mScreenProxy;
    nsRefPtr<mozilla::dom::TabChild> mTabChild;
  };

  nsTArray<ScreenCacheEntry> mScreenCache;
};

#endif // nsScreenManagerProxy_h
