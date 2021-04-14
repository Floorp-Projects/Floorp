/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SandboxInternal_h
#define mozilla_SandboxInternal_h

#include <signal.h>

#include "mozilla/Types.h"

struct sock_fprog;

namespace mozilla {

// SandboxCrash() has to be in libxul to use internal interfaces, but
// its caller in libmozsandbox.
// See also bug 1101170.

typedef void (*SandboxCrashFunc)(int, siginfo_t*, void*, const void*);
extern MOZ_EXPORT SandboxCrashFunc gSandboxCrashFunc;
extern const sock_fprog* gSetSandboxFilter;

}  // namespace mozilla

#endif  // mozilla_SandboxInternal_h
