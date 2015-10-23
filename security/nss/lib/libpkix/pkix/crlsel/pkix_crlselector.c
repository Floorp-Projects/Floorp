/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * pkix_crlselector.c
 *
 * CRLSelector Function Definitions
 *
 */

#include "pkix_crlselector.h"

/* --CRLSelector Private-Functions-------------------------------------- */

/*
 * FUNCTION: pkix_CRLSelector_Destroy
 * (see comments for PKIX_PL_DestructorCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_CRLSelector_Destroy(
        PKIX_PL_Object *object,
        void *plContext)
{
        PKIX_CRLSelector *selector = NULL;

        PKIX_ENTER(CRLSELECTOR, "pkix_CRLSelector_Destroy");
        PKIX_NULLCHECK_ONE(object);

        PKIX_CHECK(pkix_CheckType(object, PKIX_CRLSELECTOR_TYPE, plContext),
                    PKIX_OBJECTNOTCRLSELECTOR);

        selector = (PKIX_CRLSelector *)object;

        selector->matchCallback = NULL;

        PKIX_DECREF(selector->params);
        PKIX_DECREF(selector->context);

cleanup:

        PKIX_RETURN(CRLSELECTOR);
}

/*
 * FUNCTION: pkix_CRLSelector_ToString_Helper
 *
 * DESCRIPTION:
 *  Helper function that creates a string representation of CRLSelector
 *  pointed to by "crlParams" and stores its address in the object pointed to
 *  by "pString".
 *
 * PARAMETERS
 *  "list"
 *      Address of CRLSelector whose string representation is desired.
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
 *  Returns a CRLSelector Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_CRLSelector_ToString_Helper(
        PKIX_CRLSelector *crlSelector,
        PKIX_PL_String **pString,
        void *plContext)
{
        PKIX_PL_String *crlSelectorString = NULL;
        PKIX_PL_String *formatString = NULL;
        PKIX_PL_String *crlParamsString = NULL;
        PKIX_PL_String *crlContextString = NULL;
        char *asciiFormat = NULL;

        PKIX_ENTER(CRLSELECTOR, "pkix_CRLSelector_ToString_Helper");
        PKIX_NULLCHECK_TWO(crlSelector, pString);
        PKIX_NULLCHECK_ONE(crlSelector->params);

        asciiFormat =
                "\n\t[\n"
                "\tMatchCallback: 0x%x\n"
                "\tParams:          %s\n"
                "\tContext:         %s\n"
                "\t]\n";

        PKIX_CHECK(PKIX_PL_String_Create
                    (PKIX_ESCASCII,
                    asciiFormat,
                    0,
                    &formatString,
                    plContext),
                    PKIX_STRINGCREATEFAILED);

        /* Params */
        PKIX_TOSTRING
                    ((PKIX_PL_Object *)crlSelector->params,
                    &crlParamsString,
                    plContext,
                    PKIX_COMCRLSELPARAMSTOSTRINGFAILED);

        /* Context */
        PKIX_TOSTRING(crlSelector->context, &crlContextString, plContext,
                    PKIX_LISTTOSTRINGFAILED);

        PKIX_CHECK(PKIX_PL_Sprintf
                    (&crlSelectorString,
                    plContext,
                    formatString,
                    crlSelector->matchCallback,
                    crlParamsString,
                    crlContextString),
                    PKIX_SPRINTFFAILED);

        *pString = crlSelectorString;

cleanup:

        PKIX_DECREF(crlParamsString);
        PKIX_DECREF(crlContextString);
        PKIX_DECREF(formatString);

        PKIX_RETURN(CRLSELECTOR);
}

