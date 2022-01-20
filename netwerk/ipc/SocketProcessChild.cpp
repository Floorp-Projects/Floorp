/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SocketProcessChild.h"
#include "SocketProcessLogging.h"

#include "base/task.h"
#include "InputChannelThrottleQueueChild.h"
#include "HttpInfo.h"
#include "HttpTransactionChild.h"
#include "HttpConnectionMgrChild.h"
#include "mozilla/Assertions.h"
#include "mozilla/Atomics.h"
#include "mozilla/Components.h"
#include "mozilla/dom/MemoryReportRequest.h"
#include "mozilla/ipc/CrashReporterClient.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ipc/FileDescriptorSetChild.h"
#include "mozilla/ipc/IPCStreamAlloc.h"
#include "mozilla/ipc/ProcessChild.h"
#include "mozilla/net/AltSvcTransactionChild.h"
#include "mozilla/net/BackgroundDataBridgeParent.h"
#include "mozilla/net/DNSRequestChild.h"
#include "mozilla/net/DNSRequestParent.h"
#include "mozilla/net/NativeDNSResolverOverrideChild.h"
#include "mozilla/net/ProxyAutoConfigChild.h"
#include "mozilla/net/TRRServiceChild.h"
#include "mozilla/ipc/PChildToParentStreamChild.h"
#include "mozilla/ipc/PParentToChildStreamChild.h"
#include "mozilla/ipc/ProcessUtils.h"
#include "mozilla/Preferences.h"
#include "mozilla/RemoteLazyInputStreamChild.h"
#include "mozilla/StaticPrefs_network.h"
#include "mozilla/Telemetry.h"
#include "NetworkConnectivityService.h"
#include "nsDebugImpl.h"
#include "nsHttpConnectionInfo.h"
#include "nsHttpHandler.h"
#include "nsIDNSService.h"
#include "nsIHttpActivityObserver.h"
#include "nsNetUtil.h"
#include "nsNSSComponent.h"
#include "nsSocketTransportService2.h"
#include "nsThreadManager.h"
#include "SocketProcessBridgeParent.h"
#include "jsapi.h"
#include "js/Initialization.h"
#include "XPCSelfHostedShmem.h"

#if defined(XP_WIN)
#  include <process.h>
#else
#  include <unistd.h>
#endif

#if defined(XP_LINUX) && defined(MOZ_SANDBOX)
#  include "mozilla/Sandbox.h"
#endif

#include "ChildProfilerController.h"

#ifdef MOZ_WEBRTC
#  include "mozilla/net/WebrtcTCPSocketChild.h"
#endif

#if defined(MOZ_SANDBOX) && defined(MOZ_DEBUG) && defined(ENABLE_TESTS)
#  include "mozilla/SandboxTestingChild.h"
#endif

