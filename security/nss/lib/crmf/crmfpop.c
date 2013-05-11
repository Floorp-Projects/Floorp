/* -*- Mode: C; tab-width: 8 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "crmf.h"
#include "crmfi.h"
#include "secasn1.h"
#include "keyhi.h"
#include "cryptohi.h"

#define CRMF_DEFAULT_ALLOC_SIZE 1024

SECStatus
crmf_init_encoder_callback_arg (struct crmfEncoderArg *encoderArg, 
				SECItem               *derDest) 
{
    derDest->data = PORT_ZNewArray(unsigned char, CRMF_DEFAULT_ALLOC_SIZE);
    if (derDest->data == NULL) {
        return SECFailure;
    }
    derDest->len = 0;
    encoderArg->allocatedLen = CRMF_DEFAULT_ALLOC_SIZE;
    encoderArg->buffer = derDest;
    return SECSuccess;

}

/* Caller should release or unmark the pool, instead of doing it here.
** But there are NO callers of this function at present...
*/
SECStatus 
CRMF_CertReqMsgSetRAVerifiedPOP(CRMFCertReqMsg *inCertReqMsg)
{
    SECItem               *dummy;
    CRMFProofOfPossession *pop;
    PLArenaPool           *poolp;
    void                  *mark;

    PORT_Assert(inCertReqMsg != NULL && inCertReqMsg->pop == NULL);
    poolp = inCertReqMsg->poolp;
    mark = PORT_ArenaMark(poolp);
    if (CRMF_CertReqMsgGetPOPType(inCertReqMsg) != crmfNoPOPChoice) {
        return SECFailure;
    }
    pop = PORT_ArenaZNew(poolp, CRMFProofOfPossession);
    if (pop == NULL) {
        goto loser;
    }
    pop->popUsed = crmfRAVerified;
    pop->popChoice.raVerified.data = NULL;
    pop->popChoice.raVerified.len  = 0;
    inCertReqMsg->pop = pop;
    dummy = SEC_ASN1EncodeItem(poolp, &(inCertReqMsg->derPOP),
			       &(pop->popChoice.raVerified),
			       CRMFRAVerifiedTemplate);
    return SECSuccess;
 loser:
    PORT_ArenaRelease(poolp, mark);
    return SECFailure;
}

static SECOidTag
crmf_get_key_sign_tag(SECKEYPublicKey *inPubKey)
{
    /* maintain backward compatibility with older
     * implementations */
    if (inPubKey->keyType == rsaKey) {
        return SEC_OID_PKCS1_SHA1_WITH_RSA_ENCRYPTION;
    }
    return SEC_GetSignatureAlgorithmOidTag(inPubKey->keyType, SEC_OID_UNKNOWN);
}

static SECAlgorithmID*
crmf_create_poposignkey_algid(PLArenaPool      *poolp,
			      SECKEYPublicKey  *inPubKey)
{
    SECAlgorithmID *algID;
    SECOidTag       tag;
    SECStatus       rv;
    void           *mark;

    mark = PORT_ArenaMark(poolp);
    algID = PORT_ArenaZNew(poolp, SECAlgorithmID);
    if (algID == NULL) {
        goto loser;
    }
    tag = crmf_get_key_sign_tag(inPubKey);
    if (tag == SEC_OID_UNKNOWN) {
        goto loser;
    }
    rv = SECOID_SetAlgorithmID(poolp, algID, tag, NULL);
    if (rv != SECSuccess) {
        goto loser;
    }
    PORT_ArenaUnmark(poolp, mark);
    return algID;
 loser:
    PORT_ArenaRelease(poolp, mark);
    return NULL;
}

static CRMFPOPOSigningKeyInput*
crmf_create_poposigningkeyinput(PLArenaPool *poolp, CERTCertificate *inCert,
				CRMFMACPasswordCallback fn, void *arg)
{
  /* PSM isn't going to do this, so we'll fail here for now.*/
     return NULL;
}

