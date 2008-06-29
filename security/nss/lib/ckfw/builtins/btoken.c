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

#ifdef DEBUG
static const char CVS_ID[] = "@(#) $RCSfile: btoken.c,v $ $Revision: 1.4 $ $Date: 2008/01/23 07:34:49 $";
#endif /* DEBUG */

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
