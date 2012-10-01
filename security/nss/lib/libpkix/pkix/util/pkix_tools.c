/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * pkix_tools.c
 *
 * Private Utility Functions
 *
 */

#include "pkix_tools.h"

#define CACHE_ITEM_PERIOD_SECONDS  (3600)  /* one hour */

/*
 * This cahce period is only for CertCache. A Cert from a trusted CertStore
 * should be checked more frequently for update new arrival, etc.
 */
#define CACHE_TRUST_ITEM_PERIOD_SECONDS  (CACHE_ITEM_PERIOD_SECONDS/10)

extern PKIX_PL_HashTable *cachedCertChainTable;
extern PKIX_PL_HashTable *cachedCertTable;
extern PKIX_PL_HashTable *cachedCrlEntryTable;

/* Following variables are used to checked cache hits - can be taken out */
extern int pkix_ccAddCount;
extern int pkix_ccLookupCount;
extern int pkix_ccRemoveCount;
extern int pkix_cAddCount;
extern int pkix_cLookupCount;
extern int pkix_cRemoveCount;
extern int pkix_ceAddCount;
extern int pkix_ceLookupCount;

#ifdef PKIX_OBJECT_LEAK_TEST
/* Following variables are used for object leak test */
char *nonNullValue = "Non Empty Value";
PKIX_Boolean noErrorState = PKIX_TRUE;
PKIX_Boolean runningLeakTest;
PKIX_Boolean errorGenerated;
PKIX_UInt32 stackPosition;
PKIX_UInt32 *fnStackInvCountArr;
char **fnStackNameArr;
PLHashTable *fnInvTable;
PKIX_UInt32 testStartFnStackPosition;
char *errorFnStackString;
#endif /* PKIX_OBJECT_LEAK_TEST */

/* --Private-Functions-------------------------------------------- */

#ifdef PKIX_OBJECT_LEAK_TEST
/*
 * FUNCTION: pkix_ErrorGen_Hash
 * DESCRIPTION:
 *
 * Hash function to be used in object leak test hash table.
 *
 */
PLHashNumber PR_CALLBACK
pkix_ErrorGen_Hash (const void *key)
{
    char *str = NULL;
    PLHashNumber rv = (*(PRUint8*)key) << 5;
    PRUint32 i, counter = 0;
    PRUint8 *rvc = (PRUint8 *)&rv;

    while ((str = fnStackNameArr[counter++]) != NULL) {
        PRUint32 len = strlen(str);
        for( i = 0; i < len; i++ ) {
            rvc[ i % sizeof(rv) ] ^= *str;
            str++;
        }
    }

    return rv;
}

#endif /* PKIX_OBJECT_LEAK_TEST */

/*
 * FUNCTION: pkix_IsCertSelfIssued
 * DESCRIPTION:
 *
 *  Checks whether the Cert pointed to by "cert" is self-issued and stores the
 *  Boolean result at "pSelfIssued". A Cert is considered self-issued if the
 *  Cert's issuer matches the Cert's subject. If the subject or issuer is
 *  not specified, a PKIX_FALSE is returned.
 *
 * PARAMETERS:
 *  "cert"
 *      Address of Cert used to determine whether Cert is self-issued.
 *      Must be non-NULL.
 *  "pSelfIssued"
 *      Address where Boolean will be stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Cert Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_IsCertSelfIssued(
        PKIX_PL_Cert *cert,
        PKIX_Boolean *pSelfIssued,
        void *plContext)
{
        PKIX_PL_X500Name *subject = NULL;
        PKIX_PL_X500Name *issuer = NULL;

        PKIX_ENTER(CERT, "pkix_IsCertSelfIssued");
        PKIX_NULLCHECK_TWO(cert, pSelfIssued);

        PKIX_CHECK(PKIX_PL_Cert_GetSubject(cert, &subject, plContext),
                    PKIX_CERTGETSUBJECTFAILED);

        PKIX_CHECK(PKIX_PL_Cert_GetIssuer(cert, &issuer, plContext),
                    PKIX_CERTGETISSUERFAILED);

        if (subject == NULL || issuer == NULL) {
                *pSelfIssued = PKIX_FALSE;
        } else {

                PKIX_CHECK(PKIX_PL_X500Name_Match
                    (subject, issuer, pSelfIssued, plContext),
                    PKIX_X500NAMEMATCHFAILED);
        }

cleanup:
        PKIX_DECREF(subject);
        PKIX_DECREF(issuer);

        PKIX_RETURN(CERT);
}

/*
 * FUNCTION: pkix_Throw
 * DESCRIPTION:
 *
 *  Creates an Error using the value of "errorCode", the character array
 *  pointed to by "funcName", the character array pointed to by "errorText",
 *  and the Error pointed to by "cause" (if any), and stores it at "pError".
 *
 *  If "cause" is not NULL and has an errorCode of "PKIX_FATAL_ERROR",
 *  then there is no point creating a new Error object. Rather, we simply
 *  store "cause" at "pError".
 *
 * PARAMETERS:
 *  "errorCode"
 *      Value of error code.
 *  "funcName"
 *      Address of EscASCII array representing name of function throwing error.
 *      Must be non-NULL.
 *  "errnum"
 *      PKIX_ERRMSGNUM of error description for new error.
 *  "cause"
 *      Address of Error representing error's cause.
 *  "pError"
 *      Address where object pointer will be stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns an Error Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_Throw(
        PKIX_ERRORCLASS errorClass,
        const char *funcName,
        PKIX_ERRORCODE errorCode,
        PKIX_ERRORCLASS overrideClass,
        PKIX_Error *cause,
        PKIX_Error **pError,
        void *plContext)
{
        PKIX_Error *error = NULL;

        PKIX_ENTER(ERROR, "pkix_Throw");
        PKIX_NULLCHECK_TWO(funcName, pError);

        *pError = NULL;

#ifdef PKIX_OBJECT_LEAK_TEST        
        noErrorState = PKIX_TRUE;
        if (pkixLog) {
#ifdef PKIX_ERROR_DESCRIPTION            
            PR_LOG(pkixLog, 4, ("Error in function \"%s\":\"%s\" with cause \"%s\"\n",
                                funcName, PKIX_ErrorText[errorCode],
                                (cause ? PKIX_ErrorText[cause->errCode] : "null")));
#else
            PR_LOG(pkixLog, 4, ("Error in function \"%s\": error code \"%d\"\n",
                                funcName, errorCode));
#endif /* PKIX_ERROR_DESCRIPTION */
            PORT_Assert(strcmp(funcName, "PKIX_PL_Object_DecRef"));
        }
