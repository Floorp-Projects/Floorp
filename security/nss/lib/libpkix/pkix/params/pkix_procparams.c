/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * pkix_procparams.c
 *
 * ProcessingParams Object Functions
 *
 */

#include "pkix_procparams.h"

/* --Private-Functions-------------------------------------------- */

/*
 * FUNCTION: pkix_ProcessingParams_Destroy
 * (see comments for PKIX_PL_DestructorCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_ProcessingParams_Destroy(
        PKIX_PL_Object *object,
        void *plContext)
{
        PKIX_ProcessingParams *params = NULL;

        PKIX_ENTER(PROCESSINGPARAMS, "pkix_ProcessingParams_Destroy");
        PKIX_NULLCHECK_ONE(object);

        /* Check that this object is a processing params object */
        PKIX_CHECK(pkix_CheckType
                    (object, PKIX_PROCESSINGPARAMS_TYPE, plContext),
                    PKIX_OBJECTNOTPROCESSINGPARAMS);

        params = (PKIX_ProcessingParams *)object;

        PKIX_DECREF(params->trustAnchors);
        PKIX_DECREF(params->hintCerts);
        PKIX_DECREF(params->constraints);
        PKIX_DECREF(params->date);
        PKIX_DECREF(params->initialPolicies);
        PKIX_DECREF(params->certChainCheckers);
        PKIX_DECREF(params->revChecker);
        PKIX_DECREF(params->certStores);
        PKIX_DECREF(params->resourceLimits);

cleanup:

        PKIX_RETURN(PROCESSINGPARAMS);
}

/*
 * FUNCTION: pkix_ProcessingParams_Equals
 * (see comments for PKIX_PL_EqualsCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_ProcessingParams_Equals(
        PKIX_PL_Object *first,
        PKIX_PL_Object *second,
        PKIX_Boolean *pResult,
        void *plContext)
{
        PKIX_UInt32 secondType;
        PKIX_Boolean cmpResult;
        PKIX_ProcessingParams *firstProcParams = NULL;
        PKIX_ProcessingParams *secondProcParams = NULL;

        PKIX_ENTER(PROCESSINGPARAMS, "pkix_ProcessingParams_Equals");
        PKIX_NULLCHECK_THREE(first, second, pResult);

        PKIX_CHECK(pkix_CheckType(first, PKIX_PROCESSINGPARAMS_TYPE, plContext),
                    PKIX_FIRSTOBJECTNOTPROCESSINGPARAMS);

        PKIX_CHECK(PKIX_PL_Object_GetType(second, &secondType, plContext),
                    PKIX_COULDNOTGETTYPEOFSECONDARGUMENT);

        *pResult = PKIX_FALSE;

        if (secondType != PKIX_PROCESSINGPARAMS_TYPE) goto cleanup;

        firstProcParams = (PKIX_ProcessingParams *)first;
        secondProcParams = (PKIX_ProcessingParams *)second;

        /* Do the simplest tests first */
        if ((firstProcParams->qualifiersRejected) !=
            (secondProcParams->qualifiersRejected)) {
                goto cleanup;
        }

        if (firstProcParams->isCrlRevocationCheckingEnabled !=
            secondProcParams->isCrlRevocationCheckingEnabled) {
                goto cleanup;
        }
        if (firstProcParams->isCrlRevocationCheckingEnabledWithNISTPolicy !=
            secondProcParams->isCrlRevocationCheckingEnabledWithNISTPolicy) {
                goto cleanup;
        }

        /* trustAnchors can never be NULL */

        PKIX_EQUALS
                (firstProcParams->trustAnchors,
                secondProcParams->trustAnchors,
                &cmpResult,
                plContext,
                PKIX_OBJECTEQUALSFAILED);

        if (!cmpResult) goto cleanup;

        PKIX_EQUALS
                (firstProcParams->hintCerts,
                secondProcParams->hintCerts,
                &cmpResult,
                plContext,
                PKIX_OBJECTEQUALSFAILED);

        if (!cmpResult) goto cleanup;

        PKIX_EQUALS
                (firstProcParams->date,
                secondProcParams->date,
                &cmpResult,
                plContext,
                PKIX_OBJECTEQUALSFAILED);

        if (!cmpResult) goto cleanup;

        PKIX_EQUALS
                (firstProcParams->constraints,
                secondProcParams->constraints,
                &cmpResult,
                plContext,
                PKIX_OBJECTEQUALSFAILED);

        if (!cmpResult) goto cleanup;

        PKIX_EQUALS
                (firstProcParams->initialPolicies,
                secondProcParams->initialPolicies,
                &cmpResult,
                plContext,
                PKIX_OBJECTEQUALSFAILED);

        if (!cmpResult) goto cleanup;

        /* There is no Equals function for CertChainCheckers */

        PKIX_EQUALS
                    ((PKIX_PL_Object *)firstProcParams->certStores,
                    (PKIX_PL_Object *)secondProcParams->certStores,
                    &cmpResult,
                    plContext,
                    PKIX_OBJECTEQUALSFAILED);

        if (!cmpResult) goto cleanup;

        PKIX_EQUALS
                (firstProcParams->resourceLimits,
                secondProcParams->resourceLimits,
                &cmpResult,
                plContext,
                PKIX_OBJECTEQUALSFAILED);

        if (cmpResult == PKIX_FALSE) {
                *pResult = PKIX_FALSE;
                goto cleanup;
        }

        *pResult = cmpResult;

cleanup:

        PKIX_RETURN(PROCESSINGPARAMS);
}

