/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set expandtab ts=4 sw=4 sts=4 cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AppProtocolHandler_
#define AppProtocolHandler_

#include "nsIProtocolHandler.h"
#include "nsClassHashtable.h"
#include "mozilla/dom/AppInfoBinding.h"

class AppProtocolHandler : public nsIProtocolHandler
{
public:
  NS_DECL_ISUPPORTS

  // nsIProtocolHandler methods:
  NS_DECL_NSIPROTOCOLHANDLER

  // AppProtocolHandler methods:
  AppProtocolHandler();

  // Define a Create method to be used with a factory:
  static nsresult Create(nsISupports* aOuter,
                         const nsIID& aIID,
                         void* *aResult);

private:
  virtual ~AppProtocolHandler();
  nsClassHashtable<nsCStringHashKey, mozilla::dom::AppInfo> mAppInfoCache;
};

#endif /* AppProtocolHandler_ */
