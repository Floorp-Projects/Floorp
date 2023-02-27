/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_WindowsBCryptInitialization_h
#define mozilla_WindowsBCryptInitialization_h

#include "mozilla/Types.h"

namespace mozilla {

// This functions ensures that calling BCryptGenRandom will work later:
//  - It triggers a first call to BCryptGenRandom() to pre-load
//    bcryptPrimitives.dll while the current thread still has an unrestricted
//    impersonation token. We need to perform that operation in sandboxed
//    processes to warmup the BCryptGenRandom() call that is used by others,
//    especially Rust. See bug 1746524, bug 1751094, bug 1751177.
//  - If that first call fails, we detect it and hook BCryptGenRandom to
//    install a fallback based on RtlGenRandom for calls that use flag
//    BCRYPT_USE_SYSTEM_PREFERRED_RNG. We need this because BCryptGenRandom
//    failures are currently fatal and on some machines BCryptGenRandom is
//    broken (usually Windows 7). We hope to remove this hook in the future
//    once the Rust stdlib and the getrandom crate both have their own
//    RtlGenRandom-based fallback. See bug 1788004.
MFBT_API bool WindowsBCryptInitialization();

}  // namespace mozilla

#endif  // mozilla_WindowsBCryptInitialization_h
