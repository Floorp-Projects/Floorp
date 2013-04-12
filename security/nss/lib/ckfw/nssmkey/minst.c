/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifdef DEBUG
static const char CVS_ID[] = "@(#) $RCSfile$ $Revision$ $Date$";
#endif /* DEBUG */

#include "ckmk.h"

/*
 * nssmkey/minstance.c
 *
 * This file implements the NSSCKMDInstance object for the 
 * "nssmkey" cryptoki module.
 */

/*
 * NSSCKMDInstance methods
 */

static CK_ULONG
ckmk_mdInstance_GetNSlots
(
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  CK_RV *pError
)
{
  return (CK_ULONG)1;
}

static CK_VERSION
ckmk_mdInstance_GetCryptokiVersion
(
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance
)
{
  return nss_ckmk_CryptokiVersion;
}

static NSSUTF8 *
ckmk_mdInstance_GetManufacturerID
(
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  CK_RV *pError
)
{
  return (NSSUTF8 *)nss_ckmk_ManufacturerID;
}

static NSSUTF8 *
ckmk_mdInstance_GetLibraryDescription
(
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  CK_RV *pError
)
{
  return (NSSUTF8 *)nss_ckmk_LibraryDescription;
}

static CK_VERSION
ckmk_mdInstance_GetLibraryVersion
(
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance
)
{
  return nss_ckmk_LibraryVersion;
}

static CK_RV
ckmk_mdInstance_GetSlots
(
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  NSSCKMDSlot *slots[]
)
{
  slots[0] = (NSSCKMDSlot *)&nss_ckmk_mdSlot;
  return CKR_OK;
}

static CK_BBOOL
ckmk_mdInstance_ModuleHandlesSessionObjects
(
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance
)
{
  /* we don't want to allow any session object creation, at least
   * until we can investigate whether or not we can use those objects
   */
  return CK_TRUE;
}

NSS_IMPLEMENT_DATA const NSSCKMDInstance
nss_ckmk_mdInstance = {
  (void *)NULL, /* etc */
  NULL, /* Initialize */
  NULL, /* Finalize */
  ckmk_mdInstance_GetNSlots,
  ckmk_mdInstance_GetCryptokiVersion,
  ckmk_mdInstance_GetManufacturerID,
  ckmk_mdInstance_GetLibraryDescription,
  ckmk_mdInstance_GetLibraryVersion,
  ckmk_mdInstance_ModuleHandlesSessionObjects, 
  /*NULL, /* HandleSessionObjects */
  ckmk_mdInstance_GetSlots,
  NULL, /* WaitForSlotEvent */
  (void *)NULL /* null terminator */
};
