/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_TRRServiceChild_h
#define mozilla_net_TRRServiceChild_h

#include "mozilla/net/PTRRServiceChild.h"
#include "nsIObserver.h"
#include "nsWeakReference.h"

namespace mozilla {

namespace ipc {
class FileDescriptor;
}  // namespace ipc

namespace net {

class TRRServiceChild : public PTRRServiceChild,
                        public nsIObserver,
                        public nsSupportsWeakReference {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  TRRServiceChild();
  static TRRServiceChild* GetSingleton();

  void Init(const bool& aCaptiveIsPassed, const bool& aParentalControlEnabled,
            nsTArray<nsCString>&& aDNSSuffixList);
  mozilla::ipc::IPCResult RecvNotifyObserver(const nsCString& aTopic,
                                             const nsString& aData);
  mozilla::ipc::IPCResult RecvUpdatePlatformDNSInformation(
      nsTArray<nsCString>&& aDNSSuffixList);
  mozilla::ipc::IPCResult RecvUpdateParentalControlEnabled(
      const bool& aEnabled);
  mozilla::ipc::IPCResult RecvClearDNSCache(const bool& aTrrToo);
  mozilla::ipc::IPCResult RecvSetDetectedTrrURI(const nsCString& aURI);
  mozilla::ipc::IPCResult RecvSetDefaultTRRConnectionInfo(
      Maybe<HttpConnectionInfoCloneArgs>&& aArgs);
  mozilla::ipc::IPCResult RecvUpdateEtcHosts(nsTArray<nsCString>&& aHosts);

 private:
  virtual ~TRRServiceChild();
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_TRRServiceChild_h
