/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * pkix_pl_certpolicyqualifier.c
 *
 * CertPolicyQualifier Type Functions
 *
 */

#include "pkix_pl_certpolicyqualifier.h"

/*
 * FUNCTION: pkix_pl_CertPolicyQualifier_Create
 * DESCRIPTION:
 *
 *  Creates a CertPolicyQualifier object with the OID given by "oid"
 *  and the ByteArray given by "qualifier", and stores it at "pObject".
 *
 * PARAMETERS
 *  "oid"
 *      Address of OID of the desired policyQualifierId; must be non-NULL
 *  "qualifier"
 *      Address of ByteArray with the desired value of the qualifier;
 *      must be non-NULL
 *  "pObject"
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
pkix_pl_CertPolicyQualifier_Create(
        PKIX_PL_OID *oid,
        PKIX_PL_ByteArray *qualifier,
        PKIX_PL_CertPolicyQualifier **pObject,
        void *plContext)
{
        PKIX_PL_CertPolicyQualifier *qual = NULL;

        PKIX_ENTER(CERTPOLICYQUALIFIER, "pkix_pl_CertPolicyQualifier_Create");

        PKIX_NULLCHECK_THREE(oid, qualifier, pObject);

        PKIX_CHECK(PKIX_PL_Object_Alloc
                (PKIX_CERTPOLICYQUALIFIER_TYPE,
                sizeof (PKIX_PL_CertPolicyQualifier),
                (PKIX_PL_Object **)&qual,
                plContext),
                PKIX_COULDNOTCREATECERTPOLICYQUALIFIEROBJECT);

        PKIX_INCREF(oid);
        qual->policyQualifierId = oid;

        PKIX_INCREF(qualifier);
        qual->qualifier = qualifier;

        *pObject = qual;
        qual = NULL;

cleanup:
        PKIX_DECREF(qual);

        PKIX_RETURN(CERTPOLICYQUALIFIER);
}

/*
 * FUNCTION: pkix_pl_CertPolicyQualifier_Destroy
 * (see comments for PKIX_PL_DestructorCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_CertPolicyQualifier_Destroy(
        PKIX_PL_Object *object,
        void *plContext)
{
        PKIX_PL_CertPolicyQualifier *certPQ = NULL;

        PKIX_ENTER(CERTPOLICYQUALIFIER, "pkix_pl_CertPolicyQualifier_Destroy");

        PKIX_NULLCHECK_ONE(object);

        PKIX_CHECK(pkix_CheckType
                (object, PKIX_CERTPOLICYQUALIFIER_TYPE, plContext),
                PKIX_OBJECTNOTCERTPOLICYQUALIFIER);

        certPQ = (PKIX_PL_CertPolicyQualifier*)object;

        PKIX_DECREF(certPQ->policyQualifierId);
        PKIX_DECREF(certPQ->qualifier);

cleanup:

        PKIX_RETURN(CERTPOLICYQUALIFIER);
}

/*
 * FUNCTION: pkix_pl_CertPolicyQualifier_ToString
 * (see comments for PKIX_PL_ToStringCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_CertPolicyQualifier_ToString(
        PKIX_PL_Object *object,
        PKIX_PL_String **pString,
        void *plContext)
{
        PKIX_PL_CertPolicyQualifier *certPQ = NULL;
        char *asciiFormat = "%s:%s";
        PKIX_PL_String *formatString = NULL;
        PKIX_PL_String *pqIDString = NULL;
        PKIX_PL_String *pqValString = NULL;
        PKIX_PL_String *outString = NULL;

        PKIX_ENTER(CERTPOLICYQUALIFIER, "pkix_pl_CertPolicyQualifier_ToString");

        PKIX_NULLCHECK_TWO(object, pString);

        PKIX_CHECK(pkix_CheckType
                (object, PKIX_CERTPOLICYQUALIFIER_TYPE, plContext),
                PKIX_OBJECTNOTCERTPOLICYQUALIFIER);

        certPQ = (PKIX_PL_CertPolicyQualifier *)object;

        /*
         * The policyQualifierId is required. If there is no qualifier,
         * we should have a ByteArray of zero length.
         */
        PKIX_NULLCHECK_TWO(certPQ->policyQualifierId, certPQ->qualifier);

        PKIX_CHECK(PKIX_PL_String_Create
                (PKIX_ESCASCII, asciiFormat, 0, &formatString, plContext),
                PKIX_STRINGCREATEFAILED);

        PKIX_TOSTRING(certPQ->policyQualifierId, &pqIDString, plContext,
                PKIX_OIDTOSTRINGFAILED);

        PKIX_CHECK(pkix_pl_ByteArray_ToHexString
                (certPQ->qualifier, &pqValString, plContext),
                PKIX_BYTEARRAYTOHEXSTRINGFAILED);

        PKIX_CHECK(PKIX_PL_Sprintf
                (&outString, plContext, formatString, pqIDString, pqValString),
                PKIX_SPRINTFFAILED);

        *pString = outString;

