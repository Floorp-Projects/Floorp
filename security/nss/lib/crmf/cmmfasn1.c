/* -*- Mode: C; tab-width: 8 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cmmf.h"
#include "cmmfi.h"
#include "secasn1.h"
#include "secitem.h"

SEC_ASN1_MKSUB(SEC_SignedCertificateTemplate)

static const SEC_ASN1Template CMMFSequenceOfCertifiedKeyPairsTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF, 0, CMMFCertifiedKeyPairTemplate}
};

static const SEC_ASN1Template CMMFKeyRecRepContentTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(CMMFKeyRecRepContent)},
    { SEC_ASN1_INLINE, offsetof(CMMFKeyRecRepContent, status), 
      CMMFPKIStatusInfoTemplate},
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_POINTER | 
		SEC_ASN1_XTRN | 0,
      offsetof(CMMFKeyRecRepContent, newSigCert),
      SEC_ASN1_SUB(SEC_SignedCertificateTemplate)},
    { SEC_ASN1_CONSTRUCTED | SEC_ASN1_OPTIONAL | SEC_ASN1_CONTEXT_SPECIFIC | 1,
      offsetof(CMMFKeyRecRepContent, caCerts),
      CMMFSequenceOfCertsTemplate},
    { SEC_ASN1_CONSTRUCTED | SEC_ASN1_OPTIONAL | SEC_ASN1_CONTEXT_SPECIFIC | 2,
      offsetof(CMMFKeyRecRepContent, keyPairHist),
      CMMFSequenceOfCertifiedKeyPairsTemplate},
    { 0 }
};

SECStatus
CMMF_EncodeCertRepContent (CMMFCertRepContent        *inCertRepContent,
			   CRMFEncoderOutputCallback  inCallback,
			   void                      *inArg)
{
    return cmmf_user_encode(inCertRepContent, inCallback, inArg,
			    CMMFCertRepContentTemplate);
}

SECStatus
CMMF_EncodePOPODecKeyChallContent(CMMFPOPODecKeyChallContent *inDecKeyChall,
				  CRMFEncoderOutputCallback inCallback,
				  void                     *inArg)
{
    return cmmf_user_encode(inDecKeyChall, inCallback, inArg,
			    CMMFPOPODecKeyChallContentTemplate);
}

CMMFPOPODecKeyRespContent*
CMMF_CreatePOPODecKeyRespContentFromDER(const char *buf, long len)
{
    PLArenaPool               *poolp;
    CMMFPOPODecKeyRespContent *decKeyResp;
    SECStatus                  rv;

    poolp = PORT_NewArena(CRMF_DEFAULT_ARENA_SIZE);
    if (poolp == NULL) {
        return NULL;
    }
    decKeyResp = PORT_ArenaZNew(poolp, CMMFPOPODecKeyRespContent);
    if (decKeyResp == NULL) {
        goto loser;
    }
    decKeyResp->poolp = poolp;
    rv = SEC_ASN1Decode(poolp, decKeyResp, CMMFPOPODecKeyRespContentTemplate,
			buf, len);
    if (rv != SECSuccess) {
        goto loser;
    }
    return decKeyResp;
    
 loser:
    if (poolp != NULL) {
        PORT_FreeArena(poolp, PR_FALSE);
    }
    return NULL;
}

SECStatus
CMMF_EncodeKeyRecRepContent(CMMFKeyRecRepContent      *inKeyRecRep,
			    CRMFEncoderOutputCallback  inCallback,
			    void                      *inArg)
{
    return cmmf_user_encode(inKeyRecRep, inCallback, inArg,
			    CMMFKeyRecRepContentTemplate);
}

CMMFKeyRecRepContent* 
CMMF_CreateKeyRecRepContentFromDER(CERTCertDBHandle *db, const char *buf, 
				   long len)
{
    PLArenaPool          *poolp;
    CMMFKeyRecRepContent *keyRecContent;
    SECStatus             rv;

    poolp = PORT_NewArena(CRMF_DEFAULT_ARENA_SIZE);
    if (poolp == NULL) {
        return NULL;
    }
    keyRecContent = PORT_ArenaZNew(poolp, CMMFKeyRecRepContent);
    if (keyRecContent == NULL) {
        goto loser;
    }
    keyRecContent->poolp = poolp;
    rv = SEC_ASN1Decode(poolp, keyRecContent, CMMFKeyRecRepContentTemplate,
			buf, len);
    if (rv != SECSuccess) {
        goto loser;
    }
    if (keyRecContent->keyPairHist != NULL) {
        while(keyRecContent->keyPairHist[keyRecContent->numKeyPairs] != NULL) {
	    rv = cmmf_decode_process_certified_key_pair(poolp, db,
		       keyRecContent->keyPairHist[keyRecContent->numKeyPairs]);
	    if (rv != SECSuccess) {
	        goto loser;
	    }
	    keyRecContent->numKeyPairs++;
	}
	keyRecContent->allocKeyPairs = keyRecContent->numKeyPairs;
    }
    keyRecContent->isDecoded = PR_TRUE;
    return keyRecContent;
 loser:
    if (poolp != NULL) {
        PORT_FreeArena(poolp, PR_FALSE);
    }
    return NULL;
}