#endif /* PKIX_OBJECT_LEAK_TEST */

        /* if cause has error class of PKIX_FATAL_ERROR, return immediately */
        if (cause) {
                if (cause->errClass == PKIX_FATAL_ERROR){
                        PKIX_INCREF(cause);
                        *pError = cause;
                        goto cleanup;
                }
        }
        
        if (overrideClass == PKIX_FATAL_ERROR){
                errorClass = overrideClass;
        }

       pkixTempResult = PKIX_Error_Create(errorClass, cause, NULL,
                                           errorCode, &error, plContext);
       
       if (!pkixTempResult) {
           /* Setting plErr error code:
            *    get it from PORT_GetError if it is a leaf error and
            *    default error code does not exist(eq 0)               */
           if (!cause && !error->plErr) {
               error->plErr = PKIX_PL_GetPLErrorCode();
           }
       }

       *pError = error;

cleanup:

        PKIX_DEBUG_EXIT(ERROR);
        pkixErrorClass = 0;
#ifdef PKIX_OBJECT_LEAK_TEST        
        noErrorState = PKIX_FALSE;

        if (runningLeakTest && fnStackNameArr) {
            PR_LOG(pkixLog, 5,
                   ("%s%*s<- %s(%d) - %s\n", (errorGenerated ? "*" : " "),
                    stackPosition, " ", fnStackNameArr[stackPosition],
                    stackPosition, myFuncName));
            fnStackNameArr[stackPosition--] = NULL;
        }
#endif /* PKIX_OBJECT_LEAK_TEST */
        return (pkixTempResult);
}

/*
 * FUNCTION: pkix_CheckTypes
 * DESCRIPTION:
 *
 *  Checks that the types of the Object pointed to by "first" and the Object
 *  pointed to by "second" are both equal to the value of "type". If they
 *  are not equal, a PKIX_Error is returned.
 *
 * PARAMETERS:
 *  "first"
 *      Address of first Object. Must be non-NULL.
 *  "second"
 *      Address of second Object. Must be non-NULL.
 *  "type"
 *      Value of type to check against.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns an Error Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_CheckTypes(
        PKIX_PL_Object *first,
        PKIX_PL_Object *second,
        PKIX_UInt32 type,
        void *plContext)
{
        PKIX_UInt32 firstType, secondType;

        PKIX_ENTER(OBJECT, "pkix_CheckTypes");
        PKIX_NULLCHECK_TWO(first, second);

        PKIX_CHECK(PKIX_PL_Object_GetType(first, &firstType, plContext),
                    PKIX_COULDNOTGETFIRSTOBJECTTYPE);

        PKIX_CHECK(PKIX_PL_Object_GetType(second, &secondType, plContext),
                    PKIX_COULDNOTGETSECONDOBJECTTYPE);

        if ((firstType != type)||(firstType != secondType)) {
                PKIX_ERROR(PKIX_OBJECTTYPESDONOTMATCH);
        }

cleanup:

        PKIX_RETURN(OBJECT);
}

/*
 * FUNCTION: pkix_CheckType
 * DESCRIPTION:
 *
 *  Checks that the type of the Object pointed to by "object" is equal to the
 *  value of "type". If it is not equal, a PKIX_Error is returned.
 *
 * PARAMETERS:
 *  "object"
 *      Address of Object. Must be non-NULL.
 *  "type"
 *      Value of type to check against.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns an Error Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_CheckType(
        PKIX_PL_Object *object,
        PKIX_UInt32 type,
        void *plContext)
{
        return (pkix_CheckTypes(object, object, type, plContext));
}

/*
 * FUNCTION: pkix_hash
 * DESCRIPTION:
 *
 *  Computes a hash value for "length" bytes starting at the array of bytes
 *  pointed to by "bytes" and stores the result at "pHash".
 *
 *  XXX To speed this up, we could probably read 32 bits at a time from
 *  bytes (maybe even 64 bits on some platforms)
 *
 * PARAMETERS:
 *  "bytes"
 *      Address of array of bytes to hash. Must be non-NULL.
 *  "length"
 *      Number of bytes to hash.
 *  "pHash"
 *      Address where object pointer will be stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_hash(
        const unsigned char *bytes,
        PKIX_UInt32 length,
        PKIX_UInt32 *pHash,
        void *plContext)
{
        PKIX_UInt32 i;
        PKIX_UInt32 hash;

        PKIX_ENTER(OBJECT, "pkix_hash");
        if (length != 0) {
                PKIX_NULLCHECK_ONE(bytes);
        }
        PKIX_NULLCHECK_ONE(pHash);

        hash = 0;
        for (i = 0; i < length; i++) {
                /* hash = 31 * hash + bytes[i]; */
                hash = (hash << 5) - hash + bytes[i];
        }

        *pHash = hash;

        PKIX_RETURN(OBJECT);
}

/*
 * FUNCTION: pkix_countArray
 * DESCRIPTION:
 *
 *  Counts the number of elements in the  null-terminated array of pointers
 *  pointed to by "array" and returns the result.
 *
 * PARAMETERS
 *  "array"
 *      Address of null-terminated array of pointers.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns the number of elements in the array.
 */
PKIX_UInt32
pkix_countArray(void **array)
{
        PKIX_UInt32 count = 0;

        if (array) {
                while (*array++) {
                        count++;
                }
        }
        return (count);
}

/*
 * FUNCTION: pkix_duplicateImmutable
 * DESCRIPTION:
 *
 *  Convenience callback function used for duplicating immutable objects.
 *  Since the objects can not be modified, this function simply increments the
 *  reference count on the object, and returns a reference to that object.
 *
 *  (see comments for PKIX_PL_DuplicateCallback in pkix_pl_system.h)
 */
PKIX_Error *
pkix_duplicateImmutable(
        PKIX_PL_Object *object,
        PKIX_PL_Object **pNewObject,
        void *plContext)
{
        PKIX_ENTER(OBJECT, "pkix_duplicateImmutable");
        PKIX_NULLCHECK_TWO(object, pNewObject);

        PKIX_INCREF(object);

        *pNewObject = object;

cleanup:
        PKIX_RETURN(OBJECT);
}

/* --String-Encoding-Conversion-Functions------------------------ */

/*
 * FUNCTION: pkix_hex2i
 * DESCRIPTION:
 *
 *  Converts hexadecimal character "c" to its integer value and returns result.
 *
 * PARAMETERS
 *  "c"
 *      Character to convert to a hex value.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  The hexadecimal value of "c". Otherwise -1. (Unsigned 0xFFFFFFFF).
 */
