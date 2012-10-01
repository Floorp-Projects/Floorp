/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * pkix_buildresult.c
 *
 * BuildResult Object Functions
 *
 */

#include "pkix_buildresult.h"

/* --Private-Functions-------------------------------------------- */

/*
 * FUNCTION: pkix_BuildResult_Destroy
 * (see comments for PKIX_PL_DestructorCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_BuildResult_Destroy(
        PKIX_PL_Object *object,
        void *plContext)
{
        PKIX_BuildResult *result = NULL;

        PKIX_ENTER(BUILDRESULT, "pkix_BuildResult_Destroy");
        PKIX_NULLCHECK_ONE(object);

        /* Check that this object is a build result object */
        PKIX_CHECK(pkix_CheckType(object, PKIX_BUILDRESULT_TYPE, plContext),
                    PKIX_OBJECTNOTBUILDRESULT);

        result = (PKIX_BuildResult *)object;

        PKIX_DECREF(result->valResult);
        PKIX_DECREF(result->certChain);

cleanup:

        PKIX_RETURN(BUILDRESULT);
}

/*
 * FUNCTION: pkix_BuildResult_Equals
 * (see comments for PKIX_PL_EqualsCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_BuildResult_Equals(
        PKIX_PL_Object *first,
        PKIX_PL_Object *second,
        PKIX_Boolean *pResult,
        void *plContext)
{
        PKIX_UInt32 secondType;
        PKIX_Boolean cmpResult;
        PKIX_BuildResult *firstBuildResult = NULL;
        PKIX_BuildResult *secondBuildResult = NULL;

        PKIX_ENTER(BUILDRESULT, "pkix_BuildResult_Equals");
        PKIX_NULLCHECK_THREE(first, second, pResult);

        PKIX_CHECK(pkix_CheckType(first, PKIX_BUILDRESULT_TYPE, plContext),
                    PKIX_FIRSTOBJECTNOTBUILDRESULT);

        PKIX_CHECK(PKIX_PL_Object_GetType(second, &secondType, plContext),
                    PKIX_COULDNOTGETTYPEOFSECONDARGUMENT);

        *pResult = PKIX_FALSE;

        if (secondType != PKIX_BUILDRESULT_TYPE) goto cleanup;

        firstBuildResult = (PKIX_BuildResult *)first;
        secondBuildResult = (PKIX_BuildResult *)second;

        PKIX_CHECK(PKIX_PL_Object_Equals
                    ((PKIX_PL_Object *)firstBuildResult->valResult,
                    (PKIX_PL_Object *)secondBuildResult->valResult,
                    &cmpResult,
                    plContext),
                    PKIX_OBJECTEQUALSFAILED);

        if (!cmpResult) goto cleanup;

        PKIX_CHECK(PKIX_PL_Object_Equals
                    ((PKIX_PL_Object *)firstBuildResult->certChain,
                    (PKIX_PL_Object *)secondBuildResult->certChain,
                    &cmpResult,
                    plContext),
                    PKIX_OBJECTEQUALSFAILED);

        if (!cmpResult) goto cleanup;

        /*
         * The remaining case is that both are null,
         * which we consider equality.
         *      cmpResult = PKIX_TRUE;
         */

        *pResult = cmpResult;

cleanup:

        PKIX_RETURN(BUILDRESULT);
}

/*
 * FUNCTION: pkix_BuildResult_Hashcode
 * (see comments for PKIX_PL_HashcodeCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_BuildResult_Hashcode(
        PKIX_PL_Object *object,
        PKIX_UInt32 *pHashcode,
        void *plContext)
{
        PKIX_BuildResult *buildResult = NULL;
        PKIX_UInt32 hash = 0;
        PKIX_UInt32 valResultHash = 0;
        PKIX_UInt32 certChainHash = 0;

        PKIX_ENTER(BUILDRESULT, "pkix_BuildResult_Hashcode");
        PKIX_NULLCHECK_TWO(object, pHashcode);

        PKIX_CHECK(pkix_CheckType(object, PKIX_BUILDRESULT_TYPE, plContext),
                    PKIX_OBJECTNOTBUILDRESULT);

        buildResult = (PKIX_BuildResult*)object;

        PKIX_CHECK(PKIX_PL_Object_Hashcode
                    ((PKIX_PL_Object *)buildResult->valResult,
                    &valResultHash,
                    plContext),
                    PKIX_OBJECTHASHCODEFAILED);

        PKIX_CHECK(PKIX_PL_Object_Hashcode
                    ((PKIX_PL_Object *)buildResult->certChain,
                    &certChainHash,
                    plContext),
                    PKIX_OBJECTHASHCODEFAILED);

        hash = 31*(31 * valResultHash + certChainHash);

        *pHashcode = hash;

cleanup:

        PKIX_RETURN(BUILDRESULT);
}

/*
 * FUNCTION: pkix_BuildResult_ToString
 * (see comments for PKIX_PL_ToStringCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_BuildResult_ToString(
        PKIX_PL_Object *object,
        PKIX_PL_String **pString,
        void *plContext)
{
        PKIX_BuildResult *buildResult = NULL;
        PKIX_PL_String *formatString = NULL;
        PKIX_PL_String *buildResultString = NULL;

        PKIX_ValidateResult *valResult = NULL;
        PKIX_List *certChain = NULL;

        PKIX_PL_String *valResultString = NULL;
        PKIX_PL_String *certChainString = NULL;

        char *asciiFormat =
                "[\n"
                "\tValidateResult: \t\t%s"
                "\tCertChain:    \t\t%s\n"
                "]\n";

        PKIX_ENTER(BUILDRESULT, "pkix_BuildResult_ToString");
        PKIX_NULLCHECK_TWO(object, pString);

        PKIX_CHECK(pkix_CheckType(object, PKIX_BUILDRESULT_TYPE, plContext),
                PKIX_OBJECTNOTBUILDRESULT);

        buildResult = (PKIX_BuildResult*)object;

        valResult = buildResult->valResult;

        PKIX_CHECK(PKIX_PL_String_Create
                (PKIX_ESCASCII, asciiFormat, 0, &formatString, plContext),
                PKIX_STRINGCREATEFAILED);

        PKIX_CHECK(PKIX_PL_Object_ToString
                ((PKIX_PL_Object *)valResult, &valResultString, plContext),
                PKIX_OBJECTTOSTRINGFAILED);

        certChain = buildResult->certChain;

        PKIX_CHECK(PKIX_PL_Object_ToString
                ((PKIX_PL_Object *)certChain, &certChainString, plContext),
                PKIX_OBJECTTOSTRINGFAILED);

        PKIX_CHECK(PKIX_PL_Sprintf
                (&buildResultString,
                plContext,
                formatString,
                valResultString,
                certChainString),
                PKIX_SPRINTFFAILED);

        *pString = buildResultString;

cleanup:

        PKIX_DECREF(formatString);
        PKIX_DECREF(valResultString);
        PKIX_DECREF(certChainString);

        PKIX_RETURN(BUILDRESULT);
}

/*
 * FUNCTION: pkix_BuildResult_RegisterSelf
 * DESCRIPTION:
 *  Registers PKIX_BUILDRESULT_TYPE and its related functions with
 *  systemClasses[]
 * THREAD SAFETY:
 *  Not Thread Safe - for performance and complexity reasons
 *
 *  Since this function is only called by PKIX_PL_Initialize, which should
 *  only be called once, it is acceptable that this function is not
 *  thread-safe.
 */
