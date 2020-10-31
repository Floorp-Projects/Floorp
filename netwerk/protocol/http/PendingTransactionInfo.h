/* vim:t ts=4 sw=2 sts=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PendingTransactionInfo_h__
#define PendingTransactionInfo_h__

#include "HalfOpenSocket.h"

namespace mozilla {
namespace net {

class PendingTransactionInfo final : public ARefBase {
 public:
  explicit PendingTransactionInfo(nsHttpTransaction* trans)
      : mTransaction(trans) {}

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(PendingTransactionInfo, override)

  void PrintDiagnostics(nsCString& log);

  // Return true if the transaction has claimed a HalfOpen socket or
  // a connection in TLS handshake phase.
  bool IsAlreadyClaimedInitializingConn();

  void AbandonHalfOpenAndForgetActiveConn();

  // Try to claim a halfOpen socket. We can only claim it if it is not
  // claimed yet.
  bool TryClaimingHalfOpen(HalfOpenSocket* sock);
  // Similar as above, but for a ActiveConn that is performing a TLS handshake
  // and has only a NullTransaction associated.
  bool TryClaimingActiveConn(HttpConnectionBase* conn);
  // It is similar as above, but in tihs case the halfOpen is made for this
  // PendingTransactionInfo and it is already claimed.
  void AddHalfOpen(HalfOpenSocket* sock);

  nsHttpTransaction* Transaction() const { return mTransaction; }

 private:
  RefPtr<nsHttpTransaction> mTransaction;
  nsWeakPtr mHalfOpen;
  nsWeakPtr mActiveConn;

  ~PendingTransactionInfo();
};

}  // namespace net
}  // namespace mozilla

#endif  // !PendingTransactionInfo_h__
