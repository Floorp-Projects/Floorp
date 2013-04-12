/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef DEBUG
static const char CVS_ID[] = "@(#) $RCSfile$ $Revision$ $Date$";
#endif /* DEBUG */

#include "ckmk.h"

/*
 * nssmkey/mslot.c
 *
 * This file implements the NSSCKMDSlot object for the
 * "nssmkey" cryptoki module.
 */

static NSSUTF8 *
ckmk_mdSlot_GetSlotDescription
(
  NSSCKMDSlot *mdSlot,
  NSSCKFWSlot *fwSlot,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  CK_RV *pError
)
{
  return (NSSUTF8 *)nss_ckmk_SlotDescription;
}

static NSSUTF8 *
ckmk_mdSlot_GetManufacturerID
(
  NSSCKMDSlot *mdSlot,
  NSSCKFWSlot *fwSlot,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  CK_RV *pError
)
{
  return (NSSUTF8 *)nss_ckmk_ManufacturerID;
}

static CK_VERSION
ckmk_mdSlot_GetHardwareVersion
(
  NSSCKMDSlot *mdSlot,
  NSSCKFWSlot *fwSlot,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance
)
{
  return nss_ckmk_HardwareVersion;
}

static CK_VERSION
ckmk_mdSlot_GetFirmwareVersion
(
  NSSCKMDSlot *mdSlot,
  NSSCKFWSlot *fwSlot,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance
)
{
  return nss_ckmk_FirmwareVersion;
}

static NSSCKMDToken *
ckmk_mdSlot_GetToken
(
  NSSCKMDSlot *mdSlot,
  NSSCKFWSlot *fwSlot,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  CK_RV *pError
)
{
  return (NSSCKMDToken *)&nss_ckmk_mdToken;
}

NSS_IMPLEMENT_DATA const NSSCKMDSlot
nss_ckmk_mdSlot = {
  (void *)NULL, /* etc */
  NULL, /* Initialize */
  NULL, /* Destroy */
  ckmk_mdSlot_GetSlotDescription,
  ckmk_mdSlot_GetManufacturerID,
  NULL, /* GetTokenPresent -- defaults to true */
  NULL, /* GetRemovableDevice -- defaults to false */
  NULL, /* GetHardwareSlot -- defaults to false */
  ckmk_mdSlot_GetHardwareVersion,
  ckmk_mdSlot_GetFirmwareVersion,
  ckmk_mdSlot_GetToken,
  (void *)NULL /* null terminator */
};
