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

#include "nsUninstallObject.h"
#include "prmem.h"
#include "prmon.h"
#include "prlog.h"
#include "prprf.h"
#include "xp.h"
#include "NSReg.h"
#include "VerReg.h"
#include "softupdt.h"
#include "nsPrivilegeManager.h"
#include "nsTarget.h"
#include "nsSUError.h"

extern int SU_DETAILS_UNINSTALL;

PR_BEGIN_EXTERN_C

/* PUBLIC METHODS */

nsUninstallObject::nsUninstallObject(nsSoftwareUpdate* inSoftUpdate, 
                                     char* inRegName, 
                                     char* *errorMsg) : nsInstallObject(inSoftUpdate)
{
  int err;

  regName = NULL;
  userName = NULL;
  if ( (inRegName == NULL) || (XP_STRLEN(inRegName) == 0) ) {
    *errorMsg = SU_GetErrorMsg3("RegName is NULL ", SUERR_INVALID_ARGUMENTS );
    return;
  }

  if (inSoftUpdate == NULL) {
    *errorMsg = SU_GetErrorMsg3("SoftwareUpdate object is NULL ", 
                                SUERR_INVALID_ARGUMENTS);
    return;
  }

  regName = XP_STRDUP(inRegName);

  /* Request impersonation privileges */
  nsPrivilegeManager* privMgr = nsPrivilegeManager::getPrivilegeManager();
  nsTarget* impersonation = nsTarget::findTarget(IMPERSONATOR);
  nsTarget* target = NULL;

  if ((privMgr != NULL) && (impersonation != NULL)) {
    /* XXX: We should get the SystemPrincipal and enablePrivilege on that. 
     * Or may be we should get rid of impersonation
     */
    privMgr->enablePrivilege(impersonation, 1);
    target = nsTarget::findTarget(INSTALL_PRIV);
    if (target != NULL) {
      if (!privMgr->enablePrivilege( target, softUpdate->GetPrincipal(), 1 )) {
        *errorMsg = SU_GetErrorMsg3("Permssion was denied", SUERR_ACCESS_DENIED);
        return;
      }
    }
  }

  userName = (char*)XP_CALLOC(MAXREGPATHLEN, sizeof(char));
  err = VR_GetUninstallUserName( regName, userName, MAXREGPATHLEN );
  if (err != REGERR_OK)
  {
     XP_FREEIF(userName);
     userName = NULL;
  }

  if ( userName == NULL ) 
  {
    char *msg = NULL;
    msg = PR_sprintf_append(msg, "No such component %s", regName);
    *errorMsg = SU_GetErrorMsg3(msg, SUERR_NO_SUCH_COMPONENT);
    PR_FREEIF(msg);
    return;
  }
}


nsUninstallObject::~nsUninstallObject()
{
  XP_FREEIF(regName);
  XP_FREEIF(userName);
}

  
/* Complete
 * Uninstalls the package
 */
char* nsUninstallObject::Complete()
{
  nsTarget* execTarget = NULL;

  if ((softUpdate == NULL) || (regName == NULL)) {
    return SU_GetErrorMsg3("Invalid arguments to UninstallObject ", 
                                SUERR_INVALID_ARGUMENTS);
  }

  nsPrivilegeManager* privMgr = nsPrivilegeManager::getPrivilegeManager();
  nsTarget* impersonation = nsTarget::findTarget(IMPERSONATOR);

  if ((privMgr != NULL) && (impersonation != NULL)) {
    /* XXX: We should get the SystemPrincipal and enablePrivilege on that. 
     * Or may be we should get rid of impersonation
     */
    privMgr->enablePrivilege(impersonation, 1);
    execTarget = nsTarget::findTarget(INSTALL_PRIV);
    if (execTarget != NULL) {
      if (!privMgr->enablePrivilege( execTarget, softUpdate->GetPrincipal(), 1 )) {
        return SU_GetErrorMsg3("Permssion was denied", SUERR_ACCESS_DENIED);
      }
    }
  }

  char *errorMsg = NativeComplete( regName );

  if (execTarget != NULL) {
    privMgr->revertPrivilege( execTarget, 1 );
  }
  return errorMsg;
}
 
 
char* nsUninstallObject::Prepare()
{
  return NULL;
}
  
void nsUninstallObject::Abort()
{
}
  
char* nsUninstallObject::toString()
{
  return SU_GetString1(SU_DETAILS_UNINSTALL, userName); // Needs I10n
}

  
/* PRIVATE METHODS */

char* nsUninstallObject::NativeComplete(char* regname)
{
  char* errorMsg = NULL;
  if ( regname != NULL && *regname != '\0' ) {
    int err = SU_Uninstall( regName );
    if (err != REGERR_OK) {
      errorMsg = SU_GetErrorMsg3("Uninstall failed", err);
    }
  }
  return errorMsg;
}
/* CanUninstall
* UninstallObject() uninstall files, 
* hence this function returns false. 
*/
PRBool nsUninstallObject::CanUninstall()
{
    return PR_FALSE;
}

/* RegisterPackageNode
* UninstallObject() uninstalls files which no longer need to be registered,
* hence this function returns false.
*/
PRBool nsUninstallObject::RegisterPackageNode()
{
    return PR_FALSE;
}

PR_END_EXTERN_C
