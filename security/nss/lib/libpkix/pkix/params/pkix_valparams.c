/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * pkix_valparams.c
 *
 * Validate Params Object Functions
 *
 */

#include "pkix_valparams.h"

/* --Private-Functions-------------------------------------------- */

/*
 * FUNCTION: pkix_ValidateParams_Destroy
 * (see comments for PKIX_PL_DestructorCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_ValidateParams_Destroy(
        PKIX_PL_Object *object,
        void *plContext)
{
        PKIX_ValidateParams *params = NULL;

        PKIX_ENTER(VALIDATEPARAMS, "pkix_ValidateParams_Destroy");
        PKIX_NULLCHECK_ONE(object);

        /* Check that this object is a validate params object */
        PKIX_CHECK(pkix_CheckType(object, PKIX_VALIDATEPARAMS_TYPE, plContext),
                    PKIX_OBJECTNOTVALIDATEPARAMS);

        params = (PKIX_ValidateParams *)object;

        PKIX_DECREF(params->procParams);
        PKIX_DECREF(params->chain);

cleanup:

        PKIX_RETURN(VALIDATEPARAMS);
}

/*
 * FUNCTION: pkix_ValidateParams_Equals
 * (see comments for PKIX_PL_EqualsCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_ValidateParams_Equals(
        PKIX_PL_Object *first,
        PKIX_PL_Object *second,
        PKIX_Boolean *pResult,
        void *plContext)
{
        PKIX_UInt32 secondType;
        PKIX_Boolean cmpResult;
        PKIX_ValidateParams *firstValParams = NULL;
        PKIX_ValidateParams *secondValParams = NULL;

        PKIX_ENTER(VALIDATEPARAMS, "pkix_ValidateParams_Equals");
        PKIX_NULLCHECK_THREE(first, second, pResult);

        PKIX_CHECK(pkix_CheckType(first, PKIX_VALIDATEPARAMS_TYPE, plContext),
                    PKIX_FIRSTOBJECTNOTVALIDATEPARAMS);

        PKIX_CHECK(PKIX_PL_Object_GetType(second, &secondType, plContext),
                    PKIX_COULDNOTGETTYPEOFSECONDARGUMENT);

        *pResult = PKIX_FALSE;

        if (secondType != PKIX_VALIDATEPARAMS_TYPE) goto cleanup;

        firstValParams = (PKIX_ValidateParams *)first;
        secondValParams = (PKIX_ValidateParams *)second;

        PKIX_CHECK(PKIX_PL_Object_Equals
                    ((PKIX_PL_Object *)firstValParams->procParams,
                    (PKIX_PL_Object *)secondValParams->procParams,
                    &cmpResult,
                    plContext),
                    PKIX_OBJECTEQUALSFAILED);

        if (!cmpResult) goto cleanup;

        PKIX_CHECK(PKIX_PL_Object_Equals
                    ((PKIX_PL_Object *)firstValParams->chain,
                    (PKIX_PL_Object *)secondValParams->chain,
                    &cmpResult,
                    plContext),
                    PKIX_OBJECTEQUALSFAILED);

        if (!cmpResult) goto cleanup;

        *pResult = cmpResult;

cleanup:

        PKIX_RETURN(VALIDATEPARAMS);
}

/*
 * FUNCTION: pkix_ValidateParams_Hashcode
 * (see comments for PKIX_PL_HashcodeCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_ValidateParams_Hashcode(
        PKIX_PL_Object *object,
        PKIX_UInt32 *pHashcode,
        void *plContext)
{
        PKIX_ValidateParams *valParams = NULL;
        PKIX_UInt32 hash = 0;
        PKIX_UInt32 procParamsHash = 0;
        PKIX_UInt32 chainHash = 0;

        PKIX_ENTER(VALIDATEPARAMS, "pkix_ValidateParams_Hashcode");
        PKIX_NULLCHECK_TWO(object, pHashcode);

        PKIX_CHECK(pkix_CheckType(object, PKIX_VALIDATEPARAMS_TYPE, plContext),
                    PKIX_OBJECTNOTVALIDATEPARAMS);

        valParams = (PKIX_ValidateParams*)object;

        PKIX_CHECK(PKIX_PL_Object_Hashcode
                    ((PKIX_PL_Object *)valParams->procParams,
                    &procParamsHash,
                    plContext),
                    PKIX_OBJECTHASHCODEFAILED);

        PKIX_CHECK(PKIX_PL_Object_Hashcode
                    ((PKIX_PL_Object *)valParams->chain,
                    &chainHash,
                    plContext),
                    PKIX_OBJECTHASHCODEFAILED);

        hash = 31 * procParamsHash + chainHash;

        *pHashcode = hash;

cleanup:

        PKIX_RETURN(VALIDATEPARAMS);
}

/*
 * FUNCTION: pkix_ValidateParams_ToString
 * (see comments for PKIX_PL_ToStringCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_ValidateParams_ToString(
        PKIX_PL_Object *object,
        PKIX_PL_String **pString,
        void *plContext)
{
        PKIX_ValidateParams *valParams = NULL;
        char *asciiFormat = NULL;
        PKIX_PL_String *formatString = NULL;
        PKIX_PL_String *valParamsString = NULL;

        PKIX_PL_String *procParamsString = NULL;
        PKIX_PL_String *chainString = NULL;

        PKIX_ENTER(VALIDATEPARAMS, "pkix_ValidateParams_ToString");
        PKIX_NULLCHECK_TWO(object, pString);

        PKIX_CHECK(pkix_CheckType(object, PKIX_VALIDATEPARAMS_TYPE, plContext),
                    PKIX_OBJECTNOTVALIDATEPARAMS);

        asciiFormat =
                "[\n"
                "\tProcessing Params: \n"
                "\t********BEGIN PROCESSING PARAMS********\n"
                "\t\t%s\n"
                "\t********END PROCESSING PARAMS********\n"
                "\tChain:    \t\t%s\n"
                "]\n";

        PKIX_CHECK(PKIX_PL_String_Create
                    (PKIX_ESCASCII,
                    asciiFormat,
                    0,
                    &formatString,
                    plContext),
                    PKIX_STRINGCREATEFAILED);

        valParams = (PKIX_ValidateParams*)object;

        PKIX_CHECK(PKIX_PL_Object_ToString
                    ((PKIX_PL_Object*)valParams->procParams,
                    &procParamsString,
                    plContext),
                    PKIX_OBJECTTOSTRINGFAILED);

        PKIX_CHECK(PKIX_PL_Object_ToString
                    ((PKIX_PL_Object *)valParams->chain,
                    &chainString,
                    plContext),
                    PKIX_OBJECTTOSTRINGFAILED);

        PKIX_CHECK(PKIX_PL_Sprintf
                    (&valParamsString,
                    plContext,
                    formatString,
                    procParamsString,
                    chainString),
                    PKIX_SPRINTFFAILED);

        *pString = valParamsString;

cleanup:

        PKIX_DECREF(formatString);
        PKIX_DECREF(procParamsString);
        PKIX_DECREF(chainString);

        PKIX_RETURN(VALIDATEPARAMS);
}

/*
 * FUNCTION: pkix_ValidateParams_RegisterSelf
 * DESCRIPTION:
 *  Registers PKIX_VALIDATEPARAMS_TYPE and its related functions with
 *  systemClasses[]
 * THREAD SAFETY:
 *  Not Thread Safe - for performance and complexity reasons
 *
 *  Since this function is only called by PKIX_PL_Initialize, which should
 *  only be called once, it is acceptable that this function is not
 *  thread-safe.
 */
