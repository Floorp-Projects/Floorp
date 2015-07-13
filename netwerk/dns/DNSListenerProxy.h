/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DNSListenerProxy_h__
#define DNSListenerProxy_h__

#include "nsIDNSListener.h"
#include "nsIDNSRecord.h"
#include "nsProxyRelease.h"
#include "nsThreadUtils.h"

class nsIEventTarget;
class nsICancelable;

namespace mozilla {
namespace net {

class DNSListenerProxy final
    : public nsIDNSListener
    , public nsIDNSListenerProxy
{
public:
  DNSListenerProxy(nsIDNSListener* aListener, nsIEventTarget* aTargetThread)
    // Sometimes aListener is a main-thread only object like XPCWrappedJS, and
    // sometimes it's a threadsafe object like nsSOCKSSocketInfo. Use a main-
    // thread pointer holder, but disable strict enforcement of thread invariants.
    // The AddRef implementation of XPCWrappedJS will assert if we go wrong here.
    : mListener(new nsMainThreadPtrHolder<nsIDNSListener>(aListener, false))
    , mTargetThread(aTargetThread)
  { }

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIDNSLISTENER
  NS_DECL_NSIDNSLISTENERPROXY

  class OnLookupCompleteRunnable : public nsRunnable
  {
  public:
    OnLookupCompleteRunnable(const nsMainThreadPtrHandle<nsIDNSListener>& aListener,
                             nsICancelable* aRequest,
                             nsIDNSRecord* aRecord,
                             nsresult aStatus)
      : mListener(aListener)
      , mRequest(aRequest)
      , mRecord(aRecord)
      , mStatus(aStatus)
    { }

    NS_DECL_NSIRUNNABLE

  private:
    nsMainThreadPtrHandle<nsIDNSListener> mListener;
    nsCOMPtr<nsICancelable> mRequest;
    nsCOMPtr<nsIDNSRecord> mRecord;
    nsresult mStatus;
  };

private:
  ~DNSListenerProxy() {}

  nsMainThreadPtrHandle<nsIDNSListener> mListener;
  nsCOMPtr<nsIEventTarget> mTargetThread;
};


} // namespace net
} // namespace mozilla

#endif // DNSListenerProxy_h__
