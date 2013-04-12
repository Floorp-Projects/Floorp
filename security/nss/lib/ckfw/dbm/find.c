/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef DEBUG
static const char CVS_ID[] = "@(#) $RCSfile$ $Revision$ $Date$";
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
