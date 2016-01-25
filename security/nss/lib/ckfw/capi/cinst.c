/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ckcapi.h"

/*
 * ckcapi/cinstance.c
 *
 * This file implements the NSSCKMDInstance object for the
 * "capi" cryptoki module.
 */

/*
 * NSSCKMDInstance methods
 */

static CK_ULONG
ckcapi_mdInstance_GetNSlots(
    NSSCKMDInstance *mdInstance,
    NSSCKFWInstance *fwInstance,
    CK_RV *pError)
{
    return (CK_ULONG)1;
}

static CK_VERSION
ckcapi_mdInstance_GetCryptokiVersion(
    NSSCKMDInstance *mdInstance,
    NSSCKFWInstance *fwInstance)
{
    return nss_ckcapi_CryptokiVersion;
}

static NSSUTF8 *
ckcapi_mdInstance_GetManufacturerID(
    NSSCKMDInstance *mdInstance,
    NSSCKFWInstance *fwInstance,
    CK_RV *pError)
{
    return (NSSUTF8 *)nss_ckcapi_ManufacturerID;
}

static NSSUTF8 *
ckcapi_mdInstance_GetLibraryDescription(
    NSSCKMDInstance *mdInstance,
    NSSCKFWInstance *fwInstance,
    CK_RV *pError)
{
    return (NSSUTF8 *)nss_ckcapi_LibraryDescription;
}

static CK_VERSION
ckcapi_mdInstance_GetLibraryVersion(
    NSSCKMDInstance *mdInstance,
    NSSCKFWInstance *fwInstance)
{
    return nss_ckcapi_LibraryVersion;
}

static CK_RV
ckcapi_mdInstance_GetSlots(
    NSSCKMDInstance *mdInstance,
    NSSCKFWInstance *fwInstance,
    NSSCKMDSlot *slots[])
{
    slots[0] = (NSSCKMDSlot *)&nss_ckcapi_mdSlot;
    return CKR_OK;
}

static CK_BBOOL
ckcapi_mdInstance_ModuleHandlesSessionObjects(
    NSSCKMDInstance *mdInstance,
    NSSCKFWInstance *fwInstance)
{
    /* we don't want to allow any session object creation, at least
     * until we can investigate whether or not we can use those objects
     */
    return CK_TRUE;
}

NSS_IMPLEMENT_DATA const NSSCKMDInstance
    nss_ckcapi_mdInstance = {
        (void *)NULL, /* etc */
        NULL,         /* Initialize */
        NULL,         /* Finalize */
        ckcapi_mdInstance_GetNSlots,
        ckcapi_mdInstance_GetCryptokiVersion,
        ckcapi_mdInstance_GetManufacturerID,
        ckcapi_mdInstance_GetLibraryDescription,
        ckcapi_mdInstance_GetLibraryVersion,
        ckcapi_mdInstance_ModuleHandlesSessionObjects,
        /*NULL, /* HandleSessionObjects */
        ckcapi_mdInstance_GetSlots,
        NULL,        /* WaitForSlotEvent */
        (void *)NULL /* null terminator */
    };
