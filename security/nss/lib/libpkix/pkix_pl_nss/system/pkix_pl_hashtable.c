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
 *   Sun Microsystems
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
/*
 * pkix_pl_hashtable.c
 *
 * Hashtable Object Functions
 *
 */

#include "pkix_pl_hashtable.h"

/* --Private-Functions-------------------------------------------- */

/*
 * FUNCTION: pkix_pl_HashTable_Destroy
 * (see comments for PKIX_PL_DestructorCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_HashTable_Destroy(
        PKIX_PL_Object *object,
        void *plContext)
{
        PKIX_PL_HashTable *ht = NULL;
        pkix_pl_HT_Elem *item = NULL;
        PKIX_UInt32 i;

        PKIX_ENTER(HASHTABLE, "pkix_pl_HashTable_Destroy");
        PKIX_NULLCHECK_ONE(object);

        PKIX_CHECK(pkix_CheckType(object, PKIX_HASHTABLE_TYPE, plContext),
                    PKIX_OBJECTNOTHASHTABLE);

        ht = (PKIX_PL_HashTable*) object;

        /* DecRef every object in the primitive hash table */
        for (i = 0; i < ht->primHash->size; i++) {
                for (item = ht->primHash->buckets[i];
                    item != NULL;
                    item = item->next) {
                        PKIX_DECREF(item->key);
                        PKIX_DECREF(item->value);
                }
        }

        PKIX_CHECK(pkix_pl_PrimHashTable_Destroy(ht->primHash, plContext),
                    PKIX_PRIMHASHTABLEDESTROYFAILED);

        PKIX_DECREF(ht->tableLock);

cleanup:

        PKIX_RETURN(HASHTABLE);
}

/*
 * FUNCTION: pkix_pl_HashTable_RegisterSelf
 * DESCRIPTION:
 * Registers PKIX_HASHTABLE_TYPE and its related functions with systemClasses[]
 * THREAD SAFETY:
 *  Not Thread Safe - for performance and complexity reasons
 *
 *  Since this function is only called by PKIX_PL_Initialize, which should
 *  only be called once, it is acceptable that this function is not
 *  thread-safe.
 */
PKIX_Error *
pkix_pl_HashTable_RegisterSelf(
        void *plContext)
{

        extern pkix_ClassTable_Entry systemClasses[PKIX_NUMTYPES];
        pkix_ClassTable_Entry entry;

        PKIX_ENTER(HASHTABLE, "pkix_pl_HashTable_RegisterSelf");

        entry.description = "HashTable";
        entry.destructor = pkix_pl_HashTable_Destroy;
        entry.equalsFunction = NULL;
        entry.hashcodeFunction = NULL;
        entry.toStringFunction = NULL;
        entry.comparator = NULL;
        entry.duplicateFunction = NULL;

        systemClasses[PKIX_HASHTABLE_TYPE] = entry;

        PKIX_RETURN(HASHTABLE);
}

/* --Public-Functions------------------------------------------------------- */

/*
 * FUNCTION: PKIX_PL_HashTable_Create (see comments in pkix_pl_system.h)
 */
PKIX_Error *
PKIX_PL_HashTable_Create(
        PKIX_UInt32 numBuckets,
        PKIX_UInt32 maxEntriesPerBucket,
        PKIX_PL_HashTable **pResult,
        void *plContext)
{
        PKIX_PL_HashTable *hashTable = NULL;

        PKIX_ENTER(HASHTABLE, "PKIX_PL_HashTable_Create");
        PKIX_NULLCHECK_ONE(pResult);

        if (numBuckets == 0) {
                PKIX_ERROR(PKIX_NUMBUCKETSEQUALSZERO);
        }

        /* Allocate a new hashtable */
        PKIX_CHECK(PKIX_PL_Object_Alloc
                (PKIX_HASHTABLE_TYPE,
                sizeof (PKIX_PL_HashTable),
                (PKIX_PL_Object **)&hashTable,
                plContext),
                PKIX_COULDNOTCREATEHASHTABLEOBJECT);

        /* Create the underlying primitive hash table type */
        PKIX_CHECK(pkix_pl_PrimHashTable_Create
                    (numBuckets, &hashTable->primHash, plContext),
                    PKIX_PRIMHASHTABLECREATEFAILED);

        /* Create a lock for this table */
        PKIX_CHECK(PKIX_PL_Mutex_Create(&hashTable->tableLock, plContext),
                    PKIX_ERRORCREATINGTABLELOCK);

        hashTable->maxEntriesPerBucket = maxEntriesPerBucket;

        *pResult = hashTable;

cleanup:

        if (PKIX_ERROR_RECEIVED){
                PKIX_DECREF(hashTable);
        }

        PKIX_RETURN(HASHTABLE);
}