namespace mozilla {
namespace net {

using namespace ipc;

static bool sInitializedJS = false;

static Atomic<SocketProcessChild*> sSocketProcessChild;

SocketProcessChild::SocketProcessChild() {
  LOG(("CONSTRUCT SocketProcessChild::SocketProcessChild\n"));
  nsDebugImpl::SetMultiprocessMode("Socket");

  MOZ_COUNT_CTOR(SocketProcessChild);
  sSocketProcessChild = this;
}

SocketProcessChild::~SocketProcessChild() {
  LOG(("DESTRUCT SocketProcessChild::SocketProcessChild\n"));
  MOZ_COUNT_DTOR(SocketProcessChild);
  sSocketProcessChild = nullptr;
}

/* static */
SocketProcessChild* SocketProcessChild::GetSingleton() {
  return sSocketProcessChild;
}

#if defined(XP_MACOSX)
extern "C" {
void CGSShutdownServerConnections();
};
#endif

bool SocketProcessChild::Init(base::ProcessId aParentPid,
                              const char* aParentBuildID,
                              mozilla::ipc::ScopedPort aPort) {
  if (NS_WARN_IF(NS_FAILED(nsThreadManager::get().Init()))) {
    return false;
  }
  if (NS_WARN_IF(!Open(std::move(aPort), aParentPid))) {
    return false;
  }
  // This must be sent before any IPDL message, which may hit sentinel
  // errors due to parent and content processes having different
  // versions.
  MessageChannel* channel = GetIPCChannel();
  if (channel && !channel->SendBuildIDsMatchMessage(aParentBuildID)) {
    // We need to quit this process if the buildID doesn't match the parent's.
    // This can occur when an update occurred in the background.
    ProcessChild::QuickExit();
  }

  // Init crash reporter support.
  CrashReporterClient::InitSingleton(this);

  if (NS_FAILED(NS_InitMinimalXPCOM())) {
    return false;
  }

  if (StaticPrefs::network_proxy_parse_pac_on_socket_process()) {
    // For parsing PAC.
    const char* jsInitFailureReason = JS_InitWithFailureDiagnostic();
    if (jsInitFailureReason) {
      MOZ_CRASH_UNSAFE(jsInitFailureReason);
    }
    sInitializedJS = true;

    xpc::SelfHostedShmem::GetSingleton();
  }

  BackgroundChild::Startup();
  SetThisProcessName("Socket Process");
#if defined(XP_MACOSX)
  // Close all current connections to the WindowServer. This ensures that the
  // Activity Monitor will not label the socket process as "Not responding"
  // because it's not running a native event loop. See bug 1384336.
  CGSShutdownServerConnections();
#endif  // XP_MACOSX

  nsresult rv;
  nsCOMPtr<nsIIOService> ios = do_GetIOService(&rv);
  if (NS_FAILED(rv)) {
    return false;
  }

  nsCOMPtr<nsIProtocolHandler> handler;
  rv = ios->GetProtocolHandler("http", getter_AddRefs(handler));
  if (NS_FAILED(rv)) {
    return false;
  }

  // Initialize DNS Service here, since it needs to be done in main thread.
  nsCOMPtr<nsIDNSService> dns =
      do_GetService("@mozilla.org/network/dns-service;1", &rv);
  return NS_SUCCEEDED(rv);
}

void SocketProcessChild::ActorDestroy(ActorDestroyReason aWhy) {
  LOG(("SocketProcessChild::ActorDestroy\n"));

  mShuttingDown = true;

  if (AbnormalShutdown == aWhy) {
    NS_WARNING("Shutting down Socket process early due to a crash!");
    ProcessChild::QuickExit();
  }

  if (mProfilerController) {
    mProfilerController->Shutdown();
    mProfilerController = nullptr;
  }

  CrashReporterClient::DestroySingleton();
  XRE_ShutdownChildProcess();
}

void SocketProcessChild::CleanUp() {
  LOG(("SocketProcessChild::CleanUp\n"));

  for (const auto& parent : mSocketProcessBridgeParentMap.Values()) {
    if (!parent->Closed()) {
      parent->Close();
    }
  }

  {
    MutexAutoLock lock(mMutex);
    mBackgroundDataBridgeMap.Clear();
  }
  NS_ShutdownXPCOM(nullptr);

  if (sInitializedJS) {
    JS_ShutDown();
  }
}

mozilla::ipc::IPCResult SocketProcessChild::RecvInit(
    const SocketPorcessInitAttributes& aAttributes) {
  Unused << RecvSetOffline(aAttributes.mOffline());
  Unused << RecvSetConnectivity(aAttributes.mConnectivity());
  if (aAttributes.mInitSandbox()) {
    Unused << RecvInitLinuxSandbox(aAttributes.mSandboxBroker());
  }
  return IPC_OK();
}

IPCResult SocketProcessChild::RecvPreferenceUpdate(const Pref& aPref) {
  Preferences::SetPreference(aPref);
  return IPC_OK();
}

mozilla::ipc::IPCResult SocketProcessChild::RecvRequestMemoryReport(
    const uint32_t& aGeneration, const bool& aAnonymize,
    const bool& aMinimizeMemoryUsage,
    const Maybe<ipc::FileDescriptor>& aDMDFile,
    const RequestMemoryReportResolver& aResolver) {
  nsPrintfCString processName("Socket (pid %u)", (unsigned)getpid());

  mozilla::dom::MemoryReportRequestClient::Start(
      aGeneration, aAnonymize, aMinimizeMemoryUsage, aDMDFile, processName,
      [&](const MemoryReport& aReport) {
        Unused << GetSingleton()->SendAddMemoryReport(aReport);
      },
      aResolver);
  return IPC_OK();
}

mozilla::ipc::IPCResult SocketProcessChild::RecvSetOffline(
    const bool& aOffline) {
  LOG(("SocketProcessChild::RecvSetOffline aOffline=%d\n", aOffline));

  nsCOMPtr<nsIIOService> io(do_GetIOService());
  NS_ASSERTION(io, "IO Service can not be null");

  io->SetOffline(aOffline);

  return IPC_OK();
}

mozilla::ipc::IPCResult SocketProcessChild::RecvSetConnectivity(
    const bool& aConnectivity) {
  nsCOMPtr<nsIIOService> io(do_GetIOService());
  nsCOMPtr<nsIIOServiceInternal> ioInternal(do_QueryInterface(io));
  NS_ASSERTION(ioInternal, "IO Service can not be null");

  ioInternal->SetConnectivity(aConnectivity);

  return IPC_OK();
}

mozilla::ipc::IPCResult SocketProcessChild::RecvInitLinuxSandbox(
    const Maybe<ipc::FileDescriptor>& aBrokerFd) {
#if defined(XP_LINUX) && defined(MOZ_SANDBOX)
  int fd = -1;
  if (aBrokerFd.isSome()) {
    fd = aBrokerFd.value().ClonePlatformHandle().release();
  }
  SetSocketProcessSandbox(fd);
#endif  // XP_LINUX && MOZ_SANDBOX
  return IPC_OK();
}

mozilla::ipc::IPCResult SocketProcessChild::RecvInitSocketProcessBridgeParent(
    const ProcessId& aContentProcessId,
    Endpoint<mozilla::net::PSocketProcessBridgeParent>&& aEndpoint) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mSocketProcessBridgeParentMap.Contains(aContentProcessId));

