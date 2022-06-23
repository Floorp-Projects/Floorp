/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ContentHandlerService_h
#define ContentHandlerService_h

#include "mozilla/dom/PHandlerService.h"
#include "nsIHandlerService.h"
#include "nsClassHashtable.h"
#include "nsIMIMEInfo.h"

namespace mozilla {

class HandlerServiceChild;

namespace dom {

class PHandlerServiceChild;

class ContentHandlerService : public nsIHandlerService {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIHANDLERSERVICE

  ContentHandlerService();
  [[nodiscard]] nsresult Init();
  static void nsIHandlerInfoToHandlerInfo(nsIHandlerInfo* aInfo,
                                          HandlerInfo* aHandlerInfo);

  static already_AddRefed<nsIHandlerService> Create();

 private:
  virtual ~ContentHandlerService();
  RefPtr<HandlerServiceChild> mHandlerServiceChild;
  nsClassHashtable<nsCStringHashKey, nsCString> mExtToTypeMap;
};

class RemoteHandlerApp : public nsIHandlerApp {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIHANDLERAPP

  explicit RemoteHandlerApp(HandlerApp aAppChild) : mAppChild(aAppChild) {}

 private:
  virtual ~RemoteHandlerApp() {}
  HandlerApp mAppChild;
};

}  // namespace dom
}  // namespace mozilla
#endif
