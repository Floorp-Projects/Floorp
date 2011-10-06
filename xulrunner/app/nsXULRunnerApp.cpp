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
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by IBM Corporation are Copyright (C) 2003
 * IBM Corporation. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@meer.net>
 *   Benjamin Smedberg <benjamin@smedbergs.us>
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

#include <stdio.h>
#include <stdlib.h>
#ifdef XP_WIN
#include <windows.h>
#endif

#include "nsXULAppAPI.h"
#include "nsXPCOMGlue.h"
#include "nsAppRunner.h"
#include "nsILocalFile.h"
#include "nsIXULAppInstall.h"
#include "nsCOMPtr.h"
#include "nsMemory.h"
#include "nsCRTGlue.h"
#include "nsStringAPI.h"
#include "nsServiceManagerUtils.h"
#include "plstr.h"
#include "prprf.h"
#include "prenv.h"
#include "nsINIParser.h"

#ifdef XP_WIN
#include "nsWindowsWMain.cpp"
#endif

/**
 * Output a string to the user.  This method is really only meant to be used to
 * output last-ditch error messages designed for developers NOT END USERS.
 *
 * @param isError
 *        Pass true to indicate severe errors.
 * @param fmt
 *        printf-style format string followed by arguments.
 */
static void Output(bool isError, const char *fmt, ... )
{
  va_list ap;
  va_start(ap, fmt);

#if (defined(XP_WIN) && !MOZ_WINCONSOLE)
  char *msg = PR_vsmprintf(fmt, ap);
  if (msg)
  {
    UINT flags = MB_OK;
    if (isError)
      flags |= MB_ICONERROR;
    else
      flags |= MB_ICONINFORMATION;
    
    wchar_t wide_msg[2048];
    MultiByteToWideChar(CP_ACP,
			0,
			msg,
			-1,
			wide_msg,
			sizeof(wide_msg) / sizeof(wchar_t));
    
    MessageBoxW(NULL, wide_msg, L"XULRunner", flags);

    PR_smprintf_free(msg);
  }
#else
  vfprintf(stderr, fmt, ap);
#endif

  va_end(ap);
}

/**
 * Return true if |arg| matches the given argument name.
 */
static bool IsArg(const char* arg, const char* s)
{
  if (*arg == '-')
  {
    if (*++arg == '-')
      ++arg;
    return !PL_strcasecmp(arg, s);
  }

#if defined(XP_WIN) || defined(XP_OS2)
  if (*arg == '/')
    return !PL_strcasecmp(++arg, s);
#endif

  return PR_FALSE;
}

static nsresult
GetGREVersion(const char *argv0,
              nsACString *aMilestone,
              nsACString *aVersion)
{
  if (aMilestone)
    aMilestone->Assign("<Error>");
  if (aVersion)
    aVersion->Assign("<Error>");

  nsCOMPtr<nsILocalFile> iniFile;
  nsresult rv = XRE_GetBinaryPath(argv0, getter_AddRefs(iniFile));
  if (NS_FAILED(rv))
    return rv;

  iniFile->SetNativeLeafName(NS_LITERAL_CSTRING("platform.ini"));

  nsINIParser parser;
  rv = parser.Init(iniFile);
  if (NS_FAILED(rv))
    return rv;

  if (aMilestone) {
    rv = parser.GetString("Build", "Milestone", *aMilestone);
    if (NS_FAILED(rv))
      return rv;
  }
  if (aVersion) {
    rv = parser.GetString("Build", "BuildID", *aVersion);
    if (NS_FAILED(rv))
      return rv;
  }
  return NS_OK;
}

static void Usage(const char *argv0)
{
    nsCAutoString milestone;
    GetGREVersion(argv0, &milestone, nsnull);

    // display additional information (XXX make localizable?)
    Output(PR_FALSE,
           "Mozilla XULRunner %s\n\n"
           "Usage: " XULRUNNER_PROGNAME " [OPTIONS]\n"
           "       " XULRUNNER_PROGNAME " APP-FILE [APP-OPTIONS...]\n"
           "\n"
           "OPTIONS\n"
           "      --app                  specify APP-FILE (optional)\n"
           "  -h, --help                 show this message\n"
           "  -v, --version              show version\n"
           "  --gre-version              print the GRE version string on stdout\n"
           "  --install-app <application> [<destination> [<directoryname>]]\n"
           "                             Install a XUL application.\n"
           "\n"
           "APP-FILE\n"
           "  Application initialization file.\n"
           "\n"
           "APP-OPTIONS\n"
           "  Application specific options.\n",
           milestone.get());
}

