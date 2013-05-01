/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef DEBUG
static const char CVS_ID[] = "@(#) $RCSfile$ $Revision$ $Date$";
#endif /* DEBUG */

#include "ckcapi.h"

/*
 * ckcapi/ctoken.c
 *
 * This file implements the NSSCKMDToken object for the
 * "nss to capi" cryptoki module.
 */

static NSSUTF8 *
ckcapi_mdToken_GetLabel
(
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  CK_RV *pError
)
{
  return (NSSUTF8 *)nss_ckcapi_TokenLabel;
}

static NSSUTF8 *
ckcapi_mdToken_GetManufacturerID
(
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  CK_RV *pError
)
{
  return (NSSUTF8 *)nss_ckcapi_ManufacturerID;
}

static NSSUTF8 *
ckcapi_mdToken_GetModel
(
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  CK_RV *pError
)
{
  return (NSSUTF8 *)nss_ckcapi_TokenModel;
}

static NSSUTF8 *
ckcapi_mdToken_GetSerialNumber
(
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  CK_RV *pError
)
{
  return (NSSUTF8 *)nss_ckcapi_TokenSerialNumber;
}

static CK_BBOOL
ckcapi_mdToken_GetIsWriteProtected
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
ckcapi_mdToken_GetUserPinInitialized
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
ckcapi_mdToken_GetHardwareVersion
(
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance
)
{
  return nss_ckcapi_HardwareVersion;
}

static CK_VERSION
ckcapi_mdToken_GetFirmwareVersion
(
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance
)
{
  return nss_ckcapi_FirmwareVersion;
}

static NSSCKMDSession *
ckcapi_mdToken_OpenSession
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
  return nss_ckcapi_CreateSession(fwSession, pError);
}

static CK_ULONG
ckcapi_mdToken_GetMechanismCount
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
ckcapi_mdToken_GetMechanismTypes
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
ckcapi_mdToken_GetMechanism
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
  return (NSSCKMDMechanism *)&nss_ckcapi_mdMechanismRSA;
}

NSS_IMPLEMENT_DATA const NSSCKMDToken
nss_ckcapi_mdToken = {
  (void *)NULL, /* etc */
  NULL, /* Setup */
  NULL, /* Invalidate */
  NULL, /* InitToken -- default errs */
  ckcapi_mdToken_GetLabel,
  ckcapi_mdToken_GetManufacturerID,
  ckcapi_mdToken_GetModel,
  ckcapi_mdToken_GetSerialNumber,
  NULL, /* GetHasRNG -- default is false */
  ckcapi_mdToken_GetIsWriteProtected,
  NULL, /* GetLoginRequired -- default is false */
  ckcapi_mdToken_GetUserPinInitialized,
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
  ckcapi_mdToken_GetHardwareVersion,
  ckcapi_mdToken_GetFirmwareVersion,
  NULL, /* GetUTCTime -- no clock */
  ckcapi_mdToken_OpenSession,
  ckcapi_mdToken_GetMechanismCount,
  ckcapi_mdToken_GetMechanismTypes,
  ckcapi_mdToken_GetMechanism,
  (void *)NULL /* null terminator */
};
