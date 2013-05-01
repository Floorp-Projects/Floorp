/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef DEBUG
static const char CVS_ID[] = "@(#) $RCSfile$ $Revision$ $Date$";
#endif /* DEBUG */

#include "builtins.h"

/*
 * builtins/slot.c
 *
 * This file implements the NSSCKMDSlot object for the
 * "builtin objects" cryptoki module.
 */

static NSSUTF8 *
builtins_mdSlot_GetSlotDescription
(
  NSSCKMDSlot *mdSlot,
  NSSCKFWSlot *fwSlot,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  CK_RV *pError
)
{
  return (NSSUTF8 *)nss_builtins_SlotDescription;
}

static NSSUTF8 *
builtins_mdSlot_GetManufacturerID
(
  NSSCKMDSlot *mdSlot,
  NSSCKFWSlot *fwSlot,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  CK_RV *pError
)
{
  return (NSSUTF8 *)nss_builtins_ManufacturerID;
}

static CK_VERSION
builtins_mdSlot_GetHardwareVersion
(
  NSSCKMDSlot *mdSlot,
  NSSCKFWSlot *fwSlot,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance
)
{
  return nss_builtins_HardwareVersion;
}

static CK_VERSION
builtins_mdSlot_GetFirmwareVersion
(
  NSSCKMDSlot *mdSlot,
  NSSCKFWSlot *fwSlot,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance
)
{
  return nss_builtins_FirmwareVersion;
}

static NSSCKMDToken *
builtins_mdSlot_GetToken
(
  NSSCKMDSlot *mdSlot,
  NSSCKFWSlot *fwSlot,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  CK_RV *pError
)
{
  return (NSSCKMDToken *)&nss_builtins_mdToken;
}

const NSSCKMDSlot
nss_builtins_mdSlot = {
  (void *)NULL, /* etc */
  NULL, /* Initialize */
  NULL, /* Destroy */
  builtins_mdSlot_GetSlotDescription,
  builtins_mdSlot_GetManufacturerID,
  NULL, /* GetTokenPresent -- defaults to true */
  NULL, /* GetRemovableDevice -- defaults to false */
  NULL, /* GetHardwareSlot -- defaults to false */
  builtins_mdSlot_GetHardwareVersion,
  builtins_mdSlot_GetFirmwareVersion,
  builtins_mdSlot_GetToken,
  (void *)NULL /* null terminator */
};
