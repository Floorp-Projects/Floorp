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
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Sun Microsystems
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
 * pkix_pl_ocspresponse.c
 *
 */

#include "pkix_pl_ocspresponse.h"

/* ----Functions-lifted-from-ocsp.c------------------------------------- */
/*
 * The initial version of OcspResponse processing used a legacy routine,
 * CERT_VerifyOCSPResponseSignature, to verify that the OCSP response was
 * properly signed. That legacy routine has been moved here, to replace its
 * call to CERT_VerifyCert with the libpkix version.
 */

/*
 * The following structure is only used internally.  It is allocated when
 * someone turns on OCSP checking, and hangs off of the status-configuration
 * structure in the certdb structure.  We use it to keep configuration
 * information specific to OCSP checking.
 */
typedef struct ocspCheckingContextStr {
    PRBool useDefaultResponder;
    char *defaultResponderURI;
    char *defaultResponderNickname;
    CERTCertificate *defaultResponderCert;
} ocspCheckingContext;

extern const SEC_ASN1Template ocsp_ResponseDataTemplate[];

static PRBool
ocsp_matchcert(SECItem *certIndex,CERTCertificate *testCert)
{
    SECItem item;
    unsigned char buf[HASH_LENGTH_MAX];

    item.data = buf;
    item.len = SHA1_LENGTH;

    if (CERT_GetSPKIDigest(NULL,testCert,SEC_OID_SHA1, &item) == NULL) {
	return PR_FALSE;
    }
    if  (SECITEM_ItemsAreEqual(certIndex,&item)) {
	return PR_TRUE;
    }
    if (CERT_GetSPKIDigest(NULL,testCert,SEC_OID_MD5, &item) == NULL) {
	return PR_FALSE;
    }
    if  (SECITEM_ItemsAreEqual(certIndex,&item)) {
	return PR_TRUE;
    }
    if (CERT_GetSPKIDigest(NULL,testCert,SEC_OID_MD2, &item) == NULL) {
	return PR_FALSE;
    }
    if  (SECITEM_ItemsAreEqual(certIndex,&item)) {
	return PR_TRUE;
    }

    return PR_FALSE;
}

/*
 * Return true if the given signerCert is the default responder for
 * the given certID.  If not, or if any error, return false.
 */
static CERTCertificate *
ocsp_CertGetDefaultResponder(CERTCertDBHandle *handle,CERTOCSPCertID *certID)
{
    CERTStatusConfig *statusConfig;
    ocspCheckingContext *ocspcx = NULL;

    statusConfig = CERT_GetStatusConfig(handle);
    if (statusConfig != NULL) {
	ocspcx = statusConfig->statusContext;

	/*
	 * This is actually an internal error, because we should never
	 * have a good statusConfig without a good statusContext, too.
	 * For lack of anything better, though, we just assert and use
	 * the same error as if there were no statusConfig (set below).
	 */
	PORT_Assert(ocspcx != NULL);
    }

    if (ocspcx == NULL) {
	PORT_SetError(SEC_ERROR_OCSP_NOT_ENABLED);
	goto loser;
   }

   /*
    * Right now we have only one default responder.  It applies to
    * all certs when it is used, so the check is simple and certID
    * has no bearing on the answer.  Someday in the future we may
    * allow configuration of different responders for different
    * issuers, and then we would have to use the issuer specified
    * in certID to determine if signerCert is the right one.
    */
    if (ocspcx->useDefaultResponder) {
	PORT_Assert(ocspcx->defaultResponderCert != NULL);
	return ocspcx->defaultResponderCert;
    }

loser:
    return NULL;
}

/* ----Public functions------------------------------------- */
/*
 * This is the libpkix replacement for CERT_VerifyOCSPResponseSignature.
 * It is used if it has been set as the verifyFcn member of ocspChecker.
 */
