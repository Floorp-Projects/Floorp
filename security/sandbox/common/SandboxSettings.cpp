/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozISandboxSettings.h"

#include "mozilla/Components.h"
#include "mozilla/Preferences.h"

#include "prenv.h"

using namespace mozilla;

namespace mozilla {

int GetEffectiveContentSandboxLevel() {
  if (PR_GetEnv("MOZ_DISABLE_CONTENT_SANDBOX")) {
    return 0;
  }
  int level = Preferences::GetInt("security.sandbox.content.level");
// On Windows and macOS, enforce a minimum content sandbox level of 1 (except on
// Nightly, where it can be set to 0).
#if !defined(NIGHTLY_BUILD) && (defined(XP_WIN) || defined(XP_MACOSX))
  if (level < 1) {
    level = 1;
  }
#endif
#ifdef XP_LINUX
  // Level 4 and up will break direct access to audio.
  if (level > 3 && !Preferences::GetBool("media.cubeb.sandbox")) {
    level = 3;
  }
#endif

  return level;
}

bool IsContentSandboxEnabled() { return GetEffectiveContentSandboxLevel() > 0; }

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

  SandboxSettings() {}

 private:
  ~SandboxSettings() {}
};

NS_IMPL_ISUPPORTS(SandboxSettings, mozISandboxSettings)

NS_IMETHODIMP SandboxSettings::GetEffectiveContentSandboxLevel(
    int32_t* aRetVal) {
  *aRetVal = mozilla::GetEffectiveContentSandboxLevel();
  return NS_OK;
}

}  // namespace mozilla

NS_IMPL_COMPONENT_FACTORY(mozISandboxSettings) {
  return MakeAndAddRef<SandboxSettings>().downcast<nsISupports>();
}
