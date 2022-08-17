/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_WindowsEventLog_h
#define mozilla_WindowsEventLog_h

/**
 * Report messages to the Windows Event Log.
 */

#include <stdio.h>
#include <windows.h>

#include "mozilla/Attributes.h"
#include "mozilla/UniquePtr.h"

/**
 * This header is intended for self-contained, header-only, utility code for
 * Win32. It may be used outside of xul.dll, in places such as
 * default-browser-agent.exe or notificationrouter.dll. If your code creates
 * dependencies on Mozilla libraries, you should put it elsewhere.
 */

#define MOZ_WIN_EVENT_LOG_ERROR(source, hr) \
  mozilla::WriteWindowsEventLogHresult(source, hr, __FUNCTION__, __LINE__)
#define MOZ_WIN_EVENT_LOG_ERROR_MESSAGE(source, format, ...)              \
  mozilla::WriteWindowsEventLogErrorMessage(source, format, __FUNCTION__, \
                                            __LINE__, ##__VA_ARGS__)

namespace mozilla {

static void WriteWindowsEventLogErrorBuffer(const wchar_t* eventSourceName,
                                            const wchar_t* buffer,
                                            DWORD eventId) {
  HANDLE source = RegisterEventSourceW(nullptr, eventSourceName);
  if (!source) {
    // Not much we can do about this.
    return;
  }

  const wchar_t* stringsArray[] = {buffer};
  ReportEventW(source, EVENTLOG_ERROR_TYPE, 0, eventId, nullptr, 1, 0,
               stringsArray, nullptr);

  DeregisterEventSource(source);
}

void WriteWindowsEventLogHresult(const wchar_t* eventSourceName, HRESULT hr,
                                 const char* sourceFile, int sourceLine) {
  const wchar_t* format = L"0x%X in %S:%d";
  int bufferSize = _scwprintf(format, hr, sourceFile, sourceLine);
  ++bufferSize;  // Extra character for terminating null
  mozilla::UniquePtr<wchar_t[]> errorStr =
      mozilla::MakeUnique<wchar_t[]>(bufferSize);

  _snwprintf_s(errorStr.get(), bufferSize, _TRUNCATE, format, hr, sourceFile,
               sourceLine);

  WriteWindowsEventLogErrorBuffer(eventSourceName, errorStr.get(), hr);
}

MOZ_FORMAT_WPRINTF(1, 4)
void WriteWindowsEventLogErrorMessage(const wchar_t* eventSourceName,
                                      const wchar_t* messageFormat,
                                      const char* sourceFile, int sourceLine,
                                      ...) {
  // First assemble the passed message
  va_list ap;
  va_start(ap, sourceLine);
  int bufferSize = _vscwprintf(messageFormat, ap);
  ++bufferSize;  // Extra character for terminating null
  va_end(ap);
  mozilla::UniquePtr<wchar_t[]> message =
      mozilla::MakeUnique<wchar_t[]>(bufferSize);

  va_start(ap, sourceLine);
  vswprintf(message.get(), bufferSize, messageFormat, ap);
  va_end(ap);

  // Next, assemble the complete error message to print
  const wchar_t* errorFormat = L"Error: %s (%S:%d)";
  bufferSize = _scwprintf(errorFormat, message.get(), sourceFile, sourceLine);
  ++bufferSize;  // Extra character for terminating null
  mozilla::UniquePtr<wchar_t[]> errorStr =
      mozilla::MakeUnique<wchar_t[]>(bufferSize);

  _snwprintf_s(errorStr.get(), bufferSize, _TRUNCATE, errorFormat,
               message.get(), sourceFile, sourceLine);

  WriteWindowsEventLogErrorBuffer(eventSourceName, errorStr.get(), 0);
}

}  // namespace mozilla

#endif  // mozilla_WindowsEventLog_h