/*
 * FUNCTION: pkix_ProcessingParams_Hashcode
 * (see comments for PKIX_PL_HashcodeCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_ProcessingParams_Hashcode(
        PKIX_PL_Object *object,
        PKIX_UInt32 *pHashcode,
        void *plContext)
{
        PKIX_ProcessingParams *procParams = NULL;
        PKIX_UInt32 hash = 0;
        PKIX_UInt32 anchorsHash = 0;
        PKIX_UInt32 hintCertsHash = 0;
        PKIX_UInt32 dateHash = 0;
        PKIX_UInt32 constraintsHash = 0;
        PKIX_UInt32 initialHash = 0;
        PKIX_UInt32 rejectedHash = 0;
        PKIX_UInt32 certChainCheckersHash = 0;
        PKIX_UInt32 revCheckerHash = 0;
        PKIX_UInt32 certStoresHash = 0;
        PKIX_UInt32 resourceLimitsHash = 0;

        PKIX_ENTER(PROCESSINGPARAMS, "pkix_ProcessingParams_Hashcode");
        PKIX_NULLCHECK_TWO(object, pHashcode);

        PKIX_CHECK(pkix_CheckType
                    (object, PKIX_PROCESSINGPARAMS_TYPE, plContext),
                    PKIX_OBJECTNOTPROCESSINGPARAMS);

        procParams = (PKIX_ProcessingParams*)object;

        PKIX_HASHCODE(procParams->trustAnchors, &anchorsHash, plContext,
                PKIX_OBJECTHASHCODEFAILED);

        PKIX_HASHCODE(procParams->hintCerts, &hintCertsHash, plContext,
                PKIX_OBJECTHASHCODEFAILED);

        PKIX_HASHCODE(procParams->date, &dateHash, plContext,
                PKIX_OBJECTHASHCODEFAILED);

        PKIX_HASHCODE(procParams->constraints, &constraintsHash, plContext,
                PKIX_OBJECTHASHCODEFAILED);

        PKIX_HASHCODE(procParams->initialPolicies, &initialHash, plContext,
                PKIX_OBJECTHASHCODEFAILED);

        rejectedHash = procParams->qualifiersRejected;

        /* There is no Hash function for CertChainCheckers */

        PKIX_HASHCODE(procParams->certStores, &certStoresHash, plContext,
                PKIX_OBJECTHASHCODEFAILED);

        PKIX_HASHCODE(procParams->resourceLimits,
                &resourceLimitsHash,
                plContext,
                PKIX_OBJECTHASHCODEFAILED);

        hash = (31 * ((31 * anchorsHash) + hintCertsHash + dateHash)) +
                constraintsHash + initialHash + rejectedHash;

        hash += ((((certStoresHash + resourceLimitsHash) << 7) +
                certChainCheckersHash + revCheckerHash +
                procParams->isCrlRevocationCheckingEnabled +
                procParams->isCrlRevocationCheckingEnabledWithNISTPolicy) << 7);

        *pHashcode = hash;

cleanup:

        PKIX_RETURN(PROCESSINGPARAMS);
}

/*
 * FUNCTION: pkix_ProcessingParams_ToString
 * (see comments for PKIX_PL_ToStringCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_ProcessingParams_ToString(
        PKIX_PL_Object *object,
        PKIX_PL_String **pString,
        void *plContext)
{
        PKIX_ProcessingParams *procParams = NULL;
        char *asciiFormat = NULL;
        PKIX_PL_String *formatString = NULL;
        PKIX_PL_String *procParamsString = NULL;
        PKIX_PL_String *anchorsString = NULL;
        PKIX_PL_String *dateString = NULL;
        PKIX_PL_String *constraintsString = NULL;
        PKIX_PL_String *InitialPoliciesString = NULL;
        PKIX_PL_String *qualsRejectedString = NULL;
        PKIX_List *certStores = NULL;
        PKIX_PL_String *certStoresString = NULL;
        PKIX_PL_String *resourceLimitsString = NULL;

        PKIX_ENTER(PROCESSINGPARAMS, "pkix_ProcessingParams_ToString");
        PKIX_NULLCHECK_TWO(object, pString);

        PKIX_CHECK(pkix_CheckType
                    (object, PKIX_PROCESSINGPARAMS_TYPE, plContext),
                    PKIX_OBJECTNOTPROCESSINGPARAMS);

        asciiFormat =
                "[\n"
                "\tTrust Anchors: \n"
                "\t********BEGIN LIST OF TRUST ANCHORS********\n"
                "\t\t%s\n"
                "\t********END LIST OF TRUST ANCHORS********\n"
                "\tDate:    \t\t%s\n"
                "\tTarget Constraints:    %s\n"
                "\tInitial Policies:      %s\n"
                "\tQualifiers Rejected:   %s\n"
                "\tCert Stores:           %s\n"
                "\tResource Limits:       %s\n"
                "\tCRL Checking Enabled:  %d\n"
                "]\n";

        PKIX_CHECK(PKIX_PL_String_Create
                    (PKIX_ESCASCII,
                    asciiFormat,
                    0,
                    &formatString,
                    plContext),
                    PKIX_STRINGCREATEFAILED);

        procParams = (PKIX_ProcessingParams*)object;

        PKIX_TOSTRING(procParams->trustAnchors, &anchorsString, plContext,
                PKIX_OBJECTTOSTRINGFAILED);

        PKIX_TOSTRING(procParams->date, &dateString, plContext,
                PKIX_OBJECTTOSTRINGFAILED);

        PKIX_TOSTRING(procParams->constraints, &constraintsString, plContext,
                PKIX_OBJECTTOSTRINGFAILED);

        PKIX_TOSTRING
                (procParams->initialPolicies, &InitialPoliciesString, plContext,
                PKIX_OBJECTTOSTRINGFAILED);

        PKIX_CHECK(PKIX_PL_String_Create
                (PKIX_ESCASCII,
                (procParams->qualifiersRejected)?"TRUE":"FALSE",
                0,
                &qualsRejectedString,
                plContext),
                PKIX_STRINGCREATEFAILED);

        /* There is no ToString function for CertChainCheckers */

       PKIX_CHECK(PKIX_ProcessingParams_GetCertStores
                (procParams, &certStores, plContext),
                PKIX_PROCESSINGPARAMSGETCERTSTORESFAILED);

        PKIX_TOSTRING(certStores, &certStoresString, plContext,
                PKIX_LISTTOSTRINGFAILED);

        PKIX_TOSTRING(procParams->resourceLimits,
                &resourceLimitsString,
                plContext,
                PKIX_OBJECTTOSTRINGFAILED);

        PKIX_CHECK(PKIX_PL_Sprintf
                (&procParamsString,
                plContext,
                formatString,
                anchorsString,
                dateString,
                constraintsString,
                InitialPoliciesString,
                qualsRejectedString,
                certStoresString,
                resourceLimitsString,
                procParams->isCrlRevocationCheckingEnabled,
                procParams->isCrlRevocationCheckingEnabledWithNISTPolicy),
                PKIX_SPRINTFFAILED);

        *pString = procParamsString;

