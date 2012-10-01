/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cmmf.h"
#include "cmmfi.h"

SEC_ASN1_MKSUB(SECOID_AlgorithmIDTemplate)
SEC_ASN1_MKSUB(SEC_AnyTemplate)
SEC_ASN1_MKSUB(SEC_IntegerTemplate)
SEC_ASN1_MKSUB(SEC_SignedCertificateTemplate)

static const SEC_ASN1Template CMMFCertResponseTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(CMMFCertResponse)},
    { SEC_ASN1_INTEGER, offsetof(CMMFCertResponse, certReqId)},
    { SEC_ASN1_INLINE, offsetof(CMMFCertResponse, status), 
      CMMFPKIStatusInfoTemplate},
    { SEC_ASN1_OPTIONAL | SEC_ASN1_POINTER, 
      offsetof(CMMFCertResponse, certifiedKeyPair),
      CMMFCertifiedKeyPairTemplate},
    { 0 }
};

static const SEC_ASN1Template CMMFCertOrEncCertTemplate[] = {
    { SEC_ASN1_ANY, offsetof(CMMFCertOrEncCert, derValue), NULL, 
      sizeof(CMMFCertOrEncCert)},
    { 0 }
};

const SEC_ASN1Template CMMFCertifiedKeyPairTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(CMMFCertifiedKeyPair)},
    { SEC_ASN1_INLINE, offsetof(CMMFCertifiedKeyPair, certOrEncCert),
      CMMFCertOrEncCertTemplate },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_POINTER | 0,
      offsetof(CMMFCertifiedKeyPair, privateKey),
      CRMFEncryptedValueTemplate},
    { SEC_ASN1_NO_STREAM | SEC_ASN1_OPTIONAL | SEC_ASN1_CONTEXT_SPECIFIC | 
      SEC_ASN1_XTRN | 1,
      offsetof (CMMFCertifiedKeyPair, derPublicationInfo),
      SEC_ASN1_SUB(SEC_AnyTemplate) },
    { 0 }
};

const SEC_ASN1Template CMMFPKIStatusInfoTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(CMMFPKIStatusInfo)},
    { SEC_ASN1_INTEGER, offsetof(CMMFPKIStatusInfo, status)},
    { SEC_ASN1_OPTIONAL | SEC_ASN1_UTF8_STRING, 
      offsetof(CMMFPKIStatusInfo, statusString)},
    { SEC_ASN1_OPTIONAL | SEC_ASN1_BIT_STRING, 
      offsetof(CMMFPKIStatusInfo, failInfo)},
    { 0 }
};

const SEC_ASN1Template CMMFSequenceOfCertsTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF| SEC_ASN1_XTRN, 0, 
			SEC_ASN1_SUB(SEC_SignedCertificateTemplate)}
};

const SEC_ASN1Template CMMFRandTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(CMMFRand)},
    { SEC_ASN1_INTEGER, offsetof(CMMFRand, integer)},
    { SEC_ASN1_OCTET_STRING, offsetof(CMMFRand, senderHash)},
    { 0 }
};

const SEC_ASN1Template CMMFPOPODecKeyRespContentTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF | SEC_ASN1_XTRN, 
      offsetof(CMMFPOPODecKeyRespContent, responses),
      SEC_ASN1_SUB(SEC_IntegerTemplate), 
      sizeof(CMMFPOPODecKeyRespContent)},
    { 0 }
};

const SEC_ASN1Template CMMFCertOrEncCertEncryptedCertTemplate[] = {
    { SEC_ASN1_CONTEXT_SPECIFIC | 1,
      0,
      CRMFEncryptedValueTemplate},
    { 0 }
};

const SEC_ASN1Template CMMFCertOrEncCertCertificateTemplate[] = {
    { SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_XTRN | 0,
      0,
      SEC_ASN1_SUB(SEC_SignedCertificateTemplate)},
    { 0 }
};

const SEC_ASN1Template CMMFCertRepContentTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(CMMFCertRepContent)},
    { SEC_ASN1_CONSTRUCTED | SEC_ASN1_OPTIONAL |
      SEC_ASN1_CONTEXT_SPECIFIC | 1,
      offsetof(CMMFCertRepContent, caPubs),
      CMMFSequenceOfCertsTemplate },
    { SEC_ASN1_SEQUENCE_OF, offsetof(CMMFCertRepContent, response),
      CMMFCertResponseTemplate},
    { 0 }
};

