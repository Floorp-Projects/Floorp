/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsOpenWindowInfo_h
#define nsOpenWindowInfo_h

#include "nsIOpenWindowInfo.h"
#include "nsISupportsImpl.h"
#include "mozilla/OriginAttributes.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/ClientOpenWindowUtils.h"

class nsOpenWindowInfo : public nsIOpenWindowInfo {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOPENWINDOWINFO

  bool mForceNoOpener = false;
  bool mIsRemote = false;
  bool mIsForPrinting = false;
  bool mIsForWindowDotPrint = false;
  bool mIsTopLevelCreatedByWebContent = false;
  RefPtr<mozilla::dom::BrowserParent> mNextRemoteBrowser;
  mozilla::OriginAttributes mOriginAttributes;
  RefPtr<mozilla::dom::BrowsingContext> mParent;
  RefPtr<nsIBrowsingContextReadyCallback> mBrowsingContextReadyCallback;

 private:
  virtual ~nsOpenWindowInfo() = default;
};

class nsBrowsingContextReadyCallback : public nsIBrowsingContextReadyCallback {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIBROWSINGCONTEXTREADYCALLBACK

  explicit nsBrowsingContextReadyCallback(
      RefPtr<mozilla::dom::BrowsingContextCallbackReceivedPromise::Private>
          aPromise);

 private:
  virtual ~nsBrowsingContextReadyCallback();

  RefPtr<mozilla::dom::BrowsingContextCallbackReceivedPromise::Private>
      mPromise;
};

#endif  // nsOpenWindowInfo_h