PKIX_Error *
PKIX_PL_OcspResponse_UseBuildChain(
        PKIX_PL_Cert *signerCert,
	PKIX_PL_Date *producedAt,
        PKIX_ProcessingParams *procParams,
        void **pNBIOContext,
        void **pState,
        PKIX_BuildResult **pBuildResult,
        PKIX_VerifyNode **pVerifyTree,
	void *plContext)
{
        PKIX_ProcessingParams *caProcParams = NULL;
        PKIX_PL_Date *date = NULL;
        PKIX_ComCertSelParams *certSelParams = NULL;
        PKIX_CertSelector *certSelector = NULL;
        void *state = NULL;
        void *nbioContext = NULL;
	PKIX_Error *buildError = NULL;

        PKIX_ENTER(OCSPRESPONSE, "pkix_OcspResponse_UseBuildChain");
        PKIX_NULLCHECK_THREE(signerCert, producedAt, procParams);
        PKIX_NULLCHECK_THREE(pNBIOContext, pState, pBuildResult);

        nbioContext = *pNBIOContext;
        *pNBIOContext = NULL;

        /* Are we resuming after a WOULDBLOCK return, or starting anew ? */
        if (nbioContext == NULL) {
                /* Starting anew */
		PKIX_CHECK(PKIX_PL_Object_Duplicate
                        ((PKIX_PL_Object *)procParams,
                        (PKIX_PL_Object **)&caProcParams,
                        plContext),
        	        PKIX_OBJECTDUPLICATEFAILED);

		PKIX_CHECK(PKIX_ProcessingParams_SetDate(procParams, date, plContext),
	                PKIX_PROCESSINGPARAMSSETDATEFAILED);

	        /* create CertSelector with target certificate in params */

		PKIX_CHECK(PKIX_CertSelector_Create
	                (NULL, NULL, &certSelector, plContext),
	                PKIX_CERTSELECTORCREATEFAILED);

		PKIX_CHECK(PKIX_ComCertSelParams_Create
	                (&certSelParams, plContext),
	                PKIX_COMCERTSELPARAMSCREATEFAILED);

	        PKIX_CHECK(PKIX_ComCertSelParams_SetCertificate
        	        (certSelParams, signerCert, plContext),
                	PKIX_COMCERTSELPARAMSSETCERTIFICATEFAILED);

	        PKIX_CHECK(PKIX_CertSelector_SetCommonCertSelectorParams
	                (certSelector, certSelParams, plContext),
	                PKIX_CERTSELECTORSETCOMMONCERTSELECTORPARAMSFAILED);

	        PKIX_CHECK(PKIX_ProcessingParams_SetTargetCertConstraints
        	        (caProcParams, certSelector, plContext),
                	PKIX_PROCESSINGPARAMSSETTARGETCERTCONSTRAINTSFAILED);
	}

        buildError = PKIX_BuildChain
                (caProcParams,
                &nbioContext,
                pState,
                pBuildResult,
		pVerifyTree,
                plContext);

        /* non-null nbioContext means the build would block */
        if (nbioContext != NULL) {

                *pNBIOContext = nbioContext;

        /* no buildResult means the build has failed */
        } else if (buildError || (pBuildResult == NULL)) {
                PKIX_DECREF(buildError);
                PKIX_ERROR(PKIX_UNABLETOBUILDCHAIN);
        } else {
                PKIX_DECREF(*pState);
        }

cleanup:

        PKIX_DECREF(caProcParams);
        PKIX_DECREF(date);
        PKIX_DECREF(certSelParams);
        PKIX_DECREF(certSelector);

        PKIX_RETURN(OCSPRESPONSE);
}

/* --Private-OcspResponse-Functions------------------------------------- */

/*
 * FUNCTION: pkix_pl_OcspResponse_Destroy
 * (see comments for PKIX_PL_DestructorCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_OcspResponse_Destroy(
        PKIX_PL_Object *object,
        void *plContext)
{
        PKIX_PL_OcspResponse *ocspRsp = NULL;
        const SEC_HttpClientFcn *httpClient = NULL;
        const SEC_HttpClientFcnV1 *hcv1 = NULL;

        PKIX_ENTER(OCSPRESPONSE, "pkix_pl_OcspResponse_Destroy");
        PKIX_NULLCHECK_ONE(object);

        PKIX_CHECK(pkix_CheckType(object, PKIX_OCSPRESPONSE_TYPE, plContext),
                    PKIX_OBJECTNOTANOCSPRESPONSE);

        ocspRsp = (PKIX_PL_OcspResponse *)object;

        if (ocspRsp->encodedResponse != NULL) {
                PKIX_PL_NSSCALL(OCSPRESPONSE, SECITEM_FreeItem,
                        (ocspRsp->encodedResponse, PR_TRUE));
                ocspRsp->encodedResponse = NULL;
        }

        if (ocspRsp->decoded != NULL) {
                PKIX_PL_NSSCALL(OCSPRESPONSE, CERT_DestroyOCSPResponse,
                        (ocspRsp->decoded));
                ocspRsp->decoded = NULL;
                ocspRsp->signerCert = NULL;
        }

        if (ocspRsp->signerCert != NULL) {
                PKIX_PL_NSSCALL(OCSPRESPONSE, CERT_DestroyCertificate,
                        (ocspRsp->signerCert));
                ocspRsp->signerCert = NULL;
        }

        httpClient = (const SEC_HttpClientFcn *)(ocspRsp->httpClient);

        if (httpClient && (httpClient->version == 1)) {

                hcv1 = &(httpClient->fcnTable.ftable1);

                if (ocspRsp->requestSession != NULL) {
                        PKIX_PL_NSSCALL(OCSPRESPONSE, hcv1->freeFcn,
                                (ocspRsp->requestSession));
                        ocspRsp->requestSession = NULL;
                }

                if (ocspRsp->serverSession != NULL) {
                        PKIX_PL_NSSCALL(OCSPRESPONSE, hcv1->freeSessionFcn,
                                (ocspRsp->serverSession));
                        ocspRsp->serverSession = NULL;
                }
        }

        if (ocspRsp->arena != NULL) {
                PORT_FreeArena(ocspRsp->arena, PR_FALSE);
                ocspRsp->arena = NULL;
        }

	PKIX_DECREF(ocspRsp->producedAtDate);
	PKIX_DECREF(ocspRsp->targetCert);

cleanup:

        PKIX_RETURN(OCSPRESPONSE);
}

/*
 * FUNCTION: pkix_pl_OcspResponse_Hashcode
 * (see comments for PKIX_PL_HashcodeCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_OcspResponse_Hashcode(
        PKIX_PL_Object *object,
        PKIX_UInt32 *pHashcode,
        void *plContext)
{
        PKIX_PL_OcspResponse *ocspRsp = NULL;

        PKIX_ENTER(OCSPRESPONSE, "pkix_pl_OcspResponse_Hashcode");
        PKIX_NULLCHECK_TWO(object, pHashcode);

        PKIX_CHECK(pkix_CheckType(object, PKIX_OCSPRESPONSE_TYPE, plContext),
                    PKIX_OBJECTNOTANOCSPRESPONSE);

        ocspRsp = (PKIX_PL_OcspResponse *)object;

        if (ocspRsp->encodedResponse->data == NULL) {
                *pHashcode = 0;
        } else {
                PKIX_CHECK(pkix_hash
                        (ocspRsp->encodedResponse->data,
                        ocspRsp->encodedResponse->len,
                        pHashcode,
                        plContext),
                        PKIX_HASHFAILED);
        }

cleanup:

        PKIX_RETURN(OCSPRESPONSE);
}

/*
 * FUNCTION: pkix_pl_OcspResponse_Equals
 * (see comments for PKIX_PL_Equals_Callback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_OcspResponse_Equals(
        PKIX_PL_Object *firstObj,
        PKIX_PL_Object *secondObj,
        PKIX_Boolean *pResult,
        void *plContext)
{
        PKIX_UInt32 secondType = 0;
        PKIX_UInt32 firstLen = 0;
        PKIX_UInt32 i = 0;
        PKIX_PL_OcspResponse *rsp1 = NULL;
        PKIX_PL_OcspResponse *rsp2 = NULL;
        const unsigned char *firstData = NULL;
        const unsigned char *secondData = NULL;

        PKIX_ENTER(OCSPRESPONSE, "pkix_pl_OcspResponse_Equals");
        PKIX_NULLCHECK_THREE(firstObj, secondObj, pResult);

        /* test that firstObj is a OcspResponse */
        PKIX_CHECK(pkix_CheckType(firstObj, PKIX_OCSPRESPONSE_TYPE, plContext),
                    PKIX_FIRSTOBJARGUMENTNOTANOCSPRESPONSE);

        /*
         * Since we know firstObj is a OcspResponse, if both references are
         * identical, they must be equal
         */
        if (firstObj == secondObj){
                *pResult = PKIX_TRUE;
                goto cleanup;
        }

        /*
         * If secondObj isn't a OcspResponse, we don't throw an error.
         * We simply return a Boolean result of FALSE
         */
        *pResult = PKIX_FALSE;
        PKIX_CHECK(PKIX_PL_Object_GetType(secondObj, &secondType, plContext),
                PKIX_COULDNOTGETTYPEOFSECONDARGUMENT);
        if (secondType != PKIX_OCSPRESPONSE_TYPE) {
                goto cleanup;
        }

        rsp1 = (PKIX_PL_OcspResponse *)firstObj;
        rsp2 = (PKIX_PL_OcspResponse *)secondObj;

        /* If either lacks an encoded string, they cannot be compared */
        firstData = (const unsigned char *)rsp1->encodedResponse->data;
        secondData = (const unsigned char *)rsp2->encodedResponse->data;
        if ((firstData == NULL) || (secondData == NULL)) {
                goto cleanup;
        }

        firstLen = rsp1->encodedResponse->len;

        if (firstLen != rsp2->encodedResponse->len) {
                goto cleanup;
        }

        for (i = 0; i < firstLen; i++) {
                if (*firstData++ != *secondData++) {
                        goto cleanup;
                }
        }

        *pResult = PKIX_TRUE;

