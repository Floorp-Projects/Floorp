/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_WinHeaderOnlyUtils_h
#define mozilla_WinHeaderOnlyUtils_h

#include <windows.h>

#include "mozilla/Maybe.h"
#include "mozilla/WindowsVersion.h"
#include "nsWindowsHelpers.h"

/**
 * This header is intended for self-contained, header-only, utility code for
 * Win32. It may be used outside of xul.dll, in places such as firefox.exe or
 * mozglue.dll. If your code creates dependencies on Mozilla libraries, you
 * should put it elsewhere.
 */

#if _WIN32_WINNT < _WIN32_WINNT_WIN8
typedef struct _FILE_ID_INFO
{
  ULONGLONG   VolumeSerialNumber;
  FILE_ID_128 FileId;
} FILE_ID_INFO;

#define FileIdInfo ((FILE_INFO_BY_HANDLE_CLASS)18)

#endif // _WIN32_WINNT < _WIN32_WINNT_WIN8

namespace mozilla {

// How long to wait for a created process to become available for input,
// to prevent that process's windows being forced to the background.
// This is used across update, restart, and the launcher.
const DWORD kWaitForInputIdleTimeoutMS = 10*1000;

/**
 * Wait for a child GUI process to become "idle." Idle means that the process
 * has created its message queue and has begun waiting for user input.
 *
 * Note that this must only be used when the child process is going to display
 * GUI! Otherwise you're going to be waiting for a very long time ;-)
 *
 * @return true if we successfully waited for input idle;
 *         false if we timed out or failed to wait.
 */
inline bool
WaitForInputIdle(HANDLE aProcess, DWORD aTimeoutMs = kWaitForInputIdleTimeoutMS)
{
  const DWORD kSleepTimeMs = 10;
  const DWORD waitStart = aTimeoutMs == INFINITE ? 0 : ::GetTickCount();
  DWORD elapsed = 0;

  while (true) {
    if (aTimeoutMs != INFINITE) {
      elapsed = ::GetTickCount() - waitStart;
    }

    if (elapsed >= aTimeoutMs) {
      return false;
    }

    DWORD waitResult = ::WaitForInputIdle(aProcess, aTimeoutMs - elapsed);
    if (!waitResult) {
      return true;
    }

    if (waitResult == WAIT_FAILED && ::GetLastError() == ERROR_NOT_GUI_PROCESS) {
      ::Sleep(kSleepTimeMs);
      continue;
    }

    return false;
  }
}

class FileUniqueId final
{
public:
  explicit FileUniqueId(const wchar_t* aPath)
    : mId()
  {
    if (!aPath) {
      return;
    }

    nsAutoHandle file(::CreateFileW(aPath, 0, FILE_SHARE_READ |
                                    FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                    nullptr, OPEN_EXISTING,
                                    FILE_FLAG_BACKUP_SEMANTICS, nullptr));
    if (file == INVALID_HANDLE_VALUE) {
      return;
    }

    GetId(file);
  }

  explicit FileUniqueId(const nsAutoHandle& aFile)
    : mId()
  {
    GetId(aFile);
  }

  FileUniqueId(const FileUniqueId& aOther)
    : mId(aOther.mId)
  {
  }

  ~FileUniqueId() = default;

  explicit operator bool() const
  {
    FILE_ID_INFO zeros = {};
    return memcmp(&mId, &zeros, sizeof(FILE_ID_INFO));
  }

  FileUniqueId& operator=(const FileUniqueId& aOther)
  {
    mId = aOther.mId;
    return *this;
  }

  FileUniqueId(FileUniqueId&& aOther) = delete;
  FileUniqueId& operator=(FileUniqueId&& aOther) = delete;

  bool operator==(const FileUniqueId& aOther) const
  {
    return !memcmp(&mId, &aOther.mId, sizeof(FILE_ID_INFO));
  }

  bool operator!=(const FileUniqueId& aOther) const
  {
    return !((*this) == aOther);
  }

private:
  void GetId(const nsAutoHandle& aFile)
  {
    if (IsWin8OrLater()) {
      ::GetFileInformationByHandleEx(aFile.get(), FileIdInfo, &mId, sizeof(mId));
      return;
    }

    BY_HANDLE_FILE_INFORMATION info = {};
    if (!::GetFileInformationByHandle(aFile.get(), &info)) {
      return;
    }

    mId.VolumeSerialNumber = info.dwVolumeSerialNumber;
    memcpy(&mId.FileId.Identifier[0], &info.nFileIndexLow,
           sizeof(DWORD));
    memcpy(&mId.FileId.Identifier[sizeof(DWORD)], &info.nFileIndexHigh,
           sizeof(DWORD));
  }

private:
  FILE_ID_INFO  mId;
};

inline Maybe<bool>
DoPathsPointToIdenticalFile(const wchar_t* aPath1, const wchar_t* aPath2)
{
  FileUniqueId id1(aPath1);
  if (!id1) {
    return Nothing();
  }

  FileUniqueId id2(aPath2);
  if (!id2) {
    return Nothing();
  }

  return Some(id1 == id2);
}

} // namespace mozilla

#endif // mozilla_WinHeaderOnlyUtils_h
