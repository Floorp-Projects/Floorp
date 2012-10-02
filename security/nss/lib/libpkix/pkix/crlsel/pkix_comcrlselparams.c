/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * pkix_comcrlselparams.c
 *
 * ComCRLSelParams Function Definitions
 *
 */

#include "pkix_comcrlselparams.h"

/* --ComCRLSelParams-Private-Functions------------------------------------ */

/*
 * FUNCTION: pkix_ComCrlSelParams_Destroy
 * (see comments for PKIX_PL_DestructorCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_ComCRLSelParams_Destroy(
        PKIX_PL_Object *object,
        void *plContext)
{
        PKIX_ComCRLSelParams *params = NULL;

        PKIX_ENTER(COMCRLSELPARAMS, "pkix_ComCRLSelParams_Destroy");
        PKIX_NULLCHECK_ONE(object);

        PKIX_CHECK(pkix_CheckType
                    (object, PKIX_COMCRLSELPARAMS_TYPE, plContext),
                    PKIX_OBJECTNOTCOMCRLSELPARAMS);

        params = (PKIX_ComCRLSelParams *)object;

        PKIX_DECREF(params->issuerNames);
        PKIX_DECREF(params->cert);
        PKIX_DECREF(params->date);
        PKIX_DECREF(params->maxCRLNumber);
        PKIX_DECREF(params->minCRLNumber);
        PKIX_DECREF(params->crldpList);

cleanup:

        PKIX_RETURN(COMCRLSELPARAMS);
}

/*
 * FUNCTION: pkix_ComCRLSelParams_ToString_Helper
 * DESCRIPTION:
 *
 *  Helper function that creates a string representation of ComCRLSelParams
 *  pointed to by "crlParams" and stores the result at "pString".
 *
 * PARAMETERS
 *  "crlParams"
 *      Address of ComCRLSelParams whose string representation is desired.
 *      Must be non-NULL.
 *  "pString"
 *      Address of object pointer's destination. Must be non-NULL.
 *  "plContext" - Platform-specific context pointer.
 *
 * THREAD SAFETY:
 *  Conditionally Thread Safe
 *      (see Thread Safety Definitions in Programmer's Guide)
 *
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a CRLEntry Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_ComCRLSelParams_ToString_Helper(
        PKIX_ComCRLSelParams *crlParams,
        PKIX_PL_String **pString,
        void *plContext)
{
        PKIX_PL_String *crlIssuerNamesString = NULL;
        PKIX_PL_String *crlDateString = NULL;
        PKIX_PL_String *crlMaxCRLNumberString = NULL;
        PKIX_PL_String *crlMinCRLNumberString = NULL;
        PKIX_PL_String *crlCertString = NULL;
        PKIX_PL_String *crlParamsString = NULL;
        char *asciiFormat = NULL;
        PKIX_PL_String *formatString = NULL;

        PKIX_ENTER(COMCRLSELPARAMS, "pkix_ComCRLSelParams_ToString_Helper");
        PKIX_NULLCHECK_TWO(crlParams, pString);

        asciiFormat =
                "\n\t[\n"
                "\tIssuerNames:     %s\n"
                "\tDate:            %s\n"
                "\tmaxCRLNumber:    %s\n"
                "\tminCRLNumber:    %s\n"
                "\tCertificate:     %s\n"
                "\t]\n";

        PKIX_CHECK(PKIX_PL_String_Create
                    (PKIX_ESCASCII,
                    asciiFormat,
                    0,
                    &formatString,
                    plContext),
                    PKIX_STRINGCREATEFAILED);

        PKIX_TOSTRING
                (crlParams->issuerNames, &crlIssuerNamesString, plContext,
                PKIX_LISTTOSTRINGFAILED);

        PKIX_TOSTRING(crlParams->date, &crlDateString, plContext,
                PKIX_DATETOSTRINGFAILED);

        PKIX_TOSTRING
                (crlParams->maxCRLNumber, &crlMaxCRLNumberString, plContext,
                PKIX_BIGINTTOSTRINGFAILED);

        PKIX_TOSTRING
                (crlParams->minCRLNumber, &crlMinCRLNumberString, plContext,
                PKIX_BIGINTTOSTRINGFAILED);

        PKIX_TOSTRING(crlParams->cert, &crlCertString, plContext,
                PKIX_CERTTOSTRINGFAILED);

        PKIX_CHECK(PKIX_PL_Sprintf
                    (&crlParamsString,
                    plContext,
                    formatString,
                    crlIssuerNamesString,
                    crlDateString,
                    crlMaxCRLNumberString,
                    crlMinCRLNumberString,
                    crlCertString),
                    PKIX_SPRINTFFAILED);

        *pString = crlParamsString;

cleanup:

        PKIX_DECREF(crlIssuerNamesString);
        PKIX_DECREF(crlDateString);
        PKIX_DECREF(crlMaxCRLNumberString);
        PKIX_DECREF(crlMinCRLNumberString);
        PKIX_DECREF(crlCertString);
        PKIX_DECREF(formatString);

        PKIX_RETURN(COMCRLSELPARAMS);
}

/*
 * FUNCTION: pkix_ComCRLSelParams_ToString
 * (see comments for PKIX_PL_ToStringCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_ComCRLSelParams_ToString(
        PKIX_PL_Object *object,
        PKIX_PL_String **pString,
        void *plContext)
{
        PKIX_PL_String *crlParamsString = NULL;
        PKIX_ComCRLSelParams *crlParams = NULL;

        PKIX_ENTER(COMCRLSELPARAMS, "pkix_ComCRLSelParams_ToString");
        PKIX_NULLCHECK_TWO(object, pString);

        PKIX_CHECK(pkix_CheckType(object, PKIX_COMCRLSELPARAMS_TYPE, plContext),
                    PKIX_OBJECTNOTCOMCRLSELPARAMS);

        crlParams = (PKIX_ComCRLSelParams *) object;

        PKIX_CHECK(pkix_ComCRLSelParams_ToString_Helper
                    (crlParams, &crlParamsString, plContext),
                    PKIX_COMCRLSELPARAMSTOSTRINGHELPERFAILED);

        *pString = crlParamsString;

cleanup:

        PKIX_RETURN(COMCRLSELPARAMS);
}

/*
 * FUNCTION: pkix_ComCRLSelParams_Hashcode
 * (see comments for PKIX_PL_HashcodeCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_ComCRLSelParams_Hashcode(
        PKIX_PL_Object *object,
        PKIX_UInt32 *pHashcode,
        void *plContext)
{
        PKIX_ComCRLSelParams *crlParams = NULL;
        PKIX_UInt32 namesHash = 0;
        PKIX_UInt32 certHash = 0;
        PKIX_UInt32 dateHash = 0;
        PKIX_UInt32 maxCRLNumberHash = 0;
        PKIX_UInt32 minCRLNumberHash = 0;
        PKIX_UInt32 hash = 0;

        PKIX_ENTER(COMCRLSELPARAMS, "pkix_ComCRLSelParams_Hashcode");
        PKIX_NULLCHECK_TWO(object, pHashcode);

        PKIX_CHECK(pkix_CheckType(object, PKIX_COMCRLSELPARAMS_TYPE, plContext),
                    PKIX_OBJECTNOTCOMCRLSELPARAMS);

        crlParams = (PKIX_ComCRLSelParams *)object;

        PKIX_HASHCODE(crlParams->issuerNames, &namesHash, plContext,
                    PKIX_OBJECTHASHCODEFAILED);

        PKIX_HASHCODE(crlParams->cert, &certHash, plContext,
                    PKIX_OBJECTHASHCODEFAILED);

        PKIX_HASHCODE(crlParams->date, &dateHash, plContext,
                    PKIX_OBJECTHASHCODEFAILED);

        PKIX_HASHCODE(crlParams->maxCRLNumber, &maxCRLNumberHash, plContext,
                    PKIX_OBJECTHASHCODEFAILED);

        PKIX_HASHCODE(crlParams->minCRLNumber, &minCRLNumberHash, plContext,
                    PKIX_OBJECTHASHCODEFAILED);


        hash = (((namesHash << 3) + certHash) << 3) + dateHash;
        hash = (hash << 3) + maxCRLNumberHash + minCRLNumberHash;

        *pHashcode = hash;

cleanup:

        PKIX_RETURN(COMCRLSELPARAMS);
}

/*
 * FUNCTION: pkix_ComCRLSelParams_Equals
 * (see comments for PKIX_PL_Equals_Callback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_ComCRLSelParams_Equals(
        PKIX_PL_Object *firstObject,
        PKIX_PL_Object *secondObject,
        PKIX_Boolean *pResult,
        void *plContext)
{
        PKIX_ComCRLSelParams *firstCrlParams = NULL;
        PKIX_ComCRLSelParams *secondCrlParams = NULL;
        PKIX_UInt32 secondType;
        PKIX_Boolean cmpResult = PKIX_FALSE;

        PKIX_ENTER(COMCRLSELPARAMS, "pkix_ComCRLSelParams_Equals");
        PKIX_NULLCHECK_THREE(firstObject, secondObject, pResult);

        /* test that firstObject is a ComCRLSelParams */
        PKIX_CHECK(pkix_CheckType
                    (firstObject, PKIX_COMCRLSELPARAMS_TYPE, plContext),
                    PKIX_FIRSTOBJECTNOTCOMCRLSELPARAMS);

        firstCrlParams = (PKIX_ComCRLSelParams *)firstObject;
        secondCrlParams = (PKIX_ComCRLSelParams *)secondObject;

        /*
         * Since we know firstObject is a ComCRLSelParams, if both references
         * are identical, they must be equal
         */
        if (firstCrlParams == secondCrlParams){
                *pResult = PKIX_TRUE;
                goto cleanup;
        }

        /*
         * If secondComCRLSelParams isn't a ComCRLSelParams, we don't
         * throw an error. We simply return a Boolean result of FALSE
         */
        *pResult = PKIX_FALSE;
        PKIX_CHECK(PKIX_PL_Object_GetType
                    ((PKIX_PL_Object *)secondCrlParams, &secondType, plContext),
                    PKIX_COULDNOTGETTYPEOFSECONDARGUMENT);

        if (secondType != PKIX_COMCRLSELPARAMS_TYPE) {
                goto cleanup;
        }

        /* Compare Issuer Names */
        PKIX_EQUALS
                    (firstCrlParams->issuerNames,
                    secondCrlParams->issuerNames,
                    &cmpResult,
                    plContext,
                    PKIX_LISTEQUALSFAILED);

        if (cmpResult != PKIX_TRUE) {
                goto cleanup;
        }

        /* Compare Date */
        PKIX_EQUALS
                    (firstCrlParams->date,
                    secondCrlParams->date,
                    &cmpResult,
                    plContext,
                    PKIX_DATEEQUALSFAILED);

        if (cmpResult != PKIX_TRUE) {
                goto cleanup;
        }

        /* Compare Max CRL Number */
        PKIX_EQUALS
                    (firstCrlParams->maxCRLNumber,
                    secondCrlParams->maxCRLNumber,
                    &cmpResult,
                    plContext,
                    PKIX_BIGINTEQUALSFAILED);

        if (cmpResult != PKIX_TRUE) {
                goto cleanup;
        }

        /* Compare Min CRL Number */
        PKIX_EQUALS
                    (firstCrlParams->minCRLNumber,
                    secondCrlParams->minCRLNumber,
                    &cmpResult,
                    plContext,
                    PKIX_BIGINTEQUALSFAILED);

        if (cmpResult != PKIX_TRUE) {
                goto cleanup;
        }

        /* Compare Cert */
        PKIX_EQUALS
                    (firstCrlParams->cert,
                    secondCrlParams->cert,
                    &cmpResult,
                    plContext,
                    PKIX_CERTEQUALSFAILED);

        *pResult = cmpResult;

