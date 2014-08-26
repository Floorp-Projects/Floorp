/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SandboxInternal_h
#define mozilla_SandboxInternal_h

// The code in Sandbox.cpp can't link against libxul, where
// SandboxCrash.cpp lives, so it has to use a callback, defined here.

#include <signal.h>

#include "mozilla/Types.h"

namespace mozilla {

typedef void (*SandboxCrashFunc)(int, siginfo_t*, void*);
extern MFBT_API SandboxCrashFunc gSandboxCrashFunc;

} // namespace mozilla

#endif // mozilla_SandboxInternal_h
