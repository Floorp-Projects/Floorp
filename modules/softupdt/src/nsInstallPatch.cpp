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
#include "prprf.h"
#include "xp.h"
#include "xpgetstr.h"
#include "plhash.h"

#include "softupdt.h"
#include "su_instl.h"
#include "su_folderspec.h"
#include "gdiff.h"

#include "nsInstallPatch.h"
#include "nsSoftUpdateEnums.h"
#include "nsSUError.h"

#include "nsTarget.h"
#include "nsPrivilegeManager.h"

#include "VerReg.h"

extern int SU_ERROR_NO_SUCH_COMPONENT;
extern int SU_ERROR_INSTALL_FILE_UNEXPECTED;
extern int SU_ERROR_UNEXPECTED;
extern int SU_DETAILS_PATCH;

PR_BEGIN_EXTERN_C

/* PUBLIC METHODS */

/*  Constructor
 *   inSoftUpdate    - softUpdate object we belong to
 *   inVRName        - full path of the registry component
 *   inVInfo         - full version info
 *   inJarLocation   - location inside the JAR file
 *   folderspec      - FolderSpec of target file
 *   inPartialPath   - target file on disk relative to folderspec
 */
nsInstallPatch::nsInstallPatch(nsSoftwareUpdate* inSoftUpdate,
                               char* inVRName,
                               nsVersionInfo* inVInfo,
                               char* inJarLocation,
                               char* *errorMsg) : nsInstallObject(inSoftUpdate)
{
  int err;

  vrName = NULL;
  versionInfo = NULL;
  jarLocation = NULL;
  patchURL = NULL;
  targetfile = NULL;
  patchedfile = NULL;
  *errorMsg = checkPrivileges();
  if (*errorMsg != NULL)
    return;
  
  targetfile = (char*)XP_CALLOC(MAXREGPATHLEN, sizeof(char));
  err = VR_GetPath( vrName, MAXREGPATHLEN, targetfile );
  
  if (err != REGERR_OK)
  {
     XP_FREEIF(targetfile);
     targetfile = NULL;
  }

  if ( targetfile == NULL ) {
    *errorMsg = SU_GetErrorMsg4(SU_ERROR_NO_SUCH_COMPONENT, 
                                SUERR_NO_SUCH_COMPONENT);
    return;
  }

  if ((inVRName == NULL) || (inJarLocation == NULL)) {
    *errorMsg = SU_GetErrorMsg3("Invalid arguments to the constructor", 
                                SUERR_INVALID_ARGUMENTS);
    return;
  }

  vrName      = XP_STRDUP(inVRName);
  versionInfo = inVInfo; /* Who owns this object? May be we should make a copy of it */
  jarLocation = XP_STRDUP(inJarLocation);
}


nsInstallPatch::nsInstallPatch(nsSoftwareUpdate* inSoftUpdate,
                               char* inVRName,
                               nsVersionInfo* inVInfo,
                               char* inJarLocation,
                               nsFolderSpec* folderSpec,
                               char* inPartialPath, 
                               char* *errorMsg) : nsInstallObject(inSoftUpdate)
{
  vrName = NULL;
  versionInfo = NULL;
  jarLocation = NULL;
  patchURL = NULL;
  targetfile = NULL;
  patchedfile = NULL;

  if ((inVRName == NULL) || (inJarLocation == NULL) || (folderSpec == NULL)) {
    *errorMsg = SU_GetErrorMsg3("Invalid arguments to the constructor", 
                                SUERR_INVALID_ARGUMENTS);
    return;
  }

  *errorMsg = checkPrivileges();
  if (*errorMsg != NULL)
    return;

  targetfile = folderSpec->MakeFullPath( inPartialPath, errorMsg );
  if ( errorMsg != NULL ) {
    return;
  }
  vrName      = XP_STRDUP(inVRName);
  versionInfo = inVInfo;
  jarLocation = XP_STRDUP(inJarLocation);
}