cleanup:

        PKIX_DECREF(formatString);
        PKIX_DECREF(anchorsString);
        PKIX_DECREF(dateString);
        PKIX_DECREF(constraintsString);
        PKIX_DECREF(InitialPoliciesString);
        PKIX_DECREF(qualsRejectedString);
        PKIX_DECREF(certStores);
        PKIX_DECREF(certStoresString);
        PKIX_DECREF(resourceLimitsString);

        PKIX_RETURN(PROCESSINGPARAMS);
}

/*
 * FUNCTION: pkix_ProcessingParams_Duplicate
 * (see comments for PKIX_PL_DuplicateCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_ProcessingParams_Duplicate(
        PKIX_PL_Object *object,
        PKIX_PL_Object **pNewObject,
        void *plContext)
{
        PKIX_ProcessingParams *params = NULL;
        PKIX_ProcessingParams *paramsDuplicate = NULL;

        PKIX_ENTER(PROCESSINGPARAMS, "pkix_ProcessingParams_Duplicate");
        PKIX_NULLCHECK_TWO(object, pNewObject);

        PKIX_CHECK(pkix_CheckType
                (object, PKIX_PROCESSINGPARAMS_TYPE, plContext),
                PKIX_OBJECTNOTPROCESSINGPARAMS);

        params = (PKIX_ProcessingParams *)object;

        PKIX_CHECK(PKIX_PL_Object_Alloc
                (PKIX_PROCESSINGPARAMS_TYPE,
                sizeof (PKIX_ProcessingParams),
                (PKIX_PL_Object **)&paramsDuplicate,
                plContext),
                PKIX_PROCESSINGPARAMSCREATEFAILED);

        /* initialize fields */
        PKIX_DUPLICATE
                (params->trustAnchors,
                &(paramsDuplicate->trustAnchors),
                plContext,
                PKIX_OBJECTDUPLICATEFAILED);

        PKIX_DUPLICATE
                (params->hintCerts, &(paramsDuplicate->hintCerts), plContext,
                PKIX_OBJECTDUPLICATEFAILED);

        PKIX_DUPLICATE
                (params->constraints,
                &(paramsDuplicate->constraints),
                plContext,
                PKIX_OBJECTDUPLICATEFAILED);

        PKIX_DUPLICATE
                (params->date, &(paramsDuplicate->date), plContext,
                PKIX_OBJECTDUPLICATEFAILED);

        PKIX_DUPLICATE
                (params->initialPolicies,
                &(paramsDuplicate->initialPolicies),
                plContext,
                PKIX_OBJECTDUPLICATEFAILED);

        paramsDuplicate->initialPolicyMappingInhibit =
                params->initialPolicyMappingInhibit;
        paramsDuplicate->initialAnyPolicyInhibit =
                params->initialAnyPolicyInhibit;
        paramsDuplicate->initialExplicitPolicy = params->initialExplicitPolicy;
        paramsDuplicate->qualifiersRejected = params->qualifiersRejected;

        PKIX_DUPLICATE
                (params->certChainCheckers,
                &(paramsDuplicate->certChainCheckers),
                plContext,
                PKIX_OBJECTDUPLICATEFAILED);

        PKIX_DUPLICATE
                (params->revChecker,
                &(paramsDuplicate->revChecker),
                plContext,
                PKIX_OBJECTDUPLICATEFAILED);

        PKIX_DUPLICATE
                (params->certStores, &(paramsDuplicate->certStores), plContext,
                PKIX_OBJECTDUPLICATEFAILED);

        PKIX_DUPLICATE
                (params->resourceLimits,
                &(paramsDuplicate->resourceLimits),
                plContext,
                PKIX_OBJECTDUPLICATEFAILED);

        paramsDuplicate->isCrlRevocationCheckingEnabled =
                params->isCrlRevocationCheckingEnabled;

        paramsDuplicate->isCrlRevocationCheckingEnabledWithNISTPolicy =
                params->isCrlRevocationCheckingEnabledWithNISTPolicy;

        *pNewObject = (PKIX_PL_Object *)paramsDuplicate;

cleanup:

        if (PKIX_ERROR_RECEIVED){
                PKIX_DECREF(paramsDuplicate);
        }

        PKIX_RETURN(PROCESSINGPARAMS);

}

/*
 * FUNCTION: pkix_ProcessingParams_RegisterSelf
 * DESCRIPTION:
 *  Registers PKIX_PROCESSINGPARAMS_TYPE and its related functions with
 *  systemClasses[]
 * THREAD SAFETY:
 *  Not Thread Safe - for performance and complexity reasons
 *
 *  Since this function is only called by PKIX_PL_Initialize, which should
 *  only be called once, it is acceptable that this function is not
 *  thread-safe.
 */
PKIX_Error *
pkix_ProcessingParams_RegisterSelf(void *plContext)
{
        extern pkix_ClassTable_Entry systemClasses[PKIX_NUMTYPES];
        pkix_ClassTable_Entry entry;

        PKIX_ENTER(PROCESSINGPARAMS, "pkix_ProcessingParams_RegisterSelf");

        entry.description = "ProcessingParams";
        entry.objCounter = 0;
        entry.typeObjectSize = sizeof(PKIX_ProcessingParams);
        entry.destructor = pkix_ProcessingParams_Destroy;
        entry.equalsFunction = pkix_ProcessingParams_Equals;
        entry.hashcodeFunction = pkix_ProcessingParams_Hashcode;
        entry.toStringFunction = pkix_ProcessingParams_ToString;
        entry.comparator = NULL;
        entry.duplicateFunction = pkix_ProcessingParams_Duplicate;

        systemClasses[PKIX_PROCESSINGPARAMS_TYPE] = entry;

        PKIX_RETURN(PROCESSINGPARAMS);
}

/* --Public-Functions--------------------------------------------- */

/*
 * FUNCTION: PKIX_ProcessingParams_Create (see comments in pkix_params.h)
 */
