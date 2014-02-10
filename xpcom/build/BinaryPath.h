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

namespace mozilla {

class BinaryPath {
public:
#ifdef XP_WIN
  static nsresult Get(const char *argv0, char aResult[MAXPATHLEN])
  {
    wchar_t wide_path[MAXPATHLEN];
    nsresult rv = GetW(argv0, wide_path);
    if (NS_FAILED(rv))
      return rv;
    WideCharToMultiByte(CP_UTF8, 0, wide_path, -1,
                        aResult, MAXPATHLEN, nullptr, nullptr);
    return NS_OK;
  }

private:
  static nsresult GetW(const char *argv0, wchar_t aResult[MAXPATHLEN])
  {
    if (::GetModuleFileNameW(0, aResult, MAXPATHLEN))
      return NS_OK;
    return NS_ERROR_FAILURE;
  }

#elif defined(XP_MACOSX)
  static nsresult Get(const char *argv0, char aResult[MAXPATHLEN])
  {
    // Works even if we're not bundled.
    CFBundleRef appBundle = CFBundleGetMainBundle();
    if (!appBundle)
      return NS_ERROR_FAILURE;

    CFURLRef executableURL = CFBundleCopyExecutableURL(appBundle);
    if (!executableURL)
      return NS_ERROR_FAILURE;

    nsresult rv;
    if (CFURLGetFileSystemRepresentation(executableURL, false, (UInt8 *)aResult, MAXPATHLEN))
      rv = NS_OK;
    else
      rv = NS_ERROR_FAILURE;
    CFRelease(executableURL);
    return rv;
  }

#elif defined(ANDROID)
  static nsresult Get(const char *argv0, char aResult[MAXPATHLEN])
  {
    // On Android, we use the GRE_HOME variable that is set by the Java
    // bootstrap code.
    const char *greHome = getenv("GRE_HOME");
#if defined(MOZ_WIDGET_GONK)
    if (!greHome)
      greHome = "/system/b2g";
#endif

    if (!greHome)
      return NS_ERROR_FAILURE;

    snprintf(aResult, MAXPATHLEN, "%s/%s", greHome, "dummy");
    aResult[MAXPATHLEN-1] = '\0';
    return NS_OK;
  }

#elif defined(XP_UNIX)
  static nsresult Get(const char *argv0, char aResult[MAXPATHLEN])
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
    if (strchr(argv0, '/') && realpath(argv0, aResult) &&
        stat(aResult, &fileStat) == 0)
      return NS_OK;

    const char *path = getenv("PATH");
    if (!path)
      return NS_ERROR_FAILURE;

    char *pathdup = strdup(path);
    if (!pathdup)
      return NS_ERROR_OUT_OF_MEMORY;

    bool found = false;
    char *token = strtok(pathdup, ":");
    while (token) {
      char tmpPath[MAXPATHLEN];
      sprintf(tmpPath, "%s/%s", token, argv0);
      if (realpath(tmpPath, aResult) && stat(aResult, &fileStat) == 0) {
        found = true;
        break;
      }
      token = strtok(nullptr, ":");
    }
    free(pathdup);
    if (found)
      return NS_OK;
    return NS_ERROR_FAILURE;
  }

#else
#error Oops, you need platform-specific code here
#endif

public:
  static nsresult GetFile(const char *argv0, nsIFile* *aResult)
  {
    nsCOMPtr<nsIFile> lf;
#ifdef XP_WIN
    wchar_t exePath[MAXPATHLEN];
    nsresult rv = GetW(argv0, exePath);
#else
    char exePath[MAXPATHLEN];
    nsresult rv = Get(argv0, exePath);
#endif
    if (NS_FAILED(rv))
      return rv;
#ifdef XP_WIN
    rv = NS_NewLocalFile(nsDependentString(exePath), true,
                         getter_AddRefs(lf));
#else
    rv = NS_NewNativeLocalFile(nsDependentCString(exePath), true,
                               getter_AddRefs(lf));
#endif
    if (NS_FAILED(rv))
      return rv;
    NS_ADDREF(*aResult = lf);
    return NS_OK;
  }
};

}

#endif /* mozilla_BinaryPath_h */