PKIX_UInt32
pkix_hex2i(char c)
{
        if ((c >= '0')&&(c <= '9'))
                return (c-'0');
        else if ((c >= 'a')&&(c <= 'f'))
                return (c-'a'+10);
        else if ((c >= 'A')&&(c <= 'F'))
                return (c-'A'+10);
        else
                return ((PKIX_UInt32)(-1));
}

/*
 * FUNCTION: pkix_i2hex
 * DESCRIPTION:
 *
 *  Converts integer value "digit" to its ASCII hex value
 *
 * PARAMETERS
 *  "digit"
 *      Value of integer to convert to ASCII hex value. Must be 0-15.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  The ASCII hexadecimal value of "digit".
 */
char
pkix_i2hex(char digit)
{
        if ((digit >= 0)&&(digit <= 9))
                return (digit+'0');
        else if ((digit >= 0xa)&&(digit <= 0xf))
                return (digit - 10 + 'a');
        else
                return (-1);
}

/*
 * FUNCTION: pkix_isPlaintext
 * DESCRIPTION:
 *
 *  Returns whether character "c" is plaintext using EscASCII or EscASCII_Debug
 *  depending on the value of "debug".
 *
 *  In EscASCII, [01, 7E] except '&' are plaintext.
 *  In EscASCII_Debug [20, 7E] except '&' are plaintext.
 *
 * PARAMETERS:
 *  "c"
 *      Character to check.
 *  "debug"
 *      Value of debug flag.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  True if "c" is plaintext.
 */
PKIX_Boolean
pkix_isPlaintext(unsigned char c, PKIX_Boolean debug) {
        return ((c >= 0x01)&&(c <= 0x7E)&&(c != '&')&&(!debug || (c >= 20)));
}

/* --Cache-Functions------------------------ */

/*
 * FUNCTION: pkix_CacheCertChain_Lookup
 * DESCRIPTION:
 *
 *  Look up CertChain Hash Table for a cached BuildResult based on "targetCert"
 *  and "anchors" as the hash keys. If there is no item to match the key,
 *  PKIX_FALSE is stored at "pFound". If an item is found, its cache time is
 *  compared to "testDate". If expired, the item is removed and PKIX_FALSE is
 *  stored at "pFound". Otherwise, PKIX_TRUE is stored at "pFound" and the 
 *  BuildResult is stored at "pBuildResult".
 *  The hashtable is maintained in the following ways:
 *  1) When creating the hashtable, maximum bucket size can be specified (0 for
 *     unlimited). If items in a bucket reaches its full size, an new addition
 *     will trigger the removal of the old as FIFO sequence.
 *  2) A PKIX_PL_Date created with current time offset by constant 
 *     CACHE_ITEM_PERIOD_SECONDS is attached to each item in the Hash Table.
 *     When an item is retrieved, this date is compared against "testDate" for
 *     validity. If comparison indicates this item is expired, the item is
 *     removed from the bucket.
 *
 * PARAMETERS:
 *  "targetCert"
 *      Address of Target Cert as key to retrieve this CertChain. Must be 
 *      non-NULL.
 *  "anchors"
 *      Address of PKIX_List of "anchors" is used as key to retrive CertChain.
 *      Must be non-NULL.
 *  "testDate"
 *      Address of PKIX_PL_Date for verifying time validity and cache validity.
 *      May be NULL. If testDate is NULL, this cache item will not be out-dated.
 *  "pFound"
 *      Address of PKIX_Boolean indicating valid data is found.
 *      Must be non-NULL.
 *  "pBuildResult"
 *      Address where BuildResult will be stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns an Error Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_CacheCertChain_Lookup(
        PKIX_PL_Cert* targetCert,
        PKIX_List* anchors,
        PKIX_PL_Date *testDate,
        PKIX_Boolean *pFound,
        PKIX_BuildResult **pBuildResult,
        void *plContext)
{
        PKIX_List *cachedValues = NULL;
        PKIX_List *cachedKeys = NULL;
        PKIX_Error *cachedCertChainError = NULL;
        PKIX_PL_Date *cacheValidUntilDate = NULL;
        PKIX_PL_Date *validityDate = NULL;
        PKIX_Int32 cmpValidTimeResult = 0;
        PKIX_Int32 cmpCacheTimeResult = 0;

        PKIX_ENTER(BUILD, "pkix_CacheCertChain_Lookup");

        PKIX_NULLCHECK_FOUR(targetCert, anchors, pFound, pBuildResult);

        *pFound = PKIX_FALSE;

        /* use trust anchors and target cert as hash key */

        PKIX_CHECK(PKIX_List_Create(&cachedKeys, plContext),
                    PKIX_LISTCREATEFAILED);

        PKIX_CHECK(PKIX_List_AppendItem
                    (cachedKeys,
                    (PKIX_PL_Object *)targetCert,
                    plContext),
                    PKIX_LISTAPPENDITEMFAILED);

        PKIX_CHECK(PKIX_List_AppendItem
                    (cachedKeys,
                    (PKIX_PL_Object *)anchors,
                    plContext),
                    PKIX_LISTAPPENDITEMFAILED);

        cachedCertChainError = PKIX_PL_HashTable_Lookup
                    (cachedCertChainTable,
                    (PKIX_PL_Object *) cachedKeys,
                    (PKIX_PL_Object **) &cachedValues,
                    plContext);

        pkix_ccLookupCount++;

        /* retrieve data from hashed value list */

        if (cachedValues != NULL && cachedCertChainError == NULL) {

            PKIX_CHECK(PKIX_List_GetItem
                    (cachedValues,
                    0,
                    (PKIX_PL_Object **) &cacheValidUntilDate,
                    plContext),
                    PKIX_LISTGETITEMFAILED);

            /* check validity time and cache age time */
            PKIX_CHECK(PKIX_List_GetItem
                    (cachedValues,
                    1,
                    (PKIX_PL_Object **) &validityDate,
                    plContext),
                    PKIX_LISTGETITEMFAILED);

            /* if testDate is not set, this cache item is not out-dated */
            if (testDate) {

                PKIX_CHECK(PKIX_PL_Object_Compare
                     ((PKIX_PL_Object *)testDate,
                     (PKIX_PL_Object *)cacheValidUntilDate,
                     &cmpCacheTimeResult,
                     plContext),
                     PKIX_OBJECTCOMPARATORFAILED);

                PKIX_CHECK(PKIX_PL_Object_Compare
                     ((PKIX_PL_Object *)testDate,
                     (PKIX_PL_Object *)validityDate,
                     &cmpValidTimeResult,
                     plContext),
                     PKIX_OBJECTCOMPARATORFAILED);
            }

            /* certs' date are all valid and cache item is not old */
            if (cmpValidTimeResult <= 0 && cmpCacheTimeResult <=0) {

                PKIX_CHECK(PKIX_List_GetItem
                    (cachedValues,
                    2,
                    (PKIX_PL_Object **) pBuildResult,
                    plContext),
                    PKIX_LISTGETITEMFAILED);

                *pFound = PKIX_TRUE;

            } else {

                pkix_ccRemoveCount++;
                *pFound = PKIX_FALSE;

                /* out-dated item, remove it from cache */
                PKIX_CHECK(PKIX_PL_HashTable_Remove
                    (cachedCertChainTable,
                    (PKIX_PL_Object *) cachedKeys,
                    plContext),
                    PKIX_HASHTABLEREMOVEFAILED);
            }
        }

