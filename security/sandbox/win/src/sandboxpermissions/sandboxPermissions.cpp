/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "sandboxPermissions.h"
#include "mozilla/Assertions.h"
#include "mozilla/sandboxing/permissionsService.h"

namespace mozilla
{

sandboxing::PermissionsService* SandboxPermissions::sPermissionsService = nullptr;

void
SandboxPermissions::Initialize(sandboxing::PermissionsService* aPermissionsService,
                               FileAccessViolationFunc aFileAccessViolationFunc)
{
  sPermissionsService = aPermissionsService;
  sPermissionsService->SetFileAccessViolationFunc(aFileAccessViolationFunc);
}

void
SandboxPermissions::GrantFileAccess(uint32_t aProcessId, const wchar_t* aFilename,
                                    bool aPermitWrite)
{
  MOZ_ASSERT(sPermissionsService, "Must initialize sandbox PermissionsService");
  sPermissionsService->GrantFileAccess(aProcessId, aFilename, aPermitWrite);
}

void
SandboxPermissions::RemovePermissionsForProcess(uint32_t aProcessId)
{
  if (!sPermissionsService) {
    return;   // No permissions service was initialized
  }
  sPermissionsService->RemovePermissionsForProcess(aProcessId);
}

}  // namespace mozilla