  mSocketProcessBridgeParentMap.InsertOrUpdate(
      aContentProcessId, MakeRefPtr<SocketProcessBridgeParent>(
                             aContentProcessId, std::move(aEndpoint)));
  return IPC_OK();
}

mozilla::ipc::IPCResult SocketProcessChild::RecvInitProfiler(
    Endpoint<PProfilerChild>&& aEndpoint) {
  mProfilerController =
      mozilla::ChildProfilerController::Create(std::move(aEndpoint));
  return IPC_OK();
}

#if defined(MOZ_SANDBOX) && defined(MOZ_DEBUG) && defined(ENABLE_TESTS)
mozilla::ipc::IPCResult SocketProcessChild::RecvInitSandboxTesting(
    Endpoint<PSandboxTestingChild>&& aEndpoint) {
  if (!SandboxTestingChild::Initialize(std::move(aEndpoint))) {
    return IPC_FAIL(
        this, "InitSandboxTesting failed to initialise the child process.");
  }
  return IPC_OK();
}
#endif

mozilla::ipc::IPCResult SocketProcessChild::RecvSocketProcessTelemetryPing() {
  const uint32_t kExpectedUintValue = 42;
  Telemetry::ScalarSet(Telemetry::ScalarID::TELEMETRY_TEST_SOCKET_ONLY_UINT,
                       kExpectedUintValue);
  return IPC_OK();
}

void SocketProcessChild::DestroySocketProcessBridgeParent(ProcessId aId) {
  MOZ_ASSERT(NS_IsMainThread());

  mSocketProcessBridgeParentMap.Remove(aId);
}

PWebrtcTCPSocketChild* SocketProcessChild::AllocPWebrtcTCPSocketChild(
    const Maybe<TabId>& tabId) {
  // We don't allocate here: instead we always use IPDL constructor that takes
  // an existing object
  MOZ_ASSERT_UNREACHABLE(
      "AllocPWebrtcTCPSocketChild should not be called on"
      " socket child");
  return nullptr;
}

bool SocketProcessChild::DeallocPWebrtcTCPSocketChild(
    PWebrtcTCPSocketChild* aActor) {
#ifdef MOZ_WEBRTC
  WebrtcTCPSocketChild* child = static_cast<WebrtcTCPSocketChild*>(aActor);
  child->ReleaseIPDLReference();
#endif
  return true;
}

already_AddRefed<PHttpTransactionChild>
SocketProcessChild::AllocPHttpTransactionChild() {
  RefPtr<HttpTransactionChild> actor = new HttpTransactionChild();
  return actor.forget();
}

PFileDescriptorSetChild* SocketProcessChild::AllocPFileDescriptorSetChild(
    const FileDescriptor& aFD) {
  return new FileDescriptorSetChild(aFD);
}

bool SocketProcessChild::DeallocPFileDescriptorSetChild(
    PFileDescriptorSetChild* aActor) {
  delete aActor;
  return true;
}

