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

#include "nsXPCOMGlue.h"

#include "nsINIParser.h"
#include "nsVersionComparator.h"
#include "nsXPCOMPrivate.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

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
# include <dirent.h>
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
# include <dirent.h>
# include <FindDirectory.h>
#endif

#include <sys/stat.h>

/**
 * Like strncat, appends a buffer to another buffer. This is where the
 * similarity ends. Firstly, the "count" here is the total size of the buffer
 * (not the number of chars to append. Secondly, the function returns PR_FALSE
 * if the buffer is not long enough to hold the concatenated string.
 */
static PRBool safe_strncat(char *dest, const char *append, PRUint32 count)
{
  char *end = dest + count - 1;

  // skip to the end of dest
  while (*dest)
    ++dest;

  while (*append && dest < end) {
    *dest = *append;
    ++dest, ++append;
  }

  *dest = '\0';

  return *append == '\0';
}

#ifdef XP_WIN
static PRBool
CheckVersion(const PRUnichar* toCheck,
             const GREVersionRange *versions,
             PRUint32 versionsLength);
#endif
static PRBool
CheckVersion(const char* toCheck,
             const GREVersionRange *versions,
             PRUint32 versionsLength);


#if defined(XP_MACOSX)

static PRBool
GRE_FindGREFramework(const char* rootPath,
                     const GREVersionRange *versions,
                     PRUint32 versionsLength,
                     const GREProperty *properties,
                     PRUint32 propertiesLength,
                     char* buffer, PRUint32 buflen);

#elif defined(XP_UNIX) || defined(XP_BEOS)

static PRBool
GRE_GetPathFromConfigDir(const char* dirname,
                         const GREVersionRange *versions,
                         PRUint32 versionsLength,
                         const GREProperty *properties,
                         PRUint32 propertiesLength,
                         char* buffer, PRUint32 buflen);

static PRBool
GRE_GetPathFromConfigFile(const char* filename,
                          const GREVersionRange *versions,
                          PRUint32 versionsLength,
                          const GREProperty *properties,
                          PRUint32 propertiesLength,
                          char* buffer, PRUint32 buflen);

#elif defined(XP_WIN)

static PRBool
GRE_GetPathFromRegKey(HKEY aRegKey,
                      const GREVersionRange *versions,
                      PRUint32 versionsLength,
                      const GREProperty *properties,
                      PRUint32 propertiesLength,
                      PRUnichar* buffer, PRUint32 buflen);

#endif