nsInstallPatch::~nsInstallPatch()
{
  XP_FREEIF(vrName);
  XP_FREEIF(jarLocation);
  XP_FREEIF(patchURL);
  XP_FREEIF(targetfile);
  XP_FREEIF(patchedfile);
  /* Raman: Fix it. How do we delete versionInfo. If we have copy on our side it is easy. */
  // ?? delete versionInfo;
}

char* nsInstallPatch::Prepare(void)
{
  char* errorMsg = NULL;
  char* srcname = NULL;
  PRBool deleteOldSrc;
  nsTarget* priv = NULL;
  char * targetfileURL;
  XP_StatStruct statinfo;
  int err = 0;

  if ((softUpdate == NULL) || (jarLocation == NULL) || (targetfile == NULL)) {
    return SU_GetErrorMsg3("Invalid arguments to the constructor", 
                           SUERR_INVALID_ARGUMENTS);
  }

  nsPrivilegeManager* privMgr = nsPrivilegeManager::getPrivilegeManager();
  nsTarget* impersonation = nsTarget::findTarget(IMPERSONATOR);

  if ((privMgr != NULL) && (impersonation != NULL)) {
    /* We should fix the DEPTH
     */
    privMgr->enablePrivilege(impersonation, 1);
    priv = nsTarget::findTarget(INSTALL_PRIV);
    if (priv != NULL) {
      if (!privMgr->enablePrivilege( priv, softUpdate->GetPrincipal(), 1)) {
        return SU_GetErrorMsg3("Permssion was denied", SUERR_ACCESS_DENIED);
      }
    }
  }

  /* Check for READONLY or DIRECTORY */
  	
          
	targetfileURL = XP_PlatformFileToURL(targetfile);
    if (targetfileURL != NULL)
	{
		char * temp = XP_STRDUP(&targetfileURL[7]);
		XP_FREEIF(targetfileURL);
		targetfileURL = temp;
		if (targetfileURL)
		{
			err = XP_Stat(targetfileURL, &statinfo, xpURL);
			if (err != -1)
			{
				if ( XP_STAT_READONLY( statinfo ) )
				{
					errorMsg = SU_GetErrorMsg4(SU_ERROR_UNEXPECTED, SUERR_FILE_READ_ONLY);
					
				}
				else if (!S_ISDIR(statinfo.st_mode))
				{
					errorMsg =  SU_GetErrorMsg4(SU_ERROR_UNEXPECTED, SUERR_FILE_IS_DIRECTORY);
				}
			}
			else
			{
				errorMsg =  SU_GetErrorMsg4(SU_ERROR_UNEXPECTED, SUERR_FILE_DOES_NOT_EXIST);
			}
			
			XP_FREEIF(targetfileURL);
		}
	}

	if (errorMsg != NULL)
	{
		return errorMsg;
	}

  /* DFT */
  
  patchURL = softUpdate->ExtractJARFile( jarLocation, targetfile, &errorMsg );

  IntegerKey ikey(PL_HashString(targetfile));
  srcname = (char *)softUpdate->patchList->Get( &ikey );
  if (srcname != NULL) {
    deleteOldSrc = PR_TRUE;
  } else {
    srcname = targetfile;
    deleteOldSrc = PR_FALSE;
  }

  patchedfile = NativePatch( srcname, patchURL, &errorMsg );
  if (errorMsg != NULL) {
    return errorMsg;
  }

  if ( patchedfile != NULL ) {
    softUpdate->patchList->Put( &ikey, patchedfile );
  } else {
    char *msg = XP_Cat(targetfile, " not patched");
    errorMsg = SU_GetErrorMsg3(msg, SUERR_INVALID_ARGUMENTS);
  }

  if ( deleteOldSrc ) {
    NativeDeleteFile( srcname );
  }
  return errorMsg;
}

/* Complete
 * Completes the install:
 * - move the patched file to the final location
 * - updates the registry
 */