PKIX_Error *
PKIX_ProcessingParams_Create(
        PKIX_ProcessingParams **pParams,
        void *plContext)
{
        PKIX_ProcessingParams *params = NULL;

        PKIX_ENTER(PROCESSINGPARAMS, "PKIX_ProcessingParams_Create");
        PKIX_NULLCHECK_ONE(pParams);

        PKIX_CHECK(PKIX_PL_Object_Alloc
                    (PKIX_PROCESSINGPARAMS_TYPE,
                    sizeof (PKIX_ProcessingParams),
                    (PKIX_PL_Object **)&params,
                    plContext),
                    PKIX_COULDNOTCREATEPROCESSINGPARAMSOBJECT);

        /* initialize fields */
        PKIX_CHECK(PKIX_List_Create(&params->trustAnchors, plContext),
                   PKIX_LISTCREATEFAILED);
        PKIX_CHECK(PKIX_List_SetImmutable(params->trustAnchors, plContext),
                    PKIX_LISTSETIMMUTABLEFAILED);

        PKIX_CHECK(PKIX_PL_Date_Create_UTCTime
                   (NULL, &params->date, plContext),
                   PKIX_DATECREATEUTCTIMEFAILED);

        params->hintCerts = NULL;
        params->constraints = NULL;
        params->initialPolicies = NULL;
        params->initialPolicyMappingInhibit = PKIX_FALSE;
        params->initialAnyPolicyInhibit = PKIX_FALSE;
        params->initialExplicitPolicy = PKIX_FALSE;
        params->qualifiersRejected = PKIX_FALSE;
        params->certChainCheckers = NULL;
        params->revChecker = NULL;
        params->certStores = NULL;
        params->resourceLimits = NULL;

        params->isCrlRevocationCheckingEnabled = PKIX_TRUE;

        params->isCrlRevocationCheckingEnabledWithNISTPolicy = PKIX_TRUE;

        params->useAIAForCertFetching = PKIX_FALSE;
        params->qualifyTargetCert = PKIX_TRUE;
        params->useOnlyTrustAnchors = PKIX_TRUE;

        *pParams = params;
        params = NULL;

cleanup:

        PKIX_DECREF(params);

        PKIX_RETURN(PROCESSINGPARAMS);

}

/*
 * FUNCTION: PKIX_ProcessingParams_GetUseAIAForCertFetching
 * (see comments in pkix_params.h)
 */
PKIX_Error *
PKIX_ProcessingParams_GetUseAIAForCertFetching(
        PKIX_ProcessingParams *params,
        PKIX_Boolean *pUseAIA,  /* list of TrustAnchor */
        void *plContext)
{
        PKIX_ENTER(PROCESSINGPARAMS, "PKIX_ProcessingParams_GetUseAIAForCertFetching");
        PKIX_NULLCHECK_TWO(params, pUseAIA);

        *pUseAIA = params->useAIAForCertFetching;

        PKIX_RETURN(PROCESSINGPARAMS);
}

/*
 * FUNCTION: PKIX_ProcessingParams_SetUseAIAForCertFetching
 * (see comments in pkix_params.h)
 */
PKIX_Error *
PKIX_ProcessingParams_SetUseAIAForCertFetching(
        PKIX_ProcessingParams *params,
        PKIX_Boolean useAIA,  
        void *plContext)
{
        PKIX_ENTER(PROCESSINGPARAMS, "PKIX_ProcessingParams_SetUseAIAForCertFetching");
        PKIX_NULLCHECK_ONE(params);

        params->useAIAForCertFetching = useAIA;

        PKIX_RETURN(PROCESSINGPARAMS);
}

/*
 * FUNCTION: PKIX_ProcessingParams_GetQualifyTargetCert
 * (see comments in pkix_params.h)
 */
PKIX_Error *
PKIX_ProcessingParams_GetValidateTargetCert(
        PKIX_ProcessingParams *params,
        PKIX_Boolean *pQualifyTargetCert,
        void *plContext)
{
        PKIX_ENTER(PROCESSINGPARAMS,
                   "PKIX_ProcessingParams_GetValidateTargetCert");
        PKIX_NULLCHECK_TWO(params, pQualifyTargetCert);

        *pQualifyTargetCert = params->qualifyTargetCert;

        PKIX_RETURN(PROCESSINGPARAMS);
}

/*
 * FUNCTION: PKIX_ProcessingParams_SetQualifyTargetCert
 * (see comments in pkix_params.h)
 */
PKIX_Error *
PKIX_ProcessingParams_SetQualifyTargetCert(
        PKIX_ProcessingParams *params,
        PKIX_Boolean qualifyTargetCert,
        void *plContext)
{
        PKIX_ENTER(PROCESSINGPARAMS,
                   "PKIX_ProcessingParams_SetQualifyTargetCert");
        PKIX_NULLCHECK_ONE(params);

        params->qualifyTargetCert = qualifyTargetCert;

        PKIX_RETURN(PROCESSINGPARAMS);
}

/*
 * FUNCTION: PKIX_ProcessingParams_SetTrustAnchors
 * (see comments in pkix_params.h)
 */
PKIX_Error *
PKIX_ProcessingParams_SetTrustAnchors(
        PKIX_ProcessingParams *params,
        PKIX_List *anchors,  /* list of TrustAnchor */
        void *plContext)
{
        PKIX_ENTER(PROCESSINGPARAMS, "PKIX_ProcessingParams_SetTrustAnchors");
        PKIX_NULLCHECK_TWO(params, anchors);

        PKIX_DECREF(params->trustAnchors);

        PKIX_INCREF(anchors);
        params->trustAnchors = anchors;
        PKIX_CHECK(PKIX_List_SetImmutable(params->trustAnchors, plContext),
                    PKIX_LISTSETIMMUTABLEFAILED);

cleanup:
        PKIX_RETURN(PROCESSINGPARAMS);
}

/*
 * FUNCTION: PKIX_ProcessingParams_GetTrustAnchors
 * (see comments in pkix_params.h)
 */