nsresult
GRE_GetGREPathWithProperties(const GREVersionRange *versions,
                             PRUint32 versionsLength,
                             const GREProperty *properties,
                             PRUint32 propertiesLength,
                             char *aBuffer, PRUint32 aBufLen)
{
#ifdef TARGET_XPCOM_ABI
  // append the ABI to the properties to match only binary 
  // compatible GREs
  static const GREProperty kExtraProperty =
    { "abi", TARGET_XPCOM_ABI };

  GREProperty *allProperties = new GREProperty[propertiesLength + 1];
  if (!allProperties)
    return NS_ERROR_OUT_OF_MEMORY;

  for (PRUint32 i=0; i<propertiesLength; i++) {
    allProperties[i].property = properties[i].property;
    allProperties[i].value    = properties[i].value;
  }
  allProperties[propertiesLength].property = kExtraProperty.property;
  allProperties[propertiesLength].value    = kExtraProperty.value;
  PRUint32 allPropertiesLength = propertiesLength + 1;
#else
  const GREProperty *allProperties = properties;
  PRUint32 allPropertiesLength = propertiesLength;
#endif

  // if GRE_HOME is in the environment, use that GRE
  const char* env = getenv("GRE_HOME");
  if (env && *env) {
    char p[MAXPATHLEN];
    snprintf(p, sizeof(p), "%s" XPCOM_FILE_PATH_SEPARATOR XPCOM_DLL, env);
    p[sizeof(p) - 1] = '\0';

#if XP_UNIX
    if (realpath(p, aBuffer))
      return NS_OK;
#elif WINCE
    if (p[0] != '\\') 
    {
      unsigned short dir[MAX_PATH];
      unsigned short path[MAX_PATH];
      MultiByteToWideChar(CP_ACP, 0, p, -1, path, MAX_PATH);
      _wfullpath(dir,path,MAX_PATH);
      WideCharToMultiByte(CP_ACP, 0, dir, -1, aBuffer, MAX_PATH, NULL, NULL);
    }
    else {
      strcpy(aBuffer, p);
    }
    return NS_OK;
#elif XP_WIN
    if (_fullpath(aBuffer, p, aBufLen))
      return NS_OK;
#elif XP_OS2
    // realpath on OS/2 returns a unix-ized path, so re-native-ize
    if (realpath(p, aBuffer)) {
      for (char* ptr = strchr(aBuffer, '/'); ptr; ptr = strchr(ptr, '/'))
        *ptr = '\\';
      return NS_OK;
    }
#elif XP_BEOS
    BPath path;
    status_t result;
    result = path.SetTo(p,0,true);
    if (result == B_OK)
    {
      sprintf(aBuffer, path.Path());
      return NS_OK;
    }
#else
    // hope for the best
    // xxxbsmedberg: other platforms should have a "make absolute" function
#endif

    if (strlen(p) >= aBufLen)
      return NS_ERROR_FILE_NAME_TOO_LONG;

    strcpy(aBuffer, p);

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

  // Check the bundle first, for <bundle>/Contents/Frameworks/XUL.framework/libxpcom.dylib
  CFBundleRef appBundle = CFBundleGetMainBundle();
  if (appBundle) {
    CFURLRef fwurl = CFBundleCopyPrivateFrameworksURL(appBundle);
    CFURLRef absfwurl = nsnull;
    if (fwurl) {
      absfwurl = CFURLCopyAbsoluteURL(fwurl);
      CFRelease(fwurl);
    }

    if (absfwurl) {
      CFURLRef xulurl =
        CFURLCreateCopyAppendingPathComponent(NULL, absfwurl,
                                              CFSTR(GRE_FRAMEWORK_NAME),
                                              PR_TRUE);

      if (xulurl) {
        CFURLRef xpcomurl =
          CFURLCreateCopyAppendingPathComponent(NULL, xulurl,
                                                CFSTR("libxpcom.dylib"),
                                                PR_FALSE);

        if (xpcomurl) {
          char tbuffer[MAXPATHLEN];

          if (CFURLGetFileSystemRepresentation(xpcomurl, PR_TRUE,
                                               (UInt8*) tbuffer,
                                               sizeof(tbuffer)) &&
              access(tbuffer, R_OK | X_OK) == 0) {
            if (!realpath(tbuffer, aBuffer)) {
              aBuffer[0] = '\0';
            }
          }

          CFRelease(xpcomurl);
        }

        CFRelease(xulurl);
      }

      CFRelease(absfwurl);
    }
  }

  if (aBuffer[0])
    return NS_OK;

  // Check ~/Library/Frameworks/XUL.framework/Versions/<version>/libxpcom.dylib
  const char *home = getenv("HOME");
  if (home && *home && GRE_FindGREFramework(home,
                                            versions, versionsLength,
                                            allProperties, allPropertiesLength,
                                            aBuffer, aBufLen)) {
    return NS_OK;
  }

  // Check /Library/Frameworks/XUL.framework/Versions/<version>/libxpcom.dylib
  if (GRE_FindGREFramework("",
                           versions, versionsLength,
                           allProperties, allPropertiesLength,
                           aBuffer, aBufLen)) {
    return NS_OK;
  }

#elif defined(XP_UNIX) 
  env = getenv("MOZ_GRE_CONF");
  if (env && GRE_GetPathFromConfigFile(env,
                                       versions, versionsLength,
                                       allProperties, allPropertiesLength,
                                       aBuffer, aBufLen)) {
    return NS_OK;
  }

  env = getenv("HOME");
  if (env && *env) {
    char buffer[MAXPATHLEN];

    // Look in ~/.gre.config

    snprintf(buffer, sizeof(buffer),
             "%s" XPCOM_FILE_PATH_SEPARATOR GRE_CONF_NAME, env);
    
    if (GRE_GetPathFromConfigFile(buffer,
                                  versions, versionsLength,
                                  allProperties, allPropertiesLength,
                                  aBuffer, aBufLen)) {
      return NS_OK;
    }

    // Look in ~/.gre.d/*.conf

    snprintf(buffer, sizeof(buffer),
             "%s" XPCOM_FILE_PATH_SEPARATOR GRE_USER_CONF_DIR, env);

    if (GRE_GetPathFromConfigDir(buffer,
                                 versions, versionsLength,
                                 allProperties, allPropertiesLength,
                                 aBuffer, aBufLen)) {
      return NS_OK;
    }
  }

  // Look for a global /etc/gre.conf file
  if (GRE_GetPathFromConfigFile(GRE_CONF_PATH,
                                versions, versionsLength,
                                allProperties, allPropertiesLength,
                                aBuffer, aBufLen)) {
    return NS_OK;
  }

  // Look for a group of config files in /etc/gre.d/
  if (GRE_GetPathFromConfigDir(GRE_CONF_DIR,
                               versions, versionsLength,
                               allProperties, allPropertiesLength,
                               aBuffer, aBufLen)) {
    return NS_OK;
  }

#elif defined(XP_BEOS)
  env = getenv("MOZ_GRE_CONF");
  if (env && GRE_GetPathFromConfigFile(env,
                                       versions, versionsLength,
                                       allProperties, allPropertiesLength,
                                       aBuffer, aBufLen)) {
    return NS_OK;
  }

  char p[MAXPATHLEN]; 
  if (find_directory(B_USER_SETTINGS_DIRECTORY, 0, 0, p, MAXPATHLEN)) {
    char buffer[MAXPATHLEN];

    // Look in B_USER_SETTINGS_DIRECTORY/gre.config
    snprintf(buffer, sizeof(buffer),
             "%s" XPCOM_FILE_PATH_SEPARATOR GRE_CONF_NAME, p);
    
    if (GRE_GetPathFromConfigFile(buffer,
                                  versions, versionsLength,
                                  allProperties, allPropertiesLength,
                                  aBuffer, aBufLen)) {
      return NS_OK;
    }

    // Look in B_USER_SETTINGS_DIRECTORY/gre.d/*.conf
    snprintf(buffer, sizeof(buffer),
             "%s" XPCOM_FILE_PATH_SEPARATOR GRE_CONF_DIR, p);

    if (GRE_GetPathFromConfigDir(buffer,
                                 versions, versionsLength,
                                 allProperties, allPropertiesLength,
                                 aBuffer, aBufLen)) {
      return NS_OK;
    }
  }
  
  // Hope Zeta OS and Haiku OS multiuser versions will respect BeBook,
  // for BeOS R5 COMMON and USER are equal
  if (find_directory(B_COMMON_SETTINGS_DIRECTORY, 0, 0, p, MAXPATHLEN)) {
    char buffer[MAXPATHLEN];
    
    // Look for a B_COMMON_SETTINGS_DIRECTORY/gre.conf file
    snprintf(buffer, sizeof(buffer),
             "%s" XPCOM_FILE_PATH_SEPARATOR GRE_CONF_PATH, p);
    if (GRE_GetPathFromConfigFile(buffer,
                                  versions, versionsLength,
                                  allProperties, allPropertiesLength,
                                  aBuffer, aBufLen)) {
      return NS_OK;
    }

    // Look for a group of config files in B_COMMON_SETTINGS_DIRECTORY/gre.d/
    snprintf(buffer, sizeof(buffer),
             "%s" XPCOM_FILE_PATH_SEPARATOR GRE_CONF_DIR, p);
    if (GRE_GetPathFromConfigDir(buffer,
                                 versions, versionsLength,
                                 allProperties, allPropertiesLength,
                                 aBuffer, aBufLen)) {
      return NS_OK;
    }
  }

#elif defined(XP_WIN)
  HKEY hRegKey = NULL;
    
  // A couple of key points here:
  // 1. Note the usage of the "Software\\mozilla.org\\GRE" subkey - this allows
  //    us to have multiple versions of GREs on the same machine by having
  //    subkeys such as 1.0, 1.1, 2.0 etc. under it.
  // 2. In this sample below we're looking for the location of GRE version 1.2
  //    i.e. we're compatible with GRE 1.2 and we're trying to find it's install
  //    location.
  //
  // Please see http://www.mozilla.org/projects/embedding/GRE.html for
  // more info.
  //
  if (::RegOpenKeyExW(HKEY_CURRENT_USER, GRE_WIN_REG_LOC, 0,
                      KEY_READ, &hRegKey) == ERROR_SUCCESS) {
      PRBool ok = GRE_GetPathFromRegKey(hRegKey,
                                        versions, versionsLength,
                                        allProperties, allPropertiesLength,
                                        (WCHAR*) NS_ConvertUTF8toUTF16(aBuffer).get(), aBufLen);
      ::RegCloseKey(hRegKey);

      if (ok)
          return NS_OK;
  }

  if (::RegOpenKeyExW(HKEY_LOCAL_MACHINE, GRE_WIN_REG_LOC, 0,
                      KEY_ENUMERATE_SUB_KEYS, &hRegKey) == ERROR_SUCCESS) {
      PRBool ok = GRE_GetPathFromRegKey(hRegKey,
                                        versions, versionsLength,
                                        allProperties, allPropertiesLength,
                                        (WCHAR*) NS_ConvertUTF8toUTF16(aBuffer).get(), aBufLen);
      ::RegCloseKey(hRegKey);

      if (ok)
          return NS_OK;
  }
#endif

  return NS_ERROR_FAILURE;
}

static PRBool
CheckVersion(const char* toCheck,
             const GREVersionRange *versions,
             PRUint32 versionsLength)
{
  
  for (const GREVersionRange *versionsEnd = versions + versionsLength;
       versions < versionsEnd;
       ++versions) {
    PRInt32 c = NS_CompareVersions(toCheck, versions->lower);
    if (c < 0)
      continue;

    if (!c && !versions->lowerInclusive)
      continue;

    c = NS_CompareVersions(toCheck, versions->upper);
    if (c > 0)
      continue;

    if (!c && !versions->upperInclusive)
      continue;

    return PR_TRUE;
  }

  return PR_FALSE;
}

#ifdef XP_WIN
static PRBool
CheckVersion(const PRUnichar* toCheck,
             const GREVersionRange *versions,
             PRUint32 versionsLength)
{
  
  for (const GREVersionRange *versionsEnd = versions + versionsLength;
       versions < versionsEnd;
       ++versions) {
      PRInt32 c = NS_CompareVersions(toCheck, NS_ConvertUTF8toUTF16(versions->lower).get());
      if (c < 0)
        continue;

      if (!c && !versions->lowerInclusive)
        continue;

      c = NS_CompareVersions(toCheck, NS_ConvertUTF8toUTF16(versions->upper).get());
      if (c > 0)
        continue;

      if (!c && !versions->upperInclusive)
        continue;

      return PR_TRUE;
  }

  return PR_FALSE;
}
#endif


#ifdef XP_MACOSX
PRBool
GRE_FindGREFramework(const char* rootPath,
                     const GREVersionRange *versions,
                     PRUint32 versionsLength,
                     const GREProperty *properties,
                     PRUint32 propertiesLength,
                     char* buffer, PRUint32 buflen)
{
  PRBool found = PR_FALSE;

  snprintf(buffer, buflen,
           "%s/Library/Frameworks/" GRE_FRAMEWORK_NAME "/Versions", rootPath);
  DIR *dir = opendir(buffer);
  if (dir) {
    struct dirent *entry;
    while (!found && (entry = readdir(dir))) {
      if (CheckVersion(entry->d_name, versions, versionsLength)) {
        snprintf(buffer, buflen,
                 "%s/Library/Frameworks/" GRE_FRAMEWORK_NAME
                 "/Versions/%s/" XPCOM_DLL, rootPath, entry->d_name);
        if (access(buffer, R_OK | X_OK) == 0)
          found = PR_TRUE;
      }
    }

    closedir(dir);
  }
  
  if (found)
    return PR_TRUE;

  buffer[0] = '\0';
  return PR_FALSE;
}
    
#elif defined(XP_UNIX) || defined(XP_BEOS)

static PRBool IsConfFile(const char *filename)
{
  const char *dot = strrchr(filename, '.');

  return (dot && strcmp(dot, ".conf") == 0);
}

PRBool
GRE_GetPathFromConfigDir(const char* dirname,
                         const GREVersionRange *versions,
                         PRUint32 versionsLength,
                         const GREProperty *properties,
                         PRUint32 propertiesLength,
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

    found = GRE_GetPathFromConfigFile(fullPath,
                                      versions, versionsLength,
                                      properties, propertiesLength,
                                      buffer, buflen);
  }

  closedir(dir);

  return found;
}

