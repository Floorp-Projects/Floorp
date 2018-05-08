/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SafeMode_h
#define mozilla_SafeMode_h

// NB: This code must be able to run apart from XPCOM

#include "mozilla/CmdLineAndEnvUtils.h"
#include "mozilla/Maybe.h"

#if defined(XP_WIN)
#include "mozilla/PolicyChecks.h"
#include <windows.h>
#endif // defined(XP_WIN)

// Undo X11/X.h's definition of None
#undef None

namespace mozilla {

enum class SafeModeFlag : uint32_t
{
  None = 0,
  Unset = (1 << 0)
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(SafeModeFlag)

template <typename CharT>
inline Maybe<bool>
IsSafeModeRequested(int& aArgc, CharT* aArgv[],
                    const SafeModeFlag aFlags = SafeModeFlag::Unset)
{
  CheckArgFlag checkArgFlags = CheckArgFlag::CheckOSInt;
  if (aFlags & SafeModeFlag::Unset) {
    checkArgFlags |= CheckArgFlag::RemoveArg;
  }

  ArgResult ar = CheckArg(aArgc, aArgv,
                          GetLiteral<CharT, FlagLiteral::safemode>(),
                          static_cast<const CharT**>(nullptr),
                          checkArgFlags);
  if (ar == ARG_BAD) {
    return Nothing();
  }

  bool result = ar == ARG_FOUND;

#if defined(XP_WIN)
  // If the shift key is pressed and the ctrl and / or alt keys are not pressed
  // during startup, start in safe mode. GetKeyState returns a short and the high
  // order bit will be 1 if the key is pressed. By masking the returned short
  // with 0x8000 the result will be 0 if the key is not pressed and non-zero
  // otherwise.
  if ((GetKeyState(VK_SHIFT) & 0x8000) &&
      !(GetKeyState(VK_CONTROL) & 0x8000) &&
      !(GetKeyState(VK_MENU) & 0x8000) &&
      !EnvHasValue("MOZ_DISABLE_SAFE_MODE_KEY")) {
    result = true;
  }

  if (result && PolicyCheckBoolean(L"DisableSafeMode")) {
    result = false;
  }
#endif // defined(XP_WIN)

#if defined(XP_MACOSX)
  if ((GetCurrentEventKeyModifiers() & optionKey) &&
      !EnvHasValue("MOZ_DISABLE_SAFE_MODE_KEY")) {
    result = true;
  }
#endif // defined(XP_MACOSX)

  // The Safe Mode Policy should not be enforced for the env var case
  // (used by updater and crash-recovery).
  if (EnvHasValue("MOZ_SAFE_MODE_RESTART")) {
    result = true;
    if (aFlags & SafeModeFlag::Unset) {
      // unset the env variable
      SaveToEnv("MOZ_SAFE_MODE_RESTART=");
    }
  }

  return Some(result);
}

} // namespace mozilla

#endif // mozilla_SafeMode_h