PKIX_Error *
PKIX_ProcessingParams_GetTrustAnchors(
        PKIX_ProcessingParams *params,
        PKIX_List **pAnchors,  /* list of TrustAnchor */
        void *plContext)
{
        PKIX_ENTER(PROCESSINGPARAMS, "PKIX_ProcessingParams_GetTrustAnchors");
        PKIX_NULLCHECK_TWO(params, pAnchors);

        PKIX_INCREF(params->trustAnchors);

        *pAnchors = params->trustAnchors;

cleanup:
        PKIX_RETURN(PROCESSINGPARAMS);
}

/**
 * FUNCTION: PKIX_ProcessingParams_SetUseOnlyTrustAnchors
 * (see comments in pkix_params.h)
 */
PKIX_Error *
PKIX_ProcessingParams_GetUseOnlyTrustAnchors(
        PKIX_ProcessingParams *params,
        PKIX_Boolean *pUseOnlyTrustAnchors,
        void *plContext)
{
        PKIX_ENTER(PROCESSINGPARAMS,
                   "PKIX_ProcessingParams_SetUseTrustAnchorsOnly");
        PKIX_NULLCHECK_TWO(params, pUseOnlyTrustAnchors);

        *pUseOnlyTrustAnchors = params->useOnlyTrustAnchors;

        PKIX_RETURN(PROCESSINGPARAMS);
}

/**
 * FUNCTION: PKIX_ProcessingParams_SetUseOnlyTrustAnchors
 * (see comments in pkix_params.h)
 */
PKIX_Error *
PKIX_ProcessingParams_SetUseOnlyTrustAnchors(
        PKIX_ProcessingParams *params,
        PKIX_Boolean useOnlyTrustAnchors,
        void *plContext)
{
        PKIX_ENTER(PROCESSINGPARAMS,
                   "PKIX_ProcessingParams_SetUseTrustAnchorsOnly");
        PKIX_NULLCHECK_ONE(params);

        params->useOnlyTrustAnchors = useOnlyTrustAnchors;

        PKIX_RETURN(PROCESSINGPARAMS);
}

/*
 * FUNCTION: PKIX_ProcessingParams_GetDate (see comments in pkix_params.h)
 */
PKIX_Error *
PKIX_ProcessingParams_GetDate(
        PKIX_ProcessingParams *params,
        PKIX_PL_Date **pDate,
        void *plContext)
{
        PKIX_ENTER(PROCESSINGPARAMS, "PKIX_ProcessingParams_GetDate");
        PKIX_NULLCHECK_TWO(params, pDate);

        PKIX_INCREF(params->date);
        *pDate = params->date;

cleanup:
        PKIX_RETURN(PROCESSINGPARAMS);
}

/*
 * FUNCTION: PKIX_ProcessingParams_SetDate (see comments in pkix_params.h)
 */
PKIX_Error *
PKIX_ProcessingParams_SetDate(
        PKIX_ProcessingParams *params,
        PKIX_PL_Date *date,
        void *plContext)
{
        PKIX_ENTER(PROCESSINGPARAMS, "PKIX_ProcessingParams_SetDate");
        PKIX_NULLCHECK_ONE(params);

        PKIX_DECREF(params->date);

        PKIX_INCREF(date);
        params->date = date;

        PKIX_CHECK(PKIX_PL_Object_InvalidateCache
                    ((PKIX_PL_Object *)params, plContext),
                    PKIX_OBJECTINVALIDATECACHEFAILED);

cleanup:

        if (PKIX_ERROR_RECEIVED && params) {
            PKIX_DECREF(params->date);
        }

        PKIX_RETURN(PROCESSINGPARAMS);
}

/*
 * FUNCTION: PKIX_ProcessingParams_GetTargetCertConstraints
 * (see comments in pkix_params.h)
 */
PKIX_Error *
PKIX_ProcessingParams_GetTargetCertConstraints(
        PKIX_ProcessingParams *params,
        PKIX_CertSelector **pConstraints,
        void *plContext)
{
        PKIX_ENTER(PROCESSINGPARAMS,
                    "PKIX_ProcessingParams_GetTargetCertConstraints");

        PKIX_NULLCHECK_TWO(params, pConstraints);

        PKIX_INCREF(params->constraints);
        *pConstraints = params->constraints;

cleanup:
        PKIX_RETURN(PROCESSINGPARAMS);
}

/*
 * FUNCTION: PKIX_ProcessingParams_SetTargetCertConstraints
 * (see comments in pkix_params.h)
 */
PKIX_Error *
PKIX_ProcessingParams_SetTargetCertConstraints(
        PKIX_ProcessingParams *params,
        PKIX_CertSelector *constraints,
        void *plContext)
{

        PKIX_ENTER(PROCESSINGPARAMS,
                    "PKIX_ProcessingParams_SetTargetCertConstraints");

        PKIX_NULLCHECK_ONE(params);

        PKIX_DECREF(params->constraints);

        PKIX_INCREF(constraints);
        params->constraints = constraints;

        PKIX_CHECK(PKIX_PL_Object_InvalidateCache
                    ((PKIX_PL_Object *)params, plContext),
                    PKIX_OBJECTINVALIDATECACHEFAILED);

cleanup:
        if (PKIX_ERROR_RECEIVED && params) {
            PKIX_DECREF(params->constraints);
        }

        PKIX_RETURN(PROCESSINGPARAMS);
}

/*
 * FUNCTION: PKIX_ProcessingParams_GetInitialPolicies
 *      (see comments in pkix_params.h)
 */
PKIX_Error *
PKIX_ProcessingParams_GetInitialPolicies(
        PKIX_ProcessingParams *params,
        PKIX_List **pInitPolicies, /* list of PKIX_PL_OID */
        void *plContext)
{

        PKIX_ENTER(PROCESSINGPARAMS,
                "PKIX_ProcessingParams_GetInitialPolicies");

        PKIX_NULLCHECK_TWO(params, pInitPolicies);

        if (params->initialPolicies == NULL) {
                PKIX_CHECK(PKIX_List_Create
                        (&params->initialPolicies, plContext),
                        PKIX_UNABLETOCREATELIST);
                PKIX_CHECK(PKIX_List_SetImmutable
                        (params->initialPolicies, plContext),
                        PKIX_UNABLETOMAKELISTIMMUTABLE);
                PKIX_CHECK(PKIX_PL_Object_InvalidateCache
                        ((PKIX_PL_Object *)params, plContext),
                        PKIX_OBJECTINVALIDATECACHEFAILED);
        }

        PKIX_INCREF(params->initialPolicies);
        *pInitPolicies = params->initialPolicies;

cleanup:

        PKIX_RETURN(PROCESSINGPARAMS);
}

