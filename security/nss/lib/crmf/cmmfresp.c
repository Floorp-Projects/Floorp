/* -*- Mode: C; tab-width: 8 -*-*/
/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

/*
 * This file will contain all routines dealing with creating a 
 * CMMFCertRepContent structure through Create/Set functions.
 */

#include "cmmf.h"
#include "cmmfi.h"
#include "crmf.h"
#include "crmfi.h"
#include "secitem.h"
#include "secder.h"

CMMFCertRepContent*
CMMF_CreateCertRepContent(void)
{
    CMMFCertRepContent *retCertRep;
    PRArenaPool        *poolp;

    poolp = PORT_NewArena(CRMF_DEFAULT_ARENA_SIZE);
    if (poolp == NULL) {
        goto loser;
    }
    retCertRep = PORT_ArenaZNew(poolp, CMMFCertRepContent);
    if (retCertRep == NULL) {
        goto loser;
    }
    retCertRep->poolp = poolp;
    return retCertRep;
 loser:
    if (poolp != NULL) {
        PORT_FreeArena(poolp, PR_FALSE);
    }
    return NULL;
}

SECStatus 
cmmf_CertOrEncCertSetCertificate(CMMFCertOrEncCert *certOrEncCert,
				 PRArenaPool       *poolp,
				 CERTCertificate   *inCert)
{
    SECItem               *derDest = NULL;
    SECStatus             rv;

    if (inCert->derCert.data == NULL) {
        derDest = SEC_ASN1EncodeItem(NULL, NULL, inCert, 
				     CMMFCertOrEncCertCertificateTemplate);
	if (derDest == NULL) {
	    goto loser;
	}
    } else {
        derDest = SECITEM_DupItem(&inCert->derCert);
	if (derDest == NULL) {
	    goto loser;
	}
    }
    PORT_Assert(certOrEncCert->cert.certificate == NULL);
    certOrEncCert->cert.certificate = CERT_DupCertificate(inCert);
    certOrEncCert->choice = cmmfCertificate;
    if (poolp != NULL) {
        rv = SECITEM_CopyItem(poolp, &certOrEncCert->derValue, derDest);
	if (rv != SECSuccess) {
	    goto loser;
	}
    } else {
        certOrEncCert->derValue = *derDest;
    }
    PORT_Free(derDest);
    return SECSuccess;
 loser:
    if (derDest != NULL) {
        SECITEM_FreeItem(derDest, PR_TRUE);
    }
    return rv;
}

SECStatus
cmmf_ExtractCertsFromList(CERTCertList      *inCertList,
			  PRArenaPool       *poolp,
			  CERTCertificate ***certArray)
{
    CERTCertificate  **arrayLocalCopy;
    CERTCertListNode  *node;
    int                numNodes = 0, i;

    for (node = CERT_LIST_HEAD(inCertList); !CERT_LIST_END(node, inCertList);
	 node = CERT_LIST_NEXT(node)) {
        numNodes++;
    }

    arrayLocalCopy = *certArray = (poolp == NULL) ?
                    PORT_NewArray(CERTCertificate*, (numNodes+1)) :
                    PORT_ArenaNewArray(poolp, CERTCertificate*, (numNodes+1));
    if (arrayLocalCopy == NULL) {
        return SECFailure;
    }
    for (node = CERT_LIST_HEAD(inCertList), i=0; 
	 !CERT_LIST_END(node, inCertList);
	 node = CERT_LIST_NEXT(node), i++) {
        arrayLocalCopy[i] = CERT_DupCertificate(node->cert);
	if (arrayLocalCopy[i] == NULL) {
	    int j;
	    
	    for (j=0; j<i; j++) {
	        CERT_DestroyCertificate(arrayLocalCopy[j]);
	    }
	    if (poolp == NULL) {
	        PORT_Free(arrayLocalCopy);
	    }
	    *certArray = NULL;
	    return SECFailure;
	}
    }
    arrayLocalCopy[numNodes] = NULL;
    return SECSuccess;
}