cleanup:

        PKIX_DECREF(cachedValues);
        PKIX_DECREF(cachedKeys);
        PKIX_DECREF(cachedCertChainError);
        PKIX_DECREF(cacheValidUntilDate);
        PKIX_DECREF(validityDate);

        PKIX_RETURN(BUILD);

}

/*
 * FUNCTION: pkix_CacheCertChain_Remove
 * DESCRIPTION:
 *
 *  Remove CertChain Hash Table entry based on "targetCert" and "anchors"
 *  as the hash keys. If there is no item to match the key, no action is
 *  taken.
 *  The hashtable is maintained in the following ways:
 *  1) When creating the hashtable, maximum bucket size can be specified (0 for
 *     unlimited). If items in a bucket reaches its full size, an new addition
 *     will trigger the removal of the old as FIFO sequence.
 *  2) A PKIX_PL_Date created with current time offset by constant 
 *     CACHE_ITEM_PERIOD_SECONDS is attached to each item in the Hash Table.
 *     When an item is retrieved, this date is compared against "testDate" for
 *     validity. If comparison indicates this item is expired, the item is
 *     removed from the bucket.
 *
 * PARAMETERS:
 *  "targetCert"
 *      Address of Target Cert as key to retrieve this CertChain. Must be 
 *      non-NULL.
 *  "anchors"
 *      Address of PKIX_List of "anchors" is used as key to retrive CertChain.
 *      Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns an Error Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_CacheCertChain_Remove(
        PKIX_PL_Cert* targetCert,
        PKIX_List* anchors,
        void *plContext)
{
        PKIX_List *cachedKeys = NULL;

        PKIX_ENTER(BUILD, "pkix_CacheCertChain_Remove");
        PKIX_NULLCHECK_TWO(targetCert, anchors);

        /* use trust anchors and target cert as hash key */

        PKIX_CHECK(PKIX_List_Create(&cachedKeys, plContext),
                    PKIX_LISTCREATEFAILED);

        PKIX_CHECK(PKIX_List_AppendItem
                    (cachedKeys,
                    (PKIX_PL_Object *)targetCert,
                    plContext),
                    PKIX_LISTAPPENDITEMFAILED);

        PKIX_CHECK(PKIX_List_AppendItem
                    (cachedKeys,
                    (PKIX_PL_Object *)anchors,
                    plContext),
                    PKIX_LISTAPPENDITEMFAILED);

        PKIX_CHECK_ONLY_FATAL(PKIX_PL_HashTable_Remove
                    (cachedCertChainTable,
                    (PKIX_PL_Object *) cachedKeys,
                    plContext),
                    PKIX_HASHTABLEREMOVEFAILED);

        pkix_ccRemoveCount++;

cleanup:

        PKIX_DECREF(cachedKeys);

        PKIX_RETURN(BUILD);

}