cleanup:

        PKIX_DECREF(formatString);
        PKIX_DECREF(pqIDString);
        PKIX_DECREF(pqValString);
        PKIX_RETURN(CERTPOLICYQUALIFIER);
}

/*
 * FUNCTION: pkix_pl_CertPolicyQualifier_Hashcode
 * (see comments for PKIX_PL_HashcodeCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_CertPolicyQualifier_Hashcode(
        PKIX_PL_Object *object,
        PKIX_UInt32 *pHashcode,
        void *plContext)
{
        PKIX_PL_CertPolicyQualifier *certPQ = NULL;
        PKIX_UInt32 cpidHash = 0;
        PKIX_UInt32 cpqHash = 0;

        PKIX_ENTER(CERTPOLICYQUALIFIER, "pkix_pl_CertPolicyQualifier_Hashcode");
        PKIX_NULLCHECK_TWO(object, pHashcode);

        PKIX_CHECK(pkix_CheckType
                (object, PKIX_CERTPOLICYQUALIFIER_TYPE, plContext),
                PKIX_OBJECTNOTCERTPOLICYQUALIFIER);

        certPQ = (PKIX_PL_CertPolicyQualifier *)object;

        PKIX_NULLCHECK_TWO(certPQ->policyQualifierId, certPQ->qualifier);

        PKIX_HASHCODE(certPQ->policyQualifierId, &cpidHash, plContext,
                PKIX_ERRORINOIDHASHCODE);

        PKIX_HASHCODE(certPQ->qualifier, &cpqHash, plContext,
                PKIX_ERRORINBYTEARRAYHASHCODE);

        *pHashcode = cpidHash*31 + cpqHash;

cleanup:

        PKIX_RETURN(CERTPOLICYQUALIFIER);
}


/*
 * FUNCTION: pkix_pl_CertPolicyQualifier_Equals
 * (see comments for PKIX_PL_Equals_Callback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_CertPolicyQualifier_Equals(
        PKIX_PL_Object *firstObject,
        PKIX_PL_Object *secondObject,
        PKIX_Boolean *pResult,
        void *plContext)
{
        PKIX_PL_CertPolicyQualifier *firstCPQ = NULL;
        PKIX_PL_CertPolicyQualifier *secondCPQ = NULL;
        PKIX_UInt32 secondType = 0;
        PKIX_Boolean compare = PKIX_FALSE;

        PKIX_ENTER(CERTPOLICYQUALIFIER, "pkix_pl_CertPolicyQualifier_Equals");
        PKIX_NULLCHECK_THREE(firstObject, secondObject, pResult);

        /* test that firstObject is a CertPolicyQualifier */
        PKIX_CHECK(pkix_CheckType
                (firstObject, PKIX_CERTPOLICYQUALIFIER_TYPE, plContext),
                PKIX_FIRSTOBJECTNOTCERTPOLICYQUALIFIER);

        /*
         * Since we know firstObject is a CertPolicyQualifier,
         * if both references are identical, they must be equal
         */
        if (firstObject == secondObject){
                *pResult = PKIX_TRUE;
                goto cleanup;
        }

        /*
         * If secondObject isn't a CertPolicyQualifier, we
         * don't throw an error. We simply return FALSE.
         */
        PKIX_CHECK(PKIX_PL_Object_GetType
                (secondObject, &secondType, plContext),
                PKIX_COULDNOTGETTYPEOFSECONDARGUMENT);
        if (secondType != PKIX_CERTPOLICYQUALIFIER_TYPE) {
                *pResult = PKIX_FALSE;
                goto cleanup;
        }

        firstCPQ = (PKIX_PL_CertPolicyQualifier *)firstObject;
        secondCPQ = (PKIX_PL_CertPolicyQualifier *)secondObject;

        /*
         * Compare the value of the OID components
         */

        PKIX_NULLCHECK_TWO
                (firstCPQ->policyQualifierId, secondCPQ->policyQualifierId);

        PKIX_EQUALS
                (firstCPQ->policyQualifierId,
                secondCPQ->policyQualifierId,
                &compare,
                plContext,
                PKIX_OIDEQUALSFAILED);

        /*
         * If the OIDs did not match, we don't need to
         * compare the ByteArrays. If the OIDs did match,
         * the return value is the value of the
         * ByteArray comparison.
         */
        if (compare) {
                PKIX_NULLCHECK_TWO(firstCPQ->qualifier, secondCPQ->qualifier);

                PKIX_EQUALS
                        (firstCPQ->qualifier,
                        secondCPQ->qualifier,
                        &compare,
                        plContext,
                        PKIX_BYTEARRAYEQUALSFAILED);
        }

        *pResult = compare;