cleanup:

        PKIX_RETURN(OCSPRESPONSE);
}

/*
 * FUNCTION: pkix_pl_OcspResponse_RegisterSelf
 * DESCRIPTION:
 *  Registers PKIX_OCSPRESPONSE_TYPE and its related functions with
 *  systemClasses[]
 * PARAMETERS:
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Not Thread Safe - for performance and complexity reasons
 *
 *  Since this function is only called by PKIX_PL_Initialize, which should
 *  only be called once, it is acceptable that this function is not
 *  thread-safe.
 */
PKIX_Error *
pkix_pl_OcspResponse_RegisterSelf(void *plContext)
{
        extern pkix_ClassTable_Entry systemClasses[PKIX_NUMTYPES];
        pkix_ClassTable_Entry entry;

        PKIX_ENTER(OCSPRESPONSE, "pkix_pl_OcspResponse_RegisterSelf");

        entry.description = "OcspResponse";
        entry.destructor = pkix_pl_OcspResponse_Destroy;
        entry.equalsFunction = pkix_pl_OcspResponse_Equals;
        entry.hashcodeFunction = pkix_pl_OcspResponse_Hashcode;
        entry.toStringFunction = NULL;
        entry.comparator = NULL;
        entry.duplicateFunction = pkix_duplicateImmutable;

        systemClasses[PKIX_OCSPRESPONSE_TYPE] = entry;

        PKIX_RETURN(OCSPRESPONSE);
}

/* --Public-Functions------------------------------------------------------- */

