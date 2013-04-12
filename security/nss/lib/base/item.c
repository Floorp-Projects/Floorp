/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef DEBUG
static const char CVS_ID[] = "@(#) $RCSfile$ $Revision$ $Date$";
#endif /* DEBUG */

/*
 * item.c
 *
 * This contains some item-manipulation code.
 */

#ifndef BASE_H
#include "base.h"
#endif /* BASE_H */

/*
 * nssItem_Create
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_ARENA_MARKED_BY_ANOTHER_THREAD
 *  NSS_ERROR_INVALID_POINTER
 *  
 * Return value:
 *  A pointer to an NSSItem upon success
 *  NULL upon failure
 */

NSS_IMPLEMENT NSSItem *
nssItem_Create
(
  NSSArena *arenaOpt,
  NSSItem *rvOpt,
  PRUint32 length,
  const void *data
)
{
  NSSItem *rv = (NSSItem *)NULL;

#ifdef DEBUG
  if( (NSSArena *)NULL != arenaOpt ) {
    if( PR_SUCCESS != nssArena_verifyPointer(arenaOpt) ) {
      return (NSSItem *)NULL;
    }
  }

  if( (const void *)NULL == data ) {
    if( length > 0 ) {
      nss_SetError(NSS_ERROR_INVALID_POINTER);
      return (NSSItem *)NULL;
    }
  }
#endif /* DEBUG */

  if( (NSSItem *)NULL == rvOpt ) {
    rv = (NSSItem *)nss_ZNEW(arenaOpt, NSSItem);
    if( (NSSItem *)NULL == rv ) {
      goto loser;
    }
  } else {
    rv = rvOpt;
  }

  rv->size = length;
  rv->data = nss_ZAlloc(arenaOpt, length);
  if( (void *)NULL == rv->data ) {
    goto loser;
  }

  if( length > 0 ) {
    (void)nsslibc_memcpy(rv->data, data, length);
  }

  return rv;

 loser:
  if( rv != rvOpt ) {
    nss_ZFreeIf(rv);
  }

  return (NSSItem *)NULL;
}

NSS_IMPLEMENT void
nssItem_Destroy
(
  NSSItem *item
)
{
  nss_ClearErrorStack();

  nss_ZFreeIf(item->data);
  nss_ZFreeIf(item);

}

/*
 * nssItem_Duplicate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_ARENA_MARKED_BY_ANOTHER_THREAD
 *  NSS_ERROR_INVALID_ITEM
 *  
 * Return value:
 *  A pointer to an NSSItem upon success
 *  NULL upon failure
 */

NSS_IMPLEMENT NSSItem *
nssItem_Duplicate
(
  NSSItem *obj,
  NSSArena *arenaOpt,
  NSSItem *rvOpt
)
{
#ifdef DEBUG
  if( (NSSArena *)NULL != arenaOpt ) {
    if( PR_SUCCESS != nssArena_verifyPointer(arenaOpt) ) {
      return (NSSItem *)NULL;
    }
  }

  if( (NSSItem *)NULL == obj ) {
    nss_SetError(NSS_ERROR_INVALID_ITEM);
    return (NSSItem *)NULL;
  }
#endif /* DEBUG */

  return nssItem_Create(arenaOpt, rvOpt, obj->size, obj->data);
}

#ifdef DEBUG
/*
 * nssItem_verifyPointer
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_ITEM
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_IMPLEMENT PRStatus
nssItem_verifyPointer
(
  const NSSItem *item
)
{
  if( ((const NSSItem *)NULL == item) ||
      (((void *)NULL == item->data) && (item->size > 0)) ) {
    nss_SetError(NSS_ERROR_INVALID_ITEM);
    return PR_FAILURE;
  }

  return PR_SUCCESS;
}
#endif /* DEBUG */

/*
 * nssItem_Equal
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_ITEM
 *
 * Return value:
 *  PR_TRUE if the items are identical
 *  PR_FALSE if they aren't
 *  PR_FALSE upon error
 */

NSS_IMPLEMENT PRBool
nssItem_Equal
(
  const NSSItem *one,
  const NSSItem *two,
  PRStatus *statusOpt
)
{
  if( (PRStatus *)NULL != statusOpt ) {
    *statusOpt = PR_SUCCESS;
  }

  if( ((const NSSItem *)NULL == one) && ((const NSSItem *)NULL == two) ) {
    return PR_TRUE;
  }

  if( ((const NSSItem *)NULL == one) || ((const NSSItem *)NULL == two) ) {
    return PR_FALSE;
  }

  if( one->size != two->size ) {
    return PR_FALSE;
  }

  return nsslibc_memequal(one->data, two->data, one->size, statusOpt);
}
