/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This is a stripped down version of Chromium source file base/win/registry.h
// Within our copy of Chromium files this is only used in base/win/windows_version.cc
// in OSInfo::processor_model_name, which we don't use.

#ifndef BASE_WIN_REGISTRY_H_
#define BASE_WIN_REGISTRY_H_

namespace base {
namespace win {

class BASE_EXPORT RegKey {
 public:
  RegKey(HKEY rootkey, const wchar_t* subkey, REGSAM access) {}
  ~RegKey() {}

  LONG ReadValue(const wchar_t* name, std::wstring* out_value) const
  {
    return 0;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(RegKey);
};

}  // namespace win
}  // namespace base

#endif  // BASE_WIN_REGISTRY_H_
