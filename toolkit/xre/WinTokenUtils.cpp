/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "WinTokenUtils.h"

using namespace mozilla;

// If |aToken| is nullptr, CheckTokenMembership uses the calling thread's
// primary token to check membership for.
static LauncherResult<bool> IsMemberOfSidType(
    const nsAutoHandle& aToken, const WELL_KNOWN_SID_TYPE aWellKnownSid) {
  BYTE sid[SECURITY_MAX_SID_SIZE];
  DWORD sidSize = sizeof(sid);
  if (!CreateWellKnownSid(aWellKnownSid, nullptr, sid, &sidSize)) {
    return LAUNCHER_ERROR_FROM_LAST();
  }

  BOOL isMember;
  if (!CheckTokenMembership(aToken, sid, &isMember)) {
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
  // To check whether the process was launched with Administrator privileges or
  // not, we cannot simply check the integrity level of the current process
  // because the launcher process spawns the browser process with the medium
  // integrity level even though the launcher process is high integrity level.
  // We check whether the thread's token contains Administrators SID or not
  // instead.
  return UserHasAdminPrivileges().andThen(
      [](bool containsAdminGroup) -> LauncherResult<bool> {
        if (!containsAdminGroup) {
          // We don't have Administrator privileges, no need to check if UAC is
          // enabled.
          return false;
        }

        // We have Administrator privileges, now check if we have them while UAC
        // is disabled.
        return IsUacEnabled().map(
            [](bool isUacEnabled) { return !isUacEnabled; });
      });
}

LauncherResult<bool> UserHasAdminPrivileges() {
  return IsMemberOfSidType(nsAutoHandle(), WinBuiltinAdministratorsSid);
}

LauncherResult<bool> UserIsLocalSystem() {
  return IsMemberOfSidType(nsAutoHandle(), WinLocalSystemSid);
}

}  // namespace mozilla