/*
 * FUNCTION: PKIX_ProcessingParams_SetInitialPolicies
 *      (see comments in pkix_params.h)
 */
PKIX_Error *
PKIX_ProcessingParams_SetInitialPolicies(
        PKIX_ProcessingParams *params,
        PKIX_List *initPolicies, /* list of PKIX_PL_OID */
        void *plContext)
{
        PKIX_ENTER(PROCESSINGPARAMS,
                "PKIX_ProcessingParams_SetInitialPolicies");
        PKIX_NULLCHECK_ONE(params);

        PKIX_DECREF(params->initialPolicies);

        PKIX_INCREF(initPolicies);
        params->initialPolicies = initPolicies;

        PKIX_CHECK(PKIX_PL_Object_InvalidateCache
                ((PKIX_PL_Object *)params, plContext),
                PKIX_OBJECTINVALIDATECACHEFAILED);

cleanup:

        if (PKIX_ERROR_RECEIVED && params) {
            PKIX_DECREF(params->initialPolicies);
        }
        PKIX_RETURN(PROCESSINGPARAMS);
}

/*
 * FUNCTION: PKIX_ProcessingParams_GetPolicyQualifiersRejected
 *      (see comments in pkix_params.h)
 */
PKIX_Error *
PKIX_ProcessingParams_GetPolicyQualifiersRejected(
        PKIX_ProcessingParams *params,
        PKIX_Boolean *pRejected,
        void *plContext)
{
        PKIX_ENTER(PROCESSINGPARAMS,
                "PKIX_ProcessingParams_GetPolicyQualifiersRejected");

        PKIX_NULLCHECK_TWO(params, pRejected);

        *pRejected = params->qualifiersRejected;

        PKIX_RETURN(PROCESSINGPARAMS);
}

/*
 * FUNCTION: PKIX_ProcessingParams_SetPolicyQualifiersRejected
 *      (see comments in pkix_params.h)
 */
PKIX_Error *
PKIX_ProcessingParams_SetPolicyQualifiersRejected(
        PKIX_ProcessingParams *params,
        PKIX_Boolean rejected,
        void *plContext)
{
        PKIX_ENTER(PROCESSINGPARAMS,
                "PKIX_ProcessingParams_SetPolicyQualifiersRejected");

        PKIX_NULLCHECK_ONE(params);

        params->qualifiersRejected = rejected;

        PKIX_CHECK(PKIX_PL_Object_InvalidateCache
                ((PKIX_PL_Object *)params, plContext),
                PKIX_OBJECTINVALIDATECACHEFAILED);

cleanup:

        PKIX_RETURN(PROCESSINGPARAMS);
}

/*
 * FUNCTION: PKIX_ProcessingParams_GetCertChainCheckers
 * (see comments in pkix_params.h)
 */
PKIX_Error *
PKIX_ProcessingParams_GetCertChainCheckers(
        PKIX_ProcessingParams *params,
        PKIX_List **pCheckers,  /* list of PKIX_CertChainChecker */
        void *plContext)
{
        PKIX_ENTER(PROCESSINGPARAMS,
                   "PKIX_ProcessingParams_GetCertChainCheckers");
        PKIX_NULLCHECK_TWO(params, pCheckers);

        PKIX_INCREF(params->certChainCheckers);
        *pCheckers = params->certChainCheckers;

cleanup:
        PKIX_RETURN(PROCESSINGPARAMS);
}

/*
 * FUNCTION: PKIX_ProcessingParams_SetCertChainCheckers
 * (see comments in pkix_params.h)
 */
PKIX_Error *
PKIX_ProcessingParams_SetCertChainCheckers(
        PKIX_ProcessingParams *params,
        PKIX_List *checkers,  /* list of PKIX_CertChainChecker */
        void *plContext)
{

        PKIX_ENTER(PROCESSINGPARAMS,
                   "PKIX_ProcessingParams_SetCertChainCheckers");
        PKIX_NULLCHECK_ONE(params);

        PKIX_DECREF(params->certChainCheckers);

        PKIX_INCREF(checkers);
        params->certChainCheckers = checkers;

        PKIX_CHECK(PKIX_PL_Object_InvalidateCache
                ((PKIX_PL_Object *)params, plContext),
                PKIX_OBJECTINVALIDATECACHEFAILED);

cleanup:

        if (PKIX_ERROR_RECEIVED && params) {
            PKIX_DECREF(params->certChainCheckers);
        }

        PKIX_RETURN(PROCESSINGPARAMS);
}

/*
 * FUNCTION: PKIX_ProcessingParams_AddCertChainCheckers
 * (see comments in pkix_params.h)
 */
PKIX_Error *
PKIX_ProcessingParams_AddCertChainChecker(
        PKIX_ProcessingParams *params,
        PKIX_CertChainChecker *checker,
        void *plContext)
{
        PKIX_List *list = NULL;

        PKIX_ENTER(PROCESSINGPARAMS,
                   "PKIX_ProcessingParams_AddCertChainChecker");
        PKIX_NULLCHECK_TWO(params, checker);

        if (params->certChainCheckers == NULL) {

                PKIX_CHECK(PKIX_List_Create(&list, plContext),
                    PKIX_LISTCREATEFAILED);

                params->certChainCheckers = list;
        }

        PKIX_CHECK(PKIX_List_AppendItem
            (params->certChainCheckers, (PKIX_PL_Object *)checker, plContext),
            PKIX_LISTAPPENDITEMFAILED);

        PKIX_CHECK(PKIX_PL_Object_InvalidateCache
            ((PKIX_PL_Object *)params, plContext),
            PKIX_OBJECTINVALIDATECACHEFAILED);

        list = NULL;

cleanup:

        if (list && params) {
            PKIX_DECREF(params->certChainCheckers);
        }

        PKIX_RETURN(PROCESSINGPARAMS);
}

