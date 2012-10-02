/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * pkix_pl_primhash.c
 *
 * Primitive (non-object) Hashtable Functions
 *
 */

#include "pkix_pl_primhash.h"

/* --Private-Functions---------------------------------------- */

/*
 * FUNCTION: pkix_pl_KeyComparator_Default
 * DESCRIPTION:
 *
 *  Compares the integer pointed to by "firstKey" with the integer pointed to
 *  by "secondKey" for equality and stores the Boolean result at "pResult".
 *  This default key comparator assumes each key is a PKIX_UInt32, and it
 *  simply tests them for equality.
 *
 * PARAMETERS:
 *  "firstKey"
 *      Address of the first integer key to compare. Must be non-NULL.
 *      The EqualsCallback for this Object will be called.
 *  "secondKey"
 *      Address of the second integer key to compare. Must be non-NULL.
 *  "pResult"
 *      Address where Boolean will be stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_KeyComparator_Default(
        PKIX_UInt32 *firstKey,
        PKIX_UInt32 *secondKey,
        PKIX_Boolean *pResult,
        void *plContext)
{
        /* Assume both keys are pointers to PKIX_UInt32 */
        PKIX_UInt32 firstInt, secondInt;

        PKIX_ENTER(HASHTABLE, "pkix_pl_KeyComparator_Default");
        PKIX_NULLCHECK_THREE(firstKey, secondKey, pResult);

        firstInt = *firstKey;
        secondInt = *secondKey;

        *pResult = (firstInt == secondInt)?PKIX_TRUE:PKIX_FALSE;

        PKIX_RETURN(HASHTABLE);
}


/*
 * FUNCTION: pkix_pl_PrimHashTable_Create
 * DESCRIPTION:
 *
 *  Creates a new PrimHashtable object with a number of buckets equal to
 *  "numBuckets" and stores the result at "pResult".
 *
 * PARAMETERS:
 *  "numBuckets"
 *      The number of hash table buckets. Must be non-zero.
 *  "pResult"
 *      Address where PrimHashTable pointer will be stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_PrimHashTable_Create(
        PKIX_UInt32 numBuckets,
        pkix_pl_PrimHashTable **pResult,
        void *plContext)
{
        pkix_pl_PrimHashTable *primHashTable = NULL;
        PKIX_UInt32 i;

        PKIX_ENTER(HASHTABLE, "pkix_pl_PrimHashTable_Create");
        PKIX_NULLCHECK_ONE(pResult);

        if (numBuckets == 0) {
                PKIX_ERROR(PKIX_NUMBUCKETSEQUALSZERO);
        }

        /* Allocate a new hashtable */
        PKIX_CHECK(PKIX_PL_Malloc
                    (sizeof (pkix_pl_PrimHashTable),
                    (void **)&primHashTable,
                    plContext),
                    PKIX_MALLOCFAILED);

        primHashTable->size = numBuckets;

        /* Allocate space for the buckets */
        PKIX_CHECK(PKIX_PL_Malloc
                    (numBuckets * sizeof (pkix_pl_HT_Elem*),
                    (void **)&primHashTable->buckets,
                    plContext),
                    PKIX_MALLOCFAILED);

        for (i = 0; i < numBuckets; i++) {
                primHashTable->buckets[i] = NULL;
        }

        *pResult = primHashTable;

cleanup:

        if (PKIX_ERROR_RECEIVED){
                PKIX_FREE(primHashTable);
        }

        PKIX_RETURN(HASHTABLE);
}

