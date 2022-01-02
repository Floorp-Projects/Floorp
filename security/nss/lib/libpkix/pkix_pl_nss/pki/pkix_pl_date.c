/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * pkix_pl_date.c
 *
 * Date Object Definitions
 *
 */

#include "pkix_pl_date.h"

/* --Private-Date-Functions------------------------------------- */
/*
 * FUNCTION: pkix_pl_Date_GetPRTime
 * DESCRIPTION:
 *
 *  Translates into a PRTime the Date embodied by the Date object pointed to
 *  by "date", and stores it at "pPRTime".
 *
 * PARAMETERS
 *  "date"
 *      Address of Date whose PRTime representation is desired. Must be
 *      non-NULL.
 *  "pPRTime"
 *      Address where PRTime value will be stored. Must be non-NULL.
 *  "plContext" - Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Date Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_Date_GetPRTime(
        PKIX_PL_Date *date,
        PRTime *pPRTime,
        void *plContext)
{
        PKIX_ENTER(DATE, "PKIX_PL_Date_GetPRTime");
        PKIX_NULLCHECK_TWO(date, pPRTime);

        *pPRTime = date->nssTime;

        PKIX_RETURN(DATE);
}

/*
 * FUNCTION: pkix_pl_Date_CreateFromPRTime
 * DESCRIPTION:
 *
 *  Creates a new Date from the PRTime whose value is "prtime", and stores the
 *  result at "pDate".
 *
 * PARAMETERS
 *  "prtime"
 *      The PRTime value to be embodied in the new Date object.
 *  "pDate"
 *      Address where object pointer will be stored. Must be non-NULL.
 *  "plContext" - Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Date Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_Date_CreateFromPRTime(
        PRTime prtime,
        PKIX_PL_Date **pDate,
        void *plContext)
{
        PKIX_PL_Date *date = NULL;

        PKIX_ENTER(DATE, "PKIX_PL_Date_CreateFromPRTime");
        PKIX_NULLCHECK_ONE(pDate);

        /* create a PKIX_PL_Date object */
        PKIX_CHECK(PKIX_PL_Object_Alloc
                    (PKIX_DATE_TYPE,
                    sizeof (PKIX_PL_Date),
                    (PKIX_PL_Object **)&date,
                    plContext),
                    PKIX_COULDNOTCREATEOBJECT);
        /* populate the nssTime field */
        date->nssTime = prtime;
        *pDate = date;
cleanup:
        PKIX_RETURN(DATE);
}

/*
 * FUNCTION: pkix_pl_Date_ToString_Helper
 * DESCRIPTION:
 *
 *  Helper function that creates a string representation of the SECItem pointed
 *  to by "nssTime" (which represents a date) and stores it at "pString".
 *
 * PARAMETERS
 *  "nssTime"
 *      Address of SECItem whose string representation is desired.
 *      Must be non-NULL.
 *  "pString"
 *      Address where object pointer will be stored. Must be non-NULL.
 *  "plContext" - Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Date Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_Date_ToString_Helper(
        SECItem *nssTime,
        PKIX_PL_String **pString,
        void *plContext)
{
        char *asciiDate = NULL;

        PKIX_ENTER(DATE, "pkix_pl_Date_ToString_Helper");
        PKIX_NULLCHECK_TWO(nssTime, pString);

        switch (nssTime->type) {
        case siUTCTime:
                PKIX_PL_NSSCALLRV
                        (DATE, asciiDate, DER_UTCDayToAscii, (nssTime));
                if (!asciiDate){
                        PKIX_ERROR(PKIX_DERUTCTIMETOASCIIFAILED);
                }
                break;
        case siGeneralizedTime:
                /*
                 * we don't currently have any way to create GeneralizedTime.
                 * this code is only here so that it will be in place when
                 * we do have the capability to create GeneralizedTime.
                 */
                PKIX_PL_NSSCALLRV
                        (DATE, asciiDate, DER_GeneralizedDayToAscii, (nssTime));
                if (!asciiDate){
                        PKIX_ERROR(PKIX_DERGENERALIZEDDAYTOASCIIFAILED);
                }
                break;
        default:
                PKIX_ERROR(PKIX_UNRECOGNIZEDTIMETYPE);
        }

        PKIX_CHECK(PKIX_PL_String_Create
                    (PKIX_ESCASCII, asciiDate, 0, pString, plContext),
                    PKIX_STRINGCREATEFAILED);

cleanup:
        PR_Free(asciiDate);

        PKIX_RETURN(DATE);
}