/*
 * FUNCTION: PKIX_PL_HashTable_Add (see comments in pkix_pl_system.h)
 */
PKIX_Error *
PKIX_PL_HashTable_Add(
        PKIX_PL_HashTable *ht,
        PKIX_PL_Object *key,
        PKIX_PL_Object *value,
        void *plContext)
{
        PKIX_PL_Object *deletedKey = NULL;
        PKIX_PL_Object *deletedValue = NULL;
        PKIX_UInt32 hashCode;
        PKIX_PL_EqualsCallback keyComp;
        PKIX_Boolean tableLocked = PKIX_FALSE;
        PKIX_UInt32 bucketSize = 0;

        PKIX_ENTER(HASHTABLE, "PKIX_PL_HashTable_Add");
        PKIX_NULLCHECK_THREE(ht, key, value);

        /* Insert into primitive hashtable */

        PKIX_CHECK(PKIX_PL_Object_Hashcode(key, &hashCode, plContext),
                    PKIX_OBJECTHASHCODEFAILED);

        PKIX_CHECK(pkix_pl_Object_RetrieveEqualsCallback
                    (key, &keyComp, plContext),
                    PKIX_OBJECTRETRIEVEEQUALSCALLBACKFAILED);

        PKIX_CHECK(pkix_pl_PrimHashTable_GetBucketSize
                (ht->primHash,
                hashCode,
                &bucketSize,
                plContext),
                PKIX_PRIMHASHTABLEGETBUCKETSIZEFAILED);

        if (ht->maxEntriesPerBucket != 0 &&
            bucketSize >= ht->maxEntriesPerBucket) {
                /* drop the last one in the bucket */
                PKIX_CHECK(pkix_pl_PrimHashTable_RemoveFIFO
                        (ht->primHash,
                        hashCode,
                        (void **) &deletedKey,
                        (void **) &deletedValue,
                        plContext),
                        PKIX_PRIMHASHTABLEGETBUCKETSIZEFAILED);
                PKIX_DECREF(deletedKey);
                PKIX_DECREF(deletedValue);
        }

        PKIX_MUTEX_LOCK(ht->tableLock);
        tableLocked = PKIX_TRUE;

        PKIX_CHECK(pkix_pl_PrimHashTable_Add
                (ht->primHash,
                (void *)key,
                (void *)value,
                hashCode,
                keyComp,
                plContext),
                PKIX_PRIMHASHTABLEADDFAILED);

        PKIX_MUTEX_UNLOCK(ht->tableLock);
        tableLocked = PKIX_FALSE;

        PKIX_INCREF(key);
        PKIX_INCREF(value);

        /*
         * we don't call PKIX_PL_InvalidateCache here b/c we have
         * not implemented toString or hashcode for this Object
         */

cleanup:

        if (PKIX_ERROR_RECEIVED && tableLocked){
                PKIX_MUTEX_UNLOCK(ht->tableLock);
        }

        PKIX_RETURN(HASHTABLE);
}

/*
 * FUNCTION: PKIX_PL_HashTable_Remove (see comments in pkix_pl_system.h)
 */
