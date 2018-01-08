/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __SECURITY_SANDBOX_SANDBOXBROKER_H__
#define __SECURITY_SANDBOX_SANDBOXBROKER_H__

#include <stdint.h>
#include <windows.h>

#include "build/build_config.h"
#include "mozilla/ipc/EnvironmentMap.h"
#include "nsXULAppAPI.h"

namespace sandbox {
  class BrokerServices;
  class TargetPolicy;
}

namespace mozilla {

class SandboxBroker
{
public:
  SandboxBroker();

  static void Initialize(sandbox::BrokerServices* aBrokerServices);

  /**
   * Do initialization that depends on parts of the Gecko machinery having been
   * created first.
   */
  static void GeckoDependentInitialize();

  bool LaunchApp(const wchar_t *aPath,
                 const wchar_t *aArguments,
                 base::EnvironmentMap& aEnvironment,
                 GeckoProcessType aProcessType,
                 const bool aEnableLogging,
                 void **aProcessHandle);
  virtual ~SandboxBroker();

  // Security levels for different types of processes
#if defined(MOZ_CONTENT_SANDBOX)
  void SetSecurityLevelForContentProcess(int32_t aSandboxLevel,
                                         bool aIsFileProcess);
#endif

  void SetSecurityLevelForGPUProcess(int32_t aSandboxLevel);

  bool SetSecurityLevelForPluginProcess(int32_t aSandboxLevel);
#ifdef MOZ_ENABLE_SKIA_PDF
  bool SetSecurityLevelForPDFiumProcess();
#endif
  enum SandboxLevel {
    LockDown,
    Restricted
  };
  bool SetSecurityLevelForGMPlugin(SandboxLevel aLevel);

  // File system permissions
  bool AllowReadFile(wchar_t const *file);

  // Exposes AddTargetPeer from broker services, so that none sandboxed
  // processes can be added as handle duplication targets.
  bool AddTargetPeer(HANDLE aPeerProcess);

  // Set up dummy interceptions via the broker, so we can log calls.
  void ApplyLoggingPolicy();

private:
  static sandbox::BrokerServices *sBrokerService;
  static bool sRunningFromNetworkDrive;
  sandbox::TargetPolicy *mPolicy;
};

} // mozilla

#endif
