/* -*- Mode: C; tab-width: 8 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/*
 * This file will contain all routines needed by a client that has 
 * to parse a CMMFCertRepContent structure and retirieve the appropriate
 * data.
 */

#include "cmmf.h"
#include "cmmfi.h"
#include "crmf.h"
#include "crmfi.h"
#include "secitem.h"
#include "secder.h"
#include "secasn1.h"

CMMFCertRepContent*
CMMF_CreateCertRepContentFromDER(CERTCertDBHandle *db, const char *buf, 
				 long len)
{
    PLArenaPool        *poolp;
    CMMFCertRepContent *certRepContent;
    SECStatus           rv;
    int                 i;

    poolp = PORT_NewArena(CRMF_DEFAULT_ARENA_SIZE);
    if (poolp == NULL) {
        return NULL;
    }
    certRepContent = PORT_ArenaZNew(poolp, CMMFCertRepContent);
    if (certRepContent == NULL) {
        goto loser;
    }
    certRepContent->poolp = poolp;
    rv = SEC_ASN1Decode(poolp, certRepContent, CMMFCertRepContentTemplate,
			buf, len);
    if (rv != SECSuccess) {
        goto loser;
    }
    if (certRepContent->response != NULL) {
        for (i=0; certRepContent->response[i] != NULL; i++) {
	    rv = cmmf_decode_process_cert_response(poolp, db,
					       certRepContent->response[i]);
	    if (rv != SECSuccess) {
	        goto loser;
	    }
	}
    }
    certRepContent->isDecoded = PR_TRUE;
    return certRepContent;
 loser:
    PORT_FreeArena(poolp, PR_FALSE);
    return NULL;
}

long
CMMF_CertResponseGetCertReqId(CMMFCertResponse *inCertResp)
{
    PORT_Assert(inCertResp != NULL);
    if (inCertResp == NULL) {
        return -1;
    }
    return DER_GetInteger(&inCertResp->certReqId);
}

PRBool
cmmf_CertRepContentIsIndexValid(CMMFCertRepContent *inCertRepContent,
                                int                 inIndex)
{
    int numResponses;

    PORT_Assert(inCertRepContent != NULL);
    numResponses = CMMF_CertRepContentGetNumResponses(inCertRepContent);
    return (PRBool)(inIndex >= 0 && inIndex < numResponses);
}

CMMFCertResponse*
CMMF_CertRepContentGetResponseAtIndex(CMMFCertRepContent *inCertRepContent,
				      int                 inIndex)
{
    CMMFCertResponse *certResponse;
    SECStatus         rv;

    PORT_Assert(inCertRepContent != NULL &&
		cmmf_CertRepContentIsIndexValid(inCertRepContent, inIndex));
    if (inCertRepContent == NULL ||
	!cmmf_CertRepContentIsIndexValid(inCertRepContent, inIndex)) {
        return NULL;
    }
    certResponse = PORT_ZNew(CMMFCertResponse);
    rv = cmmf_CopyCertResponse(NULL, certResponse, 
			       inCertRepContent->response[inIndex]);
    if (rv != SECSuccess) {
        CMMF_DestroyCertResponse(certResponse);
	certResponse = NULL;
    }
    return certResponse;
}

CMMFPKIStatus
CMMF_CertResponseGetPKIStatusInfoStatus(CMMFCertResponse *inCertResp)
{
    PORT_Assert(inCertResp != NULL);
    if (inCertResp == NULL) {
        return cmmfNoPKIStatus;
    }
    return cmmf_PKIStatusInfoGetStatus(&inCertResp->status);
}

CERTCertificate*
CMMF_CertResponseGetCertificate(CMMFCertResponse *inCertResp,
				CERTCertDBHandle *inCertdb)
{
    PORT_Assert(inCertResp != NULL);
    if (inCertResp == NULL || inCertResp->certifiedKeyPair == NULL) {
        return NULL;
    }
    
    return cmmf_CertOrEncCertGetCertificate(
		&inCertResp->certifiedKeyPair->certOrEncCert, inCertdb);
				   
}

CERTCertList*
CMMF_CertRepContentGetCAPubs (CMMFCertRepContent *inCertRepContent)
{
    PORT_Assert (inCertRepContent != NULL);
    if (inCertRepContent == NULL || inCertRepContent->caPubs == NULL) {
        return NULL;
    }
    return cmmf_MakeCertList(inCertRepContent->caPubs);
}

