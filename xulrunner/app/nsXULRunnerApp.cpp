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
#include "nsRegisterGRE.h"
#include "nsAppRunner.h"
#include "nsINIParser.h"
#include "nsILocalFile.h"
#include "nsCOMPtr.h"
#include "nsMemory.h"
#include "nsBuildID.h"
#include "plstr.h"
#include "prprf.h"
#include "prenv.h"

/**
 * Output a string to the user.  This method is really only meant to be used to
 * output last-ditch error messages designed for developers NOT END USERS.
 *
 * @param isError
 *        Pass true to indicate severe errors.
 * @param fmt
 *        printf-style format string followed by arguments.
 */
static void Output(PRBool isError, const char *fmt, ... )
{
  va_list ap;
  va_start(ap, fmt);

#if defined(XP_WIN) && !MOZ_WINCONSOLE
  char *msg = PR_vsmprintf(fmt, ap);
  if (msg)
  {
    UINT flags = MB_OK;
    if (isError)
      flags |= MB_ICONERROR;
    else 
      flags |= MB_ICONINFORMATION;
    MessageBox(NULL, msg, "XULRunner", flags);
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
static PRBool IsArg(const char* arg, const char* s)
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

/**
 * Version checking.
 */

static PRUint32 geckoVersion = 0;

static PRUint32 ParseVersion(const char *versionStr)
{
  PRUint16 major, minor;
  if (PR_sscanf(versionStr, "%hu.%hu", &major, &minor) != 2) {
    NS_WARNING("invalid version string");
    return 0;
  }

  return PRUint32(major) << 16 | PRUint32(minor);
}

static PRBool CheckMinVersion(const char *versionStr)
{
  PRUint32 v = ParseVersion(versionStr);
  return geckoVersion >= v;
}

static PRBool CheckMaxVersion(const char *versionStr)
{
  PRUint32 v = ParseVersion(versionStr);
  return geckoVersion <= v;
}

/**
 * Parse application data.
 */
static int LoadAppData(const char* appDataFile, nsXREAppData* aResult,
                       nsCString& vendor, nsCString& name, nsCString& version,
                       nsCString& buildID, nsCString& appID,
                       nsCString& copyright)
{
  nsresult rv;

  nsCOMPtr<nsILocalFile> lf;
  XRE_GetFileFromPath(appDataFile, getter_AddRefs(lf));
  if (!lf)
    return 2;

  nsCOMPtr<nsIFile> appDir;
  rv = lf->GetParent(getter_AddRefs(appDir));
  if (NS_FAILED(rv))
    return 2;

  rv = CallQueryInterface(appDir, &aResult->directory);
  if (NS_FAILED(rv))
    return 2;

  nsINIParser parser;
  rv = parser.Init(lf);
  if (NS_FAILED(rv))
    return 2;

  // Gecko version checking
  //
  // TODO: If these version checks fail, then look for a compatible XULRunner
  //       version on the system, and launch it instead.

  nsCAutoString gkVersion;
  rv = parser.GetString("Gecko", "MinVersion", gkVersion);

  if (NS_FAILED(rv) || !CheckMinVersion(gkVersion.get())) {
    Output(PR_TRUE, "Error: Gecko MinVersion requirement not met.\n");
    return 1;
  }

  rv = parser.GetString("Gecko", "MaxVersion", gkVersion);
  if (NS_SUCCEEDED(rv) && !CheckMaxVersion(gkVersion.get())) {
    Output(PR_TRUE, "Error: Gecko MaxVersion requirement not met.\n");
    return 1;
  }

  PRUint32 i;

  // Read string-valued fields
  const struct {
    const char *key;
    const char **fill;
    nsCString  &buf;
    PRBool      required;
  } string_fields[] = {
    { "Vendor",    &aResult->vendor,    vendor,    PR_FALSE },
    { "Name",      &aResult->name,      name,      PR_TRUE  },
    { "Version",   &aResult->version,   version,   PR_FALSE },
    { "BuildID",   &aResult->buildID,   buildID,   PR_TRUE  },
    { "ID",        &aResult->ID,        appID,     PR_FALSE },
    { "Copyright", &aResult->copyright, copyright, PR_FALSE }
  };
  for (i = 0; i < NS_ARRAY_LENGTH(string_fields); ++i) {
    rv = parser.GetString("App", string_fields[i].key,
                          string_fields[i].buf);
    if (NS_SUCCEEDED(rv)) {
      *string_fields[i].fill = string_fields[i].buf.get();
    }
    else if (string_fields[i].required) {
      Output(PR_TRUE, "Error: %x: No \"%s\" field.\n",
             rv, string_fields[i].key);
      return 1;
    }
  }

  // Read boolean-valued fields
  const struct {
    const char* key;
    PRUint32 flag;
  } boolean_fields[] = {
    { "EnableProfileMigrator",  NS_XRE_ENABLE_PROFILE_MIGRATOR  },
    { "EnableExtensionManager", NS_XRE_ENABLE_EXTENSION_MANAGER }
  };
  char buf[6]; // large enough to hold "false"
  aResult->flags = 0;
  for (i = 0; i < NS_ARRAY_LENGTH(boolean_fields); ++i) {
    rv = parser.GetString("XRE", boolean_fields[i].key, buf, sizeof(buf));
    // accept a truncated result since we are only interested in the
    // first character.  this is designed to allow the possibility of
    // expanding these boolean attributes to express additional options.
    if ((NS_SUCCEEDED(rv) || rv == NS_ERROR_LOSS_OF_SIGNIFICANT_DATA) &&
        (buf[0] == '1' || buf[0] == 't' || buf[0] == 'T')) {
      aResult->flags |= boolean_fields[i].flag;
    }
  } 

#ifdef DEBUG
  printf("---------------------------------------------------------\n");
  printf("     Vendor %s\n", aResult->vendor);
  printf("       Name %s\n", aResult->name);
  printf("    Version %s\n", aResult->version);
  printf("    BuildID %s\n", aResult->buildID);
  printf("  Copyright %s\n", aResult->copyright);
  printf("      Flags %08x\n", aResult->flags);
  printf("---------------------------------------------------------\n");
#endif

  return 0;
}

static void Usage()
{
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
           "  --register-global          register this GRE in the machine registry\n"
           "  --register-user            register this GRE in the user registry\n"
           "  --unregister-global        unregister this GRE formerly registered with\n"
           "                             --register-global\n"
           "  --unregister-user          unregister this GRE formely registered with\n"
           "                             --register-user\n"
           "  --find-gre <version>       Find a GRE with version <version> and print\n"
           "                             the path on stdout\n"
           "\n"
           "APP-FILE\n"
           "  Application initialization file.\n"
           "\n"
           "APP-OPTIONS\n"
           "  Application specific options.\n",
           GRE_BUILD_ID);
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

int main(int argc, char* argv[])
{
  if (argc > 1 && (IsArg(argv[1], "h") ||
                   IsArg(argv[1], "help") ||
                   IsArg(argv[1], "?")))
  {
    Usage();
    return 0;
  }

  if (argc == 2 && (IsArg(argv[1], "v") || IsArg(argv[1], "version")))
  {
    Output(PR_FALSE, "Mozilla XULRunner %s\n", GRE_BUILD_ID);
    return 0;
  }

  if (argc > 1) {
    PRBool registerGlobal = IsArg(argv[1], "register-global");
    PRBool registerUser   = IsArg(argv[1], "register-user");
    if (registerGlobal || registerUser) {
      if (argc != 2) {
        Usage();
        return 1;
      }

      nsCOMPtr<nsIFile> regDir;
      nsresult rv = GetXULRunnerDir(argv[0], getter_AddRefs(regDir));
      if (NS_FAILED(rv))
        return 2;

      return RegisterXULRunner(registerGlobal, regDir) ? 0 : 2;
    }

    registerGlobal = IsArg(argv[1], "unregister-global");
    registerUser   = IsArg(argv[1], "unregister-user");
    if (registerGlobal || registerUser) {
      if (argc != 2) {
        Usage();
        return 1;
      }

      nsCOMPtr<nsIFile> regDir;
      nsresult rv = GetXULRunnerDir(argv[0], getter_AddRefs(regDir));
      if (NS_FAILED(rv))
        return 2;
      UnregisterXULRunner(registerGlobal, regDir);
      return 0;
    }

    if (IsArg(argv[1], "find-gre")) {
      if (argc != 3) {
        Usage();
        return 1;
      }

      char version[MAXPATHLEN];
      nsresult rv = GRE_GetGREPathForVersion(argv[2], version, MAXPATHLEN);
      if (NS_FAILED(rv))
        return 1;

      printf("%s\n", version);
      return 0;
    }

    if (IsArg(argv[1], "gre-version")) {
      if (argc != 2) {
        Usage();
        return 1;
      }

      printf("%s\n", GRE_BUILD_ID);
      return 0;
    }
  }

  geckoVersion = ParseVersion(GRE_BUILD_ID);

  const char *appDataFile = PR_GetEnv("XUL_APP_FILE");

  if (!(appDataFile && *appDataFile)) {
    if (argc < 2) {
      Usage();
      return 1;
    }

    if (IsArg(argv[1], "app")) {
      if (argc == 2) {
        Usage();
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

  nsCAutoString vendor, name, version, buildID, appID, copyright;

  nsXREAppData appData = { sizeof(nsXREAppData), 0 };

  int rv = LoadAppData(appDataFile, &appData,
                       vendor, name, version, buildID, appID, copyright);
  if (!rv)
    rv = XRE_main(argc, argv, &appData);

  NS_IF_RELEASE(appData.directory);

  return rv;
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
