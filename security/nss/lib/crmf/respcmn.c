/* -*- Mode: C; tab-width: 8 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cmmf.h"
#include "cmmfi.h"
#include "secitem.h"
#include "secder.h"

SECStatus 
cmmf_DestroyPKIStatusInfo (CMMFPKIStatusInfo *info, PRBool freeit)
{
    if (info->status.data != NULL) {
        PORT_Free(info->status.data);
        info->status.data = NULL;
    }
    if (info->statusString.data != NULL) {
        PORT_Free(info->statusString.data);
        info->statusString.data = NULL;
    }
    if (info->failInfo.data != NULL) {
        PORT_Free(info->failInfo.data);
        info->failInfo.data = NULL;
    }
    if (freeit) {
        PORT_Free(info);
    }
    return SECSuccess;
}

SECStatus
CMMF_DestroyCertResponse(CMMFCertResponse *inCertResp)
{
    PORT_Assert(inCertResp != NULL);
    if (inCertResp != NULL) {
        if (inCertResp->certReqId.data != NULL) {
	    PORT_Free(inCertResp->certReqId.data);
	}
	cmmf_DestroyPKIStatusInfo(&inCertResp->status, PR_FALSE);
	if (inCertResp->certifiedKeyPair != NULL) {
	    CMMF_DestroyCertifiedKeyPair(inCertResp->certifiedKeyPair);
	}
	PORT_Free(inCertResp);
    }
    return SECSuccess;
}

SECStatus
CMMF_DestroyCertRepContent(CMMFCertRepContent *inCertRepContent)
{
    PORT_Assert(inCertRepContent != NULL);
    if (inCertRepContent != NULL) {
        CMMFCertResponse   **pResponse = inCertRepContent->response;
        if (pResponse != NULL) {
            for (; *pResponse != NULL; pResponse++) {
	        CMMFCertifiedKeyPair *certKeyPair = (*pResponse)->certifiedKeyPair;
		/* XXX Why not call CMMF_DestroyCertifiedKeyPair or
		** XXX cmmf_DestroyCertOrEncCert ?  
		*/
                if (certKeyPair != NULL                    &&
                    certKeyPair->certOrEncCert.choice == cmmfCertificate &&
                    certKeyPair->certOrEncCert.cert.certificate != NULL) {
                    CERT_DestroyCertificate
                                 (certKeyPair->certOrEncCert.cert.certificate);
		    certKeyPair->certOrEncCert.cert.certificate = NULL;
                }
            }
        }
	if (inCertRepContent->caPubs) {
	    CERTCertificate     **caPubs = inCertRepContent->caPubs;
	    for (; *caPubs; ++caPubs) {
		CERT_DestroyCertificate(*caPubs);
		*caPubs = NULL;
	    }
	}
	if (inCertRepContent->poolp != NULL) {
	    PORT_FreeArena(inCertRepContent->poolp, PR_TRUE);
	}
    }
    return SECSuccess;
}

SECStatus
CMMF_DestroyPOPODecKeyChallContent(CMMFPOPODecKeyChallContent *inDecKeyCont)
{
    PORT_Assert(inDecKeyCont != NULL);
    if (inDecKeyCont != NULL && inDecKeyCont->poolp) {
        PORT_FreeArena(inDecKeyCont->poolp, PR_FALSE);
    }
    return SECSuccess;
}

SECStatus
crmf_create_prtime(SECItem *src, PRTime **dest)
{
   *dest = PORT_ZNew(PRTime);
    return DER_DecodeTimeChoice(*dest, src);
}

CRMFCertExtension*
crmf_copy_cert_extension(PRArenaPool *poolp, CRMFCertExtension *inExtension)
{
    PRBool             isCritical;
    SECOidTag          id;
    SECItem           *data;
    CRMFCertExtension *newExt;

    PORT_Assert(inExtension != NULL);
    if (inExtension == NULL) {
        return NULL;
    }
    id         = CRMF_CertExtensionGetOidTag(inExtension);
    isCritical = CRMF_CertExtensionGetIsCritical(inExtension);
    data       = CRMF_CertExtensionGetValue(inExtension);
    newExt = crmf_create_cert_extension(poolp, id, 
					isCritical,
					data);
    SECITEM_FreeItem(data, PR_TRUE);
    return newExt;    
}

static SECItem*
cmmf_encode_certificate(CERTCertificate *inCert)
{
    return SEC_ASN1EncodeItem(NULL, NULL, inCert, 
			      SEC_ASN1_GET(SEC_SignedCertificateTemplate));
}

