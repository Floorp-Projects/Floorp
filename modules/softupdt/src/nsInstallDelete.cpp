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
#include "softupdt.h"
#include "su_instl.h"
#include "nsInstallDelete.h"
#include "nsSUError.h"
#include "VerReg.h"

#include "nsPrivilegeManager.h"
#include "nsTarget.h"

extern int SU_DETAILS_DELETE_FILE;
extern int SU_DETAILS_DELETE_COMPONENT;
extern int SU_ERROR_NOT_IN_REGISTRY;
extern int SU_ERROR_FILE_READ_ONLY;
extern int SU_ERROR_FILE_IS_DIRECTORY;
extern int SU_ERROR_UNEXPECTED;


PR_BEGIN_EXTERN_C

/* PUBLIC METHODS */

/*	Constructor
 *    inFolder	- a folder object representing the directory that contains the file
 *    inRelativeFileName  - a relative path and file name
 */
nsInstallDelete::nsInstallDelete(nsSoftwareUpdate* inSoftUpdate, 
                                 nsFolderSpec* inFolder, 
                                 char* inRelativeFileName, 
                                 char* *errorMsg) : nsInstallObject(inSoftUpdate)
{
  registryName = NULL;
  finalFile = NULL;
  deleteStatus = DELETE_FILE;
  FILE_DOES_NOT_EXIST = SUERR_FILE_DOES_NOT_EXIST;
  FILE_READ_ONLY = SUERR_FILE_READ_ONLY;
  FILE_IS_DIRECTORY = SUERR_FILE_IS_DIRECTORY;

  if ((inFolder == NULL) || (inSoftUpdate == NULL)) {
    *errorMsg = SU_GetErrorMsg3("Invalid arguments to the constructor", 
                               SUERR_INVALID_ARGUMENTS);
    return;
  }

  finalFile =	inFolder->MakeFullPath(inRelativeFileName, errorMsg);
  if (*errorMsg != NULL) {
    return;
  }
  processInstallDelete(errorMsg);
}

/*	Constructor
 *    inRegistryName	- name of the component in the registry
 */
nsInstallDelete::nsInstallDelete(nsSoftwareUpdate* inSoftUpdate, 
                                 char* inRegistryName, 
                                 char* *errorMsg) : nsInstallObject(inSoftUpdate)
{
  finalFile = NULL;
  deleteStatus = DELETE_COMPONENT;
  FILE_DOES_NOT_EXIST = SUERR_FILE_DOES_NOT_EXIST;
  FILE_READ_ONLY = SUERR_FILE_READ_ONLY;
  FILE_IS_DIRECTORY = SUERR_FILE_IS_DIRECTORY;

  if ((inRegistryName == NULL) || (inSoftUpdate == NULL)) {
    *errorMsg = SU_GetErrorMsg3("Invalid arguments to the constructor", 
                               SUERR_INVALID_ARGUMENTS);
    return;
  }

  registryName = XP_STRDUP(inRegistryName);
  processInstallDelete(errorMsg);
}

nsInstallDelete::~nsInstallDelete()
{
  XP_FREEIF(finalFile);
  XP_FREEIF(registryName);
}

char* nsInstallDelete::Prepare()
{
  // no set-up necessary
  return NULL;
}

/* Complete
 * Completes the install by deleting the file
 * Security hazard: make sure we request the right permissions
 */
char* nsInstallDelete::Complete()
{
  char* errorMsg = NULL;
  int err = -1;
  nsTarget* execTarget = NULL;

  if (softUpdate == NULL) {
    return SU_GetErrorMsg3("Invalid arguments to the constructor", 
                           SUERR_INVALID_ARGUMENTS);
  }

  nsPrivilegeManager* privMgr = nsPrivilegeManager::getPrivilegeManager();
  nsTarget* impersonation = nsTarget::findTarget(IMPERSONATOR);

  if ((privMgr != NULL) && (impersonation != NULL)) {
    privMgr->enablePrivilege(impersonation, 1);
    execTarget = nsTarget::findTarget(INSTALL_PRIV);
    if (execTarget != NULL) {
      if (!privMgr->enablePrivilege( execTarget, softUpdate->GetPrincipal(), 1 )) {
        return SU_GetErrorMsg3("Permssion was denied", SUERR_ACCESS_DENIED);
      }
    }
  }

  if (deleteStatus == DELETE_COMPONENT) {
    err = VR_Remove( registryName );
  }
  char *msg = NULL;
  if ((deleteStatus == DELETE_FILE) || (err == REGERR_OK)) {
    if (finalFile != NULL) {
      err = NativeComplete();
      if ((err != 0) && (err != SUERR_FILE_DOES_NOT_EXIST))	{
        if (execTarget != NULL) {
          privMgr->revertPrivilege( execTarget, 1 );
        }
        msg = SU_GetString1(SU_ERROR_UNEXPECTED, finalFile);
      }
    }
  }	else {
    msg = SU_GetString1(SU_ERROR_UNEXPECTED, finalFile);
  }
  if (msg != NULL) {
    errorMsg = SU_GetErrorMsg3(msg, err);
    XP_FREE(msg);
  }
  return errorMsg;
}

void nsInstallDelete::Abort()
{
}

char* nsInstallDelete::toString()
{
  if (deleteStatus == DELETE_FILE) {
    return SU_GetString1(SU_DETAILS_DELETE_FILE, finalFile);
  }	else {
    return SU_GetString1(SU_DETAILS_DELETE_COMPONENT, registryName);
  }
}

