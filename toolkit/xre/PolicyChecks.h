/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_PolicyChecks_h
#define mozilla_PolicyChecks_h

#if defined(XP_WIN)

#include <windows.h>

// NB: This code must be able to run apart from XPCOM

namespace mozilla {

inline bool
PolicyHasRegValue(HKEY aKey, LPCWSTR aName, DWORD* aValue)
{
  DWORD len = sizeof(DWORD);
  LONG ret = ::RegGetValueW(aKey, L"SOFTWARE\\Policies\\Mozilla\\Firefox", aName,
                            RRF_RT_DWORD, nullptr, aValue, &len);
  return ret == ERROR_SUCCESS;
}

inline bool
PolicyCheckBoolean(LPCWSTR aPolicyName)
{
  DWORD value;
  if (PolicyHasRegValue(HKEY_LOCAL_MACHINE, aPolicyName, &value)) {
    return value == 1;
  }

  if (PolicyHasRegValue(HKEY_CURRENT_USER, aPolicyName, &value)) {
    return value == 1;
  }

  return false;
}

} // namespace mozilla

#endif // defined(XP_WIN)

#endif // mozilla_PolicyChecks_h
