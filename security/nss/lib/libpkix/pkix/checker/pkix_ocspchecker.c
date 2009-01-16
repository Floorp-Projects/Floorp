/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the PKIX-C library.
 *
 * The Initial Developer of the Original Code is
 * Sun Microsystems, Inc.
 * Portions created by the Initial Developer are
 * Copyright 2004-2007 Sun Microsystems, Inc.  All Rights Reserved.
 *
 * Contributor(s):
 *   Sun Microsystems, Inc.
 *   Red Hat, Inc.
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
/*
 * pkix_ocspchecker.c
 *
 * OcspChecker Object Functions
 *
 */

#include "pkix_ocspchecker.h"
#include "pkix_pl_ocspcertid.h"
#include "pkix_error.h"


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
        PKIX_OcspChecker *checker = NULL;

        PKIX_ENTER(OCSPCHECKER, "pkix_OcspChecker_Destroy");
        PKIX_NULLCHECK_ONE(object);

        /* Check that this object is a ocsp checker */
        PKIX_CHECK(pkix_CheckType
                    (object, PKIX_OCSPCHECKER_TYPE, plContext),
                    PKIX_OBJECTNOTOCSPCHECKER);

        checker = (PKIX_OcspChecker *)object;

	PKIX_DECREF(checker->response);
        PKIX_DECREF(checker->validityTime);
        PKIX_DECREF(checker->cert);

        /* These are not yet ref-counted objects */
        /* PKIX_DECREF(checker->passwordInfo); */
        /* PKIX_DECREF(checker->responder); */
        /* PKIX_DECREF(checker->nbioContext); */