/*
 * FUNCTION: pkix_pl_OcspResponse_Create
 * DESCRIPTION:
 *
 *  This function transmits the OcspRequest pointed to by "request" and obtains
 *  an OcspResponse, which it stores at "pOcspResponse". If the HTTPClient
 *  supports non-blocking I/O this function may store a non-NULL value at
 *  "pNBIOContext" (the WOULDBLOCK condition). In that case the caller should
 *  make a subsequent call with the same value in "pNBIOContext" and
 *  "pOcspResponse" to resume the operation. Additional WOULDBLOCK returns may
 *  occur; the caller should persist until a return occurs with NULL stored at
 *  "pNBIOContext".
 *
 *  If a SEC_HttpClientFcn "responder" is supplied, it is used as the client
 *  to which the OCSP query is sent. If none is supplied, the default responder
 *  is used.
 *
 *  If an OcspResponse_VerifyCallback "verifyFcn" is supplied, it is used to
 *  verify the Cert received from the responder as the signer. If none is
 *  supplied, the default verification function is used.
 *
 *  The contents of "request" are ignored on calls subsequent to a WOULDBLOCK
 *  return, and the caller is permitted to supply NULL.
 *
 * PARAMETERS
 *  "request"
 *      Address of the OcspRequest for which a response is desired.
 *  "responder"
 *      Address, if non-NULL, of the SEC_HttpClientFcn to be sent the OCSP
 *      query.
 *  "verifyFcn"
 *      Address, if non-NULL, of the OcspResponse_VerifyCallback function to be
 *      used to verify the Cert of the OCSP responder.
 *  "pNBIOContext"
 *      Address at which platform-dependent information is stored for handling
 *      of non-blocking I/O. Must be non-NULL.
 *  "pOcspResponse"
 *      The address where the created OcspResponse is stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns an OcspResponse Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_OcspResponse_Create(
        PKIX_PL_OcspRequest *request,
        void *responder,
        PKIX_PL_OcspResponse_VerifyCallback verifyFcn,
        void **pNBIOContext,
        PKIX_PL_OcspResponse **pResponse,
        void *plContext)
{
        void *nbioContext = NULL;
        PKIX_PL_OcspResponse *ocspResponse = NULL;
        const SEC_HttpClientFcn *httpClient = NULL;
        const SEC_HttpClientFcnV1 *hcv1 = NULL;
        SECStatus rv = SECFailure;
        char *location = NULL;
        char *hostname = NULL;
        char *path = NULL;
        PRUint16 port = 0;
        SEC_HTTP_SERVER_SESSION serverSession = NULL;
        SEC_HTTP_REQUEST_SESSION requestSession = NULL;
        SECItem *encodedRequest = NULL;
        PRUint16 responseCode = 0;
        char *responseData = NULL;
        PRUint32 responseDataLen = 0;

        PKIX_ENTER(OCSPRESPONSE, "pkix_pl_OcspResponse_Create");
        PKIX_NULLCHECK_TWO(pNBIOContext, pResponse);

        nbioContext = *pNBIOContext;
        *pNBIOContext = NULL;

        if (nbioContext != NULL) {

                ocspResponse = *pResponse;
                PKIX_NULLCHECK_ONE(ocspResponse);

                httpClient = ocspResponse->httpClient;
                serverSession = ocspResponse->serverSession;
                requestSession = ocspResponse->requestSession;
                PKIX_NULLCHECK_THREE(httpClient, serverSession, requestSession);

        } else {

                PKIX_NULLCHECK_ONE(request);

                PKIX_CHECK(pkix_pl_OcspRequest_GetEncoded
                        (request, &encodedRequest, plContext),
                        PKIX_OCSPREQUESTGETENCODEDFAILED);

                /* prepare initial message to HTTPClient */

                /* Is there a default responder and is it enabled? */
                if (!responder) {
                        PKIX_PL_NSSCALLRV
                                (OCSPRESPONSE,
                                responder,
                                (void *)GetRegisteredHttpClient,
                                ());
                }

                httpClient = (const SEC_HttpClientFcn *)responder;

                if (httpClient && (httpClient->version == 1)) {

                        hcv1 = &(httpClient->fcnTable.ftable1);

                        PKIX_CHECK(pkix_pl_OcspRequest_GetLocation
                                (request, &location, plContext),
                                PKIX_OCSPREQUESTGETLOCATIONFAILED);

                        /* parse location -> hostname, port, path */    
                        PKIX_PL_NSSCALLRV(OCSPRESPONSE, rv, CERT_ParseURL,
                                (location, &hostname, &port, &path));

                        if ((hostname == NULL) || (path == NULL)) {
                                PKIX_ERROR(PKIX_URLPARSINGFAILED);
                        }

                        PKIX_PL_NSSCALLRV
                                (OCSPRESPONSE,
                                rv,
                                hcv1->createSessionFcn,
                                (hostname, port, &serverSession));

                        if (rv != SECSuccess) {
                                PKIX_ERROR(PKIX_OCSPSERVERERROR);
                        }       

                        PKIX_PL_NSSCALLRV
                                (OCSPRESPONSE, rv, hcv1->createFcn,
                                (serverSession,
                                "http",
                                path,
                                "POST",
                                PR_TicksPerSecond() * 60,
                                &requestSession));

                        if (rv != SECSuccess) {
                                PKIX_ERROR(PKIX_OCSPSERVERERROR);
                        }       

                        PKIX_PL_NSSCALLRV
                                (OCSPRESPONSE, rv, hcv1->setPostDataFcn,
                                (requestSession,
                                (char *)encodedRequest->data,
                                encodedRequest->len,
                                "application/ocsp-request"));

                        if (rv != SECSuccess) {
                                PKIX_ERROR(PKIX_OCSPSERVERERROR);
                        }       

                        /* create a PKIX_PL_OcspResponse object */
                        PKIX_CHECK(PKIX_PL_Object_Alloc
                                    (PKIX_OCSPRESPONSE_TYPE,
                                    sizeof (PKIX_PL_OcspResponse),
                                    (PKIX_PL_Object **)&ocspResponse,
                                    plContext),
                                    PKIX_COULDNOTCREATEOBJECT);

                        ocspResponse->httpClient = httpClient;
                        ocspResponse->serverSession = serverSession;
                        ocspResponse->requestSession = requestSession;
                        ocspResponse->verifyFcn = verifyFcn;
                        ocspResponse->encodedResponse = NULL;
                        ocspResponse->arena = NULL;
	        	PKIX_PL_NSSCALLRV
				(OCSPRESPONSE,
				ocspResponse->handle,
				CERT_GetDefaultCertDB,
				());
			ocspResponse->producedAt = 0;
			ocspResponse->producedAtDate = NULL;
                        ocspResponse->targetCert = NULL;
                        ocspResponse->decoded = NULL;
                        ocspResponse->signerCert = NULL;

                        PKIX_CHECK(pkix_pl_OcspRequest_GetCertID
                                (request, &ocspResponse->certID, plContext),
                                PKIX_OCSPREQUESTGETCERTIDFAILED);
                }
        }

        /* begin or resume IO to HTTPClient */
        if (httpClient && (httpClient->version == 1)) {

                hcv1 = &(httpClient->fcnTable.ftable1);

                responseDataLen = MAX_OCSP_RESPONSE_LEN;

                PKIX_PL_NSSCALLRV(OCSPRESPONSE, rv, hcv1->trySendAndReceiveFcn,
                        (requestSession,
                        (PRPollDesc **)&nbioContext,
                        &responseCode,
                        NULL,   /* responseContentType */
                        NULL,   /* responseHeaders */
                        (const char **)&responseData,
                        &responseDataLen));

                if (rv != SECSuccess) {
                        PKIX_ERROR(PKIX_OCSPSERVERERROR);
                }       

                if (nbioContext != NULL) {
                        *pNBIOContext = nbioContext;
                        goto cleanup;
                }

                if (responseCode != 200) {
                        PKIX_ERROR(PKIX_BADHTTPRESPONSE);
                }


                PKIX_PL_NSSCALLRV
                        (OCSPRESPONSE, ocspResponse->arena, PORT_NewArena,
                        (DER_DEFAULT_CHUNKSIZE));

                if (ocspResponse->arena == NULL) {
                        PKIX_ERROR(PKIX_OUTOFMEMORY);
                }

                PKIX_PL_NSSCALLRV
                        (OCSPRESPONSE,
                        ocspResponse->encodedResponse,
                        SECITEM_AllocItem,
                        (ocspResponse->arena, NULL, responseDataLen));

                if (ocspResponse->encodedResponse == NULL) {
                        PKIX_ERROR(PKIX_OUTOFMEMORY);
                }

                PKIX_PL_NSSCALL(OCSPRESPONSE, PORT_Memcpy,
                        (ocspResponse->encodedResponse->data,
                        responseData,
                        responseDataLen));

        }

        *pResponse = ocspResponse;

