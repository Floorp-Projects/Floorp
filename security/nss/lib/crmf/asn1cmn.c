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

#include "nssrenam.h"
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


