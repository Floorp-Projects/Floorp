/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __DEFAULT_BROWSER_AGENT_COMMON_H__
#define __DEFAULT_BROWSER_AGENT_COMMON_H__

#include "mozilla/WinHeaderOnlyUtils.h"

#define AGENT_REGKEY_NAME \
  L"SOFTWARE\\" MOZ_APP_VENDOR "\\" MOZ_APP_BASENAME "\\Default Browser Agent"

namespace mozilla::default_agent {

ULONGLONG GetCurrentTimestamp();
// Passing a zero as the second argument (or omitting it) causes the function
// to get the current time rather than using a passed value.
ULONGLONG SecondsPassedSince(ULONGLONG initialTime, ULONGLONG currentTime = 0);

using FilePathResult = mozilla::WindowsErrorResult<std::wstring>;
FilePathResult GenerateUUIDStr();

FilePathResult GetRelativeBinaryPath(const wchar_t* suffix);

}  // namespace mozilla::default_agent

#endif  // __DEFAULT_BROWSER_AGENT_COMMON_H__
