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
static const char CVS_ID[] = "@(#) $RCSfile: hash.c,v $ $Revision: 1.5 $ $Date: 2010/09/09 21:14:24 $";
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
