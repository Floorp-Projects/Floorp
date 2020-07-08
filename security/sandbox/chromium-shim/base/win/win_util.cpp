/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This is a partial implementation of Chromium's source file
// base/win/win_util.cc

#include "base/win/win_util.h"

#include "base/logging.h"
#include "base/strings/string_util.h"

namespace base {
namespace win {

std::wstring GetWindowObjectName(HANDLE handle) {
  // Get the size of the name.
  std::wstring object_name;

  DWORD size = 0;
  ::GetUserObjectInformation(handle, UOI_NAME, nullptr, 0, &size);
  if (!size) {
    DPCHECK(false);
    return object_name;
  }

  LOG_ASSERT(size % sizeof(wchar_t) == 0u);

  // Query the name of the object.
  if (!::GetUserObjectInformation(
          handle, UOI_NAME, WriteInto(&object_name, size / sizeof(wchar_t)),
          size, &size)) {
    DPCHECK(false);
  }

  return object_name;
}

}  // namespace win
}  // namespace base
