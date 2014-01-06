/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * pkix_ocspchecker.c
 *
 * OcspChecker Object Functions
 *
 */

#include "pkix_ocspchecker.h"
#include "pkix_pl_ocspcertid.h"
#include "pkix_error.h"


/* --Private-Data-and-Types--------------------------------------- */

typedef struct pkix_OcspCheckerStruct {
    /* RevocationMethod is the super class of OcspChecker. */
    pkix_RevocationMethod method;
    PKIX_PL_VerifyCallback certVerifyFcn;
} pkix_OcspChecker;

/* --Private-Functions-------------------------------------------- */

/*
 * FUNCTION: pkix_OcspChecker_Destroy
 *      (see comments for PKIX_PL_DestructorCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_OcspChecker_Destroy(
        PKIX_PL_Object *object,
        void *plContext)
{
    return NULL;
}

/*
 * FUNCTION: pkix_OcspChecker_RegisterSelf
 * DESCRIPTION:
 *  Registers PKIX_OCSPCHECKER_TYPE and its related functions with
 *  systemClasses[]
 * THREAD SAFETY:
 *  Not Thread Safe - for performance and complexity reasons
 *
 *  Since this function is only called by PKIX_PL_Initialize, which should
 *  only be called once, it is acceptable that this function is not
 *  thread-safe.
 */
PKIX_Error *
pkix_OcspChecker_RegisterSelf(void *plContext)
{
        extern pkix_ClassTable_Entry systemClasses[PKIX_NUMTYPES];
        pkix_ClassTable_Entry* entry = &systemClasses[PKIX_OCSPCHECKER_TYPE];

        PKIX_ENTER(OCSPCHECKER, "pkix_OcspChecker_RegisterSelf");

        entry->description = "OcspChecker";
        entry->typeObjectSize = sizeof(pkix_OcspChecker);
        entry->destructor = pkix_OcspChecker_Destroy;

        PKIX_RETURN(OCSPCHECKER);
}


/*
 * FUNCTION: pkix_OcspChecker_Create
 */
PKIX_Error *
pkix_OcspChecker_Create(PKIX_RevocationMethodType methodType,
                        PKIX_UInt32 flags,
                        PKIX_UInt32 priority,
                        pkix_LocalRevocationCheckFn localRevChecker,
                        pkix_ExternalRevocationCheckFn externalRevChecker,
                        PKIX_PL_VerifyCallback verifyFn,
                        pkix_RevocationMethod **pChecker,
                        void *plContext)
{
        pkix_OcspChecker *method = NULL;

        PKIX_ENTER(OCSPCHECKER, "pkix_OcspChecker_Create");
        PKIX_NULLCHECK_ONE(pChecker);

        PKIX_CHECK(PKIX_PL_Object_Alloc
                    (PKIX_OCSPCHECKER_TYPE,
                    sizeof (pkix_OcspChecker),
                    (PKIX_PL_Object **)&method,
                    plContext),
                    PKIX_COULDNOTCREATECERTCHAINCHECKEROBJECT);

        pkixErrorResult = pkix_RevocationMethod_Init(
            (pkix_RevocationMethod*)method, methodType, flags,  priority,
            localRevChecker, externalRevChecker, plContext);
        if (pkixErrorResult) {
            goto cleanup;
        }
        method->certVerifyFcn = (PKIX_PL_VerifyCallback)verifyFn;

        *pChecker = (pkix_RevocationMethod*)method;
        method = NULL;

cleanup:
        PKIX_DECREF(method);

        PKIX_RETURN(OCSPCHECKER);
}

/*
 * FUNCTION: pkix_OcspChecker_MapResultCodeToRevStatus
 */
PKIX_RevocationStatus
pkix_OcspChecker_MapResultCodeToRevStatus(SECErrorCodes resultCode)
{
        switch (resultCode) {
            case SEC_ERROR_REVOKED_CERTIFICATE:
                return PKIX_RevStatus_Revoked;
            default:
                return PKIX_RevStatus_NoInfo;
        }
}

/* --Public-Functions--------------------------------------------- */

/*
 * FUNCTION: pkix_OcspChecker_Check (see comments in pkix_checker.h)
 */

/*
 * The OCSPChecker is created in an idle state, and remains in this state until
 * either (a) the default Responder has been set and enabled, and a Check
 * request is received with no responder specified, or (b) a Check request is
 * received with a specified responder. A request message is constructed and
 * given to the HttpClient. If non-blocking I/O is used the client may return
 * with WOULDBLOCK, in which case the OCSPChecker returns the WOULDBLOCK
 * condition to its caller in turn. On a subsequent call the I/O is resumed.
 * When a response is received it is decoded and the results provided to the
 * caller.
 *
 */
