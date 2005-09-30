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

#include "nsBuildID.h"

#include "nsEmbedString.h"
#include "nsXPCOMPrivate.h"
#include "nsXPCOMGlue.h"
#include "nsILocalFile.h"
#include "nsIDirectoryService.h"
#include "nsDirectoryServiceDefs.h"
#include "nsCOMPtr.h"
#include "nsMemory.h"

#include "nspr.h"
#include "plstr.h"

#ifdef XP_WIN32
#include <windows.h>
#include <stdlib.h>
#include <mbstring.h>
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

static PRBool GRE_GetCurrentProcessDirectory(char* buffer);

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
    DWORD bufLength = ::GetModuleFileName(0, buffer, MAXPATHLEN);
    if (bufLength == 0 || bufLength == MAXPATHLEN)
        return PR_FALSE;
    // chop of the executable name by finding the rightmost backslash
    unsigned char* lastSlash = _mbsrchr((unsigned char*) buffer, '\\');
    if (lastSlash) {
        *(lastSlash) = '\0';
        return PR_TRUE;
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
    if (getenv("MOZILLA_FIVE_HOME") == nsnull)
    {
        putenv("MOZILLA_FIVE_HOME=" MOZ_DEFAULT_MOZILLA_FIVE_HOME);
    }
#endif

    char *moz5 = getenv("MOZILLA_FIVE_HOME");

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

static char sXPCOMPath[MAXPATHLEN] = "";

extern "C" char const *
GRE_GetXPCOMPath()
{
  // we've already done this...
  if (*sXPCOMPath)
    return sXPCOMPath;

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
      strcpy(sXPCOMPath, buffer);
      return sXPCOMPath;
    }
  }

  static const GREVersionRange version = {
    GRE_BUILD_ID, PR_TRUE,
    GRE_BUILD_ID, PR_TRUE
  };

  GRE_GetGREPathWithProperties(&version, 1,
                               nsnull, 0,
                               sXPCOMPath, MAXPATHLEN);
  if (*sXPCOMPath)
    return sXPCOMPath;

  return nsnull;
}

extern "C" nsresult
GRE_GetGREDirectory(nsILocalFile* *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  nsresult rv = NS_ERROR_FAILURE;

  // Get the path of the GRE which is compatible with our embedding application
  // from the registry

  const char *pGREDir = GRE_GetXPCOMPath();
  if(!pGREDir)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsILocalFile> xpcomPath;
  nsEmbedCString leaf(pGREDir);
  rv = NS_NewNativeLocalFile(leaf, PR_TRUE, getter_AddRefs(xpcomPath));

  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIFile> directory;
  rv = xpcomPath->GetParent(getter_AddRefs(directory));
  if (NS_FAILED(rv))
    return rv;

  return CallQueryInterface(directory, _retval);
}
