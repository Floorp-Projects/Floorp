/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SocketProcessHost.h"

#include "SocketProcessParent.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/ipc/FileDescriptor.h"
#include "mozilla/ipc/ProcessUtils.h"
#include "nsAppRunner.h"
#include "nsIOService.h"
#include "nsIObserverService.h"
#include "ProfilerParent.h"
#include "nsNetUtil.h"
#include "mozilla/ipc/Endpoint.h"
#include "mozilla/ipc/ProcessChild.h"

#if defined(XP_LINUX) && defined(MOZ_SANDBOX)
#  include "mozilla/SandboxBroker.h"
#  include "mozilla/SandboxBrokerPolicyFactory.h"
#  include "mozilla/SandboxSettings.h"
#endif

#if defined(XP_MACOSX) && defined(MOZ_SANDBOX)
#  include "mozilla/Sandbox.h"
#endif

#if defined(XP_WIN)
#  include "mozilla/WinDllServices.h"
#endif

using namespace mozilla::ipc;

namespace mozilla {
namespace net {

#if defined(XP_MACOSX) && defined(MOZ_SANDBOX)
bool SocketProcessHost::sLaunchWithMacSandbox = false;
#endif

SocketProcessHost::SocketProcessHost(Listener* aListener)
    : GeckoChildProcessHost(GeckoProcessType_Socket),
      mListener(aListener),
      mTaskFactory(Some(this)),
      mLaunchPhase(LaunchPhase::Unlaunched),
      mShutdownRequested(false),
      mChannelClosed(false) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_COUNT_CTOR(SocketProcessHost);
#if defined(XP_MACOSX) && defined(MOZ_SANDBOX)
  if (!sLaunchWithMacSandbox) {
    sLaunchWithMacSandbox =
        (PR_GetEnv("MOZ_DISABLE_SOCKET_PROCESS_SANDBOX") == nullptr);
  }
  mDisableOSActivityMode = sLaunchWithMacSandbox;
#endif
}

SocketProcessHost::~SocketProcessHost() { MOZ_COUNT_DTOR(SocketProcessHost); }

bool SocketProcessHost::Launch() {
  MOZ_ASSERT(mLaunchPhase == LaunchPhase::Unlaunched);
  MOZ_ASSERT(!mSocketProcessParent);
  MOZ_ASSERT(NS_IsMainThread());

  std::vector<std::string> extraArgs;
  ProcessChild::AddPlatformBuildID(extraArgs);

  SharedPreferenceSerializer prefSerializer;
  if (!prefSerializer.SerializeToSharedMemory(GeckoProcessType_VR,
                                              /* remoteType */ ""_ns)) {
    return false;
  }
  prefSerializer.AddSharedPrefCmdLineArgs(*this, extraArgs);

  mLaunchPhase = LaunchPhase::Waiting;
  if (!GeckoChildProcessHost::LaunchAndWaitForProcessHandle(extraArgs)) {
    mLaunchPhase = LaunchPhase::Complete;
    return false;
  }

  return true;
}

static void HandleErrorAfterDestroy(
    RefPtr<SocketProcessHost::Listener>&& aListener) {
  if (!aListener) {
    return;
  }

  NS_DispatchToMainThread(NS_NewRunnableFunction(
      "HandleErrorAfterDestroy", [listener = std::move(aListener)]() {
        listener->OnProcessLaunchComplete(nullptr, false);
      }));
}

void SocketProcessHost::OnChannelConnected(base::ProcessId peer_pid) {
  MOZ_ASSERT(!NS_IsMainThread());

  GeckoChildProcessHost::OnChannelConnected(peer_pid);

  // Post a task to the main thread. Take the lock because mTaskFactory is not
  // thread-safe.
  RefPtr<Runnable> runnable;
  {
    MonitorAutoLock lock(mMonitor);
    if (!mTaskFactory) {
      HandleErrorAfterDestroy(std::move(mListener));
      return;
    }
    runnable =
        (*mTaskFactory)
            .NewRunnableMethod(&SocketProcessHost::OnChannelConnectedTask);
  }
  NS_DispatchToMainThread(runnable);
}

void SocketProcessHost::OnChannelConnectedTask() {
  MOZ_ASSERT(NS_IsMainThread());

  if (mLaunchPhase == LaunchPhase::Waiting) {
    InitAfterConnect(true);
  }
}

void SocketProcessHost::InitAfterConnect(bool aSucceeded) {
  MOZ_ASSERT(mLaunchPhase == LaunchPhase::Waiting);
  MOZ_ASSERT(!mSocketProcessParent);
  MOZ_ASSERT(NS_IsMainThread());

  mLaunchPhase = LaunchPhase::Complete;
  if (!aSucceeded) {
    if (mListener) {
      mListener->OnProcessLaunchComplete(this, false);
    }
    return;
  }

  mSocketProcessParent = MakeRefPtr<SocketProcessParent>(this);
  DebugOnly<bool> rv = TakeInitialEndpoint().Bind(mSocketProcessParent.get());
  MOZ_ASSERT(rv);

  SocketPorcessInitAttributes attributes;
  nsCOMPtr<nsIIOService> ioService(do_GetIOService());
  MOZ_ASSERT(ioService, "No IO service?");
  DebugOnly<nsresult> result = ioService->GetOffline(&attributes.mOffline());
  MOZ_ASSERT(NS_SUCCEEDED(result), "Failed getting offline?");
  result = ioService->GetConnectivity(&attributes.mConnectivity());
  MOZ_ASSERT(NS_SUCCEEDED(result), "Failed getting connectivity?");

  attributes.mInitSandbox() = false;

#if defined(XP_WIN)
  RefPtr<DllServices> dllSvc(DllServices::Get());
  attributes.mIsReadyForBackgroundProcessing() =
      dllSvc->IsReadyForBackgroundProcessing();
#endif

#if defined(XP_LINUX) && defined(MOZ_SANDBOX)
  if (GetEffectiveSocketProcessSandboxLevel() > 0) {
    auto policy = SandboxBrokerPolicyFactory::GetSocketProcessPolicy(
        GetActor()->OtherPid());
    if (policy != nullptr) {
      attributes.mSandboxBroker() = Some(FileDescriptor());
      mSandboxBroker =
          SandboxBroker::Create(std::move(policy), GetActor()->OtherPid(),
                                attributes.mSandboxBroker().ref());
      // This is unlikely to fail and probably indicates OS resource
      // exhaustion.
      Unused << NS_WARN_IF(mSandboxBroker == nullptr);
      MOZ_ASSERT(attributes.mSandboxBroker().ref().IsValid());
    }
    attributes.mInitSandbox() = true;
  }
#endif  // XP_LINUX && MOZ_SANDBOX

  Unused << GetActor()->SendInit(attributes);

  Unused << GetActor()->SendInitProfiler(
      ProfilerParent::CreateForProcess(GetActor()->OtherPid()));

  if (mListener) {
    mListener->OnProcessLaunchComplete(this, true);
  }
}

void SocketProcessHost::Shutdown() {
  MOZ_ASSERT(!mShutdownRequested);
  MOZ_ASSERT(NS_IsMainThread());

  mListener = nullptr;

  if (mSocketProcessParent) {
    // OnChannelClosed uses this to check if the shutdown was expected or
    // unexpected.
    mShutdownRequested = true;

    // The channel might already be closed if we got here unexpectedly.
    if (!mChannelClosed) {
      mSocketProcessParent->Close();
    }

    return;
  }

  DestroyProcess();
}

void SocketProcessHost::OnChannelClosed() {
  MOZ_ASSERT(NS_IsMainThread());

  mChannelClosed = true;

  if (!mShutdownRequested && mListener) {
    // This is an unclean shutdown. Notify our listener that we're going away.
    mListener->OnProcessUnexpectedShutdown(this);
  } else {
    DestroyProcess();
  }

  // Release the actor.
  SocketProcessParent::Destroy(std::move(mSocketProcessParent));
  MOZ_ASSERT(!mSocketProcessParent);
}

void SocketProcessHost::DestroyProcess() {
  {
    MonitorAutoLock lock(mMonitor);
    mTaskFactory.reset();
  }

  GetCurrentSerialEventTarget()->Dispatch(NS_NewRunnableFunction(
      "DestroySocketProcessRunnable", [this] { Destroy(); }));
}

#if defined(XP_MACOSX) && defined(MOZ_SANDBOX)
bool SocketProcessHost::FillMacSandboxInfo(MacSandboxInfo& aInfo) {
  GeckoChildProcessHost::FillMacSandboxInfo(aInfo);
  if (!aInfo.shouldLog && PR_GetEnv("MOZ_SANDBOX_SOCKET_PROCESS_LOGGING")) {
    aInfo.shouldLog = true;
  }
  return true;
}

/* static */
MacSandboxType SocketProcessHost::GetMacSandboxType() {
  return MacSandboxType_Socket;
}
#endif

//-----------------------------------------------------------------------------
// SocketProcessMemoryReporter
//-----------------------------------------------------------------------------

bool SocketProcessMemoryReporter::IsAlive() const {
  MOZ_ASSERT(gIOService);

  if (!gIOService->mSocketProcess) {
    return false;
  }

  return gIOService->mSocketProcess->IsConnected();
}

bool SocketProcessMemoryReporter::SendRequestMemoryReport(
    const uint32_t& aGeneration, const bool& aAnonymize,
    const bool& aMinimizeMemoryUsage,
    const Maybe<ipc::FileDescriptor>& aDMDFile) {
  MOZ_ASSERT(gIOService);

  if (!gIOService->mSocketProcess) {
    return false;
  }

  SocketProcessParent* actor = gIOService->mSocketProcess->GetActor();
  if (!actor) {
    return false;
  }

  return actor->SendRequestMemoryReport(aGeneration, aAnonymize,
                                        aMinimizeMemoryUsage, aDMDFile);
}

int32_t SocketProcessMemoryReporter::Pid() const {
  MOZ_ASSERT(gIOService);
  return gIOService->SocketProcessPid();
}

}  // namespace net
}  // namespace mozilla