PChildToParentStreamChild*
SocketProcessChild::AllocPChildToParentStreamChild() {
  MOZ_CRASH("PChildToParentStreamChild actors should be manually constructed!");
}

bool SocketProcessChild::DeallocPChildToParentStreamChild(
    PChildToParentStreamChild* aActor) {
  delete aActor;
  return true;
}

PParentToChildStreamChild*
SocketProcessChild::AllocPParentToChildStreamChild() {
  return mozilla::ipc::AllocPParentToChildStreamChild();
}

bool SocketProcessChild::DeallocPParentToChildStreamChild(
    PParentToChildStreamChild* aActor) {
  delete aActor;
  return true;
}

PChildToParentStreamChild*
SocketProcessChild::SendPChildToParentStreamConstructor(
    PChildToParentStreamChild* aActor) {
  MOZ_ASSERT(NS_IsMainThread());
  return PSocketProcessChild::SendPChildToParentStreamConstructor(aActor);
}

PFileDescriptorSetChild* SocketProcessChild::SendPFileDescriptorSetConstructor(
    const FileDescriptor& aFD) {
  MOZ_ASSERT(NS_IsMainThread());
  return PSocketProcessChild::SendPFileDescriptorSetConstructor(aFD);
}

already_AddRefed<PHttpConnectionMgrChild>
SocketProcessChild::AllocPHttpConnectionMgrChild(
    const HttpHandlerInitArgs& aArgs) {
  LOG(("SocketProcessChild::AllocPHttpConnectionMgrChild \n"));
  MOZ_ASSERT(gHttpHandler);
  gHttpHandler->SetHttpHandlerInitArgs(aArgs);

  RefPtr<HttpConnectionMgrChild> actor = new HttpConnectionMgrChild();
  return actor.forget();
}

mozilla::ipc::IPCResult SocketProcessChild::RecvUpdateDeviceModelId(
    const nsCString& aModelId) {
  MOZ_ASSERT(gHttpHandler);
  gHttpHandler->SetDeviceModelId(aModelId);
  return IPC_OK();
}

mozilla::ipc::IPCResult
SocketProcessChild::RecvOnHttpActivityDistributorActivated(
    const bool& aIsActivated) {
  if (nsCOMPtr<nsIHttpActivityObserver> distributor =
          components::HttpActivityDistributor::Service()) {
    distributor->SetIsActive(aIsActivated);
  }
  return IPC_OK();
}
already_AddRefed<PInputChannelThrottleQueueChild>
SocketProcessChild::AllocPInputChannelThrottleQueueChild(
    const uint32_t& aMeanBytesPerSecond, const uint32_t& aMaxBytesPerSecond) {
  RefPtr<InputChannelThrottleQueueChild> p =
      new InputChannelThrottleQueueChild();
  p->Init(aMeanBytesPerSecond, aMaxBytesPerSecond);
  return p.forget();
}

already_AddRefed<PAltSvcTransactionChild>
SocketProcessChild::AllocPAltSvcTransactionChild(
    const HttpConnectionInfoCloneArgs& aConnInfo, const uint32_t& aCaps) {
  RefPtr<nsHttpConnectionInfo> cinfo =
      nsHttpConnectionInfo::DeserializeHttpConnectionInfoCloneArgs(aConnInfo);
  RefPtr<AltSvcTransactionChild> child =
      new AltSvcTransactionChild(cinfo, aCaps);
  return child.forget();
}

already_AddRefed<PDNSRequestChild> SocketProcessChild::AllocPDNSRequestChild(
    const nsCString& aHost, const nsCString& aTrrServer, const uint16_t& aType,
    const OriginAttributes& aOriginAttributes, const uint32_t& aFlags) {
  RefPtr<DNSRequestHandler> handler = new DNSRequestHandler();
  RefPtr<DNSRequestChild> actor = new DNSRequestChild(handler);
  return actor.forget();
}

mozilla::ipc::IPCResult SocketProcessChild::RecvPDNSRequestConstructor(
    PDNSRequestChild* aActor, const nsCString& aHost,
    const nsCString& aTrrServer, const uint16_t& aType,
    const OriginAttributes& aOriginAttributes, const uint32_t& aFlags) {
  RefPtr<DNSRequestChild> actor = static_cast<DNSRequestChild*>(aActor);
  RefPtr<DNSRequestHandler> handler =
      actor->GetDNSRequest()->AsDNSRequestHandler();
  handler->DoAsyncResolve(aHost, aTrrServer, aType, aOriginAttributes, aFlags);
  return IPC_OK();
}

