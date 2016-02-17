/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
