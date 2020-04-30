/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef __REMOTE_SANDBOXBROKER_H__
#define __REMOTE_SANDBOXBROKER_H__

#include "sandboxBroker.h"
#include "RemoteSandboxBrokerParent.h"

namespace mozilla {

// To make sandboxing an x86 plugin-container process on Windows on ARM64,
// we launch an x86 child process which in turn launches and sandboxes the x86
// plugin-container child. This means the sandbox broker (in the remote
// x86 sandbox launcher process) can be same-arch with the process that it's
// sandboxing, which means all the sandbox's assumptions about things being
// same arch still hold.
class RemoteSandboxBroker : public AbstractSandboxBroker {
 public:
  RemoteSandboxBroker();

  void Shutdown() override;

  // Note: This should be called on the IPC launch thread, and this spins
  // the event loop. So this means potentially another IPC launch could occur
  // re-entrantly while calling this.
  bool LaunchApp(const wchar_t* aPath, const wchar_t* aArguments,
                 base::EnvironmentMap& aEnvironment,
                 GeckoProcessType aProcessType, const bool aEnableLogging,
                 const IMAGE_THUNK_DATA*, void** aProcessHandle) override;

  // Security levels for different types of processes
  void SetSecurityLevelForContentProcess(int32_t aSandboxLevel,
                                         bool aIsFileProcess) override;
  void SetSecurityLevelForGPUProcess(
      int32_t aSandboxLevel, const nsCOMPtr<nsIFile>& aProfileDir) override;
  bool SetSecurityLevelForRDDProcess() override;
  bool SetSecurityLevelForSocketProcess() override;
  bool SetSecurityLevelForPluginProcess(int32_t aSandboxLevel) override;
  bool SetSecurityLevelForGMPlugin(SandboxLevel aLevel,
                                   bool aIsRemoteLaunch = false) override;
  bool AllowReadFile(wchar_t const* file) override;
  void AddHandleToShare(HANDLE aHandle) override;

 private:
  virtual ~RemoteSandboxBroker();

  // Parameters that we use to launch the child process.
  LaunchParameters mParameters;

  RemoteSandboxBrokerParent mParent;

  // We bind the RemoteSandboxBrokerParent to the IPC launch thread.
  // As such, we must close its channel on the same thread. So we save
  // a reference to the IPC launch thread here.
  nsCOMPtr<nsIEventTarget> mIPCLaunchThread;

  // True if we've been shutdown.
  bool mShutdown = false;
};

}  // namespace mozilla

#endif  // __REMOTE_SANDBOXBROKER_H__
