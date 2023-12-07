/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file is not build directly. Instead, it is included in multiple
// shared objects.

#ifdef nsWindowsRestart_cpp
#  error \
      "nsWindowsRestart.cpp is not a header file, and must only be included once."
#else
#  define nsWindowsRestart_cpp
#endif

#include "mozilla/CmdLineAndEnvUtils.h"
#include "nsUTF8Utils.h"

#include <shellapi.h>

// Needed for CreateEnvironmentBlock
#include <userenv.h>
#ifndef __MINGW32__
#  pragma comment(lib, "userenv.lib")
#endif

/**
 * Convert UTF8 to UTF16 without using the normal XPCOM goop, which we
 * can't link to updater.exe.
 */
static char16_t* AllocConvertUTF8toUTF16(const char* arg) {
  // UTF16 can't be longer in units than UTF8
  size_t len = strlen(arg);
  char16_t* s = new char16_t[(len + 1) * sizeof(char16_t)];
  if (!s) return nullptr;

  size_t dstLen = ::MultiByteToWideChar(CP_UTF8, 0, arg, len,
                                        reinterpret_cast<wchar_t*>(s), len);
  s[dstLen] = 0;

  return s;
}

static void FreeAllocStrings(int argc, wchar_t** argv) {
  while (argc) {
    --argc;
    delete[] argv[argc];
  }

  delete[] argv;
}

static wchar_t** AllocConvertUTF8toUTF16Strings(int argc, char** argv) {
  wchar_t** argvConverted = new wchar_t*[argc];
  if (!argvConverted) return nullptr;

  for (int i = 0; i < argc; ++i) {
    argvConverted[i] =
        reinterpret_cast<wchar_t*>(AllocConvertUTF8toUTF16(argv[i]));
    if (!argvConverted[i]) {
      FreeAllocStrings(i, argvConverted);
      return nullptr;
    }
  }
  return argvConverted;
}

/**
 * Return true if we are in a job with JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE and
 * we can break away from it.
 * CreateProcess fails if we try to break away from a job but it's not allowed.
 * So if we cannot determine the result due to a failure, we assume we don't
 * need to break away and this returns false.
 */
static bool NeedToBreakAwayFromJob() {
  // If we can't determine if we are in a job, we assume we're not in a job.
  BOOL inJob = FALSE;
  if (!::IsProcessInJob(::GetCurrentProcess(), nullptr, &inJob)) {
    return false;
  }

  // If there is no job, there is nothing to worry about.
  if (!inJob) {
    return false;
  }

  // If we can't get the job object flags, we assume no need to break away from
  // it.
  JOBOBJECT_EXTENDED_LIMIT_INFORMATION job_info = {};
  if (!::QueryInformationJobObject(nullptr, JobObjectExtendedLimitInformation,
                                   &job_info, sizeof(job_info), nullptr)) {
    return false;
  }

  // If JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE is not set, no need to worry about
  // the job.
  if (!(job_info.BasicLimitInformation.LimitFlags &
        JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE)) {
    return false;
  }

  // If we can't break away from the current job, there is nothing we can do.
  if (!(job_info.BasicLimitInformation.LimitFlags &
        JOB_OBJECT_LIMIT_BREAKAWAY_OK)) {
    return false;
  }

  // We can and need to break away from the job.
  return true;
}

/**
 * Launch a child process with the specified arguments.
 * @note argv[0] is ignored
 * @note The form of this function that takes char **argv expects UTF-8
 */

BOOL WinLaunchChild(const wchar_t* exePath, int argc, wchar_t** argv,
                    HANDLE userToken, HANDLE* hProcess);

BOOL WinLaunchChild(const wchar_t* exePath, int argc, char** argv,
                    HANDLE userToken, HANDLE* hProcess) {
  wchar_t** argvConverted = AllocConvertUTF8toUTF16Strings(argc, argv);
  if (!argvConverted) return FALSE;

  BOOL ok = WinLaunchChild(exePath, argc, argvConverted, userToken, hProcess);
  FreeAllocStrings(argc, argvConverted);
  return ok;
}

BOOL WinLaunchChild(const wchar_t* exePath, int argc, wchar_t** argv,
                    HANDLE userToken, HANDLE* hProcess) {
  BOOL ok;

  mozilla::UniquePtr<wchar_t[]> cl(mozilla::MakeCommandLine(argc, argv));
  if (!cl) {
    return FALSE;
  }

  DWORD creationFlags =
      NeedToBreakAwayFromJob() ? CREATE_BREAKAWAY_FROM_JOB : 0;

  STARTUPINFOW si = {0};
  si.cb = sizeof(STARTUPINFOW);
  si.lpDesktop = const_cast<LPWSTR>(L"winsta0\\Default");
  PROCESS_INFORMATION pi = {0};

  if (userToken == nullptr) {
    ok = CreateProcessW(exePath, cl.get(),
                        nullptr,  // no special security attributes
                        nullptr,  // no special thread attributes
                        FALSE,    // don't inherit filehandles
                        creationFlags,
                        nullptr,  // inherit my environment
                        nullptr,  // use my current directory
                        &si, &pi);
  } else {
    // Create an environment block for the process we're about to start using
    // the user's token.
    LPVOID environmentBlock = nullptr;
    if (!CreateEnvironmentBlock(&environmentBlock, userToken, TRUE)) {
      environmentBlock = nullptr;
    }

    ok = CreateProcessAsUserW(userToken, exePath, cl.get(),
                              nullptr,  // no special security attributes
                              nullptr,  // no special thread attributes
                              FALSE,    // don't inherit filehandles
                              creationFlags, environmentBlock,
                              nullptr,  // use my current directory
                              &si, &pi);

    if (environmentBlock) {
      DestroyEnvironmentBlock(environmentBlock);
    }
  }

  if (ok) {
    if (hProcess) {
      *hProcess = pi.hProcess;  // the caller now owns the HANDLE
    } else {
      CloseHandle(pi.hProcess);
    }
    CloseHandle(pi.hThread);
  } else {
    LPVOID lpMsgBuf = nullptr;
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                      FORMAT_MESSAGE_IGNORE_INSERTS,
                  nullptr, GetLastError(),
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&lpMsgBuf,
                  0, nullptr);
    wprintf(L"Error restarting: %s\n",
            lpMsgBuf ? static_cast<const wchar_t*>(lpMsgBuf) : L"(null)");
    if (lpMsgBuf) LocalFree(lpMsgBuf);
  }

  return ok;
}
