/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "WinTokenUtils.h"
#include "nsWindowsHelpers.h"

using namespace mozilla;

// If |aToken| is nullptr, CheckTokenMembership uses the calling thread's
// primary token to check membership for.
static LauncherResult<bool> IsMemberOfAdministrators(
    const nsAutoHandle& aToken) {
  BYTE adminsGroupSid[SECURITY_MAX_SID_SIZE];
  DWORD adminsGroupSidSize = sizeof(adminsGroupSid);
  if (!CreateWellKnownSid(WinBuiltinAdministratorsSid, nullptr, adminsGroupSid,
                          &adminsGroupSidSize)) {
    return LAUNCHER_ERROR_FROM_LAST();
  }

  BOOL isMember;
  if (!CheckTokenMembership(aToken, adminsGroupSid, &isMember)) {
    return LAUNCHER_ERROR_FROM_LAST();
  }
  return !!isMember;
}

static LauncherResult<bool> IsUacEnabled() {
  DWORD len = sizeof(DWORD);
  DWORD value;
  LSTATUS status = RegGetValueW(
      HKEY_LOCAL_MACHINE,
      L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System",
      L"EnableLUA", RRF_RT_DWORD, nullptr, &value, &len);
  if (status != ERROR_SUCCESS) {
    return LAUNCHER_ERROR_FROM_WIN32(status);
  }

  // UAC is disabled only when EnableLUA is 0.
  return (value != 0);
}

namespace mozilla {

LauncherResult<bool> IsAdminWithoutUac() {
  // To check whether the process was launched with Administrator priviledges
  // or not, we cannot simply check the integrity level of the current process
  // because the launcher process spawns the browser process with the medium
  // integrity level even though the launcher process is high integrity level.
  // We check whether the thread's token contains Administratos SID or not
  // instead.
  LauncherResult<bool> containsAdminGroup =
      IsMemberOfAdministrators(nsAutoHandle());
  if (containsAdminGroup.isErr()) {
    return containsAdminGroup.propagateErr();
  }

  if (!containsAdminGroup.unwrap()) {
    return false;
  }

  LauncherResult<bool> isUacEnabled = IsUacEnabled();
  if (isUacEnabled.isErr()) {
    return isUacEnabled.propagateErr();
  }

  return !isUacEnabled.unwrap();
}

}  // namespace mozilla