cleanup:

        PKIX_RETURN(COMCRLSELPARAMS);
}

/*
 * FUNCTION: pkix_ComCRLSelParams_Duplicate
 * (see comments for PKIX_PL_Duplicate_Callback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_ComCRLSelParams_Duplicate(
        PKIX_PL_Object *object,
        PKIX_PL_Object **pNewObject,
        void *plContext)
{
        PKIX_ComCRLSelParams *old;
        PKIX_ComCRLSelParams *new = NULL;

        PKIX_ENTER(COMCRLSELPARAMS, "pkix_ComCRLSelParams_Duplicate");
        PKIX_NULLCHECK_TWO(object, pNewObject);

        PKIX_CHECK(pkix_CheckType(object, PKIX_COMCRLSELPARAMS_TYPE, plContext),
                    PKIX_OBJECTNOTCOMCRLSELPARAMS);

        old = (PKIX_ComCRLSelParams *)object;

        PKIX_CHECK(PKIX_PL_Object_Alloc
                    (PKIX_COMCRLSELPARAMS_TYPE,
                    (PKIX_UInt32)(sizeof (PKIX_ComCRLSelParams)),
                    (PKIX_PL_Object **)&new,
                    plContext),
                    PKIX_OBJECTALLOCFAILED);

        PKIX_DUPLICATE(old->cert, &new->cert, plContext,
                    PKIX_OBJECTDUPLICATECERTFAILED);

        PKIX_DUPLICATE(old->crldpList, &new->crldpList, plContext,
                    PKIX_OBJECTDUPLICATECERTFAILED);

        PKIX_DUPLICATE(old->issuerNames, &new->issuerNames, plContext,
                    PKIX_OBJECTDUPLICATEISSUERNAMESFAILED);

        PKIX_DUPLICATE(old->date, &new->date, plContext,
                    PKIX_OBJECTDUPLICATEDATEFAILED);

        PKIX_DUPLICATE(old->maxCRLNumber, &new->maxCRLNumber, plContext,
                    PKIX_OBJECTDUPLICATEMAXCRLNUMBERFAILED);

        PKIX_DUPLICATE(old->minCRLNumber, &new->minCRLNumber, plContext,
                    PKIX_OBJECTDUPLICATEMINCRLNUMBERFAILED);

        *pNewObject = (PKIX_PL_Object *)new;

cleanup:

        if (PKIX_ERROR_RECEIVED){
                PKIX_DECREF(new);
        }

        PKIX_RETURN(COMCRLSELPARAMS);
}

/*
 * FUNCTION: pkix_ComCrlSelParams_RegisterSelf
 * DESCRIPTION:
 *  Registers PKIX_COMCRLSELPARAMS_TYPE and its related functions with
 *  systemClasses[]
 * THREAD SAFETY:
 *  Not Thread Safe - for performance and complexity reasons
 *
 *  Since this function is only called by PKIX_PL_Initialize, which should
 *  only be called once, it is acceptable that this function is not
 *  thread-safe.
 */