/*
 * FUNCTION: pkix_pl_PrimHashTable_Add
 * DESCRIPTION:
 *
 *  Adds the value pointed to by "value" to the PrimHashTable pointed to by
 *  "ht" using the key pointed to by "key" and the hashCode value equal to
 *  "hashCode", using the function pointed to by "keyComp" to compare keys.
 *  Assumes the key is either a PKIX_UInt32 or a PKIX_PL_Object. If the value
 *  already exists in the hashtable, this function returns a non-fatal error.
 *
 * PARAMETERS:
 *  "ht"
 *      Address of PrimHashtable to insert into. Must be non-NULL.
 *  "key"
 *      Address of key. Typically a PKIX_UInt32 or PKIX_PL_Object.
 *      Must be non-NULL.
 *  "value"
 *      Address of Object to be added to PrimHashtable. Must be non-NULL.
 *  "hashCode"
 *      Hashcode value of the key.
 *  "keyComp"
 *      Address of function used to determine if two keys are equal.
 *      If NULL, pkix_pl_KeyComparator_Default is used.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Not Thread Safe - assumes exclusive access to "ht"
 *  (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a HashTable Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_PrimHashTable_Add(
        pkix_pl_PrimHashTable *ht,
        void *key,
        void *value,
        PKIX_UInt32 hashCode,
        PKIX_PL_EqualsCallback keyComp,
        void *plContext)
{
        pkix_pl_HT_Elem **elemPtr = NULL;
        pkix_pl_HT_Elem *element = NULL;
        PKIX_Boolean compResult = PKIX_FALSE;

        PKIX_ENTER(HASHTABLE, "pkix_pl_PrimHashTable_Add");
        PKIX_NULLCHECK_THREE(ht, key, value);

        for (elemPtr = &((ht->buckets)[hashCode%ht->size]), element = *elemPtr;
            element != NULL; elemPtr = &(element->next), element = *elemPtr) {

                if (element->hashCode != hashCode){
                        /* no possibility of a match */
                        continue;
                }

                if (keyComp == NULL){
                        PKIX_CHECK(pkix_pl_KeyComparator_Default
                                ((PKIX_UInt32 *)key,
                                (PKIX_UInt32 *)(element->key),
                                &compResult,
                                plContext),
                                PKIX_COULDNOTTESTWHETHERKEYSEQUAL);
                } else {
                        PKIX_CHECK(keyComp
                                ((PKIX_PL_Object *)key,
                                (PKIX_PL_Object *)(element->key),
                                &compResult,
                                plContext),
                                PKIX_COULDNOTTESTWHETHERKEYSEQUAL);
                }

                if ((element->hashCode == hashCode) &&
                    (compResult == PKIX_TRUE)){
                        /* Same key already exists in the table */
                    PKIX_ERROR(PKIX_ATTEMPTTOADDDUPLICATEKEY);
                }
        }

        /* Next Element should be NULL at this point */
        if (element != NULL) {
                PKIX_ERROR(PKIX_ERRORTRAVERSINGBUCKET);
        }

        /* Create a new HT_Elem */
        PKIX_CHECK(PKIX_PL_Malloc
                    (sizeof (pkix_pl_HT_Elem), (void **)elemPtr, plContext),
                    PKIX_MALLOCFAILED);

        element = *elemPtr;

        element->key = key;
        element->value = value;
        element->hashCode = hashCode;
        element->next = NULL;

cleanup:

        PKIX_RETURN(HASHTABLE);
}

/*
 * FUNCTION: pkix_pl_PrimHashTable_Remove
 * DESCRIPTION:
 *
 *  Removes any objects with the key pointed to by "key" and hashCode value
 *  equal to "hashCode" from the PrimHashtable pointed to by "ht", using the
 *  function pointed to by "keyComp" to compare keys, and stores the object's
 *  value at "pResult". Assumes "key" is a PKIX_UInt32 or a PKIX_PL_Object.
 *  This function sets "pResult" to NULL if the key is not in the hashtable.
 *
 * PARAMETERS:
 *  "ht"
 *      Address of PrimHashtable to remove object. Must be non-NULL.
 *  "key"
 *      Address of key for lookup. Typically a PKIX_UInt32 or PKIX_PL_Object.
 *      Must be non-NULL.
 *  "value"
 *      Address of Object to be added to PrimHashtable. Must be non-NULL.
 *  "hashCode"
 *      Hashcode value of the key.
 *  "keyComp"
 *      Address of function used to determine if two keys are equal.
 *      If NULL, pkix_pl_KeyComparator_Default is used.
 *  "pResult"
 *      Address where value will be stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Not Thread Safe - assumes exclusive access to "ht"
 *  (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a HashTable Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_PrimHashTable_Remove(
        pkix_pl_PrimHashTable *ht,
        void *key,
        PKIX_UInt32 hashCode,
        PKIX_PL_EqualsCallback keyComp,
        void **pKey,
        void **pValue,
        void *plContext)
{
        pkix_pl_HT_Elem *element = NULL;
        pkix_pl_HT_Elem *prior = NULL;
        PKIX_Boolean compResult;

        PKIX_ENTER(HASHTABLE, "pkix_pl_PrimHashTable_Remove");
        PKIX_NULLCHECK_FOUR(ht, key, pKey, pValue);

        *pKey = NULL;
        *pValue = NULL;

        for (element = ht->buckets[hashCode%ht->size], prior = element;
            (element != NULL);
            prior = element, element = element->next) {

                if (element->hashCode != hashCode){
                        /* no possibility of a match */
                        continue;
                }

                if (keyComp == NULL){
                        PKIX_CHECK(pkix_pl_KeyComparator_Default
                                ((PKIX_UInt32 *)key,
                                (PKIX_UInt32 *)(element->key),
                                &compResult,
                                plContext),
                                PKIX_COULDNOTTESTWHETHERKEYSEQUAL);
                } else {
                        PKIX_CHECK(keyComp
                                ((PKIX_PL_Object *)key,
                                (PKIX_PL_Object *)(element->key),
                                &compResult,
                                plContext),
                                PKIX_COULDNOTTESTWHETHERKEYSEQUAL);
                }

                if ((element->hashCode == hashCode) &&
                    (compResult == PKIX_TRUE)){
                        if (element != prior) {
                                prior->next = element->next;
                        } else {
                                ht->buckets[hashCode%ht->size] = element->next;
                        }
                        *pKey = element->key;
                        *pValue = element->value;
                        element->key = NULL;
                        element->value = NULL;
                        element->next = NULL;
                        PKIX_FREE(element);
                        goto cleanup;
                }
        }