cleanup:

        if (path != NULL) {
                PKIX_PL_NSSCALL(OCSPRESPONSE, PORT_Free, (path));
        }

        if (hostname != NULL) {
                PKIX_PL_NSSCALL(OCSPRESPONSE, PORT_Free, (hostname));
        }

        if (PKIX_ERROR_RECEIVED){
                PKIX_DECREF(ocspResponse);
        }

        PKIX_RETURN(OCSPRESPONSE);
}

/*
 * FUNCTION: pkix_pl_OcspResponse_Decode
 * DESCRIPTION:
 *
 *  This function decodes the DER data contained in the OcspResponse pointed to
 *  by "response", storing PKIX_TRUE at "pPassed" if the decoding was
 *  successful, and PKIX_FALSE otherwise.
 *
 * PARAMETERS
 *  "response"
 *      The address of the OcspResponse whose DER data is to be decoded. Must
 *      be non-NULL.
 *  "pPassed"
 *      Address at which the Boolean result is stored. Must be non-NULL.
 *  "pReturnCode"
 *      Address at which the SECErrorCodes result is stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns an OcspResponse Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */

PKIX_Error *
pkix_pl_OcspResponse_Decode(
        PKIX_PL_OcspResponse *response,
        PKIX_Boolean *pPassed,
        SECErrorCodes *pReturnCode,
        void *plContext)
{

        PKIX_ENTER(OCSPRESPONSE, "PKIX_PL_OcspResponse_Decode");
        PKIX_NULLCHECK_TWO(response, response->encodedResponse);

        PKIX_PL_NSSCALLRV
                (OCSPRESPONSE, response->decoded, CERT_DecodeOCSPResponse,
                (response->encodedResponse));

	if (response->decoded != NULL) {
                *pPassed = PKIX_TRUE;
                *pReturnCode = 0;
        } else {
                *pPassed = PKIX_FALSE;
                PKIX_PL_NSSCALLRV
                        (OCSPRESPONSE, *pReturnCode, PORT_GetError, ());
        }

        PKIX_RETURN(OCSPRESPONSE);
}

/*
 * FUNCTION: pkix_pl_OcspResponse_GetStatus
 * DESCRIPTION:
 *
 *  This function checks the response status of the OcspResponse pointed to
 *  by "response", storing PKIX_TRUE at "pPassed" if the responder understood
 *  the request and considered it valid, and PKIX_FALSE otherwise.
 *
 * PARAMETERS
 *  "response"
 *      The address of the OcspResponse whose status is to be retrieved. Must
 *      be non-NULL.
 *  "pPassed"
 *      Address at which the Boolean result is stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns an OcspResponse Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */

PKIX_Error *
pkix_pl_OcspResponse_GetStatus(
        PKIX_PL_OcspResponse *response,
        PKIX_Boolean *pPassed,
        SECErrorCodes *pReturnCode,
        void *plContext)
{
        SECStatus rv = SECFailure;

        PKIX_ENTER(OCSPRESPONSE, "PKIX_PL_OcspResponse_GetStatus");
        PKIX_NULLCHECK_FOUR(response, response->decoded, pPassed, pReturnCode);

        PKIX_PL_NSSCALLRV(OCSPRESPONSE, rv, CERT_GetOCSPResponseStatus,
                (response->decoded));

	if (rv == SECSuccess) {
                *pPassed = PKIX_TRUE;
                *pReturnCode = 0;
        } else {
                *pPassed = PKIX_FALSE;
                PKIX_PL_NSSCALLRV
                        (OCSPRESPONSE, *pReturnCode, PORT_GetError, ());
        }

        PKIX_RETURN(OCSPRESPONSE);
}

