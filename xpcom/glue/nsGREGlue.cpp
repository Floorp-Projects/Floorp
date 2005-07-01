/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Sean Su <ssu@netscape.com>
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

#include "nsXPCOMPrivate.h"
#include "nsXPCOMGlue.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef XP_WIN32
# include <windows.h>
# include <mbstring.h>
# include <io.h>
# define snprintf _snprintf
# define R_OK 04
#elif defined(XP_OS2)
# define INCL_DOS
# include <os2.h>
#elif defined(XP_MACOSX)
# include <CFBundle.h>
# include <unistd.h>
#elif defined(XP_UNIX)
# include <unistd.h>
# include <sys/param.h>
# include <dirent.h>
#elif defined(XP_BEOS)
# include <FindDirectory.h>
# include <Path.h>
# include <unistd.h>
# include <sys/param.h>
# include <OS.h>
# include <image.h>
#endif

#include <sys/stat.h>


#if defined(XP_MACOSX)

static PRBool
GRE_FindGREFramework(const char* version, const char* rootPath,
                     char* buffer, PRUint32 buflen);

#elif defined(XP_UNIX)

static PRBool
GRE_GetPathFromConfigDir(const char* version, const char* dirname,
                         char* buffer, PRUint32 buflen);
static PRBool
GRE_GetPathFromConfigFile(const char* version, const char* dirname,
                          char* buffer, PRUint32 buflen);

#elif defined(XP_WIN)

static PRBool
GRE_GetPathFromRegKey(const char* version, HKEY aRegKey,
                      char* buffer, PRUint32 buflen);

#endif

nsresult
GRE_GetGREPathForVersion(const char *aVersion,
                         char *aBuffer, PRUint32 aBufLen)
{
  // if GRE_HOME is in the environment, use that GRE
  const char* env = getenv("GRE_HOME");
  if (env && *env) {
#if XP_UNIX
    if (realpath(env, aBuffer))
      return NS_OK;
#elif XP_WIN
    if (_fullpath(aBuffer, env, aBufLen))
      return NS_OK;
#else
    // hope for the best
    // xxxbsmedberg: other platforms should have a "make absolute" function
#endif

    if (strlen(env) >= aBufLen)
      return NS_ERROR_FILE_NAME_TOO_LONG;

    strcpy(aBuffer, env);

    return NS_OK;
  }

  // the Gecko bits that sit next to the application or in the LD_LIBRARY_PATH
  env = getenv("USE_LOCAL_GRE");
  if (env && *env) {
    *aBuffer = nsnull;
    return NS_OK;
  }

#ifdef XP_MACOSX
  aBuffer[0] = '\0';

  // Check the bundle first, for <bundle>/Contents/Frameworks/XUL/libxpcom.dylib
  CFBundleRef appBundle = CFBundleGetMainBundle();
  if (appBundle) {
    CFURLRef fwurl = CFBundleCopyPrivateFrameworksURL(appBundle);
    if (fwurl) {
      CFURLRef xulurl =
        CFURLCreateCopyAppendingPathComponent(NULL, fwurl,
                                              CFSTR("XUL"), PR_TRUE);

      if (xulurl) {
        CFURLRef xpcomurl =
          CFURLCreateCopyAppendingPathComponent(NULL, xulurl,
                                                CFSTR("libxpcom.dylib"),
                                                PR_FALSE);

        if (xpcomurl) {
          if (!CFURLGetFileSystemRepresentation(xpcomurl, PR_TRUE,
                                               (UInt8*) aBuffer, aBufLen) ||
              access(aBuffer, R_OK | X_OK) != 0)
            aBuffer[0] = '\0';
        }

        CFRelease(xulurl);
      }

      CFRelease(fwurl);
    }
  }

  if (aBuffer[0])
    return NS_OK;

  // Check ~/Library/Frameworks/XUL/Versions/<version>/libxpcom.dylib
  const char *home = getenv("HOME");
  if (home && *home && GRE_FindGREFramework(aVersion, home, aBuffer, aBufLen)) {
    return NS_OK;
  }

  // Check /Library/Frameworks/XUL/Versions/<version>/libxpcom.dylib
  if (GRE_FindGREFramework(aVersion, "", aBuffer, aBufLen)) {
    return NS_OK;
  }

#elif defined(XP_UNIX) 
  env = getenv("MOZ_GRE_CONF");
  if (env && GRE_GetPathFromConfigFile(aVersion, env, aBuffer, aBufLen)) {
    return NS_OK;
  }

  env = getenv("HOME");
  if (env && *env) {
    char buffer[MAXPATHLEN];

    // Look in ~/.gre.config

    snprintf(buffer, sizeof(buffer),
             "%s" XPCOM_FILE_PATH_SEPARATOR GRE_CONF_NAME, env);
    
    if (GRE_GetPathFromConfigFile(aVersion, buffer, aBuffer, aBufLen)) {
      return NS_OK;
    }

    // Look in ~/.gre.d/*.conf

    snprintf(buffer, sizeof(buffer),
             "%s" XPCOM_FILE_PATH_SEPARATOR GRE_USER_CONF_DIR, env);

    if (GRE_GetPathFromConfigDir(aVersion, buffer, aBuffer, aBufLen)) {
      return NS_OK;
    }
  }

  // Look for a global /etc/gre.conf file
  if (GRE_GetPathFromConfigFile(aVersion, GRE_CONF_PATH, aBuffer, aBufLen)) {
    return NS_OK;
  }

  // Look for a group of config files in /etc/gre.d/
  if (GRE_GetPathFromConfigDir(aVersion, GRE_CONF_DIR, aBuffer, aBufLen)) {
    return NS_OK;
  }

#elif defined(XP_WIN)
  HKEY hRegKey = NULL;
    
  // A couple of key points here:
  // 1. Note the usage of the "Software\\Mozilla\\GRE" subkey - this allows
  //    us to have multiple versions of GREs on the same machine by having
  //    subkeys such as 1.0, 1.1, 2.0 etc. under it.
  // 2. In this sample below we're looking for the location of GRE version 1.2
  //    i.e. we're compatible with GRE 1.2 and we're trying to find it's install
  //    location.
  //
  // Please see http://www.mozilla.org/projects/embedding/GRE.html for
  // more info.
  //
  if (::RegOpenKeyEx(HKEY_CURRENT_USER, GRE_WIN_REG_LOC, 0,
                     KEY_READ, &hRegKey) == ERROR_SUCCESS) {
    PRBool ok = GRE_GetPathFromRegKey(aVersion, hRegKey, aBuffer, aBufLen);
    ::RegCloseKey(hRegKey);

    if (ok)
      return NS_OK;
  }

  if (::RegOpenKeyEx(HKEY_LOCAL_MACHINE, GRE_WIN_REG_LOC, 0,
                     KEY_ENUMERATE_SUB_KEYS, &hRegKey) == ERROR_SUCCESS) {
    PRBool ok = GRE_GetPathFromRegKey(aVersion, hRegKey, aBuffer, aBufLen);
    ::RegCloseKey(hRegKey);

    if (ok)
      return NS_OK;
  }
#endif

  return NS_ERROR_FAILURE;
}

