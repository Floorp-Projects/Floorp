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
 * The Original Code is Mozilla XULRunner.
 *
 * The Initial Developer of the Original Code is
 * Benjamin Smedberg <benjamin@smedbergs.us>
 *
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Mozilla Foundation <http://www.mozilla.org/>. All Rights Reserved.
 *
 * Contributor(s):
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

#include "nsXPCOMGlue.h"
#include "nsINIParser.h"
#include "prtypes.h"
#include "nsXPCOMPrivate.h" // for XP MAXPATHLEN
#include "nsMemory.h" // for NS_ARRAY_LENGTH
#include "nsXULAppAPI.h"
#include "nsILocalFile.h"

#include <stdarg.h>

#ifdef XP_WIN
#include <windows.h>
#include <io.h>
#define snprintf _snprintf
#define vsnprintf _vsnprintf
#define PATH_SEPARATOR_CHAR '\\'
#include "nsWindowsRestart.cpp"
#define R_OK 04
#elif defined(XP_MACOSX)
#include <CFBundle.h>
#define PATH_SEPARATOR_CHAR '/'
#elif defined (XP_OS2)
#define INCL_DOS
#define INCL_DOSMISC
#define INCL_DOSERRORS
#include <os2.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#define PATH_SEPARATOR_CHAR '\\'
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#define PATH_SEPARATOR_CHAR '/'
#endif

#define VERSION_MAXLEN 128

static void Output(PRBool isError, const char *fmt, ... )
{
  va_list ap;
  va_start(ap, fmt);

#if defined(XP_WIN) && !MOZ_WINCONSOLE
  char msg[2048];

  vsnprintf(msg, sizeof(msg), fmt, ap);

  UINT flags = MB_OK;
  if (isError)
    flags |= MB_ICONERROR;
  else
    flags |= MB_ICONINFORMATION;
  MessageBox(NULL, msg, "XULRunner", flags);
#else
  vfprintf(stderr, fmt, ap);
#endif

  va_end(ap);
}

class AutoAppData
{
public:
  AutoAppData(nsILocalFile* aINIFile) : mAppData(nsnull) {
    nsresult rv = XRE_CreateAppData(aINIFile, &mAppData);
    if (NS_FAILED(rv))
      mAppData = nsnull;
  }
  ~AutoAppData() {
    if (mAppData)
      XRE_FreeAppData(mAppData);
  }

  operator nsXREAppData*() const { return mAppData; }
  nsXREAppData* operator -> () const { return mAppData; }

private:
  nsXREAppData* mAppData;
};

XRE_CreateAppDataType XRE_CreateAppData;
XRE_FreeAppDataType XRE_FreeAppData;
XRE_mainType XRE_main;