/*
 * FUNCTION: PKIX_ProcessingParams_GetRevocationChecker
 * (see comments in pkix_params.h)
 */
PKIX_Error *
PKIX_ProcessingParams_GetRevocationChecker(
        PKIX_ProcessingParams *params,
        PKIX_RevocationChecker **pChecker,
        void *plContext)
{

        PKIX_ENTER
            (PROCESSINGPARAMS, "PKIX_ProcessingParams_GetRevocationCheckers");
        PKIX_NULLCHECK_TWO(params, pChecker);

        PKIX_INCREF(params->revChecker);
        *pChecker = params->revChecker;

cleanup:

        PKIX_RETURN(PROCESSINGPARAMS);
}

/*
 * FUNCTION: PKIX_ProcessingParams_SetRevocationChecker
 * (see comments in pkix_params.h)
 */
PKIX_Error *
PKIX_ProcessingParams_SetRevocationChecker(
        PKIX_ProcessingParams *params,
        PKIX_RevocationChecker *checker,
        void *plContext)
{

        PKIX_ENTER(PROCESSINGPARAMS,
                   "PKIX_ProcessingParams_InitRevocationChecker");
        PKIX_NULLCHECK_ONE(params);

        PKIX_DECREF(params->revChecker);
        PKIX_INCREF(checker);
        params->revChecker = checker;

        PKIX_CHECK(PKIX_PL_Object_InvalidateCache
                ((PKIX_PL_Object *)params, plContext),
                PKIX_OBJECTINVALIDATECACHEFAILED);
cleanup:

        PKIX_RETURN(PROCESSINGPARAMS);
}

/*
 * FUNCTION: PKIX_ProcessingParams_GetCertStores
 * (see comments in pkix_params.h)
 */
PKIX_Error *
PKIX_ProcessingParams_GetCertStores(
        PKIX_ProcessingParams *params,
        PKIX_List **pStores,  /* list of PKIX_CertStore */
        void *plContext)
{
        PKIX_ENTER(PROCESSINGPARAMS, "PKIX_ProcessingParams_GetCertStores");

        PKIX_NULLCHECK_TWO(params, pStores);

        if (!params->certStores){
                PKIX_CHECK(PKIX_List_Create(&params->certStores, plContext),
                            PKIX_UNABLETOCREATELIST);
        }

        PKIX_INCREF(params->certStores);
        *pStores = params->certStores;

cleanup:

        PKIX_RETURN(PROCESSINGPARAMS);
}

/*
 * FUNCTION: PKIX_ProcessingParams_SetCertStores
 * (see comments in pkix_params.h)
 */
PKIX_Error *
PKIX_ProcessingParams_SetCertStores(
        PKIX_ProcessingParams *params,
        PKIX_List *stores,  /* list of PKIX_CertStore */
        void *plContext)
{
        PKIX_ENTER(PROCESSINGPARAMS, "PKIX_ProcessingParams_SetCertStores");

        PKIX_NULLCHECK_ONE(params);

        PKIX_DECREF(params->certStores);

        PKIX_INCREF(stores);
        params->certStores = stores;

        PKIX_CHECK(PKIX_PL_Object_InvalidateCache
                ((PKIX_PL_Object *)params, plContext),
                PKIX_OBJECTINVALIDATECACHEFAILED);

cleanup:

        if (PKIX_ERROR_RECEIVED && params) {
            PKIX_DECREF(params->certStores);
        }

        PKIX_RETURN(PROCESSINGPARAMS);
}

/*
 * FUNCTION: PKIX_ProcessingParams_AddCertStore
 * (see comments in pkix_params.h)
 */
PKIX_Error *
PKIX_ProcessingParams_AddCertStore(
        PKIX_ProcessingParams *params,
        PKIX_CertStore *store,
        void *plContext)
{
        PKIX_List *certStores = NULL;

        PKIX_ENTER(PROCESSINGPARAMS, "PKIX_ProcessingParams_AddCertStore");
        PKIX_NULLCHECK_TWO(params, store);

        PKIX_CHECK(PKIX_ProcessingParams_GetCertStores
                    (params, &certStores, plContext),
                    PKIX_PROCESSINGPARAMSGETCERTSTORESFAILED);

        PKIX_CHECK(PKIX_List_AppendItem
                    (certStores, (PKIX_PL_Object *)store, plContext),
                    PKIX_LISTAPPENDITEMFAILED);

        PKIX_CHECK(PKIX_PL_Object_InvalidateCache
                    ((PKIX_PL_Object *)params, plContext),
                    PKIX_OBJECTINVALIDATECACHEFAILED);

cleanup:

        PKIX_DECREF(certStores);
        PKIX_RETURN(PROCESSINGPARAMS);
}

/*
 * FUNCTION: PKIX_ProcessingParams_SetResourceLimits
 * (see comments in pkix_params.h)
 */
PKIX_Error *
PKIX_ProcessingParams_SetResourceLimits(
        PKIX_ProcessingParams *params,
        PKIX_ResourceLimits *resourceLimits,
        void *plContext)
{
        PKIX_ENTER(PROCESSINGPARAMS,
                   "PKIX_ProcessingParams_SetResourceLimits");

        PKIX_NULLCHECK_TWO(params, resourceLimits);

        PKIX_DECREF(params->resourceLimits);
        PKIX_INCREF(resourceLimits);
        params->resourceLimits = resourceLimits;

cleanup:
        if (PKIX_ERROR_RECEIVED && params) {
            PKIX_DECREF(params->resourceLimits);
        }

        PKIX_RETURN(PROCESSINGPARAMS);
}

/*
 * FUNCTION: PKIX_ProcessingParams_GetResourceLimits
 * (see comments in pkix_params.h)
 */
PKIX_Error *
PKIX_ProcessingParams_GetResourceLimits(
        PKIX_ProcessingParams *params,
        PKIX_ResourceLimits **pResourceLimits,
        void *plContext)
{
        PKIX_ENTER(PROCESSINGPARAMS,
                   "PKIX_ProcessingParams_GetResourceLimits");

        PKIX_NULLCHECK_TWO(params, pResourceLimits);

        PKIX_INCREF(params->resourceLimits);
        *pResourceLimits = params->resourceLimits;

cleanup:
        PKIX_RETURN(PROCESSINGPARAMS);
}

