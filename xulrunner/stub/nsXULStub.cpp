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
#define strcasecmp _stricmp
#define PATH_SEPARATOR_CHAR '\\'
#define R_OK 04
#elif defined(XP_MACOSX)
#include <unistd.h>
#include <sys/stat.h>
#include <CoreFoundation/CoreFoundation.h>
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

#ifdef XP_WIN
#define XRE_DONT_PROTECT_DLL_LOAD
#include "nsWindowsWMain.cpp"
#endif

#ifdef XP_BEOS
#include <Entry.h>
#include <Path.h>
#endif

#define VERSION_MAXLEN 128

static void Output(PRBool isError, const char *fmt, ... )
{
  va_list ap;
  va_start(ap, fmt);

#if (defined(XP_WIN) && !MOZ_WINCONSOLE) || defined(WINCE)
  char msg[2048];

  vsnprintf(msg, sizeof(msg), fmt, ap);

  UINT flags = MB_OK;
  if (isError)
    flags |= MB_ICONERROR;
  else
    flags |= MB_ICONINFORMATION;

  wchar_t wide_msg[1024];
  MultiByteToWideChar(CP_ACP,
		      0,
		      msg,
		      -1,
		      wide_msg,
		      sizeof(wide_msg) / sizeof(wchar_t));
  
  MessageBoxW(NULL, wide_msg, L"XULRunner", flags);
#else
  vfprintf(stderr, fmt, ap);
#endif

  va_end(ap);
}

/**
 * Return true if |arg| matches the given argument name.
 */
static PRBool IsArg(const char* arg, const char* s)
{
  if (*arg == '-')
  {
    if (*++arg == '-')
      ++arg;
    return !strcasecmp(arg, s);
  }

#if defined(XP_WIN) || defined(XP_OS2)
  if (*arg == '/')
    return !strcasecmp(++arg, s);
#endif

  return PR_FALSE;
}

/**
 * Return true if |aDir| is a valid file/directory.
 */
static PRBool FolderExists(const char* aDir)
{
#ifdef XP_WIN
  wchar_t wideDir[MAX_PATH];
  MultiByteToWideChar(CP_UTF8, 0, aDir, -1, wideDir, MAX_PATH);
  DWORD fileAttrs = GetFileAttributesW(wideDir);
  return fileAttrs != INVALID_FILE_ATTRIBUTES;
#else
  return access(aDir, R_OK) == 0;
#endif
}