SECStatus
CMMF_CertRepContentSetCertResponses(CMMFCertRepContent *inCertRepContent,
				    CMMFCertResponse  **inCertResponses,
				    int                 inNumResponses)
{
    PRArenaPool       *poolp;
    CMMFCertResponse **respArr, *newResp;
    void              *mark;
    SECStatus          rv;
    int                i;

    PORT_Assert (inCertRepContent != NULL &&
		 inCertResponses  != NULL &&
		 inNumResponses    > 0);
    if (inCertRepContent == NULL ||
	inCertResponses  == NULL ||
	inCertRepContent->response != NULL) {
        return SECFailure;
    }
    poolp = inCertRepContent->poolp;
    mark = PORT_ArenaMark(poolp);
    respArr = inCertRepContent->response = 
        PORT_ArenaZNewArray(poolp, CMMFCertResponse*, (inNumResponses+1));
    if (respArr == NULL) {
        goto loser;
    }
    for (i=0; i<inNumResponses; i++) {
        newResp = PORT_ArenaZNew(poolp, CMMFCertResponse);
	if (newResp == NULL) {
	    goto loser;
	}
        rv = cmmf_CopyCertResponse(poolp, newResp, inCertResponses[i]);
	if (rv != SECSuccess) {
	    goto loser;
	}
	respArr[i] = newResp;
    }
    respArr[inNumResponses] = NULL;
    PORT_ArenaUnmark(poolp, mark);
    return SECSuccess;

 loser:
    PORT_ArenaRelease(poolp, mark);
    return SECFailure;
}

CMMFCertResponse*
CMMF_CreateCertResponse(long inCertReqId)
{
    SECItem          *dummy;
    CMMFCertResponse *newResp;
    
    newResp = PORT_ZNew(CMMFCertResponse);
    if (newResp == NULL) {
        goto loser;
    }
    dummy = SEC_ASN1EncodeInteger(NULL, &newResp->certReqId, inCertReqId);
    if (dummy != &newResp->certReqId) {
        goto loser;
    }
    return newResp;

 loser:
    if (newResp != NULL) {
        CMMF_DestroyCertResponse(newResp);
    }
    return NULL;
}

SECStatus
CMMF_CertResponseSetPKIStatusInfoStatus(CMMFCertResponse *inCertResp,
					CMMFPKIStatus     inPKIStatus)
{
    PORT_Assert (inCertResp != NULL && inPKIStatus >= cmmfGranted
		 && inPKIStatus < cmmfNumPKIStatus);

    if  (inCertResp == NULL) {
        return SECFailure;
    }
    return cmmf_PKIStatusInfoSetStatus(&inCertResp->status, NULL,
				       inPKIStatus);
}

SECStatus
CMMF_CertResponseSetCertificate (CMMFCertResponse *inCertResp,
				 CERTCertificate  *inCertificate)
{
    CMMFCertifiedKeyPair *keyPair = NULL;
    SECStatus             rv = SECFailure;

    PORT_Assert(inCertResp != NULL && inCertificate != NULL);
    if (inCertResp == NULL || inCertificate == NULL) {
        return SECFailure;
    }
    if (inCertResp->certifiedKeyPair == NULL) {
        keyPair = inCertResp->certifiedKeyPair = 
	    PORT_ZNew(CMMFCertifiedKeyPair);
    } else {
        keyPair = inCertResp->certifiedKeyPair;
    }
    if (keyPair == NULL) {
        goto loser;
    }
    rv = cmmf_CertOrEncCertSetCertificate(&keyPair->certOrEncCert, NULL,
					  inCertificate);
    if (rv != SECSuccess) {
        goto loser;
    }
    return SECSuccess;
 loser:
    if (keyPair) {
        if (keyPair->certOrEncCert.derValue.data) {
	    PORT_Free(keyPair->certOrEncCert.derValue.data);
	}
	PORT_Free(keyPair);
    }
    return rv;
}


SECStatus
CMMF_CertRepContentSetCAPubs(CMMFCertRepContent *inCertRepContent,
			     CERTCertList       *inCAPubs)
{
    PRArenaPool      *poolp;
    void             *mark;
    SECStatus         rv;

    PORT_Assert(inCertRepContent != NULL &&
		inCAPubs         != NULL &&
		inCertRepContent->caPubs == NULL);
    
    if (inCertRepContent == NULL ||
	inCAPubs == NULL || inCertRepContent == NULL) {
        return SECFailure;
    }

    poolp = inCertRepContent->poolp;
    mark = PORT_ArenaMark(poolp);

    rv = cmmf_ExtractCertsFromList(inCAPubs, poolp,
				   &inCertRepContent->caPubs);

    if (rv != SECSuccess) {
        PORT_ArenaRelease(poolp, mark);
    } else {
        PORT_ArenaUnmark(poolp, mark);
    }
    return rv;
}

CERTCertificate*
CMMF_CertifiedKeyPairGetCertificate(CMMFCertifiedKeyPair *inCertKeyPair,
				    CERTCertDBHandle     *inCertdb)
{
    PORT_Assert(inCertKeyPair != NULL);
    if (inCertKeyPair == NULL) {
        return NULL;
    }
    return cmmf_CertOrEncCertGetCertificate(&inCertKeyPair->certOrEncCert,
					    inCertdb);
}
