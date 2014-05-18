/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_NullHttpTransaction_h
#define mozilla_net_NullHttpTransaction_h

#include "nsAHttpTransaction.h"
#include "mozilla/Attributes.h"

// This is the minimal nsAHttpTransaction implementation. A NullHttpTransaction
// can be used to drive connection level semantics (such as SSL handshakes
// tunnels) so that a nsHttpConnection becomes fully established in
// anticipation of a real transaction needing to use it soon.

namespace mozilla { namespace net {

class nsAHttpConnection;
class nsHttpConnectionInfo;
class nsHttpRequestHead;

// 6c445340-3b82-4345-8efa-4902c3b8805a
#define NS_NULLHTTPTRANSACTION_IID \
{ 0x6c445340, 0x3b82, 0x4345, {0x8e, 0xfa, 0x49, 0x02, 0xc3, 0xb8, 0x80, 0x5a }}

class NullHttpTransaction : public nsAHttpTransaction
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_NULLHTTPTRANSACTION_IID)
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSAHTTPTRANSACTION

  NullHttpTransaction(nsHttpConnectionInfo *ci,
                      nsIInterfaceRequestor *callbacks,
                      uint32_t caps);
  virtual ~NullHttpTransaction();

  // Overload of nsAHttpTransaction methods
  bool IsNullTransaction() MOZ_OVERRIDE MOZ_FINAL { return true; }
  bool ResponseTimeoutEnabled() const MOZ_OVERRIDE MOZ_FINAL {return true; }
  PRIntervalTime ResponseTimeout() MOZ_OVERRIDE MOZ_FINAL
  {
    return PR_SecondsToInterval(15);
  }

private:

  nsresult mStatus;
  uint32_t mCaps;
  // mCapsToClear holds flags that should be cleared in mCaps, e.g. unset
  // NS_HTTP_REFRESH_DNS when DNS refresh request has completed to avoid
  // redundant requests on the network. To deal with raciness, only unsetting
  // bitfields should be allowed: 'lost races' will thus err on the
  // conservative side, e.g. by going ahead with a 2nd DNS refresh.
  uint32_t mCapsToClear;
  nsRefPtr<nsAHttpConnection> mConnection;
  nsCOMPtr<nsIInterfaceRequestor> mCallbacks;
  nsRefPtr<nsHttpConnectionInfo> mConnectionInfo;
  nsHttpRequestHead *mRequestHead;
  bool mIsDone;
};

NS_DEFINE_STATIC_IID_ACCESSOR(NullHttpTransaction, NS_NULLHTTPTRANSACTION_IID)

}} // namespace mozilla::net

#endif // mozilla_net_NullHttpTransaction_h