#define READ_BUFFER_SIZE 1024

struct INIClosure
{
  nsINIParser           *parser;
  const GREVersionRange *versions;
  PRUint32               versionsLength;
  const GREProperty     *properties;
  PRUint32               propertiesLength;
  char                  *pathBuffer;
  PRUint32               buflen;
  PRBool                 found;
};

static PRBool
CheckINIHeader(const char *aHeader, void *aClosure)
{
  nsresult rv;

  INIClosure *c = reinterpret_cast<INIClosure *>(aClosure);

  if (!CheckVersion(aHeader, c->versions, c->versionsLength))
    return PR_TRUE;

  const GREProperty *properties = c->properties;
  const GREProperty *endProperties = properties + c->propertiesLength;
  for (; properties < endProperties; ++properties) {
    char buffer[MAXPATHLEN];
    rv = c->parser->GetString(aHeader, properties->property,
                             buffer, sizeof(buffer));
    if (NS_FAILED(rv))
      return PR_TRUE;

    if (strcmp(buffer, properties->value))
      return PR_TRUE;
  }

  rv = c->parser->GetString(aHeader, "GRE_PATH", c->pathBuffer, c->buflen);
  if (NS_FAILED(rv))
    return PR_TRUE;

  if (!safe_strncat(c->pathBuffer, "/" XPCOM_DLL, c->buflen) ||
      access(c->pathBuffer, R_OK))
    return PR_TRUE;

  // We found a good GRE! Stop looking.
  c->found = PR_TRUE;
  return PR_FALSE;
}

