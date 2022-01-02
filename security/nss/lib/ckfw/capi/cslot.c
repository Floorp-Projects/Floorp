/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ckcapi.h"

/*
 * ckcapi/cslot.c
 *
 * This file implements the NSSCKMDSlot object for the
 * "nss to capi" cryptoki module.
 */

static NSSUTF8 *
ckcapi_mdSlot_GetSlotDescription(
    NSSCKMDSlot *mdSlot,
    NSSCKFWSlot *fwSlot,
    NSSCKMDInstance *mdInstance,
    NSSCKFWInstance *fwInstance,
    CK_RV *pError)
{
    return (NSSUTF8 *)nss_ckcapi_SlotDescription;
}

static NSSUTF8 *
ckcapi_mdSlot_GetManufacturerID(
    NSSCKMDSlot *mdSlot,
    NSSCKFWSlot *fwSlot,
    NSSCKMDInstance *mdInstance,
    NSSCKFWInstance *fwInstance,
    CK_RV *pError)
{
    return (NSSUTF8 *)nss_ckcapi_ManufacturerID;
}

static CK_VERSION
ckcapi_mdSlot_GetHardwareVersion(
    NSSCKMDSlot *mdSlot,
    NSSCKFWSlot *fwSlot,
    NSSCKMDInstance *mdInstance,
    NSSCKFWInstance *fwInstance)
{
    return nss_ckcapi_HardwareVersion;
}

static CK_VERSION
ckcapi_mdSlot_GetFirmwareVersion(
    NSSCKMDSlot *mdSlot,
    NSSCKFWSlot *fwSlot,
    NSSCKMDInstance *mdInstance,
    NSSCKFWInstance *fwInstance)
{
    return nss_ckcapi_FirmwareVersion;
}

static NSSCKMDToken *
ckcapi_mdSlot_GetToken(
    NSSCKMDSlot *mdSlot,
    NSSCKFWSlot *fwSlot,
    NSSCKMDInstance *mdInstance,
    NSSCKFWInstance *fwInstance,
    CK_RV *pError)
{
    return (NSSCKMDToken *)&nss_ckcapi_mdToken;
}

NSS_IMPLEMENT_DATA const NSSCKMDSlot
    nss_ckcapi_mdSlot = {
        (void *)NULL, /* etc */
        NULL,         /* Initialize */
        NULL,         /* Destroy */
        ckcapi_mdSlot_GetSlotDescription,
        ckcapi_mdSlot_GetManufacturerID,
        NULL, /* GetTokenPresent -- defaults to true */
        NULL, /* GetRemovableDevice -- defaults to false */
        NULL, /* GetHardwareSlot -- defaults to false */
        ckcapi_mdSlot_GetHardwareVersion,
        ckcapi_mdSlot_GetFirmwareVersion,
        ckcapi_mdSlot_GetToken,
        (void *)NULL /* null terminator */
    };
