/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SocketProcessChild.h"
#include "SocketProcessLogging.h"

#include "base/task.h"
#include "InputChannelThrottleQueueChild.h"
#include "HttpTransactionChild.h"
#include "HttpConnectionMgrChild.h"
#include "mozilla/Assertions.h"
#include "mozilla/dom/MemoryReportRequest.h"
#include "mozilla/ipc/CrashReporterClient.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/FileDescriptorSetChild.h"
#include "mozilla/ipc/IPCStreamAlloc.h"
#include "mozilla/ipc/ProcessChild.h"
#include "mozilla/net/AltSvcTransactionChild.h"
#include "mozilla/net/DNSRequestChild.h"
#include "mozilla/ipc/PChildToParentStreamChild.h"
#include "mozilla/ipc/PParentToChildStreamChild.h"
#include "mozilla/Preferences.h"
#include "nsDebugImpl.h"
#include "nsIDNSService.h"
#include "nsIHttpActivityObserver.h"
#include "nsThreadManager.h"
#include "ProcessUtils.h"
#include "SocketProcessBridgeParent.h"

#if defined(XP_LINUX) && defined(MOZ_SANDBOX)
#  include "mozilla/Sandbox.h"
#endif

#ifdef MOZ_GECKO_PROFILER
#  include "ChildProfilerController.h"
#endif

#ifdef MOZ_WEBRTC
#  include "mozilla/net/WebrtcTCPSocketChild.h"
#endif

namespace mozilla {
namespace net {

using namespace ipc;

SocketProcessChild* sSocketProcessChild;

SocketProcessChild::SocketProcessChild() : mShuttingDown(false) {
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
                              const char* aParentBuildID, MessageLoop* aIOLoop,
                              UniquePtr<IPC::Channel> aChannel) {
  if (NS_WARN_IF(NS_FAILED(nsThreadManager::get().Init()))) {
    return false;
  }
  if (NS_WARN_IF(!Open(std::move(aChannel), aParentPid, aIOLoop))) {
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

  BackgroundChild::Startup();
  SetThisProcessName("Socket Process");
#if defined(XP_MACOSX)
  // Close all current connections to the WindowServer. This ensures that the
  // Activity Monitor will not label the socket process as "Not responding"
  // because it's not running a native event loop. See bug 1384336.
  CGSShutdownServerConnections();
#endif  // XP_MACOSX
  return true;
}

void SocketProcessChild::ActorDestroy(ActorDestroyReason aWhy) {
  LOG(("SocketProcessChild::ActorDestroy\n"));

  mShuttingDown = true;

  if (AbnormalShutdown == aWhy) {
    NS_WARNING("Shutting down Socket process early due to a crash!");
    ProcessChild::QuickExit();
  }

#ifdef MOZ_GECKO_PROFILER
  if (mProfilerController) {
    mProfilerController->Shutdown();
    mProfilerController = nullptr;
  }
#endif

  CrashReporterClient::DestroySingleton();
  XRE_ShutdownChildProcess();
}

void SocketProcessChild::CleanUp() {
  LOG(("SocketProcessChild::CleanUp\n"));

  for (auto iter = mSocketProcessBridgeParentMap.Iter(); !iter.Done();
       iter.Next()) {
    if (!iter.Data()->Closed()) {
      iter.Data()->Close();
    }
  }
  NS_ShutdownXPCOM(nullptr);
}

IPCResult SocketProcessChild::RecvPreferenceUpdate(const Pref& aPref) {
  Preferences::SetPreference(aPref);
  return IPC_OK();
}

mozilla::ipc::IPCResult SocketProcessChild::RecvRequestMemoryReport(
    const uint32_t& aGeneration, const bool& aAnonymize,
    const bool& aMinimizeMemoryUsage,
    const Maybe<ipc::FileDescriptor>& aDMDFile) {
  nsPrintfCString processName("SocketProcess");

  mozilla::dom::MemoryReportRequestClient::Start(
      aGeneration, aAnonymize, aMinimizeMemoryUsage, aDMDFile, processName,
      [&](const MemoryReport& aReport) {
        Unused << GetSingleton()->SendAddMemoryReport(aReport);
      },
      [&](const uint32_t& aGeneration) {
        return GetSingleton()->SendFinishMemoryReport(aGeneration);
      });
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
  MOZ_ASSERT(!mSocketProcessBridgeParentMap.Get(aContentProcessId, nullptr));

  mSocketProcessBridgeParentMap.Put(
      aContentProcessId, MakeRefPtr<SocketProcessBridgeParent>(
                             aContentProcessId, std::move(aEndpoint)));
  return IPC_OK();
}

mozilla::ipc::IPCResult SocketProcessChild::RecvInitProfiler(
    Endpoint<PProfilerChild>&& aEndpoint) {
#ifdef MOZ_GECKO_PROFILER
  mProfilerController =
      mozilla::ChildProfilerController::Create(std::move(aEndpoint));
#endif
  return IPC_OK();
}

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
SocketProcessChild::AllocPHttpConnectionMgrChild() {
  LOG(("SocketProcessChild::AllocPHttpConnectionMgrChild \n"));
  if (!gHttpHandler) {
    nsresult rv;
    nsCOMPtr<nsIIOService> ios = do_GetIOService(&rv);
    if (NS_FAILED(rv)) {
      return nullptr;
    }

    nsCOMPtr<nsIProtocolHandler> handler;
    rv = ios->GetProtocolHandler("http", getter_AddRefs(handler));
    if (NS_FAILED(rv)) {
      return nullptr;
    }

    // Initialize DNS Service here, since it needs to be done in main thread.
    nsCOMPtr<nsIDNSService> dns =
        do_GetService("@mozilla.org/network/dns-service;1", &rv);
    if (NS_FAILED(rv)) {
      return nullptr;
    }

    RefPtr<HttpConnectionMgrChild> actor = new HttpConnectionMgrChild();
    return actor.forget();
  }

  return nullptr;
}

mozilla::ipc::IPCResult
SocketProcessChild::RecvOnHttpActivityDistributorActivated(
    const bool& aIsActivated) {
  if (nsCOMPtr<nsIHttpActivityObserver> distributor =
          services::GetActivityDistributor()) {
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

}  // namespace net
}  // namespace mozilla
