/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef DEBUG
static const char CVS_ID[] = "@(#) $RCSfile: hash.c,v $ $Revision: 1.12 $ $Date: 2012/04/25 14:49:26 $";
#endif /* DEBUG */

/*
 * hash.c
 *
 * This is merely a couple wrappers around NSPR's PLHashTable, using
 * the identity hash and arena-aware allocators.
 * This is a copy of ckfw/hash.c, with modifications to use NSS types
 * (not Cryptoki types).  Would like for this to be a single implementation,
 * but doesn't seem like it will work.
 */

#ifndef BASE_H
#include "base.h"
#endif /* BASE_H */

#include "prbit.h"

/*
 * nssHash
 *
 *  nssHash_Create
 *  nssHash_Destroy
 *  nssHash_Add
 *  nssHash_Remove
 *  nssHash_Count
 *  nssHash_Exists
 *  nssHash_Lookup
 *  nssHash_Iterate
 */

struct nssHashStr {
  NSSArena *arena;
  PRBool i_alloced_arena;
  PRLock *mutex;

  /*
   * The invariant that mutex protects is:
   *   The count accurately reflects the hashtable state.
   */

  PLHashTable *plHashTable;
  PRUint32 count;
};

static PLHashNumber
nss_identity_hash
(
  const void *key
)
{
  PRUint32 i = (PRUint32)key;
  PR_ASSERT(sizeof(PLHashNumber) == sizeof(PRUint32));
  return (PLHashNumber)i;
}

static PLHashNumber
nss_item_hash
(
  const void *key
)
{
  unsigned int i;
  PLHashNumber h;
  NSSItem *it = (NSSItem *)key;
  h = 0;
  for (i=0; i<it->size; i++)
    h = PR_ROTATE_LEFT32(h, 4) ^ ((unsigned char *)it->data)[i];
  return h;
}

static int
nss_compare_items(const void *v1, const void *v2)
{
  PRStatus ignore;
  return (int)nssItem_Equal((NSSItem *)v1, (NSSItem *)v2, &ignore);
}

/*
 * nssHash_create
 *
 */
NSS_IMPLEMENT nssHash *
nssHash_Create
(
  NSSArena *arenaOpt,
  PRUint32 numBuckets,
  PLHashFunction keyHash,
  PLHashComparator keyCompare,
  PLHashComparator valueCompare
)
{
  nssHash *rv;
  NSSArena *arena;
  PRBool i_alloced;

#ifdef NSSDEBUG
  if( arenaOpt && PR_SUCCESS != nssArena_verifyPointer(arenaOpt) ) {
    nss_SetError(NSS_ERROR_INVALID_POINTER);
    return (nssHash *)NULL;
  }
#endif /* NSSDEBUG */

  if (arenaOpt) {
    arena = arenaOpt;
    i_alloced = PR_FALSE;
  } else {
    arena = nssArena_Create();
    i_alloced = PR_TRUE;
  }

  rv = nss_ZNEW(arena, nssHash);
  if( (nssHash *)NULL == rv ) {
    goto loser;
  }

  rv->mutex = PZ_NewLock(nssILockOther);
  if( (PZLock *)NULL == rv->mutex ) {
    goto loser;
  }

  rv->plHashTable = PL_NewHashTable(numBuckets, 
                                    keyHash, keyCompare, valueCompare,
                                    &nssArenaHashAllocOps, arena);
  if( (PLHashTable *)NULL == rv->plHashTable ) {
    (void)PZ_DestroyLock(rv->mutex);
    goto loser;
  }

  rv->count = 0;
  rv->arena = arena;
  rv->i_alloced_arena = i_alloced;

  return rv;
loser:
  (void)nss_ZFreeIf(rv);
  return (nssHash *)NULL;
}

/*
 * nssHash_CreatePointer
 *
 */
NSS_IMPLEMENT nssHash *
nssHash_CreatePointer
(
  NSSArena *arenaOpt,
  PRUint32 numBuckets
)
{
  return nssHash_Create(arenaOpt, numBuckets, 
                        nss_identity_hash, PL_CompareValues, PL_CompareValues);
}

