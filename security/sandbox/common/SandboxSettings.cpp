/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/SandboxSettings.h"
#include "mozISandboxSettings.h"

#include "mozilla/Components.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/StaticPrefs_security.h"

#include "prenv.h"

#ifdef XP_WIN
#  include "mozilla/gfx/gfxVars.h"
#  include "mozilla/WindowsVersion.h"
#  include "nsExceptionHandler.h"
#endif  // XP_WIN

using namespace mozilla;

namespace mozilla {

const char* ContentWin32kLockdownStateToString(
    ContentWin32kLockdownState aValue) {
  switch (aValue) {
    case ContentWin32kLockdownState::LockdownEnabled:
      return "Win32k Lockdown enabled";

    case ContentWin32kLockdownState::MissingWebRender:
      return "Win32k Lockdown disabled -- Missing WebRender";

    case ContentWin32kLockdownState::OperatingSystemNotSupported:
      return "Win32k Lockdown disabled -- Operating system not supported";

    case ContentWin32kLockdownState::PrefNotSet:
      return "Win32k Lockdown disabled -- Preference not set";
  }

  MOZ_CRASH("Should never reach here");
}

ContentWin32kLockdownState GetContentWin32kLockdownState() {
#ifdef XP_WIN
  static ContentWin32kLockdownState result = [] {
    ContentWin32kLockdownState state = [] {
      if (!IsWin8OrLater()) {
        return ContentWin32kLockdownState::OperatingSystemNotSupported;
      }

      // Win32k Lockdown requires WebRender, but WR is not currently guaranteed
      // on all computers. It can also fail to initialize and fallback to
      // non-WR render path.
      //
      // We don't want a situation where "Win32k Lockdown + No WR" occurs
      // without the user explicitly requesting unsupported behavior.
      if (!gfx::gfxVars::UseWebRender()) {
        return ContentWin32kLockdownState::MissingWebRender;
      }

      // It's important that this goes last, as we'd like to know in
      // telemetry and crash reporting if the only thing holding the user
      // back from Win32k Lockdown is the-lack-of-asking-for-it
      if (!StaticPrefs::security_sandbox_content_win32k_disable()) {
        return ContentWin32kLockdownState::PrefNotSet;
      }

      return ContentWin32kLockdownState::LockdownEnabled;
    }();

    const char* stateStr = ContentWin32kLockdownStateToString(state);
    CrashReporter::AnnotateCrashReport(
        CrashReporter::Annotation::ContentSandboxWin32kState,
        nsDependentCString(stateStr));

    return state;
  }();

  return result;

#else  // XP_WIN

  return ContentWin32kLockdownState::OperatingSystemNotSupported;

#endif  // XP_WIN
}

int GetEffectiveContentSandboxLevel() {
  if (PR_GetEnv("MOZ_DISABLE_CONTENT_SANDBOX")) {
    return 0;
  }
  int level = StaticPrefs::security_sandbox_content_level_DoNotUseDirectly();
// On Windows and macOS, enforce a minimum content sandbox level of 1 (except on
// Nightly, where it can be set to 0).
#if !defined(NIGHTLY_BUILD) && (defined(XP_WIN) || defined(XP_MACOSX))
  if (level < 1) {
    level = 1;
  }
#endif
#ifdef XP_LINUX
  // Level 4 and up will break direct access to audio.
  if (level > 3 && !StaticPrefs::media_cubeb_sandbox()) {
    level = 3;
  }
#endif

  return level;
}

bool IsContentSandboxEnabled() { return GetEffectiveContentSandboxLevel() > 0; }

int GetEffectiveSocketProcessSandboxLevel() {
  if (PR_GetEnv("MOZ_DISABLE_SOCKET_PROCESS_SANDBOX")) {
    return 0;
  }

  int level =
      StaticPrefs::security_sandbox_socket_process_level_DoNotUseDirectly();

  return level;
}

#if defined(XP_MACOSX)
int ClampFlashSandboxLevel(const int aLevel) {
  const int minLevel = 0;
  const int maxLevel = 3;

  if (aLevel < minLevel) {
    return minLevel;
  }

  if (aLevel > maxLevel) {
    return maxLevel;
  }
  return aLevel;
}
#endif

class SandboxSettings final : public mozISandboxSettings {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_MOZISANDBOXSETTINGS

  SandboxSettings() = default;

 private:
  ~SandboxSettings() = default;
};

NS_IMPL_ISUPPORTS(SandboxSettings, mozISandboxSettings)

NS_IMETHODIMP SandboxSettings::GetEffectiveContentSandboxLevel(
    int32_t* aRetVal) {
  *aRetVal = mozilla::GetEffectiveContentSandboxLevel();
  return NS_OK;
}

NS_IMETHODIMP SandboxSettings::GetContentWin32kLockdownState(int32_t* aRetVal) {
  *aRetVal = static_cast<int32_t>(mozilla::GetContentWin32kLockdownState());
  return NS_OK;
}

NS_IMETHODIMP
SandboxSettings::GetContentWin32kLockdownStateString(nsAString& aString) {
  ContentWin32kLockdownState lockdownState =
      mozilla::GetContentWin32kLockdownState();
  aString = NS_ConvertASCIItoUTF16(
      mozilla::ContentWin32kLockdownStateToString(lockdownState));
  return NS_OK;
}

}  // namespace mozilla

NS_IMPL_COMPONENT_FACTORY(mozISandboxSettings) {
  return MakeAndAddRef<SandboxSettings>().downcast<nsISupports>();
}