static nsresult GetRealPath(const char* appDataFile, char* *aResult)
{
#ifdef XP_WIN
  wchar_t wAppDataFile[MAX_PATH];
  wchar_t wIniPath[MAX_PATH];
  MultiByteToWideChar(CP_UTF8, 0, appDataFile, -1, wAppDataFile, MAX_PATH);
  _wfullpath(wIniPath, wAppDataFile, MAX_PATH);
  WideCharToMultiByte(CP_UTF8, 0, wIniPath, -1, *aResult, MAX_PATH, 0, 0);
#else
  struct stat fileStat;
  if (!realpath(appDataFile, *aResult) || stat(*aResult, &fileStat))
    return NS_ERROR_FAILURE;
#endif
  if (!*aResult || !**aResult)
    return NS_ERROR_FAILURE;

  return NS_OK;
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


#ifdef WINCE
void
ForwardToWindow(HWND wnd) {
  // For WinCE, we're stuck with providing our own argv[0] for the remote
  // command-line.
  WCHAR wPath[MAX_PATH] = L"dummy ";
  WCHAR *wCmd = ::GetCommandLineW();
  WCHAR wCwd[MAX_PATH];
  _wgetcwd(wCwd, MAX_PATH);

  // Construct a narrow UTF8 buffer <path> <commandline>\0<workingdir>\0
  size_t len = wcslen(wPath) + wcslen(wCmd) + wcslen(wCwd) + 2;
  WCHAR *wMsg = (WCHAR *)malloc(len * sizeof(*wMsg));
  wcscpy(wMsg, wPath);
  wcscpy(wMsg + wcslen(wPath), wCmd);                // The command line
  wcscpy(wMsg + wcslen(wPath) + wcslen(wCmd) + 1, wCwd); // Working dir

  // Then convert to UTF-8, assuming worst-case explosion of characters
  char *msg = (char *)malloc(len * 4);
  WideCharToMultiByte(CP_UTF8, 0, wMsg, len, msg, len * 4, NULL, NULL);

  // We used to set dwData to zero, when we didn't send the working dir.
  // Now we're using it as a version number.
  COPYDATASTRUCT cds = { 1, len, (void *)msg };

  // Bring the already running Mozilla process to the foreground.
  // nsWindow will restore the window (if minimized) and raise it.
  // for activating the existing window on wince we need "| 0x01"
  // see http://msdn.microsoft.com/en-us/library/ms940024.aspx for details
  ::SetForegroundWindow((HWND)(((ULONG) wnd) | 0x01));
  ::SendMessage(wnd, WM_COPYDATA, 0, (LPARAM)&cds);
  free(wMsg);
  free(msg);
}
#endif

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
  wchar_t wide_path[MAX_PATH];
  if (!::GetModuleFileNameW(NULL, wide_path, MAX_PATH))
    return 1;

  WideCharToMultiByte(CP_UTF8, 0, wide_path,-1,
		      iniPath, MAX_PATH, NULL, NULL);

#elif defined(XP_OS2)
   PPIB ppib;
   PTIB ptib;

   DosGetInfoBlocks(&ptib, &ppib);
   DosQueryModuleName(ppib->pib_hmte, sizeof(iniPath), iniPath);

#elif defined(XP_BEOS)
   BEntry e((const char *)argv[0], true); // traverse symlink
   BPath p;
   status_t err;
   err = e.GetPath(&p);
   NS_ASSERTION(err == B_OK, "realpath failed");

   if (err == B_OK)
     // p.Path returns a pointer, so use strcpy to store path in iniPath
     strcpy(iniPath, p.Path());

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

  greFound = FolderExists(greDir);

  strncpy(lastSlash, "application.ini", sizeof(iniPath) - (lastSlash - iniPath));

#endif

  // If -app parameter was passed in, it is now time to take it under 
  // consideration.
  const char *appDataFile;
  appDataFile = getenv("XUL_APP_FILE");
  if (!appDataFile || !*appDataFile) 
    if (argc > 1 && IsArg(argv[1], "app")) {
      if (argc == 2) {
        Output(PR_FALSE, "specify APP-FILE (optional)\n");
        return 1;
      }
      argv[1] = argv[0];
      ++argv;
      --argc;

      appDataFile = argv[1];
      argv[1] = argv[0];
      ++argv;
      --argc;

      char kAppEnv[MAXPATHLEN];
      snprintf(kAppEnv, MAXPATHLEN, "XUL_APP_FILE=%s", appDataFile);
      if (putenv(kAppEnv)) 
        Output(PR_FALSE, "Couldn't set %s.\n", kAppEnv);

      char *result = (char*) calloc(sizeof(char), MAXPATHLEN);
      if (NS_FAILED(GetRealPath(appDataFile, &result))) {
        Output(PR_TRUE, "Invalid application.ini path.\n");
        return 1;
      }
      
      // We have a valid application.ini path passed in to the -app parameter 
      // but not yet a valid greDir, so lets look for it also on the same folder
      // as the stub.
      if (!greFound) {
        lastSlash = strrchr(iniPath, PATH_SEPARATOR_CHAR);
        if (!lastSlash)
          return 1;

        *(++lastSlash) = '\0';

        snprintf(greDir, sizeof(greDir), "%s" XPCOM_DLL, iniPath);
        greFound = FolderExists(greDir);
      }
      
      // copy it back.
      strcpy(iniPath, result);
    }
  
  nsINIParser parser;
  rv = parser.Init(iniPath);
  if (NS_FAILED(rv)) {
    fprintf(stderr, "Could not read application.ini\n");
    return 1;
  }

#ifdef WINCE
  // On Windows Mobile and WinCE, we can save a lot of time by not
  // waiting for XUL and XPCOM to load up.  Let's see if we can find
  // an existing app window to forward our command-line to now.

  // Shouldn't attempt this if the -no-remote parameter has been provided.
  bool noRemote = false;
  for (int i = 1; i < argc; i++) {
    if (IsArg(argv[i], "no-remote")) {
      noRemote = true;
      break;
    }
  }

  if (!noRemote) {
    char windowName[512];  // Is there a const for appname like VERSION_MAXLEN?
    rv = parser.GetString("App", "Name", windowName, sizeof(windowName));
    if (NS_FAILED(rv)) {
      fprintf(stderr, "Couldn't figure out the application name\n");
      return 1;
    }

    // Lookup the hidden message window created by nsNativeAppSupport
    strncat(windowName, "MessageWindow", sizeof(windowName) - strlen(windowName));
    WCHAR wWindowName[512];
    MultiByteToWideChar(CP_UTF8, 0, windowName, -1, wWindowName, sizeof(wWindowName));
    HWND wnd = ::FindWindowW(wWindowName, NULL);
    if (wnd) {
      // Forward the command-line and bail out
      ForwardToWindow(wnd);
      return 0;
    }
  }
#endif

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
#ifdef XP_UNIX
    // Using a symlinked greDir will fail during startup. Not sure why, but if
    // we resolve the symlink, everything works as expected.
    char resolved_greDir[MAXPATHLEN] = "";  
    if (realpath(greDir, resolved_greDir) && *resolved_greDir) {
      strncpy(greDir, resolved_greDir, MAXPATHLEN);
    }
#endif
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
    if (rv == NS_ERROR_OUT_OF_MEMORY) {
      char applicationName[2000] = "this application";
      parser.GetString("App", "Name", applicationName, sizeof(applicationName));
      Output(PR_TRUE, "Not enough memory available to start %s.\n",
             applicationName);
    } else {
      Output(PR_TRUE, "Couldn't load XPCOM.\n");
    }
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
#ifdef XP_WIN
    // On Windows and Windows CE, iniPath is UTF-8 encoded,
    // so we need to convert it.
    rv = NS_NewLocalFile(NS_ConvertUTF8toUTF16(iniPath), PR_FALSE,
                         getter_AddRefs(iniFile));
#else
    rv = NS_NewNativeLocalFile(nsDependentCString(iniPath), PR_FALSE,
                               getter_AddRefs(iniFile));
#endif
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
#ifdef XP_WIN
      // same as iniPath.
      NS_NewLocalFile(NS_ConvertUTF8toUTF16(greDir), PR_FALSE,
                      &appData->xreDirectory);
#else
      NS_NewNativeLocalFile(nsDependentCString(greDir), PR_FALSE,
                            &appData->xreDirectory);
#endif
    }

    retval = XRE_main(argc, argv, appData);
  }

  NS_LogTerm();

  XPCOMGlueShutdown();

  return retval;
}
