/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtins.h"

/*
 * builtins/token.c
 *
 * This file implements the NSSCKMDToken object for the
 * "builtin objects" cryptoki module.
 */

static NSSUTF8 *
builtins_mdToken_GetLabel
(
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  CK_RV *pError
)
{
  return (NSSUTF8 *)nss_builtins_TokenLabel;
}

static NSSUTF8 *
builtins_mdToken_GetManufacturerID
(
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  CK_RV *pError
)
{
  return (NSSUTF8 *)nss_builtins_ManufacturerID;
}

static NSSUTF8 *
builtins_mdToken_GetModel
(
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  CK_RV *pError
)
{
  return (NSSUTF8 *)nss_builtins_TokenModel;
}

static NSSUTF8 *
builtins_mdToken_GetSerialNumber
(
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  CK_RV *pError
)
{
  return (NSSUTF8 *)nss_builtins_TokenSerialNumber;
}

static CK_BBOOL
builtins_mdToken_GetIsWriteProtected
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
builtins_mdToken_GetHardwareVersion
(
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance
)
{
  return nss_builtins_HardwareVersion;
}

static CK_VERSION
builtins_mdToken_GetFirmwareVersion
(
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance
)
{
  return nss_builtins_FirmwareVersion;
}

static NSSCKMDSession *
builtins_mdToken_OpenSession
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
  return nss_builtins_CreateSession(fwSession, pError);
}

const NSSCKMDToken
nss_builtins_mdToken = {
  (void *)NULL, /* etc */
  NULL, /* Setup */
  NULL, /* Invalidate */
  NULL, /* InitToken -- default errs */
  builtins_mdToken_GetLabel,
  builtins_mdToken_GetManufacturerID,
  builtins_mdToken_GetModel,
  builtins_mdToken_GetSerialNumber,
  NULL, /* GetHasRNG -- default is false */
  builtins_mdToken_GetIsWriteProtected,
  NULL, /* GetLoginRequired -- default is false */
  NULL, /* GetUserPinInitialized -- default is false */
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
  builtins_mdToken_GetHardwareVersion,
  builtins_mdToken_GetFirmwareVersion,
  NULL, /* GetUTCTime -- no clock */
  builtins_mdToken_OpenSession,
  NULL, /* GetMechanismCount -- default is zero */
  NULL, /* GetMechanismTypes -- irrelevant */
  NULL, /* GetMechanism -- irrelevant */
  (void *)NULL /* null terminator */
};