/*
 * FUNCTION: pkix_CRLSelector_ToString
 * (see comments for PKIX_PL_ToStringCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_CRLSelector_ToString(
        PKIX_PL_Object *object,
        PKIX_PL_String **pString,
        void *plContext)
{
        PKIX_PL_String *crlSelectorString = NULL;
        PKIX_CRLSelector *crlSelector = NULL;

        PKIX_ENTER(CRLSELECTOR, "pkix_CRLSelector_ToString");
        PKIX_NULLCHECK_TWO(object, pString);

        PKIX_CHECK(pkix_CheckType(object, PKIX_CRLSELECTOR_TYPE, plContext),
                    PKIX_OBJECTNOTCRLSELECTOR);

        crlSelector = (PKIX_CRLSelector *) object;

        PKIX_CHECK(pkix_CRLSelector_ToString_Helper
                    (crlSelector, &crlSelectorString, plContext),
                    PKIX_CRLSELECTORTOSTRINGHELPERFAILED);

        *pString = crlSelectorString;

cleanup:

        PKIX_RETURN(CRLSELECTOR);
}

/*
 * FUNCTION: pkix_CRLSelector_Hashcode
 * (see comments for PKIX_PL_HashcodeCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_CRLSelector_Hashcode(
        PKIX_PL_Object *object,
        PKIX_UInt32 *pHashcode,
        void *plContext)
{
        PKIX_UInt32 paramsHash = 0;
        PKIX_UInt32 contextHash = 0;
        PKIX_UInt32 hash = 0;

        PKIX_CRLSelector *crlSelector = NULL;

        PKIX_ENTER(CRLSELECTOR, "pkix_CRLSelector_Hashcode");
        PKIX_NULLCHECK_TWO(object, pHashcode);

        PKIX_CHECK(pkix_CheckType(object, PKIX_CRLSELECTOR_TYPE, plContext),
                    PKIX_OBJECTNOTCRLSELECTOR);

        crlSelector = (PKIX_CRLSelector *)object;

        PKIX_HASHCODE(crlSelector->params, &paramsHash, plContext,
                PKIX_OBJECTHASHCODEFAILED);

        PKIX_HASHCODE(crlSelector->context, &contextHash, plContext,
                PKIX_OBJECTHASHCODEFAILED);

        hash = 31 * ((PKIX_UInt32)((char *)crlSelector->matchCallback - (char *)NULL) +
                    (contextHash << 3)) + paramsHash;

        *pHashcode = hash;

cleanup:

        PKIX_RETURN(CRLSELECTOR);
}

/*
 * FUNCTION: pkix_CRLSelector_Equals
 * (see comments for PKIX_PL_Equals_Callback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_CRLSelector_Equals(
        PKIX_PL_Object *firstObject,
        PKIX_PL_Object *secondObject,
        PKIX_Boolean *pResult,
        void *plContext)
{
        PKIX_CRLSelector *firstCrlSelector = NULL;
        PKIX_CRLSelector *secondCrlSelector = NULL;
        PKIX_UInt32 secondType;
        PKIX_Boolean cmpResult = PKIX_FALSE;

        PKIX_ENTER(CRLSELECTOR, "pkix_CRLSelector_Equals");
        PKIX_NULLCHECK_THREE(firstObject, secondObject, pResult);

        /* test that firstObject is a CRLSelector */
        PKIX_CHECK(pkix_CheckType
                    (firstObject, PKIX_CRLSELECTOR_TYPE, plContext),
                    PKIX_FIRSTOBJECTNOTCRLSELECTOR);

        firstCrlSelector = (PKIX_CRLSelector *)firstObject;
        secondCrlSelector = (PKIX_CRLSelector *)secondObject;

        /*
         * Since we know firstObject is a CRLSelector, if both references are
         * identical, they must be equal
         */
        if (firstCrlSelector == secondCrlSelector){
                *pResult = PKIX_TRUE;
                goto cleanup;
        }

        /*
         * If secondCRLSelector isn't a CRLSelector, we don't throw an error.
         * We simply return a Boolean result of FALSE
         */
        *pResult = PKIX_FALSE;
        PKIX_CHECK(PKIX_PL_Object_GetType
                    ((PKIX_PL_Object *)secondCrlSelector,
                    &secondType,
                    plContext),
                    PKIX_COULDNOTGETTYPEOFSECONDARGUMENT);

        if (secondType != PKIX_CRLSELECTOR_TYPE) {
                goto cleanup;
        }

        /* Compare MatchCallback address */
        cmpResult = (firstCrlSelector->matchCallback ==
                    secondCrlSelector->matchCallback);

        if (cmpResult == PKIX_FALSE) {
                goto cleanup;
        }

        /* Compare Common CRL Selector Params */
        PKIX_EQUALS
                (firstCrlSelector->params,
                secondCrlSelector->params,
                &cmpResult,
                plContext,
                PKIX_COMCRLSELPARAMSEQUALSFAILED);


        if (cmpResult == PKIX_FALSE) {
                goto cleanup;
        }

        /* Compare Context */
        PKIX_EQUALS
                (firstCrlSelector->context,
                secondCrlSelector->context,
                &cmpResult,
                plContext,
                PKIX_COMCRLSELPARAMSEQUALSFAILED);

        *pResult = cmpResult;

