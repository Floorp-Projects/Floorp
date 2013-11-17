/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * pkix_pl_ocspresponse.c
 *
 */

#include "pkix_pl_ocspresponse.h"

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
        } else if (buildError) {
                pkixErrorResult = buildError;
                buildError = NULL;
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

        if (ocspRsp->nssOCSPResponse != NULL) {
                CERT_DestroyOCSPResponse(ocspRsp->nssOCSPResponse);
                ocspRsp->nssOCSPResponse = NULL;
        }

        if (ocspRsp->signerCert != NULL) {
                CERT_DestroyCertificate(ocspRsp->signerCert);
                ocspRsp->signerCert = NULL;
        }

        httpClient = (const SEC_HttpClientFcn *)(ocspRsp->httpClient);

        if (httpClient && (httpClient->version == 1)) {

                hcv1 = &(httpClient->fcnTable.ftable1);

                if (ocspRsp->sessionRequest != NULL) {
                    (*hcv1->freeFcn)(ocspRsp->sessionRequest);
                    ocspRsp->sessionRequest = NULL;
                }

                if (ocspRsp->serverSession != NULL) {
                    (*hcv1->freeSessionFcn)(ocspRsp->serverSession);
                    ocspRsp->serverSession = NULL;
                }
        }

        if (ocspRsp->arena != NULL) {
                PORT_FreeArena(ocspRsp->arena, PR_FALSE);
                ocspRsp->arena = NULL;
        }

	PKIX_DECREF(ocspRsp->producedAtDate);
	PKIX_DECREF(ocspRsp->pkixSignerCert);
	PKIX_DECREF(ocspRsp->request);

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
        pkix_ClassTable_Entry *entry = &systemClasses[PKIX_OCSPRESPONSE_TYPE];

        PKIX_ENTER(OCSPRESPONSE, "pkix_pl_OcspResponse_RegisterSelf");

        entry->description = "OcspResponse";
        entry->typeObjectSize = sizeof(PKIX_PL_OcspResponse);
        entry->destructor = pkix_pl_OcspResponse_Destroy;
        entry->equalsFunction = pkix_pl_OcspResponse_Equals;
        entry->hashcodeFunction = pkix_pl_OcspResponse_Hashcode;
        entry->duplicateFunction = pkix_duplicateImmutable;

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
 *  "httpMechanism"
 *      GET or POST
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
        const char *httpMechanism,
        void *responder,
        PKIX_PL_VerifyCallback verifyFcn,
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
        char *responseContentType = NULL;
        PRUint16 port = 0;
        SEC_HTTP_SERVER_SESSION serverSession = NULL;
        SEC_HTTP_REQUEST_SESSION sessionRequest = NULL;
        SECItem *encodedRequest = NULL;
        PRUint16 responseCode = 0;
        char *responseData = NULL;
 
        PKIX_ENTER(OCSPRESPONSE, "pkix_pl_OcspResponse_Create");
        PKIX_NULLCHECK_TWO(pNBIOContext, pResponse);

	if (!strcmp(httpMechanism, "GET") && !strcmp(httpMechanism, "POST")) {
		PKIX_ERROR(PKIX_INVALIDOCSPHTTPMETHOD);
	}

        nbioContext = *pNBIOContext;
        *pNBIOContext = NULL;

        if (nbioContext != NULL) {

                ocspResponse = *pResponse;
                PKIX_NULLCHECK_ONE(ocspResponse);

                httpClient = ocspResponse->httpClient;
                serverSession = ocspResponse->serverSession;
                sessionRequest = ocspResponse->sessionRequest;
                PKIX_NULLCHECK_THREE(httpClient, serverSession, sessionRequest);

        } else {
                PKIX_UInt32 timeout =
                    ((PKIX_PL_NssContext*)plContext)->timeoutSeconds;

                PKIX_NULLCHECK_ONE(request);

                PKIX_CHECK(pkix_pl_OcspRequest_GetEncoded
                        (request, &encodedRequest, plContext),
                        PKIX_OCSPREQUESTGETENCODEDFAILED);

                /* prepare initial message to HTTPClient */

                /* Is there a default responder and is it enabled? */
                if (responder) {
                    httpClient = (const SEC_HttpClientFcn *)responder;
                } else {
                    httpClient = SEC_GetRegisteredHttpClient();
                }

                if (httpClient && (httpClient->version == 1)) {
			char *fullGetPath = NULL;
			const char *sessionPath = NULL;
			PRBool usePOST = !strcmp(httpMechanism, "POST");

                        hcv1 = &(httpClient->fcnTable.ftable1);

                        PKIX_CHECK(pkix_pl_OcspRequest_GetLocation
                                (request, &location, plContext),
                                PKIX_OCSPREQUESTGETLOCATIONFAILED);

                        /* parse location -> hostname, port, path */    
                        rv = CERT_ParseURL(location, &hostname, &port, &path);
                        if (rv == SECFailure || hostname == NULL || path == NULL) {
                                PKIX_ERROR(PKIX_URLPARSINGFAILED);
                        }

                        rv = (*hcv1->createSessionFcn)(hostname, port,
                                                       &serverSession);
                        if (rv != SECSuccess) {
                                PKIX_ERROR(PKIX_OCSPSERVERERROR);
                        }       

			if (usePOST) {
				sessionPath = path;
			} else {
				/* calculate, are we allowed to use GET? */
				enum { max_get_request_size = 255 }; /* defined by RFC2560 */
				unsigned char b64ReqBuf[max_get_request_size+1];
				size_t base64size;
				size_t slashLengthIfNeeded = 0;
				size_t pathLength;
				PRInt32 urlEncodedBufLength;
				size_t getURLLength;
				char *walkOutput = NULL;

				pathLength = strlen(path);
				if (path[pathLength-1] != '/') {
					slashLengthIfNeeded = 1;
				}
				base64size = (((encodedRequest->len +2)/3) * 4);
				if (base64size > max_get_request_size) {
					PKIX_ERROR(PKIX_OCSPGETREQUESTTOOBIG);
				}
				memset(b64ReqBuf, 0, sizeof(b64ReqBuf));
				PL_Base64Encode(encodedRequest->data, encodedRequest->len, b64ReqBuf);
				urlEncodedBufLength = ocsp_UrlEncodeBase64Buf(b64ReqBuf, NULL);
				getURLLength = pathLength + urlEncodedBufLength + slashLengthIfNeeded;
				fullGetPath = (char*)PORT_Alloc(getURLLength);
				if (!fullGetPath) {
					PKIX_ERROR(PKIX_OUTOFMEMORY);
				}
				strcpy(fullGetPath, path);
				walkOutput = fullGetPath + pathLength;
				if (walkOutput > fullGetPath && slashLengthIfNeeded) {
					strcpy(walkOutput, "/");
					++walkOutput;
				}
				ocsp_UrlEncodeBase64Buf(b64ReqBuf, walkOutput);
				sessionPath = fullGetPath;
			}

                        rv = (*hcv1->createFcn)(serverSession, "http",
                                                sessionPath, httpMechanism,
                                                PR_SecondsToInterval(timeout),
                                                &sessionRequest);
			sessionPath = NULL;
			if (fullGetPath) {
				PORT_Free(fullGetPath);
				fullGetPath = NULL;
			}
			
                        if (rv != SECSuccess) {
                                PKIX_ERROR(PKIX_OCSPSERVERERROR);
                        }       

			if (usePOST) {
				rv = (*hcv1->setPostDataFcn)(sessionRequest,
							  (char *)encodedRequest->data,
							  encodedRequest->len,
							  "application/ocsp-request");
				if (rv != SECSuccess) {
					PKIX_ERROR(PKIX_OCSPSERVERERROR);
				}
			}

                        /* create a PKIX_PL_OcspResponse object */
                        PKIX_CHECK(PKIX_PL_Object_Alloc
                                    (PKIX_OCSPRESPONSE_TYPE,
                                    sizeof (PKIX_PL_OcspResponse),
                                    (PKIX_PL_Object **)&ocspResponse,
                                    plContext),
                                    PKIX_COULDNOTCREATEOBJECT);

                        PKIX_INCREF(request);
                        ocspResponse->request = request;
                        ocspResponse->httpClient = httpClient;
                        ocspResponse->serverSession = serverSession;
                        serverSession = NULL;
                        ocspResponse->sessionRequest = sessionRequest;
                        sessionRequest = NULL;
                        ocspResponse->verifyFcn = verifyFcn;
                        ocspResponse->handle = CERT_GetDefaultCertDB();
                        ocspResponse->encodedResponse = NULL;
                        ocspResponse->arena = NULL;
                        ocspResponse->producedAt = 0;
                        ocspResponse->producedAtDate = NULL;
                        ocspResponse->pkixSignerCert = NULL;
                        ocspResponse->nssOCSPResponse = NULL;
                        ocspResponse->signerCert = NULL;
                }
        }

        /* begin or resume IO to HTTPClient */
        if (httpClient && (httpClient->version == 1)) {
                PRUint32 responseDataLen = 
                   ((PKIX_PL_NssContext*)plContext)->maxResponseLength;

                hcv1 = &(httpClient->fcnTable.ftable1);

                rv = (*hcv1->trySendAndReceiveFcn)(ocspResponse->sessionRequest,
                        (PRPollDesc **)&nbioContext,
                        &responseCode,
                        (const char **)&responseContentType,
                        NULL,   /* responseHeaders */
                        (const char **)&responseData,
                        &responseDataLen);

                if (rv != SECSuccess) {
                        PKIX_ERROR(PKIX_OCSPSERVERERROR);
                }
                /* responseContentType is a pointer to the null-terminated
                 * string returned by httpclient. Memory allocated for context
                 * type will be freed with freeing of the HttpClient struct. */
                if (PORT_Strcasecmp(responseContentType, 
                                   "application/ocsp-response")) {
                       PKIX_ERROR(PKIX_OCSPSERVERERROR);
                }
                if (nbioContext != NULL) {
                        *pNBIOContext = nbioContext;
                        goto cleanup;
                }
                if (responseCode != 200) {
                        PKIX_ERROR(PKIX_OCSPBADHTTPRESPONSE);
                }
                ocspResponse->arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
                if (ocspResponse->arena == NULL) {
                        PKIX_ERROR(PKIX_OUTOFMEMORY);
                }
                ocspResponse->encodedResponse = SECITEM_AllocItem
                        (ocspResponse->arena, NULL, responseDataLen);
                if (ocspResponse->encodedResponse == NULL) {
                        PKIX_ERROR(PKIX_OUTOFMEMORY);
                }
                PORT_Memcpy(ocspResponse->encodedResponse->data,
                            responseData, responseDataLen);
        }
        *pResponse = ocspResponse;
        ocspResponse = NULL;

cleanup:

        if (path != NULL) {
            PORT_Free(path);
        }
        if (hostname != NULL) {
            PORT_Free(hostname);
        }
        if (ocspResponse) {
            PKIX_DECREF(ocspResponse);
        }
        if (serverSession) {
            hcv1->freeSessionFcn(serverSession);
        }
        if (sessionRequest) {
            hcv1->freeFcn(sessionRequest);
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

        response->nssOCSPResponse =
            CERT_DecodeOCSPResponse(response->encodedResponse);

	if (response->nssOCSPResponse != NULL) {
                *pPassed = PKIX_TRUE;
                *pReturnCode = 0;
        } else {
                *pPassed = PKIX_FALSE;
                *pReturnCode = PORT_GetError();
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
        PKIX_NULLCHECK_FOUR(response, response->nssOCSPResponse, pPassed, pReturnCode);

        rv = CERT_GetOCSPResponseStatus(response->nssOCSPResponse);

	if (rv == SECSuccess) {
                *pPassed = PKIX_TRUE;
                *pReturnCode = 0;
        } else {
                *pPassed = PKIX_FALSE;
                *pReturnCode = PORT_GetError();
        }

        PKIX_RETURN(OCSPRESPONSE);
}


static PKIX_Error*
pkix_pl_OcspResponse_VerifyResponse(
        PKIX_PL_OcspResponse *response,
        PKIX_ProcessingParams *procParams,
        SECCertUsage certUsage,
        void **state,
        PKIX_BuildResult **buildResult,
        void **pNBIOContext,
        void *plContext)
{
    SECStatus rv = SECFailure;

    PKIX_ENTER(OCSPRESPONSE, "pkix_pl_OcspResponse_VerifyResponse");

    if (response->verifyFcn != NULL) {
        void *lplContext = NULL;
        
        PKIX_CHECK(
            PKIX_PL_NssContext_Create(((SECCertificateUsage)1) << certUsage,
                                      PKIX_FALSE, NULL, &lplContext),
            PKIX_NSSCONTEXTCREATEFAILED);

        PKIX_CHECK(
            (response->verifyFcn)((PKIX_PL_Object*)response->pkixSignerCert,
                                  NULL, response->producedAtDate,
                                  procParams, pNBIOContext,
                                  state, buildResult,
                                  NULL, lplContext),
            PKIX_CERTVERIFYKEYUSAGEFAILED);
        rv = SECSuccess;
    } else {
        rv = CERT_VerifyCert(response->handle, response->signerCert, PKIX_TRUE,
                             certUsage, response->producedAt, NULL, NULL);
        if (rv != SECSuccess) {
            PKIX_ERROR(PKIX_CERTVERIFYKEYUSAGEFAILED);
        }
    }

cleanup:
    if (rv != SECSuccess) {
        PORT_SetError(SEC_ERROR_OCSP_INVALID_SIGNING_CERT);
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
        void **pNBIOContext,
        void *plContext)
{
        SECStatus rv = SECFailure;
        CERTOCSPResponse *nssOCSPResponse = NULL;
        CERTCertificate *issuerCert = NULL;
        PKIX_BuildResult *buildResult = NULL;
        void *nbio = NULL;
        void *state = NULL;

        ocspSignature *signature = NULL;
        ocspResponseData *tbsData = NULL;
        SECItem *tbsResponseDataDER = NULL;


        PKIX_ENTER(OCSPRESPONSE, "pkix_pl_OcspResponse_VerifySignature");
        PKIX_NULLCHECK_FOUR(response, cert, pPassed,  pNBIOContext);

        nbio = *pNBIOContext;
        *pNBIOContext = NULL;

        nssOCSPResponse = response->nssOCSPResponse;
        if (nssOCSPResponse == NULL) {
            PORT_SetError(SEC_ERROR_OCSP_MALFORMED_RESPONSE);
            goto cleanup;
        }

        tbsData =
            ocsp_GetResponseData(nssOCSPResponse, &tbsResponseDataDER);
        
        signature = ocsp_GetResponseSignature(nssOCSPResponse);


        /* Are we resuming after a WOULDBLOCK response? */
        if (nbio == NULL) {
            /* No, this is a new query */

            issuerCert = CERT_FindCertIssuer(cert->nssCert, PR_Now(),
                                             certUsageAnyCA);
            
            /*
             * If this signature has already gone through verification,
             * just return the cached result.
             */
            if (signature->wasChecked) {
                if (signature->status == SECSuccess) {
                    response->signerCert =
                        CERT_DupCertificate(signature->cert);
                } else {
                    PORT_SetError(signature->failureReason);
                    goto cleanup;
                }
            }
            
            response->signerCert = 
                ocsp_GetSignerCertificate(response->handle, tbsData,
                                          signature, issuerCert);
            
            if (response->signerCert == NULL) {
                if (PORT_GetError() == SEC_ERROR_UNKNOWN_CERT) {
                    /* Make the error a little more specific. */
                    PORT_SetError(SEC_ERROR_OCSP_INVALID_SIGNING_CERT);
                }
                goto cleanup;
            }            
            PKIX_CHECK( 
                PKIX_PL_Cert_CreateFromCERTCertificate(response->signerCert,
                                                       &(response->pkixSignerCert),
                                                       plContext),
                PKIX_CERTCREATEWITHNSSCERTFAILED);
            
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
            rv = DER_GeneralizedTimeToTime(&response->producedAt,
                                           &tbsData->producedAt);
            if (rv != SECSuccess) {
                PORT_SetError(SEC_ERROR_OCSP_MALFORMED_RESPONSE);
                goto cleanup;
            }
            
            /*
             * We need producedAtDate and pkixSignerCert if we are calling a
             * user-supplied verification function. Let's put their
             * creation before the code that gets repeated when
             * non-blocking I/O is used.
             */
            
            PKIX_CHECK(
                pkix_pl_Date_CreateFromPRTime((PRTime)response->producedAt,
                                              &(response->producedAtDate),
                                              plContext),
                PKIX_DATECREATEFROMPRTIMEFAILED);
            
	}
        
        /*
         * Just because we have a cert does not mean it is any good; check
         * it for validity, trust and usage. Use the caller-supplied
         * verification function, if one was supplied.
         */
        if (ocsp_CertIsOCSPDefaultResponder(response->handle,
                                            response->signerCert)) {
            rv = SECSuccess;
        } else {
            SECCertUsage certUsage;
            if (CERT_IsCACert(response->signerCert, NULL)) {
                certUsage = certUsageAnyCA;
            } else {
                certUsage = certUsageStatusResponder;
            }
            PKIX_CHECK_ONLY_FATAL(
                pkix_pl_OcspResponse_VerifyResponse(response, procParams,
                                                    certUsage, &state,
                                                    &buildResult, &nbio,
                                                    plContext),
                PKIX_CERTVERIFYKEYUSAGEFAILED);
            if (pkixTempErrorReceived) {
                rv = SECFailure;
                goto cleanup;
            }
            if (nbio != NULL) {
                *pNBIOContext = nbio;
                goto cleanup;
            }            
        }

        rv = ocsp_VerifyResponseSignature(response->signerCert, signature,
                                          tbsResponseDataDER, NULL);
        
cleanup:
        if (rv == SECSuccess) {
            *pPassed = PKIX_TRUE;
        } else {
            *pPassed = PKIX_FALSE;
        }
        
        if (signature) {
            if (signature->wasChecked) {
                signature->status = rv;
            }
            
            if (rv != SECSuccess) {
                signature->failureReason = PORT_GetError();
                if (response->signerCert != NULL) {
                    CERT_DestroyCertificate(response->signerCert);
                    response->signerCert = NULL;
                }
            } else {
                /* Save signer's certificate in signature. */
                signature->cert = CERT_DupCertificate(response->signerCert);
            }
        }

	if (issuerCert)
	    CERT_DestroyCertificate(issuerCert);
        
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
        PKIX_PL_OcspCertID *cid,
        PKIX_PL_OcspResponse *response,
        PKIX_Boolean allowCachingOfFailures,
        PKIX_PL_Date *validity,
        PKIX_Boolean *pPassed,
        SECErrorCodes *pReturnCode,
        void *plContext)
{
        PRTime time = 0;
        SECStatus rv = SECFailure;
        CERTOCSPSingleResponse *single = NULL;

        PKIX_ENTER(OCSPRESPONSE, "pkix_pl_OcspResponse_GetStatusForCert");
        PKIX_NULLCHECK_THREE(response, pPassed, pReturnCode);

        /*
         * It is an error to call this function except following a successful
         * return from pkix_pl_OcspResponse_VerifySignature, which would have
         * set response->signerCert.
         */
        PKIX_NULLCHECK_TWO(response->signerCert, response->request);
        PKIX_NULLCHECK_TWO(cid, cid->certID);

        if (validity != NULL) {
            PKIX_Error *er = pkix_pl_Date_GetPRTime(validity, &time, plContext);
            PKIX_DECREF(er);
        }
        if (!time) {
            time = PR_Now();
        }

        rv = ocsp_GetVerifiedSingleResponseForCertID(response->handle,
                                                     response->nssOCSPResponse,
                                                     cid->certID, 
                                                     response->signerCert,
                                                     time, &single);
        if (rv == SECSuccess) {
                /*
                 * Check whether the status says revoked, and if so 
                 * how that compares to the time value passed into this routine.
                 */
                rv = ocsp_CertHasGoodStatus(single->certStatus, time);
        }

        if (rv == SECSuccess || allowCachingOfFailures) {
                /* allowed to update the cache */
                PRBool certIDWasConsumed = PR_FALSE;

                if (single) {
                        ocsp_CacheSingleResponse(cid->certID,single,
                                                 &certIDWasConsumed);
                } else {
                        cert_RememberOCSPProcessingFailure(cid->certID,
                                                           &certIDWasConsumed);
                }

                if (certIDWasConsumed) {
                        cid->certID = NULL;
                }
        }

	if (rv == SECSuccess) {
                *pPassed = PKIX_TRUE;
                *pReturnCode = 0;
        } else {
                *pPassed = PKIX_FALSE;
                *pReturnCode = PORT_GetError();
        }

        PKIX_RETURN(OCSPRESPONSE);
}