PKIX_Error *
pkix_ComCRLSelParams_RegisterSelf(void *plContext)
{
        extern pkix_ClassTable_Entry systemClasses[PKIX_NUMTYPES];
        pkix_ClassTable_Entry entry;

        PKIX_ENTER(COMCRLSELPARAMS, "pkix_ComCRLSelParams_RegisterSelf");

        entry.description = "ComCRLSelParams";
        entry.objCounter = 0;
        entry.typeObjectSize = sizeof(PKIX_ComCRLSelParams);
        entry.destructor = pkix_ComCRLSelParams_Destroy;
        entry.equalsFunction = pkix_ComCRLSelParams_Equals;
        entry.hashcodeFunction = pkix_ComCRLSelParams_Hashcode;
        entry.toStringFunction = pkix_ComCRLSelParams_ToString;
        entry.comparator = NULL;
        entry.duplicateFunction = pkix_ComCRLSelParams_Duplicate;

        systemClasses[PKIX_COMCRLSELPARAMS_TYPE] = entry;

        PKIX_RETURN(COMCRLSELPARAMS);
}

/* --ComCRLSelParams-Public-Functions------------------------------------- */

/*
 * FUNCTION: PKIX_ComCRLSelParams_Create (see comments in pkix_crlsel.h)
 */