/*
 * nssHash_CreateString
 *
 */
NSS_IMPLEMENT nssHash *
nssHash_CreateString
(
  NSSArena *arenaOpt,
  PRUint32 numBuckets
)
{
  return nssHash_Create(arenaOpt, numBuckets, 
                        PL_HashString, PL_CompareStrings, PL_CompareStrings);
}

/*
 * nssHash_CreateItem
 *
 */
NSS_IMPLEMENT nssHash *
nssHash_CreateItem
(
  NSSArena *arenaOpt,
  PRUint32 numBuckets
)
{
  return nssHash_Create(arenaOpt, numBuckets, 
                        nss_item_hash, nss_compare_items, PL_CompareValues);
}

/*
 * nssHash_Destroy
 *
 */
NSS_IMPLEMENT void
nssHash_Destroy
(
  nssHash *hash
)
{
  (void)PZ_DestroyLock(hash->mutex);
  PL_HashTableDestroy(hash->plHashTable);
  if (hash->i_alloced_arena) {
    nssArena_Destroy(hash->arena);
  } else {
    nss_ZFreeIf(hash);
  }
}

/*
 * nssHash_Add
 *
 */
NSS_IMPLEMENT PRStatus
nssHash_Add
(
  nssHash *hash,
  const void *key,
  const void *value
)
{
  PRStatus error = PR_FAILURE;
  PLHashEntry *he;

  PZ_Lock(hash->mutex);
  
  he = PL_HashTableAdd(hash->plHashTable, key, (void *)value);
  if( (PLHashEntry *)NULL == he ) {
    nss_SetError(NSS_ERROR_NO_MEMORY);
  } else if (he->value != value) {
    nss_SetError(NSS_ERROR_HASH_COLLISION);
  } else {
    hash->count++;
    error = PR_SUCCESS;
  }

  (void)PZ_Unlock(hash->mutex);

  return error;
}

/*
 * nssHash_Remove
 *
 */
NSS_IMPLEMENT void
nssHash_Remove
(
  nssHash *hash,
  const void *it
)
{
  PRBool found;

  PZ_Lock(hash->mutex);

  found = PL_HashTableRemove(hash->plHashTable, it);
  if( found ) {
    hash->count--;
  }

  (void)PZ_Unlock(hash->mutex);
  return;
}

/*
 * nssHash_Count
 *
 */
NSS_IMPLEMENT PRUint32
nssHash_Count
(
  nssHash *hash
)
{
  PRUint32 count;

  PZ_Lock(hash->mutex);

  count = hash->count;

  (void)PZ_Unlock(hash->mutex);

  return count;
}

/*
 * nssHash_Exists
 *
 */
NSS_IMPLEMENT PRBool
nssHash_Exists
(
  nssHash *hash,
  const void *it
)
{
  void *value;

  PZ_Lock(hash->mutex);

  value = PL_HashTableLookup(hash->plHashTable, it);

  (void)PZ_Unlock(hash->mutex);

  if( (void *)NULL == value ) {
    return PR_FALSE;
  } else {
    return PR_TRUE;
  }
}

/*
 * nssHash_Lookup
 *
 */
NSS_IMPLEMENT void *
nssHash_Lookup
(
  nssHash *hash,
  const void *it
)
{
  void *rv;

  PZ_Lock(hash->mutex);

  rv = PL_HashTableLookup(hash->plHashTable, it);

  (void)PZ_Unlock(hash->mutex);

  return rv;
}

struct arg_str {
  nssHashIterator fcn;
  void *closure;
};

static PRIntn
nss_hash_enumerator
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
 * nssHash_Iterate
 *
 * NOTE that the iteration function will be called with the hashtable locked.
 */
NSS_IMPLEMENT void
nssHash_Iterate
(
  nssHash *hash,
  nssHashIterator fcn,
  void *closure
)
{
  struct arg_str as;
  as.fcn = fcn;
  as.closure = closure;

  PZ_Lock(hash->mutex);

  PL_HashTableEnumerateEntries(hash->plHashTable, nss_hash_enumerator, &as);

  (void)PZ_Unlock(hash->mutex);

  return;
}
