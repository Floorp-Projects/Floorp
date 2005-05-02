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
#include "nsAppRunner.h"
#include "nsINIParser.h"
#include "nsILocalFile.h"
#include "nsCOMPtr.h"
#include "nsMemory.h"
#include "nsBuildID.h"
#include "plstr.h"
#include "prprf.h"

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
static const nsXREAppData* LoadAppData(const char* appDataFile)
{
  static char vendor[256], name[256], version[32], buildID[32], copyright[512];
  static nsXREAppData data = {
      vendor, name, version, buildID, {0,0,0,{0,0,0,0,0,0,0,0}}, copyright, 0 };
  
  nsCOMPtr<nsILocalFile> lf;
  XRE_GetFileFromPath(appDataFile, getter_AddRefs(lf));
  if (!lf)
    return nsnull;

  nsINIParser parser; 
  if (NS_FAILED(parser.Init(lf)))
    return nsnull;

  nsresult rv;

  // Gecko version checking
  //
  // TODO: If these version checks fail, then look for a compatible XULRunner
  //       version on the system, and launch it instead.

  char gkVersion[32];
  rv = parser.GetString("Gecko", "MinVersion", gkVersion, sizeof(gkVersion));
  if (NS_FAILED(rv) || !CheckMinVersion(gkVersion)) {
    Output(PR_TRUE, "Error: Gecko MinVersion requirement not met.\n");
    return nsnull;
  }

  rv = parser.GetString("Gecko", "MaxVersion", gkVersion, sizeof(gkVersion));
  if (NS_SUCCEEDED(rv) && !CheckMaxVersion(gkVersion)) {
    Output(PR_TRUE, "Error: Gecko MaxVersion requirement not met.\n");
    return nsnull;
  }

  // Read the app ID, if specified
  char id[38] = "";
  rv = parser.GetString("App", "ID", id, sizeof(id));
  if (NS_SUCCEEDED(rv)) {
    if(!data.id.Parse(id)) {
      memset(&data.id, 0, sizeof(data.id));
    }
  }

  PRUint32 i;

  // Read string-valued fields
  const struct {
    const char* key;
    char* buf;
    size_t bufLen;
    PRBool required;
  } string_fields[] = {
    { "Vendor",    vendor,    sizeof(vendor),    PR_FALSE },
    { "Name",      name,      sizeof(name),      PR_TRUE  },
    { "Version",   version,   sizeof(version),   PR_FALSE },
    { "BuildID",   buildID,   sizeof(buildID),   PR_TRUE  },
    { "Copyright", copyright, sizeof(copyright), PR_FALSE }
  };
  for (i = 0; i < NS_ARRAY_LENGTH(string_fields); ++i) {
    rv = parser.GetString("App", string_fields[i].key, string_fields[i].buf,
                          string_fields[i].bufLen);
    if (NS_FAILED(rv)) {
      if (string_fields[i].required) {
        Output(PR_TRUE, "Error: %x: No \"%s\" field.\n",
               rv, string_fields[i].key);
        return nsnull;
      } else {
        string_fields[i].buf[0] = '\0';
      }
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
  data.flags = 0;
  for (i = 0; i < NS_ARRAY_LENGTH(boolean_fields); ++i) {
    rv = parser.GetString("XRE", boolean_fields[i].key, buf, sizeof(buf));
    // accept a truncated result since we are only interested in the
    // first character.  this is designed to allow the possibility of
    // expanding these boolean attributes to express additional options.
    if ((NS_SUCCEEDED(rv) || rv == NS_ERROR_LOSS_OF_SIGNIFICANT_DATA) &&
        (buf[0] == '1' || buf[0] == 't' || buf[0] == 'T')) {
      data.flags |= boolean_fields[i].flag;
    }
  } 

#ifdef DEBUG
  printf("---------------------------------------------------------\n");
  printf("     Vendor %s\n", data.appVendor);
  printf("       Name %s\n", data.appName);
  printf("    Version %s\n", data.appVersion);
  printf("    BuildID %s\n", data.appBuildID);
  printf("  Copyright %s\n", data.copyright);
  printf("      Flags %08x\n", data.flags);
  printf("---------------------------------------------------------\n");
#endif

  return &data;
}

int main(int argc, char* argv[])
{
  if (argc == 1 || IsArg(argv[1], "h")
                || IsArg(argv[1], "help")
                || IsArg(argv[1], "?"))
  {
    // display additional information (XXX make localizable?)
    Output(PR_FALSE,
           "Mozilla XULRunner " MOZILLA_VERSION " %d\n\n"
           "Usage: " XULRUNNER_PROGNAME 
           " [OPTIONS] [APP-FILE [APP-OPTIONS...]]\n"
           "\n"
           "OPTIONS\n"
           "      --app        specify APP-FILE (optional)\n"
           "  -h, --help       show this message\n"
           "  -v, --version    show version\n"
           "\n"
           "APP-FILE\n"
           "  Application initialization file.\n"
           "\n"
           "APP-OPTIONS\n"
           "  Application specific options.\n",
           BUILD_ID);
    return 0;
  }

  if (argc == 2 && (IsArg(argv[1], "v") || IsArg(argv[1], "version")))
  {
    Output(PR_FALSE, "Mozilla XULRunner " MOZILLA_VERSION " %d\n",
           BUILD_ID);
    return 0;
  }

  geckoVersion = ParseVersion(GRE_BUILD_ID);

  PRBool hasAppOption;
  const char *appDataFile;
  if (IsArg(argv[1], "app"))
  {
    if (argc == 2)
    {
      Output(PR_TRUE, "Error: APP-FILE must be specified!\n");
      return 1;
    }
    hasAppOption = PR_TRUE;
    appDataFile = argv[2];
  }
  else
  {
    hasAppOption = PR_FALSE;
    appDataFile = argv[1];
  }

  const nsXREAppData *appData = LoadAppData(appDataFile);
  if (!appData)
  {
    Output(PR_TRUE, "Error: Invalid or missing application data!\n");
    return 1;
  }

  char **argv2 = NULL;
  if (!hasAppOption)
  {
    // fixup argv to start with -app.
    argv2 = (char **) malloc(sizeof(char*) * (argc + 2));
    argv2[0] = argv[0];
    argv2[1] = "-app";
    for (int i=1; i<argc; ++i)
      argv2[i+1] = argv[i];
    argv2[argc+1] = NULL;
    argv = argv2;
    argc++;
  }

  int rv = XRE_main(argc, argv, appData);

  if (argv2)
    free(argv2);

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