static nsresult
GetXULRunnerDir(const char *argv0, nsIFile* *aResult)
{
  nsresult rv;

  nsCOMPtr<nsILocalFile> appFile;
  rv = XRE_GetBinaryPath(argv0, getter_AddRefs(appFile));
  if (NS_FAILED(rv)) {
    Output(PR_TRUE, "Could not find XULRunner application path.\n");
    return rv;
  }

  rv = appFile->GetParent(aResult);
  if (NS_FAILED(rv)) {
    Output(PR_TRUE, "Could not find XULRunner installation dir.\n");
  }
  return rv;
}

static int
InstallXULApp(nsIFile* aXULRunnerDir,
              const char *aAppLocation,
              const char *aInstallTo,
              const char *aLeafName)
{
  nsCOMPtr<nsILocalFile> appLocation;
  nsCOMPtr<nsILocalFile> installTo;
  nsString leafName;

  nsresult rv = XRE_GetFileFromPath(aAppLocation, getter_AddRefs(appLocation));
  if (NS_FAILED(rv))
    return 2;

  if (aInstallTo) {
    rv = XRE_GetFileFromPath(aInstallTo, getter_AddRefs(installTo));
    if (NS_FAILED(rv))
      return 2;
  }

  if (aLeafName)
    NS_CStringToUTF16(nsDependentCString(aLeafName),
                      NS_CSTRING_ENCODING_NATIVE_FILESYSTEM, leafName);

  rv = NS_InitXPCOM2(nsnull, aXULRunnerDir, nsnull);
  if (NS_FAILED(rv))
    return 3;

  {
    // Scope our COMPtr to avoid holding XPCOM refs beyond xpcom shutdown
    nsCOMPtr<nsIXULAppInstall> install
      (do_GetService("@mozilla.org/xulrunner/app-install-service;1"));
    if (!install) {
      rv = NS_ERROR_FAILURE;
    }
    else {
      rv = install->InstallApplication(appLocation, installTo, leafName);
    }
  }

  NS_ShutdownXPCOM(nsnull);

  if (NS_FAILED(rv))
    return 3;

  return 0;
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

int main(int argc, char* argv[])
{
  if (argc > 1 && (IsArg(argv[1], "h") ||
                   IsArg(argv[1], "help") ||
                   IsArg(argv[1], "?")))
  {
    Usage(argv[0]);
    return 0;
  }

  if (argc == 2 && (IsArg(argv[1], "v") || IsArg(argv[1], "version")))
  {
    nsCAutoString milestone;
    nsCAutoString version;
    GetGREVersion(argv[0], &milestone, &version);
    Output(PR_FALSE, "Mozilla XULRunner %s - %s\n",
           milestone.get(), version.get());
    return 0;
  }

  if (argc > 1) {
    nsCAutoString milestone;
    nsresult rv = GetGREVersion(argv[0], &milestone, nsnull);
    if (NS_FAILED(rv))
      return 2;

    if (IsArg(argv[1], "gre-version")) {
      if (argc != 2) {
        Usage(argv[0]);
        return 1;
      }

      printf("%s\n", milestone.get());
      return 0;
    }

    if (IsArg(argv[1], "install-app")) {
      if (argc < 3 || argc > 5) {
        Usage(argv[0]);
        return 1;
      }

      char *appLocation = argv[2];

      char *installTo = nsnull;
      if (argc > 3) {
        installTo = argv[3];
        if (!*installTo) // left blank?
          installTo = nsnull;
      }

      char *leafName = nsnull;
      if (argc > 4) {
        leafName = argv[4];
        if (!*leafName)
          leafName = nsnull;
      }

      nsCOMPtr<nsIFile> regDir;
      rv = GetXULRunnerDir(argv[0], getter_AddRefs(regDir));
      if (NS_FAILED(rv))
        return 2;

      return InstallXULApp(regDir, appLocation, installTo, leafName);
    }
  }

  const char *appDataFile = PR_GetEnv("XUL_APP_FILE");

  if (!(appDataFile && *appDataFile)) {
    if (argc < 2) {
      Usage(argv[0]);
      return 1;
    }

    if (IsArg(argv[1], "app")) {
      if (argc == 2) {
        Usage(argv[0]);
        return 1;
      }
      argv[1] = argv[0];
      ++argv;
      --argc;
    }

    appDataFile = argv[1];
    argv[1] = argv[0];
    ++argv;
    --argc;

    static char kAppEnv[MAXPATHLEN];
    PR_snprintf(kAppEnv, MAXPATHLEN, "XUL_APP_FILE=%s", appDataFile);
    PR_SetEnv(kAppEnv);
  }

  nsCOMPtr<nsILocalFile> appDataLF;
  nsresult rv = XRE_GetFileFromPath(appDataFile, getter_AddRefs(appDataLF));
  if (NS_FAILED(rv)) {
    Output(PR_TRUE, "Error: unrecognized application.ini path.\n");
    return 2;
  }

  AutoAppData appData(appDataLF);
  if (!appData) {
    Output(PR_TRUE, "Error: couldn't parse application.ini.\n");
    return 2;
  }

  return XRE_main(argc, argv, appData);
}