#ifdef XP_MACOSX
PRBool
GRE_FindGREFramework(const char* version, const char* rootPath,
                     char* buffer, PRUint32 buflen)
{
  snprintf(buffer, buflen,
           "%s/Library/Frameworks/" GRE_FRAMEWORK_NAME "/Versions/%s/" XPCOM_DLL,
           rootPath, version);

  if (access(buffer, R_OK | X_OK) == 0)
    return PR_TRUE;

  buffer[0] = '\0';
  return PR_FALSE;
}
    
#elif defined(XP_UNIX)

static PRBool IsConfFile(const char *filename)
{
  const char *dot = strrchr(filename, '.');

  return (dot && strcmp(dot, ".conf") == 0);
}

PRBool
GRE_GetPathFromConfigDir(const char* version, const char* dirname,
                         char* buffer, PRUint32 buflen)
{
  // Open the directory provided and try to read any files in that
  // directory that end with .conf.  We look for an entry that might
  // point to the GRE that we're interested in.
  DIR *dir = opendir(dirname);
  if (!dir)
    return nsnull;

  PRBool found = PR_FALSE;
  struct dirent *entry;

  while (!found && (entry = readdir(dir))) {

    // Only look for files that end in .conf
    // IsConfFile will skip "." and ".."
    if (!IsConfFile(entry->d_name))
      continue;

    char fullPath[MAXPATHLEN];
    snprintf(fullPath, sizeof(fullPath), "%s" XPCOM_FILE_PATH_SEPARATOR "%s",
             dirname, entry->d_name);

    found = GRE_GetPathFromConfigFile(version, fullPath, buffer, buflen);
  }

  closedir(dir);

  return found;
}

#define READ_BUFFER_SIZE 1024