PRBool
GRE_GetPathFromConfigFile(const char* filename,
                          const GREVersionRange *versions,
                          PRUint32 versionsLength,
                          const GREProperty *properties,
                          PRUint32 propertiesLength,
                          char* pathBuffer, PRUint32 buflen)
{
  nsINIParser parser;
  nsresult rv = parser.Init(filename);
  if (NS_FAILED(rv))
    return PR_FALSE;

  INIClosure c = {
    &parser,
    versions, versionsLength,
    properties, propertiesLength,
    pathBuffer, buflen,
    PR_FALSE
  };

  parser.GetSections(CheckINIHeader, &c);
  return c.found;
}

#elif defined(XP_WIN)

static PRBool
CopyWithEnvExpansion(PRUnichar* aDest, const PRUnichar* aSource, PRUint32 aBufLen,
                     DWORD aType)
{
  switch (aType) {
  case REG_SZ:
    if (wcslen(aSource) >= aBufLen)
      return PR_FALSE;

    wcscpy(aDest, aSource);
    return PR_TRUE;

  case REG_EXPAND_SZ:
    if (ExpandEnvironmentStringsW(aSource, aDest, aBufLen) > aBufLen)
      return PR_FALSE;

    return PR_TRUE;
  };

  // Whoops! We expected REG_SZ or REG_EXPAND_SZ, what happened here?

  return PR_FALSE;
}