PKIX_Error *
pkix_BuildResult_RegisterSelf(void *plContext)
{

        extern pkix_ClassTable_Entry systemClasses[PKIX_NUMTYPES];
        pkix_ClassTable_Entry entry;

        PKIX_ENTER(BUILDRESULT, "pkix_BuildResult_RegisterSelf");

        entry.description = "BuildResult";
        entry.objCounter = 0;
        entry.typeObjectSize = sizeof(PKIX_BuildResult);
        entry.destructor = pkix_BuildResult_Destroy;
        entry.equalsFunction = pkix_BuildResult_Equals;
        entry.hashcodeFunction = pkix_BuildResult_Hashcode;
        entry.toStringFunction = pkix_BuildResult_ToString;
        entry.comparator = NULL;
        entry.duplicateFunction = pkix_duplicateImmutable;

        systemClasses[PKIX_BUILDRESULT_TYPE] = entry;

        PKIX_RETURN(BUILDRESULT);
}

/*
 * FUNCTION: pkix_BuildResult_Create
 * DESCRIPTION:
 *
 *  Creates a new BuildResult Object using the ValidateResult pointed to by
 *  "valResult" and the List pointed to by "certChain", and stores it at
 *  "pResult".
 *
 * PARAMETERS
 *  "valResult"
 *      Address of ValidateResult component. Must be non-NULL.
 *  "certChain
 *      Address of List component. Must be non-NULL.
 *  "pResult"
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
pkix_BuildResult_Create(
        PKIX_ValidateResult *valResult,
        PKIX_List *certChain,
        PKIX_BuildResult **pResult,
        void *plContext)
{
        PKIX_BuildResult *result = NULL;

        PKIX_ENTER(BUILDRESULT, "pkix_BuildResult_Create");
        PKIX_NULLCHECK_THREE(valResult, certChain, pResult);

        PKIX_CHECK(PKIX_PL_Object_Alloc
                    (PKIX_BUILDRESULT_TYPE,
                    sizeof (PKIX_BuildResult),
                    (PKIX_PL_Object **)&result,
                    plContext),
                    PKIX_COULDNOTCREATEBUILDRESULTOBJECT);

        /* initialize fields */

        PKIX_INCREF(valResult);
        result->valResult = valResult;

        PKIX_INCREF(certChain);
        result->certChain = certChain;

        PKIX_CHECK(PKIX_List_SetImmutable(result->certChain, plContext),
                     PKIX_LISTSETIMMUTABLEFAILED);

        *pResult = result;
        result = NULL;

cleanup:

        PKIX_DECREF(result);

        PKIX_RETURN(BUILDRESULT);

}

/* --Public-Functions--------------------------------------------- */


/*
 * FUNCTION: PKIX_BuildResult_GetValidateResult
 *      (see comments in pkix_result.h)
 */
PKIX_Error *
PKIX_BuildResult_GetValidateResult(
        PKIX_BuildResult *result,
        PKIX_ValidateResult **pResult,
        void *plContext)
{
        PKIX_ENTER(BUILDRESULT, "PKIX_BuildResult_GetValidateResult");
        PKIX_NULLCHECK_TWO(result, pResult);

        PKIX_INCREF(result->valResult);
        *pResult = result->valResult;

cleanup:
        PKIX_RETURN(BUILDRESULT);
}



/*
 * FUNCTION: PKIX_BuildResult_GetCertChain
 *      (see comments in pkix_result.h)
 */
PKIX_Error *
PKIX_BuildResult_GetCertChain(
        PKIX_BuildResult *result,
        PKIX_List **pChain,
        void *plContext)
{
        PKIX_ENTER(BUILDRESULT, "PKIX_BuildResult_GetCertChain");
        PKIX_NULLCHECK_TWO(result, pChain);

        PKIX_INCREF(result->certChain);
        *pChain = result->certChain;

cleanup:
        PKIX_RETURN(BUILDRESULT);
}