PKIX_Error *
pkix_ValidateParams_RegisterSelf(void *plContext)
{

        extern pkix_ClassTable_Entry systemClasses[PKIX_NUMTYPES];
        pkix_ClassTable_Entry entry;

        PKIX_ENTER(VALIDATEPARAMS, "pkix_ValidateParams_RegisterSelf");

        entry.description = "ValidateParams";
        entry.objCounter = 0;
        entry.typeObjectSize = sizeof(PKIX_ValidateParams);
        entry.destructor = pkix_ValidateParams_Destroy;
        entry.equalsFunction = pkix_ValidateParams_Equals;
        entry.hashcodeFunction = pkix_ValidateParams_Hashcode;
        entry.toStringFunction = pkix_ValidateParams_ToString;
        entry.comparator = NULL;
        entry.duplicateFunction = NULL;

        systemClasses[PKIX_VALIDATEPARAMS_TYPE] = entry;

        PKIX_RETURN(VALIDATEPARAMS);
}

/* --Public-Functions--------------------------------------------- */

/*
 * FUNCTION: PKIX_ValidateParams_Create (see comments in pkix_params.h)
 */
PKIX_Error *
PKIX_ValidateParams_Create(
        PKIX_ProcessingParams *procParams,
        PKIX_List *chain,
        PKIX_ValidateParams **pParams,
        void *plContext)
{
        PKIX_ValidateParams *params = NULL;

        PKIX_ENTER(VALIDATEPARAMS, "PKIX_ValidateParams_Create");
        PKIX_NULLCHECK_THREE(procParams, chain, pParams);

        PKIX_CHECK(PKIX_PL_Object_Alloc
                    (PKIX_VALIDATEPARAMS_TYPE,
                    sizeof (PKIX_ValidateParams),
                    (PKIX_PL_Object **)&params,
                    plContext),
                    PKIX_COULDNOTCREATEVALIDATEPARAMSOBJECT);

        /* initialize fields */
        PKIX_INCREF(procParams);
        params->procParams = procParams;

        PKIX_INCREF(chain);
        params->chain = chain;

        *pParams = params;
        params = NULL;

cleanup:

        PKIX_DECREF(params);

        PKIX_RETURN(VALIDATEPARAMS);

}

/*
 * FUNCTION: PKIX_ValidateParams_GetProcessingParams
 *      (see comments in pkix_params.h)
 */
PKIX_Error *
PKIX_ValidateParams_GetProcessingParams(
        PKIX_ValidateParams *valParams,
        PKIX_ProcessingParams **pProcParams,
        void *plContext)
{
        PKIX_ENTER(VALIDATEPARAMS, "PKIX_ValidateParams_GetProcessingParams");
        PKIX_NULLCHECK_TWO(valParams, pProcParams);

        PKIX_INCREF(valParams->procParams);

        *pProcParams = valParams->procParams;

cleanup:
        PKIX_RETURN(VALIDATEPARAMS);
}

/*
 * FUNCTION: PKIX_ValidateParams_GetCertChain (see comments in pkix_params.h)
 */
PKIX_Error *
PKIX_ValidateParams_GetCertChain(
        PKIX_ValidateParams *valParams,
        PKIX_List **pChain,
        void *plContext)
{
        PKIX_ENTER(VALIDATEPARAMS, "PKIX_ValidateParams_GetCertChain");
        PKIX_NULLCHECK_TWO(valParams, pChain);

        PKIX_INCREF(valParams->chain);

        *pChain = valParams->chain;

cleanup:
        PKIX_RETURN(VALIDATEPARAMS);
}
