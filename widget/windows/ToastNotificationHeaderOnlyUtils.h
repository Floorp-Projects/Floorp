/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ToastNotificationHeaderOnlyUtils_h
#define mozilla_ToastNotificationHeaderOnlyUtils_h

/**
 * This header is intended for self-contained, header-only, utility code to
 * share between Windows toast notification code in firefox.exe and
 * notificationserver.dll.
 */

// Use XPCOM logging if we're in a XUL context, otherwise use Windows Event
// logging.
// NOTE: The `printf` `format` equivalent argument to `NOTIFY_LOG` is converted
// to a wide string when outside of a XUL context. String format specifiers need
// to specify they're a wide string with `%ls` or narrow string with `%hs`.
#include "mozilla/Logging.h"
#ifdef IMPL_LIBXUL
namespace mozilla::widget {
extern LazyLogModule sWASLog;
}  // namespace mozilla::widget
#  define NOTIFY_LOG(_level, _args) \
    MOZ_LOG(mozilla::widget::sWASLog, _level, _args)
#else
#  include "mozilla/WindowsEventLog.h"

bool gVerbose = false;

#  define NOTIFY_LOG(_level, _args)                       \
    if (gVerbose || _level == mozilla::LogLevel::Error) { \
      POST_EXPAND_NOTIFY_LOG(MOZ_LOG_EXPAND_ARGS _args);  \
    }
#  define POST_EXPAND_NOTIFY_LOG(...) \
    MOZ_WIN_EVENT_LOG_ERROR_MESSAGE(  \
        L"" MOZ_APP_DISPLAYNAME " Notification Server", L"" __VA_ARGS__)
#endif

#include <functional>
#include <string>

#include "nsWindowsHelpers.h"

namespace mozilla::widget::toastnotification {

const wchar_t kLaunchArgProgram[] = L"program";
const wchar_t kLaunchArgProfile[] = L"profile";
const wchar_t kLaunchArgTag[] = L"windowsTag";
const wchar_t kLaunchArgLogging[] = L"logging";
const wchar_t kLaunchArgAction[] = L"action";

const DWORD kNotificationServerTimeoutMs = (10 * 1000);

struct ToastNotificationPidMessage {
  DWORD pid = 0;
};

struct ToastNotificationPermissionMessage {
  DWORD setForegroundPermissionGranted = 0;
};

inline std::wstring GetNotificationPipeName(const wchar_t* aTag) {
  // Prefix required by pipe API.
  std::wstring pipeName(LR"(\\.\pipe\)");

  pipeName += L"" MOZ_APP_NAME;
  pipeName += aTag;

  return pipeName;
}

inline bool WaitEventWithTimeout(const HANDLE& event) {
  DWORD result = WaitForSingleObject(event, kNotificationServerTimeoutMs);

  switch (result) {
    case WAIT_OBJECT_0:
      NOTIFY_LOG(LogLevel::Info, ("Pipe wait signaled"));
      return true;
    case WAIT_TIMEOUT:
      NOTIFY_LOG(LogLevel::Warning, ("Pipe wait timed out"));
      return false;
    case WAIT_FAILED:
      NOTIFY_LOG(LogLevel::Error,
                 ("Pipe wait failed, error %lu", GetLastError()));
      return false;
    case WAIT_ABANDONED:
      NOTIFY_LOG(LogLevel::Error, ("Pipe wait abandoned"));
      return false;
    default:
      NOTIFY_LOG(LogLevel::Error, ("Pipe wait unknown error"));
      return false;
  }
}

/* Handles running overlapped transactions for a Windows pipe. This function
 * manages lifetimes of Event and OVERLAPPED objects to ensure they are not used
 * while an overlapped operation is pending. */
inline bool SyncDoOverlappedIOWithTimeout(
    const nsAutoHandle& pipe, const size_t bytesExpected,
    const std::function<BOOL(OVERLAPPED&)>& transactPipe) {
  nsAutoHandle event(CreateEventW(nullptr, TRUE, FALSE, nullptr));
  if (!event) {
    NOTIFY_LOG(
        LogLevel::Error,
        ("Error creating pipe transaction event, error %lu", GetLastError()));
    return false;
  }

  OVERLAPPED overlapped{};
  overlapped.hEvent = event.get();
  BOOL result = transactPipe(overlapped);

  if (!result && GetLastError() != ERROR_IO_PENDING) {
    NOTIFY_LOG(LogLevel::Error,
               ("Error reading from pipe, error %lu", GetLastError()));
    return false;
  }

  if (!WaitEventWithTimeout(overlapped.hEvent)) {
    NOTIFY_LOG(LogLevel::Warning, ("Pipe transaction timed out, canceling "
                                   "(transaction may still succeed)."));

    CancelIo(pipe.get());

    // Transaction may still succeed before cancellation is handled; fall
    // through to normal handling.
  }

  DWORD bytesTransferred = 0;
  // Pipe transfer has either been signaled or cancelled by this point, so it
  // should be safe to wait on.
  BOOL overlappedResult =
      GetOverlappedResult(pipe.get(), &overlapped, &bytesTransferred, TRUE);

  if (!overlappedResult) {
    NOTIFY_LOG(
        LogLevel::Error,
        ("Error retrieving pipe overlapped result, error %lu", GetLastError()));
    return false;
  } else if (bytesTransferred != bytesExpected) {
    NOTIFY_LOG(LogLevel::Error,
               ("%lu bytes read from pipe, but %zu bytes expected",
                bytesTransferred, bytesExpected));
    return false;
  }

  return true;
}

}  // namespace mozilla::widget::toastnotification

#endif  // mozilla_ToastNotificationHeaderOnlyUtils_h
