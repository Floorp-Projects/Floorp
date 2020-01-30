/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "HttpConnectionMgrChild.h"
#include "HttpTransactionChild.h"
#include "EventTokenBucket.h"
#include "nsHttpConnectionInfo.h"
#include "nsHttpConnectionMgr.h"
#include "nsHttpHandler.h"

namespace mozilla {
namespace net {

HttpConnectionMgrChild::HttpConnectionMgrChild()
    : mConnMgr(gHttpHandler->ConnMgr()) {
  MOZ_ASSERT(mConnMgr);
}

HttpConnectionMgrChild::~HttpConnectionMgrChild() {
  LOG(("HttpConnectionMgrChild dtor:%p", this));
}

void HttpConnectionMgrChild::ActorDestroy(ActorDestroyReason aWhy) {
  LOG(("HttpConnectionMgrChild::ActorDestroy [this=%p]\n", this));
}

mozilla::ipc::IPCResult
HttpConnectionMgrChild::RecvDoShiftReloadConnectionCleanup(
    const Maybe<HttpConnectionInfoCloneArgs>& aArgs) {
  nsresult rv;
  if (aArgs) {
    RefPtr<nsHttpConnectionInfo> cinfo =
        nsHttpConnectionInfo::DeserializeHttpConnectionInfoCloneArgs(
            aArgs.ref());
    rv = mConnMgr->DoShiftReloadConnectionCleanup(cinfo);
  } else {
    rv = mConnMgr->DoShiftReloadConnectionCleanup(nullptr);
  }
  if (NS_FAILED(rv)) {
    LOG(
        ("HttpConnectionMgrChild::RecvDoShiftReloadConnectionCleanup failed "
         "(%08x)\n",
         static_cast<uint32_t>(rv)));
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult HttpConnectionMgrChild::RecvPruneDeadConnections() {
  nsresult rv = mConnMgr->PruneDeadConnections();
  if (NS_FAILED(rv)) {
    LOG(("HttpConnectionMgrChild::RecvPruneDeadConnections failed (%08x)\n",
         static_cast<uint32_t>(rv)));
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
HttpConnectionMgrChild::RecvAbortAndCloseAllConnections() {
  mConnMgr->AbortAndCloseAllConnections(0, nullptr);
  return IPC_OK();
}

mozilla::ipc::IPCResult
HttpConnectionMgrChild::RecvUpdateCurrentTopLevelOuterContentWindowId(
    const uint64_t& aWindowId) {
  mConnMgr->UpdateCurrentTopLevelOuterContentWindowId(aWindowId);
  return IPC_OK();
}

nsHttpTransaction* ToRealHttpTransaction(PHttpTransactionChild* aTrans) {
  HttpTransactionChild* transChild = static_cast<HttpTransactionChild*>(aTrans);
  LOG(("ToRealHttpTransaction: [transChild=%p] \n", transChild));
  RefPtr<nsHttpTransaction> trans = transChild->GetHttpTransaction();
  MOZ_ASSERT(trans);
  return trans;
}

mozilla::ipc::IPCResult HttpConnectionMgrChild::RecvAddTransaction(
    PHttpTransactionChild* aTrans, const int32_t& aPriority) {
  Unused << mConnMgr->AddTransaction(ToRealHttpTransaction(aTrans), aPriority);
  return IPC_OK();
}

mozilla::ipc::IPCResult
HttpConnectionMgrChild::RecvAddTransactionWithStickyConn(
    PHttpTransactionChild* aTrans, const int32_t& aPriority,
    PHttpTransactionChild* aTransWithStickyConn) {
  Unused << mConnMgr->AddTransactionWithStickyConn(
      ToRealHttpTransaction(aTrans), aPriority,
      ToRealHttpTransaction(aTransWithStickyConn));
  return IPC_OK();
}

mozilla::ipc::IPCResult HttpConnectionMgrChild::RecvRescheduleTransaction(
    PHttpTransactionChild* aTrans, const int32_t& aPriority) {
  Unused << mConnMgr->RescheduleTransaction(ToRealHttpTransaction(aTrans),
                                            aPriority);
  return IPC_OK();
}

mozilla::ipc::IPCResult
HttpConnectionMgrChild::RecvUpdateClassOfServiceOnTransaction(
    PHttpTransactionChild* aTrans, const uint32_t& aClassOfService) {
  mConnMgr->UpdateClassOfServiceOnTransaction(ToRealHttpTransaction(aTrans),
                                              aClassOfService);
  return IPC_OK();
}

mozilla::ipc::IPCResult HttpConnectionMgrChild::RecvCancelTransaction(
    PHttpTransactionChild* aTrans, const nsresult& aReason) {
  Unused << mConnMgr->CancelTransaction(ToRealHttpTransaction(aTrans), aReason);
  return IPC_OK();
}

mozilla::ipc::IPCResult HttpConnectionMgrChild::RecvVerifyTraffic() {
  nsresult rv = mConnMgr->VerifyTraffic();
  if (NS_FAILED(rv)) {
    LOG(("HttpConnectionMgrChild::RecvVerifyTraffic failed (%08x)\n",
         static_cast<uint32_t>(rv)));
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult HttpConnectionMgrChild::RecvClearConnectionHistory() {
  Unused << mConnMgr->ClearConnectionHistory();
  return IPC_OK();
}

}  // namespace net
}  // namespace mozilla
