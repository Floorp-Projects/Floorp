/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SocketProcessParent.h"
#include "SocketProcessLogging.h"

#include "AltServiceParent.h"
#include "CachePushChecker.h"
#include "HttpTransactionParent.h"
#include "SocketProcessHost.h"
#include "mozilla/Components.h"
#include "mozilla/dom/MemoryReportRequest.h"
#include "mozilla/ipc/FileDescriptorSetParent.h"
#include "mozilla/ipc/IPCStreamAlloc.h"
#include "mozilla/ipc/PChildToParentStreamParent.h"
#include "mozilla/ipc/PParentToChildStreamParent.h"
#include "mozilla/net/DNSRequestParent.h"
#include "mozilla/net/ProxyConfigLookupParent.h"
#include "mozilla/RemoteLazyInputStreamParent.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TelemetryIPC.h"
#include "nsIAppStartup.h"
#include "nsIConsoleService.h"
#include "nsIHttpActivityObserver.h"
#include "nsIObserverService.h"
#include "nsNSSIOLayer.h"
#include "nsIOService.h"
#include "nsHttpHandler.h"
#include "nsHttpConnectionInfo.h"
#include "PSMIPCCommon.h"
#include "secerr.h"
#ifdef MOZ_WEBRTC
#  include "mozilla/dom/ContentProcessManager.h"
#  include "mozilla/dom/BrowserParent.h"
#  include "mozilla/net/WebrtcTCPSocketParent.h"
#endif
#if defined(MOZ_WIDGET_ANDROID)
#  include "mozilla/java/GeckoProcessManagerWrappers.h"
#  include "mozilla/java/GeckoProcessTypeWrappers.h"
#endif  // defined(MOZ_WIDGET_ANDROID)

namespace mozilla {
namespace net {

static SocketProcessParent* sSocketProcessParent;

SocketProcessParent::SocketProcessParent(SocketProcessHost* aHost)
    : mHost(aHost) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mHost);

  MOZ_COUNT_CTOR(SocketProcessParent);
  sSocketProcessParent = this;
}

SocketProcessParent::~SocketProcessParent() {
  MOZ_ASSERT(NS_IsMainThread());

  MOZ_COUNT_DTOR(SocketProcessParent);
  sSocketProcessParent = nullptr;
}

/* static */
SocketProcessParent* SocketProcessParent::GetSingleton() {
  MOZ_ASSERT(NS_IsMainThread());

  return sSocketProcessParent;
}

void SocketProcessParent::ActorDestroy(ActorDestroyReason aWhy) {
#if defined(MOZ_WIDGET_ANDROID)
  nsCOMPtr<nsIEventTarget> launcherThread(ipc::GetIPCLauncher());
  MOZ_ASSERT(launcherThread);

  auto procType = java::GeckoProcessType::SOCKET();
  auto selector =
      java::GeckoProcessManager::Selector::New(procType, OtherPid());

  launcherThread->Dispatch(NS_NewRunnableFunction(
      "SocketProcessParent::ActorDestroy",
      [selector = java::GeckoProcessManager::Selector::GlobalRef(selector)]() {
        java::GeckoProcessManager::ShutdownProcess(selector);
      }));
#endif  // defined(MOZ_WIDGET_ANDROID)

  if (aWhy == AbnormalShutdown) {
    GenerateCrashReport(OtherPid());

    if (PR_GetEnv("MOZ_CRASHREPORTER_SHUTDOWN")) {
      printf_stderr("Shutting down due to socket process crash.\n");
      nsCOMPtr<nsIAppStartup> appService =
          do_GetService("@mozilla.org/toolkit/app-startup;1");
      if (appService) {
        bool userAllowedQuit = true;
        appService->Quit(nsIAppStartup::eForceQuit, 1, &userAllowedQuit);
      }
    }
  }

  if (mHost) {
    mHost->OnChannelClosed();
  }
}

bool SocketProcessParent::SendRequestMemoryReport(
    const uint32_t& aGeneration, const bool& aAnonymize,
    const bool& aMinimizeMemoryUsage,
    const Maybe<ipc::FileDescriptor>& aDMDFile) {
  mMemoryReportRequest = MakeUnique<dom::MemoryReportRequestHost>(aGeneration);

  PSocketProcessParent::SendRequestMemoryReport(
      aGeneration, aAnonymize, aMinimizeMemoryUsage, aDMDFile,
      [&](const uint32_t& aGeneration2) {
        MOZ_ASSERT(gIOService);
        if (!gIOService->SocketProcess()) {
          return;
        }
        SocketProcessParent* actor = gIOService->SocketProcess()->GetActor();
        if (!actor) {
          return;
        }
        if (actor->mMemoryReportRequest) {
          actor->mMemoryReportRequest->Finish(aGeneration2);
          actor->mMemoryReportRequest = nullptr;
        }
      },
      [&](mozilla::ipc::ResponseRejectReason) {
        MOZ_ASSERT(gIOService);
        if (!gIOService->SocketProcess()) {
          return;
        }
        SocketProcessParent* actor = gIOService->SocketProcess()->GetActor();
        if (!actor) {
          return;
        }
        actor->mMemoryReportRequest = nullptr;
      });

  return true;
}

