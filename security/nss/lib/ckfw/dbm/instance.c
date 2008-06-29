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
static const char CVS_ID[] = "@(#) $RCSfile: instance.c,v $ $Revision: 1.4 $ $Date: 2006/03/02 22:48:54 $";
#endif /* DEBUG */

#include "ckdbm.h"

static CK_RV
nss_dbm_mdInstance_Initialize
(
  NSSCKMDInstance *mdInstance,                                    
  NSSCKFWInstance *fwInstance,
  NSSUTF8 *configurationData
)
{
  CK_RV rv = CKR_OK;
  NSSArena *arena;
  nss_dbm_instance_t *instance;

  arena = NSSCKFWInstance_GetArena(fwInstance, &rv);
  if( ((NSSArena *)NULL == arena) && (CKR_OK != rv) ) {
    return rv;
  }

  instance = nss_ZNEW(arena, nss_dbm_instance_t);
  if( (nss_dbm_instance_t *)NULL == instance ) {
    return CKR_HOST_MEMORY;
  }

  instance->arena = arena;

  /*
   * This should parse the configuration data for information on
   * number and locations of databases, modes (e.g. readonly), etc.
   * But for now, we'll have one slot with a creatable read-write
   * database called "cert8.db."
   */

  instance->nSlots = 1;
  instance->filenames = nss_ZNEWARRAY(arena, char *, instance->nSlots);
  if( (char **)NULL == instance->filenames ) {
    return CKR_HOST_MEMORY;
  }

  instance->flags = nss_ZNEWARRAY(arena, int, instance->nSlots);
  if( (int *)NULL == instance->flags ) {
    return CKR_HOST_MEMORY;
  }

  instance->filenames[0] = "cert8.db";
  instance->flags[0] = O_RDWR|O_CREAT;

  mdInstance->etc = (void *)instance;
  return CKR_OK;
}

/* nss_dbm_mdInstance_Finalize is not required */

static CK_ULONG
nss_dbm_mdInstance_GetNSlots
(
  NSSCKMDInstance *mdInstance,                                    
  NSSCKFWInstance *fwInstance,
  CK_RV *pError
)
{
  nss_dbm_instance_t *instance = (nss_dbm_instance_t *)mdInstance->etc;
  return instance->nSlots;
}

static CK_VERSION
nss_dbm_mdInstance_GetCryptokiVersion
(
  NSSCKMDInstance *mdInstance,                                    
  NSSCKFWInstance *fwInstance
)
{
  static CK_VERSION rv = { 2, 1 };
  return rv;
}

static NSSUTF8 *
nss_dbm_mdInstance_GetManufacturerID
(
  NSSCKMDInstance *mdInstance,                                    
  NSSCKFWInstance *fwInstance,
  CK_RV *pError
)
{
  return "Mozilla Foundation";
}

static NSSUTF8 *
nss_dbm_mdInstance_GetLibraryDescription
(
  NSSCKMDInstance *mdInstance,                                    
  NSSCKFWInstance *fwInstance,
  CK_RV *pError
)
{
  return "Berkeley Database Module";
}

static CK_VERSION
nss_dbm_mdInstance_GetLibraryVersion
(
  NSSCKMDInstance *mdInstance,                                    
  NSSCKFWInstance *fwInstance
)
{
  static CK_VERSION rv = { 1, 0 }; /* My own version number */
  return rv;
}

static CK_BBOOL
nss_dbm_mdInstance_ModuleHandlesSessionObjects
(
  NSSCKMDInstance *mdInstance,                                    
  NSSCKFWInstance *fwInstance
)
{
  return CK_TRUE;
}

static CK_RV
nss_dbm_mdInstance_GetSlots
(
  NSSCKMDInstance *mdInstance,                                    
  NSSCKFWInstance *fwInstance,
  NSSCKMDSlot *slots[]
)
{
  nss_dbm_instance_t *instance = (nss_dbm_instance_t *)mdInstance->etc;
  CK_ULONG i;
  CK_RV rv = CKR_OK;

  for( i = 0; i < instance->nSlots; i++ ) {
    slots[i] = nss_dbm_mdSlot_factory(instance, instance->filenames[i], 
                                      instance->flags[i], &rv);
    if( (NSSCKMDSlot *)NULL == slots[i] ) {
      return rv;
    }
  }

  return rv;
}

/* nss_dbm_mdInstance_WaitForSlotEvent is not relevant */

NSS_IMPLEMENT_DATA NSSCKMDInstance 
nss_dbm_mdInstance = {
  NULL, /* etc; filled in later */
  nss_dbm_mdInstance_Initialize,
  NULL, /* nss_dbm_mdInstance_Finalize */
  nss_dbm_mdInstance_GetNSlots,
  nss_dbm_mdInstance_GetCryptokiVersion,
  nss_dbm_mdInstance_GetManufacturerID,
  nss_dbm_mdInstance_GetLibraryDescription,
  nss_dbm_mdInstance_GetLibraryVersion,
  nss_dbm_mdInstance_ModuleHandlesSessionObjects,
  nss_dbm_mdInstance_GetSlots,
  NULL, /* nss_dbm_mdInstance_WaitForSlotEvent */
  NULL /* terminator */
};
