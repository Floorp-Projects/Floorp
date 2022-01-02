/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * pkix_pl_publickey.c
 *
 * Certificate Object Functions
 *
 */

#include "pkix_pl_publickey.h"

/* --Private-Cert-Functions------------------------------------- */

/*
 * FUNCTION: pkix_pl_PublicKey_ToString_Helper
 * DESCRIPTION:
 *
 *  Helper function that creates a string representation of the PublicKey
 *  pointed to by "pkixPubKey" and stores it at "pString".
 *
 * PARAMETERS
 *  "pkixPubKey"
 *      Address of PublicKey whose string representation is desired.
 *      Must be non-NULL.
 *  "pString"
 *      Address where object pointer will be stored. Must be non-NULL.
 *  "plContext" - Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a PublicKey Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_PublicKey_ToString_Helper(
        PKIX_PL_PublicKey *pkixPubKey,
        PKIX_PL_String **pString,
        void *plContext)
{
        SECAlgorithmID algorithm;
        SECOidTag pubKeyTag;
        char *asciiOID = NULL;
        PKIX_Boolean freeAsciiOID = PKIX_FALSE;
        SECItem oidBytes;

        PKIX_ENTER(PUBLICKEY, "pkix_pl_PublicKey_ToString_Helper");
        PKIX_NULLCHECK_THREE(pkixPubKey, pkixPubKey->nssSPKI, pString);

        /*
         * XXX for now, we print out public key algorithm's
         * description - add params and bytes later
         */

        /*
         * If the algorithm OID is known to NSS,
         * we print out the ASCII description that is
         * registered with NSS. Otherwise, if unknown,
         * we print out the OID numbers (eg. "1.2.840.3")
         */

        algorithm = pkixPubKey->nssSPKI->algorithm;

        PKIX_PUBLICKEY_DEBUG("\t\tCalling SECOID_GetAlgorithmTag).\n");
        pubKeyTag = SECOID_GetAlgorithmTag(&algorithm);
        if (pubKeyTag != SEC_OID_UNKNOWN){
                PKIX_PUBLICKEY_DEBUG
                        ("\t\tCalling SECOID_FindOIDTagDescription).\n");
                asciiOID = (char *)SECOID_FindOIDTagDescription(pubKeyTag);
                if (!asciiOID){
                        PKIX_ERROR(PKIX_SECOIDFINDOIDTAGDESCRIPTIONFAILED);
                }
        } else { /* pubKeyTag == SEC_OID_UNKNOWN */
                oidBytes = algorithm.algorithm;
                PKIX_CHECK(pkix_pl_oidBytes2Ascii
                            (&oidBytes, &asciiOID, plContext),
                            PKIX_OIDBYTES2ASCIIFAILED);
                freeAsciiOID = PKIX_TRUE;
        }

        PKIX_CHECK(PKIX_PL_String_Create
                (PKIX_ESCASCII, (void *)asciiOID, 0, pString, plContext),
                PKIX_UNABLETOCREATEPSTRING);

cleanup:

        /*
         * we only free asciiOID if it was malloc'ed by pkix_pl_oidBytes2Ascii
         */
        if (freeAsciiOID){
                PKIX_FREE(asciiOID);
        }

        PKIX_RETURN(PUBLICKEY);
}

