/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */
/*
  hashtbl.c -- hash tables for Cartman, specifically for use with
  resources (32-bit integer keys).
 */

#include "hashtbl.h"

#define BUCKETNUM(key) ((key) & (HASH_NUM_BUCKETS-1))

/* SSMHashBucket support routines */

void ssmbucket_invariant(SSMHashBucket *bucket)
{
    PR_ASSERT(bucket->m_size >= 0);
    PR_ASSERT(bucket->m_allocSize > 0);
    PR_ASSERT(bucket->m_size <= bucket->m_allocSize);
    PR_ASSERT(bucket->m_keys != NULL);
    PR_ASSERT(bucket->m_values != NULL);
}

SSMStatus ssmbucket_remove(SSMHashBucket *bucket, PRIntn whichElem)
{
    SSMHashKey *keyptr;
    void **valptr;
    PRIntn numToMove;

    ssmbucket_invariant(bucket);
    PR_ASSERT(whichElem < bucket->m_size);
    

    if (whichElem >= bucket->m_size) return PR_FAILURE;

    keyptr = bucket->m_keys + whichElem;
    valptr = bucket->m_values + whichElem;
    numToMove = bucket->m_size - (whichElem + 1);
    memmove(keyptr, keyptr + 1, numToMove * sizeof(SSMHashKey));
    memmove(valptr, valptr + 1, numToMove * sizeof(void *));
    bucket->m_size--;
    
    ssmbucket_invariant(bucket);
    return PR_SUCCESS;
}

PRIntn ssmbucket_find(SSMHashBucket *bucket, SSMHashKey key)
{
    PRIntn i;
    for(i=0;i<bucket->m_size; i++)
    {
        if (bucket->m_keys[i] == key)
            return i;
    }
    return -1;
}

SSMStatus ssmbucket_grow(SSMHashBucket *bucket)
{
    size_t newSize;
    SSMHashKey *newKeys = NULL;
    void **newValues = NULL;

    ssmbucket_invariant(bucket);

    /* If we're growing, double the size */
    newSize = bucket->m_allocSize * 2;

    /* realloc the memory. is this broken? */
    newKeys = (SSMHashKey *) PR_REALLOC(bucket->m_keys, newSize * sizeof(SSMHashKey));
    if (!newKeys) goto loser;

    newValues = (void **) PR_REALLOC(bucket->m_values, newSize * sizeof(void *));
    if (!newValues) goto loser;

    bucket->m_keys = newKeys;
    bucket->m_values = newValues;
    bucket->m_allocSize = newSize;

    ssmbucket_invariant(bucket);
    return PR_SUCCESS;

 loser:
    if (newKeys) PR_Free(newKeys);
    if (newValues) PR_Free(newValues);
    return PR_FAILURE;
}

SSMStatus ssmbucket_add(SSMHashBucket *bucket, SSMHashKey key, void *value)
{
    PRIntn newSize = bucket->m_size + 1;
    SSMStatus rv = PR_SUCCESS;

    ssmbucket_invariant(bucket);

    if (bucket->m_size >= bucket->m_allocSize)
        rv = ssmbucket_grow(bucket);

    if (rv == PR_SUCCESS)
    {
        bucket->m_keys[newSize-1] = key;
        bucket->m_values[newSize-1] = value;
        bucket->m_size = newSize;
    }

    ssmbucket_invariant(bucket);
    return rv;
}

SSMStatus ssmbucket_free(SSMHashBucket *bucket)
{
    if (bucket)
    {
        if (bucket->m_keys) 
        {
            PR_Free(bucket->m_keys);
            bucket->m_keys = NULL;
        }
        
        if (bucket->m_values)
        {
            PR_Free(bucket->m_values);
            bucket->m_values = NULL;
        }
        
        bucket->m_size = bucket->m_allocSize = 0;
    }
    return PR_SUCCESS;
}

SSMStatus ssmbucket_init(SSMHashBucket *bucket)
{
    bucket->m_keys = NULL;
    bucket->m_values = NULL;

    bucket->m_keys = (SSMHashKey *) PR_CALLOC(2 * sizeof(SSMHashKey));
    if (!bucket->m_keys) goto loser;

    bucket->m_values = (void **) PR_CALLOC(2 * sizeof(void *));
    if (!bucket->m_values) goto loser;

    bucket->m_size = 0;
    bucket->m_allocSize = 2;
    
    return PR_SUCCESS;
 loser:
    ssmbucket_free(bucket);
    return PR_FAILURE;
}

/************************************************************
** FUNCTION: SSM_HashInsert
** DESCRIPTION: Insert a new data item keyed by (key) into (hash).
** INPUTS:
**   hash
**     The hash into which the item is to be inserted.
**   key
**     The key used to store the item.
**   value
**     The item to be inserted.
** RETURNS:
**   PR_SUCCESS on success.
**   PR_FAILURE on failure.
**
*************************************************************/
SSMStatus SSM_HashInsert(SSMHashTable *hash, SSMHashKey key, void *value)
{
    PRIntn whichElem;
    if ((hash == NULL) || (value == NULL))
    {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return PR_FAILURE;
    }
    PR_EnterMonitor(hash->m_hashLock);
    
    /* If the key already exists, replace the value.
       Otherwise, add the new key/value. */
    whichElem = ssmbucket_find(&(hash->m_buckets[BUCKETNUM(key)]), key);
    if (whichElem >= 0)
        hash->m_buckets[BUCKETNUM(key)].m_values[whichElem] = value;
    else
        ssmbucket_add(&(hash->m_buckets[BUCKETNUM(key)]), key, value);
    PR_ExitMonitor(hash->m_hashLock);

    return PR_SUCCESS;
}

