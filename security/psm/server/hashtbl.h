/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

#ifndef __SSM_HASHTBL_H__
#define __SSM_HASHTBL_H__

#include "nspr.h"
#include "prtypes.h"
#include "prmem.h"
#include "seccomon.h"
#include "secitem.h"
#include "ssmdefs.h"

/* It's a cheap method of hashing; we use the lower HASH_USE_LOW_BITS 
   bits of the key to determine what bucket it fits in. 
   Oh, what a hack. */
#define HASH_USE_LOW_BITS 4
#define HASH_NUM_BUCKETS (1 << HASH_USE_LOW_BITS)

typedef PRInt32 SSMHashKey;

struct _ssm_hashbucket
{
    SSMHashKey *m_keys;
    void **m_values;
    PRIntn m_size;
    PRIntn m_allocSize;
};

typedef struct _ssm_hashbucket SSMHashBucket;

struct _ssm_hashtbl 
{
    SSMHashBucket m_buckets[HASH_NUM_BUCKETS];
    PRIntn m_size;
    PRMonitor * m_hashLock;
};

typedef struct _ssm_hashtbl SSMHashTable;


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
**   If successful, returns PR_SUCCESS.
**   If failed, returns either PR_FAILURE or the underlying NSPR error,
**      if it is available.
**
*************************************************************/
SSMStatus SSM_HashInsert(SSMHashTable *hash, SSMHashKey key, void *value);

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
**   If successful, returns PR_SUCCESS.
**   If failed, returns either PR_FAILURE or the underlying NSPR error,
**      if it is available.
**
*************************************************************/
SSMStatus SSM_HashFind(SSMHashTable *hash, SSMHashKey key, void **value);


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
**   If successful, returns PR_SUCCESS.
**   If failed, returns either PR_FAILURE or the underlying NSPR error,
**      if it is available.
**
*************************************************************/
SSMStatus SSM_HashRemove(SSMHashTable *hash, SSMHashKey key, void **value);

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
**   If successful, returns PR_SUCCESS.
**   If failed, returns either PR_FAILURE or the underlying NSPR error,
**      if it is available.
**
*************************************************************/
SSMStatus SSM_HashKeys(SSMHashTable *hash, SSMHashKey **keys, PRIntn *numKeys);

/************************************************************
** FUNCTION: SSM_HashCount
** DESCRIPTION: Returns a count of the number of items in the hash.
** INPUTS:
**   hash
**     The hash to be counted.
**   numItems
**     Returns the number of items in the hash.
** RETURNS:
**   If successful, returns PR_SUCCESS.
**   If failed, returns either PR_FAILURE or the underlying NSPR error,
**      if it is available.
**
*************************************************************/
SSMStatus SSM_HashCount(SSMHashTable *hash, PRIntn *numItems);

/************************************************************
** FUNCTION: SSM_HashDestroy
** DESCRIPTION: Destroy a hash. Items in the hash are not freed.
** INPUTS:
**   hash
**     The hash to be destroyed.
** RETURNS:
**   If successful, returns PR_SUCCESS.
**   If failed, returns either PR_FAILURE or the underlying NSPR error,
**      if it is available.
**
*************************************************************/
SSMStatus SSM_HashDestroy(SSMHashTable *hash);

/************************************************************
** FUNCTION: SSM_HashCreate
** DESCRIPTION: Create and initialize a hash.
p** INPUTS:
**   hash
**     Returns the newly created hash.
** RETURNS:
**   If successful, returns PR_SUCCESS.
**   If failed, returns either PR_FAILURE or the underlying NSPR error,
**      if it is available.
**
*************************************************************/
SSMStatus SSM_HashCreate(SSMHashTable **hash);


#endif /* __SSM_HASHTBL_H__ */