PRBool
GRE_GetPathFromConfigFile(const char* version, const char* filename,
                          char* pathBuffer, PRUint32 buflen)
{
  int versionlen = strlen(version);

  *pathBuffer = '\0';
  char buffer[READ_BUFFER_SIZE];
  FILE *cfg;
  PRBool foundHeader = PR_FALSE;

  if ((cfg = fopen(filename,"r")) == nsnull) {
    return nsnull;
  }

  while (fgets(buffer, READ_BUFFER_SIZE, cfg) != nsnull) {
    // skip over comment lines and blank lines
    if (buffer[0] == '#' || buffer[0] == '\n') {
      continue;
    }

    // we found a section heading, check to see if it is the one we
    // are interested in.
    if (buffer[0] == '[') {
      foundHeader == (buffer[versionlen + 1] == ']' &&
                      strncmp(buffer + 1, version, versionlen) == 0);
      continue;
    }

    static const char kGreHome[] = "GRE_PATH=";

    if (foundHeader && !strncmp(buffer, kGreHome, sizeof(kGreHome) - 1)) {
      strncpy(pathBuffer, buffer + 9, buflen);
      // kill the line feed if any
      PRInt32 len = strlen(pathBuffer);
      len--;

      if (pathBuffer[len] == '\n')
        pathBuffer[len] = '\0';
      break;
    }
  }
  fclose(cfg);
  return (*pathBuffer != '\0');
}

#elif defined(XP_WIN)

static PRBool
CopyWithEnvExpansion(char* aDest, const char* aSource, PRUint32 aBufLen,
                     DWORD aType)
{
  switch (aType) {
  case REG_SZ:
    if (strlen(aSource) >= aBufLen)
      return PR_FALSE;

    strcpy(aDest, aSource);
    return PR_TRUE;

  case REG_EXPAND_SZ:
    if (ExpandEnvironmentStrings(aSource, aDest, aBufLen) > aBufLen)
      return PR_FALSE;

    return PR_TRUE;
  };

  // Whoops! We expected REG_SZ or REG_EXPAND_SZ, what happened here?

  return PR_FALSE;
}

PRBool
GRE_GetPathFromRegKey(const char* aVersion, HKEY aRegKey,
              char* aBuffer, PRUint32 aBufLen)
{
  // Formerly, GREs were registered at the key HKLM/Software/Mozilla/GRE/<version>
  // valuepair GreHome=Path. Nowadays, they are registered in any subkey of
  // Software/Mozilla/GRE, with the following valuepairs:
  //   Version=<version> (REG_SZ)
  //   GreHome=<path>    (REG_SZ or REG_EXPAND_SZ)
  //
  // Additional meta-info may be available in the future, including
  // localization info, ABI, and other information which might be pertinent
  // to selecting one GRE over another.
  //
  // When a GRE is being registered, it should try to register itself at
  // HKLM/Software/Mozilla/GRE/<Version> first, to preserve compatibility
  // with older glue. If this key is already taken (i.e. there is more than
  // one GRE of that version installed), it should append a unique number to
  // the version, for example:
  //   1.1 (already in use), 1.1_1, 1.1_2, etc...

  DWORD i = 0;

  while (PR_TRUE) {
    char name[MAXPATHLEN + 1];
    DWORD nameLen = MAXPATHLEN;
    if (::RegEnumKeyEx(aRegKey, i, name, &nameLen, NULL, NULL, NULL, NULL) !=
        ERROR_SUCCESS) {
      break;
    }

    HKEY subKey = NULL;
    if (::RegOpenKeyEx(aRegKey, name, 0, KEY_QUERY_VALUE, &subKey) !=
        ERROR_SUCCESS) {
      continue;
    }

    char version[40];
    DWORD versionlen = 40;
    char pathbuf[MAXPATHLEN];
    DWORD pathlen = sizeof(pathbuf);
    DWORD pathtype;

    if (::RegQueryValueEx(subKey, "Version", NULL, NULL,
                          (BYTE*) version, &versionlen) == ERROR_SUCCESS &&
        strncmp(aVersion, version, versionlen) == 0) {
      if (::RegQueryValueEx(subKey, "GreHome", NULL, &pathtype,
                            (BYTE*) pathbuf, &pathlen) == ERROR_SUCCESS &&
          *pathbuf &&
          CopyWithEnvExpansion(aBuffer, pathbuf, aBufLen, pathtype) &&
          access(aBuffer, R_OK) == 0) {
        RegCloseKey(subKey);
        return PR_TRUE;
      }
    }

    RegCloseKey(subKey);

    ++i;
  }

  return PR_FALSE;
}
#endif // XP_WIN