/*
 * FUNCTION: pkix_CacheCertChain_Add
 * DESCRIPTION:
 *
 *  Add a BuildResult to the CertChain Hash Table for a "buildResult" with
 *  "targetCert" and "anchors" as the hash keys.
 *  "validityDate" is the most restricted notAfter date of all Certs in
 *  this CertChain and is verified when this BuildChain is retrieved.
 *  The hashtable is maintained in the following ways:
 *  1) When creating the hashtable, maximum bucket size can be specified (0 for
 *     unlimited). If items in a bucket reaches its full size, an new addition
 *     will trigger the removal of the old as FIFO sequence.
 *  2) A PKIX_PL_Date created with current time offset by constant 
 *     CACHE_ITEM_PERIOD_SECONDS is attached to each item in the Hash Table.
 *     When an item is retrieved, this date is compared against "testDate" for
 *     validity. If comparison indicates this item is expired, the item is
 *     removed from the bucket.
 *
 * PARAMETERS:
 *  "targetCert"
 *      Address of Target Cert as key to retrieve this CertChain. Must be 
 *      non-NULL.
 *  "anchors"
 *      Address of PKIX_List of "anchors" is used as key to retrive CertChain.
 *      Must be non-NULL.
 *  "validityDate"
 *      Address of PKIX_PL_Date contains the most restriced notAfter time of
 *      all "certs". Must be non-NULL.
 *      Address of PKIX_Boolean indicating valid data is found.
 *      Must be non-NULL.
 *  "buildResult"
 *      Address of BuildResult to be cached. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns an Error Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_CacheCertChain_Add(
        PKIX_PL_Cert* targetCert,
        PKIX_List* anchors,
        PKIX_PL_Date *validityDate,
        PKIX_BuildResult *buildResult,
        void *plContext)
{
        PKIX_List *cachedValues = NULL;
        PKIX_List *cachedKeys = NULL;
        PKIX_Error *cachedCertChainError = NULL;
        PKIX_PL_Date *cacheValidUntilDate = NULL;

        PKIX_ENTER(BUILD, "pkix_CacheCertChain_Add");

        PKIX_NULLCHECK_FOUR(targetCert, anchors, validityDate, buildResult);

        PKIX_CHECK(PKIX_List_Create(&cachedKeys, plContext),
                PKIX_LISTCREATEFAILED);

        PKIX_CHECK(PKIX_List_AppendItem
                (cachedKeys, (PKIX_PL_Object *)targetCert, plContext),
                PKIX_LISTAPPENDITEMFAILED);

        PKIX_CHECK(PKIX_List_AppendItem
                (cachedKeys, (PKIX_PL_Object *)anchors, plContext),
                PKIX_LISTAPPENDITEMFAILED);

        PKIX_CHECK(PKIX_List_Create(&cachedValues, plContext),
                PKIX_LISTCREATEFAILED);

        PKIX_CHECK(PKIX_PL_Date_Create_CurrentOffBySeconds
                (CACHE_ITEM_PERIOD_SECONDS,
                &cacheValidUntilDate,
                plContext),
               PKIX_DATECREATECURRENTOFFBYSECONDSFAILED);

        PKIX_CHECK(PKIX_List_AppendItem
                (cachedValues,
                (PKIX_PL_Object *)cacheValidUntilDate,
                plContext),
                PKIX_LISTAPPENDITEMFAILED);

        PKIX_CHECK(PKIX_List_AppendItem
                (cachedValues, (PKIX_PL_Object *)validityDate, plContext),
                PKIX_LISTAPPENDITEMFAILED);

        PKIX_CHECK(PKIX_List_AppendItem
                (cachedValues, (PKIX_PL_Object *)buildResult, plContext),
                PKIX_LISTAPPENDITEMFAILED);

        cachedCertChainError = PKIX_PL_HashTable_Add
                (cachedCertChainTable,
                (PKIX_PL_Object *) cachedKeys,
                (PKIX_PL_Object *) cachedValues,
                plContext);

        pkix_ccAddCount++;

        if (cachedCertChainError != NULL) {
                PKIX_DEBUG("PKIX_PL_HashTable_Add for CertChain skipped: "
                        "entry existed\n");
        }

cleanup:

        PKIX_DECREF(cachedValues);
        PKIX_DECREF(cachedKeys);
        PKIX_DECREF(cachedCertChainError);
        PKIX_DECREF(cacheValidUntilDate);

        PKIX_RETURN(BUILD);
}

/*
 * FUNCTION: pkix_CacheCert_Lookup
 * DESCRIPTION:
 *
 *  Look up Cert Hash Table for a cached item based on "store" and Subject in
 *  "certSelParams" as the hash keys and returns values Certs in "pCerts".
 *  If there isn't an item to match the key, a PKIX_FALSE is returned at
 *  "pFound". The item's cache time is verified with "testDate". If out-dated,
 *  this item is removed and PKIX_FALSE is returned at "pFound".
 *  This hashtable is maintained in the following ways:
 *  1) When creating the hashtable, maximum bucket size can be specified (0 for
 *     unlimited). If items in a bucket reaches its full size, an new addition
 *     will trigger the removal of the old as FIFO sequence.
 *  2) A PKIX_PL_Date created with current time offset by constant 
 *     CACHE_ITEM_PERIOD_SECONDS is attached to each item in the Hash Table.
 *     If the CertStore this Cert is from is a trusted one, the cache period is
 *     shorter so cache can be updated more frequently.
 *     When an item is retrieved, this date is compared against "testDate" for
 *     validity. If comparison indicates this item is expired, the item is
 *     removed from the bucket.
 *
 * PARAMETERS:
 *  "store"
 *      Address of CertStore as key to retrieve this CertChain. Must be 
 *      non-NULL.
 *  "certSelParams"
 *      Address of ComCertSelParams that its subject is used as key to retrieve
 *      this CertChain. Must be non-NULL.
 *  "testDate"
 *      Address of PKIX_PL_Date for verifying time cache validity.
 *      Must be non-NULL. If testDate is NULL, this cache item won't be out
 *      dated.
 *  "pFound"
 *      Address of KPKIX_Boolean indicating valid data is found.
 *      Must be non-NULL.
 *  "pCerts"
 *      Address PKIX_List where the CertChain will be stored. Must be no-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns an Error Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_CacheCert_Lookup(
        PKIX_CertStore *store,
        PKIX_ComCertSelParams *certSelParams,
        PKIX_PL_Date *testDate,
        PKIX_Boolean *pFound,
        PKIX_List** pCerts,
        void *plContext)
{
        PKIX_PL_Cert *cert = NULL;
        PKIX_List *cachedKeys = NULL;
        PKIX_List *cachedValues = NULL;
        PKIX_List *cachedCertList = NULL;
        PKIX_List *selCertList = NULL;
        PKIX_PL_X500Name *subject = NULL;
        PKIX_PL_Date *invalidAfterDate = NULL;
        PKIX_PL_Date *cacheValidUntilDate = NULL;
        PKIX_CertSelector *certSel = NULL;
        PKIX_Error *cachedCertError = NULL;
        PKIX_Error *selectorError = NULL;
        PKIX_CertSelector_MatchCallback selectorMatch = NULL;
        PKIX_Int32 cmpValidTimeResult = PKIX_FALSE;
        PKIX_Int32 cmpCacheTimeResult = 0;
        PKIX_UInt32 numItems = 0;
        PKIX_UInt32 i;

        PKIX_ENTER(BUILD, "pkix_CacheCert_Lookup");
        PKIX_NULLCHECK_TWO(store, certSelParams);
        PKIX_NULLCHECK_TWO(pFound, pCerts);

        *pFound = PKIX_FALSE;

        PKIX_CHECK(PKIX_List_Create(&cachedKeys, plContext),
                PKIX_LISTCREATEFAILED);

        PKIX_CHECK(PKIX_List_AppendItem
                (cachedKeys, (PKIX_PL_Object *)store, plContext),
                PKIX_LISTAPPENDITEMFAILED);

        PKIX_CHECK(PKIX_ComCertSelParams_GetSubject
                (certSelParams, &subject, plContext),
                PKIX_COMCERTSELPARAMSGETSUBJECTFAILED);

        PKIX_NULLCHECK_ONE(subject);

        PKIX_CHECK(PKIX_List_AppendItem
                (cachedKeys, (PKIX_PL_Object *)subject, plContext),
                PKIX_LISTAPPENDITEMFAILED);

        cachedCertError = PKIX_PL_HashTable_Lookup
                    (cachedCertTable,
                    (PKIX_PL_Object *) cachedKeys,
                    (PKIX_PL_Object **) &cachedValues,
                    plContext);
        pkix_cLookupCount++;

        if (cachedValues != NULL && cachedCertError == NULL) {

                PKIX_CHECK(PKIX_List_GetItem
                        (cachedValues,
                        0,
                        (PKIX_PL_Object **) &cacheValidUntilDate,
                        plContext),
                        PKIX_LISTGETITEMFAILED);

                if (testDate) {
                    PKIX_CHECK(PKIX_PL_Object_Compare
                         ((PKIX_PL_Object *)testDate,
                         (PKIX_PL_Object *)cacheValidUntilDate,
                         &cmpCacheTimeResult,
                         plContext),
                         PKIX_OBJECTCOMPARATORFAILED);
                }

                if (cmpCacheTimeResult <= 0) {

                    PKIX_CHECK(PKIX_List_GetItem
                        (cachedValues,
                        1,
                        (PKIX_PL_Object **) &cachedCertList,
                        plContext),
                        PKIX_LISTGETITEMFAILED);

                    /*
                     * Certs put on cache satifies only for Subject,
                     * user selector and ComCertSelParams to filter.
                     */
                    PKIX_CHECK(PKIX_CertSelector_Create
                          (NULL, NULL, &certSel, plContext),
                          PKIX_CERTSELECTORCREATEFAILED);

                    PKIX_CHECK(PKIX_CertSelector_SetCommonCertSelectorParams
                          (certSel, certSelParams, plContext),
                          PKIX_CERTSELECTORSETCOMMONCERTSELECTORPARAMSFAILED);

                    PKIX_CHECK(PKIX_CertSelector_GetMatchCallback
                          (certSel, &selectorMatch, plContext),
                          PKIX_CERTSELECTORGETMATCHCALLBACKFAILED);

                    PKIX_CHECK(PKIX_List_Create(&selCertList, plContext),
                            PKIX_LISTCREATEFAILED);

                    /* 
                     * If any of the Cert on the list is out-dated, invalidate
                     * this cache item.
                     */
                    PKIX_CHECK(PKIX_List_GetLength
                        (cachedCertList, &numItems, plContext),
                        PKIX_LISTGETLENGTHFAILED);

                    for (i = 0; i < numItems; i++){

                        PKIX_CHECK(PKIX_List_GetItem
                            (cachedCertList,
                            i,
                            (PKIX_PL_Object **)&cert,
                            plContext),
                            PKIX_LISTGETITEMFAILED);

                        PKIX_CHECK(PKIX_PL_Cert_GetValidityNotAfter
                            (cert, &invalidAfterDate, plContext),
                            PKIX_CERTGETVALIDITYNOTAFTERFAILED);

                        if (testDate) {
                            PKIX_CHECK(PKIX_PL_Object_Compare
                                ((PKIX_PL_Object *)invalidAfterDate,
                                (PKIX_PL_Object *)testDate,
                                &cmpValidTimeResult,
                                plContext),
                                PKIX_OBJECTCOMPARATORFAILED);
                        }

                        if (cmpValidTimeResult < 0) {

                            pkix_cRemoveCount++;
                            *pFound = PKIX_FALSE;

                            /* one cert is out-dated, remove item from cache */
                            PKIX_CHECK(PKIX_PL_HashTable_Remove
                                    (cachedCertTable,
                                    (PKIX_PL_Object *) cachedKeys,
                                    plContext),
                                    PKIX_HASHTABLEREMOVEFAILED);
                            goto cleanup;
                        }

                        selectorError = selectorMatch(certSel, cert, plContext);
                        if (!selectorError){
                            /* put on the return list */
                            PKIX_CHECK(PKIX_List_AppendItem
                                   (selCertList,
                                   (PKIX_PL_Object *)cert,
                                   plContext),
                                  PKIX_LISTAPPENDITEMFAILED);
                        } else {
                            PKIX_DECREF(selectorError);
                        }

                        PKIX_DECREF(cert);
                        PKIX_DECREF(invalidAfterDate);

                    }

                    if (*pFound) {
                        PKIX_INCREF(selCertList);
                        *pCerts = selCertList;
                    }

                } else {

                    pkix_cRemoveCount++;
                    *pFound = PKIX_FALSE;
                    /* cache item is out-dated, remove it from cache */
                    PKIX_CHECK(PKIX_PL_HashTable_Remove
                                (cachedCertTable,
                                (PKIX_PL_Object *) cachedKeys,
                                plContext),
                                PKIX_HASHTABLEREMOVEFAILED);
                }

        } 

