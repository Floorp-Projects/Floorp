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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
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

#include "prmem.h"
#include "prmon.h"
#include "prlog.h"
#include "prthread.h"
#include "fe_proto.h"
#include "xp.h"
#include "xp_str.h"
#include "xpgetstr.h"
#include "prefapi.h"
#include "proto.h"
#include "prprf.h"
#include "prthread.h"
#include "nsFolderSpec.h"
#include "nsSUError.h"

extern "C" {
#include "su_folderspec.h"
}

#ifndef MAX_PATH
#if defined(XP_WIN) || defined(XP_OS2)
#define MAX_PATH _MAX_PATH
#endif
#ifdef XP_UNIX
#if defined(HPUX) || defined(SCO)
/*
** HPUX: PATH_MAX is defined in <limits.h> to be 1023, but they
**       recommend that it not be used, and that pathconf() be
**       used to determine the maximum at runtime.
** SCO:  This is what MAXPATHLEN is set to in <arpa/ftp.h> and
**       NL_MAXPATHLEN in <nl_types.h>.  PATH_MAX is defined in
**       <limits.h> to be 256, but the comments in that file
**       claim the setting is wrong.
*/
#define MAX_PATH 1024
#else
#define MAX_PATH PATH_MAX
#endif
#endif
#endif

extern int SU_INSTALL_ASK_FOR_DIRECTORY;


PR_BEGIN_EXTERN_C

/* Public Methods */

/* Constructor
 */
nsFolderSpec::nsFolderSpec(char* inFolderID , char* inVRPath, char* inPackageName)
{
  char *errorMsg = NULL;
  
  urlPath = folderID = versionRegistryPath = userPackageName = NULL;

  /* Since urlPath is set to NULL, this FolderSpec is essentially the error message */
  if ((inFolderID == NULL) || (inVRPath == NULL) || (inPackageName == NULL)) {
    return;
  }

  folderID = XP_STRDUP(inFolderID);
  versionRegistryPath = XP_STRDUP(inVRPath);
  userPackageName = XP_STRDUP(inPackageName);
  
  /* Setting the urlPath to a real file patch. */
  
  *errorMsg = NULL;
  urlPath = SetDirectoryPath(&errorMsg);
  if (errorMsg != NULL) 
  {
    urlPath = NULL;
    return;
  }
  
  
  
}

nsFolderSpec::~nsFolderSpec(void)
{
  if (folderID) 
    XP_FREE(folderID);
  if (versionRegistryPath) 
    XP_FREE(versionRegistryPath);
  if (userPackageName) 
    XP_FREE(userPackageName);
  if (urlPath) 
    XP_FREE(urlPath);
}


/*
 * GetDirectoryPath
 * Returns urlPath
 * 
 * Caller should not dispose of the return value
 */
char* nsFolderSpec::GetDirectoryPath(void)
{
  return urlPath;
}


/*
 * SetDirectoryPath
 * sets full path to the directory in the standard URL form
 */
char* nsFolderSpec::SetDirectoryPath(char* *errorMsg)
{
  if ((folderID == NULL) || (versionRegistryPath == NULL)) {
    *errorMsg = SU_GetErrorMsg3("Invalid arguments to the constructor", 
                               SUERR_INVALID_ARGUMENTS);
    return NULL;
  }

  if (urlPath == NULL) {
    if (XP_STRCMP(folderID, "User Pick") == 0)  {
      // Default package folder

      // Try the version registry default path first
      // urlPath = VersionRegistry.getDefaultDirectory( versionRegistryPath );
      // if (urlPath == NULL)
      urlPath = PickDefaultDirectory(errorMsg);

    } else if (XP_STRCMP(folderID, "Installed") == 0)  {
      // Then use the Version Registry path
      urlPath = XP_STRDUP(versionRegistryPath);
    } else {
      // Built-in folder
      /* NativeGetDirectoryPath updates urlPath */
      int err = NativeGetDirectoryPath();
      if (err != 0)
        *errorMsg = SU_GetErrorMsg3(folderID, err);
    }
  }
  return urlPath;
}

/**
 * Returns full path to a file. Makes sure that the full path is bellow
 * this directory (security measure)
 *
 * Caller should free the returned string
 *
 * @param relativePath      relative path
 * @return                  full path to a file
 */
char* nsFolderSpec::MakeFullPath(char* relativePath, char* *errorMsg)
{
  // Security check. Make sure that we do not have '.. in the name
  // if ( (GetSecurityTargetID() == SoftwareUpdate.LIMITED_INSTALL) &&
  //        ( relativePath.regionMatches(0, "..", 0, 2)))
  //    throw new SoftUpdateException(Strings.error_IllegalPath(), SoftwareUpdate.ILLEGAL_RELATIVE_PATH );
  char *fullPath=NULL;
  char *dir_path;
  *errorMsg = NULL;
  dir_path = GetDirectoryPath();
  if (dir_path == NULL) 
  {
    return NULL;
  }
  fullPath = XP_Cat(dir_path, GetNativePath(relativePath));
  return fullPath;
}

