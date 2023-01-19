/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * pkix_resourcelimits.c
 *
 * Resourcelimits Params Object Functions
 *
 */

#include "pkix_resourcelimits.h"

/* --Private-Functions-------------------------------------------- */

/*
 * FUNCTION: pkix_ResourceLimits_Destroy
 * (see comments for PKIX_PL_DestructorCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_ResourceLimits_Destroy(
        PKIX_PL_Object *object,
        void *plContext)
{
        PKIX_ResourceLimits *rLimits = NULL;

        PKIX_ENTER(RESOURCELIMITS, "pkix_ResourceLimits_Destroy");
        PKIX_NULLCHECK_ONE(object);

        /* Check that this object is a ResourceLimits object */
        PKIX_CHECK(pkix_CheckType(object, PKIX_RESOURCELIMITS_TYPE, plContext),
                    PKIX_OBJECTNOTRESOURCELIMITS);

        rLimits = (PKIX_ResourceLimits *)object;

        rLimits->maxTime = 0;
        rLimits->maxFanout = 0;
        rLimits->maxDepth = 0;
        rLimits->maxCertsNumber = 0;
        rLimits->maxCrlsNumber = 0;

cleanup:

        PKIX_RETURN(RESOURCELIMITS);
}

/*
 * FUNCTION: pkix_ResourceLimits_Equals
 * (see comments for PKIX_PL_EqualsCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_ResourceLimits_Equals(
        PKIX_PL_Object *first,
        PKIX_PL_Object *second,
        PKIX_Boolean *pResult,
        void *plContext)
{
        PKIX_UInt32 secondType;
        PKIX_Boolean cmpResult;
        PKIX_ResourceLimits *firstRLimits = NULL;
        PKIX_ResourceLimits *secondRLimits = NULL;

        PKIX_ENTER(RESOURCELIMITS, "pkix_ResourceLimits_Equals");
        PKIX_NULLCHECK_THREE(first, second, pResult);

        PKIX_CHECK(pkix_CheckType(first, PKIX_RESOURCELIMITS_TYPE, plContext),
                    PKIX_FIRSTOBJECTNOTRESOURCELIMITS);

        PKIX_CHECK(PKIX_PL_Object_GetType(second, &secondType, plContext),
                    PKIX_COULDNOTGETTYPEOFSECONDARGUMENT);

        *pResult = PKIX_FALSE;

        if (secondType != PKIX_RESOURCELIMITS_TYPE) goto cleanup;

        firstRLimits = (PKIX_ResourceLimits *)first;
        secondRLimits = (PKIX_ResourceLimits *)second;

        cmpResult = (firstRLimits->maxTime == secondRLimits->maxTime) &&
                    (firstRLimits->maxFanout == secondRLimits->maxFanout) &&
                    (firstRLimits->maxDepth == secondRLimits->maxDepth) &&
                    (firstRLimits->maxCertsNumber == 
                        secondRLimits->maxCertsNumber) &&
                    (firstRLimits->maxCrlsNumber == 
                        secondRLimits->maxCrlsNumber);

        *pResult = cmpResult;

cleanup:

        PKIX_RETURN(RESOURCELIMITS);
}

/*
 * FUNCTION: pkix_ResourceLimits_Hashcode
 * (see comments for PKIX_PL_HashcodeCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_ResourceLimits_Hashcode(
        PKIX_PL_Object *object,
        PKIX_UInt32 *pHashcode,
        void *plContext)
{
        PKIX_ResourceLimits *rLimits = NULL;
        PKIX_UInt32 hash = 0;

        PKIX_ENTER(RESOURCELIMITS, "pkix_ResourceLimits_Hashcode");
        PKIX_NULLCHECK_TWO(object, pHashcode);

        PKIX_CHECK(pkix_CheckType(object, PKIX_RESOURCELIMITS_TYPE, plContext),
                    PKIX_OBJECTNOTRESOURCELIMITS);

        rLimits = (PKIX_ResourceLimits*)object;

        hash = 31 * rLimits->maxTime + (rLimits->maxFanout << 1) +
                (rLimits->maxDepth << 2) + (rLimits->maxCertsNumber << 3) +
                rLimits->maxCrlsNumber;

        *pHashcode = hash;

cleanup:

        PKIX_RETURN(RESOURCELIMITS);
}

/*
 * FUNCTION: pkix_ResourceLimits_ToString
 * (see comments for PKIX_PL_ToStringCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_ResourceLimits_ToString(
        PKIX_PL_Object *object,
        PKIX_PL_String **pString,
        void *plContext)
{
        PKIX_ResourceLimits *rLimits = NULL;
        char *asciiFormat = NULL;
        PKIX_PL_String *formatString = NULL;
        PKIX_PL_String *rLimitsString = NULL;

        PKIX_ENTER(RESOURCELIMITS, "pkix_ResourceLimits_ToString");
        PKIX_NULLCHECK_TWO(object, pString);

        PKIX_CHECK(pkix_CheckType(object, PKIX_RESOURCELIMITS_TYPE, plContext),
                    PKIX_OBJECTNOTRESOURCELIMITS);

        /* maxCertsNumber and maxCrlsNumber are not supported */
        asciiFormat =
                "[\n"
                "\tMaxTime:           \t\t%d\n"
                "\tMaxFanout:         \t\t%d\n"
                "\tMaxDepth:         \t\t%d\n"
                "]\n";

        PKIX_CHECK(PKIX_PL_String_Create
                    (PKIX_ESCASCII,
                    asciiFormat,
                    0,
                    &formatString,
                    plContext),
                    PKIX_STRINGCREATEFAILED);

        rLimits = (PKIX_ResourceLimits*)object;

        PKIX_CHECK(PKIX_PL_Sprintf
                    (&rLimitsString,
                    plContext,
                    formatString,
                    rLimits->maxTime,
                    rLimits->maxFanout,
                    rLimits->maxDepth),
                    PKIX_SPRINTFFAILED);

        *pString = rLimitsString;