PKIX_Error *
pkix_OcspChecker_CheckLocal(
        PKIX_PL_Cert *cert,
        PKIX_PL_Cert *issuer,
        PKIX_PL_Date *date,
        pkix_RevocationMethod *checkerObject,
        PKIX_ProcessingParams *procParams,
        PKIX_UInt32 methodFlags,
        PKIX_Boolean chainVerificationState,
        PKIX_RevocationStatus *pRevStatus,
        PKIX_UInt32 *pReasonCode,
        void *plContext)
{
        PKIX_PL_OcspCertID    *cid = NULL;
        PKIX_Boolean           hasFreshStatus = PKIX_FALSE;
        PKIX_Boolean           statusIsGood = PKIX_FALSE;
        SECErrorCodes          resultCode = SEC_ERROR_REVOKED_CERTIFICATE_OCSP;
        PKIX_RevocationStatus  revStatus = PKIX_RevStatus_NoInfo;

        PKIX_ENTER(OCSPCHECKER, "pkix_OcspChecker_CheckLocal");

        PKIX_CHECK(
            PKIX_PL_OcspCertID_Create(cert, NULL, &cid,
                                      plContext),
            PKIX_OCSPCERTIDCREATEFAILED);
        if (!cid) {
            goto cleanup;
        }

        PKIX_CHECK(
            PKIX_PL_OcspCertID_GetFreshCacheStatus(cid, date,
                                                   &hasFreshStatus,
                                                   &statusIsGood,
                                                   &resultCode,
                                                   plContext),
            PKIX_OCSPCERTIDGETFRESHCACHESTATUSFAILED);
        if (hasFreshStatus) {
            if (statusIsGood) {
                revStatus = PKIX_RevStatus_Success;
                resultCode = 0;
            } else {
                revStatus = pkix_OcspChecker_MapResultCodeToRevStatus(resultCode);
            }
        }

cleanup:
        *pRevStatus = revStatus;

        /* ocsp carries only tree statuses: good, bad, and unknown.
         * revStatus is used to pass them. reasonCode is always set
         * to be unknown. */
        *pReasonCode = crlEntryReasonUnspecified;
        PKIX_DECREF(cid);

        PKIX_RETURN(OCSPCHECKER);
}


/*
 * The OCSPChecker is created in an idle state, and remains in this state until
 * either (a) the default Responder has been set and enabled, and a Check
 * request is received with no responder specified, or (b) a Check request is
 * received with a specified responder. A request message is constructed and
 * given to the HttpClient. When a response is received it is decoded and the
 * results provided to the caller.
 *
 * During the most recent enhancement of this function, it has been found that
 * it doesn't correctly implement non-blocking I/O.
 * 
 * The nbioContext is used in two places, for "response-obtaining" and
 * for "response-verification".
 * 
 * However, if this function gets called to resume, it always
 * repeats the "request creation" and "response fetching" steps!
 * As a result, the earlier operation is never resumed.
 */
