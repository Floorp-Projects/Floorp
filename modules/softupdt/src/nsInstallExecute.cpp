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
#include "su_instl.h"

#include "nsInstallExecute.h"
#include "nsSUError.h"

#include "nsPrivilegeManager.h"
#include "nsTarget.h"

extern int SU_ERROR_UNEXPECTED;
extern int SU_DETAILS_EXECUTE_PROGRESS;
extern int SU_DETAILS_EXECUTE_PROGRESS2;

PR_BEGIN_EXTERN_C

/* Public Methods */

/*	Constructor
 *	inJarLocation	- location inside the JAR file
 *	inZigPtr        - pointer to the ZIG *
 */
nsInstallExecute::nsInstallExecute(nsSoftwareUpdate* inSoftUpdate, 
                                   char* inJarLocation, 
                                   char* *errorMsg, 
                                   char* inArgs) : nsInstallObject(inSoftUpdate)
{
  jarLocation = NULL;
  tempFile = NULL;
  args = NULL;
  cmdline = NULL;

  if ((inArgs == NULL) || (inJarLocation == NULL) || 
      (inSoftUpdate == NULL)) {
    *errorMsg = SU_GetErrorMsg3("Invalid arguments to the constructor", 
                               SUERR_INVALID_ARGUMENTS);
    return;
  }

  /* Request impersonation privileges */
  nsTarget* target = NULL;

  /* Request impersonation privileges */
  nsPrivilegeManager* privMgr = nsPrivilegeManager::getPrivilegeManager();
  nsTarget* impersonation = nsTarget::findTarget(IMPERSONATOR);

  if ((privMgr != NULL) && (impersonation != NULL)) {
    privMgr->enablePrivilege(impersonation, 1);

    /* check the security permissions */
    target = nsTarget::findTarget(INSTALL_PRIV);
    if (target != NULL) {
      /* XXX: we need a way to indicate that a dialog box should appear.*/
      if (!privMgr->enablePrivilege( target, softUpdate->GetPrincipal(), 1 )) {
        *errorMsg = SU_GetErrorMsg3("Permssion was denied", SUERR_ACCESS_DENIED);
        return;
      }
    }
  }
  jarLocation = XP_STRDUP(inJarLocation);
  args = XP_STRDUP(inArgs);
}


nsInstallExecute::~nsInstallExecute()
{
  XP_FREEIF(jarLocation);
  XP_FREEIF(args);
  XP_FREEIF(cmdline);
  XP_FREEIF(tempFile);
}

/* Prepare
 * Extracts	file out of	the	JAR	archive	into the temp directory
 */
char* nsInstallExecute::Prepare(void)
{
  char *errorMsg = NULL;
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

  tempFile = softUpdate->ExtractJARFile( jarLocation, NULL, &errorMsg );
  if (errorMsg != NULL) {
    PR_ASSERT(tempFile == NULL);
    return errorMsg;
  }

  if (tempFile == NULL) {
    return SU_GetErrorMsg3("Extraction of JAR file failed", SUERR_ACCESS_DENIED);
  }
  
#ifdef XP_MAC
    cmdline = XP_STRDUP(tempFile);
#else
  if ( args == NULL )
    cmdline = XP_STRDUP(tempFile);
  else
    cmdline = XP_Cat(tempFile, " ", args);
#endif /* XP_MAC */
  XP_FREE(tempFile);
  return NULL;
}

/* Complete
 * Completes the install by executing the file
 * Security hazard: make sure we request the right permissions
 */
char* nsInstallExecute::Complete(void)
{
  char* errorMsg = NULL;
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

  errorMsg = NativeComplete();
  if (execTarget != NULL) {
    privMgr->revertPrivilege( execTarget, 1 );
  }
  // 
  // System.out.println("File executed: " + tempFile);
  return errorMsg;
}

void nsInstallExecute::Abort(void)
{
  NativeAbort();
}

char* nsInstallExecute::toString()
{
  char *ret_val;
  char *msg = XP_Cat(jarLocation, tempFile);
  if (tempFile == NULL) {
    ret_val = SU_GetString1(SU_DETAILS_EXECUTE_PROGRESS, msg);
  } else {
    ret_val = SU_GetString1(SU_DETAILS_EXECUTE_PROGRESS2, msg);
  }
  XP_FREE(msg);
  return ret_val;
}

/* Private Methods */
char* nsInstallExecute::NativeComplete(void)
{
  char* errorMsg = NULL;
  int32 err;

  err = FE_ExecuteFile( tempFile, cmdline );
  XP_ASSERT( err == 0 );
  if ( err != 0 ) {
    errorMsg = SU_GetErrorMsg4(SU_ERROR_UNEXPECTED, err);
  }
  return errorMsg;
}

void nsInstallExecute::NativeAbort(void)
{
  char * currentName;
  int result;

  if (tempFile == NULL)
    return;

  /* Get the names */
  currentName = (char*)tempFile;

  result = XP_FileRemove(currentName, xpURL);
  XP_ASSERT(result == 0);
}

/* CanUninstall
* InstallExecute() installs files which cannot be uninstalled,
* since they are executed, hence this function returns false. 
*/
PRBool 
nsInstallExecute::CanUninstall()
{
    return FALSE;
}

/* RegisterPackageNode
* InstallExecute() installs files which need to be registered,
* hence this function returns true.
*/
PRBool
nsInstallExecute::RegisterPackageNode()
{
    return TRUE;
}


PR_END_EXTERN_C
