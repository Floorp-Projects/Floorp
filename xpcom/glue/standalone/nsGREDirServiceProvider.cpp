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

#include "nsBuildID.h"

#include "nsEmbedString.h"
#include "nsXPCOMPrivate.h"
#include "nsXPCOMGlue.h"
#include "nsILocalFile.h"
#include "nsIDirectoryService.h"
#include "nsDirectoryServiceDefs.h"
#include "nsCOMPtr.h"

#include "nspr.h"
#include "plstr.h"

#ifdef XP_WIN32
#include <windows.h>
#include <stdlib.h>
#elif defined(XP_OS2)
#define INCL_DOS
#include <os2.h>
#include <stdlib.h>
#include <stdio.h>
#include "prenv.h"
#elif defined(XP_MACOSX)
#include <Processes.h>
#include <CFBundle.h>
#elif defined(XP_UNIX)
#include <unistd.h>
#include <stdlib.h>
#include <sys/param.h>
#include "prenv.h"
#elif defined(XP_BEOS)
#include <FindDirectory.h>
#include <Path.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/param.h>
#include <OS.h>
#include <image.h>
#include "prenv.h"
#endif

#include <sys/stat.h>

#include "nsGREDirServiceProvider.h"

PRBool GRE_GetCurrentProcessDirectory(char* buffer);
PRBool GRE_GetPathFromConfigDir(const char* dirname, char* buffer);
PRBool GRE_GetPathFromConfigFile(const char* dirname, char* buffer);

//*****************************************************************************
// nsGREDirServiceProvider::nsISupports
//*****************************************************************************   

NS_IMPL_ISUPPORTS1(nsGREDirServiceProvider, nsIDirectoryServiceProvider)
  
//*****************************************************************************
// nsGREDirServiceProvider::nsIDirectoryServiceProvider
//*****************************************************************************   

NS_IMETHODIMP
nsGREDirServiceProvider::GetFile(const char *prop, PRBool *persistant, nsIFile **_retval)
{
  *_retval = nsnull;
  *persistant = PR_TRUE;

  //---------------------------------------------------------------
  // Note that by returning a valid localFile's for NS_GRE_DIR,
  // your app is indicating to XPCOM that it found a GRE version
  // with which it's compatible with and intends to be "run against"
  // that GRE.
  //
  // Please see http://www.mozilla.org/projects/embedding/GRE.html
  // for more info on GRE.
  //---------------------------------------------------------------
  if(strcmp(prop, NS_GRE_DIR) == 0)
  {
    nsILocalFile* lfile = nsnull;
    nsresult rv = GRE_GetGREDirectory(&lfile);
    *_retval = lfile;
    return rv;
  }

  return NS_ERROR_FAILURE;
}

//*****************************************************************************
// Implementations from nsXPCOMGlue.h and helper functions.
//*****************************************************************************   