PKIX_Error *
pkix_OcspChecker_CheckExternal(
        PKIX_PL_Cert *cert,
        PKIX_PL_Cert *issuer,
        PKIX_PL_Date *date,
        pkix_RevocationMethod *checkerObject,
        PKIX_ProcessingParams *procParams,
        PKIX_UInt32 methodFlags,
        PKIX_RevocationStatus *pRevStatus,
        PKIX_UInt32 *pReasonCode,
        void **pNBIOContext,
        void *plContext)
{
        SECErrorCodes resultCode = SEC_ERROR_REVOKED_CERTIFICATE_OCSP;
        PKIX_Boolean uriFound = PKIX_FALSE;
        PKIX_Boolean passed = PKIX_TRUE;
        pkix_OcspChecker *checker = NULL;
        PKIX_PL_OcspCertID *cid = NULL;
        PKIX_PL_OcspRequest *request = NULL;
        PKIX_PL_OcspResponse *response = NULL;
        PKIX_PL_Date *validity = NULL;
        PKIX_RevocationStatus revStatus = PKIX_RevStatus_NoInfo;
        void *nbioContext = NULL;
        enum { stageGET, stagePOST } currentStage;
        PRBool retry = PR_FALSE;

        PKIX_ENTER(OCSPCHECKER, "pkix_OcspChecker_CheckExternal");

        PKIX_CHECK(
            pkix_CheckType((PKIX_PL_Object*)checkerObject,
                           PKIX_OCSPCHECKER_TYPE, plContext),
                PKIX_OBJECTNOTOCSPCHECKER);

        checker = (pkix_OcspChecker *)checkerObject;

        PKIX_CHECK(
            PKIX_PL_OcspCertID_Create(cert, NULL, &cid,
                                      plContext),
            PKIX_OCSPCERTIDCREATEFAILED);
        
        /* create request */
        PKIX_CHECK(
            pkix_pl_OcspRequest_Create(cert, cid, validity, NULL, 
                                       methodFlags, &uriFound, &request,
                                       plContext),
            PKIX_OCSPREQUESTCREATEFAILED);
        
        if (uriFound == PKIX_FALSE) {
            /* no caching for certs lacking URI */
            resultCode = 0;
            goto cleanup;
        }

        if (methodFlags & CERT_REV_M_FORCE_POST_METHOD_FOR_OCSP) {
            /* Do not try HTTP GET, only HTTP POST */
            currentStage = stagePOST;
        } else {
            /* Try HTTP GET first, falling back to POST */
            currentStage = stageGET;
        }

        do {
                const char *method;
                passed = PKIX_TRUE;

                retry = PR_FALSE;
                if (currentStage == stageGET) {
                        method = "GET";
                } else {
                        PORT_Assert(currentStage == stagePOST);
                        method = "POST";
                }

                /* send request and create a response object */
                PKIX_CHECK_NO_GOTO(
                    pkix_pl_OcspResponse_Create(request, method, NULL,
                                                checker->certVerifyFcn,
                                                &nbioContext,
                                                &response,
                                                plContext),
                    PKIX_OCSPRESPONSECREATEFAILED);

                if (pkixErrorResult) {
                    passed = PKIX_FALSE;
                }

                if (passed && nbioContext != 0) {
                        *pNBIOContext = nbioContext;
                        goto cleanup;
                }

                if (passed){
                        PKIX_CHECK_NO_GOTO(
                            pkix_pl_OcspResponse_Decode(response, &passed,
                                                        &resultCode, plContext),
                            PKIX_OCSPRESPONSEDECODEFAILED);
                        if (pkixErrorResult) {
                            passed = PKIX_FALSE;
                        }
                }
                
                if (passed){
                        PKIX_CHECK_NO_GOTO(
                            pkix_pl_OcspResponse_GetStatus(response, &passed,
                                                           &resultCode, plContext),
                            PKIX_OCSPRESPONSEGETSTATUSRETURNEDANERROR);
                        if (pkixErrorResult) {
                            passed = PKIX_FALSE;
                        }
                }

                if (passed){
                        PKIX_CHECK_NO_GOTO(
                            pkix_pl_OcspResponse_VerifySignature(response, cert,
                                                                 procParams, &passed, 
                                                                 &nbioContext, plContext),
                            PKIX_OCSPRESPONSEVERIFYSIGNATUREFAILED);
                        if (pkixErrorResult) {
                            passed = PKIX_FALSE;
                        } else {
                                if (nbioContext != 0) {
                                        *pNBIOContext = nbioContext;
                                        goto cleanup;
                                }
                        }
                }

                if (!passed && currentStage == stagePOST) {
                        /* We won't retry a POST failure, so it's final.
                         * Because the following block with its call to
                         *   pkix_pl_OcspResponse_GetStatusForCert
                         * will take care of caching good or bad state,
                         * but we only execute that next block if there hasn't
                         * been a failure yet, we must cache the POST
                         * failure now.
                         */
                         
                        if (cid && cid->certID) {
                                /* Caching MIGHT consume the cid. */
                                PKIX_Error *err;
                                err = PKIX_PL_OcspCertID_RememberOCSPProcessingFailure(
                                        cid, plContext);
                                if (err) {
                                        PKIX_PL_Object_DecRef((PKIX_PL_Object*)err, plContext);
                                }
                        }
                }

                if (passed){
                        PKIX_Boolean allowCachingOfFailures =
                                (currentStage == stagePOST) ? PKIX_TRUE : PKIX_FALSE;
                        
                        PKIX_CHECK_NO_GOTO(
                            pkix_pl_OcspResponse_GetStatusForCert(cid, response,
                                                                  allowCachingOfFailures,
                                                                  date,
                                                                  &passed, &resultCode,
                                                                  plContext),
                            PKIX_OCSPRESPONSEGETSTATUSFORCERTFAILED);
                        if (pkixErrorResult) {
                            passed = PKIX_FALSE;
                        } else if (passed == PKIX_FALSE) {
                                revStatus = pkix_OcspChecker_MapResultCodeToRevStatus(resultCode);
                        } else {
                                revStatus = PKIX_RevStatus_Success;
                        }
                }

                if (currentStage == stageGET && revStatus != PKIX_RevStatus_Success &&
                                                revStatus != PKIX_RevStatus_Revoked) {
                        /* we'll retry */
                        PKIX_DECREF(response);
                        retry = PR_TRUE;
                        currentStage = stagePOST;
                        revStatus = PKIX_RevStatus_NoInfo;
                        if (pkixErrorResult) {
                                PKIX_PL_Object_DecRef((PKIX_PL_Object*)pkixErrorResult,
                                                      plContext);
                                pkixErrorResult = NULL;
                        }
                }
        } while (retry);

cleanup:
        if (revStatus == PKIX_RevStatus_NoInfo && (uriFound || 
	    methodFlags & PKIX_REV_M_REQUIRE_INFO_ON_MISSING_SOURCE) &&
            methodFlags & PKIX_REV_M_FAIL_ON_MISSING_FRESH_INFO) {
            revStatus = PKIX_RevStatus_Revoked;
        }
        *pRevStatus = revStatus;

        /* ocsp carries only three statuses: good, bad, and unknown.
         * revStatus is used to pass them. reasonCode is always set
         * to be unknown. */
        *pReasonCode = crlEntryReasonUnspecified;

        PKIX_DECREF(cid);
        PKIX_DECREF(request);
        PKIX_DECREF(response);

        PKIX_RETURN(OCSPCHECKER);
}