cleanup:

        PKIX_RETURN(CERTPOLICYQUALIFIER);
}

/*
 * FUNCTION: pkix_pl_CertPolicyQualifier_RegisterSelf
 * DESCRIPTION:
 *  Registers PKIX_CERTPOLICYQUALIFIER_TYPE and its related
 *  functions with systemClasses[]
 * THREAD SAFETY:
 *  Not Thread Safe - for performance and complexity reasons
 *
 *  Since this function is only called by PKIX_PL_Initialize,
 *  which should only be called once, it is acceptable that
 *  this function is not thread-safe.
 */
PKIX_Error *
pkix_pl_CertPolicyQualifier_RegisterSelf(void *plContext)
{

        extern pkix_ClassTable_Entry systemClasses[PKIX_NUMTYPES];
        pkix_ClassTable_Entry entry;

        PKIX_ENTER(CERTPOLICYQUALIFIER,
                "pkix_pl_CertPolicyQualifier_RegisterSelf");

        entry.description = "CertPolicyQualifier";
        entry.objCounter = 0;
        entry.typeObjectSize = sizeof(PKIX_PL_CertPolicyQualifier);
        entry.destructor = pkix_pl_CertPolicyQualifier_Destroy;
        entry.equalsFunction = pkix_pl_CertPolicyQualifier_Equals;
        entry.hashcodeFunction = pkix_pl_CertPolicyQualifier_Hashcode;
        entry.toStringFunction = pkix_pl_CertPolicyQualifier_ToString;
        entry.comparator = NULL;
        entry.duplicateFunction = pkix_duplicateImmutable;

        systemClasses[PKIX_CERTPOLICYQUALIFIER_TYPE] = entry;

        PKIX_RETURN(CERTPOLICYQUALIFIER);
}

/* --Public-CertPolicyQualifier-Functions------------------------- */

/*
 * FUNCTION: PKIX_PL_PolicyQualifier_GetPolicyQualifierId
 * (see comments in pkix_pl_pki.h)
 */
PKIX_Error *
PKIX_PL_PolicyQualifier_GetPolicyQualifierId(
        PKIX_PL_CertPolicyQualifier *policyQualifierInfo,
        PKIX_PL_OID **pPolicyQualifierId,
        void *plContext)
{
        PKIX_ENTER(CERTPOLICYQUALIFIER,
                "PKIX_PL_PolicyQualifier_GetPolicyQualifierId");

        PKIX_NULLCHECK_TWO(policyQualifierInfo, pPolicyQualifierId);

        PKIX_INCREF(policyQualifierInfo->policyQualifierId);

        *pPolicyQualifierId = policyQualifierInfo->policyQualifierId;

cleanup:
        PKIX_RETURN(CERTPOLICYQUALIFIER);
}

/*
 * FUNCTION: PKIX_PL_PolicyQualifier_GetQualifier
 * (see comments in pkix_pl_pki.h)
 */
PKIX_Error *
PKIX_PL_PolicyQualifier_GetQualifier(
        PKIX_PL_CertPolicyQualifier *policyQualifierInfo,
        PKIX_PL_ByteArray **pQualifier,
        void *plContext)
{
        PKIX_ENTER(CERTPOLICYQUALIFIER, "PKIX_PL_PolicyQualifier_GetQualifier");

        PKIX_NULLCHECK_TWO(policyQualifierInfo, pQualifier);

        PKIX_INCREF(policyQualifierInfo->qualifier);

        *pQualifier = policyQualifierInfo->qualifier;

cleanup:
        PKIX_RETURN(CERTPOLICYQUALIFIER);
}
