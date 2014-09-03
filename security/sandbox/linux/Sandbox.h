/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_Sandbox_h
#define mozilla_Sandbox_h

#include "mozilla/Types.h"

namespace mozilla {

// The Set*Sandbox() functions must not be called if the corresponding
// CanSandbox*() function has returned false; if sandboxing is
// attempted, any failure to enable it is fatal.
//
// If sandboxing is disabled for a process type with the corresponding
// environment variable, Set*Sandbox() does nothing and CanSandbox*()
// returns true.

#ifdef MOZ_CONTENT_SANDBOX
// Disabled by setting env var MOZ_DISABLE_CONTENT_SANDBOX.
MOZ_EXPORT bool CanSandboxContentProcess();
MOZ_EXPORT void SetContentProcessSandbox();
#endif
#ifdef MOZ_GMP_SANDBOX
// Disabled by setting env var MOZ_DISABLE_GMP_SANDBOX.
MOZ_EXPORT bool CanSandboxMediaPlugin();
MOZ_EXPORT void SetMediaPluginSandbox(const char *aFilePath);
#endif

} // namespace mozilla

#endif // mozilla_Sandbox_h