void
crmf_generic_encoder_callback(void *arg, const char* buf, unsigned long len,
			      int depth, SEC_ASN1EncodingPart data_kind)
{
    struct crmfEncoderArg *encoderArg = (struct crmfEncoderArg*)arg;
    unsigned char *cursor;
    
   if (encoderArg->buffer->len + len > encoderArg->allocatedLen) {
        int newSize = encoderArg->buffer->len+CRMF_DEFAULT_ALLOC_SIZE;
        void *dummy = PORT_Realloc(encoderArg->buffer->data, newSize);
	if (dummy == NULL) {
	    /* I really want to return an error code here */
	    PORT_Assert(0);
	    return;
	}
	encoderArg->buffer->data = dummy;
	encoderArg->allocatedLen = newSize;
    }
    cursor = &(encoderArg->buffer->data[encoderArg->buffer->len]);
    PORT_Memcpy (cursor, buf, len);
    encoderArg->buffer->len += len;    
}

static SECStatus
crmf_encode_certreq(CRMFCertRequest *inCertReq, SECItem *derDest)
{
    struct crmfEncoderArg encoderArg;
    SECStatus             rv;
  
    rv = crmf_init_encoder_callback_arg (&encoderArg, derDest);
    if (rv != SECSuccess) {
        return SECFailure;
    }
    return SEC_ASN1Encode(inCertReq, CRMFCertRequestTemplate, 
			  crmf_generic_encoder_callback, &encoderArg);
}

static SECStatus
crmf_sign_certreq(PLArenaPool        *poolp,
		  CRMFPOPOSigningKey *crmfSignKey, 
		  CRMFCertRequest    *certReq,
		  SECKEYPrivateKey   *inKey,
		  SECAlgorithmID     *inAlgId)
{
    SECItem                      derCertReq = { siBuffer, NULL, 0 };
    SECItem                      certReqSig = { siBuffer, NULL, 0 };
    SECStatus                    rv = SECSuccess;

    rv = crmf_encode_certreq(certReq, &derCertReq);
    if (rv != SECSuccess) {
       goto loser;
    }
    rv = SEC_SignData(&certReqSig, derCertReq.data, derCertReq.len,
		      inKey,SECOID_GetAlgorithmTag(inAlgId));
    if (rv != SECSuccess) {
        goto loser;
    }
 
    /* Now make it a part of the POPOSigningKey */
    rv = SECITEM_CopyItem(poolp, &(crmfSignKey->signature), &certReqSig);
    /* Convert this length to number of bits */
    crmfSignKey->signature.len <<= 3; 
   
 loser:
    if (derCertReq.data != NULL) {
        PORT_Free(derCertReq.data);
    }
    if (certReqSig.data != NULL) {
        PORT_Free(certReqSig.data);
    }
    return rv;
}

static SECStatus
crmf_create_poposignkey(PLArenaPool             *poolp,
			CRMFCertReqMsg          *inCertReqMsg, 
			CRMFPOPOSigningKeyInput *signKeyInput, 
			SECKEYPrivateKey        *inPrivKey,
			SECAlgorithmID          *inAlgID,
			CRMFPOPOSigningKey      *signKey) 
{
    CRMFCertRequest    *certReq;
    void               *mark;
    PRBool              useSignKeyInput;
    SECStatus           rv;
    
    PORT_Assert(inCertReqMsg != NULL && inCertReqMsg->certReq != NULL);
    mark = PORT_ArenaMark(poolp);
    if (signKey == NULL) {
        goto loser;
    }
    certReq = inCertReqMsg->certReq;
    useSignKeyInput = !(CRMF_DoesRequestHaveField(certReq,crmfSubject)  &&
			CRMF_DoesRequestHaveField(certReq,crmfPublicKey));

    if (useSignKeyInput) {
        goto loser; 
    } else {
        rv = crmf_sign_certreq(poolp, signKey, certReq,inPrivKey, inAlgID);
	if (rv != SECSuccess) {
	    goto loser;
	}
    }
    PORT_ArenaUnmark(poolp,mark);
    return SECSuccess;
 loser:
    PORT_ArenaRelease(poolp,mark);
    return SECFailure;
}

