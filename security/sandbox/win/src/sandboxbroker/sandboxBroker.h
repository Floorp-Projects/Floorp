/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __SECURITY_SANDBOX_SANDBOXBROKER_H__
#define __SECURITY_SANDBOX_SANDBOXBROKER_H__

#ifdef SANDBOX_EXPORTS
#define SANDBOX_EXPORT __declspec(dllexport)
#else
#define SANDBOX_EXPORT __declspec(dllimport)
#endif

#include <stdint.h>
#include <windows.h>

namespace sandbox {
  class BrokerServices;
  class TargetPolicy;
}

namespace mozilla {

class SANDBOX_EXPORT SandboxBroker
{
public:
  SandboxBroker();
  bool LaunchApp(const wchar_t *aPath,
                 const wchar_t *aArguments,
                 const bool aEnableLogging,
                 void **aProcessHandle);
  virtual ~SandboxBroker();

  // Security levels for different types of processes
#if defined(MOZ_CONTENT_SANDBOX)
  bool SetSecurityLevelForContentProcess(int32_t aSandboxLevel);
#endif
  bool SetSecurityLevelForPluginProcess(int32_t aSandboxLevel);
  bool SetSecurityLevelForIPDLUnitTestProcess();
  bool SetSecurityLevelForGMPlugin();

  // File system permissions
  bool AllowReadFile(wchar_t const *file);
  bool AllowReadWriteFile(wchar_t const *file);
  bool AllowDirectory(wchar_t const *dir);

  // Exposes AddTargetPeer from broker services, so that none sandboxed
  // processes can be added as handle duplication targets.
  bool AddTargetPeer(HANDLE aPeerProcess);
private:
  static sandbox::BrokerServices *sBrokerService;
  sandbox::TargetPolicy *mPolicy;
};

} // mozilla

#endif
