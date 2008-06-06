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
static const char CVS_ID[] = "@(#) $RCSfile: slot.c,v $ $Revision: 1.3 $ $Date: 2006/03/02 22:48:54 $";
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