cleanup:

        PKIX_RETURN(CRLSELECTOR);
}

/*
 * FUNCTION: pkix_CRLSelector_Duplicate
 * (see comments for PKIX_PL_Duplicate_Callback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_CRLSelector_Duplicate(
        PKIX_PL_Object *object,
        PKIX_PL_Object **pNewObject,
        void *plContext)
{
        PKIX_CRLSelector *old;
        PKIX_CRLSelector *new = NULL;

        PKIX_ENTER(CRLSELECTOR, "pkix_CRLSelector_Duplicate");
        PKIX_NULLCHECK_TWO(object, pNewObject);

        PKIX_CHECK(pkix_CheckType
                    (object, PKIX_CRLSELECTOR_TYPE, plContext),
                    PKIX_OBJECTNOTCRLSELECTOR);

        old = (PKIX_CRLSelector *)object;

        PKIX_CHECK(PKIX_PL_Object_Alloc
                    (PKIX_CRLSELECTOR_TYPE,
                    (PKIX_UInt32)(sizeof (PKIX_CRLSelector)),
                    (PKIX_PL_Object **)&new,
                    plContext),
                    PKIX_CREATECRLSELECTORDUPLICATEOBJECTFAILED);

        new->matchCallback = old->matchCallback;

        PKIX_DUPLICATE(old->params, &new->params, plContext,
                    PKIX_OBJECTDUPLICATEPARAMSFAILED);

        PKIX_DUPLICATE(old->context, &new->context, plContext,
                PKIX_OBJECTDUPLICATECONTEXTFAILED);

        *pNewObject = (PKIX_PL_Object *)new;

cleanup:

        if (PKIX_ERROR_RECEIVED){
                PKIX_DECREF(new);
        }

        PKIX_RETURN(CRLSELECTOR);
}

/*
 * FUNCTION: pkix_CRLSelector_DefaultMatch
 *
 * DESCRIPTION:
 *  This function compares the parameter values (Issuer, date, and CRL number)
 *  set in the ComCRLSelParams of the CRLSelector pointed to by "selector" with
 *  the corresponding values in the CRL pointed to by "crl". When all the
 *  criteria set in the parameter values match the values in "crl", PKIX_TRUE is
 *  stored at "pMatch". If the CRL does not match the CRLSelector's criteria,
 *  PKIX_FALSE is stored at "pMatch".
 *
 * PARAMETERS
 *  "selector"
 *      Address of CRLSelector which is verified for a match
 *      Must be non-NULL.
 *  "crl"
 *      Address of the CRL object to be verified. Must be non-NULL.
 *  "pMatch"
 *      Address at which Boolean result is stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 *
 * THREAD SAFETY:
 *  Conditionally Thread Safe
 *      (see Thread Safety Definitions in Programmer's Guide)
 *
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a CRLSelector Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_CRLSelector_DefaultMatch(
        PKIX_CRLSelector *selector,
        PKIX_PL_CRL *crl,
        PKIX_Boolean *pMatch,
        void *plContext)
{
        PKIX_ComCRLSelParams *params = NULL;
        PKIX_PL_X500Name *crlIssuerName = NULL;
        PKIX_PL_X500Name *issuerName = NULL;
        PKIX_List *selIssuerNames = NULL;
        PKIX_PL_Date *selDate = NULL;
        PKIX_Boolean result = PKIX_TRUE;
        PKIX_UInt32 numIssuers = 0;
        PKIX_UInt32 i;
        PKIX_PL_BigInt *minCRLNumber = NULL;
        PKIX_PL_BigInt *maxCRLNumber = NULL;
        PKIX_PL_BigInt *crlNumber = NULL;
        PKIX_Boolean nistPolicyEnabled = PKIX_FALSE;

        PKIX_ENTER(CRLSELECTOR, "pkix_CRLSelector_DefaultMatch");
        PKIX_NULLCHECK_TWO(selector, crl);

        *pMatch = PKIX_TRUE;
        params = selector->params;

        /* No matching parameter provided, just a match */
        if (params == NULL) {
                goto cleanup;
        }

        PKIX_CHECK(PKIX_ComCRLSelParams_GetIssuerNames
                    (params, &selIssuerNames, plContext),
                    PKIX_COMCRLSELPARAMSGETISSUERNAMESFAILED);

        /* Check for Issuers */
        if (selIssuerNames != NULL){

                result = PKIX_FALSE;

                PKIX_CHECK(PKIX_PL_CRL_GetIssuer
                            (crl, &crlIssuerName, plContext),
                            PKIX_CRLGETISSUERFAILED);

                PKIX_CHECK(PKIX_List_GetLength
                            (selIssuerNames, &numIssuers, plContext),
                            PKIX_LISTGETLENGTHFAILED);

                for (i = 0; i < numIssuers; i++){

                        PKIX_CHECK(PKIX_List_GetItem
                                    (selIssuerNames,
                                    i,
                                    (PKIX_PL_Object **)&issuerName,
                                    plContext),
                                    PKIX_LISTGETITEMFAILED);

                        PKIX_CHECK(PKIX_PL_X500Name_Match
                                    (crlIssuerName,
                                    issuerName,
                                    &result,
                                    plContext),
                                    PKIX_X500NAMEMATCHFAILED);

                        PKIX_DECREF(issuerName);

                        if (result == PKIX_TRUE) {
                                break;
                        }
                }

                if (result == PKIX_FALSE) {
                        PKIX_CRLSELECTOR_DEBUG("Issuer Match Failed\N");
                        *pMatch = PKIX_FALSE;
                        goto cleanup;
                }

        }

        PKIX_CHECK(PKIX_ComCRLSelParams_GetDateAndTime
                    (params, &selDate, plContext),
                    PKIX_COMCRLSELPARAMSGETDATEANDTIMEFAILED);

        /* Check for Date */
        if (selDate != NULL){

                PKIX_CHECK(PKIX_ComCRLSelParams_GetNISTPolicyEnabled
                            (params, &nistPolicyEnabled, plContext),
                           PKIX_COMCRLSELPARAMSGETNISTPOLICYENABLEDFAILED);

                /* check crl dates only for if NIST policies enforced */
                if (nistPolicyEnabled) {
                        result = PKIX_FALSE;
                    
                        PKIX_CHECK(PKIX_PL_CRL_VerifyUpdateTime
                                   (crl, selDate, &result, plContext),
                                   PKIX_CRLVERIFYUPDATETIMEFAILED);
                    
                        if (result == PKIX_FALSE) {
                                *pMatch = PKIX_FALSE;
                                goto cleanup;
                        }
                }

        }

        /* Check for CRL number in range */
        PKIX_CHECK(PKIX_PL_CRL_GetCRLNumber(crl, &crlNumber, plContext),
                    PKIX_CRLGETCRLNUMBERFAILED);

        if (crlNumber != NULL) {
                result = PKIX_FALSE;

                PKIX_CHECK(PKIX_ComCRLSelParams_GetMinCRLNumber
                            (params, &minCRLNumber, plContext),
                            PKIX_COMCRLSELPARAMSGETMINCRLNUMBERFAILED);

                if (minCRLNumber != NULL) {

                        PKIX_CHECK(PKIX_PL_Object_Compare
                                    ((PKIX_PL_Object *)minCRLNumber,
                                    (PKIX_PL_Object *)crlNumber,
                                    &result,
                                    plContext),
                                    PKIX_OBJECTCOMPARATORFAILED);

                        if (result == 1) {
                                PKIX_CRLSELECTOR_DEBUG
					("CRL MinNumber Range Match Failed\n");
                        	*pMatch = PKIX_FALSE;
	                        goto cleanup;
                        }
                }

                PKIX_CHECK(PKIX_ComCRLSelParams_GetMaxCRLNumber
                            (params, &maxCRLNumber, plContext),
                            PKIX_COMCRLSELPARAMSGETMAXCRLNUMBERFAILED);

                if (maxCRLNumber != NULL) {

                        PKIX_CHECK(PKIX_PL_Object_Compare
                                    ((PKIX_PL_Object *)crlNumber,
                                    (PKIX_PL_Object *)maxCRLNumber,
                                    &result,
                                    plContext),
                                    PKIX_OBJECTCOMPARATORFAILED);

                        if (result == 1) {
                               PKIX_CRLSELECTOR_DEBUG 
					(PKIX_CRLMAXNUMBERRANGEMATCHFAILED);
                        	*pMatch = PKIX_FALSE;
	                        goto cleanup;
                        }
                }
        }