mozilla::ipc::IPCResult SocketProcessParent::RecvAddMemoryReport(
    const MemoryReport& aReport) {
  if (mMemoryReportRequest) {
    mMemoryReportRequest->RecvReport(aReport);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult SocketProcessParent::RecvAccumulateChildHistograms(
    nsTArray<HistogramAccumulation>&& aAccumulations) {
  TelemetryIPC::AccumulateChildHistograms(Telemetry::ProcessID::Socket,
                                          aAccumulations);
  return IPC_OK();
}

mozilla::ipc::IPCResult SocketProcessParent::RecvAccumulateChildKeyedHistograms(
    nsTArray<KeyedHistogramAccumulation>&& aAccumulations) {
  TelemetryIPC::AccumulateChildKeyedHistograms(Telemetry::ProcessID::Socket,
                                               aAccumulations);
  return IPC_OK();
}

mozilla::ipc::IPCResult SocketProcessParent::RecvUpdateChildScalars(
    nsTArray<ScalarAction>&& aScalarActions) {
  TelemetryIPC::UpdateChildScalars(Telemetry::ProcessID::Socket,
                                   aScalarActions);
  return IPC_OK();
}

mozilla::ipc::IPCResult SocketProcessParent::RecvUpdateChildKeyedScalars(
    nsTArray<KeyedScalarAction>&& aScalarActions) {
  TelemetryIPC::UpdateChildKeyedScalars(Telemetry::ProcessID::Socket,
                                        aScalarActions);
  return IPC_OK();
}

mozilla::ipc::IPCResult SocketProcessParent::RecvRecordChildEvents(
    nsTArray<mozilla::Telemetry::ChildEventData>&& aEvents) {
  TelemetryIPC::RecordChildEvents(Telemetry::ProcessID::Socket, aEvents);
  return IPC_OK();
}

mozilla::ipc::IPCResult SocketProcessParent::RecvRecordDiscardedData(
    const mozilla::Telemetry::DiscardedData& aDiscardedData) {
  TelemetryIPC::RecordDiscardedData(Telemetry::ProcessID::Socket,
                                    aDiscardedData);
  return IPC_OK();
}

PWebrtcTCPSocketParent* SocketProcessParent::AllocPWebrtcTCPSocketParent(
    const Maybe<TabId>& aTabId) {
#ifdef MOZ_WEBRTC
  WebrtcTCPSocketParent* parent = new WebrtcTCPSocketParent(aTabId);
  parent->AddRef();
  return parent;
#else
  return nullptr;
#endif
}

bool SocketProcessParent::DeallocPWebrtcTCPSocketParent(
    PWebrtcTCPSocketParent* aActor) {
#ifdef MOZ_WEBRTC
  WebrtcTCPSocketParent* parent = static_cast<WebrtcTCPSocketParent*>(aActor);
  parent->Release();
#endif
  return true;
}

already_AddRefed<PDNSRequestParent> SocketProcessParent::AllocPDNSRequestParent(
    const nsCString& aHost, const nsCString& aTrrServer, const uint16_t& aType,
    const OriginAttributes& aOriginAttributes, const uint32_t& aFlags) {
  RefPtr<DNSRequestHandler> handler = new DNSRequestHandler();
  RefPtr<DNSRequestParent> actor = new DNSRequestParent(handler);
  return actor.forget();
}

mozilla::ipc::IPCResult SocketProcessParent::RecvPDNSRequestConstructor(
    PDNSRequestParent* aActor, const nsCString& aHost,
    const nsCString& aTrrServer, const uint16_t& aType,
    const OriginAttributes& aOriginAttributes, const uint32_t& aFlags) {
  RefPtr<DNSRequestParent> actor = static_cast<DNSRequestParent*>(aActor);
  RefPtr<DNSRequestHandler> handler =
      actor->GetDNSRequest()->AsDNSRequestHandler();
  handler->DoAsyncResolve(aHost, aTrrServer, aType, aOriginAttributes, aFlags);
  return IPC_OK();
}

mozilla::ipc::PFileDescriptorSetParent*
SocketProcessParent::AllocPFileDescriptorSetParent(const FileDescriptor& aFD) {
  return new mozilla::ipc::FileDescriptorSetParent(aFD);
}

bool SocketProcessParent::DeallocPFileDescriptorSetParent(
    PFileDescriptorSetParent* aActor) {
  delete static_cast<mozilla::ipc::FileDescriptorSetParent*>(aActor);
  return true;
}

mozilla::ipc::PChildToParentStreamParent*
SocketProcessParent::AllocPChildToParentStreamParent() {
  return mozilla::ipc::AllocPChildToParentStreamParent();
}

bool SocketProcessParent::DeallocPChildToParentStreamParent(
    PChildToParentStreamParent* aActor) {
  delete aActor;
  return true;
}

mozilla::ipc::PParentToChildStreamParent*
SocketProcessParent::AllocPParentToChildStreamParent() {
  MOZ_CRASH("PParentToChildStreamChild actors should be manually constructed!");
}

bool SocketProcessParent::DeallocPParentToChildStreamParent(
    PParentToChildStreamParent* aActor) {
  delete aActor;
  return true;
}

mozilla::ipc::PParentToChildStreamParent*
SocketProcessParent::SendPParentToChildStreamConstructor(
    PParentToChildStreamParent* aActor) {
  MOZ_ASSERT(NS_IsMainThread());
  return PSocketProcessParent::SendPParentToChildStreamConstructor(aActor);
}

mozilla::ipc::PFileDescriptorSetParent*
SocketProcessParent::SendPFileDescriptorSetConstructor(
    const FileDescriptor& aFD) {
  MOZ_ASSERT(NS_IsMainThread());
  return PSocketProcessParent::SendPFileDescriptorSetConstructor(aFD);
}

mozilla::ipc::IPCResult SocketProcessParent::RecvObserveHttpActivity(
    const HttpActivityArgs& aArgs, const uint32_t& aActivityType,
    const uint32_t& aActivitySubtype, const PRTime& aTimestamp,
    const uint64_t& aExtraSizeData, const nsCString& aExtraStringData) {
  nsCOMPtr<nsIHttpActivityDistributor> activityDistributor =
      components::HttpActivityDistributor::Service();
  MOZ_ASSERT(activityDistributor);

  Unused << activityDistributor->ObserveActivityWithArgs(
      aArgs, aActivityType, aActivitySubtype, aTimestamp, aExtraSizeData,
      aExtraStringData);
  return IPC_OK();
}

mozilla::ipc::IPCResult SocketProcessParent::RecvInitBackground(
    Endpoint<PBackgroundParent>&& aEndpoint) {
  LOG(("SocketProcessParent::RecvInitBackground\n"));
  if (!ipc::BackgroundParent::Alloc(nullptr, std::move(aEndpoint))) {
    return IPC_FAIL(this, "BackgroundParent::Alloc failed");
  }

  return IPC_OK();
}

already_AddRefed<PAltServiceParent>
SocketProcessParent::AllocPAltServiceParent() {
  RefPtr<AltServiceParent> actor = new AltServiceParent();
  return actor.forget();
}

mozilla::ipc::IPCResult SocketProcessParent::RecvGetTLSClientCert(
    const nsCString& aHostName, const OriginAttributes& aOriginAttributes,
    const int32_t& aPort, const uint32_t& aProviderFlags,
    const uint32_t& aProviderTlsFlags, const ByteArray& aServerCert,
    Maybe<ByteArray>&& aClientCert, nsTArray<ByteArray>&& aCollectedCANames,
    bool* aSucceeded, ByteArray* aOutCert, ByteArray* aOutKey,
    nsTArray<ByteArray>* aBuiltChain) {
  *aSucceeded = false;

  SECItem serverCertItem = {
      siBuffer, const_cast<uint8_t*>(aServerCert.data().Elements()),
      static_cast<unsigned int>(aServerCert.data().Length())};
  UniqueCERTCertificate serverCert(CERT_NewTempCertificate(
      CERT_GetDefaultCertDB(), &serverCertItem, nullptr, false, true));
  if (!serverCert) {
    return IPC_OK();
  }

  RefPtr<nsIX509Cert> clientCert;
  if (aClientCert) {
    clientCert = nsNSSCertificate::ConstructFromDER(
        BitwiseCast<char*, uint8_t*>(aClientCert->data().Elements()),
        aClientCert->data().Length());
    if (!clientCert) {
      return IPC_OK();
    }
  }

  ClientAuthInfo info(aHostName, aOriginAttributes, aPort, aProviderFlags,
                      aProviderTlsFlags, clientCert);
  nsTArray<nsTArray<uint8_t>> collectedCANames;
  for (auto& name : aCollectedCANames) {
    collectedCANames.AppendElement(std::move(name.data()));
  }

  UniqueCERTCertificate cert;
  UniqueSECKEYPrivateKey key;
  UniqueCERTCertList builtChain;
  SECStatus status =
      DoGetClientAuthData(std::move(info), serverCert,
                          std::move(collectedCANames), cert, key, builtChain);
  if (status != SECSuccess) {
    return IPC_OK();
  }

  SerializeClientCertAndKey(cert, key, *aOutCert, *aOutKey);

  if (builtChain) {
    for (CERTCertListNode* n = CERT_LIST_HEAD(builtChain);
         !CERT_LIST_END(n, builtChain); n = CERT_LIST_NEXT(n)) {
      ByteArray array;
      array.data().AppendElements(n->cert->derCert.data, n->cert->derCert.len);
      aBuiltChain->AppendElement(std::move(array));
    }
  }

  *aSucceeded = true;
  return IPC_OK();
}

already_AddRefed<PProxyConfigLookupParent>
SocketProcessParent::AllocPProxyConfigLookupParent(
    nsIURI* aURI, const uint32_t& aProxyResolveFlags) {
  RefPtr<ProxyConfigLookupParent> actor =
      new ProxyConfigLookupParent(aURI, aProxyResolveFlags);
  return actor.forget();
}

mozilla::ipc::IPCResult SocketProcessParent::RecvPProxyConfigLookupConstructor(
    PProxyConfigLookupParent* aActor, nsIURI* aURI,
    const uint32_t& aProxyResolveFlags) {
  static_cast<ProxyConfigLookupParent*>(aActor)->DoProxyLookup();
  return IPC_OK();
}

mozilla::ipc::IPCResult SocketProcessParent::RecvCachePushCheck(
    nsIURI* aPushedURL, OriginAttributes&& aOriginAttributes,
    nsCString&& aRequestString, CachePushCheckResolver&& aResolver) {
  RefPtr<CachePushChecker> checker = new CachePushChecker(
      aPushedURL, aOriginAttributes, aRequestString, aResolver);
  if (NS_FAILED(checker->DoCheck())) {
    aResolver(false);
  }
  return IPC_OK();
}

// To ensure that IPDL is finished before SocketParent gets deleted.
class DeferredDeleteSocketProcessParent : public Runnable {
 public:
  explicit DeferredDeleteSocketProcessParent(
      UniquePtr<SocketProcessParent>&& aParent)
      : Runnable("net::DeferredDeleteSocketProcessParent"),
        mParent(std::move(aParent)) {}

  NS_IMETHODIMP Run() override { return NS_OK; }

 private:
  UniquePtr<SocketProcessParent> mParent;
};

/* static */
void SocketProcessParent::Destroy(UniquePtr<SocketProcessParent>&& aParent) {
  NS_DispatchToMainThread(
      new DeferredDeleteSocketProcessParent(std::move(aParent)));
}

already_AddRefed<PRemoteLazyInputStreamParent>
SocketProcessParent::AllocPRemoteLazyInputStreamParent(const nsID& aID,
                                                       const uint64_t& aSize) {
  RefPtr<RemoteLazyInputStreamParent> actor =
      RemoteLazyInputStreamParent::Create(aID, aSize, this);
  return actor.forget();
}

mozilla::ipc::IPCResult
SocketProcessParent::RecvPRemoteLazyInputStreamConstructor(
    PRemoteLazyInputStreamParent* aActor, const nsID& aID,
    const uint64_t& aSize) {
  if (!static_cast<RemoteLazyInputStreamParent*>(aActor)->HasValidStream()) {
    return IPC_FAIL_NO_REASON(this);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult SocketProcessParent::RecvODoHServiceActivated(
    const bool& aActivated) {
  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();

  if (observerService) {
    observerService->NotifyObservers(nullptr, "odoh-service-activated",
                                     aActivated ? u"true" : u"false");
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult SocketProcessParent::RecvExcludeHttp2OrHttp3(
    const HttpConnectionInfoCloneArgs& aArgs) {
  RefPtr<nsHttpConnectionInfo> cinfo =
      nsHttpConnectionInfo::DeserializeHttpConnectionInfoCloneArgs(aArgs);
  if (!cinfo) {
    MOZ_ASSERT(false, "failed to deserizlize http connection info");
    return IPC_OK();
  }

  if (cinfo->IsHttp3()) {
    gHttpHandler->ExcludeHttp3(cinfo);
  } else {
    gHttpHandler->ExcludeHttp2(cinfo);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult SocketProcessParent::RecvOnConsoleMessage(
    const nsString& aMessage) {
  nsCOMPtr<nsIConsoleService> consoleService =
      do_GetService(NS_CONSOLESERVICE_CONTRACTID);
  if (consoleService) {
    consoleService->LogStringMessage(aMessage.get());
  }
  return IPC_OK();
}

}  // namespace net
}  // namespace mozilla
