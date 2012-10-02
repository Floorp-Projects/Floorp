/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef DEBUG
static const char CVS_ID[] = "@(#) $RCSfile: hash.c,v $ $Revision: 1.6 $ $Date: 2012/04/25 14:49:28 $";
#endif /* DEBUG */

/*
 * hash.c
 *
 * This is merely a couple wrappers around NSPR's PLHashTable, using
 * the identity hash and arena-aware allocators.  The reason I did
 * this is that hash tables are used in a few places throughout the
 * NSS Cryptoki Framework in a fairly stereotyped way, and this allows
 * me to pull the commonalities into one place.  Should we ever want
 * to change the implementation, it's all right here.
 */

#ifndef CK_T
#include "ck.h"
#endif /* CK_T */

/*
 * nssCKFWHash
 *
 *  nssCKFWHash_Create
 *  nssCKFWHash_Destroy
 *  nssCKFWHash_Add
 *  nssCKFWHash_Remove
 *  nssCKFWHash_Count
 *  nssCKFWHash_Exists
 *  nssCKFWHash_Lookup
 *  nssCKFWHash_Iterate
 */

struct nssCKFWHashStr {
  NSSCKFWMutex *mutex;

  /*
   * The invariant that mutex protects is:
   *   The count accurately reflects the hashtable state.
   */

  PLHashTable *plHashTable;
  CK_ULONG count;
};

static PLHashNumber
nss_ckfw_identity_hash
(
  const void *key
)
{
  PRUint32 i = (PRUint32)key;
  PR_ASSERT(sizeof(PLHashNumber) == sizeof(PRUint32));
  return (PLHashNumber)i;
}

/*
 * nssCKFWHash_Create
 *
 */
NSS_IMPLEMENT nssCKFWHash *
nssCKFWHash_Create
(
  NSSCKFWInstance *fwInstance,
  NSSArena *arena,
  CK_RV *pError
)
{
  nssCKFWHash *rv;

#ifdef NSSDEBUG
  if (!pError) {
    return (nssCKFWHash *)NULL;
  }

  if( PR_SUCCESS != nssArena_verifyPointer(arena) ) {
    *pError = CKR_ARGUMENTS_BAD;
    return (nssCKFWHash *)NULL;
  }
#endif /* NSSDEBUG */

  rv = nss_ZNEW(arena, nssCKFWHash);
  if (!rv) {
    *pError = CKR_HOST_MEMORY;
    return (nssCKFWHash *)NULL;
  }

  rv->mutex = nssCKFWInstance_CreateMutex(fwInstance, arena, pError);
  if (!rv->mutex) {
    if( CKR_OK == *pError ) {
      (void)nss_ZFreeIf(rv);
      *pError = CKR_GENERAL_ERROR;
    }
    return (nssCKFWHash *)NULL;
  }

  rv->plHashTable = PL_NewHashTable(0, nss_ckfw_identity_hash, 
    PL_CompareValues, PL_CompareValues, &nssArenaHashAllocOps, arena);
  if (!rv->plHashTable) {
    (void)nssCKFWMutex_Destroy(rv->mutex);
    (void)nss_ZFreeIf(rv);
    *pError = CKR_HOST_MEMORY;
    return (nssCKFWHash *)NULL;
  }

  rv->count = 0;

  return rv;
}

/*
 * nssCKFWHash_Destroy
 *
 */
NSS_IMPLEMENT void
nssCKFWHash_Destroy
(
  nssCKFWHash *hash
)
{
  (void)nssCKFWMutex_Destroy(hash->mutex);
  PL_HashTableDestroy(hash->plHashTable);
  (void)nss_ZFreeIf(hash);
}

/*
 * nssCKFWHash_Add
 *
 */
NSS_IMPLEMENT CK_RV
nssCKFWHash_Add
(
  nssCKFWHash *hash,
  const void *key,
  const void *value
)
{
  CK_RV error = CKR_OK;
  PLHashEntry *he;

  error = nssCKFWMutex_Lock(hash->mutex);
  if( CKR_OK != error ) {
    return error;
  }
  
  he = PL_HashTableAdd(hash->plHashTable, key, (void *)value);
  if (!he) {
    error = CKR_HOST_MEMORY;
  } else {
    hash->count++;
  }

  (void)nssCKFWMutex_Unlock(hash->mutex);

  return error;
}

/*
 * nssCKFWHash_Remove
 *
 */
NSS_IMPLEMENT void
nssCKFWHash_Remove
(
  nssCKFWHash *hash,
  const void *it
)
{
  PRBool found;

  if( CKR_OK != nssCKFWMutex_Lock(hash->mutex) ) {
    return;
  }

  found = PL_HashTableRemove(hash->plHashTable, it);
  if( found ) {
    hash->count--;
  }

  (void)nssCKFWMutex_Unlock(hash->mutex);
  return;
}

/*
 * nssCKFWHash_Count
 *
 */
NSS_IMPLEMENT CK_ULONG
nssCKFWHash_Count
(
  nssCKFWHash *hash
)
{
  CK_ULONG count;

  if( CKR_OK != nssCKFWMutex_Lock(hash->mutex) ) {
    return (CK_ULONG)0;
  }

  count = hash->count;

  (void)nssCKFWMutex_Unlock(hash->mutex);

  return count;
}

/*
 * nssCKFWHash_Exists
 *
 */
NSS_IMPLEMENT CK_BBOOL
nssCKFWHash_Exists
(
  nssCKFWHash *hash,
  const void *it
)
{
  void *value;

  if( CKR_OK != nssCKFWMutex_Lock(hash->mutex) ) {
    return CK_FALSE;
  }

  value = PL_HashTableLookup(hash->plHashTable, it);

  (void)nssCKFWMutex_Unlock(hash->mutex);

  if (!value) {
    return CK_FALSE;
  } else {
    return CK_TRUE;
  }
}

/*
 * nssCKFWHash_Lookup
 *
 */
NSS_IMPLEMENT void *
nssCKFWHash_Lookup
(
  nssCKFWHash *hash,
  const void *it
)
{
  void *rv;

  if( CKR_OK != nssCKFWMutex_Lock(hash->mutex) ) {
    return (void *)NULL;
  }

  rv = PL_HashTableLookup(hash->plHashTable, it);

  (void)nssCKFWMutex_Unlock(hash->mutex);

  return rv;
}

struct arg_str {
  nssCKFWHashIterator fcn;
  void *closure;
};

static PRIntn
nss_ckfwhash_enumerator
(
  PLHashEntry *he,
  PRIntn index,
  void *arg
)
{
  struct arg_str *as = (struct arg_str *)arg;
  as->fcn(he->key, he->value, as->closure);
  return HT_ENUMERATE_NEXT;
}

/*
 * nssCKFWHash_Iterate
 *
 * NOTE that the iteration function will be called with the hashtable locked.
 */
NSS_IMPLEMENT void
nssCKFWHash_Iterate
(
  nssCKFWHash *hash,
  nssCKFWHashIterator fcn,
  void *closure
)
{
  struct arg_str as;
  as.fcn = fcn;
  as.closure = closure;

  if( CKR_OK != nssCKFWMutex_Lock(hash->mutex) ) {
    return;
  }

  PL_HashTableEnumerateEntries(hash->plHashTable, nss_ckfwhash_enumerator, &as);

  (void)nssCKFWMutex_Unlock(hash->mutex);

  return;
}
