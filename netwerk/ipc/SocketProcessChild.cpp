/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SocketProcessChild.h"
#include "SocketProcessLogging.h"

#include "base/task.h"
#include "mozilla/Assertions.h"
#include "mozilla/dom/MemoryReportRequest.h"
#include "mozilla/ipc/CrashReporterClient.h"
#include "mozilla/ipc/ProcessChild.h"
#include "mozilla/Preferences.h"
#include "nsDebugImpl.h"
#include "nsThreadManager.h"
#include "ProcessUtils.h"
#include "SocketProcessBridgeParent.h"

#ifdef MOZ_GECKO_PROFILER
#  include "ChildProfilerController.h"
#endif

#ifdef MOZ_WEBRTC
#  include "mozilla/net/WebrtcProxyChannelChild.h"
#endif

namespace mozilla {
namespace net {

using namespace ipc;

static SocketProcessChild* sSocketProcessChild;

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

bool SocketProcessChild::Init(base::ProcessId aParentPid,
                              const char* aParentBuildID, MessageLoop* aIOLoop,
                              IPC::Channel* aChannel) {
  if (NS_WARN_IF(NS_FAILED(nsThreadManager::get().Init()))) {
    return false;
  }
  if (NS_WARN_IF(!Open(aChannel, aParentPid, aIOLoop))) {
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

  SetThisProcessName("Socket Process");
  return true;
}

void SocketProcessChild::ActorDestroy(ActorDestroyReason aWhy) {
  LOG(("SocketProcessChild::ActorDestroy\n"));

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

mozilla::ipc::IPCResult SocketProcessChild::RecvInitSocketProcessBridgeParent(
    const ProcessId& aContentProcessId,
    Endpoint<mozilla::net::PSocketProcessBridgeParent>&& aEndpoint) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mSocketProcessBridgeParentMap.Get(aContentProcessId, nullptr));

  mSocketProcessBridgeParentMap.Put(
      aContentProcessId,
      new SocketProcessBridgeParent(aContentProcessId, std::move(aEndpoint)));
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

PWebrtcProxyChannelChild* SocketProcessChild::AllocPWebrtcProxyChannelChild(
    const PBrowserOrId& browser) {
  // We don't allocate here: instead we always use IPDL constructor that takes
  // an existing object
  MOZ_ASSERT_UNREACHABLE(
      "AllocPWebrtcProxyChannelChild should not be called on"
      " socket child");
  return nullptr;
}

bool SocketProcessChild::DeallocPWebrtcProxyChannelChild(
    PWebrtcProxyChannelChild* aActor) {
#ifdef MOZ_WEBRTC
  WebrtcProxyChannelChild* child =
      static_cast<WebrtcProxyChannelChild*>(aActor);
  child->ReleaseIPDLReference();
#endif
  return true;
}

}  // namespace net
}  // namespace mozilla