CERTCertList*
cmmf_MakeCertList(CERTCertificate **inCerts)
{
    CERTCertList    *certList;
    CERTCertificate *currCert;
    SECItem         *derCert, *freeCert = NULL;
    SECStatus        rv;
    int              i;

    certList = CERT_NewCertList();
    if (certList == NULL) {
        return NULL;
    }
    for (i=0; inCerts[i] != NULL; i++) {
        derCert = &inCerts[i]->derCert;
	if (derCert->data == NULL) {
	    derCert = freeCert = cmmf_encode_certificate(inCerts[i]);
	}
	currCert=CERT_NewTempCertificate(CERT_GetDefaultCertDB(), 
	                                 derCert, NULL, PR_FALSE, PR_TRUE);
	if (freeCert != NULL) {
	    SECITEM_FreeItem(freeCert, PR_TRUE);
	    freeCert = NULL;
	}
	if (currCert == NULL) {
	    goto loser;
	}
	rv = CERT_AddCertToListTail(certList, currCert);
	if (rv != SECSuccess) {
	    goto loser;
	}
    }
    return certList;
 loser:
    CERT_DestroyCertList(certList);
    return NULL;
}

CMMFPKIStatus
cmmf_PKIStatusInfoGetStatus(CMMFPKIStatusInfo *inStatus)
{
    long derVal;

    derVal = DER_GetInteger(&inStatus->status);
    if (derVal == -1 || derVal < cmmfGranted || derVal >= cmmfNumPKIStatus) {
        return cmmfNoPKIStatus;
    }
    return (CMMFPKIStatus)derVal;
}

int
CMMF_CertRepContentGetNumResponses(CMMFCertRepContent *inCertRepContent)
{
    int numResponses = 0;
    PORT_Assert (inCertRepContent != NULL);
    if (inCertRepContent != NULL && inCertRepContent->response != NULL) {
        while (inCertRepContent->response[numResponses] != NULL) {
	    numResponses++;
	}
    }
    return numResponses;
}


SECStatus
cmmf_DestroyCertOrEncCert(CMMFCertOrEncCert *certOrEncCert, PRBool freeit)
{
    switch (certOrEncCert->choice) {
    case cmmfCertificate:
        CERT_DestroyCertificate(certOrEncCert->cert.certificate);
	certOrEncCert->cert.certificate = NULL;
	break;
    case cmmfEncryptedCert:
        crmf_destroy_encrypted_value(certOrEncCert->cert.encryptedCert,
				     PR_TRUE);
        certOrEncCert->cert.encryptedCert = NULL;
	break;
    default:
        break;
    }
    if (freeit) {
        PORT_Free(certOrEncCert);
    }
    return SECSuccess;
}

SECStatus
cmmf_copy_secitem (PRArenaPool *poolp, SECItem *dest, SECItem *src)
{
    SECStatus rv;

    if (src->data != NULL) {
        rv = SECITEM_CopyItem(poolp, dest, src);
    } else {
        dest->data = NULL;
	dest->len  = 0;
	rv = SECSuccess;
    }
    return rv;
}

SECStatus
CMMF_DestroyCertifiedKeyPair(CMMFCertifiedKeyPair *inCertKeyPair)
{
    PORT_Assert(inCertKeyPair != NULL);
    if (inCertKeyPair != NULL) {
        cmmf_DestroyCertOrEncCert(&inCertKeyPair->certOrEncCert, PR_FALSE);
        if (inCertKeyPair->privateKey) {
            crmf_destroy_encrypted_value(inCertKeyPair->privateKey, PR_TRUE);
        }
        if (inCertKeyPair->derPublicationInfo.data) {
            PORT_Free(inCertKeyPair->derPublicationInfo.data);
        }
        PORT_Free(inCertKeyPair);
    }
    return SECSuccess;
}

SECStatus
cmmf_CopyCertResponse(PRArenaPool      *poolp, 
		      CMMFCertResponse *dest,
		      CMMFCertResponse *src)
{
    SECStatus rv;

    if (src->certReqId.data != NULL) {
        rv = SECITEM_CopyItem(poolp, &dest->certReqId, &src->certReqId);
	if (rv != SECSuccess) {
	    return rv;
	}
    }
    rv = cmmf_CopyPKIStatusInfo(poolp, &dest->status, &src->status);
    if (rv != SECSuccess) {
        return rv;
    }
    if (src->certifiedKeyPair != NULL) {
	CMMFCertifiedKeyPair *destKeyPair;

	destKeyPair = (poolp == NULL) ? PORT_ZNew(CMMFCertifiedKeyPair) :
	                        PORT_ArenaZNew(poolp, CMMFCertifiedKeyPair);
	if (!destKeyPair) {
	    return SECFailure;
	}
	rv = cmmf_CopyCertifiedKeyPair(poolp, destKeyPair,
				       src->certifiedKeyPair);
	if (rv != SECSuccess) {
	    if (!poolp) {
	        CMMF_DestroyCertifiedKeyPair(destKeyPair);
	    }
	    return rv;
	}
	dest->certifiedKeyPair = destKeyPair;
    }
    return SECSuccess;
}

