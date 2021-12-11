/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_BinaryPath_h
#define mozilla_BinaryPath_h

#include "nsXPCOMPrivate.h"  // for MAXPATHLEN
#ifdef XP_WIN
#  include <windows.h>
#elif defined(XP_MACOSX)
#  include <CoreFoundation/CoreFoundation.h>
#elif defined(XP_UNIX)
#  include <unistd.h>
#  include <stdlib.h>
#  include <string.h>
#endif
#if defined(__FreeBSD__) || defined(__DragonFly__) || \
    defined(__FreeBSD_kernel__) || defined(__NetBSD__) || defined(__OpenBSD__)
#  include <sys/sysctl.h>
#endif
#if defined(__OpenBSD__)
#  include <sys/stat.h>
#endif
#include "mozilla/UniquePtr.h"
#include "mozilla/UniquePtrExtensions.h"

#ifdef MOZILLA_INTERNAL_API
#  include "nsCOMPtr.h"
#  include "nsIFile.h"
#  include "nsString.h"
#endif

namespace mozilla {

class BinaryPath {
 public:
#ifdef XP_WIN
  static nsresult Get(char aResult[MAXPATHLEN]) {
    wchar_t wide_path[MAXPATHLEN];
    nsresult rv = GetW(wide_path);
    if (NS_FAILED(rv)) {
      return rv;
    }
    WideCharToMultiByte(CP_UTF8, 0, wide_path, -1, aResult, MAXPATHLEN, nullptr,
                        nullptr);
    return NS_OK;
  }

  static nsresult GetLong(wchar_t aResult[MAXPATHLEN]) {
    static bool cached = false;
    static wchar_t exeLongPath[MAXPATHLEN] = L"";

    if (!cached) {
      nsresult rv = GetW(exeLongPath);

      if (NS_FAILED(rv)) {
        return rv;
      }

      if (!::GetLongPathNameW(exeLongPath, exeLongPath, MAXPATHLEN)) {
        return NS_ERROR_FAILURE;
      }

      cached = true;
    }

    if (wcscpy_s(aResult, MAXPATHLEN, exeLongPath)) {
      return NS_ERROR_FAILURE;
    }

    return NS_OK;
  }

 private:
  static nsresult GetW(wchar_t aResult[MAXPATHLEN]) {
    static bool cached = false;
    static wchar_t moduleFileName[MAXPATHLEN] = L"";

    if (!cached) {
      if (!::GetModuleFileNameW(0, moduleFileName, MAXPATHLEN)) {
        return NS_ERROR_FAILURE;
      }

      cached = true;
    }

    if (wcscpy_s(aResult, MAXPATHLEN, moduleFileName)) {
      return NS_ERROR_FAILURE;
    }

    return NS_OK;
  }

#elif defined(XP_MACOSX)
  static nsresult Get(char aResult[MAXPATHLEN]) {
    // Works even if we're not bundled.
    CFBundleRef appBundle = CFBundleGetMainBundle();
    if (!appBundle) {
      return NS_ERROR_FAILURE;
    }

    CFURLRef executableURL = CFBundleCopyExecutableURL(appBundle);
    if (!executableURL) {
      return NS_ERROR_FAILURE;
    }

    nsresult rv;
    if (CFURLGetFileSystemRepresentation(executableURL, false, (UInt8*)aResult,
                                         MAXPATHLEN)) {
      // Sanitize path in case the app was launched from Terminal via
      // './firefox' for example.
      size_t readPos = 0;
      size_t writePos = 0;
      while (aResult[readPos] != '\0') {
        if (aResult[readPos] == '.' && aResult[readPos + 1] == '/') {
          readPos += 2;
        } else {
          aResult[writePos] = aResult[readPos];
          readPos++;
          writePos++;
        }
      }
      aResult[writePos] = '\0';
      rv = NS_OK;
    } else {
      rv = NS_ERROR_FAILURE;
    }

    CFRelease(executableURL);
    return rv;
  }

#elif defined(ANDROID)
  static nsresult Get(char aResult[MAXPATHLEN]) {
    // On Android, we use the MOZ_ANDROID_LIBDIR variable that is set by the
    // Java bootstrap code.
    const char* libDir = getenv("MOZ_ANDROID_LIBDIR");
    if (!libDir) {
      return NS_ERROR_FAILURE;
    }

    snprintf(aResult, MAXPATHLEN, "%s/%s", libDir, "dummy");
    aResult[MAXPATHLEN - 1] = '\0';
    return NS_OK;
  }

#elif defined(XP_LINUX) || defined(XP_SOLARIS)
  static nsresult Get(char aResult[MAXPATHLEN]) {
#  if defined(XP_SOLARIS)
    const char path[] = "/proc/self/path/a.out";
#  else
    const char path[] = "/proc/self/exe";
#  endif

    ssize_t len = readlink(path, aResult, MAXPATHLEN - 1);
    if (len < 0) {
      return NS_ERROR_FAILURE;
    }
    aResult[len] = '\0';
#  if defined(XP_LINUX)
    // Removing suffix " (deleted)" from the binary path
    const char suffix[] = " (deleted)";
    const ssize_t suffix_len = sizeof(suffix);
    if (len >= suffix_len) {
      char* result_end = &aResult[len - (suffix_len - 1)];
      if (memcmp(result_end, suffix, suffix_len) == 0) {
        *result_end = '\0';
      }
    }
#  endif
    return NS_OK;
  }

#elif defined(__FreeBSD__) || defined(__DragonFly__) || \
    defined(__FreeBSD_kernel__) || defined(__NetBSD__)
  static nsresult Get(char aResult[MAXPATHLEN]) {
    int mib[4];
    mib[0] = CTL_KERN;
#  ifdef __NetBSD__
    mib[1] = KERN_PROC_ARGS;
    mib[2] = -1;
    mib[3] = KERN_PROC_PATHNAME;
#  else
    mib[1] = KERN_PROC;
    mib[2] = KERN_PROC_PATHNAME;
    mib[3] = -1;
#  endif

    size_t len = MAXPATHLEN;
    if (sysctl(mib, 4, aResult, &len, nullptr, 0) < 0) {
      return NS_ERROR_FAILURE;
    }

    return NS_OK;
  }

#elif defined(__OpenBSD__)
  static nsresult Get(char aResult[MAXPATHLEN]) {
    int mib[4];
    mib[0] = CTL_KERN;
    mib[1] = KERN_PROC_ARGS;
    mib[2] = getpid();
    mib[3] = KERN_PROC_ARGV;

    size_t len = 0;
    if (sysctl(mib, 4, nullptr, &len, nullptr, 0) < 0) {
      return NS_ERROR_FAILURE;
    }

    auto argv = MakeUnique<const char*[]>(len / sizeof(const char*));
    if (sysctl(mib, 4, argv.get(), &len, nullptr, 0) < 0) {
      return NS_ERROR_FAILURE;
    }

    return GetFromArgv0(argv[0], aResult);
  }

