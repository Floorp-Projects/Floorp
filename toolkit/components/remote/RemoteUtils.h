/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TOOLKIT_COMPONENTS_REMOTE_REMOTEUTILS_H_
#define TOOLKIT_COMPONENTS_REMOTE_REMOTEUTILS_H_

#include "nsString.h"
#if defined XP_WIN
#  include "WinUtils.h"
#endif

#if defined XP_WIN || defined XP_MACOSX
static void BuildClassName(const char* aProgram, const char* aProfile,
                           nsString& aClassName) {
  aClassName.AppendPrintf("Mozilla_%s", aProgram);
#  if defined XP_WIN
  nsString pfn = mozilla::widget::WinUtils::GetPackageFamilyName();
  if (!pfn.IsEmpty()) {
    aClassName.AppendPrintf("_%S", static_cast<const wchar_t*>(pfn.get()));
  }
#  endif
  aClassName.AppendPrintf("_%s_RemoteWindow", aProfile);
}
#endif

char* ConstructCommandLine(int32_t argc, char** argv, const char* aStartupToken,
                           int* aCommandLineLength);

#endif  // TOOLKIT_COMPONENTS_REMOTE_REMOTEUTILS_H_
