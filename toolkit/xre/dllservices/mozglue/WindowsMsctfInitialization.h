/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_WindowsMsctfInitialization_h
#define mozilla_WindowsMsctfInitialization_h

#include "mozilla/Types.h"

namespace mozilla {

#if defined(_M_IX86) || defined(_M_X64)

// This functions patches msctf.dll to prevent a crash with ZoneAlarm
// Anti-Keylogger on Windows 11 (bug 1777960). It needs to be called in the
// main process before any keyboard event is processed.
MFBT_API bool WindowsMsctfInitialization();

#endif  // _M_IX86 || _M_X64

}  // namespace mozilla

#endif  // mozilla_WindowsMsctfInitialization_h
