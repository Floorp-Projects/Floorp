/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsCRT.h"
#include "xp.h"
#include "nsInstallFile.h"
#include "nsVersionRegistry.h"
#include "nsSUError.h"
#include "nsSoftUpdateEnums.h"
#include "nsPrivilegeManager.h"
#include "nsTarget.h"

PR_BEGIN_EXTERN_C

/* Public Methods */

/*	Constructor
        inSoftUpdate    - softUpdate object we belong to
        inComponentName	- full path of the registry component
        inVInfo	        - full version info
        inJarLocation   - location inside the JAR file
        inFinalFileSpec	- final	location on disk
*/
nsInstallFile::nsInstallFile(nsSoftwareUpdate* inSoftUpdate,
                             char* inVRName,
                             nsVersionInfo* inVInfo,
                             char* inJarLocation,
                             nsFolderSpec* folderSpec,
                             char* inPartialPath,
                             PRBool forceInstall,
                             char* *errorMsg) : nsInstallObject(inSoftUpdate)
{
  tempFile = NULL;
  vrName = NULL;
  jarLocation = NULL;
  versionInfo = NULL;
  finalFile = NULL;
  regPackageName = NULL;
  userPackageName = NULL;
  target = NULL;
  force = PR_FALSE;
  bJavaDir = PR_FALSE;
  replace = PR_FALSE;
  bChild = PR_FALSE;
  bUpgrade = PR_FALSE;

  vrName = new nsString(inVRName);
  versionInfo	= inVInfo;
  jarLocation = new nsString(inJarLocation);
  force = forceInstall;
  char* temp = folderSpec->MakeFullPath( inPartialPath, errorMsg );
  if (temp != NULL) {
    finalFile = new nsString(temp);
  }
  bJavaDir = folderSpec->IsJavaCapable();

  /* Request impersonation privileges */
  nsTarget* impersonation = nsTarget::findTarget(IMPERSONATOR);
  nsPrivilegeManager* privMgr = nsPrivilegeManager::getPrivilegeManager();

  /* XXX: We are using security to display the dialog. Thus depth may not matter */
  if ((privMgr != NULL) && (impersonation != NULL)) {
    privMgr->enablePrivilege(impersonation, 1);

    /* check the security permissions */
    target = nsTarget::findTarget(INSTALL_PRIV);
    if (target != NULL) {
      /* XXX: we need a way to indicate that a dialog box should appear. */
      privMgr->enablePrivilege(target, softUpdate->GetPrincipal(), 1);
    }
  }
        
  temp = inSoftUpdate->GetUserPackageName();
  if (temp != NULL) {
    userPackageName = new nsString(temp);
  }
  temp = inSoftUpdate->GetRegPackageName();
  if (temp != NULL) {
    regPackageName = new nsString(temp);
  }

  // determine Child status
  if ( regPackageName == NULL ) {
    // in the "current communicator package" absolute pathnames (start
    // with slash) indicate shared files -- all others are children
    bChild = ( vrName->CharAt(0) != '/' );
  } else {
    //bChild = vrName.startsWith(regPackageName);
    /* Because nsString doesn't support startWith, implemented the following. Waiting for approval */
    bChild = (nsCRT::strncmp((PRUnichar*)vrName, (PRUnichar*)regPackageName, 
                             nsCRT::strlen((PRUnichar*)regPackageName)) == 0);
  }

  replace = NativeDoesFileExist();

}


nsInstallFile::~nsInstallFile()
{
  delete vrName;
  delete jarLocation;
  if (finalFile)
    delete finalFile;
  if (userPackageName)
    delete userPackageName;
  if (regPackageName)
    delete regPackageName;
  if (tempFile)
    delete tempFile;
}

/* Prepare
 * Extracts file out of the JAR archive into the temp directory
 */