/*
 * FUNCTION: PKIX_ProcessingParams_IsAnyPolicyInhibited
 * (see comments in pkix_params.h)
 */
PKIX_Error *
PKIX_ProcessingParams_IsAnyPolicyInhibited(
        PKIX_ProcessingParams *params,
        PKIX_Boolean *pInhibited,
        void *plContext)
{
        PKIX_ENTER(PROCESSINGPARAMS,
                "PKIX_ProcessingParams_IsAnyPolicyInhibited");

        PKIX_NULLCHECK_TWO(params, pInhibited);

        *pInhibited = params->initialAnyPolicyInhibit;

        PKIX_RETURN(PROCESSINGPARAMS);
}

/*
 * FUNCTION: PKIX_ProcessingParams_SetAnyPolicyInhibited
 * (see comments in pkix_params.h)
 */
PKIX_Error *
PKIX_ProcessingParams_SetAnyPolicyInhibited(
        PKIX_ProcessingParams *params,
        PKIX_Boolean inhibited,
        void *plContext)
{
        PKIX_ENTER(PROCESSINGPARAMS,
                "PKIX_ProcessingParams_SetAnyPolicyInhibited");

        PKIX_NULLCHECK_ONE(params);

        params->initialAnyPolicyInhibit = inhibited;

        PKIX_CHECK(PKIX_PL_Object_InvalidateCache
                    ((PKIX_PL_Object *)params, plContext),
                    PKIX_OBJECTINVALIDATECACHEFAILED);

cleanup:

        PKIX_RETURN(PROCESSINGPARAMS);
}

/*
 * FUNCTION: PKIX_ProcessingParams_IsExplicitPolicyRequired
 * (see comments in pkix_params.h)
 */
PKIX_Error *
PKIX_ProcessingParams_IsExplicitPolicyRequired(
        PKIX_ProcessingParams *params,
        PKIX_Boolean *pRequired,
        void *plContext)
{
        PKIX_ENTER(PROCESSINGPARAMS,
                "PKIX_ProcessingParams_IsExplicitPolicyRequired");

        PKIX_NULLCHECK_TWO(params, pRequired);

        *pRequired = params->initialExplicitPolicy;

        PKIX_RETURN(PROCESSINGPARAMS);
}

/*
 * FUNCTION: PKIX_ProcessingParams_SetExplicitPolicyRequired
 * (see comments in pkix_params.h)
 */
PKIX_Error *
PKIX_ProcessingParams_SetExplicitPolicyRequired(
        PKIX_ProcessingParams *params,
        PKIX_Boolean required,
        void *plContext)
{
        PKIX_ENTER(PROCESSINGPARAMS,
                "PKIX_ProcessingParams_SetExplicitPolicyRequired");

        PKIX_NULLCHECK_ONE(params);

        params->initialExplicitPolicy = required;

        PKIX_CHECK(PKIX_PL_Object_InvalidateCache
                    ((PKIX_PL_Object *)params, plContext),
                    PKIX_OBJECTINVALIDATECACHEFAILED);

cleanup:

        PKIX_RETURN(PROCESSINGPARAMS);
}

/*
 * FUNCTION: PKIX_ProcessingParams_IsPolicyMappingInhibited
 * (see comments in pkix_params.h)
 */
PKIX_Error *
PKIX_ProcessingParams_IsPolicyMappingInhibited(
        PKIX_ProcessingParams *params,
        PKIX_Boolean *pInhibited,
        void *plContext)
{
        PKIX_ENTER(PROCESSINGPARAMS,
                "PKIX_ProcessingParams_IsPolicyMappingInhibited");

        PKIX_NULLCHECK_TWO(params, pInhibited);

        *pInhibited = params->initialPolicyMappingInhibit;

        PKIX_RETURN(PROCESSINGPARAMS);
}

/*
 * FUNCTION: PKIX_ProcessingParams_SetPolicyMappingInhibited
 * (see comments in pkix_params.h)
 */
PKIX_Error *
PKIX_ProcessingParams_SetPolicyMappingInhibited(
        PKIX_ProcessingParams *params,
        PKIX_Boolean inhibited,
        void *plContext)
{
        PKIX_ENTER(PROCESSINGPARAMS,
                "PKIX_ProcessingParams_SetPolicyMappingInhibited");

        PKIX_NULLCHECK_ONE(params);

        params->initialPolicyMappingInhibit = inhibited;

        PKIX_CHECK(PKIX_PL_Object_InvalidateCache
                    ((PKIX_PL_Object *)params, plContext),
                    PKIX_OBJECTINVALIDATECACHEFAILED);

cleanup:

        PKIX_RETURN(PROCESSINGPARAMS);
}

/*
 * FUNCTION: PKIX_ProcessingParams_SetHintCerts
 * (see comments in pkix_params.h)
 */
PKIX_Error *
PKIX_ProcessingParams_SetHintCerts(
        PKIX_ProcessingParams *params,
        PKIX_List *hintCerts,
        void *plContext)
{
        PKIX_ENTER(PROCESSINGPARAMS, "PKIX_ProcessingParams_SetHintCerts");

        PKIX_NULLCHECK_ONE(params);

        PKIX_DECREF(params->hintCerts);
        PKIX_INCREF(hintCerts);
        params->hintCerts = hintCerts;

cleanup:
        if (PKIX_ERROR_RECEIVED && params) {
            PKIX_DECREF(params->hintCerts);
        }

        PKIX_RETURN(PROCESSINGPARAMS);
}

/*
 * FUNCTION: PKIX_ProcessingParams_GetHintCerts
 * (see comments in pkix_params.h)
 */
PKIX_Error *
PKIX_ProcessingParams_GetHintCerts(
        PKIX_ProcessingParams *params,
        PKIX_List **pHintCerts,
        void *plContext)
{
        PKIX_ENTER(PROCESSINGPARAMS, "PKIX_ProcessingParams_GetHintCerts");

        PKIX_NULLCHECK_TWO(params, pHintCerts);

        PKIX_INCREF(params->hintCerts);
        *pHintCerts = params->hintCerts;

cleanup:
        PKIX_RETURN(PROCESSINGPARAMS);
}
