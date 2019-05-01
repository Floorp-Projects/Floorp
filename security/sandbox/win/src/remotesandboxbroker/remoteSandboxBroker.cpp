/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "remoteSandboxBroker.h"

namespace mozilla {

RemoteSandboxBroker::RemoteSandboxBroker() {}

RemoteSandboxBroker::~RemoteSandboxBroker() {
  MOZ_ASSERT(
      mShutdown,
      "Shutdown must be called on RemoteSandboxBroker before destruction!");
}

void RemoteSandboxBroker::Shutdown() {
  MOZ_ASSERT(!mShutdown, "Don't call Shutdown() twice!");
  mShutdown = true;

  if (!mIPCLaunchThread) {
    // Can't have launched child process, nothing to shutdown.
    return;
  }

  RefPtr<RemoteSandboxBroker> self = this;
  mIPCLaunchThread->Dispatch(
      NS_NewRunnableFunction("Remote Sandbox Launch", [self, this]() {
        // Note: `self` here should be the last reference to this instance.
        mParent.Shutdown();
        mIPCLaunchThread = nullptr;
      }));
}

bool RemoteSandboxBroker::LaunchApp(const wchar_t* aPath,
                                    const wchar_t* aArguments,
                                    base::EnvironmentMap& aEnvironment,
                                    GeckoProcessType aProcessType,
                                    const bool aEnableLogging,
                                    void** aProcessHandle) {
  // Note: we expect to be called on the IPC launch thread from
  // GeckoChildProcesHost while it's launching a child process. We can't
  // run a synchronous launch here as that blocks the calling thread while
  // it dispatches a task to the IPC launch thread to spawn the process.
  // Since our calling thread is the IPC launch thread, we'd then be
  // deadlocked. So instead we do an async launch, and spin the event
  // loop until the process launch succeeds.

  // We should be on the IPC launch thread. We're shutdown on the IO thread,
  // so save a ref to the IPC launch thread here, so we can close the channel
  // on the IPC launch thread on shutdown.
  mIPCLaunchThread = GetCurrentThreadEventTarget();

  mParameters.path() = nsDependentString(aPath);
  mParameters.args() = nsDependentString(aArguments);

  auto toNsString = [](const std::wstring& s) {
    return nsDependentString(s.c_str());
  };
  for (auto itr : aEnvironment) {
    mParameters.env().AppendElement(
        EnvVar(toNsString(itr.first), toNsString(itr.second)));
  }

  mParameters.processType() = uint32_t(aProcessType);
  mParameters.enableLogging() = aEnableLogging;

  enum Result { Pending, Succeeded, Failed };
  Result res = Pending;
  auto resolve = [&](bool ok) {
    res = Succeeded;
    return GenericPromise::CreateAndResolve(ok, __func__);
  };

  auto reject = [&](nsresult) {
    res = Failed;
    return GenericPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  };

  mParent.Launch(mParameters.shareHandles())
      ->Then(GetCurrentThreadSerialEventTarget(), __func__, std::move(resolve),
             std::move(reject));

  // Spin the event loop while the sandbox launcher process launches.
  SpinEventLoopUntil([&]() { return res != Pending; });

  if (res == Failed) {
    return false;
  }

  uint64_t handle = 0;
  bool ok = false;
  bool rv = mParent.CallLaunchApp(std::move(mParameters), &ok, &handle) && ok;
  mParameters.shareHandles().Clear();
  if (!rv) {
    return false;
  }

  // Duplicate the handle of the child process that the launcher launched from
  // the launcher process's space into this process' space.
  HANDLE ourChildHandle = 0;
  bool dh = mParent.DuplicateFromLauncher((HANDLE)handle, &ourChildHandle);
  if (!dh) {
    return false;
  }

  *aProcessHandle = (void**)(ourChildHandle);

  base::ProcessHandle process = *aProcessHandle;
  SandboxBroker::AddTargetPeer(process);

  return true;
}

bool RemoteSandboxBroker::SetSecurityLevelForGMPlugin(SandboxLevel aLevel) {
  mParameters.sandboxLevel() = uint32_t(aLevel);
  return true;
}

bool RemoteSandboxBroker::AllowReadFile(wchar_t const* aFile) {
  mParameters.allowedReadFiles().AppendElement(nsDependentString(aFile));
  return true;
}

void RemoteSandboxBroker::AddHandleToShare(HANDLE aHandle) {
  mParameters.shareHandles().AppendElement(uint64_t(aHandle));
}

void RemoteSandboxBroker::SetSecurityLevelForContentProcess(
    int32_t aSandboxLevel, bool aIsFileProcess) {
  MOZ_CRASH(
      "RemoteSandboxBroker::SetSecurityLevelForContentProcess not Implemented");
}

void RemoteSandboxBroker::SetSecurityLevelForGPUProcess(int32_t aSandboxLevel) {
  MOZ_CRASH(
      "RemoteSandboxBroker::SetSecurityLevelForGPUProcess not Implemented");
}

bool RemoteSandboxBroker::SetSecurityLevelForRDDProcess() {
  MOZ_CRASH(
      "RemoteSandboxBroker::SetSecurityLevelForRDDProcess not Implemented");
}

bool RemoteSandboxBroker::SetSecurityLevelForPluginProcess(
    int32_t aSandboxLevel) {
  MOZ_CRASH(
      "RemoteSandboxBroker::SetSecurityLevelForPluginProcess not Implemented");
}

AbstractSandboxBroker* CreateRemoteSandboxBroker() {
  return new RemoteSandboxBroker();
}

}  // namespace mozilla
