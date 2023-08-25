/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_PolicyChecks_h
#define mozilla_PolicyChecks_h

#if defined(XP_WIN)

#  include <windows.h>
#  include "mozilla/Maybe.h"

#  define POLICY_REGKEY_NAME L"SOFTWARE\\Policies\\Ablaze\\" MOZ_APP_BASENAME

// NB: This code must be able to run apart from XPCOM

namespace mozilla {

// Returns Some(true) if the registry value is 1
// Returns Some(false) if the registry value is not 1
// Returns Nothing() if the registry value is not present
inline Maybe<bool> PolicyHasRegValueOfOne(HKEY aKey, LPCWSTR aName) {
  {
    DWORD len = sizeof(DWORD);
    DWORD value;
    LONG ret = ::RegGetValueW(aKey, POLICY_REGKEY_NAME, aName, RRF_RT_DWORD,
                              nullptr, &value, &len);
    if (ret == ERROR_SUCCESS) {
      return Some(value == 1);
    }
  }
  {
    ULONGLONG value;
    DWORD len = sizeof(ULONGLONG);
    LONG ret = ::RegGetValueW(aKey, POLICY_REGKEY_NAME, aName, RRF_RT_QWORD,
                              nullptr, &value, &len);
    if (ret == ERROR_SUCCESS) {
      return Some(value == 1);
    }
  }
  return Nothing();
}

inline bool PolicyCheckBoolean(LPCWSTR aPolicyName) {
  Maybe<bool> localMachineResult =
      PolicyHasRegValueOfOne(HKEY_LOCAL_MACHINE, aPolicyName);
  if (localMachineResult.isSome()) {
    return localMachineResult.value();
  }

  Maybe<bool> currentUserResult =
      PolicyHasRegValueOfOne(HKEY_CURRENT_USER, aPolicyName);
  if (currentUserResult.isSome()) {
    return currentUserResult.value();
  }

  return false;
}

}  // namespace mozilla

#endif  // defined(XP_WIN)

#endif  // mozilla_PolicyChecks_h