PRBool
GRE_GetPathFromRegKey(HKEY aRegKey,
                      const GREVersionRange *versions,
                      PRUint32 versionsLength,
                      const GREProperty *properties,
                      PRUint32 propertiesLength,
                      PRUnichar* aBuffer, PRUint32 aBufLen)
{
  // Formerly, GREs were registered at the registry key
  // HKLM/Software/mozilla.org/GRE/<version> valuepair GreHome=Path.
  // Nowadays, they are registered in any subkey of
  // Software/mozilla.org/GRE, with the following valuepairs:
  //   Version=<version> (REG_SZ)
  //   GreHome=<path>    (REG_SZ or REG_EXPAND_SZ)
  //   <Property>=<value> (REG_SZ)
  //
  // Additional meta-info may be available in the future, including
  // localization info and other information which might be pertinent
  // to selecting one GRE over another.
  //
  // When a GRE is being registered, it should try to register itself at
  // HKLM/Software/mozilla.org/GRE/<Version> first, to preserve compatibility
  // with older glue. If this key is already taken (i.e. there is more than
  // one GRE of that version installed), it should append a unique number to
  // the version, for example:
  //   1.1 (already in use), 1.1_1, 1.1_2, etc...

  DWORD i = 0;

  while (PR_TRUE) {
    PRUnichar name[MAXPATHLEN + 1];
    DWORD nameLen = MAXPATHLEN;
    if (::RegEnumKeyExW(aRegKey, i, name, &nameLen, NULL, NULL, NULL, NULL) !=
          ERROR_SUCCESS) {
        break;
    }

    HKEY subKey = NULL;
    if (::RegOpenKeyExW(aRegKey, name, 0, KEY_QUERY_VALUE, &subKey) !=
          ERROR_SUCCESS) {
        continue;
    }

    PRUnichar version[40];
    DWORD versionlen = 40;
    PRUnichar pathbuf[MAXPATHLEN];
    DWORD pathlen;
    DWORD pathtype;

    PRBool ok = PR_FALSE;

    if (::RegQueryValueExW(subKey, L"Version", NULL, NULL,
                           (BYTE*) version, &versionlen) == ERROR_SUCCESS &&
          CheckVersion(version, versions, versionsLength)) {

      ok = PR_TRUE;
      const GREProperty *props = properties;
      const GREProperty *propsEnd = properties + propertiesLength;
      for (; ok && props < propsEnd; ++props) {
        pathlen = sizeof(pathbuf);

        if (::RegQueryValueExW(subKey, NS_ConvertUTF8toUTF16(props->property).get(), NULL, &pathtype,
                               (BYTE*) pathbuf, &pathlen) != ERROR_SUCCESS ||
              wcscmp(pathbuf,  NS_ConvertUTF8toUTF16(props->value).get()))
            ok = PR_FALSE;
      }

      pathlen = sizeof(pathbuf);
      if (ok &&
          (!::RegQueryValueExW(subKey, L"GreHome", NULL, &pathtype,
                              (BYTE*) pathbuf, &pathlen) == ERROR_SUCCESS ||
           !*pathbuf ||
           !CopyWithEnvExpansion(aBuffer, pathbuf, aBufLen, pathtype))) {
        ok = PR_FALSE;
      }
      else if (!wcsncat(aBuffer, L"\\" LXPCOM_DLL, aBufLen) 
#ifdef WINCE
               || (GetFileAttributesW(aBuffer) != INVALID_FILE_ATTRIBUTES)
#else
               || _waccess(aBuffer, R_OK)
#endif
               ) {
        ok = PR_FALSE;
      }
    }

    RegCloseKey(subKey);

    if (ok)
      return PR_TRUE;

    ++i;
  }

  aBuffer[0] = '\0';

  return PR_FALSE;
}
#endif // XP_WIN