cleanup:

        PKIX_DECREF(selIssuerNames);
        PKIX_DECREF(selDate);
        PKIX_DECREF(crlIssuerName);
        PKIX_DECREF(issuerName);
        PKIX_DECREF(crlNumber);
        PKIX_DECREF(minCRLNumber);
        PKIX_DECREF(maxCRLNumber);

        PKIX_RETURN(CRLSELECTOR);
}

/*
 * FUNCTION: pkix_CRLSelector_RegisterSelf
 * DESCRIPTION:
 *  Registers PKIX_CRLSELECTOR_TYPE and its related functions with
 *  systemClasses[]
 * THREAD SAFETY:
 *  Not Thread Safe - for performance and complexity reasons
 *
 *  Since this function is only called by PKIX_PL_Initialize, which should
 *  only be called once, it is acceptable that this function is not
 *  thread-safe.
 */
PKIX_Error *
pkix_CRLSelector_RegisterSelf(void *plContext)
{
        extern pkix_ClassTable_Entry systemClasses[PKIX_NUMTYPES];
        pkix_ClassTable_Entry entry;

        PKIX_ENTER(CRLSELECTOR, "pkix_CRLSelector_RegisterSelf");

        entry.description = "CRLSelector";
        entry.objCounter = 0;
        entry.typeObjectSize = sizeof(PKIX_CRLSelector);
        entry.destructor = pkix_CRLSelector_Destroy;
        entry.equalsFunction = pkix_CRLSelector_Equals;
        entry.hashcodeFunction = pkix_CRLSelector_Hashcode;
        entry.toStringFunction = pkix_CRLSelector_ToString;
        entry.comparator = NULL;
        entry.duplicateFunction = pkix_CRLSelector_Duplicate;

        systemClasses[PKIX_CRLSELECTOR_TYPE] = entry;

        PKIX_RETURN(CRLSELECTOR);
}