/*
 * FUNCTION: pkix_pl_Date_Destroy
 * (see comments for PKIX_PL_DestructorCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_Date_Destroy(
        PKIX_PL_Object *object,
        void *plContext)
{
        PKIX_ENTER(DATE, "pkix_pl_Date_Destroy");
        PKIX_NULLCHECK_ONE(object);

        PKIX_CHECK(pkix_CheckType(object, PKIX_DATE_TYPE, plContext),
                    PKIX_OBJECTNOTDATE);
cleanup:
        PKIX_RETURN(DATE);
}

/*
 * FUNCTION: pkix_pl_Date_ToString
 * (see comments for PKIX_PL_ToStringCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_Date_ToString(
        PKIX_PL_Object *object,
        PKIX_PL_String **pString,
        void *plContext)
{
        PKIX_PL_Date *date = NULL;
        SECItem nssTime = {siBuffer, NULL, 0};
        SECStatus rv;

        PKIX_ENTER(DATE, "pkix_pl_Date_toString");
        PKIX_NULLCHECK_TWO(object, pString);

        PKIX_CHECK(pkix_CheckType(object, PKIX_DATE_TYPE, plContext),
                    PKIX_OBJECTNOTDATE);

        date = (PKIX_PL_Date *)object;
        rv = DER_EncodeTimeChoice(NULL, &nssTime, date->nssTime);
        if (rv == SECFailure) {
            PKIX_ERROR(PKIX_DERENCODETIMECHOICEFAILED);
        }
        PKIX_CHECK(pkix_pl_Date_ToString_Helper
                    (&nssTime, pString, plContext),
                    PKIX_DATETOSTRINGHELPERFAILED);
cleanup:
        if (nssTime.data) {
            SECITEM_FreeItem(&nssTime, PR_FALSE);
        }

        PKIX_RETURN(DATE);
}

/*
 * FUNCTION: pkix_pl_Date_Hashcode
 * (see comments for PKIX_PL_HashcodeCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_Date_Hashcode(
        PKIX_PL_Object *object,
        PKIX_UInt32 *pHashcode,
        void *plContext)
{
        PKIX_PL_Date *date = NULL;
        PKIX_UInt32 dateHash;

        PKIX_ENTER(DATE, "pkix_pl_Date_Hashcode");
        PKIX_NULLCHECK_TWO(object, pHashcode);

        PKIX_CHECK(pkix_CheckType(object, PKIX_DATE_TYPE, plContext),
                    PKIX_OBJECTNOTDATE);

        date = (PKIX_PL_Date *)object;

        PKIX_CHECK(pkix_hash
                ((const unsigned char *)&date->nssTime,
                sizeof(date->nssTime),
                &dateHash,
                plContext),
                PKIX_HASHFAILED);

        *pHashcode = dateHash;

cleanup:

        PKIX_RETURN(DATE);

}

/*
 * FUNCTION: pkix_pl_Date_Comparator
 * (see comments for PKIX_PL_ComparatorCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_Date_Comparator(
        PKIX_PL_Object *firstObject,
        PKIX_PL_Object *secondObject,
        PKIX_Int32 *pResult,
        void *plContext)
{
        PRTime firstTime;
        PRTime secondTime;
        SECComparison cmpResult;

        PKIX_ENTER(DATE, "pkix_pl_Date_Comparator");
        PKIX_NULLCHECK_THREE(firstObject, secondObject, pResult);

        PKIX_CHECK(pkix_CheckTypes
                    (firstObject, secondObject, PKIX_DATE_TYPE, plContext),
                    PKIX_ARGUMENTSNOTDATES);

        firstTime = ((PKIX_PL_Date *)firstObject)->nssTime;
        secondTime = ((PKIX_PL_Date *)secondObject)->nssTime;

        if (firstTime == secondTime)
            cmpResult = SECEqual;
        else if (firstTime < secondTime)
            cmpResult = SECLessThan;
        else
            cmpResult = SECGreaterThan;

        *pResult = cmpResult;

cleanup:

        PKIX_RETURN(DATE);
}

/*
 * FUNCTION: pkix_pl_Date_Equals
 * (see comments for PKIX_PL_Equals_Callback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_Date_Equals(
        PKIX_PL_Object *firstObject,
        PKIX_PL_Object *secondObject,
        PKIX_Boolean *pResult,
        void *plContext)
{
        PKIX_ENTER(DATE, "pkix_pl_Date_Equals");
        PKIX_NULLCHECK_THREE(firstObject, secondObject, pResult);

        /* test that firstObject is a Date */
        PKIX_CHECK(pkix_CheckType(firstObject, PKIX_DATE_TYPE, plContext),
                PKIX_FIRSTOBJECTNOTDATE);

        /*
         * Since we know firstObject is a Date, if both references are
         * identical, they must be equal
         */
        if (firstObject == secondObject){
                *pResult = PKIX_TRUE;
                goto cleanup;
        }

        *pResult = PKIX_FALSE;
        pkixErrorResult =
            pkix_pl_Date_Comparator(firstObject, secondObject,
                                    pResult, plContext);
        if (pkixErrorResult) {
            PKIX_DECREF(pkixErrorResult);
        }
            
cleanup:

        PKIX_RETURN(DATE);
}