void SocketProcessChild::AddDataBridgeToMap(
    uint64_t aChannelId, BackgroundDataBridgeParent* aActor) {
  ipc::AssertIsOnBackgroundThread();
  MutexAutoLock lock(mMutex);
  mBackgroundDataBridgeMap.InsertOrUpdate(aChannelId, RefPtr{aActor});
}

void SocketProcessChild::RemoveDataBridgeFromMap(uint64_t aChannelId) {
  ipc::AssertIsOnBackgroundThread();
  MutexAutoLock lock(mMutex);
  mBackgroundDataBridgeMap.Remove(aChannelId);
}

Maybe<RefPtr<BackgroundDataBridgeParent>>
SocketProcessChild::GetAndRemoveDataBridge(uint64_t aChannelId) {
  MutexAutoLock lock(mMutex);
  return mBackgroundDataBridgeMap.Extract(aChannelId);
}

mozilla::ipc::IPCResult SocketProcessChild::RecvClearSessionCache() {
  if (EnsureNSSInitializedChromeOrContent()) {
    nsNSSComponent::DoClearSSLExternalAndInternalSessionCache();
  }
  return IPC_OK();
}

already_AddRefed<PTRRServiceChild> SocketProcessChild::AllocPTRRServiceChild(
    const bool& aCaptiveIsPassed, const bool& aParentalControlEnabled,
    const nsTArray<nsCString>& aDNSSuffixList) {
  RefPtr<TRRServiceChild> actor = new TRRServiceChild();
  return actor.forget();
}

mozilla::ipc::IPCResult SocketProcessChild::RecvPTRRServiceConstructor(
    PTRRServiceChild* aActor, const bool& aCaptiveIsPassed,
    const bool& aParentalControlEnabled, nsTArray<nsCString>&& aDNSSuffixList) {
  static_cast<TRRServiceChild*>(aActor)->Init(
      aCaptiveIsPassed, aParentalControlEnabled, std::move(aDNSSuffixList));
  return IPC_OK();
}

already_AddRefed<PNativeDNSResolverOverrideChild>
SocketProcessChild::AllocPNativeDNSResolverOverrideChild() {
  RefPtr<NativeDNSResolverOverrideChild> actor =
      new NativeDNSResolverOverrideChild();
  return actor.forget();
}

mozilla::ipc::IPCResult
SocketProcessChild::RecvPNativeDNSResolverOverrideConstructor(
    PNativeDNSResolverOverrideChild* aActor) {
  return IPC_OK();
}

mozilla::ipc::IPCResult SocketProcessChild::RecvNotifyObserver(
    const nsCString& aTopic, const nsString& aData) {
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->NotifyObservers(nullptr, aTopic.get(), aData.get());
  }
  return IPC_OK();
}

already_AddRefed<PRemoteLazyInputStreamChild>
SocketProcessChild::AllocPRemoteLazyInputStreamChild(const nsID& aID,
                                                     const uint64_t& aSize) {
  RefPtr<RemoteLazyInputStreamChild> actor =
      new RemoteLazyInputStreamChild(aID, aSize);
  return actor.forget();
}

namespace {

class DataResolverBase {
 public:
  // This type is threadsafe-refcounted, as it's referenced on the socket
  // thread, but must be destroyed on the main thread.
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING_WITH_DELETE_ON_MAIN_THREAD(
      DataResolverBase)

  DataResolverBase() = default;

 protected:
  virtual ~DataResolverBase() = default;
};

template <typename DataType, typename ResolverType>
class DataResolver final : public DataResolverBase {
 public:
  explicit DataResolver(ResolverType&& aResolve)
      : mResolve(std::move(aResolve)) {}

  void OnResolve(DataType&& aData) {
    MOZ_ASSERT(OnSocketThread());

    RefPtr<DataResolver<DataType, ResolverType>> self = this;
    mData = std::move(aData);
    NS_DispatchToMainThread(NS_NewRunnableFunction(
        "net::DataResolver::OnResolve",
        [self{std::move(self)}]() { self->mResolve(std::move(self->mData)); }));
  }