/* --CRLSelector-Public-Functions---------------------------------------- */
PKIX_Error *
pkix_CRLSelector_Create(
        PKIX_CRLSelector_MatchCallback callback,
        PKIX_PL_Object *crlSelectorContext,
        PKIX_CRLSelector **pSelector,
        void *plContext)
{
        PKIX_CRLSelector *selector = NULL;

        PKIX_ENTER(CRLSELECTOR, "PKIX_CRLSelector_Create");
        PKIX_NULLCHECK_ONE(pSelector);

        PKIX_CHECK(PKIX_PL_Object_Alloc
                    (PKIX_CRLSELECTOR_TYPE,
                    sizeof (PKIX_CRLSelector),
                    (PKIX_PL_Object **)&selector,
                    plContext),
                    PKIX_COULDNOTCREATECRLSELECTOROBJECT);

        /*
         * if user specified a particular match callback, we use that one.
         * otherwise, we use the default match provided.
         */

        if (callback != NULL){
                selector->matchCallback = callback;
        } else {
                selector->matchCallback = pkix_CRLSelector_DefaultMatch;
        }

        /* initialize other fields */
        selector->params = NULL;

        PKIX_INCREF(crlSelectorContext);
        selector->context = crlSelectorContext;

        *pSelector = selector;
        selector = NULL;

cleanup:

        PKIX_DECREF(selector);

        PKIX_RETURN(CRLSELECTOR);
}

/*
 * FUNCTION: PKIX_CRLSelector_Create (see comments in pkix_crlsel.h)
 */
