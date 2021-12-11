/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This is a stripped down version of Chromium source file base/win/registry.h
// Within our copy of Chromium files this is only used in base/win/windows_version.cc
// in OSInfo::processor_model_name, which we don't use.
// It is also used in GetUBR, which is used as the VersionNumber.patch, which
// again is not needed by the sandbox.

#ifndef BASE_WIN_REGISTRY_H_
#define BASE_WIN_REGISTRY_H_

#include <winerror.h>

namespace base {
namespace win {

class BASE_EXPORT RegKey {
 public:
  RegKey() {};
  RegKey(HKEY rootkey, const wchar_t* subkey, REGSAM access) {}
  ~RegKey() {}

  LONG Open(HKEY rootkey, const wchar_t* subkey, REGSAM access) {
    return ERROR_CANTOPEN;
  }

  LONG ReadValueDW(const wchar_t* name, DWORD* out_value) const
  {
    return ERROR_CANTREAD;
  }

  LONG ReadValue(const wchar_t* name, std::wstring* out_value) const
  {
    return ERROR_CANTREAD;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(RegKey);
};

}  // namespace win
}  // namespace base

#endif  // BASE_WIN_REGISTRY_H_