cleanup:

        PKIX_RETURN(HASHTABLE);
}


/*
 * FUNCTION: pkix_pl_HashTableLookup
 * DESCRIPTION:
 *
 *  Looks up object using the key pointed to by "key" and hashCode value
 *  equal to "hashCode" from the PrimHashtable pointed to by "ht", using the
 *  function pointed to by "keyComp" to compare keys, and stores the object's
 *  value at "pResult". Assumes "key" is a PKIX_UInt32 or a PKIX_PL_Object.
 *  This function sets "pResult" to NULL if the key is not in the hashtable.
 *
 * PARAMETERS:
 *  "ht"
 *      Address of PrimHashtable to lookup object from. Must be non-NULL.
 *  "key"
 *      Address of key for lookup. Typically a PKIX_UInt32 or PKIX_PL_Object.
 *      Must be non-NULL.
 *  "keyComp"
 *      Address of function used to determine if two keys are equal.
 *      If NULL, pkix_pl_KeyComparator_Default is used.
 *  "hashCode"
 *      Hashcode value of the key.
 *  "pResult"
 *      Address where value will be stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Conditionally Thread Safe
 *      (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_PrimHashTable_Lookup(
        pkix_pl_PrimHashTable *ht,
        void *key,
        PKIX_UInt32 hashCode,
        PKIX_PL_EqualsCallback keyComp,
        void **pResult,
        void *plContext)
{
        pkix_pl_HT_Elem *element = NULL;
        PKIX_Boolean compResult = PKIX_FALSE;

        PKIX_ENTER(HASHTABLE, "pkix_pl_PrimHashTable_Lookup");
        PKIX_NULLCHECK_THREE(ht, key, pResult);

        *pResult = NULL;

        for (element = (ht->buckets)[hashCode%ht->size];
            (element != NULL) && (*pResult == NULL);
            element = element->next) {

                if (element->hashCode != hashCode){
                        /* no possibility of a match */
                        continue;
                }

                if (keyComp == NULL){
                        PKIX_CHECK(pkix_pl_KeyComparator_Default
                                ((PKIX_UInt32 *)key,
                                (PKIX_UInt32 *)(element->key),
                                &compResult,
                                plContext),
                                PKIX_COULDNOTTESTWHETHERKEYSEQUAL);
                } else {
                       pkixErrorResult =
                           keyComp((PKIX_PL_Object *)key,
                                   (PKIX_PL_Object *)(element->key),
                                   &compResult,
                                   plContext);
                       if (pkixErrorResult) {
                           pkixErrorClass = PKIX_FATAL_ERROR;
                           pkixErrorCode = PKIX_COULDNOTTESTWHETHERKEYSEQUAL;
                           goto cleanup;
                       }
                }

                if ((element->hashCode == hashCode) &&
                    (compResult == PKIX_TRUE)){
                        *pResult = element->value;
                        goto cleanup;
                }
        }

        /* if we've reached here, specified key doesn't exist in hashtable */
        *pResult = NULL;

cleanup:

        PKIX_RETURN(HASHTABLE);
}