SECStatus
CRMF_CertReqMsgSetSignaturePOP(CRMFCertReqMsg   *inCertReqMsg,
			       SECKEYPrivateKey *inPrivKey,
			       SECKEYPublicKey  *inPubKey,
			       CERTCertificate  *inCertForInput,
			       CRMFMACPasswordCallback  fn,
			       void                    *arg)
{
    SECAlgorithmID  *algID;
    PLArenaPool     *poolp;
    SECItem          derTemp = {siBuffer, NULL, 0};
    void            *mark;
    SECStatus        rv;
    CRMFPOPOSigningKeyInput *signKeyInput = NULL;
    CRMFCertRequest         *certReq;
    CRMFProofOfPossession   *pop;
    struct crmfEncoderArg    encoderArg;

    PORT_Assert(inCertReqMsg != NULL && inCertReqMsg->certReq != NULL &&
		inCertReqMsg->pop == NULL);
    certReq = inCertReqMsg->certReq;
    if (CRMF_CertReqMsgGetPOPType(inCertReqMsg) != crmfNoPOPChoice || 
	!CRMF_DoesRequestHaveField(certReq, crmfPublicKey)) {
        return SECFailure;
    } 
    poolp = inCertReqMsg->poolp;
    mark = PORT_ArenaMark(poolp);
    algID = crmf_create_poposignkey_algid(poolp, inPubKey);

    if(!CRMF_DoesRequestHaveField(certReq,crmfSubject)) {
        signKeyInput = crmf_create_poposigningkeyinput(poolp, inCertForInput,
						       fn, arg);
	if (signKeyInput == NULL) {
	    goto loser;
	}
    }

    pop = PORT_ArenaZNew(poolp, CRMFProofOfPossession);
    if (pop == NULL) {
        goto loser;
    }
    
    rv = crmf_create_poposignkey(poolp, inCertReqMsg, 
				 signKeyInput, inPrivKey, algID,
				 &(pop->popChoice.signature));
    if (rv != SECSuccess) {
        goto loser;
    }

    pop->popUsed = crmfSignature;
    pop->popChoice.signature.algorithmIdentifier = algID;
    inCertReqMsg->pop = pop;
  
    rv = crmf_init_encoder_callback_arg (&encoderArg, &derTemp);
    if (rv != SECSuccess) {
        goto loser;
    }
    rv = SEC_ASN1Encode(&pop->popChoice.signature, 
			CRMFPOPOSigningKeyTemplate,
			crmf_generic_encoder_callback, &encoderArg);
    if (rv != SECSuccess) {
        goto loser;
    }
    rv = SECITEM_CopyItem(poolp, &(inCertReqMsg->derPOP), &derTemp);
    if (rv != SECSuccess) {
        goto loser;
    }
    PORT_Free (derTemp.data);
    PORT_ArenaUnmark(poolp,mark);
    return SECSuccess;

 loser:
    PORT_ArenaRelease(poolp,mark);
    if (derTemp.data != NULL) {
        PORT_Free(derTemp.data);
    }
    return SECFailure;
}

static const SEC_ASN1Template*
crmf_get_popoprivkey_subtemplate(CRMFPOPOPrivKey *inPrivKey) 
{
    const SEC_ASN1Template *retTemplate = NULL;

    switch (inPrivKey->messageChoice) {
    case crmfThisMessage:
        retTemplate = CRMFThisMessageTemplate;
	break;
    case crmfSubsequentMessage:
        retTemplate = CRMFSubsequentMessageTemplate;
	break;
    case crmfDHMAC:
        retTemplate = CRMFDHMACTemplate;
	break;
    default:
        retTemplate = NULL;
    }
    return retTemplate;
}

static SECStatus
crmf_encode_popoprivkey(PLArenaPool            *poolp,
			CRMFCertReqMsg         *inCertReqMsg,
			CRMFPOPOPrivKey        *popoPrivKey,
			const SEC_ASN1Template *privKeyTemplate)
{
    struct crmfEncoderArg   encoderArg;
    SECItem                 derTemp = { siBuffer, NULL, 0 };
    SECStatus               rv;
    void                   *mark;
    const SEC_ASN1Template *subDerTemplate;

    mark = PORT_ArenaMark(poolp);
    rv = crmf_init_encoder_callback_arg(&encoderArg, &derTemp);
    if (rv != SECSuccess) {
        goto loser;
    }
    subDerTemplate = crmf_get_popoprivkey_subtemplate(popoPrivKey);
    /* We've got a union, so a pointer to one item is a pointer to 
     * all the items in the union.
     */
    rv = SEC_ASN1Encode(&popoPrivKey->message.thisMessage, 
			subDerTemplate,
			crmf_generic_encoder_callback, &encoderArg);
    if (rv != SECSuccess) {
        goto loser;
    }
    if (encoderArg.allocatedLen > derTemp.len+2) {
        void *dummy = PORT_Realloc(derTemp.data, derTemp.len+2);
	if (dummy == NULL) {
	    goto loser;
	}
	derTemp.data = dummy;
    }
    PORT_Memmove(&derTemp.data[2], &derTemp.data[0], derTemp.len);
    /* I couldn't figure out how to get the ASN1 encoder to implicitly
     * tag an implicitly tagged der blob.  So I'm putting in the outter-
     * most tag myself. -javi
     */
    derTemp.data[0] = (unsigned char)privKeyTemplate->kind;
    derTemp.data[1] = (unsigned char)derTemp.len;
    derTemp.len += 2;
    rv = SECITEM_CopyItem(poolp, &inCertReqMsg->derPOP, &derTemp);
    if (rv != SECSuccess) {
        goto loser;
    }
    PORT_Free(derTemp.data);
    PORT_ArenaUnmark(poolp, mark);
    return SECSuccess;
 loser:
    PORT_ArenaRelease(poolp, mark);
    if (derTemp.data) {
        PORT_Free(derTemp.data);
    }
    return SECFailure;
}

