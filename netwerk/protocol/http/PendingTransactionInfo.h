/* vim:t ts=4 sw=2 sts=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PendingTransactionInfo_h__
#define PendingTransactionInfo_h__

namespace mozilla {
namespace net {

class PendingTransactionInfo final : public ARefBase {
 public:
  explicit PendingTransactionInfo(nsHttpTransaction* trans)
      : mTransaction(trans) {}

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(PendingTransactionInfo, override)

  void PrintDiagnostics(nsCString& log);

 public:  // meant to be public.
  RefPtr<nsHttpTransaction> mTransaction;
  nsWeakPtr mHalfOpen;
  nsWeakPtr mActiveConn;

 private:
  virtual ~PendingTransactionInfo() = default;
};

}  // namespace net
}  // namespace mozilla

#endif  // !PendingTransactionInfo_h__
