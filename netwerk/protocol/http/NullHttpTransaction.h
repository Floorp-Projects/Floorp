/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_NullHttpTransaction_h
#define mozilla_net_NullHttpTransaction_h

#include "nsAHttpTransaction.h"
#include "nsAHttpConnection.h"
#include "nsIInterfaceRequestor.h"
#include "nsHttpConnectionInfo.h"
#include "nsHttpRequestHead.h"
#include "mozilla/Attributes.h"

// This is the minimal nsAHttpTransaction implementation. A NullHttpTransaction
// can be used to drive connection level semantics (such as SSL handshakes
// tunnels) so that a nsHttpConnection becomes fully established in
// anticipation of a real transaction needing to use it soon.

namespace mozilla { namespace net {

class NullHttpTransaction MOZ_FINAL : public nsAHttpTransaction
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSAHTTPTRANSACTION

  NullHttpTransaction(nsHttpConnectionInfo *ci,
                      nsIInterfaceRequestor *callbacks,
                      uint32_t caps);
  ~NullHttpTransaction();

  nsHttpConnectionInfo *ConnectionInfo() { return mConnectionInfo; }

  // An overload of nsAHttpTransaction::IsNullTransaction()
  bool IsNullTransaction() { return true; }

private:

  nsresult mStatus;
  uint32_t mCaps;
  nsRefPtr<nsAHttpConnection> mConnection;
  nsCOMPtr<nsIInterfaceRequestor> mCallbacks;
  nsRefPtr<nsHttpConnectionInfo> mConnectionInfo;
  nsHttpRequestHead *mRequestHead;
  bool mIsDone;
};

}} // namespace mozilla::net

#endif // mozilla_net_NullHttpTransaction_h