PKIX_Error *
PKIX_CRLSelector_Create(
        PKIX_PL_Cert *issuer,
        PKIX_List *crldpList,
        PKIX_PL_Date *date,
        PKIX_CRLSelector **pCrlSelector,
        void *plContext)
{
    PKIX_PL_X500Name *issuerName = NULL;
    PKIX_PL_Date *nowDate = NULL;
    PKIX_ComCRLSelParams *comCrlSelParams = NULL;
    PKIX_CRLSelector *crlSelector = NULL;

    PKIX_ENTER(CERTCHAINCHECKER, "PKIX_CrlSelector_Create");
    PKIX_NULLCHECK_ONE(issuer);

    PKIX_CHECK( 
        PKIX_PL_Cert_GetSubject(issuer, &issuerName, plContext),
        PKIX_CERTGETISSUERFAILED);

    if (date != NULL) {
            PKIX_INCREF(date);
            nowDate = date;
    } else {
        PKIX_CHECK(
                PKIX_PL_Date_Create_UTCTime(NULL, &nowDate, plContext),
                PKIX_DATECREATEUTCTIMEFAILED);
    }

    PKIX_CHECK(
        PKIX_ComCRLSelParams_Create(&comCrlSelParams, plContext),
            PKIX_COMCRLSELPARAMSCREATEFAILED);

    PKIX_CHECK(
        PKIX_ComCRLSelParams_AddIssuerName(comCrlSelParams, issuerName,
                                           plContext),
        PKIX_COMCRLSELPARAMSADDISSUERNAMEFAILED);

    PKIX_CHECK(
        PKIX_ComCRLSelParams_SetCrlDp(comCrlSelParams, crldpList,
                                      plContext),
        PKIX_COMCRLSELPARAMSSETCERTFAILED);

    PKIX_CHECK(
        PKIX_ComCRLSelParams_SetDateAndTime(comCrlSelParams, nowDate,
                                            plContext),
        PKIX_COMCRLSELPARAMSSETDATEANDTIMEFAILED);

    PKIX_CHECK(
        pkix_CRLSelector_Create(NULL, NULL, &crlSelector, plContext),
        PKIX_CRLSELECTORCREATEFAILED);

    PKIX_CHECK(
        PKIX_CRLSelector_SetCommonCRLSelectorParams(crlSelector,
                                                    comCrlSelParams,
                                                    plContext),
        PKIX_CRLSELECTORSETCOMMONCRLSELECTORPARAMSFAILED);

    *pCrlSelector = crlSelector;
    crlSelector = NULL;

cleanup:

    PKIX_DECREF(issuerName);
    PKIX_DECREF(nowDate);
    PKIX_DECREF(comCrlSelParams);
    PKIX_DECREF(crlSelector);

    PKIX_RETURN(CERTCHAINCHECKER);
}

/*
 * FUNCTION: PKIX_CRLSelector_GetMatchCallback (see comments in pkix_crlsel.h)
 */
PKIX_Error *
PKIX_CRLSelector_GetMatchCallback(
        PKIX_CRLSelector *selector,
        PKIX_CRLSelector_MatchCallback *pCallback,
        void *plContext)
{
        PKIX_ENTER(CRLSELECTOR, "PKIX_CRLSelector_GetMatchCallback");
        PKIX_NULLCHECK_TWO(selector, pCallback);

        *pCallback = selector->matchCallback;

        PKIX_RETURN(CRLSELECTOR);
}


/*
 * FUNCTION: PKIX_CRLSelector_GetCRLSelectorContext
 * (see comments in pkix_crlsel.h)
 */
PKIX_Error *
PKIX_CRLSelector_GetCRLSelectorContext(
        PKIX_CRLSelector *selector,
        void **pCrlSelectorContext,
        void *plContext)
{
        PKIX_ENTER(CRLSELECTOR, "PKIX_CRLSelector_GetCRLSelectorContext");
        PKIX_NULLCHECK_TWO(selector, pCrlSelectorContext);

        PKIX_INCREF(selector->context);

        *pCrlSelectorContext = selector->context;

cleanup:
        PKIX_RETURN(CRLSELECTOR);
}

/*
 * FUNCTION: PKIX_CRLSelector_GetCommonCRLSelectorParams
 * (see comments in pkix_crlsel.h)
 */
