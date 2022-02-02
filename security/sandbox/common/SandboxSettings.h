/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SandboxSettings_h
#define mozilla_SandboxSettings_h
#include <cinttypes>

#ifdef __OpenBSD__
#  include "nsXULAppAPI.h"
#endif

namespace mozilla {

// Return the current sandbox level. This is the
// "security.sandbox.content.level" preference, but rounded up to the current
// minimum allowed level. Returns 0 (disabled) if the env var
// MOZ_DISABLE_CONTENT_SANDBOX is set.
int GetEffectiveContentSandboxLevel();
int GetEffectiveSocketProcessSandboxLevel();

// Checks whether the effective content sandbox level is > 0.
bool IsContentSandboxEnabled();

// If you update this enum, don't forget to raise the limit in
// TelemetryEnvironmentTesting.jsm and record the new value in
// environment.rst
enum class ContentWin32kLockdownState : int32_t {
  LockdownEnabled = 1,  // no longer used
  MissingWebRender = 2,
  OperatingSystemNotSupported = 3,
  PrefNotSet = 4,  // no longer used
  MissingRemoteWebGL = 5,
  MissingNonNativeTheming = 6,
  DisabledByEnvVar = 7,  // - MOZ_ENABLE_WIN32K is set
  DisabledBySafeMode = 8,
  DisabledByE10S = 9,       // - E10S is disabled for whatever reason
  DisabledByUserPref = 10,  // - The user manually set
                            // security.sandbox.content.win32k-disable to false
  EnabledByUserPref = 11,   // The user manually set
                            // security.sandbox.content.win32k-disable to true
  DisabledByControlGroup =
      12,  // The user is in the Control Group, so it is disabled
  EnabledByTreatmentGroup =
      13,  // The user is in the Treatment Group, so it is enabled
  DisabledByDefault = 14,  // The default value of the pref is false
  EnabledByDefault = 15    // The default value of the pref is true
};

const char* ContentWin32kLockdownStateToString(
    ContentWin32kLockdownState aValue);

ContentWin32kLockdownState GetContentWin32kLockdownState();

#if defined(XP_MACOSX)
int ClampFlashSandboxLevel(const int aLevel);
#endif

#if defined(__OpenBSD__)
bool StartOpenBSDSandbox(GeckoProcessType type);
#endif

}  // namespace mozilla
#endif  // mozilla_SandboxPolicies_h