/* 
 * The caller is supposed to free the memory. 
 */
char* nsFolderSpec::toString()
{
  char* path = GetDirectoryPath();
  char* copyPath = NULL;

  if (path != NULL) 
  {
    copyPath = XP_STRDUP(path);
  }
  return copyPath;
}


/* Private Methods */

/* PickDefaultDirectory
 * asks the user for the default directory for the package
 * stores the choice
 */
char* nsFolderSpec::PickDefaultDirectory(char* *errorMsg)
{
  urlPath = NativePickDefaultDirectory();

  if (urlPath == NULL)
    *errorMsg = SU_GetErrorMsg3(folderID, SUERR_INVALID_PATH_ERR);

  return urlPath;
}


/* Private Native Methods */

/* NativeGetDirectoryPath
 * gets a platform-specific directory path
 * stores it in urlPath
 */
int nsFolderSpec::NativeGetDirectoryPath()
{
  su_DirSpecID folderDirSpecID;
  char*	folderPath = NULL;

  /* Get the name of the package to prompt for */

  folderDirSpecID = MapNameToEnum(folderID);
  switch (folderDirSpecID) 
    {
    case eBadFolder:
      return SUERR_INVALID_PATH_ERR;

    case eCurrentUserFolder:
      {
        char dir[MAX_PATH];
        int len = MAX_PATH;
        if ( PREF_GetCharPref("profile.directory", dir, &len) == PREF_NOERROR)      
          {
            char * platformDir = WH_FileName(dir, xpURL);
            if (platformDir)
              folderPath = AppendSlashToDirPath(platformDir);
            XP_FREEIF(platformDir);
          }
      }
    break;

    default:
      /* Get the FE path */
      folderPath = FE_GetDirectoryPath(folderDirSpecID);
      break;
    }


  /* Store it in the object */
  if (folderPath != NULL) {
    urlPath = NULL;
    urlPath = XP_STRDUP(folderPath);
    XP_FREE(folderPath);
    return 0;
  }

  return SUERR_INVALID_PATH_ERR;
}

/* GetNativePath
 * returns a native equivalent of a XP directory path
 */
char* nsFolderSpec::GetNativePath(char* path)
{
  char *xpPath, *p;
  char pathSeparator;

#define XP_PATH_SEPARATOR  '/'

#ifdef XP_WIN
  pathSeparator = '\\';
#elif defined(XP_MAC)
  pathSeparator = ':';
#else /* XP_UNIX */
  pathSeparator = '/';
#endif

  p = xpPath = path;

  /*
   * Traverse XP path and replace XP_PATH_SEPARATOR with
   * the platform native equivalent
   */
  if ( p == NULL )
    {
      xpPath = "";
    }
  else
    {
      while ( *p )
        {
          if ( *p == XP_PATH_SEPARATOR )
            *p = pathSeparator;
          ++p;
        }
    }

  return xpPath;
}

/*
 * NativePickDefaultDirectory
 * Platform-specific implementation of GetDirectoryPath
 */
char* nsFolderSpec::NativePickDefaultDirectory(void)
{
  su_PickDirTimer callback;
  char * packageName;
  char prompt[200];

 
  callback.context = XP_FindSomeContext();
  callback.fileName = NULL;
  callback.done = FALSE;
  /* Get the name of the package to prompt for */
  packageName = userPackageName;

  if (packageName)
    {
      /* In Java thread now, and need to call FE_PromptForFileName
       * from the main thread
       * post an event on a timer, and busy-wait until it completes.
       */
      PR_snprintf(prompt, 200, XP_GetString(SU_INSTALL_ASK_FOR_DIRECTORY), packageName); 
      callback.prompt = prompt;
      FE_SetTimeout( pickDirectoryCallback, &callback, 1 );
      while (!callback.done)  /* Busy loop for now */
        PR_Sleep(PR_INTERVAL_NO_WAIT); /* java_lang_Thread_yield(WHAT?); */
    }

  return callback.fileName;
}

PRBool nsFolderSpec::NativeIsJavaDir()
{
  char* folderName;

  /* Get the name of the package to prompt for */
  folderName = folderID;

  PR_ASSERT( folderName );
  if ( folderName != NULL) {
    int i;
    for (i=0; DirectoryTable[i].directoryName[0] != 0; i++ ) {
      if ( XP_STRCMP(folderName, DirectoryTable[i].directoryName) == 0 )
        return DirectoryTable[i].bJavaDir;
    }
  }

  return PR_FALSE;
}

PR_END_EXTERN_C

