/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "HttpConnectionMgrChild.h"
#include "HttpTransactionChild.h"
#include "AltSvcTransactionChild.h"
#include "EventTokenBucket.h"
#include "nsHttpConnectionInfo.h"
#include "nsHttpConnectionMgr.h"
#include "nsHttpHandler.h"
#include "nsISpeculativeConnect.h"

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
HttpConnectionMgrChild::RecvDoShiftReloadConnectionCleanupWithConnInfo(
    const HttpConnectionInfoCloneArgs& aArgs) {
  RefPtr<nsHttpConnectionInfo> cinfo =
      nsHttpConnectionInfo::DeserializeHttpConnectionInfoCloneArgs(aArgs);
  nsresult rv = mConnMgr->DoShiftReloadConnectionCleanupWithConnInfo(cinfo);
  if (NS_FAILED(rv)) {
    LOG(
        ("HttpConnectionMgrChild::DoShiftReloadConnectionCleanupWithConnInfo "
         "failed "
         "(%08x)\n",
         static_cast<uint32_t>(rv)));
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
HttpConnectionMgrChild::RecvUpdateCurrentTopBrowsingContextId(
    const uint64_t& aId) {
  mConnMgr->UpdateCurrentTopBrowsingContextId(aId);
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

namespace {

class SpeculativeConnectionOverrider final
    : public nsIInterfaceRequestor,
      public nsISpeculativeConnectionOverrider {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSISPECULATIVECONNECTIONOVERRIDER

  explicit SpeculativeConnectionOverrider(
      SpeculativeConnectionOverriderArgs&& aArgs)
      : mArgs(std::move(aArgs)) {}

 private:
  virtual ~SpeculativeConnectionOverrider() = default;

  SpeculativeConnectionOverriderArgs mArgs;
};

NS_IMPL_ISUPPORTS(SpeculativeConnectionOverrider, nsIInterfaceRequestor,
                  nsISpeculativeConnectionOverrider)

NS_IMETHODIMP
SpeculativeConnectionOverrider::GetInterface(const nsIID& iid, void** result) {
  if (NS_SUCCEEDED(QueryInterface(iid, result)) && *result) {
    return NS_OK;
  }
  return NS_ERROR_NO_INTERFACE;
}

NS_IMETHODIMP
SpeculativeConnectionOverrider::GetIgnoreIdle(bool* aIgnoreIdle) {
  *aIgnoreIdle = mArgs.ignoreIdle();
  return NS_OK;
}

NS_IMETHODIMP
SpeculativeConnectionOverrider::GetParallelSpeculativeConnectLimit(
    uint32_t* aParallelSpeculativeConnectLimit) {
  *aParallelSpeculativeConnectLimit = mArgs.parallelSpeculativeConnectLimit();
  return NS_OK;
}

NS_IMETHODIMP
SpeculativeConnectionOverrider::GetIsFromPredictor(bool* aIsFromPredictor) {
  *aIsFromPredictor = mArgs.isFromPredictor();
  return NS_OK;
}

NS_IMETHODIMP
SpeculativeConnectionOverrider::GetAllow1918(bool* aAllow) {
  *aAllow = mArgs.allow1918();
  return NS_OK;
}

}  // anonymous namespace

mozilla::ipc::IPCResult HttpConnectionMgrChild::RecvSpeculativeConnect(
    HttpConnectionInfoCloneArgs aConnInfo,
    Maybe<SpeculativeConnectionOverriderArgs> aOverriderArgs, uint32_t aCaps,
    Maybe<PAltSvcTransactionChild*> aTrans, const bool& aFetchHTTPSRR) {
  RefPtr<nsHttpConnectionInfo> cinfo =
      nsHttpConnectionInfo::DeserializeHttpConnectionInfoCloneArgs(aConnInfo);
  nsCOMPtr<nsIInterfaceRequestor> overrider =
      aOverriderArgs
          ? new SpeculativeConnectionOverrider(std::move(aOverriderArgs.ref()))
          : nullptr;
  RefPtr<SpeculativeTransaction> trans;
  if (aTrans) {
    trans = static_cast<AltSvcTransactionChild*>(*aTrans)->CreateTransaction();
  }

  Unused << mConnMgr->SpeculativeConnect(cinfo, overrider, aCaps, trans,
                                         aFetchHTTPSRR);
  return IPC_OK();
}

}  // namespace net
}  // namespace mozilla