 private:
  virtual ~DataResolver() = default;

  ResolverType mResolve;
  DataType mData;
};

}  // anonymous namespace

mozilla::ipc::IPCResult SocketProcessChild::RecvGetSocketData(
    GetSocketDataResolver&& aResolve) {
  if (!gSocketTransportService) {
    aResolve(SocketDataArgs());
    return IPC_OK();
  }

  RefPtr<
      DataResolver<SocketDataArgs, SocketProcessChild::GetSocketDataResolver>>
      resolver = new DataResolver<SocketDataArgs,
                                  SocketProcessChild::GetSocketDataResolver>(
          std::move(aResolve));
  gSocketTransportService->Dispatch(
      NS_NewRunnableFunction(
          "net::SocketProcessChild::RecvGetSocketData",
          [resolver{std::move(resolver)}]() {
            SocketDataArgs args;
            gSocketTransportService->GetSocketConnections(&args.info());
            args.totalSent() = gSocketTransportService->GetSentBytes();
            args.totalRecv() = gSocketTransportService->GetReceivedBytes();
            resolver->OnResolve(std::move(args));
          }),
      NS_DISPATCH_NORMAL);
  return IPC_OK();
}

mozilla::ipc::IPCResult SocketProcessChild::RecvGetDNSCacheEntries(
    GetDNSCacheEntriesResolver&& aResolve) {
  nsresult rv = NS_OK;
  nsCOMPtr<nsIDNSService> dns =
      do_GetService("@mozilla.org/network/dns-service;1", &rv);
  if (NS_FAILED(rv)) {
    aResolve(nsTArray<DNSCacheEntries>());
    return IPC_OK();
  }

  RefPtr<DataResolver<nsTArray<DNSCacheEntries>,
                      SocketProcessChild::GetDNSCacheEntriesResolver>>
      resolver =
          new DataResolver<nsTArray<DNSCacheEntries>,
                           SocketProcessChild::GetDNSCacheEntriesResolver>(
              std::move(aResolve));
  gSocketTransportService->Dispatch(
      NS_NewRunnableFunction(
          "net::SocketProcessChild::RecvGetDNSCacheEntries",
          [resolver{std::move(resolver)}, dns{std::move(dns)}]() {
            nsTArray<DNSCacheEntries> entries;
            dns->GetDNSCacheEntries(&entries);
            resolver->OnResolve(std::move(entries));
          }),
      NS_DISPATCH_NORMAL);
  return IPC_OK();
}

mozilla::ipc::IPCResult SocketProcessChild::RecvGetHttpConnectionData(
    GetHttpConnectionDataResolver&& aResolve) {
  if (!gSocketTransportService) {
    aResolve(nsTArray<HttpRetParams>());
    return IPC_OK();
  }

  RefPtr<DataResolver<nsTArray<HttpRetParams>,
                      SocketProcessChild::GetHttpConnectionDataResolver>>
      resolver =
          new DataResolver<nsTArray<HttpRetParams>,
                           SocketProcessChild::GetHttpConnectionDataResolver>(
              std::move(aResolve));
  gSocketTransportService->Dispatch(
      NS_NewRunnableFunction(
          "net::SocketProcessChild::RecvGetHttpConnectionData",
          [resolver{std::move(resolver)}]() {
            nsTArray<HttpRetParams> data;
            HttpInfo::GetHttpConnectionData(&data);
            resolver->OnResolve(std::move(data));
          }),
      NS_DISPATCH_NORMAL);
  return IPC_OK();
}

mozilla::ipc::IPCResult SocketProcessChild::RecvInitProxyAutoConfigChild(
    Endpoint<PProxyAutoConfigChild>&& aEndpoint) {
  Unused << ProxyAutoConfigChild::Create(std::move(aEndpoint));
  return IPC_OK();
}

mozilla::ipc::IPCResult SocketProcessChild::RecvRecheckIPConnectivity() {
  RefPtr<NetworkConnectivityService> ncs =
      NetworkConnectivityService::GetSingleton();
  if (ncs) {
    ncs->RecheckIPConnectivity();
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult SocketProcessChild::RecvRecheckDNS() {
  RefPtr<NetworkConnectivityService> ncs =
      NetworkConnectivityService::GetSingleton();
  if (ncs) {
    ncs->RecheckDNS();
  }
  return IPC_OK();
}

}  // namespace net
}  // namespace mozilla