char* nsInstallFile::Prepare()
{
  char *errorMsg = NULL;

  // XXX: Make the following security code into a function.

  /* Request impersonation privileges */
  nsTarget* impersonation = nsTarget::findTarget(IMPERSONATOR);
  nsPrivilegeManager* privMgr = nsPrivilegeManager::getPrivilegeManager();

  /* XXX: We are using security to display the dialog. Thus depth may not matter */
  if ((privMgr != NULL) && (impersonation != NULL)) {
    PRBool allowed = privMgr->enablePrivilege(impersonation, 1);
    if (allowed == PR_FALSE) {
      errorMsg = SU_GetErrorMsg3("Permssion was denied", nsSoftUpdateError_ACCESS_DENIED);
      return errorMsg;
    }

    /* check the security permissions */
    if (target != NULL) {
      /* XXX: we need a way to indicate that a dialog box should appear. */
      PRBool allowed = privMgr->enablePrivilege(target, softUpdate->GetPrincipal(), 1);
      if (allowed == PR_FALSE) {
        errorMsg = SU_GetErrorMsg3("Permssion was denied", nsSoftUpdateError_ACCESS_DENIED);
        return errorMsg;
      }
    }
  }

  char* jarLocationCharPtr = jarLocation->ToNewCString();
  char* finalFileCharPtr = finalFile->ToNewCString();
  char* temp = softUpdate->ExtractJARFile(jarLocationCharPtr, finalFileCharPtr, &errorMsg);
  delete jarLocationCharPtr;
  delete finalFileCharPtr;
  if (errorMsg != NULL) {
    return errorMsg;
  }
  if (temp != NULL) {
    tempFile = new nsString(temp);
    free(temp);
  }
  return NULL;
}

/* Complete
 * Completes the install:
 * - move the downloaded file to the final location
 * - updates the registry
 */
char* nsInstallFile::Complete()
{
  int err;
  int refCount;
  int rc;

  /* Check the security for our target */
  // XXX: Make the following security code into a function.

  /* Request impersonation privileges */
  nsTarget* impersonation = nsTarget::findTarget(IMPERSONATOR);
  nsPrivilegeManager* privMgr = nsPrivilegeManager::getPrivilegeManager();

  /* XXX: We are using security to display the dialog. Thus depth may not matter */
  if ((privMgr != NULL) && (impersonation != NULL)) {
    privMgr->enablePrivilege(impersonation, 1);
    
    /* check the security permissions */
    if (target != NULL) {
      /* XXX: we need a way to indicate that a dialog box should appear. */
      privMgr->enablePrivilege(target, softUpdate->GetPrincipal(), 1);
    }
  }
  
  err = NativeComplete();
  
  if ((privMgr != NULL) && (target != NULL)) {
    privMgr->revertPrivilege(target, 1);
  }
  
  
  // Add java archives to the classpath. Don't add if we're
  // replacing an existing file -- it'll already be there.
  
  if ( bJavaDir && !replace ) {
    PRBool found_zip = PR_FALSE;
    PRBool found_jar = PR_FALSE;
    int len = strlen(".zip");
    int size = finalFile->Length();
    int offset = finalFile->RFind(".zip", PR_FALSE);
    if (offset == (size - len))
      found_zip = PR_TRUE;
    offset = finalFile->RFind(".jar", PR_FALSE);
    if (offset == (size - len))
      found_jar = PR_TRUE;
    if (found_zip || found_jar) {
      AddToClasspath( finalFile );
    }
  }
  
  
  // Register file and log for Uninstall
  
  if ( 0 == err || nsSoftwareUpdate_REBOOT_NEEDED == err ) {
    // we ignore all registry errors because they're not
    // important enough to abort an otherwise OK install.
    if (!bChild) {
      int found;
      /* XXX: Fix it. memeory leak */
      found = nsVersionRegistry::uninstallFileExists(regPackageName->ToNewCString(), vrName->ToNewCString());
      if (found != REGERR_OK)
        bUpgrade = false;
      else
        bUpgrade = true;
    } else if (REGERR_OK == nsVersionRegistry::inRegistry(vrName->ToNewCString())) {
      bUpgrade = true;
    } else {
      bUpgrade = false;
    }
    
    refCount = nsVersionRegistry::getRefCount(vrName->ToNewCString());
    if (!bUpgrade) {
      if (refCount != 0) {
        rc = 1 + refCount;
        nsVersionRegistry::installComponent(vrName->ToNewCString(), finalFile->ToNewCString(), versionInfo, rc );
      } else {
        if (replace)
          nsVersionRegistry::installComponent(vrName->ToNewCString(), finalFile->ToNewCString(), versionInfo, 2);
        else
          nsVersionRegistry::installComponent(vrName->ToNewCString(), finalFile->ToNewCString(), versionInfo, 1);
      }
    } else if (bUpgrade) {
      if (refCount == 0) {
        nsVersionRegistry::installComponent(vrName->ToNewCString(), finalFile->ToNewCString(), versionInfo, 1);
      } else {
        nsVersionRegistry::installComponent(vrName->ToNewCString(), finalFile->ToNewCString(), versionInfo );
      }
    }
    
    if ( !bChild && !bUpgrade ) {
      nsVersionRegistry::uninstallAddFile(regPackageName->ToNewCString(), vrName->ToNewCString());
    }
  }
  
  if ( err != 0 ) {
    return SU_GetErrorMsg2(err, finalFile);
  }
  return NULL;
}

