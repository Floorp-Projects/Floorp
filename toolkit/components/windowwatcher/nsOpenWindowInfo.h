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

class nsOpenWindowInfo : public nsIOpenWindowInfo {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOPENWINDOWINFO

  bool mForceNoOpener = false;
  bool mIsRemote = false;
  RefPtr<BrowserParent> mNextRemoteBrowser;
  OriginAttributes mOriginAttributes;
  RefPtr<BrowsingContext> mParent;

 private:
  virtual ~nsOpenWindowInfo() = default;
};

#endif  // nsOpenWindowInfo_h