/*
 * FUNCTION: pkix_pl_DestroySPKI
 * DESCRIPTION:
 *  Frees all memory associated with the CERTSubjectPublicKeyInfo pointed to
 *  by "nssSPKI".
 * PARAMETERS
 *  "nssSPKI"
 *      Address of CERTSubjectPublicKeyInfo. Must be non-NULL.
 *  "plContext" - Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns an Object Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_DestroySPKI(
        CERTSubjectPublicKeyInfo *nssSPKI,
        void *plContext)
{
        PKIX_ENTER(PUBLICKEY, "pkix_pl_DestroySPKI");

        PKIX_NULLCHECK_ONE(nssSPKI);

        PKIX_PUBLICKEY_DEBUG("\t\tCalling SECOID_DestroyAlgorithmID).\n");
        SECOID_DestroyAlgorithmID(&nssSPKI->algorithm, PKIX_FALSE);

        PKIX_PUBLICKEY_DEBUG("\t\tCalling SECITEM_FreeItem).\n");
        SECITEM_FreeItem(&nssSPKI->subjectPublicKey, PKIX_FALSE);

        PKIX_RETURN(PUBLICKEY);
}

/*
 * FUNCTION: pkix_pl_PublicKey_Destroy
 * (see comments for PKIX_PL_DestructorCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_PublicKey_Destroy(
        PKIX_PL_Object *object,
        void *plContext)
{
        PKIX_PL_PublicKey *pubKey = NULL;

        PKIX_ENTER(PUBLICKEY, "pkix_pl_PublicKey_Destroy");

        PKIX_NULLCHECK_ONE(object);

        PKIX_CHECK(pkix_CheckType(object, PKIX_PUBLICKEY_TYPE, plContext),
                    PKIX_OBJECTNOTPUBLICKEY);

        pubKey = (PKIX_PL_PublicKey *)object;

        if (pubKey->nssSPKI) {

            PKIX_CHECK(pkix_pl_DestroySPKI(pubKey->nssSPKI, plContext),
                       PKIX_DESTROYSPKIFAILED);
            
            PKIX_FREE(pubKey->nssSPKI);
        }

cleanup:

        PKIX_RETURN(PUBLICKEY);
}

/*
 * FUNCTION: pkix_pl_PublicKey_ToString
 * (see comments for PKIX_PL_ToStringCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_PublicKey_ToString(
        PKIX_PL_Object *object,
        PKIX_PL_String **pString,
        void *plContext)
{
        PKIX_PL_PublicKey *pkixPubKey = NULL;
        PKIX_PL_String *pubKeyString = NULL;

        PKIX_ENTER(PUBLICKEY, "pkix_pl_PublicKey_toString");
        PKIX_NULLCHECK_TWO(object, pString);

        PKIX_CHECK(pkix_CheckType(object, PKIX_PUBLICKEY_TYPE, plContext),
                    PKIX_OBJECTNOTPUBLICKEY);

        pkixPubKey = (PKIX_PL_PublicKey *)object;

        PKIX_CHECK(pkix_pl_PublicKey_ToString_Helper
                    (pkixPubKey, &pubKeyString, plContext),
                    PKIX_PUBLICKEYTOSTRINGHELPERFAILED);

        *pString = pubKeyString;

cleanup:

        PKIX_RETURN(PUBLICKEY);
}

/*
 * FUNCTION: pkix_pl_PublicKey_Hashcode
 * (see comments for PKIX_PL_HashcodeCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_PublicKey_Hashcode(
        PKIX_PL_Object *object,
        PKIX_UInt32 *pHashcode,
        void *plContext)
{
        PKIX_PL_PublicKey *pkixPubKey = NULL;
        SECItem algOID;
        SECItem algParams;
        SECItem nssPubKey;
        PKIX_UInt32 algOIDHash;
        PKIX_UInt32 algParamsHash;
        PKIX_UInt32 pubKeyHash;

        PKIX_ENTER(PUBLICKEY, "pkix_pl_PublicKey_Hashcode");
        PKIX_NULLCHECK_TWO(object, pHashcode);

        PKIX_CHECK(pkix_CheckType(object, PKIX_PUBLICKEY_TYPE, plContext),
                    PKIX_OBJECTNOTPUBLICKEY);

        pkixPubKey = (PKIX_PL_PublicKey *)object;

        PKIX_NULLCHECK_ONE(pkixPubKey->nssSPKI);

        algOID = pkixPubKey->nssSPKI->algorithm.algorithm;
        algParams = pkixPubKey->nssSPKI->algorithm.parameters;
        nssPubKey = pkixPubKey->nssSPKI->subjectPublicKey;

        PKIX_CHECK(pkix_hash
                    (algOID.data, algOID.len, &algOIDHash, plContext),
                    PKIX_HASHFAILED);

        PKIX_CHECK(pkix_hash
                    (algParams.data, algParams.len, &algParamsHash, plContext),
                    PKIX_HASHFAILED);

        PKIX_CHECK(pkix_hash
                    (nssPubKey.data, nssPubKey.len, &pubKeyHash, plContext),
                    PKIX_HASHFAILED);

        *pHashcode = pubKeyHash;

cleanup:

        PKIX_RETURN(PUBLICKEY);
}


/*
 * FUNCTION: pkix_pl_PublicKey_Equals
 * (see comments for PKIX_PL_Equals_Callback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_PublicKey_Equals(
        PKIX_PL_Object *firstObject,
        PKIX_PL_Object *secondObject,
        PKIX_Boolean *pResult,
        void *plContext)
{
        PKIX_PL_PublicKey *firstPKIXPubKey = NULL;
        PKIX_PL_PublicKey *secondPKIXPubKey = NULL;
        CERTSubjectPublicKeyInfo *firstSPKI = NULL;
        CERTSubjectPublicKeyInfo *secondSPKI = NULL;
        SECComparison cmpResult;
        PKIX_UInt32 secondType;

        PKIX_ENTER(PUBLICKEY, "pkix_pl_PublicKey_Equals");
        PKIX_NULLCHECK_THREE(firstObject, secondObject, pResult);

        /* test that firstObject is a PublicKey */
        PKIX_CHECK(pkix_CheckType(firstObject, PKIX_PUBLICKEY_TYPE, plContext),
                PKIX_FIRSTOBJECTNOTPUBLICKEY);

        /*
         * Since we know firstObject is a PublicKey, if both references are
         * identical, they must be equal
         */
        if (firstObject == secondObject){
                *pResult = PKIX_TRUE;
                goto cleanup;
        }

        /*
         * If secondObject isn't a PublicKey, we don't throw an error.
         * We simply return a Boolean result of FALSE
         */
        *pResult = PKIX_FALSE;
        PKIX_CHECK(PKIX_PL_Object_GetType
                    (secondObject, &secondType, plContext),
                    PKIX_COULDNOTGETTYPEOFSECONDARGUMENT);
        if (secondType != PKIX_PUBLICKEY_TYPE) goto cleanup;

        firstPKIXPubKey = ((PKIX_PL_PublicKey *)firstObject);
        secondPKIXPubKey = (PKIX_PL_PublicKey *)secondObject;

        firstSPKI = firstPKIXPubKey->nssSPKI;
        secondSPKI = secondPKIXPubKey->nssSPKI;

        PKIX_NULLCHECK_TWO(firstSPKI, secondSPKI);

        PKIX_PL_NSSCALLRV(PUBLICKEY, cmpResult, SECOID_CompareAlgorithmID,
                        (&firstSPKI->algorithm, &secondSPKI->algorithm));

        if (cmpResult == SECEqual){
                PKIX_PUBLICKEY_DEBUG("\t\tCalling SECITEM_CompareItem).\n");
                cmpResult = SECITEM_CompareItem
                                (&firstSPKI->subjectPublicKey,
                                &secondSPKI->subjectPublicKey);
        }

        *pResult = (cmpResult == SECEqual)?PKIX_TRUE:PKIX_FALSE;