/*
 * FUNCTION: pkix_pl_PrimHashTable_Destroy
 *
 *  Destroys PrimHashTable pointed to by "ht".
 *
 * PARAMETERS:
 *  "ht"
 *      Address of PrimHashtable to free. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Not Thread Safe - assumes exclusive access to "ht"
 *  (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS
 *  Returns NULL if the function succeeds.
 *  Returns a HashTable Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_PrimHashTable_Destroy(
        pkix_pl_PrimHashTable *ht,
        void *plContext)
{
        pkix_pl_HT_Elem *element = NULL;
        pkix_pl_HT_Elem *temp = NULL;
        PKIX_UInt32 i;

        PKIX_ENTER(HASHTABLE, "pkix_pl_PrimHashTable_Destroy");
        PKIX_NULLCHECK_ONE(ht);

        /* Free each element (list) */
        for (i = 0; i < ht->size; i++) {
                for (element = ht->buckets[i];
                    element != NULL;
                    element = temp) {
                        temp = element->next;
                        element->value = NULL;
                        element->key = NULL;
                        element->hashCode = 0;
                        element->next = NULL;
                        PKIX_FREE(element);
                }
        }

        /* Free the pointer to the list array */
        PKIX_FREE(ht->buckets);
        ht->size = 0;

        /* Free the table itself */
        PKIX_FREE(ht);

        PKIX_RETURN(HASHTABLE);
}

/*
 * FUNCTION: pkix_pl_PrimHashTable_GetBucketSize
 * DESCRIPTION:
 *
 *  Retruns number of entries in the bucket the "hashCode" is designated in
 *  the hashtable "ht" in the address "pBucketSize".
 *
 * PARAMETERS:
 *  "ht"
 *      Address of PrimHashtable to get entries count. Must be non-NULL.
 *  "hashCode"
 *      Hashcode value of the key.
 *  "pBucketSize"
 *      Address that an PKIX_UInt32 is returned for number of entries in the
 *      bucket associated with the hashCode. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Not Thread Safe - assumes exclusive access to "ht"
 *  (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a HashTable Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_PrimHashTable_GetBucketSize(
        pkix_pl_PrimHashTable *ht,
        PKIX_UInt32 hashCode,
        PKIX_UInt32 *pBucketSize,
        void *plContext)
{
        pkix_pl_HT_Elem **elemPtr = NULL;
        pkix_pl_HT_Elem *element = NULL;
        PKIX_UInt32 bucketSize = 0;

        PKIX_ENTER(HASHTABLE, "pkix_pl_PrimHashTable_GetBucketSize");
        PKIX_NULLCHECK_TWO(ht, pBucketSize);

        for (elemPtr = &((ht->buckets)[hashCode%ht->size]), element = *elemPtr;
            element != NULL; elemPtr = &(element->next), element = *elemPtr) {
                bucketSize++;
	}

        *pBucketSize = bucketSize;

        PKIX_RETURN(HASHTABLE);
}

/*
 * FUNCTION: pkix_pl_PrimHashTable_RemoveFIFO
 * DESCRIPTION:
 *
 *  Remove the first entry in the bucket the "hashCode" is designated in
 *  the hashtable "ht". Since new entry is added at end of the link list
 *  the first one is the oldest (FI) therefore removed first (FO).
 *
 * PARAMETERS:
 *  "ht"
 *      Address of PrimHashtable to get entries count. Must be non-NULL.
 *  "hashCode"
 *      Hashcode value of the key.
 *  "pKey"
 *      Address of key of the entry deleted. Must be non-NULL.
 *  "pValue"
 *      Address of Value of the entry deleted. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Not Thread Safe - assumes exclusive access to "ht"
 *  (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a HashTable Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_PrimHashTable_RemoveFIFO(
        pkix_pl_PrimHashTable *ht,
        PKIX_UInt32 hashCode,
        void **pKey,
        void **pValue,
        void *plContext)
{
        pkix_pl_HT_Elem *element = NULL;

        PKIX_ENTER(HASHTABLE, "pkix_pl_PrimHashTable_Remove");
        PKIX_NULLCHECK_THREE(ht, pKey, pValue);

        element = (ht->buckets)[hashCode%ht->size];

        if (element != NULL) {

                *pKey = element->key;
                *pValue = element->value;
                ht->buckets[hashCode%ht->size] = element->next;
                element->key = NULL;
                element->value = NULL;
                element->next = NULL;
                PKIX_FREE(element);
        }

        PKIX_RETURN(HASHTABLE);
}