  static nsresult GetFromArgv0(const char* aArgv0, char aResult[MAXPATHLEN]) {
    struct stat fileStat;
    // 1) use realpath() on argv[0], which works unless we're loaded from the
    //    PATH. Only do so if argv[0] looks like a path (contains a /).
    // 2) manually walk through the PATH and look for ourself
    // 3) give up
    if (strchr(aArgv0, '/') && realpath(aArgv0, aResult) &&
        stat(aResult, &fileStat) == 0) {
      return NS_OK;
    }

    const char* path = getenv("PATH");
    if (!path) {
      return NS_ERROR_FAILURE;
    }

    char* pathdup = strdup(path);
    if (!pathdup) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    bool found = false;
    char* token = strtok(pathdup, ":");
    while (token) {
      char tmpPath[MAXPATHLEN];
      sprintf(tmpPath, "%s/%s", token, aArgv0);
      if (realpath(tmpPath, aResult) && stat(aResult, &fileStat) == 0) {
        found = true;
        break;
      }
      token = strtok(nullptr, ":");
    }
    free(pathdup);
    if (found) {
      return NS_OK;
    }
    return NS_ERROR_FAILURE;
  }

#else
#  error Oops, you need platform-specific code here
#endif

 public:
  static UniqueFreePtr<char> Get() {
    char path[MAXPATHLEN];
    if (NS_FAILED(Get(path))) {
      return nullptr;
    }
    UniqueFreePtr<char> result;
    result.reset(strdup(path));
    return result;
  }

#ifdef MOZILLA_INTERNAL_API
  static nsresult GetFile(nsIFile** aResult) {
    nsCOMPtr<nsIFile> lf;
#  ifdef XP_WIN
    wchar_t exePath[MAXPATHLEN];
    nsresult rv = GetW(exePath);
#  else
    char exePath[MAXPATHLEN];
    nsresult rv = Get(exePath);
#  endif
    if (NS_FAILED(rv)) {
      return rv;
    }
#  ifdef XP_WIN
    rv = NS_NewLocalFile(nsDependentString(exePath), true, getter_AddRefs(lf));
#  else
    rv = NS_NewNativeLocalFile(nsDependentCString(exePath), true,
                               getter_AddRefs(lf));
#  endif
    if (NS_FAILED(rv)) {
      return rv;
    }
    NS_ADDREF(*aResult = lf);
    return NS_OK;
  }
#endif
};

}  // namespace mozilla

#endif /* mozilla_BinaryPath_h */
