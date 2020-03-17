/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SocketProcessHost.h"

#include "nsAppRunner.h"
#include "nsIObserverService.h"
#include "SocketProcessParent.h"

#if defined(XP_LINUX) && defined(MOZ_SANDBOX)
#  include "mozilla/SandboxBroker.h"
#  include "mozilla/SandboxBrokerPolicyFactory.h"
#endif

#ifdef MOZ_GECKO_PROFILER
#  include "ProfilerParent.h"
#endif

#if defined(XP_MACOSX) && defined(MOZ_SANDBOX)
#  include "mozilla/Sandbox.h"
#endif

namespace mozilla {
namespace net {

#define NS_IPC_IOSERVICE_SET_OFFLINE_TOPIC "ipc:network:set-offline"

class OfflineObserver final : public nsIObserver {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  explicit OfflineObserver(SocketProcessHost* aProcessHost)
      : mProcessHost(aProcessHost) {
    MOZ_ASSERT(NS_IsMainThread());

    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs) {
      obs->AddObserver(this, NS_IPC_IOSERVICE_SET_OFFLINE_TOPIC, false);
      obs->AddObserver(this, NS_XPCOM_WILL_SHUTDOWN_OBSERVER_ID, false);
    }
  }

  void Destroy() {
    MOZ_ASSERT(NS_IsMainThread());

    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs) {
      obs->RemoveObserver(this, NS_IPC_IOSERVICE_SET_OFFLINE_TOPIC);
    }
    mProcessHost = nullptr;
  }

 private:
  // nsIObserver implementation.
  NS_IMETHOD
  Observe(nsISupports* aSubject, const char* aTopic,
          const char16_t* aData) override {
    if (!mProcessHost) {
      return NS_OK;
    }

    if (!strcmp(aTopic, NS_IPC_IOSERVICE_SET_OFFLINE_TOPIC)) {
      NS_ConvertUTF16toUTF8 dataStr(aData);
      const char* offline = dataStr.get();
      if (!mProcessHost->IsConnected() ||
          mProcessHost->GetActor()->SendSetOffline(
              !strcmp(offline, "true") ? true : false)) {
        return NS_ERROR_NOT_AVAILABLE;
      }
    } else if (!strcmp(aTopic, NS_XPCOM_WILL_SHUTDOWN_OBSERVER_ID)) {
      nsCOMPtr<nsIObserverService> obs =
          mozilla::services::GetObserverService();
      obs->RemoveObserver(this, NS_IPC_IOSERVICE_SET_OFFLINE_TOPIC);
      obs->RemoveObserver(this, NS_XPCOM_WILL_SHUTDOWN_OBSERVER_ID);
    }

    return NS_OK;
  }
  virtual ~OfflineObserver() = default;

  SocketProcessHost* mProcessHost;
};

NS_IMPL_ISUPPORTS(OfflineObserver, nsIObserver)

#if defined(XP_MACOSX) && defined(MOZ_SANDBOX)
bool SocketProcessHost::sLaunchWithMacSandbox = false;
#endif

SocketProcessHost::SocketProcessHost(Listener* aListener)
    : GeckoChildProcessHost(GeckoProcessType_Socket),
      mListener(aListener),
      mTaskFactory(this),
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

SocketProcessHost::~SocketProcessHost() {
  MOZ_COUNT_DTOR(SocketProcessHost);
  if (mOfflineObserver) {
    RefPtr<OfflineObserver> observer = mOfflineObserver;
    NS_DispatchToMainThread(
        NS_NewRunnableFunction("SocketProcessHost::DestroyOfflineObserver",
                               [observer]() { observer->Destroy(); }));
  }
}

