/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_WindowsBCryptInitialization_h
#define mozilla_WindowsBCryptInitialization_h

#include "mozilla/Types.h"

namespace mozilla {

// This functions ensures that calling BCryptGenRandom will work later. It
// triggers a first call to BCryptGenRandom() to pre-load bcryptPrimitives.dll.
// In sandboxed processes, this must happen while the current thread still has
// an unrestricted impersonation token. We need to perform that operation to
// warmup the BCryptGenRandom() calls is used by others, especially Rust. See
// bug 1746524, bug 1751094, bug 1751177, bug 1788004.
MFBT_API bool WindowsBCryptInitialization();

}  // namespace mozilla

#endif  // mozilla_WindowsBCryptInitialization_h
