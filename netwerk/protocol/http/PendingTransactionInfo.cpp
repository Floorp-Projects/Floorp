/* vim:t ts=4 sw=2 sts=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "PendingTransactionInfo.h"
#include "NullHttpTransaction.h"

// Log on level :5, instead of default :4.
#undef LOG
#define LOG(args) LOG5(args)
#undef LOG_ENABLED
#define LOG_ENABLED() LOG5_ENABLED()

namespace mozilla {
namespace net {

PendingTransactionInfo::~PendingTransactionInfo() {
  if (mDnsAndSock) {
    RefPtr<DnsAndConnectSocket> dnsAndSock = do_QueryReferent(mDnsAndSock);
    LOG(
        ("PendingTransactionInfo::PendingTransactionInfo "
         "[trans=%p halfOpen=%p]",
         mTransaction.get(), dnsAndSock.get()));
    if (dnsAndSock) {
      dnsAndSock->Unclaim();
    }
    mDnsAndSock = nullptr;
  } else if (mActiveConn) {
    RefPtr<HttpConnectionBase> activeConn = do_QueryReferent(mActiveConn);
    if (activeConn && activeConn->Transaction() &&
        activeConn->Transaction()->IsNullTransaction()) {
      NullHttpTransaction* nullTrans =
          activeConn->Transaction()->QueryNullTransaction();
      nullTrans->Unclaim();
      LOG((
          "PendingTransactionInfo::PendingTransactionInfo - mark %p unclaimed.",
          activeConn.get()));
    }
  }
}

bool PendingTransactionInfo::IsAlreadyClaimedInitializingConn() {
  LOG(
      ("PendingTransactionInfo::IsAlreadyClaimedInitializingConn "
       "[trans=%p, halfOpen=%p, activeConn=%p]\n",
       mTransaction.get(), mDnsAndSock.get(), mActiveConn.get()));

  // When this transaction has already established a half-open
  // connection, we want to prevent any duplicate half-open
  // connections from being established and bound to this
  // transaction. Allow only use of an idle persistent connection
  // (if found) for transactions referred by a half-open connection.
  bool alreadyDnsAndSockOrWaitingForTLS = false;
  if (mDnsAndSock) {
    MOZ_ASSERT(!mActiveConn);
    RefPtr<DnsAndConnectSocket> dnsAndSock = do_QueryReferent(mDnsAndSock);
    LOG(
        ("PendingTransactionInfo::IsAlreadyClaimedInitializingConn "
         "[trans=%p, dnsAndSock=%p]\n",
         mTransaction.get(), dnsAndSock.get()));
    if (dnsAndSock) {
      alreadyDnsAndSockOrWaitingForTLS = true;
    } else {
      // If we have not found the halfOpen socket, remove the pointer.
      mDnsAndSock = nullptr;
    }
  } else if (mActiveConn) {
    MOZ_ASSERT(!mDnsAndSock);
    RefPtr<HttpConnectionBase> activeConn = do_QueryReferent(mActiveConn);
    LOG(
        ("PendingTransactionInfo::IsAlreadyClaimedInitializingConn "
         "[trans=%p, activeConn=%p]\n",
         mTransaction.get(), activeConn.get()));
    // Check if this transaction claimed a connection that is still
    // performing tls handshake with a NullHttpTransaction or it is between
    // finishing tls and reclaiming (When nullTrans finishes tls handshake,
    // httpConnection does not have a transaction any more and a
    // ReclaimConnection is dispatched). But if an error occurred the
    // connection will be closed, it will exist but CanReused will be
    // false.
    if (activeConn &&
        ((activeConn->Transaction() &&
          activeConn->Transaction()->IsNullTransaction()) ||
         (!activeConn->Transaction() && activeConn->CanReuse()))) {
      alreadyDnsAndSockOrWaitingForTLS = true;
    } else {
      // If we have not found the connection, remove the pointer.
      mActiveConn = nullptr;
    }
  }

  return alreadyDnsAndSockOrWaitingForTLS;
}

nsWeakPtr PendingTransactionInfo::ForgetDnsAndConnectSocketAndActiveConn() {
  nsWeakPtr dnsAndSock = mDnsAndSock;

  mDnsAndSock = nullptr;
  mActiveConn = nullptr;
  return dnsAndSock;
}

void PendingTransactionInfo::RememberDnsAndConnectSocket(
    DnsAndConnectSocket* sock) {
  mDnsAndSock =
      do_GetWeakReference(static_cast<nsISupportsWeakReference*>(sock));
}

bool PendingTransactionInfo::TryClaimingActiveConn(HttpConnectionBase* conn) {
  nsAHttpTransaction* activeTrans = conn->Transaction();
  NullHttpTransaction* nullTrans =
      activeTrans ? activeTrans->QueryNullTransaction() : nullptr;
  if (nullTrans && nullTrans->Claim()) {
    mActiveConn =
        do_GetWeakReference(static_cast<nsISupportsWeakReference*>(conn));
    return true;
  }
  return false;
}

void PendingTransactionInfo::AddDnsAndConnectSocket(DnsAndConnectSocket* sock) {
  mDnsAndSock =
      do_GetWeakReference(static_cast<nsISupportsWeakReference*>(sock));
}

}  // namespace net
}  // namespace mozilla