cleanup:

        PKIX_RETURN(OCSPCHECKER);
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
        pkix_ClassTable_Entry entry;

        PKIX_ENTER(OCSPCHECKER, "pkix_OcspChecker_RegisterSelf");

        entry.description = "OcspChecker";
        entry.objCounter = 0;
        entry.typeObjectSize = sizeof(PKIX_OcspChecker);
        entry.destructor = pkix_OcspChecker_Destroy;
        entry.equalsFunction = NULL;
        entry.hashcodeFunction = NULL;
        entry.toStringFunction = NULL;
        entry.comparator = NULL;
        entry.duplicateFunction = NULL;

        systemClasses[PKIX_OCSPCHECKER_TYPE] = entry;

        PKIX_RETURN(OCSPCHECKER);
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
static PKIX_Error *
pkix_OcspChecker_Check(
        PKIX_PL_Object *checkerObject,
        PKIX_PL_Cert *cert,
        PKIX_ProcessingParams *procParams,
        void **pNBIOContext,
        PKIX_UInt32 *pResultCode,
        void *plContext)
{
        SECErrorCodes resultCode = SEC_ERROR_REVOKED_CERTIFICATE_OCSP;
        PKIX_Boolean uriFound = PKIX_FALSE;
        PKIX_Boolean passed = PKIX_FALSE;
        PKIX_OcspChecker *checker = NULL;
        PKIX_PL_OcspCertID *cid = NULL;
        PKIX_PL_OcspRequest *request = NULL;
        PKIX_PL_Date *validity = NULL;
        void *nbioContext = NULL;

        PKIX_ENTER(OCSPCHECKER, "pkix_OcspChecker_Check");
        PKIX_NULLCHECK_FOUR(checkerObject, cert, pNBIOContext, pResultCode);

        PKIX_CHECK(pkix_CheckType
                (checkerObject, PKIX_OCSPCHECKER_TYPE, plContext),
                PKIX_OBJECTNOTOCSPCHECKER);

        checker = (PKIX_OcspChecker *)checkerObject;

        nbioContext = *pNBIOContext;
        *pNBIOContext = 0;

        /* assert(checker->nbioContext == nbioContext) */

        if (nbioContext == 0) {
                /* We are initiating a check, not resuming previous I/O. */

                PKIX_Boolean hasFreshStatus = PKIX_FALSE;
                PKIX_Boolean statusIsGood = PKIX_FALSE;

                PKIX_CHECK(PKIX_PL_OcspCertID_Create
                        (cert,
                        validity,
                        &cid,
                        plContext),
                        PKIX_OCSPCERTIDCREATEFAILED);

                if (!cid) {
                        goto cleanup;
                }

                PKIX_CHECK(PKIX_PL_OcspCertID_GetFreshCacheStatus
                        (cid, 
                        validity, 
                        &hasFreshStatus, 
                        &statusIsGood,
                        &resultCode,
                        plContext),
                        PKIX_OCSPCERTIDGETFRESHCACHESTATUSFAILED);

                if (hasFreshStatus) {
                        /* avoid updating the cache with a cached result... */
                        passed = PKIX_TRUE; 

                        if (statusIsGood) {
                                resultCode = 0;
                        }
                        goto cleanup;
                }
                PKIX_INCREF(cert);
                PKIX_DECREF(checker->cert);
                checker->cert = cert;

                /* create request */
                PKIX_CHECK(pkix_pl_OcspRequest_Create
                        (cert,
                        cid,
                        validity,
                        NULL,           /* PKIX_PL_Cert *signerCert */
                        &uriFound,
                        &request,
                        plContext),
                        PKIX_OCSPREQUESTCREATEFAILED);
                
                /* No uri to check is considered passing! */
                if (uriFound == PKIX_FALSE) {
                        /* no caching for certs lacking URI */
                        passed = PKIX_TRUE;
                        resultCode = 0;
                        goto cleanup;
                }

        }

        /* Do we already have a response object? */
        if ((checker->response) == NULL) {
        	/* send request and create a response object */
	        PKIX_CHECK(pkix_pl_OcspResponse_Create
	                (request,
	                checker->responder,
	                checker->verifyFcn,
	                &nbioContext,
	                &(checker->response),
	                plContext),
	                PKIX_OCSPRESPONSECREATEFAILED);

        	if (nbioContext != 0) {
                	*pNBIOContext = nbioContext;
	                goto cleanup;
	        }

	        PKIX_CHECK(pkix_pl_OcspResponse_Decode
        	        ((checker->response), &passed, &resultCode, plContext),
                	PKIX_OCSPRESPONSEDECODEFAILED);
                
	        if (passed == PKIX_FALSE) {
        	        goto cleanup;
	        }

        	PKIX_CHECK(pkix_pl_OcspResponse_GetStatus
                	((checker->response), &passed, &resultCode, plContext),
	                PKIX_OCSPRESPONSEGETSTATUSRETURNEDANERROR);
                
        	if (passed == PKIX_FALSE) {
                	goto cleanup;
	        }
	}

        PKIX_CHECK(pkix_pl_OcspResponse_VerifySignature
                ((checker->response),
                cert,
                procParams,
                &passed,
		&nbioContext,
                plContext),
                PKIX_OCSPRESPONSEVERIFYSIGNATUREFAILED);

       	if (nbioContext != 0) {
               	*pNBIOContext = nbioContext;
                goto cleanup;
        }

        if (passed == PKIX_FALSE) {
                resultCode = PORT_GetError();
                goto cleanup;
        }

        PKIX_CHECK(pkix_pl_OcspResponse_GetStatusForCert
                (cid, (checker->response), &passed, &resultCode, plContext),
                PKIX_OCSPRESPONSEGETSTATUSFORCERTFAILED);

cleanup:
        if (!passed && cid && cid->certID) {
                /* We still own the certID object, which means that 
                 * it did not get consumed to create a cache entry.
                 * Let's make sure there is one.
                 */
                PKIX_Error *err;
                err = PKIX_PL_OcspCertID_RememberOCSPProcessingFailure(
                        cid, plContext);
                if (err) {
                        PKIX_PL_Object_DecRef((PKIX_PL_Object*)err, plContext);
                }
        }

        *pResultCode = (PKIX_UInt32)resultCode;

        PKIX_DECREF(cid);
        PKIX_DECREF(request);
        if (checker) {
            PKIX_DECREF(checker->response);
        }

        PKIX_RETURN(OCSPCHECKER);

}

/*
 * FUNCTION: pkix_OcspChecker_Create
 */
