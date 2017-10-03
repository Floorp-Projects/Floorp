/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_BinaryPath_h
#define mozilla_BinaryPath_h

#include "nsXPCOMPrivate.h" // for MAXPATHLEN
#ifdef XP_WIN
#include <windows.h>
#elif defined(XP_MACOSX)
#include <CoreFoundation/CoreFoundation.h>
#elif defined(XP_UNIX)
#include <sys/stat.h>
#include <string.h>
#endif
#include "mozilla/UniquePtr.h"
#include "mozilla/UniquePtrExtensions.h"

#ifdef MOZILLA_INTERNAL_API
#include "nsCOMPtr.h"
#include "nsIFile.h"
#include "nsString.h"
#endif

namespace mozilla {

class BinaryPath
{
public:
#ifdef XP_WIN
  static nsresult Get(const char* argv0, char aResult[MAXPATHLEN])
  {
    wchar_t wide_path[MAXPATHLEN];
    nsresult rv = GetW(argv0, wide_path);
    if (NS_FAILED(rv)) {
      return rv;
    }
    WideCharToMultiByte(CP_UTF8, 0, wide_path, -1,
                        aResult, MAXPATHLEN, nullptr, nullptr);
    return NS_OK;
  }

  static nsresult GetLong(wchar_t aResult[MAXPATHLEN])
  {
    static bool cached = false;
    static wchar_t exeLongPath[MAXPATHLEN] = L"";

    if (!cached) {
      nsresult rv = GetW(nullptr, exeLongPath);

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
  static nsresult GetW(const char* argv0, wchar_t aResult[MAXPATHLEN])
  {
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
  static nsresult Get(const char* argv0, char aResult[MAXPATHLEN])
  {
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
  static nsresult Get(const char* argv0, char aResult[MAXPATHLEN])
  {
    // On Android, we use the GRE_HOME variable that is set by the Java
    // bootstrap code.
    const char* greHome = getenv("GRE_HOME");
    if (!greHome) {
      return NS_ERROR_FAILURE;
    }

    snprintf(aResult, MAXPATHLEN, "%s/%s", greHome, "dummy");
    aResult[MAXPATHLEN - 1] = '\0';
    return NS_OK;
  }

#elif defined(XP_UNIX)
  static nsresult Get(const char* aArgv0, char aResult[MAXPATHLEN])
  {
    struct stat fileStat;
    // on unix, there is no official way to get the path of the current binary.
    // instead of using the MOZILLA_FIVE_HOME hack, which doesn't scale to
    // multiple applications, we will try a series of techniques:
    //
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
#error Oops, you need platform-specific code here
#endif

public:
  static UniqueFreePtr<char> Get(const char *aArgv0)
  {
    char path[MAXPATHLEN];
    if (NS_FAILED(Get(aArgv0, path))) {
      return nullptr;
    }
    UniqueFreePtr<char> result;
    result.reset(strdup(path));
    return result;
  }

#ifdef MOZILLA_INTERNAL_API
  static nsresult GetFile(const char* aArgv0, nsIFile** aResult)
  {
    nsCOMPtr<nsIFile> lf;
#ifdef XP_WIN
    wchar_t exePath[MAXPATHLEN];
    nsresult rv = GetW(aArgv0, exePath);
#else
    char exePath[MAXPATHLEN];
    nsresult rv = Get(aArgv0, exePath);
#endif
    if (NS_FAILED(rv)) {
      return rv;
    }
#ifdef XP_WIN
    rv = NS_NewLocalFile(nsDependentString(exePath), true,
                         getter_AddRefs(lf));
#else
    rv = NS_NewNativeLocalFile(nsDependentCString(exePath), true,
                               getter_AddRefs(lf));
#endif
    if (NS_FAILED(rv)) {
      return rv;
    }
    NS_ADDREF(*aResult = lf);
    return NS_OK;
  }
#endif
};

} // namespace mozilla

#endif /* mozilla_BinaryPath_h */