cleanup:

        PKIX_RETURN(PUBLICKEY);
}

/*
 * FUNCTION: pkix_pl_PublicKey_RegisterSelf
 * DESCRIPTION:
 *  Registers PKIX_PUBLICKEY_TYPE and its related functions with systemClasses[]
 * THREAD SAFETY:
 *  Not Thread Safe - for performance and complexity reasons
 *
 *  Since this function is only called by PKIX_PL_Initialize, which should
 *  only be called once, it is acceptable that this function is not
 *  thread-safe.
 */
PKIX_Error *
pkix_pl_PublicKey_RegisterSelf(void *plContext)
{

        extern pkix_ClassTable_Entry systemClasses[PKIX_NUMTYPES];
        pkix_ClassTable_Entry entry;

        PKIX_ENTER(PUBLICKEY, "pkix_pl_PublicKey_RegisterSelf");

        entry.description = "PublicKey";
        entry.objCounter = 0;
        entry.typeObjectSize = sizeof(PKIX_PL_PublicKey);
        entry.destructor = pkix_pl_PublicKey_Destroy;
        entry.equalsFunction = pkix_pl_PublicKey_Equals;
        entry.hashcodeFunction = pkix_pl_PublicKey_Hashcode;
        entry.toStringFunction = pkix_pl_PublicKey_ToString;
        entry.comparator = NULL;
        entry.duplicateFunction = pkix_duplicateImmutable;
        systemClasses[PKIX_PUBLICKEY_TYPE] = entry;

        PKIX_RETURN(PUBLICKEY);
}

/* --Public-Functions------------------------------------------------------- */

/*
 * FUNCTION: PKIX_PL_PublicKey_NeedsDSAParameters
 *              (see comments in pkix_pl_pki.h)
 */
PKIX_Error *
PKIX_PL_PublicKey_NeedsDSAParameters(
        PKIX_PL_PublicKey *pubKey,
        PKIX_Boolean *pNeedsParams,
        void *plContext)
{
        CERTSubjectPublicKeyInfo *nssSPKI = NULL;
        KeyType pubKeyType;
        PKIX_Boolean needsParams = PKIX_FALSE;

        PKIX_ENTER(PUBLICKEY, "PKIX_PL_PublicKey_NeedsDSAParameters");
        PKIX_NULLCHECK_TWO(pubKey, pNeedsParams);

        nssSPKI = pubKey->nssSPKI;

        PKIX_PUBLICKEY_DEBUG("\t\tCalling CERT_GetCertKeyType).\n");
        pubKeyType = CERT_GetCertKeyType(nssSPKI);
        if (!pubKeyType){
                PKIX_ERROR(PKIX_PUBKEYTYPENULLKEY);
        }

        if ((pubKeyType == dsaKey) &&
            (nssSPKI->algorithm.parameters.len == 0)){
                needsParams = PKIX_TRUE;
        }

        *pNeedsParams = needsParams;

cleanup:

        PKIX_RETURN(PUBLICKEY);
}

