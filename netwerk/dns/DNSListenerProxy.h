/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
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

#define DNS_LISTENER_PROXY_IID                       \
  {                                                  \
    0x8f172ca3, 0x7a7f, 0x4941, {                    \
      0xa7, 0x0b, 0xbc, 0x72, 0x80, 0x2e, 0x9d, 0x9b \
    }                                                \
  }

class DNSListenerProxy final : public nsIDNSListener {
 public:
  DNSListenerProxy(nsIDNSListener* aListener, nsIEventTarget* aTargetThread)
      // We want to make sure that |aListener| is only accessed on the target
      // thread.
      : mListener(aListener),
        mTargetThread(aTargetThread),
        mListenerAddress(reinterpret_cast<uintptr_t>(aListener)) {
    MOZ_ASSERT(mListener);
    MOZ_ASSERT(mTargetThread);
  }

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIDNSLISTENER
  NS_DECLARE_STATIC_IID_ACCESSOR(DNS_LISTENER_PROXY_IID)

  uintptr_t GetOriginalListenerAddress() const { return mListenerAddress; }

 private:
  ~DNSListenerProxy() {
    NS_ProxyRelease("DNSListenerProxy::mListener", mTargetThread,
                    mListener.forget());
  }

  nsCOMPtr<nsIDNSListener> mListener;
  nsCOMPtr<nsIEventTarget> mTargetThread;
  uintptr_t mListenerAddress;
};

NS_DEFINE_STATIC_IID_ACCESSOR(DNSListenerProxy, DNS_LISTENER_PROXY_IID)

}  // namespace net
}  // namespace mozilla

#endif  // DNSListenerProxy_h__
