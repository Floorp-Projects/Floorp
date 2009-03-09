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
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Red Hat, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bob Relyea (rrelyea@redhat.com)
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

#ifdef DEBUG
static const char CVS_ID[] = "@(#) $RCSfile: crypto.c,v $ $Revision: 1.4 $ $Date: 2009/02/09 07:55:51 $";
#endif /* DEBUG */

/*
 * crypto.c
 *
 * This file implements the NSSCKFWCryptoOperation type and methods.
 */

#ifndef CK_T
#include "ck.h"
#endif /* CK_T */

/*
 * NSSCKFWCryptoOperation
 *
 *  -- create/destroy --
 *  nssCKFWCrytoOperation_Create
 *  nssCKFWCryptoOperation_Destroy
 *
 *  -- implement public accessors --
 *  nssCKFWCryptoOperation_GetMDCryptoOperation
 *  nssCKFWCryptoOperation_GetType
 *
 *  -- private accessors --
 *
 *  -- module fronts --
 * nssCKFWCryptoOperation_GetFinalLength
 * nssCKFWCryptoOperation_GetOperationLength
 * nssCKFWCryptoOperation_Final
 * nssCKFWCryptoOperation_Update
 * nssCKFWCryptoOperation_DigestUpdate
 * nssCKFWCryptoOperation_UpdateFinal
 */

struct NSSCKFWCryptoOperationStr {
  /* NSSArena *arena; */
  NSSCKMDCryptoOperation *mdOperation;
  NSSCKMDSession *mdSession;
  NSSCKFWSession *fwSession;
  NSSCKMDToken *mdToken;
  NSSCKFWToken *fwToken;
  NSSCKMDInstance *mdInstance;
  NSSCKFWInstance *fwInstance;
  NSSCKFWCryptoOperationType type;
};

/*
 *  nssCKFWCrytoOperation_Create
 */
NSS_EXTERN NSSCKFWCryptoOperation *
nssCKFWCryptoOperation_Create(
  NSSCKMDCryptoOperation *mdOperation,
  NSSCKMDSession *mdSession,
  NSSCKFWSession *fwSession,
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  NSSCKFWCryptoOperationType type,
  CK_RV *pError
)
{
  NSSCKFWCryptoOperation *fwOperation;
  fwOperation = nss_ZNEW(NULL, NSSCKFWCryptoOperation);
  if (!fwOperation) {
    *pError = CKR_HOST_MEMORY;
    return (NSSCKFWCryptoOperation *)NULL;
  }
  fwOperation->mdOperation = mdOperation; 
  fwOperation->mdSession = mdSession; 
  fwOperation->fwSession = fwSession; 
  fwOperation->mdToken = mdToken; 
  fwOperation->fwToken = fwToken; 
  fwOperation->mdInstance = mdInstance; 
  fwOperation->fwInstance = fwInstance; 
  fwOperation->type = type; 
  return fwOperation;
}

/*
 *  nssCKFWCryptoOperation_Destroy
 */
NSS_EXTERN void
nssCKFWCryptoOperation_Destroy
(
  NSSCKFWCryptoOperation *fwOperation
)
{
  if ((NSSCKMDCryptoOperation *) NULL != fwOperation->mdOperation) {
    if (fwOperation->mdOperation->Destroy) {
      fwOperation->mdOperation->Destroy(
                                fwOperation->mdOperation,
                                fwOperation,
                                fwOperation->mdInstance,
                                fwOperation->fwInstance);
    }
  }
  nss_ZFreeIf(fwOperation);
}

/*
 *  nssCKFWCryptoOperation_GetMDCryptoOperation
 */
NSS_EXTERN NSSCKMDCryptoOperation *
nssCKFWCryptoOperation_GetMDCryptoOperation
(
  NSSCKFWCryptoOperation *fwOperation
)
{
  return fwOperation->mdOperation;
}

/*
 *  nssCKFWCryptoOperation_GetType
 */
NSS_EXTERN NSSCKFWCryptoOperationType
nssCKFWCryptoOperation_GetType
(
  NSSCKFWCryptoOperation *fwOperation
)
{
  return fwOperation->type;
}

/*
 * nssCKFWCryptoOperation_GetFinalLength
 */
NSS_EXTERN CK_ULONG
nssCKFWCryptoOperation_GetFinalLength
(
  NSSCKFWCryptoOperation *fwOperation,
  CK_RV *pError
)
{
  if (!fwOperation->mdOperation->GetFinalLength) {
    *pError = CKR_FUNCTION_FAILED;
    return 0;
  }
  return fwOperation->mdOperation->GetFinalLength(
                fwOperation->mdOperation,
                fwOperation,
                fwOperation->mdSession,
                fwOperation->fwSession,
                fwOperation->mdToken,
                fwOperation->fwToken,
                fwOperation->mdInstance,
                fwOperation->fwInstance,
                pError);
}

/*
 * nssCKFWCryptoOperation_GetOperationLength
 */