PKIX_Error *
PKIX_PL_HashTable_Remove(
        PKIX_PL_HashTable *ht,
        PKIX_PL_Object *key,
        void *plContext)
{
        PKIX_PL_Object *result = NULL;
        PKIX_UInt32 hashCode;
        PKIX_PL_EqualsCallback keyComp;
        PKIX_Boolean tableLocked = PKIX_FALSE;

        PKIX_ENTER(HASHTABLE, "PKIX_PL_HashTable_Remove");
        PKIX_NULLCHECK_TWO(ht, key);

        PKIX_CHECK(PKIX_PL_Object_Hashcode(key, &hashCode, plContext),
                    PKIX_OBJECTHASHCODEFAILED);

        PKIX_CHECK(pkix_pl_Object_RetrieveEqualsCallback
                    (key, &keyComp, plContext),
                    PKIX_OBJECTRETRIEVEEQUALSCALLBACKFAILED);

        PKIX_MUTEX_LOCK(ht->tableLock);
        tableLocked = PKIX_TRUE;

        /* Remove from primitive hashtable */
        PKIX_CHECK(pkix_pl_PrimHashTable_Remove
                (ht->primHash,
                (void *)key,
                hashCode,
                keyComp,
                (void **)&result,
                plContext),
                PKIX_PRIMHASHTABLEREMOVEFAILED);

        PKIX_MUTEX_UNLOCK(ht->tableLock);
        tableLocked = PKIX_FALSE;

        if (result != NULL) {
                PKIX_DECREF(key);
                PKIX_DECREF(result);
        } else {
                PKIX_ERROR(PKIX_ATTEMPTTOREMOVENONEXISTANTITEM);
        }

        /*
         * we don't call PKIX_PL_InvalidateCache here b/c we have
         * not implemented toString or hashcode for this Object
         */

cleanup:

        if (PKIX_ERROR_RECEIVED && tableLocked){
                PKIX_MUTEX_UNLOCK(ht->tableLock);
        }

        PKIX_RETURN(HASHTABLE);
}

/*
 * FUNCTION: PKIX_PL_HashTable_Lookup (see comments in pkix_pl_system.h)
 */
PKIX_Error *
PKIX_PL_HashTable_Lookup(
        PKIX_PL_HashTable *ht,
        PKIX_PL_Object *key,
        PKIX_PL_Object **pResult,
        void *plContext)
{
        PKIX_UInt32 hashCode;
        PKIX_PL_EqualsCallback keyComp;
        PKIX_PL_Object *result = NULL;
        PKIX_Boolean tableLocked = PKIX_FALSE;

        PKIX_ENTER(HASHTABLE, "PKIX_PL_HashTable_Lookup");
        PKIX_NULLCHECK_THREE(ht, key, pResult);

        PKIX_CHECK(PKIX_PL_Object_Hashcode(key, &hashCode, plContext),
                    PKIX_OBJECTHASHCODEFAILED);

        PKIX_CHECK(pkix_pl_Object_RetrieveEqualsCallback
                    (key, &keyComp, plContext),
                    PKIX_OBJECTRETRIEVEEQUALSCALLBACKFAILED);

        PKIX_MUTEX_LOCK(ht->tableLock);
        tableLocked = PKIX_TRUE;

        /* Lookup in primitive hashtable */
        PKIX_CHECK(pkix_pl_PrimHashTable_Lookup
                (ht->primHash,
                (void *)key,
                hashCode,
                keyComp,
                (void **)&result,
                plContext),
                PKIX_PRIMHASHTABLELOOKUPFAILED);

        PKIX_MUTEX_UNLOCK(ht->tableLock);
        tableLocked = PKIX_FALSE;

        if (result != NULL) {
                PKIX_INCREF(result);
        }
        *pResult = result;

cleanup:

        if (PKIX_ERROR_RECEIVED && tableLocked){
                PKIX_MUTEX_UNLOCK(ht->tableLock);
        }

        PKIX_RETURN(HASHTABLE);
}
