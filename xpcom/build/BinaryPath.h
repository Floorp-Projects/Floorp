/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mike Hommey <mh@glandium.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef mozilla_BinaryPath_h
#define mozilla_BinaryPath_h

#include "nsXPCOMPrivate.h" // for MAXPATHLEN
#include "prtypes.h"
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
    PRUnichar wide_path[MAXPATHLEN];
    nsresult rv = GetW(argv0, wide_path);
    if (NS_FAILED(rv))
      return rv;
    WideCharToMultiByte(CP_UTF8, 0, wide_path, -1,
                        aResult, MAXPATHLEN, NULL, NULL);
    return NS_OK;
  }

private:
  static nsresult GetW(const char *argv0, PRUnichar aResult[MAXPATHLEN])
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

#elif defined(MOZ_WIDGET_ANDROID)
  static nsresult Get(const char *argv0, char aResult[MAXPATHLEN])
  {
    // On Android, we use the GRE_HOME variable that is set by the Java
    // bootstrap code.
    const char *greHome = getenv("GRE_HOME");
    if (!greHome)
      return NS_ERROR_FAILURE;

    snprintf(aResult, MAXPATHLEN, "%s/%s", greHome, "dummy");
    aResult[MAXPATHLEN] = '\0';
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
      token = strtok(NULL, ":");
    }
    free(pathdup);
    if (found)
      return NS_OK;
    return NS_ERROR_FAILURE;
  }

#elif defined(XP_OS2)
  static nsresult Get(const char *argv0, char aResult[MAXPATHLEN])
  {
    PPIB ppib;
    PTIB ptib;
    DosGetInfoBlocks( &ptib, &ppib);
    DosQueryModuleName(ppib->pib_hmte, MAXPATHLEN, aResult);
  }

#else
#error Oops, you need platform-specific code here
#endif

public:
  static nsresult GetFile(const char *argv0, nsILocalFile* *aResult)
  {
    nsCOMPtr<nsILocalFile> lf;
#ifdef XP_WIN
    PRUnichar exePath[MAXPATHLEN];
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
