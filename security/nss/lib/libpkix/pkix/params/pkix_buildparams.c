/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * pkix_buildparams.c
 *
 * Build Params Object Functions
 *
 */

#include "pkix_buildparams.h"

/* --Private-Functions-------------------------------------------- */

/*
 * FUNCTION: pkix_BuildParams_Destroy
 * (see comments for PKIX_PL_DestructorCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_BuildParams_Destroy(
        PKIX_PL_Object *object,
        void *plContext)
{
        PKIX_BuildParams *params = NULL;

        PKIX_ENTER(BUILDPARAMS, "pkix_BuildParams_Destroy");
        PKIX_NULLCHECK_ONE(object);

        /* Check that this object is a build params object */
        PKIX_CHECK(pkix_CheckType(object, PKIX_BUILDPARAMS_TYPE, plContext),
                    "Object is not a build params object");

        params = (PKIX_BuildParams *)object;

        PKIX_DECREF(params->procParams);

cleanup:

        PKIX_RETURN(BUILDPARAMS);
}

/*
 * FUNCTION: pkix_BuildParams_Equals
 * (see comments for PKIX_PL_EqualsCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_BuildParams_Equals(
        PKIX_PL_Object *first,
        PKIX_PL_Object *second,
        PKIX_Boolean *pResult,
        void *plContext)
{
        PKIX_UInt32 secondType;
        PKIX_Boolean cmpResult;
        PKIX_BuildParams *firstBuildParams = NULL;
        PKIX_BuildParams *secondBuildParams = NULL;

        PKIX_ENTER(BUILDPARAMS, "pkix_BuildParams_Equals");
        PKIX_NULLCHECK_THREE(first, second, pResult);

        PKIX_CHECK(pkix_CheckType(first, PKIX_BUILDPARAMS_TYPE, plContext),
                    "First Argument is not a BuildParams object");

        PKIX_CHECK(PKIX_PL_Object_GetType(second, &secondType, plContext),
                    PKIX_COULDNOTGETTYPEOFSECONDARGUMENT);

        *pResult = PKIX_FALSE;

        if (secondType != PKIX_BUILDPARAMS_TYPE) goto cleanup;

        firstBuildParams = (PKIX_BuildParams *)first;
        secondBuildParams = (PKIX_BuildParams *)second;

        PKIX_CHECK(PKIX_PL_Object_Equals
                    ((PKIX_PL_Object *)firstBuildParams->procParams,
                    (PKIX_PL_Object *)secondBuildParams->procParams,
                    &cmpResult,
                    plContext),
                    PKIX_OBJECTEQUALSFAILED);

        if (!cmpResult) goto cleanup;

        *pResult = cmpResult;

cleanup:

        PKIX_RETURN(BUILDPARAMS);
}

/*
 * FUNCTION: pkix_BuildParams_Hashcode
 * (see comments for PKIX_PL_HashcodeCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_BuildParams_Hashcode(
        PKIX_PL_Object *object,
        PKIX_UInt32 *pHashcode,
        void *plContext)
{
        PKIX_BuildParams *buildParams = NULL;
        PKIX_UInt32 hash = 0;
        PKIX_UInt32 procParamsHash = 0;

        PKIX_ENTER(BUILDPARAMS, "pkix_BuildParams_Hashcode");
        PKIX_NULLCHECK_TWO(object, pHashcode);

        PKIX_CHECK(pkix_CheckType(object, PKIX_BUILDPARAMS_TYPE, plContext),
                    "Object is not a processingParams object");

        buildParams = (PKIX_BuildParams*)object;

        PKIX_CHECK(PKIX_PL_Object_Hashcode
                    ((PKIX_PL_Object *)buildParams->procParams,
                    &procParamsHash,
                    plContext),
                    PKIX_OBJECTHASHCODEFAILED);

        hash = 31 * procParamsHash;

        *pHashcode = hash;

cleanup:

        PKIX_RETURN(BUILDPARAMS);
}

/*
 * FUNCTION: pkix_BuildParams_ToString
 * (see comments for PKIX_PL_ToStringCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_BuildParams_ToString(
        PKIX_PL_Object *object,
        PKIX_PL_String **pString,
        void *plContext)
{
        PKIX_BuildParams *buildParams = NULL;
        char *asciiFormat = NULL;
        PKIX_PL_String *formatString = NULL;
        PKIX_PL_String *buildParamsString = NULL;

        PKIX_PL_String *procParamsString = NULL;

        PKIX_ENTER(BUILDPARAMS, "pkix_BuildParams_ToString");
        PKIX_NULLCHECK_TWO(object, pString);

        PKIX_CHECK(pkix_CheckType(object, PKIX_BUILDPARAMS_TYPE, plContext),
                    PKIX_OBJECTNOTBUILDPARAMS);

        asciiFormat =
                "[\n"
                "\tProcessing Params: \n"
                "\t********BEGIN PROCESSING PARAMS********\n"
                "\t\t%s\n"
                "\t********END PROCESSING PARAMS********\n"
                "]\n";

        PKIX_CHECK(PKIX_PL_String_Create
                    (PKIX_ESCASCII,
                    asciiFormat,
                    0,
                    &formatString,
                    plContext),
                    PKIX_STRINGCREATEFAILED);

        buildParams = (PKIX_BuildParams*)object;

        PKIX_CHECK(PKIX_PL_Object_ToString
                    ((PKIX_PL_Object*)buildParams->procParams,
                    &procParamsString,
                    plContext),
                    PKIX_OBJECTTOSTRINGFAILED);

        PKIX_CHECK(PKIX_PL_Sprintf
                    (&buildParamsString,
                    plContext,
                    formatString,
                    procParamsString),
                    PKIX_SPRINTFFAILED);

        *pString = buildParamsString;

cleanup:

        PKIX_DECREF(formatString);
        PKIX_DECREF(procParamsString);

        PKIX_RETURN(BUILDPARAMS);
}

/*
 * FUNCTION: pkix_BuildParams_RegisterSelf
 * DESCRIPTION:
 *  Registers PKIX_BUILDPARAMS_TYPE and its related functions with
 *  systemClasses[]
 * THREAD SAFETY:
 *  Not Thread Safe - for performance and complexity reasons
 *
 *  Since this function is only called by PKIX_PL_Initialize, which should
 *  only be called once, it is acceptable that this function is not
 *  thread-safe.
 */
