/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "sandboxBroker.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_factory.h"

namespace mozilla
{

sandbox::BrokerServices *SandboxBroker::sBrokerService = nullptr;

SandboxBroker::SandboxBroker()
{
  if (!sBrokerService) {
    sBrokerService = sandbox::SandboxFactory::GetBrokerServices();
    if (sBrokerService) {
      sandbox::ResultCode result = sBrokerService->Init();
      if (result != sandbox::SBOX_ALL_OK) {
        sBrokerService = nullptr;
      }
    }
  }
}

bool
SandboxBroker::LaunchApp(const wchar_t *aPath,
                           const wchar_t *aArguments,
                           void **aProcessHandle)
{
  // If the broker service isn't already initialized, do it now
  if (!sBrokerService) {
    return false;
  }

  // Setup the sandbox policy, this is initially:
  // Medium integrity, unrestricted, in the same window station, within the
  // same desktop, and has no job object.
  // We'll start to increase the restrictions over time.
  sandbox::TargetPolicy *policy = sBrokerService->CreatePolicy();
  policy->SetJobLevel(sandbox::JOB_NONE, 0);
  policy->SetTokenLevel(sandbox::USER_RESTRICTED_SAME_ACCESS,
                        sandbox::USER_RESTRICTED_SAME_ACCESS);
  policy->SetDelayedIntegrityLevel(sandbox::INTEGRITY_LEVEL_MEDIUM);

  // Ceate the sandboxed process
  PROCESS_INFORMATION targetInfo;
  sandbox::ResultCode result;
  result = sBrokerService->SpawnTarget(aPath, aArguments, policy, &targetInfo);

  // The sandboxed process is started in a suspended state, resumeit now that
  // we'eve set things up.
  ResumeThread(targetInfo.hThread);
  CloseHandle(targetInfo.hThread);

  // Return the process handle to the caller
  *aProcessHandle = targetInfo.hProcess;

  policy->Release();

  return true;
}

SandboxBroker::~SandboxBroker()
{
}

}
