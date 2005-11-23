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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
 * the Initial Developer. All Rights Reserved.
 * Portions created by Red Hat, Inc, are Copyright (C) 2005
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
static const char CVS_ID[] = "@(#) $RCSfile: mtoken.c,v $ $Revision: 1.1 $ $Date: 2005/11/23 23:04:08 $";
#endif /* DEBUG */

#include "ckmk.h"

/*
 * nssmkey/mtoken.c
 *
 * This file implements the NSSCKMDToken object for the
 * "nssmkey" cryptoki module.
 */

static NSSUTF8 *
ckmk_mdToken_GetLabel
(
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  CK_RV *pError
)
{
  return (NSSUTF8 *)nss_ckmk_TokenLabel;
}

static NSSUTF8 *
ckmk_mdToken_GetManufacturerID
(
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  CK_RV *pError
)
{
  return (NSSUTF8 *)nss_ckmk_ManufacturerID;
}

static NSSUTF8 *
ckmk_mdToken_GetModel
(
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  CK_RV *pError
)
{
  return (NSSUTF8 *)nss_ckmk_TokenModel;
}

static NSSUTF8 *
ckmk_mdToken_GetSerialNumber
(
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  CK_RV *pError
)
{
  return (NSSUTF8 *)nss_ckmk_TokenSerialNumber;
}

static CK_BBOOL
ckmk_mdToken_GetIsWriteProtected
(
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance
)
{
  return CK_FALSE;
}

/* fake out Mozilla so we don't try to initialize the token */
static CK_BBOOL
ckmk_mdToken_GetUserPinInitialized
(
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance
)
{
  return CK_TRUE;
}

static CK_VERSION
ckmk_mdToken_GetHardwareVersion
(
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance
)
{
  return nss_ckmk_HardwareVersion;
}

static CK_VERSION
ckmk_mdToken_GetFirmwareVersion
(
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance
)
{
  return nss_ckmk_FirmwareVersion;
}

static NSSCKMDSession *
ckmk_mdToken_OpenSession
(
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  NSSCKFWSession *fwSession,
  CK_BBOOL rw,
  CK_RV *pError
)
{
  return nss_ckmk_CreateSession(fwSession, pError);
}

static CK_ULONG
ckmk_mdToken_GetMechanismCount
(
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance
)
{
  return (CK_ULONG)1;
}

static CK_RV
ckmk_mdToken_GetMechanismTypes
(
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  CK_MECHANISM_TYPE types[]
)
{
  types[0] = CKM_RSA_PKCS;
  return CKR_OK;
}

static NSSCKMDMechanism *
ckmk_mdToken_GetMechanism
(
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  CK_MECHANISM_TYPE which,
  CK_RV *pError
)
{
  if (which != CKM_RSA_PKCS) {
    *pError = CKR_MECHANISM_INVALID;
    return (NSSCKMDMechanism *)NULL;
  }
  return (NSSCKMDMechanism *)&nss_ckmk_mdMechanismRSA;
}

NSS_IMPLEMENT_DATA const NSSCKMDToken
nss_ckmk_mdToken = {
  (void *)NULL, /* etc */
  NULL, /* Setup */
  NULL, /* Invalidate */
  NULL, /* InitToken -- default errs */
  ckmk_mdToken_GetLabel,
  ckmk_mdToken_GetManufacturerID,
  ckmk_mdToken_GetModel,
  ckmk_mdToken_GetSerialNumber,
  NULL, /* GetHasRNG -- default is false */
  ckmk_mdToken_GetIsWriteProtected,
  NULL, /* GetLoginRequired -- default is false */
  ckmk_mdToken_GetUserPinInitialized,
  NULL, /* GetRestoreKeyNotNeeded -- irrelevant */
  NULL, /* GetHasClockOnToken -- default is false */
  NULL, /* GetHasProtectedAuthenticationPath -- default is false */
  NULL, /* GetSupportsDualCryptoOperations -- default is false */
  NULL, /* GetMaxSessionCount -- default is CK_UNAVAILABLE_INFORMATION */
  NULL, /* GetMaxRwSessionCount -- default is CK_UNAVAILABLE_INFORMATION */
  NULL, /* GetMaxPinLen -- irrelevant */
  NULL, /* GetMinPinLen -- irrelevant */
  NULL, /* GetTotalPublicMemory -- default is CK_UNAVAILABLE_INFORMATION */
  NULL, /* GetFreePublicMemory -- default is CK_UNAVAILABLE_INFORMATION */
  NULL, /* GetTotalPrivateMemory -- default is CK_UNAVAILABLE_INFORMATION */
  NULL, /* GetFreePrivateMemory -- default is CK_UNAVAILABLE_INFORMATION */
  ckmk_mdToken_GetHardwareVersion,
  ckmk_mdToken_GetFirmwareVersion,
  NULL, /* GetUTCTime -- no clock */
  ckmk_mdToken_OpenSession,
  ckmk_mdToken_GetMechanismCount,
  ckmk_mdToken_GetMechanismTypes,
  ckmk_mdToken_GetMechanism,
  (void *)NULL /* null terminator */
};