void nsInstallFile::Abort()
{
  NativeAbort();
}

char* nsInstallFile::toString()
{
#ifdef XXX
  if (replace) {
    return SU_GetString2(details_ReplaceFile_MSG_ID, finalFile);
  } else {
    return SU_GetString2(details_InstallFile_MSG_ID, finalFile);
  }
#else
  return NULL;
#endif
}


/* Private Methods */

/* Private Native Methods */
void nsInstallFile::NativeAbort()
{
  char* currentName;
  int result;

  /* Get the names */
  currentName = tempFile->ToNewCString();

  result = XP_FileRemove(currentName, xpURL);
  XP_ASSERT(result == 0); /* XXX: need to fe_deletefilelater() or something */
  delete currentName;
}

/* NativeComplete 
 * copies the file to its final location
 * Tricky, we need to create the directories 
 */
int nsInstallFile::NativeComplete()
{
  char* currentName;
  char* finalName = NULL;
  char* finalNamePlatform;
  int result = 0;

  /* Get the names */
  currentName = tempFile->ToNewCString();
  
  finalNamePlatform = finalFile->ToNewCString();
  finalName = XP_PlatformFileToURL(finalNamePlatform);
	
  if ( finalName == NULL || currentName == NULL ) {
    /* memory or JRI problems */
    result = -1;
    goto end;
  } else {
    /* convert finalName name to xpURL form by stripping "file://" */
    char *temp = XP_STRDUP(&finalName[7]);
    XP_FREE(finalName);
    finalName = temp;
  }

  if (finalName != NULL) {
    if ( XP_STRCMP(finalName, currentName) == 0 ) {
      /* No need to rename, they are the same */
      result = 0;
    } else {
      XP_StatStruct s;
      if ( XP_Stat( finalName, &s, xpURL ) != 0 ) {
        /* Target file doesn't exist, try to rename file */
        result = XP_FileRename(currentName, xpURL, finalName, xpURL);
      } else {
        /* Target exists, can't trust XP_FileRename--do platform
         * specific stuff in FE_ReplaceExistingFile()
         */
        result = -1;
      }
    }
  } else {
    /* memory problem */
    result = -1;
  }

  if (result != 0) {
    XP_StatStruct s;
    if ( XP_Stat( finalName, &s, xpURL ) == 0 ) {
      /* File already exists, need to remove the original */
      result = FE_ReplaceExistingFile(currentName, xpURL, finalName, xpURL, force);

      if ( result == nsSoftwareUpdate_REBOOT_NEEDED ) {
#ifdef XP_WIN16
        if (!utilityScheduled) {
          utilityScheduled = PR_TRUE;
          FE_ScheduleRenameUtility();
        }
#endif
      }
    } else {
      /* Directory might not exist, check and create if necessary */
      char separator;
      char * end;
      separator = '/';
      end = XP_STRRCHR(finalName, separator);
      if (end) {
        end[0] = 0;
        result = XP_MakeDirectoryR( finalName, xpURL);
        end[0] = separator;
        if ( 0 == result )
          result = XP_FileRename(currentName, xpURL, finalName, xpURL);
      }
    }
#ifdef XP_UNIX
    /* Last try, can't rename() across file systems on UNIX */
    if ( -1 == result ) {
      result = FE_CopyFile(currentName, finalName);
    }
#endif
  }

end:
  XP_FREEIF(finalName);
  delete currentName;
  delete finalNamePlatform;
  return result;
}

/* Finds out if the file exists
 */
PRBool nsInstallFile::NativeDoesFileExist()
{
  char* fileName;
  char* fileNamePlatform;
  int32 err;
  XP_StatStruct statinfo;
  XP_Bool exists = FALSE;

  fileNamePlatform = finalFile->ToNewCString();
  fileName = XP_PlatformFileToURL(fileNamePlatform);
  if (fileName != NULL) {
    char * temp = XP_STRDUP(&fileName[7]);
    XP_FREEIF(fileName);
    fileName = temp;
    
    if (fileName) {
      err = XP_Stat(fileName, &statinfo, xpURL);
      if (err != -1) {
        exists = PR_TRUE;
      }
    }
  }
  XP_FREEIF(fileName);
  delete fileNamePlatform;
  return exists;

}

void nsInstallFile::AddToClasspath(nsString* file)
{
  if ( file != NULL ) {
    /* XXX: What should we do?
    LJ_AddToClassPath((PRUnichar*)file);
    */
  }
}

PR_END_EXTERN_C
