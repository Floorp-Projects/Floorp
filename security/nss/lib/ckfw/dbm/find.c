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
static const char CVS_ID[] = "@(#) $RCSfile: find.c,v $ $Revision: 1.4 $ $Date: 2006/03/02 22:48:54 $";
#endif /* DEBUG */

#include "ckdbm.h"

static void
nss_dbm_mdFindObjects_Final
(
  NSSCKMDFindObjects *mdFindObjects,
  NSSCKFWFindObjects *fwFindObjects,
  NSSCKMDSession *mdSession,
  NSSCKFWSession *fwSession,
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance
)
{
  nss_dbm_find_t *find = (nss_dbm_find_t *)mdFindObjects->etc;

  /* Locks might have system resources associated */
  (void)NSSCKFWMutex_Destroy(find->list_lock);
  (void)NSSArena_Destroy(find->arena);
}


static NSSCKMDObject *
nss_dbm_mdFindObjects_Next
(
  NSSCKMDFindObjects *mdFindObjects,
  NSSCKFWFindObjects *fwFindObjects,
  NSSCKMDSession *mdSession,
  NSSCKFWSession *fwSession,
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  NSSArena *arena,
  CK_RV *pError
)
{
  nss_dbm_find_t *find = (nss_dbm_find_t *)mdFindObjects->etc;
  struct nss_dbm_dbt_node *node;
  nss_dbm_object_t *object;
  NSSCKMDObject *rv;

  while(1) {
    /* Lock */
    {
      *pError = NSSCKFWMutex_Lock(find->list_lock);
      if( CKR_OK != *pError ) {
        return (NSSCKMDObject *)NULL;
      }
      
      node = find->found;
      if( (struct nss_dbm_dbt_node *)NULL != node ) {
        find->found = node->next;
      }
      
      *pError = NSSCKFWMutex_Unlock(find->list_lock);
      if( CKR_OK != *pError ) {
        /* screwed now */
        return (NSSCKMDObject *)NULL;
      }
    }

    if( (struct nss_dbm_dbt_node *)NULL == node ) {
      break;
    }

    if( nss_dbm_db_object_still_exists(node->dbt) ) {
      break;
    }
  }

  if( (struct nss_dbm_dbt_node *)NULL == node ) {
    *pError = CKR_OK;
    return (NSSCKMDObject *)NULL;
  }

  object = nss_ZNEW(arena, nss_dbm_object_t);
  if( (nss_dbm_object_t *)NULL == object ) {
    *pError = CKR_HOST_MEMORY;
    return (NSSCKMDObject *)NULL;
  }

  object->arena = arena;
  object->handle = nss_ZNEW(arena, nss_dbm_dbt_t);
  if( (nss_dbm_dbt_t *)NULL == object->handle ) {
    *pError = CKR_HOST_MEMORY;
    return (NSSCKMDObject *)NULL;
  }

  object->handle->my_db = node->dbt->my_db;
  object->handle->dbt.size = node->dbt->dbt.size;
  object->handle->dbt.data = nss_ZAlloc(arena, node->dbt->dbt.size);
  if( (void *)NULL == object->handle->dbt.data ) {
    *pError = CKR_HOST_MEMORY;
    return (NSSCKMDObject *)NULL;
  }

  (void)memcpy(object->handle->dbt.data, node->dbt->dbt.data, node->dbt->dbt.size);

  rv = nss_dbm_mdObject_factory(object, pError);
  if( (NSSCKMDObject *)NULL == rv ) {
    return (NSSCKMDObject *)NULL;
  }

  return rv;
}

NSS_IMPLEMENT NSSCKMDFindObjects *
nss_dbm_mdFindObjects_factory
(
  nss_dbm_find_t *find,
  CK_RV *pError
)
{
  NSSCKMDFindObjects *rv;

  rv = nss_ZNEW(find->arena, NSSCKMDFindObjects);
  if( (NSSCKMDFindObjects *)NULL == rv ) {
    *pError = CKR_HOST_MEMORY;
    return (NSSCKMDFindObjects *)NULL;
  }

  rv->etc = (void *)find;
  rv->Final = nss_dbm_mdFindObjects_Final;
  rv->Next = nss_dbm_mdFindObjects_Next;

  return rv;
}