PKIX_Error *
PKIX_ComCRLSelParams_Create(
        PKIX_ComCRLSelParams **pParams,
        void *plContext)
{
        PKIX_ComCRLSelParams *params = NULL;

        PKIX_ENTER(COMCRLSELPARAMS, "PKIX_ComCRLSelParams_Create");
        PKIX_NULLCHECK_ONE(pParams);

        PKIX_CHECK(PKIX_PL_Object_Alloc
                    (PKIX_COMCRLSELPARAMS_TYPE,
                    sizeof (PKIX_ComCRLSelParams),
                    (PKIX_PL_Object **)&params,
                    plContext),
                    PKIX_COULDNOTCREATECOMMONCRLSELECTORPARAMSOBJECT);

        /* initialize fields */
        params->issuerNames = NULL;
        params->cert = NULL;
        params->crldpList = NULL;
        params->date = NULL;
        params->nistPolicyEnabled = PKIX_TRUE;
        params->maxCRLNumber = NULL;
        params->minCRLNumber = NULL;

        *pParams = params;

cleanup:

        PKIX_RETURN(COMCRLSELPARAMS);
}

/*
 * FUNCTION: PKIX_ComCRLSelParams_GetIssuerNames (see comments in pkix_crlsel.h)
 */
PKIX_Error *
PKIX_ComCRLSelParams_GetIssuerNames(
        PKIX_ComCRLSelParams *params,
        PKIX_List **pIssuerNames,
        void *plContext)
{
        PKIX_ENTER(COMCRLSELPARAMS, "PKIX_ComCRLSelParams_GetIssuerNames");
        PKIX_NULLCHECK_TWO(params, pIssuerNames);

        PKIX_INCREF(params->issuerNames);

        *pIssuerNames = params->issuerNames;

cleanup:
        PKIX_RETURN(COMCRLSELPARAMS);
}

