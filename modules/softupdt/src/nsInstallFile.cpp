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

#include "nsCRT.h"
#include "xp.h"
#include "su_folderspec.h"
#include "su_instl.h"

#include "nsSoftUpdateEnums.h"
#include "VerReg.h"
#include "nsInstallFile.h"
#include "nsSUError.h"

#include "nsPrivilegeManager.h"
#include "nsTarget.h"
#include "jvmmgr.h"

extern int SU_ERROR_INSTALL_FILE_UNEXPECTED;
extern int SU_DETAILS_REPLACE_FILE_MSG_ID;
extern int SU_DETAILS_INSTALL_FILE_MSG_ID;

#ifdef XP_WIN16
XP_Bool	utilityScheduled = FALSE;
#endif

PR_BEGIN_EXTERN_C

static PRBool endsWith(nsString* str, char* string_to_find);

static PRBool endsWith(nsString* str, char* string_to_find) 
{
  PRBool found = PR_FALSE;
  if (str) {
    int len = strlen(".zip");
    int size = str->Length();
    int offset = str->RFind(string_to_find, PR_FALSE);
    if (offset == (size - len))
      found = PR_TRUE;
  }
  return found;
}

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
  force = PR_FALSE;
  bJavaDir = PR_FALSE;
  replace = PR_FALSE;
  bChild = PR_FALSE;
  bUpgrade = PR_FALSE;

  if ((inVRName == NULL) || (inJarLocation == NULL) || 
      (folderSpec == NULL) || (inSoftUpdate == NULL) ||
      (inVInfo == NULL)) {
    *errorMsg = SU_GetErrorMsg3("Invalid arguments to the constructor", 
                               SUERR_INVALID_ARGUMENTS);
    return;
  }
  vrName = new nsString(inVRName);
  versionInfo	= inVInfo; /* XXX: Who owns and who free's this object. Is it nsSoftwareUpdate?? */
  jarLocation = new nsString(inJarLocation);
  force = forceInstall;
  char* temp = folderSpec->MakeFullPath( inPartialPath, errorMsg );
  if (temp != NULL) {
    finalFile = new nsString(temp);
    XP_FREE(temp);
  }
  bJavaDir = folderSpec->IsJavaCapable();

  /* Request impersonation privileges */
  nsTarget* impersonation = nsTarget::findTarget(IMPERSONATOR);
  nsPrivilegeManager* privMgr = nsPrivilegeManager::getPrivilegeManager();

  if ((privMgr != NULL) && (impersonation != NULL)) {
    privMgr->enablePrivilege(impersonation, 1);

    /* check the security permissions */
    nsTarget* install_target = nsTarget::findTarget(INSTALL_PRIV);
    if (install_target != NULL) {
      if (!privMgr->enablePrivilege(install_target, softUpdate->GetPrincipal(), 1)) {
        *errorMsg = SU_GetErrorMsg3("Permssion was denied", SUERR_ACCESS_DENIED);
        return;
      }
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
  if (tempFile)
    delete tempFile;
  if (finalFile)
    delete finalFile;
  if (regPackageName)
    delete regPackageName;
  if (userPackageName)
    delete userPackageName;
}

/* Prepare
 * Extracts file out of the JAR archive into the temp directory
 */
char* nsInstallFile::Prepare()
{
  char *errorMsg = NULL;

  if (softUpdate == NULL) {
    errorMsg = SU_GetErrorMsg3("nsSoftwareUpdate object is null", 
                               SUERR_INVALID_ARGUMENTS);
    return errorMsg;
  }
  if (jarLocation == NULL) {
    errorMsg = SU_GetErrorMsg3("JAR file is null", 
                               SUERR_INVALID_ARGUMENTS);
    return errorMsg;
  }
  if (finalFile == NULL) {
    errorMsg = SU_GetErrorMsg3("folderSpec's full path (finalFile) was null", 
                               SUERR_INVALID_ARGUMENTS);
    return errorMsg;
  }

  // XXX: Make the following security code into a function.

  /* Request impersonation privileges */
  nsTarget* impersonation = nsTarget::findTarget(IMPERSONATOR);
  nsPrivilegeManager* privMgr = nsPrivilegeManager::getPrivilegeManager();

  if ((privMgr != NULL) && (impersonation != NULL)) {
    PRBool allowed = privMgr->enablePrivilege(impersonation, 1);
    if (allowed == PR_FALSE) {
      errorMsg = SU_GetErrorMsg3("Permssion was denied", SUERR_ACCESS_DENIED);
      return errorMsg;
    }

    /* check the security permissions */
    nsTarget* install_target = nsTarget::findTarget(INSTALL_PRIV);
    if (install_target != NULL) {
      PRBool allowed = privMgr->enablePrivilege(install_target, softUpdate->GetPrincipal(), 1);
      if (allowed == PR_FALSE) {
        errorMsg = SU_GetErrorMsg3("Permssion was denied", SUERR_ACCESS_DENIED);
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
    PR_ASSERT(temp == NULL);
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

  if (softUpdate == NULL) {
    return SU_GetErrorMsg3("nsSoftwareUpdate object is null", 
                           SUERR_INVALID_ARGUMENTS);
  }
  if (vrName == NULL) {
    return SU_GetErrorMsg3("version registry name is null", 
                           SUERR_INVALID_ARGUMENTS);
  }
  if (finalFile == NULL) {
    return  SU_GetErrorMsg3("folderSpec's full path (finalFile) is null", 
                            SUERR_INVALID_ARGUMENTS);
  }

  /* Check the security for our target */
  // XXX: Make the following security code into a function.

  /* Request impersonation privileges */
  nsTarget* impersonation = nsTarget::findTarget(IMPERSONATOR);
  nsPrivilegeManager* privMgr = nsPrivilegeManager::getPrivilegeManager();
  nsTarget* install_target = NULL;

  if ((privMgr != NULL) && (impersonation != NULL)) {
    privMgr->enablePrivilege(impersonation, 1);
    
    /* check the security permissions */
    install_target = nsTarget::findTarget(INSTALL_PRIV);
    if (install_target != NULL) {
      if (!privMgr->enablePrivilege(install_target, 
                                    softUpdate->GetPrincipal(), 1)) {
        return SU_GetErrorMsg3("Permssion was denied", 
                               SUERR_ACCESS_DENIED);
      }
    }
  }
  
  err = NativeComplete();
  
  if ((privMgr != NULL) && (install_target != NULL)) {
    privMgr->revertPrivilege(install_target, 1);
  }

  char *vr_name = vrName->ToNewCString();
  char *final_file = finalFile->ToNewCString();
  
  // Add java archives to the classpath. Don't add if we're
  // replacing an existing file -- it'll already be there.
  
  if ( bJavaDir && !replace ) {
    PRBool found_zip = endsWith(finalFile, ".zip");
    PRBool found_jar = endsWith(finalFile, ".jar");;
    if (found_zip || found_jar) {
      AddToClasspath( finalFile );
    }
  }
  
  
  // Register file and log for Uninstall
  
  if ( 0 == err || SU_REBOOT_NEEDED == err ) {
    // we ignore all registry errors because they're not
    // important enough to abort an otherwise OK install.
    if (!bChild) {
      int found;
      if (regPackageName) {
        char *reg_package_name = regPackageName->ToNewCString();
        found = VR_UninstallFileExistsInList( reg_package_name, vr_name );
        delete reg_package_name;
      } else {
        found = VR_UninstallFileExistsInList( "", vr_name );
      }
      if (found != REGERR_OK)
        bUpgrade = PR_FALSE;
      else
        bUpgrade = PR_TRUE;
    } else if (REGERR_OK == VR_InRegistry(vr_name)) {
      bUpgrade = PR_TRUE;
    } else {
      bUpgrade = PR_FALSE;
    }
    
    err = VR_GetRefCount( vr_name, &refCount );
    if ( err != REGERR_OK ) 
    {
      refCount = 0;
    }


    if (!bUpgrade) {
      if (refCount != 0) 
      {
        rc = 1 + refCount;
        VR_Install( vr_name, final_file, versionInfo->toString(), PR_FALSE );
        VR_SetRefCount( vr_name, rc );
      } 
      else 
      {
        if (replace)
        {
            VR_Install( vr_name, final_file, versionInfo->toString(), PR_FALSE);
            VR_SetRefCount( vr_name, 2 );
        }
        else
        {
            VR_Install( vr_name, final_file, versionInfo->toString(), PR_FALSE );
            VR_SetRefCount( vr_name, 1 );
      
        }
      }
    } 
    else if (bUpgrade) 
    {
      if (refCount == 0) 
      {
          VR_Install( vr_name, final_file, versionInfo->toString(), PR_FALSE );
          VR_SetRefCount( vr_name, 1 );
      } 
      else 
      {
          VR_Install( vr_name, final_file, versionInfo->toString(), PR_FALSE );
          VR_SetRefCount( vr_name, 0 );
      }
    }
    
    if ( !bChild && !bUpgrade ) {
      if (regPackageName) {
        char *reg_package_name = regPackageName->ToNewCString();
        VR_UninstallAddFileToList( reg_package_name, vr_name );
        delete reg_package_name;
      } else {
        VR_UninstallAddFileToList( "", vr_name );
      }
    }
  }
  delete vr_name;
  delete final_file;
  
  if ( err != 0 ) {
    return SU_GetErrorMsg2(SU_ERROR_INSTALL_FILE_UNEXPECTED, finalFile, err);
  }
  return NULL;
}

void nsInstallFile::Abort()
{
  NativeAbort();
}

char* nsInstallFile::toString()
{
  if (replace) {
    return SU_GetString2(SU_DETAILS_REPLACE_FILE_MSG_ID, finalFile);
  } else {
    return SU_GetString2(SU_DETAILS_INSTALL_FILE_MSG_ID, finalFile);
  }
}


/* Private Methods */

/* Private Native Methods */
void nsInstallFile::NativeAbort()
{
  char* currentName;
  int result;

  /* Get the names */
  if (tempFile == NULL) 
    return;
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
  char* currentName = NULL;
  char* finalName = NULL;
  char* finalNamePlatform;
  int result = 0;

  if (tempFile == NULL) {
    return -1;
  }
  /* Get the names */
  currentName = tempFile->ToNewCString();

  PR_ASSERT(finalFile != NULL);
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

      if ( result == SU_REBOOT_NEEDED ) {
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



void nsInstallFile::AddToClasspath(nsString* file)
{
  if ( file != NULL ) {
    char *final_file = file->ToNewCString();
    JVM_AddToClassPath(final_file);
    delete final_file;
  }
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

  PR_ASSERT(finalFile != NULL);
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





/* CanUninstall
* InstallFile() installs files which can be uninstalled,
* hence this function returns true. 
*/
PRBool
nsInstallFile::CanUninstall()
{
    return TRUE;
}

/* RegisterPackageNode
* InstallFile() installs files which need to be registered,
* hence this function returns true.
*/
PRBool
nsInstallFile::RegisterPackageNode()
{
    return TRUE;
}

PR_END_EXTERN_C
