/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "RemoteSandboxBrokerChild.h"
#include "mozilla/ipc/CrashReporterClient.h"
#include "nsDebugImpl.h"
#include "mozilla/ipc/CrashReporterClient.h"
#include "RemoteSandboxBrokerProcessChild.h"

using namespace mozilla::ipc;

namespace mozilla {

RemoteSandboxBrokerChild::RemoteSandboxBrokerChild() {
  nsDebugImpl::SetMultiprocessMode("RemoteSandboxBroker");
}

RemoteSandboxBrokerChild::~RemoteSandboxBrokerChild() {}

bool RemoteSandboxBrokerChild::Init(base::ProcessId aParentPid,
                                    MessageLoop* aIOLoop,
                                    IPC::Channel* aChannel) {
  if (NS_WARN_IF(!Open(aChannel, aParentPid, aIOLoop))) {
    return false;
  }
  CrashReporterClient::InitSingleton(this);
  return true;
}

void RemoteSandboxBrokerChild::ActorDestroy(ActorDestroyReason aWhy) {
  if (AbnormalShutdown == aWhy) {
    NS_WARNING("Abnormal shutdown of GMP process!");
    ipc::ProcessChild::QuickExit();
  }
  CrashReporterClient::DestroySingleton();
  XRE_ShutdownChildProcess();
}

mozilla::ipc::IPCResult RemoteSandboxBrokerChild::AnswerLaunchApp(
    LaunchParameters&& aParams, bool* aOutOk, uint64_t* aOutHandle) {
  auto towstring = [](const nsString& s) {
    return std::wstring(s.get(), s.Length());
  };

  base::EnvironmentMap envmap;
  for (const EnvVar& env : aParams.env()) {
    envmap[towstring(env.name())] = towstring(env.value());
  }

  if (!mSandboxBroker.SetSecurityLevelForGMPlugin(
          AbstractSandboxBroker::SandboxLevel(aParams.sandboxLevel()))) {
    *aOutOk = false;
    return IPC_OK();
  }

  for (const auto& path : aParams.allowedReadFiles()) {
    if (!mSandboxBroker.AllowReadFile(path.get())) {
      *aOutOk = false;
      return IPC_OK();
    }
  }

  for (const auto& handle : aParams.shareHandles()) {
    mSandboxBroker.AddHandleToShare(HANDLE(handle));
  }

  HANDLE p;
  *aOutOk =
      mSandboxBroker.LaunchApp(aParams.path().get(), aParams.args().get(),
                               envmap, GeckoProcessType(aParams.processType()),
                               aParams.enableLogging(), (void**)&p);
  if (*aOutOk) {
    *aOutHandle = uint64_t(p);
  }

  for (const auto& handle : aParams.shareHandles()) {
    CloseHandle(HANDLE(handle));
  }

  return IPC_OK();
}

}  // namespace mozilla
