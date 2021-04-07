/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "HttpConnectionMgrParent.h"
#include "AltSvcTransactionParent.h"
#include "mozilla/net/HttpTransactionParent.h"
#include "nsHttpConnectionInfo.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsISpeculativeConnect.h"
#include "nsIOService.h"
#include "nsQueryObject.h"

namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS0(HttpConnectionMgrParent)

HttpConnectionMgrParent::HttpConnectionMgrParent() : mShutDown(false) {}

nsresult HttpConnectionMgrParent::Init(
    uint16_t maxUrgentExcessiveConns, uint16_t maxConnections,
    uint16_t maxPersistentConnectionsPerHost,
    uint16_t maxPersistentConnectionsPerProxy, uint16_t maxRequestDelay,
    bool throttleEnabled, uint32_t throttleVersion, uint32_t throttleSuspendFor,
    uint32_t throttleResumeFor, uint32_t throttleReadLimit,
    uint32_t throttleReadInterval, uint32_t throttleHoldTime,
    uint32_t throttleMaxTime, bool beConservativeForProxy) {
  // We don't have to do anything here. nsHttpConnectionMgr in socket process is
  // initialized by nsHttpHandler.
  return NS_OK;
}

nsresult HttpConnectionMgrParent::Shutdown() {
  if (mShutDown) {
    return NS_OK;
  }

  if (!CanSend()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  mShutDown = true;
  Unused << Send__delete__(this);
  return NS_OK;
}

nsresult HttpConnectionMgrParent::UpdateRequestTokenBucket(
    EventTokenBucket* aBucket) {
  // We don't have to do anything here. UpdateRequestTokenBucket() will be
  // triggered by pref change in socket process.
  return NS_OK;
}

nsresult HttpConnectionMgrParent::DoShiftReloadConnectionCleanup() {
  // Do nothing here. DoShiftReloadConnectionCleanup() will be triggered by
  // observer notification or pref change in socket process.
  return NS_OK;
}

nsresult HttpConnectionMgrParent::DoShiftReloadConnectionCleanupWithConnInfo(
    nsHttpConnectionInfo* aCi) {
  if (!aCi) {
    return NS_ERROR_INVALID_ARG;
  }

  HttpConnectionInfoCloneArgs connInfoArgs;
  nsHttpConnectionInfo::SerializeHttpConnectionInfo(aCi, connInfoArgs);

  RefPtr<HttpConnectionMgrParent> self = this;
  auto task = [self, connInfoArgs{std::move(connInfoArgs)}]() {
    Unused << self->SendDoShiftReloadConnectionCleanupWithConnInfo(
        connInfoArgs);
  };
  gIOService->CallOrWaitForSocketProcess(std::move(task));
  return NS_OK;
}

nsresult HttpConnectionMgrParent::PruneDeadConnections() {
  // Do nothing here. PruneDeadConnections() will be triggered by
  // observer notification or pref change in socket process.
  return NS_OK;
}

void HttpConnectionMgrParent::AbortAndCloseAllConnections(int32_t, ARefBase*) {
  // Do nothing here. AbortAndCloseAllConnections() will be triggered by
  // observer notification in socket process.
}

nsresult HttpConnectionMgrParent::UpdateParam(nsParamName name,
                                              uint16_t value) {
  // Do nothing here. UpdateParam() will be triggered by pref change in
  // socket process.
  return NS_OK;
}

void HttpConnectionMgrParent::PrintDiagnostics() {
  // Do nothing here. PrintDiagnostics() will be triggered by pref change in
  // socket process.
}

nsresult HttpConnectionMgrParent::UpdateCurrentTopBrowsingContextId(
    uint64_t aId) {
  RefPtr<HttpConnectionMgrParent> self = this;
  auto task = [self, aId]() {
    Unused << self->SendUpdateCurrentTopBrowsingContextId(aId);
  };
  gIOService->CallOrWaitForSocketProcess(std::move(task));
  return NS_OK;
}

nsresult HttpConnectionMgrParent::AddTransaction(HttpTransactionShell* aTrans,
                                                 int32_t aPriority) {
  MOZ_ASSERT(gIOService->SocketProcessReady());

  if (!CanSend()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  Unused << SendAddTransaction(aTrans->AsHttpTransactionParent(), aPriority);
  return NS_OK;
}

nsresult HttpConnectionMgrParent::AddTransactionWithStickyConn(
    HttpTransactionShell* aTrans, int32_t aPriority,
    HttpTransactionShell* aTransWithStickyConn) {
  MOZ_ASSERT(gIOService->SocketProcessReady());

  if (!CanSend()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  Unused << SendAddTransactionWithStickyConn(
      aTrans->AsHttpTransactionParent(), aPriority,
      aTransWithStickyConn->AsHttpTransactionParent());
  return NS_OK;
}

nsresult HttpConnectionMgrParent::RescheduleTransaction(
    HttpTransactionShell* aTrans, int32_t aPriority) {
  MOZ_ASSERT(gIOService->SocketProcessReady());

  if (!CanSend()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  Unused << SendRescheduleTransaction(aTrans->AsHttpTransactionParent(),
                                      aPriority);
  return NS_OK;
}

void HttpConnectionMgrParent::UpdateClassOfServiceOnTransaction(
    HttpTransactionShell* aTrans, uint32_t aClassOfService) {
  MOZ_ASSERT(gIOService->SocketProcessReady());

  if (!CanSend()) {
    return;
  }

  Unused << SendUpdateClassOfServiceOnTransaction(
      aTrans->AsHttpTransactionParent(), aClassOfService);
}

nsresult HttpConnectionMgrParent::CancelTransaction(
    HttpTransactionShell* aTrans, nsresult aReason) {
  MOZ_ASSERT(gIOService->SocketProcessReady());

  if (!CanSend()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  Unused << SendCancelTransaction(aTrans->AsHttpTransactionParent(), aReason);
  return NS_OK;
}

nsresult HttpConnectionMgrParent::ReclaimConnection(HttpConnectionBase*) {
  MOZ_ASSERT_UNREACHABLE("ReclaimConnection should not be called");
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult HttpConnectionMgrParent::ProcessPendingQ(nsHttpConnectionInfo*) {
  MOZ_ASSERT_UNREACHABLE("ProcessPendingQ should not be called");
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult HttpConnectionMgrParent::ProcessPendingQ() {
  MOZ_ASSERT_UNREACHABLE("ProcessPendingQ should not be called");
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult HttpConnectionMgrParent::GetSocketThreadTarget(nsIEventTarget**) {
  MOZ_ASSERT_UNREACHABLE("GetSocketThreadTarget should not be called");
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult HttpConnectionMgrParent::SpeculativeConnect(
    nsHttpConnectionInfo* aConnInfo, nsIInterfaceRequestor* aCallbacks,
    uint32_t aCaps, SpeculativeTransaction* aTransaction, bool aFetchHTTPSRR) {
  NS_ENSURE_ARG_POINTER(aConnInfo);

  nsCOMPtr<nsISpeculativeConnectionOverrider> overrider =
      do_GetInterface(aCallbacks);
  Maybe<SpeculativeConnectionOverriderArgs> overriderArgs;
  if (overrider) {
    overriderArgs.emplace();
    overriderArgs->parallelSpeculativeConnectLimit() =
        overrider->GetParallelSpeculativeConnectLimit();
    overriderArgs->ignoreIdle() = overrider->GetIgnoreIdle();
    overriderArgs->isFromPredictor() = overrider->GetIsFromPredictor();
    overriderArgs->allow1918() = overrider->GetAllow1918();
  }

  HttpConnectionInfoCloneArgs connInfo;
  nsHttpConnectionInfo::SerializeHttpConnectionInfo(aConnInfo, connInfo);
  RefPtr<AltSvcTransactionParent> trans = do_QueryObject(aTransaction);
  RefPtr<HttpConnectionMgrParent> self = this;
  auto task = [self, connInfo{std::move(connInfo)},
               overriderArgs{std::move(overriderArgs)}, aCaps,
               trans{std::move(trans)}, aFetchHTTPSRR]() {
    Maybe<AltSvcTransactionParent*> maybeTrans;
    if (trans) {
      maybeTrans.emplace(trans.get());
    }
    Unused << self->SendSpeculativeConnect(connInfo, overriderArgs, aCaps,
                                           maybeTrans, aFetchHTTPSRR);
  };

  gIOService->CallOrWaitForSocketProcess(std::move(task));
  return NS_OK;
}

nsresult HttpConnectionMgrParent::VerifyTraffic() {
  // Do nothing here. VerifyTraffic() will be triggered by observer notification
  // in socket process.
  return NS_OK;
}

void HttpConnectionMgrParent::ExcludeHttp2(const nsHttpConnectionInfo* ci) {
  MOZ_ASSERT_UNREACHABLE("ExcludeHttp2 should not be called");
}

void HttpConnectionMgrParent::ExcludeHttp3(const nsHttpConnectionInfo* ci) {
  MOZ_ASSERT_UNREACHABLE("ExcludeHttp3 should not be called");
}

nsresult HttpConnectionMgrParent::ClearConnectionHistory() {
  // Do nothing here. ClearConnectionHistory() will be triggered by
  // observer notification in socket process.
  return NS_OK;
}

nsresult HttpConnectionMgrParent::CompleteUpgrade(
    HttpTransactionShell* aTrans, nsIHttpUpgradeListener* aUpgradeListener) {
  // TODO: fix this in bug 1497249
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsHttpConnectionMgr* HttpConnectionMgrParent::AsHttpConnectionMgr() {
  return nullptr;
}

HttpConnectionMgrParent* HttpConnectionMgrParent::AsHttpConnectionMgrParent() {
  return this;
}

}  // namespace net
}  // namespace mozilla