bool SocketProcessHost::Launch() {
  MOZ_ASSERT(mLaunchPhase == LaunchPhase::Unlaunched);
  MOZ_ASSERT(!mSocketProcessParent);
  MOZ_ASSERT(NS_IsMainThread());

  std::vector<std::string> extraArgs;

  nsAutoCString parentBuildID(mozilla::PlatformBuildID());
  extraArgs.push_back("-parentBuildID");
  extraArgs.push_back(parentBuildID.get());

  SharedPreferenceSerializer prefSerializer;
  if (!prefSerializer.SerializeToSharedMemory()) {
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

void SocketProcessHost::OnChannelConnected(int32_t peer_pid) {
  MOZ_ASSERT(!NS_IsMainThread());

  GeckoChildProcessHost::OnChannelConnected(peer_pid);

  // Post a task to the main thread. Take the lock because mTaskFactory is not
  // thread-safe.
  RefPtr<Runnable> runnable;
  {
    MonitorAutoLock lock(mMonitor);
    runnable = mTaskFactory.NewRunnableMethod(
        &SocketProcessHost::OnChannelConnectedTask);
  }
  NS_DispatchToMainThread(runnable);
}

void SocketProcessHost::OnChannelError() {
  MOZ_ASSERT(!NS_IsMainThread());
  GeckoChildProcessHost::OnChannelError();

  // Post a task to the main thread. Take the lock because mTaskFactory is not
  // thread-safe.
  RefPtr<Runnable> runnable;
  {
    MonitorAutoLock lock(mMonitor);
    runnable =
        mTaskFactory.NewRunnableMethod(&SocketProcessHost::OnChannelErrorTask);
  }
  NS_DispatchToMainThread(runnable);
}

void SocketProcessHost::OnChannelConnectedTask() {
  MOZ_ASSERT(NS_IsMainThread());

  if (mLaunchPhase == LaunchPhase::Waiting) {
    InitAfterConnect(true);
  }
}

void SocketProcessHost::OnChannelErrorTask() {
  MOZ_ASSERT(NS_IsMainThread());

  if (mLaunchPhase == LaunchPhase::Waiting) {
    InitAfterConnect(false);
  }
}

void SocketProcessHost::InitAfterConnect(bool aSucceeded) {
  MOZ_ASSERT(mLaunchPhase == LaunchPhase::Waiting);
  MOZ_ASSERT(!mSocketProcessParent);
  MOZ_ASSERT(NS_IsMainThread());

  mLaunchPhase = LaunchPhase::Complete;

  if (aSucceeded) {
    mSocketProcessParent = MakeUnique<SocketProcessParent>(this);
    DebugOnly<bool> rv = mSocketProcessParent->Open(
        TakeChannel(), base::GetProcId(GetChildProcessHandle()));
    MOZ_ASSERT(rv);

    nsCOMPtr<nsIIOService> ioService(do_GetIOService());
    MOZ_ASSERT(ioService, "No IO service?");
    bool offline = false;
    DebugOnly<nsresult> result = ioService->GetOffline(&offline);
    MOZ_ASSERT(NS_SUCCEEDED(result), "Failed getting offline?");

    Maybe<FileDescriptor> brokerFd;

#if defined(XP_LINUX) && defined(MOZ_SANDBOX)
    auto policy = SandboxBrokerPolicyFactory::GetSocketProcessPolicy(
        GetActor()->OtherPid());
    if (policy != nullptr) {
      brokerFd = Some(FileDescriptor());
      mSandboxBroker = SandboxBroker::Create(
          std::move(policy), GetActor()->OtherPid(), brokerFd.ref());
      // This is unlikely to fail and probably indicates OS resource
      // exhaustion.
      Unused << NS_WARN_IF(mSandboxBroker == nullptr);
      MOZ_ASSERT(brokerFd.ref().IsValid());
    }
    Unused << GetActor()->SendInitLinuxSandbox(brokerFd);
#endif  // XP_LINUX && MOZ_SANDBOX

#ifdef MOZ_GECKO_PROFILER
    Unused << GetActor()->SendInitProfiler(
        ProfilerParent::CreateForProcess(GetActor()->OtherPid()));
#endif

    Unused << GetActor()->SendSetOffline(offline);

    mOfflineObserver = new OfflineObserver(this);
  }

  if (mListener) {
    mListener->OnProcessLaunchComplete(this, aSucceeded);
  }
}

void SocketProcessHost::Shutdown() {
  MOZ_ASSERT(!mShutdownRequested);
  MOZ_ASSERT(NS_IsMainThread());

  mListener = nullptr;
  if (mOfflineObserver) {
    mOfflineObserver->Destroy();
    mOfflineObserver = nullptr;
  }

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
    mTaskFactory.RevokeAll();
  }

  MessageLoop::current()->PostTask(NS_NewRunnableFunction(
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