PKIX_Error *
pkix_BuildParams_RegisterSelf(void *plContext)
{

        extern pkix_ClassTable_Entry systemClasses[PKIX_NUMTYPES];
        pkix_ClassTable_Entry entry;

        PKIX_ENTER(BUILDPARAMS, "pkix_BuildParams_RegisterSelf");

        entry.description = "BuildParams";
        entry.objCounter = 0;
        entry.typeObjectSize = sizeof(PKIX_BuildParams);
        entry.destructor = pkix_BuildParams_Destroy;
        entry.equalsFunction = pkix_BuildParams_Equals;
        entry.hashcodeFunction = pkix_BuildParams_Hashcode;
        entry.toStringFunction = pkix_BuildParams_ToString;
        entry.comparator = NULL;
        entry.duplicateFunction = NULL;

        systemClasses[PKIX_BUILDPARAMS_TYPE] = entry;

        PKIX_RETURN(BUILDPARAMS);
}

/* --Public-Functions--------------------------------------------- */

/*
 * FUNCTION: PKIX_BuildParams_Create (see comments in pkix_params.h)
 */
PKIX_Error *
PKIX_BuildParams_Create(
        PKIX_ProcessingParams *procParams,
        PKIX_BuildParams **pParams,
        void *plContext)
{
        PKIX_BuildParams *params = NULL;

        PKIX_ENTER(BUILDPARAMS, "PKIX_BuildParams_Create");
        PKIX_NULLCHECK_TWO(procParams, pParams);

        PKIX_CHECK(PKIX_PL_Object_Alloc
                    (PKIX_BUILDPARAMS_TYPE,
                    sizeof (PKIX_BuildParams),
                    (PKIX_PL_Object **)&params,
                    plContext),
                    PKIX_COULDNOTCREATEBUILDPARAMSOBJECT);

        /* initialize fields */
        PKIX_INCREF(procParams);
        params->procParams = procParams;

        *pParams = params;
        params = NULL;

cleanup:

        PKIX_DECREF(params);

        PKIX_RETURN(BUILDPARAMS);

}

/*
 * FUNCTION: PKIX_BuildParams_GetProcessingParams
 *      (see comments in pkix_params.h)
 */
PKIX_Error *
PKIX_BuildParams_GetProcessingParams(
        PKIX_BuildParams *buildParams,
        PKIX_ProcessingParams **pProcParams,
        void *plContext)
{
        PKIX_ENTER(BUILDPARAMS, "PKIX_BuildParams_GetProcessingParams");
        PKIX_NULLCHECK_TWO(buildParams, pProcParams);

        PKIX_INCREF(buildParams->procParams);

        *pProcParams = buildParams->procParams;

cleanup:
        PKIX_RETURN(BUILDPARAMS);
}
