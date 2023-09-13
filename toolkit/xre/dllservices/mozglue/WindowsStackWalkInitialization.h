/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_WindowsStackWalkInitialization_h
#define mozilla_WindowsStackWalkInitialization_h

#include "mozilla/Types.h"

namespace mozilla {

#if defined(_M_AMD64) || defined(_M_ARM64)
MFBT_API void WindowsStackWalkInitialization();
#endif  // _M_AMD64 || _M_ARM64

}  // namespace mozilla

#endif  // mozilla_WindowsStackWalkInitialization_h
