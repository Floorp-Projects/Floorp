/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_StunAddrsRequestParent_h
#define mozilla_net_StunAddrsRequestParent_h

#include "mozilla/net/PStunAddrsRequestParent.h"

#include "nsICancelable.h"
#include "nsIDNSServiceDiscovery.h"

struct MDNSService;

namespace mozilla {
namespace net {

class StunAddrsRequestParent : public PStunAddrsRequestParent {
  friend class PStunAddrsRequestParent;

 public:
  StunAddrsRequestParent();

  NS_IMETHOD_(MozExternalRefCountType) AddRef();
  NS_IMETHOD_(MozExternalRefCountType) Release();

  mozilla::ipc::IPCResult Recv__delete__() override;

 protected:
  virtual ~StunAddrsRequestParent();

  virtual mozilla::ipc::IPCResult RecvGetStunAddrs() override;
  virtual mozilla::ipc::IPCResult RecvRegisterMDNSHostname(
      const nsCString& hostname, const nsCString& address) override;
  virtual mozilla::ipc::IPCResult RecvUnregisterMDNSHostname(
      const nsCString& hostname) override;
  virtual void ActorDestroy(ActorDestroyReason why) override;

  nsCOMPtr<nsIThread> mMainThread;
  nsCOMPtr<nsIEventTarget> mSTSThread;

  void GetStunAddrs_s();
  void SendStunAddrs_m(const NrIceStunAddrArray& addrs);

  ThreadSafeAutoRefCnt mRefCnt;
  NS_DECL_OWNINGTHREAD

 private:
  bool mIPCClosed;  // true if IPDL channel has been closed (child crash)

  class MDNSServiceWrapper {
   public:
    void RegisterHostname(const char* hostname, const char* address);
    void UnregisterHostname(const char* hostname);

    NS_IMETHOD_(MozExternalRefCountType) AddRef();
    NS_IMETHOD_(MozExternalRefCountType) Release();

   protected:
    ThreadSafeAutoRefCnt mRefCnt;
    NS_DECL_OWNINGTHREAD

   private:
    virtual ~MDNSServiceWrapper();
    void StartIfRequired();

    MDNSService* mMDNSService = nullptr;
  };

  static StaticRefPtr<MDNSServiceWrapper> mSharedMDNSService;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_StunAddrsRequestParent_h