/*
 * FUNCTION: PKIX_ComCRLSelParams_SetIssuerNames (see comments in pkix_crlsel.h)
 */
PKIX_Error *
PKIX_ComCRLSelParams_SetIssuerNames(
        PKIX_ComCRLSelParams *params,
        PKIX_List *names,
        void *plContext)
{
        PKIX_ENTER(COMCRLSELPARAMS, "PKIX_ComCRLSelParams_SetIssuerNames");
        PKIX_NULLCHECK_ONE(params); /* allows null for names from spec */

        PKIX_DECREF(params->issuerNames);

        PKIX_INCREF(names); /* if NULL, allows to reset for no action */

        params->issuerNames = names;

        PKIX_CHECK(PKIX_PL_Object_InvalidateCache
                    ((PKIX_PL_Object *)params, plContext),
                    PKIX_OBJECTINVALIDATECACHEFAILED);

cleanup:

        PKIX_RETURN(COMCRLSELPARAMS);
}

/*
 * FUNCTION: PKIX_ComCRLSelParams_AddIssuerName (see comments in pkix_crlsel.h)
 */
PKIX_Error *
PKIX_ComCRLSelParams_AddIssuerName(
        PKIX_ComCRLSelParams *params,
        PKIX_PL_X500Name *name,
        void *plContext)
{
        PKIX_List *list = NULL;

        PKIX_ENTER(COMCRLSELPARAMS, "PKIX_ComCRLSelParams_AddIssuerName");
        PKIX_NULLCHECK_ONE(params);

        if (name != NULL) {

                if (params->issuerNames == NULL) {

                    PKIX_CHECK(PKIX_List_Create(&list, plContext),
                        PKIX_LISTCREATEFAILED);
                        params->issuerNames = list;
                }

                PKIX_CHECK(PKIX_List_AppendItem
                    (params->issuerNames, (PKIX_PL_Object *)name, plContext),
                    PKIX_LISTAPPENDITEMFAILED);

                PKIX_CHECK(PKIX_PL_Object_InvalidateCache
                    ((PKIX_PL_Object *)params, plContext),
                    PKIX_OBJECTINVALIDATECACHEFAILED);

        }

cleanup:

        PKIX_RETURN(COMCRLSELPARAMS);
}


/*
 * FUNCTION: PKIX_ComCRLSelParams_GetCertificateChecking
 * (see comments in pkix_crlsel.h)
 */
PKIX_Error *
PKIX_ComCRLSelParams_GetCertificateChecking(
        PKIX_ComCRLSelParams *params,
        PKIX_PL_Cert **pCert,
        void *plContext)
{
        PKIX_ENTER(COMCRLSELPARAMS,
                    "PKIX_ComCRLSelParams_GetCertificateChecking");
        PKIX_NULLCHECK_TWO(params, pCert);

        PKIX_INCREF(params->cert);

        *pCert = params->cert;

cleanup:
        PKIX_RETURN(COMCRLSELPARAMS);
}

/*
 * FUNCTION: PKIX_ComCRLSelParams_SetCertificateChecking
 * (see comments in pkix_crlsel.h)
 */