PRBool
GRE_GetCurrentProcessDirectory(char* buffer)
{
    *buffer = '\0';

#ifdef XP_WIN
    if ( ::GetModuleFileName(0, buffer, MAXPATHLEN) ) {
        // chop of the executable name by finding the rightmost backslash
        char* lastSlash = PL_strrchr(buffer, '\\');
        if (lastSlash) {
            *(lastSlash) = '\0';
            return PR_TRUE;
        }
    }

#elif defined(XP_MACOSX)
    // Works even if we're not bundled.
    CFBundleRef appBundle = CFBundleGetMainBundle();
    if (appBundle != nsnull)
    {
        CFURLRef bundleURL = CFBundleCopyExecutableURL(appBundle);
        if (bundleURL != nsnull)
        {
            CFURLRef parentURL = CFURLCreateCopyDeletingLastPathComponent(kCFAllocatorDefault, bundleURL);
            if (parentURL)
            {
                CFStringRef path = CFURLCopyFileSystemPath(parentURL, kCFURLPOSIXPathStyle);
                if (path)
                {
                    CFStringGetCString(path, buffer, MAXPATHLEN, kCFStringEncodingUTF8);
                    CFRelease(path);
                }
                CFRelease(parentURL);
            }
            CFRelease(bundleURL);
        }
        CFRelease(appBundle);
    }
    if (*buffer) return PR_TRUE;

#elif defined(XP_UNIX)

    // In the absence of a good way to get the executable directory let
    // us try this for unix:
    //	- if MOZILLA_FIVE_HOME is defined, that is it
    //	- else give the current directory

    // The MOZ_DEFAULT_MOZILLA_FIVE_HOME variable can be set at configure time with
    // a --with-default-mozilla-five-home=foo autoconf flag.
    // 
    // The idea here is to allow for builds that have a default MOZILLA_FIVE_HOME
    // regardless of the environment.  This makes it easier to write apps that
    // embed mozilla without having to worry about setting up the environment 
    //
    // We do this py putenv()ing the default value into the environment.  Note that
    // we only do this if it is not already set.
#ifdef MOZ_DEFAULT_MOZILLA_FIVE_HOME
    if (PR_GetEnv("MOZILLA_FIVE_HOME") == nsnull)
    {
        putenv("MOZILLA_FIVE_HOME=" MOZ_DEFAULT_MOZILLA_FIVE_HOME);
    }
#endif

    char *moz5 = PR_GetEnv("MOZILLA_FIVE_HOME");

    if (moz5 && *moz5)
    {
        if (!realpath(moz5, buffer))
            strcpy(buffer, moz5);

        return PR_TRUE;
    }
    else
    {
#if defined(DEBUG)
        static PRBool firstWarning = PR_TRUE;

        if(firstWarning) {
            // Warn that MOZILLA_FIVE_HOME not set, once.
            printf("Warning: MOZILLA_FIVE_HOME not set.\n");
            firstWarning = PR_FALSE;
        }
#endif /* DEBUG */

        // Fall back to current directory.
        if (getcwd(buffer, MAXPATHLEN))
        {
            return PR_TRUE;
        }
    }

#elif defined(XP_OS2)
    PPIB ppib;
    PTIB ptib;
    char* p;
    DosGetInfoBlocks( &ptib, &ppib);
    DosQueryModuleName( ppib->pib_hmte, MAXPATHLEN, buffer);
    p = strrchr( buffer, '\\'); // XXX DBCS misery
    if (p) {
      *p  = '\0';
      return PR_TRUE;
    }

#elif defined(XP_BEOS)

    char *moz5 = getenv("MOZILLA_FIVE_HOME");
    if (moz5)
    {
      strcpy(buffer, moz5);
      return PR_TRUE;
    }
    else
    {
      int32 cookie = 0;
      image_info info;
      char *p;
      *buffer = 0;
      if(get_next_image_info(0, &cookie, &info) == B_OK)
      {
        strcpy(buffer, info.name);
        if((p = strrchr(buffer, '/')) != 0)
        {
          *p = 0;

          return PR_TRUE;
        }
      }
    }

#endif
    
  return PR_FALSE;
}

/**
 * the GRE location is stored in a static buffer so that we don't have
 * to compute it multiple times.
 */

static char sGRELocation[MAXPATHLEN] = "";

extern "C" char const *
GRE_GetGREPath()
{
  // we've already done this...
  if (*sGRELocation)
    return sGRELocation;
    
  char buffer[MAXPATHLEN];
    
  // If the xpcom library exists in the current process directory,
  // then we will not use any GRE.  The assumption here is that the
  // GRE is in the same directory as the executable.
  if (GRE_GetCurrentProcessDirectory(buffer)) {
    PRUint32 pathlen = strlen(buffer);
    strcpy(buffer + pathlen, XPCOM_FILE_PATH_SEPARATOR XPCOM_DLL);

    struct stat libStat;
    int statResult = stat(buffer, &libStat);
        
    if (statResult != -1) {
      //found our xpcom lib in the current process directory
      buffer[pathlen] = '\0';
      strcpy(sGRELocation, buffer);
      return sGRELocation;
    }
  }

  // if GRE_HOME is in the environment, use that GRE
  const char* env = PR_GetEnv("GRE_HOME");
  if (env && *env) {
#if XP_UNIX
    if (!realpath(env, sGRELocation))
      strcpy(sGRELocation, env);
#elif XP_WIN32
    if (!_fullpath(sGRELocation, env, MAXPATHLEN))
      strcpy(sGRELocation, env);
#endif
    // xxxbsmedberg: it would help that other platforms had a "make absolute" function
    return sGRELocation;
  }

  // the Gecko bits that sit next to the application or in the LD_LIBRARY_PATH
  env = PR_GetEnv("USE_LOCAL_GRE");
  if (env && *env)
    return nsnull;

#if XP_UNIX
  // check in the HOME directory
  env = PR_GetEnv("HOME");
  if (env && *env) {
    sprintf(buffer, "%s" XPCOM_FILE_PATH_SEPARATOR GRE_CONF_NAME, env);
    
    if (GRE_GetPathFromConfigFile(buffer, sGRELocation)) {
      return sGRELocation;
    }
  }
#endif

  env = PR_GetEnv("MOZ_GRE_CONF");
  if (env) {
    if (GRE_GetPathFromConfigFile(env, sGRELocation)) {
      return sGRELocation;
    }
  }

#if XP_UNIX
  // Look for a group of config files in /etc/gre.d/
  if (GRE_GetPathFromConfigDir(GRE_CONF_DIR, sGRELocation)) {
    return sGRELocation;
  }

  // Look for a global /etc/gre.conf file
  if (GRE_GetPathFromConfigFile(GRE_CONF_PATH, sGRELocation)) {
    return sGRELocation;
  }
#endif
  
#if XP_WIN32
  char szKey[256];
  HKEY hRegKey = NULL;
  DWORD dwLength = MAXPATHLEN;
    
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
  strcpy(szKey, GRE_WIN_REG_LOC GRE_BUILD_ID);
    
  if (::RegOpenKeyEx(HKEY_CURRENT_USER, szKey, 0, KEY_QUERY_VALUE, &hRegKey) == ERROR_SUCCESS) {
    if (::RegQueryValueEx(hRegKey, "GreHome", NULL, NULL, (BYTE *)sGRELocation, &dwLength) != ERROR_SUCCESS) {
      *sGRELocation = '\0';
    }
    ::RegCloseKey(hRegKey);

    if (*sGRELocation)
      return sGRELocation;
  }

  if (::RegOpenKeyEx(HKEY_LOCAL_MACHINE, szKey, 0, KEY_QUERY_VALUE, &hRegKey) == ERROR_SUCCESS) {
    if (::RegQueryValueEx(hRegKey, "GreHome", NULL, NULL, (BYTE *)sGRELocation, &dwLength) != ERROR_SUCCESS) {
      *sGRELocation = '\0';
    }
    ::RegCloseKey(hRegKey);

    if (*sGRELocation)
      return sGRELocation;
  }
#endif

  return nsnull;
}

