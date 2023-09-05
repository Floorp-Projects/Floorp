/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UtfConvert.h"

#include <string>

#include "EventLog.h"

#include "mozilla/Buffer.h"
#include "mozilla/WinHeaderOnlyUtils.h"

namespace mozilla::default_agent {

Utf16ToUtf8Result Utf16ToUtf8(const wchar_t* const utf16) {
  int utf8Len =
      WideCharToMultiByte(CP_UTF8, 0, utf16, -1, nullptr, 0, nullptr, nullptr);
  if (utf8Len == 0) {
    HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
    LOG_ERROR(hr);
    return Utf16ToUtf8Result(mozilla::WindowsError::FromHResult(hr));
  }

  mozilla::Buffer<char> utf8(utf8Len);
  int bytesWritten = WideCharToMultiByte(CP_UTF8, 0, utf16, -1, utf8.Elements(),
                                         utf8.Length(), nullptr, nullptr);
  if (bytesWritten == 0) {
    HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
    LOG_ERROR(hr);
    return Utf16ToUtf8Result(mozilla::WindowsError::FromHResult(hr));
  }

  return std::string(utf8.Elements());
}

Utf8ToUtf16Result Utf8ToUtf16(const char* const utf8) {
  int utf16Len = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
  if (utf16Len == 0) {
    HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
    LOG_ERROR(hr);
    return mozilla::Err(mozilla::WindowsError::FromHResult(hr));
  }

  mozilla::Buffer<wchar_t> utf16(utf16Len);
  int charsWritten = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, utf16.Elements(),
                                         utf16.Length());
  if (charsWritten == 0) {
    HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
    LOG_ERROR(hr);
    return mozilla::Err(mozilla::WindowsError::FromHResult(hr));
  }

  return std::wstring(utf16.Elements());
}

}  // namespace mozilla::default_agent
