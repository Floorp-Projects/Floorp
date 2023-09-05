/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "common.h"

#include "EventLog.h"

#include <windows.h>

namespace mozilla::default_agent {

ULONGLONG GetCurrentTimestamp() {
  FILETIME filetime;
  GetSystemTimeAsFileTime(&filetime);
  ULARGE_INTEGER integerTime;
  integerTime.u.LowPart = filetime.dwLowDateTime;
  integerTime.u.HighPart = filetime.dwHighDateTime;
  return integerTime.QuadPart;
}

// Passing a zero as the second argument (or omitting it) causes the function
// to get the current time rather than using a passed value.
ULONGLONG SecondsPassedSince(ULONGLONG initialTime,
                             ULONGLONG currentTime /* = 0 */) {
  if (currentTime == 0) {
    currentTime = GetCurrentTimestamp();
  }
  // Since this is returning an unsigned value, let's make sure we don't try to
  // return anything negative
  if (initialTime >= currentTime) {
    return 0;
  }

  // These timestamps are expressed in 100-nanosecond intervals
  return (currentTime - initialTime) / 10  // To microseconds
         / 1000                            // To milliseconds
         / 1000;                           // To seconds
}

FilePathResult GenerateUUIDStr() {
  UUID uuid;
  RPC_STATUS status = UuidCreate(&uuid);
  if (status != RPC_S_OK) {
    HRESULT hr = MAKE_HRESULT(1, FACILITY_RPC, status);
    LOG_ERROR(hr);
    return FilePathResult(mozilla::WindowsError::FromHResult(hr));
  }

  // 39 == length of a UUID string including braces and NUL.
  wchar_t guidBuf[39] = {};
  if (StringFromGUID2(uuid, guidBuf, 39) != 39) {
    LOG_ERROR(HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER));
    return FilePathResult(
        mozilla::WindowsError::FromWin32Error(ERROR_INSUFFICIENT_BUFFER));
  }

  // Remove the curly braces.
  return std::wstring(guidBuf + 1, guidBuf + 37);
}

FilePathResult GetRelativeBinaryPath(const wchar_t* suffix) {
  // The Path* functions don't set LastError, but this is the only thing that
  // can really cause them to fail, so if they ever do we assume this is why.
  HRESULT hr = HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);

  mozilla::UniquePtr<wchar_t[]> thisBinaryPath = mozilla::GetFullBinaryPath();
  if (!PathRemoveFileSpecW(thisBinaryPath.get())) {
    LOG_ERROR(hr);
    return FilePathResult(mozilla::WindowsError::FromHResult(hr));
  }

  wchar_t relativePath[MAX_PATH] = L"";

  if (!PathCombineW(relativePath, thisBinaryPath.get(), suffix)) {
    LOG_ERROR(hr);
    return FilePathResult(mozilla::WindowsError::FromHResult(hr));
  }

  return std::wstring(relativePath);
}

}  // namespace mozilla::default_agent