static const SEC_ASN1Template CMMFChallengeTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(CMMFChallenge)},
    { SEC_ASN1_POINTER | SEC_ASN1_OPTIONAL | SEC_ASN1_XTRN, 
      offsetof(CMMFChallenge, owf),
      SEC_ASN1_SUB(SECOID_AlgorithmIDTemplate) },
    { SEC_ASN1_OCTET_STRING, offsetof(CMMFChallenge, witness) },
    { SEC_ASN1_ANY, offsetof(CMMFChallenge, senderDER) },
    { SEC_ASN1_OCTET_STRING, offsetof(CMMFChallenge, key) },
    { SEC_ASN1_OCTET_STRING, offsetof(CMMFChallenge, challenge) },
    { 0 }
};

const SEC_ASN1Template CMMFPOPODecKeyChallContentTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF,offsetof(CMMFPOPODecKeyChallContent, challenges),
      CMMFChallengeTemplate, sizeof(CMMFPOPODecKeyChallContent) },
    { 0 }
};

SECStatus
cmmf_decode_process_cert_response(PRArenaPool      *poolp, 
				  CERTCertDBHandle *db,
				  CMMFCertResponse *inCertResp)
{
    SECStatus rv = SECSuccess;
  
    if (inCertResp->certifiedKeyPair != NULL) {
        rv = cmmf_decode_process_certified_key_pair(poolp, 
						    db,
						inCertResp->certifiedKeyPair);
    }
    return rv;
}

static CERTCertificate*
cmmf_DecodeDERCertificate(CERTCertDBHandle *db, SECItem *derCert)
{
    CERTCertificate *newCert;

    newCert = CERT_NewTempCertificate(db, derCert, NULL, PR_FALSE, PR_TRUE);
    return newCert;
}

static CMMFCertOrEncCertChoice
cmmf_get_certorenccertchoice_from_der(SECItem *der)
{
    CMMFCertOrEncCertChoice retChoice; 

    switch(der->data[0] & 0x0f) {
    case 0:
        retChoice = cmmfCertificate;
	break;
    case 1:
        retChoice = cmmfEncryptedCert;
	break;
    default:
        retChoice = cmmfNoCertOrEncCert;
	break;
    }
    return retChoice;
}

static SECStatus
cmmf_decode_process_certorenccert(PRArenaPool       *poolp,
				  CERTCertDBHandle  *db,
				  CMMFCertOrEncCert *inCertOrEncCert)
{
    SECStatus rv = SECSuccess;

    inCertOrEncCert->choice = 
        cmmf_get_certorenccertchoice_from_der(&inCertOrEncCert->derValue);

    switch (inCertOrEncCert->choice) {
    case cmmfCertificate:
        {
	    /* The DER has implicit tagging, so we gotta switch it to 
	     * un-tagged in order for the ASN1 parser to understand it.
	     * Saving the bits that were changed.
	     */ 
	    inCertOrEncCert->derValue.data[0] = 0x30;
	    inCertOrEncCert->cert.certificate = 
	        cmmf_DecodeDERCertificate(db, &inCertOrEncCert->derValue);
	    if (inCertOrEncCert->cert.certificate == NULL) {
	        rv = SECFailure;
	    }
	
	}
	break;
    case cmmfEncryptedCert:
	PORT_Assert(poolp);
	if (!poolp) {
	    PORT_SetError(SEC_ERROR_INVALID_ARGS);
	    rv = SECFailure;
	    break;
	}
        inCertOrEncCert->cert.encryptedCert =
	    PORT_ArenaZNew(poolp, CRMFEncryptedValue);
	if (inCertOrEncCert->cert.encryptedCert == NULL) {
	    rv = SECFailure;
	    break;
	}
	rv = SEC_ASN1Decode(poolp, inCertOrEncCert->cert.encryptedCert, 
			    CMMFCertOrEncCertEncryptedCertTemplate, 
			    (const char*)inCertOrEncCert->derValue.data,
			    inCertOrEncCert->derValue.len);
	break;
    default:
        rv = SECFailure;
    }
    return rv;
}

SECStatus 
cmmf_decode_process_certified_key_pair(PRArenaPool          *poolp,
				       CERTCertDBHandle     *db,
				       CMMFCertifiedKeyPair *inCertKeyPair)
{
    return cmmf_decode_process_certorenccert (poolp,
					      db,
					      &inCertKeyPair->certOrEncCert);
}


