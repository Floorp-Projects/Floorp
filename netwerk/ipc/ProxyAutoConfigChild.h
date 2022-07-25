/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ProxyAutoConfigChild_h__
#define ProxyAutoConfigChild_h__

#include "mozilla/LinkedList.h"
#include "mozilla/net/PProxyAutoConfigChild.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/UniquePtr.h"

namespace mozilla {
namespace net {

class ProxyAutoConfig;

class ProxyAutoConfigChild final : public PProxyAutoConfigChild {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ProxyAutoConfigChild)

  static bool Create(Endpoint<PProxyAutoConfigChild>&& aEndpoint);
  static bool CreatePACThread();
  static void ShutdownPACThread();

  ProxyAutoConfigChild();

  void ActorDestroy(ActorDestroyReason aWhy) override;
  mozilla::ipc::IPCResult RecvConfigurePAC(const nsACString& aPACURI,
                                           const nsACString& aPACScriptData,
                                           const bool& aIncludePath,
                                           const uint32_t& aExtraHeapSize);
  mozilla::ipc::IPCResult RecvGetProxyForURI(
      const nsACString& aTestURI, const nsACString& aTestHost,
      GetProxyForURIResolver&& aResolver);

  void Destroy();

 private:
  virtual ~ProxyAutoConfigChild();
  void ProcessPendingQ();
  bool ProcessPending();
  static void BindProxyAutoConfigChild(
      RefPtr<ProxyAutoConfigChild>&& aActor,
      Endpoint<PProxyAutoConfigChild>&& aEndpoint);

  UniquePtr<ProxyAutoConfig> mPAC;
  bool mInProgress{false};
  bool mPACLoaded{false};
  bool mShutdown{false};

  class PendingQuery final : public LinkedListElement<RefPtr<PendingQuery>> {
   public:
    NS_INLINE_DECL_REFCOUNTING(PendingQuery)

    explicit PendingQuery(const nsACString& aTestURI,
                          const nsACString& aTestHost,
                          GetProxyForURIResolver&& aResolver)
        : mURI(aTestURI), mHost(aTestHost), mResolver(std::move(aResolver)) {}

    void Resolve(nsresult aStatus, const nsACString& aResult);
    const nsCString& URI() const { return mURI; }
    const nsCString& Host() const { return mHost; }

   private:
    ~PendingQuery() = default;

    nsCString mURI;
    nsCString mHost;
    GetProxyForURIResolver mResolver;
  };

  LinkedList<RefPtr<PendingQuery>> mPendingQ;

  static StaticRefPtr<nsIThread> sPACThread;
  static bool sShutdownObserverRegistered;
  static Atomic<uint32_t> sLiveActorCount;
};

}  // namespace net
}  // namespace mozilla

#endif  // ProxyAutoConfigChild_h__