/* PRIVATE METHODS */
void nsInstallDelete::processInstallDelete(char* *errorMsg)
{
  int err;

  nsTarget* target = NULL;

  /* Request impersonation privileges */
  nsPrivilegeManager* privMgr = nsPrivilegeManager::getPrivilegeManager();
  nsTarget* impersonation = nsTarget::findTarget(IMPERSONATOR);

  if ((privMgr != NULL) && (impersonation != NULL)) {
    privMgr->enablePrivilege(impersonation, 1);
    target = nsTarget::findTarget(INSTALL_PRIV);
    if (target != NULL) {
      /* XXX: we need a way to indicate that a dialog box should appear.*/
      if (!privMgr->enablePrivilege( target, softUpdate->GetPrincipal(), 1 )) {
        *errorMsg = SU_GetErrorMsg3("Permssion was denied", SUERR_ACCESS_DENIED);
        return;
      }
    }
  }
  
  if (deleteStatus == DELETE_COMPONENT) {
    /* Check if the component is in the registry */
    err = VR_InRegistry(registryName);
    if (err != REGERR_OK) {
      char *msg = NULL;
      msg = SU_GetString1(SU_ERROR_NOT_IN_REGISTRY, registryName);
      *errorMsg = SU_GetErrorMsg3(msg, SUERR_NO_SUCH_COMPONENT);
      XP_FREEIF(msg);
      return;
    } 
    else 
    {
      finalFile = (char*)XP_CALLOC(MAXREGPATHLEN, sizeof(char));
      err = VR_GetPath( registryName, MAXREGPATHLEN, finalFile );
      if (err != REGERR_OK)
      {
        XP_FREEIF(finalFile);
        finalFile=NULL;
      }
    }
  }
  
  /* Check if the file exists and is not read only */
  if (finalFile != NULL) {
    err = NativeCheckFileStatus();
    char *msg = NULL;
    if (err == 0) {
      /* System.out.println("File exists and is not read only" + finalFile);*/
    } else if (err == SUERR_FILE_DOES_NOT_EXIST)	{
      msg = SU_GetString1(SUERR_FILE_DOES_NOT_EXIST, finalFile);
    } else if (err == SUERR_FILE_READ_ONLY) {
      msg = SU_GetString1(SU_ERROR_FILE_READ_ONLY, finalFile);
    } else if (err == SUERR_FILE_IS_DIRECTORY) {
      msg = SU_GetString1(SU_ERROR_FILE_IS_DIRECTORY, finalFile);
    } else {
      msg = SU_GetString1(SU_ERROR_UNEXPECTED, finalFile);
    }
    if (msg != NULL) {
      *errorMsg = SU_GetErrorMsg3(msg, err);
      XP_FREE(msg);
    }
  }
}

int nsInstallDelete::NativeComplete()
{
  char * fileName;
  int32 err;
  XP_StatStruct statinfo;
  
  fileName = XP_PlatformFileToURL(finalFile);
  if (fileName != NULL)
	{
      char * temp = XP_STRDUP(&fileName[7]);
      XP_FREE(fileName);
      fileName = temp;
      if (fileName)
		{
          err = XP_Stat(fileName, &statinfo, xpURL);
          if (err != -1)
			{
              if ( XP_STAT_READONLY( statinfo ) )
				{
                  err = FILE_READ_ONLY;
				}
              else if (!S_ISDIR(statinfo.st_mode))
				{
                  err = XP_FileRemove ( fileName, xpURL );
                  if (err != 0)
                    {
#ifdef XP_PC
                      /* REMIND  need to move function to generic XP file */
                      err = SU_REBOOT_NEEDED;
                      su_DeleteOldFileLater( (char*)finalFile );
#endif
					}
                  
				}
              else
                {
                  err = FILE_IS_DIRECTORY;
                }
			}
          else
			{
              err = FILE_DOES_NOT_EXIST;
			}
          
        }
      else
		{
          err = -1;
		}
	}   
  
  XP_FREEIF(fileName);
  return err;
}

int nsInstallDelete::NativeCheckFileStatus()
{
  char * fileName;
  int32 err;
  XP_StatStruct statinfo;
  
  fileName = XP_PlatformFileToURL(finalFile);
  if (fileName != NULL)
	{
      char * temp = XP_STRDUP(&fileName[7]);
      XP_FREE(fileName);
      fileName = temp;
      
      if (fileName)
		{
          err = XP_Stat(fileName, &statinfo, xpURL);
          if (err != -1)
			{
              if ( XP_STAT_READONLY( statinfo ) )
				{
                  err = FILE_READ_ONLY;
				}
              else if (!S_ISDIR(statinfo.st_mode))
                {
                  ;
                }
              else
                {
                  err = FILE_IS_DIRECTORY;
                }
			}
          else
			{
              err = FILE_DOES_NOT_EXIST;
			}
        }
      else
		{
          err = -1;
		}
      
	}
  else
	{
      err = -1;
	}
  XP_FREEIF(fileName);
  return err;
}


PRBool 
nsInstallDelete::CanUninstall()
{
    return FALSE;
}

/* RegisterPackageNode
* InstallDelete() deletes files which no longer need to be registered,
* hence this function returns false.
*/
PRBool
nsInstallDelete::RegisterPackageNode()
{
    return FALSE;
}


PR_END_EXTERN_C
