/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProxyAutoConfigChild.h"

#include "mozilla/ipc/Endpoint.h"
#include "mozilla/net/SocketProcessChild.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsThreadUtils.h"
#include "ProxyAutoConfig.h"

namespace mozilla::net {

static bool sThreadLocalSetup = false;
static uint32_t sThreadLocalIndex = 0xdeadbeef;
StaticRefPtr<nsIThread> ProxyAutoConfigChild::sPACThread;
bool ProxyAutoConfigChild::sShutdownObserverRegistered = false;
Atomic<uint32_t> ProxyAutoConfigChild::sLiveActorCount(0);

namespace {

class ShutdownObserver final : public nsIObserver {
 public:
  ShutdownObserver() = default;

  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

 private:
  ~ShutdownObserver() = default;
};

NS_IMPL_ISUPPORTS(ShutdownObserver, nsIObserver)

NS_IMETHODIMP
ShutdownObserver::Observe(nsISupports* aSubject, const char* aTopic,
                          const char16_t* aData) {
  ProxyAutoConfigChild::ShutdownPACThread();
  return NS_OK;
}

}  // namespace

// static
bool ProxyAutoConfigChild::Create(Endpoint<PProxyAutoConfigChild>&& aEndpoint) {
  if (!sPACThread && !CreatePACThread()) {
    NS_WARNING("Failed to create pac thread!");
    return false;
  }

  if (!sShutdownObserverRegistered) {
    nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
    if (NS_WARN_IF(!obs)) {
      return false;
    }
    nsCOMPtr<nsIObserver> observer = new ShutdownObserver();
    nsresult rv = obs->AddObserver(observer, "xpcom-shutdown-threads", false);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return false;
    }
    sShutdownObserverRegistered = true;
  }

  RefPtr<ProxyAutoConfigChild> actor = new ProxyAutoConfigChild();
  if (NS_FAILED(sPACThread->Dispatch(
          NS_NewRunnableFunction("ProxyAutoConfigChild::ProxyAutoConfigChild",
                                 [actor = std::move(actor),
                                  endpoint = std::move(aEndpoint)]() mutable {
                                   MOZ_ASSERT(endpoint.IsValid());

                                   // Transfer ownership to PAC thread. If
                                   // Bind() fails then we will release this
                                   // reference in Destroy.
                                   ProxyAutoConfigChild* actorTmp;
                                   actor.forget(&actorTmp);

                                   if (!endpoint.Bind(actorTmp)) {
                                     actorTmp->Destroy();
                                   }
                                 })))) {
    NS_WARNING("Failed to dispatch runnable!");
    return false;
  }

  return true;
}

// static
bool ProxyAutoConfigChild::CreatePACThread() {
  MOZ_ASSERT(NS_IsMainThread());

  if (SocketProcessChild::GetSingleton()->IsShuttingDown()) {
    NS_WARNING("Trying to create pac thread after shutdown has already begun!");
    return false;
  }

  nsCOMPtr<nsIThread> thread;
  if (NS_FAILED(NS_NewNamedThread("ProxyResolution", getter_AddRefs(thread)))) {
    NS_WARNING("NS_NewNamedThread failed!");
    return false;
  }

  sPACThread = thread.forget();
  return true;
}

// static
void ProxyAutoConfigChild::ShutdownPACThread() {
  MOZ_ASSERT(NS_IsMainThread());

  if (sPACThread) {
    // Wait until all actos are released.
    SpinEventLoopUntil("ProxyAutoConfigChild::ShutdownPACThread"_ns,
                       [&]() { return !sLiveActorCount; });

    nsCOMPtr<nsIThread> thread = sPACThread.get();
    sPACThread = nullptr;
    MOZ_ALWAYS_SUCCEEDS(thread->Shutdown());
  }
}

ProxyAutoConfigChild::ProxyAutoConfigChild()
    : mPAC(MakeUnique<ProxyAutoConfig>()) {
  if (!sThreadLocalSetup) {
    sThreadLocalSetup = true;
    PR_NewThreadPrivateIndex(&sThreadLocalIndex, nullptr);
  }

  mPAC->SetThreadLocalIndex(sThreadLocalIndex);
  sLiveActorCount++;
}

ProxyAutoConfigChild::~ProxyAutoConfigChild() {
  MOZ_ASSERT(NS_IsMainThread());
  sLiveActorCount--;
}

mozilla::ipc::IPCResult ProxyAutoConfigChild::RecvConfigurePAC(
    const nsCString& aPACURI, const nsCString& aPACScriptData,
    const bool& aIncludePath, const uint32_t& aExtraHeapSize) {
  mPAC->ConfigurePAC(aPACURI, aPACScriptData, aIncludePath, aExtraHeapSize,
                     GetMainThreadSerialEventTarget());
  return IPC_OK();
}

mozilla::ipc::IPCResult ProxyAutoConfigChild::RecvGetProxyForURI(
    const nsCString& aTestURI, const nsCString& aTestHost,
    GetProxyForURIResolver&& aResolver) {
  RefPtr<ProxyAutoConfigChild> self = this;
  auto callResolver = [self, testURI(aTestURI), testHost(aTestHost),
                       resolver{std::move(aResolver)}]() {
    if (!self->CanSend()) {
      return;
    }

    nsCString result;
    nsresult rv = self->mPAC->GetProxyForURI(testURI, testHost, result);
    resolver(Tuple<const nsresult&, const nsCString&>(rv, result));
  };
  if (mPAC->WaitingForDNSResolve()) {
    mPAC->RegisterDNSResolveCallback(
        [resolverCallback{std::move(callResolver)}]() { resolverCallback(); });
    return IPC_OK();
  }

  callResolver();
  return IPC_OK();
}

mozilla::ipc::IPCResult ProxyAutoConfigChild::RecvGC() {
  mPAC->GC();
  return IPC_OK();
}

void ProxyAutoConfigChild::ActorDestroy(ActorDestroyReason aWhy) {
  mPAC->Shutdown();

  // To avoid racing with the main thread, we need to dispatch
  // ProxyAutoConfigChild::Destroy again.
  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToCurrentThread(NewNonOwningRunnableMethod(
      "ProxyAutoConfigChild::Destroy", this, &ProxyAutoConfigChild::Destroy)));
}

void ProxyAutoConfigChild::Destroy() {
  // May be called on any thread!
  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(NewNonOwningRunnableMethod(
      "ProxyAutoConfigChild::MainThreadActorDestroy", this,
      &ProxyAutoConfigChild::MainThreadActorDestroy)));
}

void ProxyAutoConfigChild::MainThreadActorDestroy() {
  MOZ_ASSERT(NS_IsMainThread());

  // This may be the last reference!
  Release();
}

}  // namespace mozilla::net
