/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SandboxSettings_h
#define mozilla_SandboxSettings_h

namespace mozilla {

// Return the current sandbox level. This is the
// "security.sandbox.content.level" preference, but rounded up to the current
// minimum allowed level. Returns 0 (disabled) if the env var
// MOZ_DISABLE_CONTENT_SANDBOX is set.
int GetEffectiveContentSandboxLevel();

// Checks whether the effective content sandbox level is > 0.
bool IsContentSandboxEnabled();

#if defined(XP_MACOSX)
int ClampFlashSandboxLevel(const int aLevel);
#endif

#if defined(__OpenBSD__)
bool StartOpenBSDSandbox(GeckoProcessType type);
#endif

#if !defined(XP_WIN)
// Returns whether or not the currently running build is an unpackaged
// developer build. This check is implemented by looking for omni.ja in the
// the obj/dist dir. We use this routine to detect when the build dir will
// use symlinks to the repo and object dir. On Windows, dev builds don't
// use symlinks.
bool IsDevelopmentBuild();
#endif /* !XP_WIN */

}  // namespace mozilla
#endif  // mozilla_SandboxPolicies_h
