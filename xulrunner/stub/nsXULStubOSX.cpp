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
 * the Initial Developer. All Rights Reserved.
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

#include <unistd.h>
#include <stdlib.h>

#include <CFBundle.h>
#include <Processes.h>

#include "nsBuildID.h"
#include "nsXPCOMGlue.h"
#include "nsINIParser.h"

#define VERSION_MAXLEN 128

int
main(int argc, char **argv)
{
  nsresult rv;

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
  CFRelease(resourcesURL);

  if (!iniFileURL)
    return 1;

  CFStringRef iniPathStr =
    CFURLCopyFileSystemPath(iniFileURL, kCFURLPOSIXPathStyle);
  CFRelease(iniFileURL);

  if (!iniPathStr)
    return 1;

  char iniPath[PATH_MAX];
  CFStringGetCString(iniPathStr, iniPath, sizeof(iniPath),
                     kCFStringEncodingUTF8);
  CFRelease(iniPathStr);

  nsINIParser parser;
  rv = parser.Init(iniPath);
  if (NS_FAILED(rv)) {
    fprintf(stderr, "Could not read application.ini\n");
    return 1;
  }

  char greDir[PATH_MAX];
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

  char **argv2 = (char**) alloca(sizeof(char*) * (argc + 2));

  char xulBin[PATH_MAX];
  snprintf(xulBin, sizeof(xulBin), "%s/xulrunner-bin", greDir);

  setenv("DYLD_LIBRARY_PATH", greDir, 1);
  setenv("XRE_BINARY_PATH", xulBin, 1);

  // IMPORTANT: we lie to argv[0] about what binary is being run, so that it
  // thinks it's in the current app bundle, not the XUL.framework.
  argv2[0] = argv[0];
  argv2[1] = iniPath;

  for (int i = 1; i < argc; ++i)
    argv2[i + 1] = argv[i];

  argv2[argc + 1] = NULL;

  execv(xulBin, argv2);
  return 1;
}