static const SEC_ASN1Template*
crmf_get_template_for_privkey(CRMFPOPChoice inChoice) 
{
    switch (inChoice) {
    case crmfKeyAgreement:
        return CRMFPOPOKeyAgreementTemplate;
    case crmfKeyEncipherment:
        return CRMFPOPOKeyEnciphermentTemplate;
    default:
        break;
    }
    return NULL;
}

static SECStatus
crmf_add_privkey_thismessage(CRMFCertReqMsg *inCertReqMsg, SECItem *encPrivKey,
			     CRMFPOPChoice inChoice)
{
    PLArenaPool           *poolp;
    void                  *mark;
    CRMFPOPOPrivKey       *popoPrivKey;
    CRMFProofOfPossession *pop;
    SECStatus              rv;

    PORT_Assert(inCertReqMsg != NULL && encPrivKey != NULL);
    poolp = inCertReqMsg->poolp;
    mark = PORT_ArenaMark(poolp);
    pop = PORT_ArenaZNew(poolp, CRMFProofOfPossession);
    if (pop == NULL) {
        goto loser;
    }
    pop->popUsed = inChoice;
    /* popChoice is a union, so getting a pointer to one
     * field gives me a pointer to the other fields as
     * well.  This in essence points to both 
     * pop->popChoice.keyEncipherment and
     * pop->popChoice.keyAgreement
     */
    popoPrivKey = &pop->popChoice.keyEncipherment;

    rv = SECITEM_CopyItem(poolp, &(popoPrivKey->message.thisMessage),
			  encPrivKey);
    if (rv != SECSuccess) {
        goto loser;
    }
    popoPrivKey->message.thisMessage.len <<= 3;
    popoPrivKey->messageChoice = crmfThisMessage;
    inCertReqMsg->pop = pop;
    rv = crmf_encode_popoprivkey(poolp, inCertReqMsg, popoPrivKey,
				 crmf_get_template_for_privkey(inChoice));
    if (rv != SECSuccess) {
        goto loser;
    }
    PORT_ArenaUnmark(poolp, mark);
    return SECSuccess;
    
 loser:
    PORT_ArenaRelease(poolp, mark);
    return SECFailure;
}

static SECStatus
crmf_add_privkey_dhmac(CRMFCertReqMsg *inCertReqMsg, SECItem *dhmac,
                             CRMFPOPChoice inChoice)
{
    PLArenaPool           *poolp;
    void                  *mark;
    CRMFPOPOPrivKey       *popoPrivKey;
    CRMFProofOfPossession *pop;
    SECStatus              rv;

    PORT_Assert(inCertReqMsg != NULL && dhmac != NULL);
    poolp = inCertReqMsg->poolp;
    mark = PORT_ArenaMark(poolp);
    pop = PORT_ArenaZNew(poolp, CRMFProofOfPossession);
    if (pop == NULL) {
        goto loser;
    }
    pop->popUsed = inChoice;
    popoPrivKey = &pop->popChoice.keyAgreement;

    rv = SECITEM_CopyItem(poolp, &(popoPrivKey->message.dhMAC),
                          dhmac);
    if (rv != SECSuccess) {
        goto loser;
    }
    popoPrivKey->message.dhMAC.len <<= 3;
    popoPrivKey->messageChoice = crmfDHMAC;
    inCertReqMsg->pop = pop;
    rv = crmf_encode_popoprivkey(poolp, inCertReqMsg, popoPrivKey,
                                 crmf_get_template_for_privkey(inChoice));
    if (rv != SECSuccess) {
        goto loser;
    }
    PORT_ArenaUnmark(poolp, mark);
    return SECSuccess;
    
 loser:
    PORT_ArenaRelease(poolp, mark);
    return SECFailure;
}

