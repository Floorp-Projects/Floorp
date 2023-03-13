/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef HttpConnectionMgrChild_h__
#define HttpConnectionMgrChild_h__

#include "mozilla/net/PHttpConnectionMgrChild.h"
#include "mozilla/RefPtr.h"

namespace mozilla::net {

class nsHttpConnectionMgr;

class HttpConnectionMgrChild final : public PHttpConnectionMgrChild {
 public:
  NS_INLINE_DECL_REFCOUNTING(HttpConnectionMgrChild, override)

  explicit HttpConnectionMgrChild();
  void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult RecvDoShiftReloadConnectionCleanupWithConnInfo(
      const HttpConnectionInfoCloneArgs& aArgs);
  mozilla::ipc::IPCResult RecvUpdateCurrentBrowserId(const uint64_t& aId);
  mozilla::ipc::IPCResult RecvAddTransaction(PHttpTransactionChild* aTrans,
                                             const int32_t& aPriority);
  mozilla::ipc::IPCResult RecvAddTransactionWithStickyConn(
      PHttpTransactionChild* aTrans, const int32_t& aPriority,
      PHttpTransactionChild* aTransWithStickyConn);
  mozilla::ipc::IPCResult RecvRescheduleTransaction(
      PHttpTransactionChild* aTrans, const int32_t& aPriority);
  mozilla::ipc::IPCResult RecvUpdateClassOfServiceOnTransaction(
      PHttpTransactionChild* aTrans, const ClassOfService& aClassOfService);
  mozilla::ipc::IPCResult RecvCancelTransaction(PHttpTransactionChild* aTrans,
                                                const nsresult& aReason);
  mozilla::ipc::IPCResult RecvSpeculativeConnect(
      const HttpConnectionInfoCloneArgs& aConnInfo,
      Maybe<SpeculativeConnectionOverriderArgs> aOverriderArgs, uint32_t aCaps,
      Maybe<PAltSvcTransactionChild*> aTrans, const bool& aFetchHTTPSRR);
  mozilla::ipc::IPCResult RecvStartWebSocketConnection(
      PHttpTransactionChild* aTransWithStickyConn, uint32_t aListenerId);

 private:
  virtual ~HttpConnectionMgrChild();

  RefPtr<nsHttpConnectionMgr> mConnMgr;
};

}  // namespace mozilla::net

#endif  // HttpConnectionMgrChild_h__
