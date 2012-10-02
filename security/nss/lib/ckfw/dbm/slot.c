/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef DEBUG
static const char CVS_ID[] = "@(#) $RCSfile: slot.c,v $ $Revision: 1.4 $ $Date: 2012/04/25 14:49:30 $";
#endif /* DEBUG */

#include "ckdbm.h"

static CK_RV
nss_dbm_mdSlot_Initialize
(
  NSSCKMDSlot *mdSlot,
  NSSCKFWSlot *fwSlot,
  NSSCKMDInstance *mdInstance,                                    
  NSSCKFWInstance *fwInstance
)
{
  nss_dbm_slot_t *slot = (nss_dbm_slot_t *)mdSlot->etc;
  nss_dbm_instance_t *instance = (nss_dbm_instance_t *)mdInstance->etc;
  CK_RV rv = CKR_OK;

  slot->token_db = nss_dbm_db_open(instance->arena, fwInstance, slot->filename, 
                                   slot->flags, &rv);
  if( (nss_dbm_db_t *)NULL == slot->token_db ) {
    if( CKR_TOKEN_NOT_PRESENT == rv ) {
      /* This is not an error-- just means "the token isn't there" */
      rv = CKR_OK;
    }
  }

  return rv;
}

static void
nss_dbm_mdSlot_Destroy
(
  NSSCKMDSlot *mdSlot,
  NSSCKFWSlot *fwSlot,
  NSSCKMDInstance *mdInstance,                                    
  NSSCKFWInstance *fwInstance
)
{
  nss_dbm_slot_t *slot = (nss_dbm_slot_t *)mdSlot->etc;

  if( (nss_dbm_db_t *)NULL != slot->token_db ) {
    nss_dbm_db_close(slot->token_db);
    slot->token_db = (nss_dbm_db_t *)NULL;
  }
}

static NSSUTF8 *
nss_dbm_mdSlot_GetSlotDescription
(
  NSSCKMDSlot *mdSlot,
  NSSCKFWSlot *fwSlot,
  NSSCKMDInstance *mdInstance,                                    
  NSSCKFWInstance *fwInstance,
  CK_RV *pError
)
{
  return "Database";
}

static NSSUTF8 *
nss_dbm_mdSlot_GetManufacturerID
(
  NSSCKMDSlot *mdSlot,
  NSSCKFWSlot *fwSlot,
  NSSCKMDInstance *mdInstance,                                    
  NSSCKFWInstance *fwInstance,
  CK_RV *pError
)
{
  return "Berkeley";
}

static CK_BBOOL
nss_dbm_mdSlot_GetTokenPresent
(
  NSSCKMDSlot *mdSlot,
  NSSCKFWSlot *fwSlot,
  NSSCKMDInstance *mdInstance,                                    
  NSSCKFWInstance *fwInstance
)
{
  nss_dbm_slot_t *slot = (nss_dbm_slot_t *)mdSlot->etc;

  if( (nss_dbm_db_t *)NULL == slot->token_db ) {
    return CK_FALSE;
  } else {
    return CK_TRUE;
  }
}

static CK_BBOOL
nss_dbm_mdSlot_GetRemovableDevice
(
  NSSCKMDSlot *mdSlot,
  NSSCKFWSlot *fwSlot,
  NSSCKMDInstance *mdInstance,                                    
  NSSCKFWInstance *fwInstance
)
{
  /*
   * Well, this supports "tokens" (databases) that aren't there, so in
   * that sense they're removable.  It'd be nice to handle databases
   * that suddenly disappear (NFS-mounted home directories and network
   * errors, for instance) but that's a harder problem.  We'll say
   * we support removable devices, badly.
   */

  return CK_TRUE;
}

/* nss_dbm_mdSlot_GetHardwareSlot defaults to CK_FALSE */
/* 
 * nss_dbm_mdSlot_GetHardwareVersion
 * nss_dbm_mdSlot_GetFirmwareVersion
 *
 * These are kinda fuzzy concepts here.  I suppose we could return the
 * Berkeley DB version for one of them, if we had an actual number we
 * were confident in.  But mcom's "dbm" has been hacked enough that I
 * don't really know from what "real" version it stems..
 */

static NSSCKMDToken *
nss_dbm_mdSlot_GetToken
(
  NSSCKMDSlot *mdSlot,
  NSSCKFWSlot *fwSlot,
  NSSCKMDInstance *mdInstance,                                    
  NSSCKFWInstance *fwInstance,
  CK_RV *pError
)
{
  nss_dbm_slot_t *slot = (nss_dbm_slot_t *)mdSlot->etc;
  return nss_dbm_mdToken_factory(slot, pError);
}

NSS_IMPLEMENT NSSCKMDSlot *
nss_dbm_mdSlot_factory
(
  nss_dbm_instance_t *instance,
  char *filename,
  int flags,
  CK_RV *pError
)
{
  nss_dbm_slot_t *slot;
  NSSCKMDSlot *rv;

  slot = nss_ZNEW(instance->arena, nss_dbm_slot_t);
  if( (nss_dbm_slot_t *)NULL == slot ) {
    *pError = CKR_HOST_MEMORY;
    return (NSSCKMDSlot *)NULL;
  }

  slot->instance = instance;
  slot->filename = filename;
  slot->flags = flags;
  slot->token_db = (nss_dbm_db_t *)NULL;

  rv = nss_ZNEW(instance->arena, NSSCKMDSlot);
  if( (NSSCKMDSlot *)NULL == rv ) {
    *pError = CKR_HOST_MEMORY;
    return (NSSCKMDSlot *)NULL;
  }

  rv->etc = (void *)slot;
  rv->Initialize = nss_dbm_mdSlot_Initialize;
  rv->Destroy = nss_dbm_mdSlot_Destroy;
  rv->GetSlotDescription = nss_dbm_mdSlot_GetSlotDescription;
  rv->GetManufacturerID = nss_dbm_mdSlot_GetManufacturerID;
  rv->GetTokenPresent = nss_dbm_mdSlot_GetTokenPresent;
  rv->GetRemovableDevice = nss_dbm_mdSlot_GetRemovableDevice;
  /*  GetHardwareSlot */
  /*  GetHardwareVersion */
  /*  GetFirmwareVersion */
  rv->GetToken = nss_dbm_mdSlot_GetToken;
  rv->null = (void *)NULL;

  return rv;
}