/*
 * FUNCTION: pkix_pl_Date_RegisterSelf
 * DESCRIPTION:
 *  Registers PKIX_DATE_TYPE and its related functions with systemClasses[]
 * THREAD SAFETY:
 *  Not Thread Safe - for performance and complexity reasons
 *
 *  Since this function is only called by PKIX_PL_Initialize, which should
 *  only be called once, it is acceptable that this function is not
 *  thread-safe.
 */
PKIX_Error *
pkix_pl_Date_RegisterSelf(void *plContext)
{
        extern pkix_ClassTable_Entry systemClasses[PKIX_NUMTYPES];
        pkix_ClassTable_Entry* entry = &systemClasses[PKIX_DATE_TYPE];

        PKIX_ENTER(CRLCHECKER, "pkix_CrlDp_RegisterSelf");

        entry->description = "Date";
        entry->typeObjectSize = sizeof(PKIX_PL_Date);
        entry->destructor = pkix_pl_Date_Destroy;
        entry->equalsFunction = pkix_pl_Date_Equals;
        entry->hashcodeFunction = pkix_pl_Date_Hashcode;
        entry->toStringFunction = pkix_pl_Date_ToString;
        entry->comparator = pkix_pl_Date_Comparator;
        entry->duplicateFunction = pkix_duplicateImmutable;

        PKIX_RETURN(DATE);
}

/* --Public-Functions------------------------------------------------------- */

/*
 * FUNCTION: PKIX_PL_Date_Create_UTCTime (see comments in pkix_pl_pki.h)
 */
PKIX_Error *
PKIX_PL_Date_Create_UTCTime(
        PKIX_PL_String *stringRep,
        PKIX_PL_Date **pDate,
        void *plContext)
{
        PKIX_PL_Date *date = NULL;
        char *asciiString = NULL;
        PKIX_UInt32 escAsciiLength;
        SECStatus rv;
        PRTime time;

        PKIX_ENTER(DATE, "PKIX_PL_Date_Create_UTCTime");
        PKIX_NULLCHECK_ONE(pDate);

        if (stringRep == NULL){
                PKIX_DATE_DEBUG("\t\tCalling PR_Now).\n");
                time = PR_Now();
        } else {
                /* convert the input PKIX_PL_String to PKIX_ESCASCII */
                PKIX_CHECK(PKIX_PL_String_GetEncoded
                            (stringRep,
                            PKIX_ESCASCII,
                            (void **)&asciiString,
                            &escAsciiLength,
                            plContext),
                            PKIX_STRINGGETENCODEDFAILED);

                PKIX_DATE_DEBUG("\t\tCalling DER_AsciiToTime).\n");
                /* DER_AsciiToTime only supports UTCTime (2-digit years) */
                rv = DER_AsciiToTime(&time, asciiString);
                if (rv != SECSuccess){
                        PKIX_ERROR(PKIX_DERASCIITOTIMEFAILED);
                }
        }

        /* create a PKIX_PL_Date object */
        PKIX_CHECK(PKIX_PL_Object_Alloc
                    (PKIX_DATE_TYPE,
                    sizeof (PKIX_PL_Date),
                    (PKIX_PL_Object **)&date,
                    plContext),
                    PKIX_COULDNOTCREATEOBJECT);

        /* populate the nssTime field */
        date->nssTime = time;
        *pDate = date;

cleanup:
        PKIX_FREE(asciiString);

        PKIX_RETURN(DATE);
}

/*
 * FUNCTION: PKIX_PL_Date_Create_CurrentOffBySeconds
 * (see comments in pkix_pl_pki.h)
 */
PKIX_Error *
PKIX_PL_Date_Create_CurrentOffBySeconds(
        PKIX_Int32 secondsOffset,
        PKIX_PL_Date **pDate,
        void *plContext)
{
        PKIX_PL_Date *date = NULL;
        PRTime time;

        PKIX_ENTER(DATE, "PKIX_PL_Date_Create_CurrentOffBySeconds");
        PKIX_NULLCHECK_ONE(pDate);

        time = PR_Now() + PR_SecondsToInterval(secondsOffset);
        /* create a PKIX_PL_Date object */
        PKIX_CHECK(PKIX_PL_Object_Alloc
                    (PKIX_DATE_TYPE,
                    sizeof (PKIX_PL_Date),
                    (PKIX_PL_Object **)&date,
                    plContext),
                    PKIX_COULDNOTCREATEOBJECT);

        /* populate the nssTime field */
        date->nssTime = time;
        *pDate = date;

cleanup:
        PKIX_RETURN(DATE);
}

PKIX_Error *
PKIX_PL_Date_CreateFromPRTime(
        PRTime prtime,
        PKIX_PL_Date **pDate,
        void *plContext)
{
    PKIX_ENTER(DATE, "PKIX_PL_Date_CreateFromPRTime");
    PKIX_CHECK(
        pkix_pl_Date_CreateFromPRTime(prtime, pDate, plContext),
        PKIX_DATECREATEFROMPRTIMEFAILED);

cleanup:
    PKIX_RETURN(DATE);
}