SSMStatus ssm_HashFindValue(SSMHashTable *hash, SSMHashKey key, void **value,
                           PRBool removeElem)
{
    PRIntn whichElem;
    SSMStatus rv = PR_SUCCESS;


    if ((hash == NULL) || (value == NULL))
    {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return PR_FAILURE;
    }
    PR_EnterMonitor(hash->m_hashLock);

    *value = NULL; /* in case we fail */

    whichElem = ssmbucket_find(&(hash->m_buckets[BUCKETNUM(key)]), key);
    if (whichElem >= 0)
    {
        *value = hash->m_buckets[BUCKETNUM(key)].m_values[whichElem];
        if (removeElem)
            rv = ssmbucket_remove(&(hash->m_buckets[BUCKETNUM(key)]), 
                                  whichElem);
    }
    else
        rv = PR_FAILURE;
    PR_ExitMonitor(hash->m_hashLock);

    return rv;
}

/************************************************************
** FUNCTION: SSM_HashFind
** DESCRIPTION: Get the item whose key is (key).
** INPUTS:
**   hash
**     The hash from which the item is to be retrieved.
**   key
**     The key used to store the item.
**   value
**     The item to be returned. Set to NULL if no suitable
**     item is found.
** RETURNS:
**   PR_SUCCESS on success.
**   PR_FAILURE on failure.
**
*************************************************************/
SSMStatus SSM_HashFind(SSMHashTable *hash, SSMHashKey key, void **value)
{
    return ssm_HashFindValue(hash, key, value, PR_FALSE);
}

/************************************************************
** FUNCTION: SSM_HashRemove
** DESCRIPTION: Remove from a hash the item whose key is (key).
** INPUTS:
**   hash
**     The hash from which the item is to be removed.
**   key
**     The key used to store the item.
**   value
**     The value removed from the hash.
** RETURNS:
**   PR_SUCCESS on success.
**   PR_FAILURE on failure.
**
*************************************************************/
SSMStatus SSM_HashRemove(SSMHashTable *hash, SSMHashKey key, void **value)
{
    return ssm_HashFindValue(hash, key, value, PR_TRUE);
}

/************************************************************
** FUNCTION: SSM_HashKeys
** DESCRIPTION: Get keys for all items stored in the hash.
** INPUTS:
**   hash
**     The hash from which the keys are to be returned.
**   keys
**     Returns an array of keys. The array is allocated using PR_Malloc.
**   numKeys
**     Returns the number of keys in the array.
** RETURNS:
**   PR_SUCCESS on success.
**   PR_FAILURE on failure.
**
*************************************************************/
SSMStatus SSM_HashKeys(SSMHashTable *hash, SSMHashKey **keys, PRIntn *numKeys)
{
    SSMHashKey *rkeys = NULL, *walk;
    PRIntn i, howMany;

    /* in case we fail */
    *keys = NULL;
    *numKeys = 0;

    if (hash == NULL)
    {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return PR_FAILURE;
    }

    PR_EnterMonitor(hash->m_hashLock);
    rkeys = (SSMHashKey *) PR_CALLOC(hash->m_size * sizeof(SSMHashKey));
    if (!rkeys)
        return PR_FAILURE;

    /* Walk through all the buckets, grabbing their keys. */
    walk = rkeys;
    for(i=0;i<HASH_NUM_BUCKETS;i++)
    {
        howMany = hash->m_buckets[i].m_size;
        (void) memcpy(walk, hash->m_buckets[i].m_keys, 
                      hash->m_buckets[i].m_size * sizeof(SSMHashKey));
        walk += howMany;
    }
    PR_ExitMonitor(hash->m_hashLock);

    *keys = rkeys;
    *numKeys = hash->m_size;
    return PR_SUCCESS;
}


/************************************************************
** FUNCTION: SSM_HashDestroy
** DESCRIPTION: Destroy a hash. Items in the hash are not freed.
** INPUTS:
**   hash
**     The hash to be destroyed.
** RETURNS:
**   PR_SUCCESS on success.
**   PR_FAILURE on failure.
**
*************************************************************/
SSMStatus SSM_HashDestroy(SSMHashTable *hash)
{
    if (hash)
    {
        PRIntn i;
        for (i=0;i<HASH_NUM_BUCKETS;i++)
            ssmbucket_free(&(hash->m_buckets[i]));
        PR_DestroyMonitor(hash->m_hashLock);

        PR_Free(hash);
    }
    return PR_SUCCESS;
}


/************************************************************
** FUNCTION: SSM_HashCreate
** DESCRIPTION: Create and initialize a hash.
** INPUTS:
**   hash
**     Returns the newly created hash.
** RETURNS:
**   PR_SUCCESS on success.
**   PR_FAILURE on failure.
**
*************************************************************/
SSMStatus SSM_HashCreate(SSMHashTable **hash)
{
    PRIntn i;
    SSMHashTable *rhash;

    *hash = NULL; /* in case we fail */

    rhash = (SSMHashTable *) PR_CALLOC(sizeof(SSMHashTable));
    if (!rhash) goto loser;
    for(i=0;i<HASH_NUM_BUCKETS;i++)
    {
        if (ssmbucket_init(&(rhash->m_buckets[i])))
            goto loser;
    }

    rhash->m_hashLock = PR_NewMonitor();
    if (!rhash->m_hashLock)
       goto loser;
    *hash = rhash;
    return PR_SUCCESS;

 loser:
    if (rhash)
        SSM_HashDestroy(rhash);
    return PR_FAILURE;
}

SSMStatus SSM_HashCount(SSMHashTable *hash, PRIntn *numItems)
{
    *numItems = 0; /* in case we fail */

    if (hash == NULL)
    {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return PR_FAILURE;
    }

    *numItems = hash->m_size;
    return PR_SUCCESS;
}
