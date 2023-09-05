/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DEFAULT_BROWSER_UTF_CONVERT_H__
#define DEFAULT_BROWSER_UTF_CONVERT_H__

#include <string>

#include "mozilla/WinHeaderOnlyUtils.h"

namespace mozilla::default_agent {

using Utf16ToUtf8Result = mozilla::WindowsErrorResult<std::string>;
using Utf8ToUtf16Result = mozilla::WindowsErrorResult<std::wstring>;

Utf16ToUtf8Result Utf16ToUtf8(const wchar_t* const utf16);
Utf8ToUtf16Result Utf8ToUtf16(const char* const utf8);

}  // namespace mozilla::default_agent

#endif  // DEFAULT_BROWSER_UTF_CONVERT_H__