char* nsInstallPatch::Complete(void)
{
  int err;
  char* errorMsg = NULL;

  if ((softUpdate == NULL) || (targetfile == NULL) || 
      (patchedfile == NULL) || (vrName == NULL)) {
    return SU_GetErrorMsg3("Invalid arguments to the complete method", 
                           SUERR_INVALID_ARGUMENTS);
  }

  if ((errorMsg = checkPrivileges()) != NULL)
    return errorMsg;

  IntegerKey ikey(PL_HashString(targetfile));
  char* tmp = (char *)softUpdate->patchList->Get( &ikey );
  if (tmp && ( XP_STRCMP(tmp, patchedfile ) == 0 )) {

    // the patch has not been superceded--do final replacement
            
    err = NativeReplace( targetfile, patchedfile );
    
    if ( 0 == err || SU_REBOOT_NEEDED == err ) {
      err = VR_Install( vrName, targetfile, versionInfo->toString(), PR_FALSE );
      if ( err != 0 ) {
        char *msg = XP_Cat("Install component failed ", targetfile);
        errorMsg = SU_GetErrorMsg3(msg, err);
        XP_FREE(msg);
      }
    } else {
      char *msg = SU_GetString1(SU_ERROR_INSTALL_FILE_UNEXPECTED, targetfile);
      errorMsg = SU_GetErrorMsg3(msg, err);
      XP_FREE(msg);
    }
  } else {
    // nothing -- old intermediate patched file was
    // deleted by a superceding patch
  }
  return errorMsg;
}

void nsInstallPatch::Abort(void)
{
  // clean up patched file unless it has been already
  // deleted by a superceding patch
  
  if ((softUpdate == NULL) || (targetfile == NULL) || 
      (patchedfile == NULL)) {
    return;
  }

  IntegerKey ikey(PL_HashString(targetfile));
  
  char* tmp = (char *)softUpdate->patchList->Get( &ikey );
  if (tmp && ( XP_STRCMP(tmp, patchedfile ) == 0 )) {
    NativeDeleteFile( patchedfile );
  }
}

char* nsInstallPatch::toString(void)
{
  return SU_GetString1(SU_DETAILS_PATCH, targetfile);
}


/* PRIVATE METHODS */

char* nsInstallPatch::checkPrivileges(void)
{
  // This won't actually give us these privileges because
  // we lose them again as soon as the function returns. But we
  // don't really need the privs, we're just checking security

  nsTarget* priv = NULL;

  nsPrivilegeManager* privMgr = nsPrivilegeManager::getPrivilegeManager();
  nsTarget* impersonation = nsTarget::findTarget(IMPERSONATOR);

  if ((privMgr != NULL) && (impersonation != NULL)) {
    /* We should fix the DEPTH
     */
    privMgr->enablePrivilege(impersonation, 1);
    priv = nsTarget::findTarget(INSTALL_PRIV);
    if (priv != NULL) {
      if (!privMgr->enablePrivilege( priv, softUpdate->GetPrincipal(), 1)) {
        return SU_GetErrorMsg3("Permssion was denied", SUERR_ACCESS_DENIED);
      }
    }
  }
  return NULL;
}

