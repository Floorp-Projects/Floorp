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
static StaticRefPtr<ProxyAutoConfigChild> sActor;

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
void ProxyAutoConfigChild::BindProxyAutoConfigChild(
    RefPtr<ProxyAutoConfigChild>&& aActor,
    Endpoint<PProxyAutoConfigChild>&& aEndpoint) {
  // We only allow one ProxyAutoConfigChild at a time, so we need to
  // wait until the old one to be destroyed.
  if (sActor) {
    NS_DispatchToCurrentThread(NS_NewRunnableFunction(
        "BindProxyAutoConfigChild",
        [actor = std::move(aActor), endpoint = std::move(aEndpoint)]() mutable {
          ProxyAutoConfigChild::BindProxyAutoConfigChild(std::move(actor),
                                                         std::move(endpoint));
        }));
    return;
  }

  if (aEndpoint.Bind(aActor)) {
    sActor = aActor;
  }
}

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
  if (NS_FAILED(sPACThread->Dispatch(NS_NewRunnableFunction(
          "ProxyAutoConfigChild::ProxyAutoConfigChild",
          [actor = std::move(actor),
           endpoint = std::move(aEndpoint)]() mutable {
            MOZ_ASSERT(endpoint.IsValid());
            ProxyAutoConfigChild::BindProxyAutoConfigChild(std::move(actor),
                                                           std::move(endpoint));
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
                       [&]() { return !sActor; });

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
}

ProxyAutoConfigChild::~ProxyAutoConfigChild() = default;

mozilla::ipc::IPCResult ProxyAutoConfigChild::RecvConfigurePAC(
    const nsACString& aPACURI, const nsACString& aPACScriptData,
    const bool& aIncludePath, const uint32_t& aExtraHeapSize) {
  mPAC->ConfigurePAC(aPACURI, aPACScriptData, aIncludePath, aExtraHeapSize,
                     GetMainThreadSerialEventTarget());
  mPACLoaded = true;
  NS_DispatchToCurrentThread(
      NewRunnableMethod("ProxyAutoConfigChild::ProcessPendingQ", this,
                        &ProxyAutoConfigChild::ProcessPendingQ));
  return IPC_OK();
}

void ProxyAutoConfigChild::PendingQuery::Resolve(nsresult aStatus,
                                                 const nsACString& aResult) {
  mResolver(Tuple<const nsresult&, const nsACString&>(aStatus, aResult));
}

mozilla::ipc::IPCResult ProxyAutoConfigChild::RecvGetProxyForURI(
    const nsACString& aTestURI, const nsACString& aTestHost,
    GetProxyForURIResolver&& aResolver) {
  mPendingQ.insertBack(
      new PendingQuery(aTestURI, aTestHost, std::move(aResolver)));
  ProcessPendingQ();
  return IPC_OK();
}

void ProxyAutoConfigChild::ProcessPendingQ() {
  while (ProcessPending()) {
    ;
  }

  if (mShutdown) {
    mPAC->Shutdown();
  } else {
    // do GC while the thread has nothing pending
    mPAC->GC();
  }
}

bool ProxyAutoConfigChild::ProcessPending() {
  if (mPendingQ.isEmpty()) {
    return false;
  }

  if (mInProgress || !mPACLoaded) {
    return false;
  }

  if (mShutdown) {
    return true;
  }

  mInProgress = true;
  RefPtr<PendingQuery> query = mPendingQ.popFirst();
  nsCString result;
  nsresult rv = mPAC->GetProxyForURI(query->URI(), query->Host(), result);
  query->Resolve(rv, result);
  mInProgress = false;
  return true;
}

void ProxyAutoConfigChild::ActorDestroy(ActorDestroyReason aWhy) {
  mPendingQ.clear();
  mShutdown = true;
  mPAC->Shutdown();

  // To avoid racing with the main thread, we need to dispatch
  // ProxyAutoConfigChild::Destroy again.
  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToCurrentThread(NewNonOwningRunnableMethod(
      "ProxyAutoConfigChild::Destroy", this, &ProxyAutoConfigChild::Destroy)));
}

void ProxyAutoConfigChild::Destroy() { sActor = nullptr; }

}  // namespace mozilla::net
