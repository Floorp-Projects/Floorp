/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_NullHttpTransaction_h
#define mozilla_net_NullHttpTransaction_h

#include "nsAHttpTransaction.h"
#include "mozilla/Attributes.h"
#include "TimingStruct.h"

// This is the minimal nsAHttpTransaction implementation. A NullHttpTransaction
// can be used to drive connection level semantics (such as SSL handshakes
// tunnels) so that a nsHttpConnection becomes fully established in
// anticipation of a real transaction needing to use it soon.

class nsIHttpActivityObserver;

namespace mozilla {
namespace net {

class nsAHttpConnection;
class nsHttpConnectionInfo;
class nsHttpRequestHead;

// 6c445340-3b82-4345-8efa-4902c3b8805a
#define NS_NULLHTTPTRANSACTION_IID                   \
  {                                                  \
    0x6c445340, 0x3b82, 0x4345, {                    \
      0x8e, 0xfa, 0x49, 0x02, 0xc3, 0xb8, 0x80, 0x5a \
    }                                                \
  }

class NullHttpTransaction : public nsAHttpTransaction {
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_NULLHTTPTRANSACTION_IID)
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSAHTTPTRANSACTION

  NullHttpTransaction(nsHttpConnectionInfo* ci,
                      nsIInterfaceRequestor* callbacks, uint32_t caps);

  [[nodiscard]] bool Claim();
  void Unclaim();

  // Overload of nsAHttpTransaction methods
  bool IsNullTransaction() final { return true; }
  NullHttpTransaction* QueryNullTransaction() final { return this; }
  bool ResponseTimeoutEnabled() const final { return true; }
  PRIntervalTime ResponseTimeout() final { return PR_SecondsToInterval(15); }

  // We have to override this function because |mTransaction| in
  // nsHalfOpenSocket could be either nsHttpTransaction or NullHttpTransaction.
  // NullHttpTransaction will be activated on the connection immediately after
  // creation and be never put in a pending queue, so it's OK to just return 0.
  uint64_t TopBrowsingContextId() override { return 0; }

  TimingStruct Timings() { return mTimings; }

  mozilla::TimeStamp GetTcpConnectEnd() { return mTimings.tcpConnectEnd; }
  mozilla::TimeStamp GetSecureConnectionStart() {
    return mTimings.secureConnectionStart;
  }

 protected:
  virtual ~NullHttpTransaction();

 private:
  nsresult mStatus;

 protected:
  uint32_t mCaps;
  nsHttpRequestHead* mRequestHead;

 private:
  bool mIsDone;
  bool mClaimed;
  TimingStruct mTimings;

 protected:
  RefPtr<nsAHttpConnection> mConnection;
  nsCOMPtr<nsIInterfaceRequestor> mCallbacks;
  RefPtr<nsHttpConnectionInfo> mConnectionInfo;
  nsCOMPtr<nsIHttpActivityObserver> mActivityDistributor;
};

NS_DEFINE_STATIC_IID_ACCESSOR(NullHttpTransaction, NS_NULLHTTPTRANSACTION_IID)

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_NullHttpTransaction_h