cleanup:

        PKIX_DECREF(subject);
        PKIX_DECREF(certSel);
        PKIX_DECREF(cachedKeys);
        PKIX_DECREF(cachedValues);
        PKIX_DECREF(cacheValidUntilDate);
        PKIX_DECREF(cert);
        PKIX_DECREF(cachedCertList);
        PKIX_DECREF(selCertList);
        PKIX_DECREF(invalidAfterDate);
        PKIX_DECREF(cachedCertError);
        PKIX_DECREF(selectorError);

        PKIX_RETURN(BUILD);
}

/*
 * FUNCTION: pkix_CacheCert_Add
 * DESCRIPTION:
 *
 *  Add Cert Hash Table for a cached item based on "store" and Subject in
 *  "certSelParams" as the hash keys and have "certs" as the key value.
 *  This hashtable is maintained in the following ways:
 *  1) When creating the hashtable, maximum bucket size can be specified (0 for
 *     unlimited). If items in a bucket reaches its full size, an new addition
 *     will trigger the removal of the old as FIFO sequence.
 *  2) A PKIX_PL_Date created with current time offset by constant 
 *     CACHE_ITEM_PERIOD_SECONDS is attached to each item in the Hash Table.
 *     If the CertStore this Cert is from is a trusted one, the cache period is
 *     shorter so cache can be updated more frequently.
 *     When an item is retrieved, this date is compared against "testDate" for
 *     validity. If comparison indicates this item is expired, the item is
 *     removed from the bucket.
 *
 * PARAMETERS:
 *  "store"
 *      Address of CertStore as key to retrieve this CertChain. Must be 
 *      non-NULL.
 *  "certSelParams"
 *      Address of ComCertSelParams that its subject is used as key to retrieve
 *      this CertChain. Must be non-NULL.
 *  "certs"
 *      Address PKIX_List of Certs will be stored. Must be no-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns an Error Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_CacheCert_Add(
        PKIX_CertStore *store,
        PKIX_ComCertSelParams *certSelParams,
        PKIX_List* certs,
        void *plContext)
{
        PKIX_List *cachedKeys = NULL;
        PKIX_List *cachedValues = NULL;
        PKIX_PL_Date *cacheValidUntilDate = NULL;
        PKIX_PL_X500Name *subject = NULL;
        PKIX_Error *cachedCertError = NULL;
        PKIX_CertStore_CheckTrustCallback trustCallback = NULL;
        PKIX_UInt32 cachePeriod = CACHE_ITEM_PERIOD_SECONDS;
        PKIX_UInt32 numCerts = 0;

        PKIX_ENTER(BUILD, "pkix_CacheCert_Add");
        PKIX_NULLCHECK_THREE(store, certSelParams, certs);

        PKIX_CHECK(PKIX_List_GetLength(certs, &numCerts,
                                       plContext),
                   PKIX_LISTGETLENGTHFAILED);
        if (numCerts == 0) {
            /* Don't want to add an empty list. */
            goto cleanup;
        }

        PKIX_CHECK(PKIX_List_Create(&cachedKeys, plContext),
                PKIX_LISTCREATEFAILED);

        PKIX_CHECK(PKIX_List_AppendItem
                (cachedKeys, (PKIX_PL_Object *)store, plContext),
                PKIX_LISTAPPENDITEMFAILED);

        PKIX_CHECK(PKIX_ComCertSelParams_GetSubject
                (certSelParams, &subject, plContext),
                PKIX_COMCERTSELPARAMSGETSUBJECTFAILED);

        PKIX_NULLCHECK_ONE(subject);

        PKIX_CHECK(PKIX_List_AppendItem
                (cachedKeys, (PKIX_PL_Object *)subject, plContext),
                PKIX_LISTAPPENDITEMFAILED);

        PKIX_CHECK(PKIX_List_Create(&cachedValues, plContext),
                PKIX_LISTCREATEFAILED);

        PKIX_CHECK(PKIX_CertStore_GetTrustCallback
                (store, &trustCallback, plContext),
                PKIX_CERTSTOREGETTRUSTCALLBACKFAILED);

        if (trustCallback) {
                cachePeriod = CACHE_TRUST_ITEM_PERIOD_SECONDS;
        }

        PKIX_CHECK(PKIX_PL_Date_Create_CurrentOffBySeconds
               (cachePeriod, &cacheValidUntilDate, plContext),
               PKIX_DATECREATECURRENTOFFBYSECONDSFAILED);

        PKIX_CHECK(PKIX_List_AppendItem
                (cachedValues,
                (PKIX_PL_Object *)cacheValidUntilDate,
                plContext),
                PKIX_LISTAPPENDITEMFAILED);

        PKIX_CHECK(PKIX_List_AppendItem
                (cachedValues,
                (PKIX_PL_Object *)certs,
                plContext),
                PKIX_LISTAPPENDITEMFAILED);

        cachedCertError = PKIX_PL_HashTable_Add
                    (cachedCertTable,
                    (PKIX_PL_Object *) cachedKeys,
                    (PKIX_PL_Object *) cachedValues,
                    plContext);

        pkix_cAddCount++;

        if (cachedCertError != NULL) {
                PKIX_DEBUG("PKIX_PL_HashTable_Add for Certs skipped: "
                        "entry existed\n");
        }