PKIX_Error *
pkix_OcspChecker_Create(
        PKIX_PL_Date *validityTime,
        void *passwordInfo,
        void *responder,
        PKIX_OcspChecker **pChecker,
        void *plContext)
{
        PKIX_OcspChecker *checkerObject = NULL;

        PKIX_ENTER(OCSPCHECKER, "pkix_OcspChecker_Create");
        PKIX_NULLCHECK_ONE(pChecker);

        PKIX_CHECK(PKIX_PL_Object_Alloc
                    (PKIX_OCSPCHECKER_TYPE,
                    sizeof (PKIX_OcspChecker),
                    (PKIX_PL_Object **)&checkerObject,
                    plContext),
                    PKIX_COULDNOTCREATECERTCHAINCHECKEROBJECT);

        /* initialize fields */
        checkerObject->response = NULL;
        PKIX_INCREF(validityTime);
        checkerObject->validityTime = validityTime;
        checkerObject->clientIsDefault = PKIX_FALSE;
        checkerObject->verifyFcn = NULL;
        checkerObject->cert = NULL;

        /* These void*'s will need INCREFs if they become PKIX_PL_Objects */
        checkerObject->passwordInfo = passwordInfo;
        checkerObject->responder = responder;
        checkerObject->nbioContext = NULL;

        *pChecker = checkerObject;
        checkerObject = NULL;

cleanup:

        PKIX_DECREF(checkerObject);

        PKIX_RETURN(OCSPCHECKER);

}

/*
 * FUNCTION: PKIX_OcspChecker_SetPasswordInfo
 *      (see comments in pkix_checker.h)
 */
PKIX_Error *
PKIX_OcspChecker_SetPasswordInfo(
        PKIX_OcspChecker *checker,
        void *passwordInfo,
        void *plContext)
{
        PKIX_ENTER(OCSPCHECKER, "PKIX_OcspChecker_SetPasswordInfo");
        PKIX_NULLCHECK_ONE(checker);

        checker->passwordInfo = passwordInfo;

        PKIX_RETURN(OCSPCHECKER);
}

/*
 * FUNCTION: PKIX_OcspChecker_SetOCSPResponder
 *      (see comments in pkix_checker.h)
 */
PKIX_Error *
PKIX_OcspChecker_SetOCSPResponder(
        PKIX_OcspChecker *checker,
        void *ocspResponder,
        void *plContext)
{
        PKIX_ENTER(OCSPCHECKER, "PKIX_OcspChecker_SetOCSPResponder");
        PKIX_NULLCHECK_ONE(checker);

        checker->responder = ocspResponder;

        PKIX_RETURN(OCSPCHECKER);
}

/*
 * FUNCTION: PKIX_OcspChecker_SetVerifyFcn
 *      (see comments in pkix_checker.h)
 */
PKIX_Error *
PKIX_OcspChecker_SetVerifyFcn(
        PKIX_OcspChecker *checker,
        PKIX_PL_OcspResponse_VerifyCallback verifyFcn,
        void *plContext)
{
        PKIX_ENTER(OCSPCHECKER, "PKIX_OcspChecker_SetVerifyFcn");
        PKIX_NULLCHECK_ONE(checker);

        checker->verifyFcn = verifyFcn;

        PKIX_RETURN(OCSPCHECKER);
}

PKIX_Error *
PKIX_OcspChecker_Initialize(
        PKIX_PL_Date *validityTime,
        void *passwordInfo,
        void *responder,
        PKIX_RevocationChecker **pChecker,
        void *plContext)
{
        PKIX_OcspChecker *oChecker = NULL;

        PKIX_ENTER(OCSPCHECKER, "PKIX_OcspChecker_Initialize");
        PKIX_NULLCHECK_ONE(pChecker);

        PKIX_CHECK(pkix_OcspChecker_Create
                (validityTime, passwordInfo, responder, &oChecker, plContext),
                PKIX_OCSPCHECKERCREATEFAILED);

        PKIX_CHECK(PKIX_RevocationChecker_Create
                (pkix_OcspChecker_Check,
                (PKIX_PL_Object *)oChecker,
                pChecker,
                plContext),
                PKIX_REVOCATIONCHECKERCREATEFAILED);

cleanup:

        PKIX_DECREF(oChecker);

        PKIX_RETURN(OCSPCHECKER);
}