/*
 * FUNCTION: pkix_pl_OcspResponse_VerifySignature
 * DESCRIPTION:
 *
 *  This function verifies the ocspResponse signature field in the OcspResponse
 *  pointed to by "response", storing PKIX_TRUE at "pPassed" if verification
 *  is successful and PKIX_FALSE otherwise. If verification is unsuccessful an
 *  error code (an enumeration of type SECErrorCodes) is stored at *pReturnCode.
 *
 * PARAMETERS
 *  "response"
 *      The address of the OcspResponse whose signature field is to be
 *      retrieved. Must be non-NULL.
 *  "cert"
 *      The address of the Cert for which the OCSP query was made. Must be
 *      non-NULL.
 *  "procParams"
 *      Address of ProcessingParams used to initialize the ExpirationChecker
 *      and TargetCertChecker. Must be non-NULL.
 *  "pPassed"
 *      Address at which the Boolean result is stored. Must be non-NULL.
 *  "pReturnCode"
 *      Address at which the SECErrorCodes result is stored. Must be non-NULL.
 *  "pNBIOContext"
 *      Address at which the NBIOContext is stored indicating whether the
 *      checking is complete. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns an OcspResponse Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_OcspResponse_VerifySignature(
        PKIX_PL_OcspResponse *response,
        PKIX_PL_Cert *cert,
        PKIX_ProcessingParams *procParams,
        PKIX_Boolean *pPassed,
        SECErrorCodes *pReturnCode,
        void **pNBIOContext,
        void *plContext)
{
	PKIX_UInt32 certCount = 0;
	PKIX_UInt32 i = 0;
        SECStatus rv = SECFailure;
        CERTOCSPResponse *decoded = NULL;
        ocspBasicOCSPResponse *basic = NULL;
        ocspSignature *signature = NULL;
        SECItem rawSignature;
        ocspResponseData *tbsData = NULL;
        SECItem *encodedTBS = NULL;
        CERTCertificate *responder = NULL;
        CERTCertificate **certs = NULL;
        CERTCertificate *issuerCert = NULL;
        SECKEYPublicKey *signerKey = NULL;
        void *certIndex = NULL;
        PKIX_Boolean byName = PKIX_FALSE;
	void *pwarg = NULL; /* XXX must modify API if this can be non-NULL */
	void *nbio = NULL;
	void *state = NULL;
	PKIX_BuildResult *buildResult = NULL;
	PKIX_Error *verifyError = NULL;

        PKIX_ENTER(OCSPRESPONSE, "pkix_pl_OcspResponse_VerifySignature");
        PKIX_NULLCHECK_THREE(response, cert, pPassed);
        PKIX_NULLCHECK_TWO(pReturnCode, pNBIOContext);

	*pReturnCode = 0; /* start out with the presumption of success */
        nbio = *pNBIOContext;
        *pNBIOContext = NULL;

        /* Are we resuming after a WOULDBLOCK response? */
        if (nbio != NULL) {
		/* Yes, restore local pointers (previously checked for NULL) */
		/* PKIX_NULLCHECK_ONE(response); */
		decoded = response->decoded;
		/* PKIX_NULLCHECK_TWO(decoded, decoded->responseBytes); */
	        basic = decoded->responseBytes->decodedResponse.basic;
		/* PKIX_NULLCHECK_ONE(basic); */
	        tbsData = basic->tbsResponseData;
		signature = &(basic->responseSignature);
		/* PKIX_NULLCHECK_TWO(tbsData, signature); */
	} else {
		/* No, this is a new query */
	        PKIX_PL_NSSCALLRV
			(OCSPRESPONSE, issuerCert, CERT_FindCertIssuer,
        	        (cert->nssCert, PR_Now(), certUsageAnyCA));

		if (response->decoded == NULL) {
			*pReturnCode = SEC_ERROR_OCSP_MALFORMED_RESPONSE;
			goto cleanup;
		}

		decoded = response->decoded;
	        PKIX_NULLCHECK_ONE(decoded->responseBytes);
        	if (decoded->responseBytes->responseTypeTag
			!= SEC_OID_PKIX_OCSP_BASIC_RESPONSE) {
			*pReturnCode = SEC_ERROR_OCSP_UNKNOWN_RESPONSE_TYPE;
			goto cleanup;
		}

	        basic = decoded->responseBytes->decodedResponse.basic;
		if (basic == NULL) {
			*pReturnCode = SEC_ERROR_OCSP_MALFORMED_RESPONSE;
			goto cleanup;
		}


	        tbsData = basic->tbsResponseData;
		signature = &(basic->responseSignature);

		if ((tbsData == NULL) ||
		    (tbsData->responderID == NULL)) {
			*pReturnCode = SEC_ERROR_OCSP_MALFORMED_RESPONSE;
			goto cleanup;
		}

	        switch (tbsData->responderID->responderIDType) {
	            case ocspResponderID_byName:
		        byName = PKIX_TRUE;
	                certIndex =
	                    &(tbsData->responderID->responderIDValue.name);
	                break;

	          case ocspResponderID_byKey:
	                byName = PKIX_FALSE;
	                certIndex =
			    &(tbsData->responderID->responderIDValue.keyHash);
	                break;

	          case ocspResponderID_other:
	          default:
			*pReturnCode = SEC_ERROR_OCSP_MALFORMED_RESPONSE;
			goto cleanup;
		}

                /*
                 * If this signature has already gone through verification,
                 * just return the cached result.
                 */
                if (signature->wasChecked) {
                        if (signature->status == SECSuccess) {
		                PKIX_PL_NSSCALLRV
					(OCSPRESPONSE,
					response->signerCert,
					CERT_DupCertificate,
					(signature->cert));
	                } else {
		                *pReturnCode = signature->failureReason;
				goto cleanup;
		        }
                }

                /*
	         * If the signature contains some certificates as well,
                 * temporarily import them in case they are needed for
		 * verification.
	         *
                 * Note that the result of this is that each cert in "certs"
	         * needs to be destroyed.
                 */
                certCount = 0;
	        if (signature->derCerts != NULL) {
                        for (;
			    signature->derCerts[certCount] != NULL;
			    certCount++) {
	                    	/* just counting */
                            	/*IMPORT CERT TO SPKI TABLE */
                	}
        	}

	        PKIX_PL_NSSCALLRV(OCSPRESPONSE, rv, CERT_ImportCerts,
                        (response->handle,
                        certUsageStatusResponder,
                        certCount,
                        signature->derCerts,
                        &certs,
                        PR_FALSE,
                        PR_FALSE,
                        NULL));
                if (rv != SECSuccess) {
                        *pReturnCode = SEC_ERROR_OCSP_MALFORMED_RESPONSE;
                        goto cleanup;
                }

                /*
                 * Now look up the certificate that did the signing.
                 * The signer can be specified either by name or by key hash.
                 */
                if (byName == PKIX_TRUE) {
                        SECItem *encodedName;

                        PKIX_PL_NSSCALLRV
                                (OCSPRESPONSE, encodedName, SEC_ASN1EncodeItem,
                                (NULL, NULL, certIndex, CERT_NameTemplate));
                        if (encodedName == NULL) {
                                *pReturnCode =
					SEC_ERROR_OCSP_MALFORMED_RESPONSE;
                                goto cleanup;
                        }

                        PKIX_PL_NSSCALLRV
                                (OCSPRESPONSE,
				response->signerCert,
				CERT_FindCertByName,
                                (response->handle, encodedName));
                        PKIX_PL_NSSCALL(OCSPRESPONSE, SECITEM_FreeItem,
                                (encodedName, PR_TRUE));

                } else {

                        /*
                         * The signer is either 1) a known issuer CA we passed in,
                         * 2) the default OCSP responder, or 3) an intermediate CA
                         * passed in the cert list to use. Figure out which it is.
                         */
                        PKIX_PL_NSSCALLRV
                                (OCSPRESPONSE,
				responder,
				ocsp_CertGetDefaultResponder,
                                (response->handle,NULL));
                        if (responder && ocsp_matchcert(certIndex, responder)) {
		                PKIX_PL_NSSCALLRV
					(OCSPRESPONSE,
					response->signerCert,
					CERT_DupCertificate,
					(responder));
                        } else if (issuerCert &&
				ocsp_matchcert(certIndex, issuerCert)) {
		                	PKIX_PL_NSSCALLRV
						(OCSPRESPONSE,
						response->signerCert,
						CERT_DupCertificate,
						(issuerCert));
                        } 
                        for (i=0;
			    (response->signerCert == NULL) && (i < certCount);
			    i++) {
                                if (ocsp_matchcert(certIndex, certs[i])) {
		                	PKIX_PL_NSSCALLRV
						(OCSPRESPONSE,
						response->signerCert,
						CERT_DupCertificate,
						(certs[i]));
                                }
                        }
                }

                if (response->signerCert == NULL) {
                        *pReturnCode = SEC_ERROR_UNKNOWN_SIGNER;
                        goto cleanup;
                }

                /*
                 * We could mark this true at the top of this function, or
                 * always below at "finish", but if the problem was just that
                 * we could not find the signer's cert, leave that as if the
                 * signature hasn't been checked. Maybe a subsequent call will
		 * have better luck.
                 */
                signature->wasChecked = PR_TRUE;

	        /*
        	 * We are about to verify the signer certificate; we need to
	         * specify *when* that certificate must be valid -- for our
	         * purposes we expect it to be valid when the response was
	         * signed. The value of "producedAt" is the signing time.
	         */
		PKIX_PL_NSSCALLRV(OCSPRESPONSE, rv, DER_GeneralizedTimeToTime,
			(&(response->producedAt), &(tbsData->producedAt)));
	        if (rv != SECSuccess) {
			*pReturnCode = SEC_ERROR_OCSP_MALFORMED_RESPONSE;
			goto cleanup;
	        }

		/*
		 * We need producedAtDate and targetCert if we are calling a
		 * user-supplied verification function. Let's put their
		 * creation before the code that gets repeated when
		 * non-blocking I/O is used.
		 */

	        if (response->verifyFcn != NULL) {
			PKIX_CHECK(pkix_pl_Date_CreateFromPRTime
	                        ((PRTime)(response->producedAt),
				&(response->producedAtDate),
				plContext),
	        	        PKIX_DATECREATEFROMPRTIMEFAILED);

		        PKIX_CHECK(pkix_pl_Cert_CreateWithNSSCert
		                (response->signerCert,
				&(response->targetCert),
				plContext),
	        	        PKIX_CERTCREATEWITHNSSCERTFAILED);
		}
	}

        /*
         * Just because we have a cert does not mean it is any good; check
         * it for validity, trust and usage. Use the caller-supplied
         * verification function, if one was supplied.
         */
        if (response->verifyFcn != NULL) {
                verifyError = (response->verifyFcn)
                        (response->targetCert,
                        response->producedAtDate,
                        procParams,
			&nbio,
			&state,
			&buildResult,
			NULL,
                        plContext);

		if (nbio != NULL) {
			*pNBIOContext = nbio;
			goto cleanup;
		} else if (verifyError) {
			/*
			 * verifyError can be examined if more detailed error
			 * reporting is desired.
			 */
                        *pReturnCode = SEC_ERROR_OCSP_INVALID_SIGNING_CERT;
	                PKIX_DECREF(verifyError);
                        goto cleanup;
		}
        } else {

                PKIX_PL_NSSCALLRV(OCSPRESPONSE, rv, CERT_VerifyCert,
                        (response->handle,
		        response->signerCert,
        		PKIX_TRUE,
	        	certUsageStatusResponder,
		        response->producedAt,
        		pwarg,
	        	NULL));
                if (rv != SECSuccess) {
                        *pReturnCode = SEC_ERROR_OCSP_INVALID_SIGNING_CERT;
                        goto cleanup;
                }
        }


        /*
         * Now get the public key from the signer's certificate; we need
         * it to perform the verification.
         */
        PKIX_PL_NSSCALLRV(OCSPRESPONSE, signerKey, CERT_ExtractPublicKey,
                (response->signerCert));
        if (signerKey == NULL) {
                *pReturnCode = SEC_ERROR_OCSP_INVALID_SIGNING_CERT;
                goto cleanup;
        }

        /*
         * Prepare the data to be verified; it needs to be DER encoded first.
         */
        PKIX_PL_NSSCALLRV(OCSPRESPONSE, encodedTBS, SEC_ASN1EncodeItem,
                (NULL, NULL, tbsData, ocsp_ResponseDataTemplate));
        if (encodedTBS == NULL) {
                *pReturnCode = SEC_ERROR_OCSP_MALFORMED_RESPONSE;
                goto cleanup;
        }

        /*
         * We copy the signature data *pointer* and length, so that we can
         * modify the length without damaging the original copy.  This is a
         * simple copy, not a dup, so no destroy/free is necessary.
         */

        rawSignature = signature->signature;

        /*
         * The raw signature is a bit string, but we need to represent its
         * length in bytes, because that is what the verify function expects.
         * (Note: this is a macro, not a function call.)
         */
        DER_ConvertBitString(&rawSignature);

        PKIX_PL_NSSCALLRV(OCSPRESPONSE, rv, VFY_VerifyData,
                (encodedTBS->data,
                encodedTBS->len,
                signerKey,
                &rawSignature,
                SECOID_GetAlgorithmTag(&signature->signatureAlgorithm),
                pwarg));