cleanup:

        PKIX_DECREF(subject);
        PKIX_DECREF(cachedKeys);
        PKIX_DECREF(cachedValues);
        PKIX_DECREF(cacheValidUntilDate);
        PKIX_DECREF(cachedCertError);

        PKIX_RETURN(BUILD);
}

/*
 * FUNCTION: pkix_CacheCrlEntry_Lookup
 * DESCRIPTION:
 *
 *  Look up CrlEntry Hash Table for a cached item based on "store",
 *  "certIssuer" and "certSerialNumber" as the hash keys and returns values
 *  "pCrls". If there isn't an item to match the key, a PKIX_FALSE is
 *  returned at "pFound".
 *  This hashtable is maintained in the following way:
 *  1) When creating the hashtable, maximum bucket size can be specified (0 for
 *     unlimited). If items in a bucket reaches its full size, an new addition
 *     will trigger the removal of the old as FIFO sequence.
 *
 * PARAMETERS:
 *  "store"
 *      Address of CertStore as key to retrieve this CertChain. Must be 
 *      non-NULL.
 *  "certIssuer"
 *      Address of X500Name that is used as key to retrieve the CRLEntries.
 *      Must be non-NULL.
 *  "certSerialNumber"
 *      Address of BigInt that is used as key to retrieve the CRLEntries.
 *      Must be non-NULL.
 *  "pFound"
 *      Address of KPKIX_Boolean indicating valid data is found.
 *      Must be non-NULL.
 *  "pCrls"
 *      Address PKIX_List where the CRLEntry will be stored. Must be no-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns an Error Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_CacheCrlEntry_Lookup(
        PKIX_CertStore *store,
        PKIX_PL_X500Name *certIssuer,
        PKIX_PL_BigInt *certSerialNumber,
        PKIX_Boolean *pFound,
        PKIX_List** pCrls,
        void *plContext)
{
        PKIX_List *cachedKeys = NULL;
        PKIX_List *cachedCrlEntryList = NULL;
        PKIX_Error *cachedCrlEntryError = NULL;

        PKIX_ENTER(BUILD, "pkix_CacheCrlEntry_Lookup");
        PKIX_NULLCHECK_THREE(store, certIssuer, certSerialNumber);
        PKIX_NULLCHECK_TWO(pFound, pCrls);

        *pFound = PKIX_FALSE;

        /* Find CrlEntry(s) by issuer and serial number */
         
        PKIX_CHECK(PKIX_List_Create(&cachedKeys, plContext),
                    PKIX_LISTCREATEFAILED);

        PKIX_CHECK(PKIX_List_AppendItem
                    (cachedKeys, (PKIX_PL_Object *)store, plContext),
                    PKIX_LISTAPPENDITEMFAILED);

        PKIX_CHECK(PKIX_List_AppendItem
                    (cachedKeys, (PKIX_PL_Object *)certIssuer, plContext),
                    PKIX_LISTAPPENDITEMFAILED);

        PKIX_CHECK(PKIX_List_AppendItem
                    (cachedKeys,
                    (PKIX_PL_Object *)certSerialNumber,
                    plContext),
                    PKIX_LISTAPPENDITEMFAILED);

        cachedCrlEntryError = PKIX_PL_HashTable_Lookup
                    (cachedCrlEntryTable,
                    (PKIX_PL_Object *) cachedKeys,
                    (PKIX_PL_Object **) &cachedCrlEntryList,
                    plContext);
        pkix_ceLookupCount++;

        /* 
         * We don't need check Date to invalidate this cache item,
         * the item is uniquely defined and won't be reverted. Let
         * the FIFO for cleaning up.
         */

        if (cachedCrlEntryList != NULL && cachedCrlEntryError == NULL ) {

                PKIX_INCREF(cachedCrlEntryList);
                *pCrls = cachedCrlEntryList;

                *pFound = PKIX_TRUE;

        } else {

                *pFound = PKIX_FALSE;
        }