cleanup:

        PKIX_DECREF(formatString);

        PKIX_RETURN(RESOURCELIMITS);
}

/*
 * FUNCTION: pkix_ResourceLimits_RegisterSelf
 * DESCRIPTION:
 *  Registers PKIX_RESOURCELIMITS_TYPE and its related functions with
 *  systemClasses[]
 * THREAD SAFETY:
 *  Not Thread Safe - for performance and complexity reasons
 *
 *  Since this function is only called by PKIX_PL_Initialize, which should
 *  only be called once, it is acceptable that this function is not
 *  thread-safe.
 */
PKIX_Error *
pkix_ResourceLimits_RegisterSelf(void *plContext)
{

        extern pkix_ClassTable_Entry systemClasses[PKIX_NUMTYPES];
        pkix_ClassTable_Entry entry;

        PKIX_ENTER(RESOURCELIMITS, "pkix_ResourceLimits_RegisterSelf");

        entry.description = "ResourceLimits";
        entry.objCounter = 0;
        entry.typeObjectSize = sizeof(PKIX_ResourceLimits);
        entry.destructor = pkix_ResourceLimits_Destroy;
        entry.equalsFunction = pkix_ResourceLimits_Equals;
        entry.hashcodeFunction = pkix_ResourceLimits_Hashcode;
        entry.toStringFunction = pkix_ResourceLimits_ToString;
        entry.comparator = NULL;
        entry.duplicateFunction = NULL;

        systemClasses[PKIX_RESOURCELIMITS_TYPE] = entry;

        PKIX_RETURN(RESOURCELIMITS);
}

/* --Public-Functions--------------------------------------------- */

/*
 * FUNCTION: PKIX_ResourceLimits_Create (see comments in pkix_params.h)
 */
PKIX_Error *
PKIX_ResourceLimits_Create(
        PKIX_ResourceLimits **pResourceLimits,
        void *plContext)
{
        PKIX_ResourceLimits *rLimits = NULL;

        PKIX_ENTER(RESOURCELIMITS, "PKIX_ResourceLimits_Create");
        PKIX_NULLCHECK_ONE(pResourceLimits);

        PKIX_CHECK(PKIX_PL_Object_Alloc
                    (PKIX_RESOURCELIMITS_TYPE,
                    sizeof (PKIX_ResourceLimits),
                    (PKIX_PL_Object **)&rLimits,
                    plContext),
                    PKIX_COULDNOTCREATERESOURCELIMITOBJECT);

        /* initialize fields */
        rLimits->maxTime = 0;
        rLimits->maxFanout = 0;
        rLimits->maxDepth = 0;
        rLimits->maxCertsNumber = 0;
        rLimits->maxCrlsNumber = 0;

        *pResourceLimits = rLimits;

cleanup:

        PKIX_RETURN(RESOURCELIMITS);

}

/*
 * FUNCTION: PKIX_ResourceLimits_GetMaxTime
 *      (see comments in pkix_params.h)
 */
PKIX_Error *
PKIX_ResourceLimits_GetMaxTime(
        PKIX_ResourceLimits *rLimits,
        PKIX_UInt32 *pMaxTime,
        void *plContext)
{
        PKIX_ENTER(RESOURCELIMITS, "PKIX_ResourceLimits_GetMaxTime");
        PKIX_NULLCHECK_TWO(rLimits, pMaxTime);

        *pMaxTime = rLimits->maxTime;

        PKIX_RETURN(RESOURCELIMITS);
}

/*
 * FUNCTION: PKIX_ResourceLimits_SetMaxTime
 *      (see comments in pkix_params.h)
 */
PKIX_Error *
PKIX_ResourceLimits_SetMaxTime(
        PKIX_ResourceLimits *rLimits,
        PKIX_UInt32 maxTime,
        void *plContext)
{
        PKIX_ENTER(RESOURCELIMITS, "PKIX_ResourceLimits_SetMaxTime");
        PKIX_NULLCHECK_ONE(rLimits);

        rLimits->maxTime = maxTime;

        PKIX_RETURN(RESOURCELIMITS);
}

/*
 * FUNCTION: PKIX_ResourceLimits_GetMaxFanout
 *      (see comments in pkix_params.h)
 */