PKIX_Error *
PKIX_ComCRLSelParams_SetCertificateChecking(
        PKIX_ComCRLSelParams *params,
        PKIX_PL_Cert *cert,
        void *plContext)
{
        PKIX_ENTER(COMCRLSELPARAMS,
                    "PKIX_ComCRLSelParams_SetCertificateChecking");
        PKIX_NULLCHECK_ONE(params); /* allows cert to be NULL from spec */

        PKIX_DECREF(params->cert);

        PKIX_INCREF(cert);

        params->cert = cert;

        PKIX_CHECK(PKIX_PL_Object_InvalidateCache
                    ((PKIX_PL_Object *)params, plContext),
                    PKIX_OBJECTINVALIDATECACHEFAILED);

cleanup:

        PKIX_RETURN(COMCRLSELPARAMS);
}


/*
 * FUNCTION: PKIX_ComCRLSelParams_GetDateAndTime (see comments in pkix_crlsel.h)
 */
PKIX_Error *
PKIX_ComCRLSelParams_GetDateAndTime(
        PKIX_ComCRLSelParams *params,
        PKIX_PL_Date **pDate,
        void *plContext)
{
        PKIX_ENTER(COMCRLSELPARAMS,
                    "PKIX_ComCRLSelParams_GetDateAndTime");
        PKIX_NULLCHECK_TWO(params, pDate);

        PKIX_INCREF(params->date);

        *pDate = params->date;

cleanup:
        PKIX_RETURN(COMCRLSELPARAMS);
}

/*
 * FUNCTION: PKIX_ComCRLSelParams_SetDateAndTime (see comments in pkix_crlsel.h)
 */
PKIX_Error *
PKIX_ComCRLSelParams_SetDateAndTime(
        PKIX_ComCRLSelParams *params,
        PKIX_PL_Date *date,
        void *plContext)
{
        PKIX_ENTER(COMCRLSELPARAMS,
                    "PKIX_ComCRLSelParams_SetDateAndTime");
        PKIX_NULLCHECK_ONE(params); /* allows date to be NULL from spec */

        PKIX_DECREF (params->date);

        PKIX_INCREF(date);

        params->date = date;

        PKIX_CHECK(PKIX_PL_Object_InvalidateCache
                    ((PKIX_PL_Object *)params, plContext),
                    PKIX_OBJECTINVALIDATECACHEFAILED);

cleanup:

        PKIX_RETURN(COMCRLSELPARAMS);
}

/*
 * FUNCTION: PKIX_ComCRLSelParams_GetDateAndTime (see comments in pkix_crlsel.h)
 */
PKIX_Error *
PKIX_ComCRLSelParams_GetNISTPolicyEnabled(
        PKIX_ComCRLSelParams *params,
        PKIX_Boolean *pEnabled,
        void *plContext)
{
        PKIX_ENTER(COMCRLSELPARAMS,
                    "PKIX_ComCRLSelParams_GetNISTPolicyEnabled");
        PKIX_NULLCHECK_TWO(params, pEnabled);

        *pEnabled = params->nistPolicyEnabled;

        PKIX_RETURN(COMCRLSELPARAMS);
}

/*
 * FUNCTION: PKIX_ComCRLSelParams_SetDateAndTime (see comments in pkix_crlsel.h)
 */
PKIX_Error *
PKIX_ComCRLSelParams_SetNISTPolicyEnabled(
        PKIX_ComCRLSelParams *params,
        PKIX_Boolean enabled,
        void *plContext)
{
        PKIX_ENTER(COMCRLSELPARAMS,
                    "PKIX_ComCRLSelParams_SetNISTPolicyEnabled");
        PKIX_NULLCHECK_ONE(params); /* allows date to be NULL from spec */

        params->nistPolicyEnabled = enabled;

        PKIX_CHECK(PKIX_PL_Object_InvalidateCache
                    ((PKIX_PL_Object *)params, plContext),
                    PKIX_OBJECTINVALIDATECACHEFAILED);

cleanup:

        PKIX_RETURN(COMCRLSELPARAMS);
}

/*
 * FUNCTION: PKIX_ComCRLSelParams_GetMaxCRLNumber
 * (see comments in pkix_crlsel.h)
 */
