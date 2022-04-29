/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef HttpConnectionMgrParent_h__
#define HttpConnectionMgrParent_h__

#include "HttpConnectionMgrShell.h"
#include "mozilla/net/PHttpConnectionMgrParent.h"
#include "mozilla/StaticMutex.h"

namespace mozilla::net {

// HttpConnectionMgrParent plays the role of nsHttpConnectionMgr and delegates
// the work to the nsHttpConnectionMgr in socket process.
class HttpConnectionMgrParent final : public PHttpConnectionMgrParent,
                                      public HttpConnectionMgrShell {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_HTTPCONNECTIONMGRSHELL

  explicit HttpConnectionMgrParent() = default;

  static uint32_t AddHttpUpgradeListenerToMap(
      nsIHttpUpgradeListener* aListener);
  static void RemoveHttpUpgradeListenerFromMap(uint32_t aId);
  static Maybe<nsCOMPtr<nsIHttpUpgradeListener>>
  GetAndRemoveHttpUpgradeListener(uint32_t aId);

 private:
  virtual ~HttpConnectionMgrParent() = default;

  bool mShutDown{false};
  static uint32_t sListenerId;
  static StaticMutex sLock MOZ_UNANNOTATED;
  static nsTHashMap<uint32_t, nsCOMPtr<nsIHttpUpgradeListener>>
      sHttpUpgradeListenerMap;
};

}  // namespace mozilla::net

#endif  // HttpConnectionMgrParent_h__