int
main(int argc, char **argv)
{
  nsresult rv;
  char *lastSlash;

  char iniPath[MAXPATHLEN];
  char tmpPath[MAXPATHLEN];
  char greDir[MAXPATHLEN];
  PRBool greFound = PR_FALSE;

#if defined(XP_MACOSX)
  CFBundleRef appBundle = CFBundleGetMainBundle();
  if (!appBundle)
    return 1;

  CFURLRef resourcesURL = CFBundleCopyResourcesDirectoryURL(appBundle);
  if (!resourcesURL)
    return 1;

  CFURLRef absResourcesURL = CFURLCopyAbsoluteURL(resourcesURL);
  CFRelease(resourcesURL);
  if (!absResourcesURL)
    return 1;

  CFURLRef iniFileURL =
    CFURLCreateCopyAppendingPathComponent(kCFAllocatorDefault,
                                          absResourcesURL,
                                          CFSTR("application.ini"),
                                          false);
  CFRelease(absResourcesURL);
  if (!iniFileURL)
    return 1;

  CFStringRef iniPathStr =
    CFURLCopyFileSystemPath(iniFileURL, kCFURLPOSIXPathStyle);
  CFRelease(iniFileURL);
  if (!iniPathStr)
    return 1;

  CFStringGetCString(iniPathStr, iniPath, sizeof(iniPath),
                     kCFStringEncodingUTF8);
  CFRelease(iniPathStr);

#else

#ifdef XP_WIN
  if (!::GetModuleFileName(NULL, iniPath, sizeof(iniPath)))
    return 1;

#elif defined(XP_OS2)
   PPIB ppib;
   PTIB ptib;

   DosGetInfoBlocks(&ptib, &ppib);
   DosQueryModuleName(ppib->pib_hmte, sizeof(iniPath), iniPath);

#else
  // on unix, there is no official way to get the path of the current binary.
  // instead of using the MOZILLA_FIVE_HOME hack, which doesn't scale to
  // multiple applications, we will try a series of techniques:
  //
  // 1) use realpath() on argv[0], which works unless we're loaded from the
  //    PATH
  // 2) manually walk through the PATH and look for ourself
  // 3) give up

  struct stat fileStat;

  if (!realpath(argv[0], iniPath) || stat(iniPath, &fileStat)) {
    const char *path = getenv("PATH");
    if (!path)
      return 1;

    char *pathdup = strdup(path);
    if (!pathdup)
      return 1;

    PRBool found = PR_FALSE;
    char *token = strtok(pathdup, ":");
    while (token) {
      sprintf(tmpPath, "%s/%s", token, argv[0]);
      if (realpath(tmpPath, iniPath) && stat(iniPath, &fileStat) == 0) {
        found = PR_TRUE;
        break;
      }
      token = strtok(NULL, ":");
    }
    free (pathdup);
    if (!found)
      return 1;
  }
#endif

  lastSlash = strrchr(iniPath, PATH_SEPARATOR_CHAR);
  if (!lastSlash)
    return 1;

  *(++lastSlash) = '\0';

  // On Linux/Win, look for XULRunner in appdir/xulrunner

  snprintf(greDir, sizeof(greDir),
           "%sxulrunner" XPCOM_FILE_PATH_SEPARATOR XPCOM_DLL,
           iniPath);

  greFound = (access(greDir, R_OK) == 0);

  strncpy(lastSlash, "application.ini", sizeof(iniPath) - (lastSlash - iniPath));

#endif

  nsINIParser parser;
  rv = parser.Init(iniPath);
  if (NS_FAILED(rv)) {
    fprintf(stderr, "Could not read application.ini\n");
    return 1;
  }

  if (!greFound) {
    char minVersion[VERSION_MAXLEN];

    // If a gecko maxVersion is not specified, we assume that the app uses only
    // frozen APIs, and is therefore compatible with any xulrunner 1.x.
    char maxVersion[VERSION_MAXLEN] = "1.*";

    GREVersionRange range = {
      minVersion,
      PR_TRUE,
      maxVersion,
      PR_TRUE
    };

    rv = parser.GetString("Gecko", "MinVersion", minVersion, sizeof(minVersion));
    if (NS_FAILED(rv)) {
      fprintf(stderr,
              "The application.ini does not specify a [Gecko] MinVersion\n");
      return 1;
    }

    rv = parser.GetString("Gecko", "MaxVersion", maxVersion, sizeof(maxVersion));
    if (NS_SUCCEEDED(rv))
      range.upperInclusive = PR_TRUE;

    static const GREProperty kProperties[] = {
      { "xulrunner", "true" }
    };

    rv = GRE_GetGREPathWithProperties(&range, 1,
                                      kProperties, NS_ARRAY_LENGTH(kProperties),
                                      greDir, sizeof(greDir));
    if (NS_FAILED(rv)) {
      // XXXbsmedberg: Do something much smarter here: notify the
      // user/offer to download/?

      Output(PR_FALSE,
             "Could not find compatible GRE between version %s and %s.\n",
             range.lower, range.upper);
      return 1;
    }
  }
#ifdef XP_OS2
  // On OS/2 we need to set BEGINLIBPATH to be able to find XULRunner DLLs
  strcpy(tmpPath, greDir);
  lastSlash = strrchr(tmpPath, PATH_SEPARATOR_CHAR);
  if (lastSlash) {
    *lastSlash = '\0';
  }
  DosSetExtLIBPATH(tmpPath, BEGIN_LIBPATH);
#endif

  rv = XPCOMGlueStartup(greDir);
  if (NS_FAILED(rv)) {
    Output(PR_TRUE, "Couldn't load XPCOM.\n");
    return 1;
  }

  static const nsDynamicFunctionLoad kXULFuncs[] = {
    { "XRE_CreateAppData", (NSFuncPtr*) &XRE_CreateAppData },
    { "XRE_FreeAppData", (NSFuncPtr*) &XRE_FreeAppData },
    { "XRE_main", (NSFuncPtr*) &XRE_main },
    { nsnull, nsnull }
  };

  rv = XPCOMGlueLoadXULFunctions(kXULFuncs);
  if (NS_FAILED(rv)) {
    Output(PR_TRUE, "Couldn't load XRE functions.\n");
    return 1;
  }

  NS_LogInit();

  int retval;

  { // Scope COMPtr and AutoAppData
    nsCOMPtr<nsILocalFile> iniFile;
    rv = NS_NewNativeLocalFile(nsDependentCString(iniPath), PR_FALSE,
                               getter_AddRefs(iniFile));
    if (NS_FAILED(rv)) {
      Output(PR_TRUE, "Couldn't find application.ini file.\n");
      return 1;
    }

    AutoAppData appData(iniFile);
    if (!appData) {
      Output(PR_TRUE, "Error: couldn't parse application.ini.\n");
      return 1;
    }

    NS_ASSERTION(appData->directory, "Failed to get app directory.");

    if (!appData->xreDirectory) {
      // chop "libxul.so" off the GRE path
      lastSlash = strrchr(greDir, PATH_SEPARATOR_CHAR);
      if (lastSlash) {
        *lastSlash = '\0';
      }
      NS_NewNativeLocalFile(nsDependentCString(greDir), PR_FALSE,
                            &appData->xreDirectory);
    }

    retval = XRE_main(argc, argv, appData);
  }

  NS_LogTerm();

  XPCOMGlueShutdown();

  return retval;
}

#if defined( XP_WIN ) && defined( WIN32 ) && !defined(__GNUC__)
// We need WinMain in order to not be a console app.  This function is
// unused if we are a console application.
int WINAPI WinMain( HINSTANCE, HINSTANCE, LPSTR args, int )
{
  // Do the real work.
  return main( __argc, __argv );
}
#endif
