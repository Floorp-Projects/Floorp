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