PKIX_Error *
PKIX_CRLSelector_GetCommonCRLSelectorParams(
        PKIX_CRLSelector *selector,
        PKIX_ComCRLSelParams **pParams,
        void *plContext)
{
        PKIX_ENTER(CRLSELECTOR, "PKIX_CRLSelector_GetCommonCRLSelectorParams");
        PKIX_NULLCHECK_TWO(selector, pParams);

        PKIX_INCREF(selector->params);

        *pParams = selector->params;

cleanup:
        PKIX_RETURN(CRLSELECTOR);
}

/*
 * FUNCTION: PKIX_CRLSelector_SetCommonCRLSelectorParams
 * (see comments in pkix_crlsel.h)
 */
PKIX_Error *
PKIX_CRLSelector_SetCommonCRLSelectorParams(
        PKIX_CRLSelector *selector,
        PKIX_ComCRLSelParams *params,
        void *plContext)
{
        PKIX_ENTER(CRLSELECTOR, "PKIX_CRLSelector_SetCommonCRLSelectorParams");
        PKIX_NULLCHECK_TWO(selector, params);

        PKIX_DECREF(selector->params);

        PKIX_INCREF(params);
        selector->params = params;

        PKIX_CHECK(PKIX_PL_Object_InvalidateCache
                    ((PKIX_PL_Object *)selector, plContext),
                    PKIX_OBJECTINVALIDATECACHEFAILED);

cleanup:

        PKIX_RETURN(CRLSELECTOR);
}

/*
 * FUNCTION: pkix_CRLSelector_Select
 * DESCRIPTION:
 *
 *  This function applies the selector pointed to by "selector" to each CRL,
 *  in turn, in the List pointed to by "before", and creates a List containing
 *  all the CRLs that matched, or passed the selection process, storing that
 *  List at "pAfter". If no CRLs match, an empty List is stored at "pAfter".
 *
 *  The List returned in "pAfter" is immutable.
 *
 * PARAMETERS:
 *  "selector"
 *      Address of CRLSelelector to be applied to the List. Must be non-NULL.
 *  "before"
 *      Address of List that is to be filtered. Must be non-NULL.
 *  "pAfter"
 *      Address at which resulting List, possibly empty, is stored. Must be
 *      non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a CRLSelector Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_CRLSelector_Select(
	PKIX_CRLSelector *selector,
	PKIX_List *before,
	PKIX_List **pAfter,
	void *plContext)
{
	PKIX_Boolean match = PKIX_FALSE;
	PKIX_UInt32 numBefore = 0;
	PKIX_UInt32 i = 0;
	PKIX_List *filtered = NULL;
	PKIX_PL_CRL *candidate = NULL;

        PKIX_ENTER(CRLSELECTOR, "PKIX_CRLSelector_Select");
        PKIX_NULLCHECK_THREE(selector, before, pAfter);

        PKIX_CHECK(PKIX_List_Create(&filtered, plContext),
                PKIX_LISTCREATEFAILED);

        PKIX_CHECK(PKIX_List_GetLength(before, &numBefore, plContext),
                PKIX_LISTGETLENGTHFAILED);

        for (i = 0; i < numBefore; i++) {

                PKIX_CHECK(PKIX_List_GetItem
                        (before, i, (PKIX_PL_Object **)&candidate, plContext),
                        PKIX_LISTGETITEMFAILED);

                PKIX_CHECK_ONLY_FATAL(selector->matchCallback
                        (selector, candidate, &match, plContext),
                        PKIX_CRLSELECTORMATCHCALLBACKFAILED);

                if (!(PKIX_ERROR_RECEIVED) && match == PKIX_TRUE) {

                        PKIX_CHECK_ONLY_FATAL(PKIX_List_AppendItem
                                (filtered,
                                (PKIX_PL_Object *)candidate,
                                plContext),
                                PKIX_LISTAPPENDITEMFAILED);
                }

                pkixTempErrorReceived = PKIX_FALSE;
                PKIX_DECREF(candidate);
        }

        PKIX_CHECK(PKIX_List_SetImmutable(filtered, plContext),
                PKIX_LISTSETIMMUTABLEFAILED);

        /* Don't throw away the list if one CRL was bad! */
        pkixTempErrorReceived = PKIX_FALSE;

        *pAfter = filtered;
        filtered = NULL;

cleanup:

        PKIX_DECREF(filtered);
        PKIX_DECREF(candidate);

        PKIX_RETURN(CRLSELECTOR);

}
