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

#ifdef XP_WIN
#include <windows.h>
#define snprintf _snprintf
#define PATH_SEPARATOR_CHAR '\\'
#define XULRUNNER_BIN "xulrunner.exe"
#include "nsWindowsRestart.cpp"
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#define PATH_SEPARATOR_CHAR '/'
#define XULRUNNER_BIN "xulrunner-bin"
#endif

#define VERSION_MAXLEN 128

int
main(int argc, char **argv)
{
  nsresult rv;
  char *lastSlash;

  char iniPath[MAXPATHLEN];

#ifdef XP_WIN
  if (!::GetModuleFileName(NULL, iniPath, sizeof(iniPath)))
    return 1;

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
      sprintf(iniPath, "%s/%s", token, argv[0]);
      if (stat(iniPath, &fileStat) == 0) {
        found = PR_TRUE;
        break;
      }
    }
    free (pathdup);
    if (!found)
      return 1;
  }

#endif

  lastSlash = strrchr(iniPath, PATH_SEPARATOR_CHAR);
  if (!lastSlash)
    return 1;

  strcpy(lastSlash + 1, "application.ini");

  nsINIParser parser;
  rv = parser.Init(iniPath);
  if (NS_FAILED(rv)) {
    fprintf(stderr, "Could not read application.ini\n");
    return 1;
  }

  char greDir[MAXPATHLEN];
  char minVersion[VERSION_MAXLEN];

  // If a gecko maxVersion is not specified, we assume that the app uses only
  // frozen APIs, and is therefore compatible with any xulrunner 1.x.
  char maxVersion[VERSION_MAXLEN] = "2";

  GREVersionRange range = {
    minVersion,
    PR_TRUE,
    maxVersion,
    PR_FALSE
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

  rv = GRE_GetGREPathWithProperties(&range, 1, nsnull, 0,
                                    greDir, sizeof(greDir));
  if (NS_FAILED(rv)) {
    // XXXbsmedberg: Do something much smarter here: notify the
    // user/offer to download/?

    fprintf(stderr,
            "Could not find compatible GRE between version %s and %s.\n", 
            range.lower, range.upper);
    return 1;
  }

  lastSlash = strrchr(greDir, PATH_SEPARATOR_CHAR);
  if (lastSlash) {
    *lastSlash = '\0';
  }

  char **argv2 = (char**) malloc(sizeof(char*) * (argc + 2));
  if (!argv2)
    return 1;

  char xulBin[MAXPATHLEN];
  snprintf(xulBin, sizeof(xulBin),
           "%s" XPCOM_FILE_PATH_SEPARATOR XULRUNNER_BIN, greDir);

#ifndef XP_WIN
  setenv(XPCOM_SEARCH_KEY, greDir, 1);
#endif

  argv2[0] = xulBin;
  argv2[1] = iniPath;

  for (int i = 1; i < argc; ++i)
    argv2[i + 1] = argv[i];

  argv2[argc + 1] = NULL;

#ifdef XP_WIN
  BOOL ok = WinLaunchChild(xulBin, argc + 1, argv2);

  free (argv2);

  return !ok;
#else
  execv(xulBin, argv2);

  return 1;
#endif
}