PRBool
GRE_GetPathFromConfigDir(const char* dirname, char* buffer)
{
  // Open the directory provided and try to read any files in that
  // directory that end with .conf.  We look for an entry that might
  // point to the GRE that we're interested in.
  PRDir *dir = PR_OpenDir(dirname);
  if (!dir)
    return nsnull;

  PRBool found = PR_FALSE;
  PRDirEntry *entry;

  while (!found && (entry = PR_ReadDir(dir, PR_SKIP_BOTH))) {

    // Only look for files that end in .conf
    char *offset = PL_strrstr(entry->name, ".conf");
    if (!offset)
      continue;

    if (offset != entry->name + strlen(entry->name) - 5)
      continue;

    nsEmbedCString fullPath;
    NS_CStringAppendData(fullPath, dirname);
    NS_CStringAppendData(fullPath, XPCOM_FILE_PATH_SEPARATOR);
    NS_CStringAppendData(fullPath, entry->name);

    found = GRE_GetPathFromConfigFile(fullPath.get(), buffer);
  }

  PR_CloseDir(dir);

  return found;
}

PRBool
GRE_GetPathFromConfigFile(const char* filename, char* pathBuffer)
{
  *pathBuffer = '\0';
  char buffer[1024];
  FILE *cfg;
  PRBool foundHeader = PR_FALSE;
  PRInt32 versionLen = sizeof(GRE_BUILD_ID)-1;

  if((cfg=fopen(filename,"r"))==nsnull) {
    return nsnull;
  }

  while (fgets(buffer, 1024, cfg) != nsnull) {
    // skip over comment lines and blank lines
    if (buffer[0] == '#' || buffer[0] == '\n') {
      continue;
    }

    // we found a section heading, check to see if it is the one we are intersted in.
    if (buffer[0] == '[') {
      if (!strncmp (buffer+1, GRE_BUILD_ID, versionLen)) {
        foundHeader = PR_TRUE;
      }
      continue;
    }

    if (foundHeader && !strncmp (buffer, "GRE_PATH=", 9)) {
      strcpy(pathBuffer, buffer + 9);
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

extern "C" nsresult
GRE_GetGREDirectory(nsILocalFile* *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  nsresult rv = NS_ERROR_FAILURE;

  // Get the path of the GRE which is compatible with our embedding application
  // from the registry

  const char *pGREDir = GRE_GetGREPath();
  if(pGREDir) {
    nsCOMPtr<nsILocalFile> tempLocal;
    nsEmbedCString leaf;
    NS_CStringSetData(leaf, pGREDir);
    rv = NS_NewNativeLocalFile(leaf, PR_TRUE, getter_AddRefs(tempLocal));

    if (NS_SUCCEEDED(rv)) {
      *_retval = tempLocal;
      NS_ADDREF(*_retval);
    }
  }
  return rv;
}

static char sXPCOMPath[MAXPATHLEN];

extern "C" const char* 
GRE_GetXPCOMPath()
{
  const char* grePath = GRE_GetGREPath();

  if (!grePath) {
    grePath = PR_GetEnv("MOZILLA_FIVE_HOME");
    if (!grePath || !*grePath) {
      return nsnull;
    }
  }

  sprintf(sXPCOMPath, "%s" XPCOM_FILE_PATH_SEPARATOR XPCOM_DLL, grePath);

  return sXPCOMPath;
}