static SECStatus
cmmf_CopyCertOrEncCert(PRArenaPool *poolp, CMMFCertOrEncCert *dest,
		       CMMFCertOrEncCert *src)
{
    SECStatus           rv = SECSuccess;
    CRMFEncryptedValue *encVal;

    dest->choice = src->choice;
    rv = cmmf_copy_secitem(poolp, &dest->derValue, &src->derValue);
    switch (src->choice) {
    case cmmfCertificate:
        dest->cert.certificate = CERT_DupCertificate(src->cert.certificate);
	break;
    case cmmfEncryptedCert:
 	encVal = (poolp == NULL) ? PORT_ZNew(CRMFEncryptedValue) :
	                           PORT_ArenaZNew(poolp, CRMFEncryptedValue);
	if (encVal == NULL) {
	    return SECFailure;
	}
        rv = crmf_copy_encryptedvalue(poolp, src->cert.encryptedCert, encVal);
	if (rv != SECSuccess) {
	    if (!poolp) {
	        crmf_destroy_encrypted_value(encVal, PR_TRUE);
	    }
	    return rv;
	}
	dest->cert.encryptedCert = encVal;        
	break;
    default:
        rv = SECFailure;
    }
    return rv;
}

SECStatus
cmmf_CopyCertifiedKeyPair(PRArenaPool *poolp, CMMFCertifiedKeyPair *dest,
			  CMMFCertifiedKeyPair *src)
{
    SECStatus rv;

    rv = cmmf_CopyCertOrEncCert(poolp, &dest->certOrEncCert, 
				&src->certOrEncCert);
    if (rv != SECSuccess) {
        return rv;
    }

    if (src->privateKey != NULL) {
	CRMFEncryptedValue *encVal;

	encVal = (poolp == NULL) ? PORT_ZNew(CRMFEncryptedValue) :
	                           PORT_ArenaZNew(poolp, CRMFEncryptedValue);
	if (encVal == NULL) {
	    return SECFailure;
	}
	rv = crmf_copy_encryptedvalue(poolp, src->privateKey, 
				      encVal);
	if (rv != SECSuccess) {
	    if (!poolp) {
	        crmf_destroy_encrypted_value(encVal, PR_TRUE);
	    }
	    return rv;
	}
	dest->privateKey = encVal;
    }
    rv = cmmf_copy_secitem(poolp, &dest->derPublicationInfo, 
			   &src->derPublicationInfo);
    return rv;
}

SECStatus
cmmf_CopyPKIStatusInfo(PRArenaPool *poolp, CMMFPKIStatusInfo *dest,
		       CMMFPKIStatusInfo *src)
{
    SECStatus rv;

    rv = cmmf_copy_secitem (poolp, &dest->status, &src->status);
    if (rv != SECSuccess) {
        return rv;
    }
    rv = cmmf_copy_secitem (poolp, &dest->statusString, &src->statusString);
    if (rv != SECSuccess) {
        return rv;
    }
    rv = cmmf_copy_secitem (poolp, &dest->failInfo, &src->failInfo);
    return rv;
}

CERTCertificate*
cmmf_CertOrEncCertGetCertificate(CMMFCertOrEncCert *certOrEncCert,
				 CERTCertDBHandle  *certdb)
{
    if (certOrEncCert->choice           != cmmfCertificate || 
	certOrEncCert->cert.certificate == NULL) {
        return NULL;
    }
    return CERT_NewTempCertificate(certdb,
				   &certOrEncCert->cert.certificate->derCert,
				   NULL, PR_FALSE, PR_TRUE);
}

SECStatus 
cmmf_PKIStatusInfoSetStatus(CMMFPKIStatusInfo    *statusInfo,
			    PRArenaPool          *poolp,
			    CMMFPKIStatus         inStatus)
{
    SECItem *dummy;
    
    if (inStatus <cmmfGranted || inStatus >= cmmfNumPKIStatus) {
        return SECFailure;
    }

    dummy = SEC_ASN1EncodeInteger(poolp, &statusInfo->status, inStatus); 
    PORT_Assert(dummy == &statusInfo->status);
    if (dummy != &statusInfo->status) {
        SECITEM_FreeItem(dummy, PR_TRUE);
	return SECFailure;
    }
    return SECSuccess;
}


