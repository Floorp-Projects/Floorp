/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __SECURITY_SANDBOX_SANDBOXBROKER_H__
#define __SECURITY_SANDBOX_SANDBOXBROKER_H__

#include <stdint.h>
#include <windows.h>

#include "mozilla/ipc/EnvironmentMap.h"
#include "nsCOMPtr.h"
#include "nsXULAppAPI.h"
#include "nsISupportsImpl.h"

#include "mozilla/ipc/UtilityProcessSandboxing.h"
#include "mozilla/ipc/LaunchError.h"
#include "mozilla/Result.h"

namespace sandbox {
class BrokerServices;
class TargetPolicy;
}  // namespace sandbox

namespace mozilla {

class AbstractSandboxBroker {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(AbstractSandboxBroker)

  virtual void Shutdown() = 0;
  virtual Result<Ok, mozilla::ipc::LaunchError> LaunchApp(
      const wchar_t* aPath, const wchar_t* aArguments,
      base::EnvironmentMap& aEnvironment, GeckoProcessType aProcessType,
      const bool aEnableLogging, const IMAGE_THUNK_DATA* aCachedNtdllThunk,
      void** aProcessHandle) = 0;

  // Security levels for different types of processes
  virtual void SetSecurityLevelForContentProcess(int32_t aSandboxLevel,
                                                 bool aIsFileProcess) = 0;

  virtual void SetSecurityLevelForGPUProcess(int32_t aSandboxLevel) = 0;
  virtual bool SetSecurityLevelForRDDProcess() = 0;
  virtual bool SetSecurityLevelForSocketProcess() = 0;
  virtual bool SetSecurityLevelForUtilityProcess(
      mozilla::ipc::SandboxingKind aSandbox) = 0;

  enum SandboxLevel { LockDown, Restricted };
  virtual bool SetSecurityLevelForGMPlugin(SandboxLevel aLevel,
                                           bool aIsRemoteLaunch = false) = 0;

  // File system permissions
  virtual bool AllowReadFile(wchar_t const* file) = 0;

  /**
   * Share a HANDLE with the child process. The HANDLE will be made available
   * in the child process at the memory address
   * |reinterpret_cast<uintptr_t>(aHandle)|. It is the caller's responsibility
   * to communicate this address to the child.
   */
  virtual void AddHandleToShare(HANDLE aHandle) = 0;

  /**
   * @return true if policy has win32k locked down, otherwise false
   */
  virtual bool IsWin32kLockedDown() = 0;

 protected:
  virtual ~AbstractSandboxBroker() {}
};

class SandboxBroker : public AbstractSandboxBroker {
 public:
  SandboxBroker();

  static void Initialize(sandbox::BrokerServices* aBrokerServices);

  static void EnsureLpacPermsissionsOnDir(const nsString& aDir);

  void Shutdown() override {}

  /**
   * Do initialization that depends on parts of the Gecko machinery having been
   * created first.
   */
  static void GeckoDependentInitialize();

  Result<Ok, mozilla::ipc::LaunchError> LaunchApp(
      const wchar_t* aPath, const wchar_t* aArguments,
      base::EnvironmentMap& aEnvironment, GeckoProcessType aProcessType,
      const bool aEnableLogging, const IMAGE_THUNK_DATA* aCachedNtdllThunk,
      void** aProcessHandle) override;
  virtual ~SandboxBroker();

  // Security levels for different types of processes
  void SetSecurityLevelForContentProcess(int32_t aSandboxLevel,
                                         bool aIsFileProcess) override;

  void SetSecurityLevelForGPUProcess(int32_t aSandboxLevel) override;
  bool SetSecurityLevelForRDDProcess() override;
  bool SetSecurityLevelForSocketProcess() override;
  bool SetSecurityLevelForGMPlugin(SandboxLevel aLevel,
                                   bool aIsRemoteLaunch = false) override;
  bool SetSecurityLevelForUtilityProcess(
      mozilla::ipc::SandboxingKind aSandbox) override;

  // File system permissions
  bool AllowReadFile(wchar_t const* file) override;

  /**
   * Exposes AddTargetPeer from broker services, so that non-sandboxed
   * processes can be added as handle duplication targets.
   */
  static bool AddTargetPeer(HANDLE aPeerProcess);

  /**
   * Share a HANDLE with the child process. The HANDLE will be made available
   * in the child process at the memory address
   * |reinterpret_cast<uintptr_t>(aHandle)|. It is the caller's responsibility
   * to communicate this address to the child.
   */
  void AddHandleToShare(HANDLE aHandle) override;

  bool IsWin32kLockedDown() final;

  // Set up dummy interceptions via the broker, so we can log calls.
  void ApplyLoggingPolicy();

 private:
  static bool sRunningFromNetworkDrive;
  sandbox::TargetPolicy* mPolicy;
};

}  // namespace mozilla

#endif
