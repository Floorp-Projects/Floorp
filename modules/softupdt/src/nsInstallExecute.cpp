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