NSS_EXTERN CK_ULONG
nssCKFWCryptoOperation_GetOperationLength
(
  NSSCKFWCryptoOperation *fwOperation,
  NSSItem *inputBuffer,
  CK_RV *pError
)
{
  if (!fwOperation->mdOperation->GetOperationLength) {
    *pError = CKR_FUNCTION_FAILED;
    return 0;
  }
  return fwOperation->mdOperation->GetOperationLength(
                fwOperation->mdOperation,
                fwOperation,
                fwOperation->mdSession,
                fwOperation->fwSession,
                fwOperation->mdToken,
                fwOperation->fwToken,
                fwOperation->mdInstance,
                fwOperation->fwInstance,
                inputBuffer,
                pError);
}

/*
 * nssCKFWCryptoOperation_Final
 */
NSS_EXTERN CK_RV
nssCKFWCryptoOperation_Final
(
  NSSCKFWCryptoOperation *fwOperation,
  NSSItem *outputBuffer
)
{
  if (!fwOperation->mdOperation->Final) {
    return CKR_FUNCTION_FAILED;
  }
  return fwOperation->mdOperation->Final(
                fwOperation->mdOperation,
                fwOperation,
                fwOperation->mdSession,
                fwOperation->fwSession,
                fwOperation->mdToken,
                fwOperation->fwToken,
                fwOperation->mdInstance,
                fwOperation->fwInstance,
                outputBuffer);
}

/*
 * nssCKFWCryptoOperation_Update
 */
NSS_EXTERN CK_RV
nssCKFWCryptoOperation_Update
(
  NSSCKFWCryptoOperation *fwOperation,
  NSSItem *inputBuffer,
  NSSItem *outputBuffer
)
{
  if (!fwOperation->mdOperation->Update) {
    return CKR_FUNCTION_FAILED;
  }
  return fwOperation->mdOperation->Update(
                fwOperation->mdOperation,
                fwOperation,
                fwOperation->mdSession,
                fwOperation->fwSession,
                fwOperation->mdToken,
                fwOperation->fwToken,
                fwOperation->mdInstance,
                fwOperation->fwInstance,
                inputBuffer,
                outputBuffer);
}

/*
 * nssCKFWCryptoOperation_DigestUpdate
 */
NSS_EXTERN CK_RV
nssCKFWCryptoOperation_DigestUpdate
(
  NSSCKFWCryptoOperation *fwOperation,
  NSSItem *inputBuffer
)
{
  if (!fwOperation->mdOperation->DigestUpdate) {
    return CKR_FUNCTION_FAILED;
  }
  return fwOperation->mdOperation->DigestUpdate(
                fwOperation->mdOperation,
                fwOperation,
                fwOperation->mdSession,
                fwOperation->fwSession,
                fwOperation->mdToken,
                fwOperation->fwToken,
                fwOperation->mdInstance,
                fwOperation->fwInstance,
                inputBuffer);
}

/*
 * nssCKFWCryptoOperation_DigestKey
 */
NSS_EXTERN CK_RV
nssCKFWCryptoOperation_DigestKey
(
  NSSCKFWCryptoOperation *fwOperation,
  NSSCKFWObject *fwObject /* Key */
)
{
  NSSCKMDObject *mdObject;

  if (!fwOperation->mdOperation->DigestKey) {
    return CKR_FUNCTION_FAILED;
  }
  mdObject = nssCKFWObject_GetMDObject(fwObject);
  return fwOperation->mdOperation->DigestKey(
                fwOperation->mdOperation,
                fwOperation,
                fwOperation->mdToken,
                fwOperation->fwToken,
                fwOperation->mdInstance,
                fwOperation->fwInstance,
                mdObject,
                fwObject);
}

/*
 * nssCKFWCryptoOperation_UpdateFinal
 */
NSS_EXTERN CK_RV
nssCKFWCryptoOperation_UpdateFinal
(
  NSSCKFWCryptoOperation *fwOperation,
  NSSItem *inputBuffer,
  NSSItem *outputBuffer
)
{
  if (!fwOperation->mdOperation->UpdateFinal) {
    return CKR_FUNCTION_FAILED;
  }
  return fwOperation->mdOperation->UpdateFinal(
                fwOperation->mdOperation,
                fwOperation,
                fwOperation->mdSession,
                fwOperation->fwSession,
                fwOperation->mdToken,
                fwOperation->fwToken,
                fwOperation->mdInstance,
                fwOperation->fwInstance,
                inputBuffer,
                outputBuffer);
}

/*
 * nssCKFWCryptoOperation_UpdateCombo
 */
NSS_EXTERN CK_RV
nssCKFWCryptoOperation_UpdateCombo
(
  NSSCKFWCryptoOperation *fwOperation,
  NSSCKFWCryptoOperation *fwPeerOperation,
  NSSItem *inputBuffer,
  NSSItem *outputBuffer
)
{
  if (!fwOperation->mdOperation->UpdateCombo) {
    return CKR_FUNCTION_FAILED;
  }
  return fwOperation->mdOperation->UpdateCombo(
                fwOperation->mdOperation,
                fwOperation,
                fwPeerOperation->mdOperation,
                fwPeerOperation,
                fwOperation->mdSession,
                fwOperation->fwSession,
                fwOperation->mdToken,
                fwOperation->fwToken,
                fwOperation->mdInstance,
                fwOperation->fwInstance,
                inputBuffer,
                outputBuffer);
}