cleanup:
        if ((rv == SECSuccess) && (*pReturnCode == 0)) {
                *pPassed = PKIX_TRUE;
        } else {
                *pPassed = PKIX_FALSE;
        }

        if (signature->wasChecked) {
                signature->status = rv;
        }

        if (rv != SECSuccess) {
                signature->failureReason = *pReturnCode;
                if (response->signerCert != NULL) {
                        CERT_DestroyCertificate(response->signerCert);
			response->signerCert = NULL;
                }
        } else {
                /* Save signer's certificate in signature. */
                signature->cert = CERT_DupCertificate(response->signerCert);
        }

        if (encodedTBS != NULL) {
                SECITEM_FreeItem(encodedTBS, PR_TRUE);
        }

        if (signerKey != NULL) {
                SECKEY_DestroyPublicKey(signerKey);
        }

        if (certs != NULL) {
                CERT_DestroyCertArray(certs, certCount);
                /* Free CERTS from SPKDigest Table */
        }

        PKIX_RETURN(OCSPRESPONSE);
}

/*
 * FUNCTION: pkix_pl_OcspResponse_GetStatusForCert
 * DESCRIPTION:
 *
 *  This function checks the revocation status of the Cert for which the
 *  OcspResponse was obtained, storing PKIX_TRUE at "pPassed" if the Cert has
 *  not been revoked and PKIX_FALSE otherwise.
 *
 * PARAMETERS
 *  "response"
 *      The address of the OcspResponse whose certificate status is to be
 *      retrieved. Must be non-NULL.
 *  "pPassed"
 *      Address at which the Boolean result is stored. Must be non-NULL.
 *  "pReturnCode"
 *      Address at which the SECErrorCodes result is stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns an OcspResponse Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_OcspResponse_GetStatusForCert(
        PKIX_PL_OcspResponse *response,
        PKIX_Boolean *pPassed,
        SECErrorCodes *pReturnCode,
        void *plContext)
{
        SECStatus rv = SECFailure;

        PKIX_ENTER(OCSPRESPONSE, "pkix_pl_OcspResponse_GetStatusForCert");
        PKIX_NULLCHECK_THREE(response, pPassed, pReturnCode);

        /*
         * It is an error to call this function except following a successful
         * return from pkix_pl_OcspResponse_VerifySignature, which would have
         * set response->signerCert.
         */
        PKIX_NULLCHECK_ONE(response->signerCert);

        PKIX_PL_NSSCALLRV(OCSPRESPONSE, rv, CERT_GetOCSPStatusForCertID,
                (response->handle,
                response->decoded,
                response->certID,
                response->signerCert,
                PR_Now()));

	if (rv == SECSuccess) {
                *pPassed = PKIX_TRUE;
                *pReturnCode = 0;
        } else {
                *pPassed = PKIX_FALSE;
                PKIX_PL_NSSCALLRV
                        (OCSPRESPONSE, *pReturnCode, PORT_GetError, ());
        }

        PKIX_RETURN(OCSPRESPONSE);
}