static SECStatus
crmf_add_privkey_subseqmessage(CRMFCertReqMsg        *inCertReqMsg,
			       CRMFSubseqMessOptions  subsequentMessage,
			       CRMFPOPChoice          inChoice)
{
    void                  *mark;
    PLArenaPool           *poolp;
    CRMFProofOfPossession *pop;
    CRMFPOPOPrivKey       *popoPrivKey;
    SECStatus              rv;
    const SEC_ASN1Template *privKeyTemplate;

    if (subsequentMessage == crmfNoSubseqMess) {
        return SECFailure;
    }
    poolp = inCertReqMsg->poolp;
    mark = PORT_ArenaMark(poolp);
    pop = PORT_ArenaZNew(poolp, CRMFProofOfPossession);
    if (pop == NULL) {
        goto loser;
    }

    pop->popUsed = inChoice;
    /* 
     * We have a union, so a pointer to one member of the union
     * is also a member to another member of that same union.
     */
    popoPrivKey = &pop->popChoice.keyEncipherment;

    switch (subsequentMessage) {
    case crmfEncrCert:
        rv = crmf_encode_integer(poolp, 
				 &(popoPrivKey->message.subsequentMessage),
				 0);
	break;
    case crmfChallengeResp:
        rv = crmf_encode_integer(poolp,
				 &(popoPrivKey->message.subsequentMessage),
				 1);
	break;
    default:
        goto loser;
    }
    if (rv != SECSuccess) {
        goto loser;
    }
    popoPrivKey->messageChoice = crmfSubsequentMessage;
    privKeyTemplate = crmf_get_template_for_privkey(inChoice);
    inCertReqMsg->pop = pop;
    rv = crmf_encode_popoprivkey(poolp, inCertReqMsg, popoPrivKey,
				 privKeyTemplate);

    if (rv != SECSuccess) {
        goto loser;
    }
    PORT_ArenaUnmark(poolp, mark);
    return SECSuccess;
 loser:
    PORT_ArenaRelease(poolp, mark);
    return SECFailure;
}

SECStatus 
CRMF_CertReqMsgSetKeyEnciphermentPOP(CRMFCertReqMsg        *inCertReqMsg,
				     CRMFPOPOPrivKeyChoice  inKeyChoice,
				     CRMFSubseqMessOptions  subseqMess,
				     SECItem               *encPrivKey)
{
    SECStatus rv;

    PORT_Assert(inCertReqMsg != NULL && inCertReqMsg->pop == NULL);
    if (CRMF_CertReqMsgGetPOPType(inCertReqMsg) != crmfNoPOPChoice) {
        return SECFailure;
    }
    switch (inKeyChoice) {    
    case crmfThisMessage:
        rv = crmf_add_privkey_thismessage(inCertReqMsg, encPrivKey,
					  crmfKeyEncipherment);
	break;
    case crmfSubsequentMessage:
        rv = crmf_add_privkey_subseqmessage(inCertReqMsg, subseqMess, 
					    crmfKeyEncipherment);
        break;
    case crmfDHMAC:
    default:
        rv = SECFailure;
    }
    return rv;
}

SECStatus 
CRMF_CertReqMsgSetKeyAgreementPOP (CRMFCertReqMsg        *inCertReqMsg,
				   CRMFPOPOPrivKeyChoice  inKeyChoice,
				   CRMFSubseqMessOptions  subseqMess,
				   SECItem               *encPrivKey)
{
    SECStatus rv;

    PORT_Assert(inCertReqMsg != NULL && inCertReqMsg->pop == NULL);
    switch (inKeyChoice) {    
    case crmfThisMessage:
        rv = crmf_add_privkey_thismessage(inCertReqMsg, encPrivKey,
					  crmfKeyAgreement);
	break;
    case crmfSubsequentMessage:
        rv = crmf_add_privkey_subseqmessage(inCertReqMsg, subseqMess, 
					    crmfKeyAgreement);
	break;
    case crmfDHMAC:
        /* In this case encPrivKey should be the calculated dhMac
         * as specified in RFC 2511 */
        rv = crmf_add_privkey_dhmac(inCertReqMsg, encPrivKey,
                                    crmfKeyAgreement);
        break;
    default:
        rv = SECFailure;
    }
    return rv;
}