PKIX_Error *
PKIX_ResourceLimits_GetMaxFanout(
        PKIX_ResourceLimits *rLimits,
        PKIX_UInt32 *pMaxFanout,
        void *plContext)
{
        PKIX_ENTER(RESOURCELIMITS, "PKIX_ResourceLimits_GetMaxFanout");
        PKIX_NULLCHECK_TWO(rLimits, pMaxFanout);

        *pMaxFanout = rLimits->maxFanout;

        PKIX_RETURN(RESOURCELIMITS);
}

/*
 * FUNCTION: PKIX_ResourceLimits_SetMaxFanout
 *      (see comments in pkix_params.h)
 */
PKIX_Error *
PKIX_ResourceLimits_SetMaxFanout(
        PKIX_ResourceLimits *rLimits,
        PKIX_UInt32 maxFanout,
        void *plContext)
{
        PKIX_ENTER(RESOURCELIMITS, "PKIX_ResourceLimits_SetMaxFanout");
        PKIX_NULLCHECK_ONE(rLimits);

        rLimits->maxFanout = maxFanout;

        PKIX_RETURN(RESOURCELIMITS);
}

/*
 * FUNCTION: PKIX_ResourceLimits_GetMaxDepth
 *      (see comments in pkix_params.h)
 */
PKIX_Error *
PKIX_ResourceLimits_GetMaxDepth(
        PKIX_ResourceLimits *rLimits,
        PKIX_UInt32 *pMaxDepth,
        void *plContext)
{
        PKIX_ENTER(RESOURCELIMITS, "PKIX_ResourceLimits_GetMaxDepth");
        PKIX_NULLCHECK_TWO(rLimits, pMaxDepth);

        *pMaxDepth = rLimits->maxDepth;

        PKIX_RETURN(RESOURCELIMITS);
}

/*
 * FUNCTION: PKIX_ResourceLimits_SetMaxDepth
 *      (see comments in pkix_params.h)
 */
PKIX_Error *
PKIX_ResourceLimits_SetMaxDepth(
        PKIX_ResourceLimits *rLimits,
        PKIX_UInt32 maxDepth,
        void *plContext)
{
        PKIX_ENTER(RESOURCELIMITS, "PKIX_ResourceLimits_SetMaxDepth");
        PKIX_NULLCHECK_ONE(rLimits);

        rLimits->maxDepth = maxDepth;

        PKIX_RETURN(RESOURCELIMITS);
}

/*
 * FUNCTION: PKIX_ResourceLimits_GetMaxNumberOfCerts
 *      (see comments in pkix_params.h)
 */
PKIX_Error *
PKIX_ResourceLimits_GetMaxNumberOfCerts(
        PKIX_ResourceLimits *rLimits,
        PKIX_UInt32 *pMaxNumber,
        void *plContext)
{
        PKIX_ENTER(RESOURCELIMITS, "PKIX_ResourceLimits_GetMaxNumberOfCerts");
        PKIX_NULLCHECK_TWO(rLimits, pMaxNumber);

        *pMaxNumber = rLimits->maxCertsNumber;

        PKIX_RETURN(RESOURCELIMITS);
}

/*
 * FUNCTION: PKIX_ResourceLimits_SetMaxNumberOfCerts
 *      (see comments in pkix_params.h)
 */
PKIX_Error *
PKIX_ResourceLimits_SetMaxNumberOfCerts(
        PKIX_ResourceLimits *rLimits,
        PKIX_UInt32 maxNumber,
        void *plContext)
{
        PKIX_ENTER(RESOURCELIMITS, "PKIX_ResourceLimits_SetMaxNumberOfCerts");
        PKIX_NULLCHECK_ONE(rLimits);

        rLimits->maxCertsNumber = maxNumber;

        PKIX_RETURN(RESOURCELIMITS);
}

/*
 * FUNCTION: PKIX_ResourceLimits_GetMaxNumberOfCRLs
 *      (see comments in pkix_params.h)
 */
PKIX_Error *
PKIX_ResourceLimits_GetMaxNumberOfCRLs(
        PKIX_ResourceLimits *rLimits,
        PKIX_UInt32 *pMaxNumber,
        void *plContext)
{
        PKIX_ENTER(RESOURCELIMITS, "PKIX_ResourceLimits_GetMaxNumberOfCRLs");
        PKIX_NULLCHECK_TWO(rLimits, pMaxNumber);

        *pMaxNumber = rLimits->maxCrlsNumber;

        PKIX_RETURN(RESOURCELIMITS);
}

/*
 * FUNCTION: PKIX_ResourceLimits_SetMaxNumberOfCRLs
 *      (see comments in pkix_params.h)
 */
PKIX_Error *
PKIX_ResourceLimits_SetMaxNumberOfCRLs(
        PKIX_ResourceLimits *rLimits,
        PKIX_UInt32 maxNumber,
        void *plContext)
{
        PKIX_ENTER(RESOURCELIMITS, "PKIX_ResourceLimits_SetMaxNumberOfCRLs");
        PKIX_NULLCHECK_ONE(rLimits);

        rLimits->maxCrlsNumber = maxNumber;

        PKIX_RETURN(RESOURCELIMITS);
}