cleanup:

        PKIX_DECREF(cachedKeys);
        PKIX_DECREF(cachedCrlEntryList);
        PKIX_DECREF(cachedCrlEntryError);

        PKIX_RETURN(BUILD);
}

/*
 * FUNCTION: pkix_CacheCrlEntry_Add
 * DESCRIPTION:
 *
 *  Look up CrlEntry Hash Table for a cached item based on "store",
 *  "certIssuer" and "certSerialNumber" as the hash keys and have "pCrls" as
 *  the hash value. If there isn't an item to match the key, a PKIX_FALSE is
 *  returned at "pFound".
 *  This hashtable is maintained in the following way:
 *  1) When creating the hashtable, maximum bucket size can be specified (0 for
 *     unlimited). If items in a bucket reaches its full size, an new addition
 *     will trigger the removal of the old as FIFO sequence.
 *
 * PARAMETERS:
 *  "store"
 *      Address of CertStore as key to retrieve this CertChain. Must be 
 *      non-NULL.
 *  "certIssuer"
 *      Address of X500Name that is used as key to retrieve the CRLEntries.
 *      Must be non-NULL.
 *  "certSerialNumber"
 *      Address of BigInt that is used as key to retrieve the CRLEntries.
 *      Must be non-NULL.
 *  "crls"
 *      Address PKIX_List where the CRLEntry is stored. Must be no-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns an Error Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_CacheCrlEntry_Add(
        PKIX_CertStore *store,
        PKIX_PL_X500Name *certIssuer,
        PKIX_PL_BigInt *certSerialNumber,
        PKIX_List* crls,
        void *plContext)
{
        PKIX_List *cachedKeys = NULL;
        PKIX_Error *cachedCrlEntryError = NULL;

        PKIX_ENTER(BUILD, "pkix_CacheCrlEntry_Add");
        PKIX_NULLCHECK_THREE(store, certIssuer, certSerialNumber);
        PKIX_NULLCHECK_ONE(crls);

        /* Add CrlEntry(s) by issuer and serial number */
         
        PKIX_CHECK(PKIX_List_Create(&cachedKeys, plContext),
                    PKIX_LISTCREATEFAILED);

        PKIX_CHECK(PKIX_List_AppendItem
                    (cachedKeys, (PKIX_PL_Object *)store, plContext),
                    PKIX_LISTAPPENDITEMFAILED);

        PKIX_CHECK(PKIX_List_AppendItem
                    (cachedKeys, (PKIX_PL_Object *)certIssuer, plContext),
                    PKIX_LISTAPPENDITEMFAILED);

        PKIX_CHECK(PKIX_List_AppendItem
                    (cachedKeys,
                    (PKIX_PL_Object *)certSerialNumber,
                    plContext),
                    PKIX_LISTAPPENDITEMFAILED);

        cachedCrlEntryError = PKIX_PL_HashTable_Add
                    (cachedCrlEntryTable,
                    (PKIX_PL_Object *) cachedKeys,
                    (PKIX_PL_Object *) crls,
                    plContext);
        pkix_ceAddCount++;

cleanup:

        PKIX_DECREF(cachedKeys);
        PKIX_DECREF(cachedCrlEntryError);

        PKIX_RETURN(BUILD);
}

#ifdef PKIX_OBJECT_LEAK_TEST

/* TEST_START_FN and testStartFnStackPosition define at what state
 * of the stack the object leak testing should begin. The condition
 * in pkix_CheckForGeneratedError works the following way: do leak
 * testing if at position testStartFnStackPosition in stack array
 * (fnStackNameArr) we have called function TEST_START_FN.
 * Note, that stack array get filled only when executing libpkix
 * functions.
 * */
#define TEST_START_FN "PKIX_BuildChain"

PKIX_Error*
pkix_CheckForGeneratedError(PKIX_StdVars * stdVars, 
                            PKIX_ERRORCLASS errClass, 
                            char * fnName,
                            PKIX_Boolean *errSetFlag,
                            void * plContext)
{
    PKIX_Error *genErr = NULL;
    PKIX_UInt32 pos = 0;
    PKIX_UInt32 strLen = 0;

    if (fnName) { 
        if (fnStackNameArr[testStartFnStackPosition] == NULL ||
            strcmp(fnStackNameArr[testStartFnStackPosition], TEST_START_FN)
            ) {
            /* return with out error if not with in boundary */
            return NULL;
        }
        if (!strcmp(fnName, TEST_START_FN)) {
            *errSetFlag = PKIX_TRUE;
            noErrorState = PKIX_FALSE;
            errorGenerated = PKIX_FALSE;
        }
    }   

    if (noErrorState || errorGenerated)  return NULL;

    if (fnName && (
        !strcmp(fnName, "PKIX_PL_Object_DecRef") ||
        !strcmp(fnName, "PKIX_PL_Object_Unlock") ||
        !strcmp(fnName, "pkix_UnlockObject") ||
        !strcmp(fnName, "pkix_Throw") ||
        !strcmp(fnName, "pkix_trace_dump_cert") ||
        !strcmp(fnName, "PKIX_PL_Free"))) {
        /* do not generate error for this functions */
        noErrorState = PKIX_TRUE;
        *errSetFlag = PKIX_TRUE;
        return NULL;
    }

    if (PL_HashTableLookup(fnInvTable, &fnStackInvCountArr[stackPosition - 1])) {
        return NULL;
    }

    PL_HashTableAdd(fnInvTable, &fnStackInvCountArr[stackPosition - 1], nonNullValue);
    errorGenerated = PKIX_TRUE;
    noErrorState = PKIX_TRUE;
    genErr = PKIX_DoThrow(stdVars, errClass, PKIX_MEMLEAKGENERATEDERROR,
                          errClass, plContext);
    while(fnStackNameArr[pos]) {
        strLen += PORT_Strlen(fnStackNameArr[pos++]) + 1;
    }
    strLen += 1; /* end of line. */
    pos = 0;
    errorFnStackString = PORT_ZAlloc(strLen);
    while(fnStackNameArr[pos]) {
        strcat(errorFnStackString, "/");
        strcat(errorFnStackString, fnStackNameArr[pos++]);
    }
    noErrorState = PKIX_FALSE;
    
    return genErr;
}
#endif /* PKIX_OBJECT_LEAK_TEST */