char* nsInstallPatch::NativePatch( char* srcfile, char* diffURL, char* *errorMsg )
{
  char * fullSrcURL   = NULL;
  char * srcURL = NULL;
  char * newfileURL = NULL;
  char * newfile = NULL;
  int32 err = GDIFF_OK;

  *errorMsg = NULL;

  /* get all the required filenames in the correct URL format */
  
  if ( srcfile != NULL && diffURL != NULL ) {
    fullSrcURL = XP_PlatformFileToURL( srcfile );
    /* skip "file://" part */
    if ( ( fullSrcURL != NULL ) && (XP_STRLEN(fullSrcURL) > 7)) {
      char ch;
      char *p;
      
      srcURL = fullSrcURL+7; /* skip "file://" part */
      p = XP_STRRCHR( srcURL, '/' );
      if ( p != NULL ) {
        ch = p[1];
        p[1] = 0;
        newfileURL = WH_TempName( xpURL, srcURL );
        p[1] = ch;
      }
    }
  }
  
  
  /* apply the patch if conversions worked */
  
  if ( newfileURL != NULL ) {
    err = SU_PatchFile( srcURL, xpURL, diffURL, xpURL, newfileURL, xpURL );
  } else {
    /* String conversions failed -- probably out of memory */
    err = SUERR_INVALID_ARGUMENTS;
  }
  
  
  if ( err == GDIFF_OK ) {
    /* everything's fine, convert to java string and return */
    newfile = WH_FileName( newfileURL, xpURL );
  } else {
    /* convert GDIFF_ERR to SoftwareUpdate error and throw it */
    /* XXX: replace with correct error message! */
    *errorMsg = SU_GetErrorMsg4(SU_ERROR_UNEXPECTED, err);
  }
  
  
  /* cleanup */

  if ( diffURL != NULL )
    XP_FileRemove( diffURL, xpURL );
  
  XP_FREEIF( newfileURL );
  XP_FREEIF( fullSrcURL );
  
  return (newfile);
}

int   nsInstallPatch::NativeReplace( char* target, char* newfile )
{
  char * targetURL = NULL;
  char * newURL = NULL;
  char * pTarget;
  char * pNew;
  int    err = SU_SUCCESS;
  
  if ( target != NULL && newfile != NULL ) {
    targetURL = XP_PlatformFileToURL( target );
    newURL    = XP_PlatformFileToURL( newfile );
    
    if ( targetURL != NULL && newURL != NULL ) {
      XP_StatStruct st;
      
      pTarget = targetURL+7;
      pNew    = newURL+7;
      
      if ( XP_Stat( pTarget, &st, xpURL ) == SU_SUCCESS ) {
        /* file still exists */
        err = FE_ReplaceExistingFile( pNew, xpURL, pTarget, xpURL, 0 );
#ifdef XP_WIN16
        if ( err == SU_REBOOT_NEEDED && !utilityScheduled) {
          utilityScheduled = TRUE;
          FE_ScheduleRenameUtility();
        }
#endif
      } else {
        /* someone got rid of the target file? */
        /* can do simple rename, but assert */
        err = XP_FileRename( pNew, xpURL, pTarget, xpURL );
        XP_ASSERT( err == SU_SUCCESS );
        XP_ASSERT(0);
      }
    } else {
      err = -1;
    }
  } else {
    err = -1;
  }
  
  XP_FREEIF( targetURL );
  XP_FREEIF( newURL );
  
  return (err);
}

void  nsInstallPatch::NativeDeleteFile( char* filename )
{
  char * fnameURL = NULL;
  char * fname = NULL;
  int result;
  XP_StatStruct s;

  if ( filename != NULL ) {
    fnameURL = XP_PlatformFileToURL( filename );
    XP_ASSERT( fnameURL );
    
    if ( fnameURL != NULL ) {
      fname = fnameURL+7;
      
      if ( XP_Stat( fname, &s, xpURL ) == 0 ) {
        /* file is still here */
        result = XP_FileRemove( fname, xpURL );
        if ( result != SU_SUCCESS ) {
          /* XXX: schedule for later */
        }
      }
    }
  }
  XP_FREEIF( fnameURL );
}


/* CanUninstall
* InstallPatch() installs files, which can be uninstalled,
* hence this function returns true. 
*/
PRBool
nsInstallPatch::CanUninstall()
{
    return TRUE;
}

/* RegisterPackageNode
* InstallPatch() installs files which need to be registered,
* hence this function returns true.
*/
PRBool
nsInstallPatch::RegisterPackageNode()
{
    return TRUE;
}

PR_END_EXTERN_C
