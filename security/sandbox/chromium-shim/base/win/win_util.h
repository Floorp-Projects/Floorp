/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This is a partial implementation of Chromium's source file
// base/win/win_util.h

#ifndef BASE_WIN_WIN_UTIL_H_
#define BASE_WIN_WIN_UTIL_H_

#include <string>

#include "base/base_export.h"

namespace base {
namespace win {

// Returns the name of a desktop or a window station.
BASE_EXPORT std::wstring GetWindowObjectName(HANDLE handle);

}  // namespace win
}  // namespace base

#endif  // BASE_WIN_WIN_UTIL_H_