PKIX_Error *
PKIX_ComCRLSelParams_GetMaxCRLNumber(
        PKIX_ComCRLSelParams *params,
        PKIX_PL_BigInt **pMaxCRLNumber,
        void *plContext)
{
        PKIX_ENTER(COMCRLSELPARAMS,
                    "PKIX_ComCRLSelParams_GetMaxCRLNumber");
        PKIX_NULLCHECK_TWO(params, pMaxCRLNumber);

        PKIX_INCREF(params->maxCRLNumber);

        *pMaxCRLNumber = params->maxCRLNumber;

cleanup:
        PKIX_RETURN(COMCRLSELPARAMS);
}

/*
 * FUNCTION: PKIX_ComCRLSelParams_SetMaxCRLNumber
 * (see comments in pkix_crlsel.h)
 */
PKIX_Error *
PKIX_ComCRLSelParams_SetMaxCRLNumber(
        PKIX_ComCRLSelParams *params,
        PKIX_PL_BigInt *maxCRLNumber,
        void *plContext)
{
        PKIX_ENTER(COMCRLSELPARAMS,
                    "PKIX_ComCRLSelParams_SetMaxCRLNumber");
        PKIX_NULLCHECK_ONE(params); /* maxCRLNumber can be NULL - from spec */

        PKIX_DECREF(params->maxCRLNumber);

        PKIX_INCREF(maxCRLNumber);

        params->maxCRLNumber = maxCRLNumber;

        PKIX_CHECK(PKIX_PL_Object_InvalidateCache
                    ((PKIX_PL_Object *)params, plContext),
                    PKIX_OBJECTINVALIDATECACHEFAILED);

cleanup:

        PKIX_RETURN(COMCRLSELPARAMS);
}


/*
 * FUNCTION: PKIX_ComCRLSelParams_GetMinCRLNumber
 * (see comments in pkix_crlsel.h)
 */
PKIX_Error *
PKIX_ComCRLSelParams_GetMinCRLNumber(
        PKIX_ComCRLSelParams *params,
        PKIX_PL_BigInt **pMinCRLNumber,
        void *plContext)
{
        PKIX_ENTER(COMCRLSELPARAMS,
                    "PKIX_ComCRLSelParams_GetMinCRLNumber");
        PKIX_NULLCHECK_TWO(params, pMinCRLNumber);

        PKIX_INCREF(params->minCRLNumber);

        *pMinCRLNumber = params->minCRLNumber;

cleanup:
        PKIX_RETURN(COMCRLSELPARAMS);
}

/*
 * FUNCTION: PKIX_ComCRLSelParams_SetMinCRLNumber
 * (see comments in pkix_crlsel.h)
 */
PKIX_Error *
PKIX_ComCRLSelParams_SetMinCRLNumber(
        PKIX_ComCRLSelParams *params,
        PKIX_PL_BigInt *minCRLNumber,
        void *plContext)
{
        PKIX_ENTER(COMCRLSELPARAMS,
                    "PKIX_ComCRLSelParams_SetMinCRLNumber");
        PKIX_NULLCHECK_ONE(params); /* minCRLNumber can be NULL - from spec */

        PKIX_DECREF(params->minCRLNumber);

        PKIX_INCREF(minCRLNumber);

        params->minCRLNumber = minCRLNumber;

        PKIX_CHECK(PKIX_PL_Object_InvalidateCache
                    ((PKIX_PL_Object *)params, plContext),
                    PKIX_OBJECTINVALIDATECACHEFAILED);

cleanup:

        PKIX_RETURN(COMCRLSELPARAMS);
}


PKIX_Error*
PKIX_ComCRLSelParams_SetCrlDp(
         PKIX_ComCRLSelParams *params,
         PKIX_List *crldpList,
         void *plContext)
{
    PKIX_ENTER(COMCRLSELPARAMS, "PKIX_ComCRLSelParams_SetCrlDp");
    PKIX_NULLCHECK_ONE(params); /* minCRLNumber can be NULL - from spec */

    PKIX_INCREF(crldpList);
    params->crldpList = crldpList;

    PKIX_CHECK(PKIX_PL_Object_InvalidateCache
               ((PKIX_PL_Object *)params, plContext),
               PKIX_OBJECTINVALIDATECACHEFAILED);
cleanup:

        PKIX_RETURN(COMCRLSELPARAMS);
}
