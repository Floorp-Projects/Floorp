/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_LauncherResult_h
#define mozilla_LauncherResult_h

#include "mozilla/Result.h"
#include "mozilla/WinHeaderOnlyUtils.h"

namespace mozilla {

#if defined(MOZILLA_INTERNAL_API)

template <typename T>
using LauncherResult = WindowsErrorResult<T>;

#else

struct LauncherError {
  LauncherError(const char* aFile, int aLine, WindowsError aWin32Error)
      : mFile(aFile), mLine(aLine), mError(aWin32Error) {}

  const char* mFile;
  int mLine;
  WindowsError mError;

  bool operator==(const LauncherError& aOther) const {
    return mError == aOther.mError;
  }

  bool operator!=(const LauncherError& aOther) const {
    return mError != aOther.mError;
  }

  bool operator==(const WindowsError& aOther) const { return mError == aOther; }

  bool operator!=(const WindowsError& aOther) const { return mError != aOther; }
};

template <typename T>
using LauncherResult = Result<T, LauncherError>;

#endif  // defined(MOZILLA_INTERNAL_API)

using LauncherVoidResult = LauncherResult<Ok>;

}  // namespace mozilla

#if defined(MOZILLA_INTERNAL_API)

#  define LAUNCHER_ERROR_GENERIC() \
    ::mozilla::Err(::mozilla::WindowsError::CreateGeneric())

#  define LAUNCHER_ERROR_FROM_WIN32(err) \
    ::mozilla::Err(::mozilla::WindowsError::FromWin32Error(err))

#  define LAUNCHER_ERROR_FROM_LAST() \
    ::mozilla::Err(::mozilla::WindowsError::FromLastError())

#  define LAUNCHER_ERROR_FROM_NTSTATUS(ntstatus) \
    ::mozilla::Err(::mozilla::WindowsError::FromNtStatus(ntstatus))

#  define LAUNCHER_ERROR_FROM_HRESULT(hresult) \
    ::mozilla::Err(::mozilla::WindowsError::FromHResult(hresult))

#  define LAUNCHER_ERROR_FROM_MOZ_WINDOWS_ERROR(err) ::mozilla::Err(err)

#else

#  define LAUNCHER_ERROR_GENERIC()           \
    ::mozilla::Err(::mozilla::LauncherError( \
        __FILE__, __LINE__, ::mozilla::WindowsError::CreateGeneric()))

#  define LAUNCHER_ERROR_FROM_WIN32(err)     \
    ::mozilla::Err(::mozilla::LauncherError( \
        __FILE__, __LINE__, ::mozilla::WindowsError::FromWin32Error(err)))

#  define LAUNCHER_ERROR_FROM_LAST()         \
    ::mozilla::Err(::mozilla::LauncherError( \
        __FILE__, __LINE__, ::mozilla::WindowsError::FromLastError()))

#  define LAUNCHER_ERROR_FROM_NTSTATUS(ntstatus) \
    ::mozilla::Err(::mozilla::LauncherError(     \
        __FILE__, __LINE__, ::mozilla::WindowsError::FromNtStatus(ntstatus)))

#  define LAUNCHER_ERROR_FROM_HRESULT(hresult) \
    ::mozilla::Err(::mozilla::LauncherError(   \
        __FILE__, __LINE__, ::mozilla::WindowsError::FromHResult(hresult)))

// This macro wraps the supplied WindowsError with a LauncherError
#  define LAUNCHER_ERROR_FROM_MOZ_WINDOWS_ERROR(err) \
    ::mozilla::Err(::mozilla::LauncherError(__FILE__, __LINE__, err))

#endif  // defined(MOZILLA_INTERNAL_API)

// This macro enables moving of a mozilla::LauncherError from a
// mozilla::LauncherResult<Foo> into a mozilla::LauncherResult<Bar>
#define LAUNCHER_ERROR_FROM_RESULT(result) ::mozilla::Err(result.unwrapErr())

#endif  // mozilla_LauncherResult_h