/*
 * FUNCTION: PKIX_PL_PublicKey_MakeInheritedDSAPublicKey
 *              (see comments in pkix_pl_pki.h)
 */
PKIX_Error *
PKIX_PL_PublicKey_MakeInheritedDSAPublicKey(
        PKIX_PL_PublicKey *firstKey,
        PKIX_PL_PublicKey *secondKey,
        PKIX_PL_PublicKey **pResultKey,
        void *plContext)
{
        CERTSubjectPublicKeyInfo *firstSPKI = NULL;
        CERTSubjectPublicKeyInfo *secondSPKI = NULL;
        CERTSubjectPublicKeyInfo *thirdSPKI = NULL;
        PKIX_PL_PublicKey *resultKey = NULL;
        KeyType firstPubKeyType;
        KeyType secondPubKeyType;
        SECStatus rv;

        PKIX_ENTER(PUBLICKEY, "PKIX_PL_PublicKey_MakeInheritedDSAPublicKey");
        PKIX_NULLCHECK_THREE(firstKey, secondKey, pResultKey);
        PKIX_NULLCHECK_TWO(firstKey->nssSPKI, secondKey->nssSPKI);

        firstSPKI = firstKey->nssSPKI;
        secondSPKI = secondKey->nssSPKI;

        PKIX_PUBLICKEY_DEBUG("\t\tCalling CERT_GetCertKeyType).\n");
        firstPubKeyType = CERT_GetCertKeyType(firstSPKI);
        if (!firstPubKeyType){
                PKIX_ERROR(PKIX_FIRSTPUBKEYTYPENULLKEY);
        }

        PKIX_PUBLICKEY_DEBUG("\t\tCalling CERT_GetCertKeyType).\n");
        secondPubKeyType = CERT_GetCertKeyType(secondSPKI);
        if (!secondPubKeyType){
                PKIX_ERROR(PKIX_SECONDPUBKEYTYPENULLKEY);
        }

        if ((firstPubKeyType == dsaKey) &&
            (firstSPKI->algorithm.parameters.len == 0)){
                if (secondPubKeyType != dsaKey) {
                        PKIX_ERROR(PKIX_SECONDKEYNOTDSAPUBLICKEY);
                } else if (secondSPKI->algorithm.parameters.len == 0) {
                        PKIX_ERROR
                                (PKIX_SECONDKEYDSAPUBLICKEY);
                } else {
                        PKIX_CHECK(PKIX_PL_Calloc
                                    (1,
                                    sizeof (CERTSubjectPublicKeyInfo),
                                    (void **)&thirdSPKI,
                                    plContext),
                                    PKIX_CALLOCFAILED);

                        PKIX_PUBLICKEY_DEBUG
                                ("\t\tCalling"
                                "SECKEY_CopySubjectPublicKeyInfo).\n");
                        rv = SECKEY_CopySubjectPublicKeyInfo
                                (NULL, thirdSPKI, firstSPKI);
                        if (rv != SECSuccess) {
                            PKIX_ERROR
                                    (PKIX_SECKEYCOPYSUBJECTPUBLICKEYINFOFAILED);
                        }

                        PKIX_PUBLICKEY_DEBUG
                                ("\t\tCalling SECITEM_CopyItem).\n");
                        rv = SECITEM_CopyItem(NULL,
                                            &thirdSPKI->algorithm.parameters,
                                            &secondSPKI->algorithm.parameters);

                        if (rv != SECSuccess) {
                                PKIX_ERROR(PKIX_OUTOFMEMORY);
                        }

                        /* create a PKIX_PL_PublicKey object */
                        PKIX_CHECK(PKIX_PL_Object_Alloc
                                    (PKIX_PUBLICKEY_TYPE,
                                    sizeof (PKIX_PL_PublicKey),
                                    (PKIX_PL_Object **)&resultKey,
                                    plContext),
                                    PKIX_COULDNOTCREATEOBJECT);

                        /* populate the SPKI field */
                        resultKey->nssSPKI = thirdSPKI;
                        *pResultKey = resultKey;
                }
        } else {
                *pResultKey = NULL;
        }

cleanup:

        if (thirdSPKI && PKIX_ERROR_RECEIVED){
                PKIX_CHECK(pkix_pl_DestroySPKI(thirdSPKI, plContext),
                            PKIX_DESTROYSPKIFAILED);
                PKIX_FREE(thirdSPKI);
        }

        PKIX_RETURN(PUBLICKEY);
}
