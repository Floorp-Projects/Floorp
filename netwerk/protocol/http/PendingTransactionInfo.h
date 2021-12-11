/* vim:t ts=4 sw=2 sts=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PendingTransactionInfo_h__
#define PendingTransactionInfo_h__

#include "DnsAndConnectSocket.h"

namespace mozilla {
namespace net {

class PendingTransactionInfo final : public ARefBase {
 public:
  explicit PendingTransactionInfo(nsHttpTransaction* trans)
      : mTransaction(trans) {}

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(PendingTransactionInfo, override)

  void PrintDiagnostics(nsCString& log);

  // Return true if the transaction has claimed a DnsAndConnectSocket or
  // a connection in TLS handshake phase.
  bool IsAlreadyClaimedInitializingConn();

  // This function return a weak poointer to DnsAndConnectSocket.
  // The pointer is used by the caller(ConnectionEntry) to remove the
  // DnsAndConnectSocket from the internal list. PendingTransactionInfo
  // cannot perform this opereation.
  [[nodiscard]] nsWeakPtr ForgetDnsAndConnectSocketAndActiveConn();

  // Remember associated DnsAndConnectSocket.
  void RememberDnsAndConnectSocket(DnsAndConnectSocket* sock);
  // Similar as above, but for a ActiveConn that is performing a TLS handshake
  // and has only a NullTransaction associated.
  bool TryClaimingActiveConn(HttpConnectionBase* conn);
  // It is similar as above, but in tihs case the halfOpen is made for this
  // PendingTransactionInfo and it is already claimed.
  void AddDnsAndConnectSocket(DnsAndConnectSocket* sock);

  nsHttpTransaction* Transaction() const { return mTransaction; }

 private:
  RefPtr<nsHttpTransaction> mTransaction;
  nsWeakPtr mDnsAndSock;
  nsWeakPtr mActiveConn;

  ~PendingTransactionInfo();
};

class PendingComparator {
 public:
  bool Equals(const PendingTransactionInfo* aPendingTrans,
              const nsAHttpTransaction* aTrans) const {
    return aPendingTrans->Transaction() == aTrans;
  }
};

}  // namespace net
}  // namespace mozilla

#endif  // !PendingTransactionInfo_h__
