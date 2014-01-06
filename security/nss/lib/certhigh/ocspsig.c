/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "plarena.h"

#include "seccomon.h"
#include "secitem.h"
#include "secasn1.h"
#include "secder.h"
#include "cert.h"
#include "secerr.h"
#include "secoid.h"
#include "sechash.h"
#include "keyhi.h"
#include "cryptohi.h"
#include "ocsp.h"
#include "ocspti.h"
#include "ocspi.h"
#include "pk11pub.h"


extern const SEC_ASN1Template ocsp_ResponderIDByNameTemplate[];
extern const SEC_ASN1Template ocsp_ResponderIDByKeyTemplate[];
extern const SEC_ASN1Template ocsp_OCSPResponseTemplate[];

ocspCertStatus*
ocsp_CreateCertStatus(PLArenaPool *arena,
                      ocspCertStatusType status,
                      PRTime revocationTime)
{
    ocspCertStatus *cs;

    if (!arena) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return NULL;
    }

    switch (status) {
        case ocspCertStatus_good:
        case ocspCertStatus_unknown:
        case ocspCertStatus_revoked:
            break;
        default:
            PORT_SetError(SEC_ERROR_INVALID_ARGS);
            return NULL;
    }
    
    cs = PORT_ArenaZNew(arena, ocspCertStatus);
    if (!cs)
        return NULL;
    cs->certStatusType = status;
    switch (status) {
        case ocspCertStatus_good:
            cs->certStatusInfo.goodInfo = SECITEM_AllocItem(arena, NULL, 0);
            if (!cs->certStatusInfo.goodInfo)
                return NULL;
            break;
        case ocspCertStatus_unknown:
            cs->certStatusInfo.unknownInfo = SECITEM_AllocItem(arena, NULL, 0);
            if (!cs->certStatusInfo.unknownInfo)
                return NULL;
            break;
        case ocspCertStatus_revoked:
            cs->certStatusInfo.revokedInfo =
                PORT_ArenaZNew(arena, ocspRevokedInfo);
            if (!cs->certStatusInfo.revokedInfo)
                return NULL;
            cs->certStatusInfo.revokedInfo->revocationReason =
                SECITEM_AllocItem(arena, NULL, 0);
            if (!cs->certStatusInfo.revokedInfo->revocationReason)
                return NULL;
            if (DER_TimeToGeneralizedTimeArena(arena,
                    &cs->certStatusInfo.revokedInfo->revocationTime,
                    revocationTime) != SECSuccess)
                return NULL;
            break;
        default:
            PORT_Assert(PR_FALSE);
    }
    return cs;
}

static const SEC_ASN1Template mySEC_EnumeratedTemplate[] = {
    { SEC_ASN1_ENUMERATED, 0, NULL, sizeof(SECItem) }
};

static const SEC_ASN1Template mySEC_PointerToEnumeratedTemplate[] = {
    { SEC_ASN1_POINTER, 0, mySEC_EnumeratedTemplate }
};

static const SEC_ASN1Template ocsp_EncodeRevokedInfoTemplate[] = {
    { SEC_ASN1_GENERALIZED_TIME,
        offsetof(ocspRevokedInfo, revocationTime) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_EXPLICIT |
      SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC| 0,
        offsetof(ocspRevokedInfo, revocationReason),
        mySEC_PointerToEnumeratedTemplate },
    { 0 }
};

static const SEC_ASN1Template ocsp_PointerToEncodeRevokedInfoTemplate[] = {
    { SEC_ASN1_POINTER, 0,
      ocsp_EncodeRevokedInfoTemplate }
};

static const SEC_ASN1Template mySEC_NullTemplate[] = {
    { SEC_ASN1_NULL, 0, NULL, sizeof(SECItem) }
};

static const SEC_ASN1Template ocsp_CertStatusTemplate[] = {
    { SEC_ASN1_CHOICE, offsetof(ocspCertStatus, certStatusType),
        0, sizeof(ocspCertStatus) },
    { SEC_ASN1_CONTEXT_SPECIFIC | 0,
        0, mySEC_NullTemplate, ocspCertStatus_good },
    { SEC_ASN1_EXPLICIT | SEC_ASN1_CONSTRUCTED |
      SEC_ASN1_CONTEXT_SPECIFIC | 1,
        offsetof(ocspCertStatus, certStatusInfo.revokedInfo),
        ocsp_PointerToEncodeRevokedInfoTemplate, ocspCertStatus_revoked },
    { SEC_ASN1_CONTEXT_SPECIFIC | 2,
        0, mySEC_NullTemplate, ocspCertStatus_unknown },
    { 0 }
};

static const SEC_ASN1Template mySECOID_AlgorithmIDTemplate[] = {
    { SEC_ASN1_SEQUENCE,
          0, NULL, sizeof(SECAlgorithmID) },
    { SEC_ASN1_OBJECT_ID,
          offsetof(SECAlgorithmID,algorithm), },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_ANY,
          offsetof(SECAlgorithmID,parameters), },
    { 0, }
};

static const SEC_ASN1Template mySEC_AnyTemplate[] = {
    { SEC_ASN1_ANY | SEC_ASN1_MAY_STREAM, 0, NULL, sizeof(SECItem) }
};

static const SEC_ASN1Template mySEC_SequenceOfAnyTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF, 0, mySEC_AnyTemplate }
};

static const SEC_ASN1Template mySEC_PointerToSequenceOfAnyTemplate[] = {
    { SEC_ASN1_POINTER, 0, mySEC_SequenceOfAnyTemplate }
};

static const SEC_ASN1Template mySEC_IntegerTemplate[] = {
    { SEC_ASN1_INTEGER, 0, NULL, sizeof(SECItem) }
};

static const SEC_ASN1Template mySEC_PointerToIntegerTemplate[] = {
    { SEC_ASN1_POINTER, 0, mySEC_IntegerTemplate }
};

static const SEC_ASN1Template mySEC_GeneralizedTimeTemplate[] = {
    { SEC_ASN1_GENERALIZED_TIME | SEC_ASN1_MAY_STREAM, 0, NULL, sizeof(SECItem)}
};

static const SEC_ASN1Template mySEC_PointerToGeneralizedTimeTemplate[] = {
    { SEC_ASN1_POINTER, 0, mySEC_GeneralizedTimeTemplate }
};

static const SEC_ASN1Template ocsp_myCertIDTemplate[] = {
    { SEC_ASN1_SEQUENCE,
        0, NULL, sizeof(CERTOCSPCertID) },
    { SEC_ASN1_INLINE,
        offsetof(CERTOCSPCertID, hashAlgorithm),
        mySECOID_AlgorithmIDTemplate },
    { SEC_ASN1_OCTET_STRING,
        offsetof(CERTOCSPCertID, issuerNameHash) },
    { SEC_ASN1_OCTET_STRING,
        offsetof(CERTOCSPCertID, issuerKeyHash) },
    { SEC_ASN1_INTEGER,
        offsetof(CERTOCSPCertID, serialNumber) },
    { 0 }
};

static const SEC_ASN1Template myCERT_CertExtensionTemplate[] = {
    { SEC_ASN1_SEQUENCE,
          0, NULL, sizeof(CERTCertExtension) },
    { SEC_ASN1_OBJECT_ID,
          offsetof(CERTCertExtension,id) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_BOOLEAN,             /* XXX DER_DEFAULT */
          offsetof(CERTCertExtension,critical) },
    { SEC_ASN1_OCTET_STRING,
          offsetof(CERTCertExtension,value) },
    { 0, }
};

static const SEC_ASN1Template myCERT_SequenceOfCertExtensionTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF, 0, myCERT_CertExtensionTemplate }
};

static const SEC_ASN1Template myCERT_PointerToSequenceOfCertExtensionTemplate[] = {
    { SEC_ASN1_POINTER, 0, myCERT_SequenceOfCertExtensionTemplate }
};

static const SEC_ASN1Template ocsp_mySingleResponseTemplate[] = {
    { SEC_ASN1_SEQUENCE,
        0, NULL, sizeof(CERTOCSPSingleResponse) },
    { SEC_ASN1_POINTER,
        offsetof(CERTOCSPSingleResponse, certID),
        ocsp_myCertIDTemplate },
    { SEC_ASN1_ANY,
        offsetof(CERTOCSPSingleResponse, derCertStatus) },
    { SEC_ASN1_GENERALIZED_TIME,
        offsetof(CERTOCSPSingleResponse, thisUpdate) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_EXPLICIT |
      SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 0,
        offsetof(CERTOCSPSingleResponse, nextUpdate),
        mySEC_PointerToGeneralizedTimeTemplate },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_EXPLICIT |
      SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 1,
        offsetof(CERTOCSPSingleResponse, singleExtensions),
        myCERT_PointerToSequenceOfCertExtensionTemplate },
    { 0 }
};

static const SEC_ASN1Template ocsp_myResponseDataTemplate[] = {
    { SEC_ASN1_SEQUENCE,
        0, NULL, sizeof(ocspResponseData) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_EXPLICIT |           /* XXX DER_DEFAULT */
      SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 0,
        offsetof(ocspResponseData, version),
        mySEC_PointerToIntegerTemplate },
    { SEC_ASN1_ANY,
        offsetof(ocspResponseData, derResponderID) },
    { SEC_ASN1_GENERALIZED_TIME,
        offsetof(ocspResponseData, producedAt) },
    { SEC_ASN1_SEQUENCE_OF,
        offsetof(ocspResponseData, responses),
        ocsp_mySingleResponseTemplate },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_EXPLICIT |
      SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 1,
        offsetof(ocspResponseData, responseExtensions),
        myCERT_PointerToSequenceOfCertExtensionTemplate },
    { 0 }
};


static const SEC_ASN1Template ocsp_EncodeBasicOCSPResponseTemplate[] = {
    { SEC_ASN1_SEQUENCE,
        0, NULL, sizeof(ocspBasicOCSPResponse) },
    { SEC_ASN1_POINTER,
        offsetof(ocspBasicOCSPResponse, tbsResponseData),
        ocsp_myResponseDataTemplate },
    { SEC_ASN1_INLINE,
        offsetof(ocspBasicOCSPResponse, responseSignature.signatureAlgorithm),
        mySECOID_AlgorithmIDTemplate },
    { SEC_ASN1_BIT_STRING,
        offsetof(ocspBasicOCSPResponse, responseSignature.signature) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_EXPLICIT |
      SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 0,
        offsetof(ocspBasicOCSPResponse, responseSignature.derCerts),
        mySEC_PointerToSequenceOfAnyTemplate },
    { 0 }
};

static CERTOCSPSingleResponse*
ocsp_CreateSingleResponse(PLArenaPool *arena,
                          CERTOCSPCertID *id, ocspCertStatus *status,
                          PRTime thisUpdate, const PRTime *nextUpdate)
{
    CERTOCSPSingleResponse *sr;

    if (!arena || !id || !status) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return NULL;
    }

    sr = PORT_ArenaZNew(arena, CERTOCSPSingleResponse);
    if (!sr)
        return NULL;
    sr->arena = arena;
    sr->certID = id;
    sr->certStatus = status;
    if (DER_TimeToGeneralizedTimeArena(arena, &sr->thisUpdate, thisUpdate)
             != SECSuccess)
        return NULL;
    sr->nextUpdate = NULL;
    if (nextUpdate) {
        sr->nextUpdate = SECITEM_AllocItem(arena, NULL, 0);
        if (!sr->nextUpdate)
            return NULL;
        if (DER_TimeToGeneralizedTimeArena(arena, sr->nextUpdate, *nextUpdate)
             != SECSuccess)
            return NULL;
    }

    sr->singleExtensions = PORT_ArenaNewArray(arena, CERTCertExtension*, 1);
    if (!sr->singleExtensions)
        return NULL;

    sr->singleExtensions[0] = NULL;
    
    if (!SEC_ASN1EncodeItem(arena, &sr->derCertStatus,
                            status, ocsp_CertStatusTemplate))
        return NULL;

    return sr;
}

CERTOCSPSingleResponse*
CERT_CreateOCSPSingleResponseGood(PLArenaPool *arena,
                                  CERTOCSPCertID *id,
                                  PRTime thisUpdate,
                                  const PRTime *nextUpdate)
{
    ocspCertStatus * cs;
    if (!arena) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return NULL;
    }
    cs = ocsp_CreateCertStatus(arena, ocspCertStatus_good, 0);
    if (!cs)
        return NULL;
    return ocsp_CreateSingleResponse(arena, id, cs, thisUpdate, nextUpdate);
}

CERTOCSPSingleResponse*
CERT_CreateOCSPSingleResponseUnknown(PLArenaPool *arena,
                                     CERTOCSPCertID *id,
                                     PRTime thisUpdate,
                                     const PRTime *nextUpdate)
{
    ocspCertStatus * cs;
    if (!arena) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return NULL;
    }
    cs = ocsp_CreateCertStatus(arena, ocspCertStatus_unknown, 0);
    if (!cs)
        return NULL;
    return ocsp_CreateSingleResponse(arena, id, cs, thisUpdate, nextUpdate);
}

CERTOCSPSingleResponse*
CERT_CreateOCSPSingleResponseRevoked(
    PLArenaPool *arena,
    CERTOCSPCertID *id,
    PRTime thisUpdate,
    const PRTime *nextUpdate,
    PRTime revocationTime,
    const CERTCRLEntryReasonCode* revocationReason)
{
    ocspCertStatus * cs;
    /* revocationReason is not yet supported, so it must be NULL. */
    if (!arena || revocationReason) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return NULL;
    }
    cs = ocsp_CreateCertStatus(arena, ocspCertStatus_revoked, revocationTime);
    if (!cs)
        return NULL;
    return ocsp_CreateSingleResponse(arena, id, cs, thisUpdate, nextUpdate);
}

/* responderCert == 0 means:
 * create a response with an invalid signature (for testing purposes) */
SECItem*
CERT_CreateEncodedOCSPSuccessResponse(
    PLArenaPool *arena,
    CERTCertificate *responderCert,
    CERTOCSPResponderIDType responderIDType,
    PRTime producedAt,
    CERTOCSPSingleResponse **responses,
    void *wincx)
{
    PLArenaPool *tmpArena;
    ocspResponseData *rd = NULL;
    ocspResponderID *rid = NULL;
    const SEC_ASN1Template *responderIDTemplate = NULL;
    ocspBasicOCSPResponse *br = NULL;
    ocspResponseBytes *rb = NULL;
    CERTOCSPResponse *response = NULL;
    
    SECOidTag algID;
    SECOidData *od = NULL;
    SECKEYPrivateKey *privKey = NULL;
    SECItem *result = NULL;
  
    if (!arena || !responses) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return NULL;
    }
    if (responderIDType != ocspResponderID_byName &&
        responderIDType != ocspResponderID_byKey) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return NULL;
    }

    tmpArena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (!tmpArena)
        return NULL;

    rd = PORT_ArenaZNew(tmpArena, ocspResponseData);
    if (!rd)
        goto done;
    rid = PORT_ArenaZNew(tmpArena, ocspResponderID);
    if (!rid)
        goto done;
    br = PORT_ArenaZNew(tmpArena, ocspBasicOCSPResponse);
    if (!br)
        goto done;
    rb = PORT_ArenaZNew(tmpArena, ocspResponseBytes);
    if (!rb)
        goto done;
    response = PORT_ArenaZNew(tmpArena, CERTOCSPResponse);
    if (!response)
        goto done;
    
    rd->version.data=NULL;
    rd->version.len=0;
    rd->responseExtensions = NULL;
    rd->responses = responses;
    if (DER_TimeToGeneralizedTimeArena(tmpArena, &rd->producedAt, producedAt)
            != SECSuccess)
        goto done;

    if (!responderCert) {
	/* use invalid signature for testing purposes */
	unsigned char dummyChar = 'd';
	SECItem dummy;

	dummy.len = 1;
	dummy.data = &dummyChar;

	/* it's easier to produdce a keyHash out of nowhere,
	 * than to produce an encoded subject,
	 * so for our dummy response we always use byKey
	 */
	
	rid->responderIDType = ocspResponderID_byKey;
	if (!ocsp_DigestValue(tmpArena, SEC_OID_SHA1, &rid->responderIDValue.keyHash,
			      &dummy))
	    goto done;

	if (!SEC_ASN1EncodeItem(tmpArena, &rd->derResponderID, rid,
				ocsp_ResponderIDByKeyTemplate))
	    goto done;

	br->tbsResponseData = rd;

	if (!SEC_ASN1EncodeItem(tmpArena, &br->tbsResponseDataDER, br->tbsResponseData,
				ocsp_myResponseDataTemplate))
	    goto done;

	br->responseSignature.derCerts = PORT_ArenaNewArray(tmpArena, SECItem*, 1);
	if (!br->responseSignature.derCerts)
	    goto done;
	br->responseSignature.derCerts[0] = NULL;

	algID = SEC_GetSignatureAlgorithmOidTag(rsaKey, SEC_OID_SHA1);
	if (algID == SEC_OID_UNKNOWN)
	    goto done;

	/* match the regular signature code, which doesn't use the arena */
	if (!SECITEM_AllocItem(NULL, &br->responseSignature.signature, 1))
	    goto done;
	PORT_Memcpy(br->responseSignature.signature.data, &dummyChar, 1);

	/* convert len-in-bytes to len-in-bits */
	br->responseSignature.signature.len = br->responseSignature.signature.len << 3;
    }
    else {
	rid->responderIDType = responderIDType;
	if (responderIDType == ocspResponderID_byName) {
	    responderIDTemplate = ocsp_ResponderIDByNameTemplate;
	    if (CERT_CopyName(tmpArena, &rid->responderIDValue.name,
			    &responderCert->subject) != SECSuccess)
		goto done;
	}
	else {
	    responderIDTemplate = ocsp_ResponderIDByKeyTemplate;
	    if (!CERT_GetSubjectPublicKeyDigest(tmpArena, responderCert,
				SEC_OID_SHA1, &rid->responderIDValue.keyHash))
		goto done;
	}

	if (!SEC_ASN1EncodeItem(tmpArena, &rd->derResponderID, rid,
		responderIDTemplate))
	    goto done;

	br->tbsResponseData = rd;

	if (!SEC_ASN1EncodeItem(tmpArena, &br->tbsResponseDataDER, br->tbsResponseData,
		ocsp_myResponseDataTemplate))
	    goto done;

	br->responseSignature.derCerts = PORT_ArenaNewArray(tmpArena, SECItem*, 1);
	if (!br->responseSignature.derCerts)
	    goto done;
	br->responseSignature.derCerts[0] = NULL;

	privKey = PK11_FindKeyByAnyCert(responderCert, wincx);
	if (!privKey)
	    goto done;

	algID = SEC_GetSignatureAlgorithmOidTag(privKey->keyType, SEC_OID_SHA1);
	if (algID == SEC_OID_UNKNOWN)
	    goto done;

	if (SEC_SignData(&br->responseSignature.signature,
			    br->tbsResponseDataDER.data, br->tbsResponseDataDER.len,
			    privKey, algID)
		!= SECSuccess)
	    goto done;

	/* convert len-in-bytes to len-in-bits */
	br->responseSignature.signature.len = br->responseSignature.signature.len << 3;

	/* br->responseSignature.signature wasn't allocated from arena,
	* we must free it when done. */
    }

    if (SECOID_SetAlgorithmID(tmpArena, &br->responseSignature.signatureAlgorithm, algID, 0)
	    != SECSuccess)
	goto done;

    if (!SEC_ASN1EncodeItem(tmpArena, &rb->response, br,
                            ocsp_EncodeBasicOCSPResponseTemplate))
        goto done;

    rb->responseTypeTag = SEC_OID_PKIX_OCSP_BASIC_RESPONSE;

    od = SECOID_FindOIDByTag(rb->responseTypeTag);
    if (!od)
        goto done;

    rb->responseType = od->oid;
    rb->decodedResponse.basic = br;

    response->arena = tmpArena;
    response->responseBytes = rb;
    response->statusValue = ocspResponse_successful;

    if (!SEC_ASN1EncodeInteger(tmpArena, &response->responseStatus,
                               response->statusValue))
        goto done;

    result = SEC_ASN1EncodeItem(arena, NULL, response, ocsp_OCSPResponseTemplate);

done:
    if (privKey)
        SECKEY_DestroyPrivateKey(privKey);
    if (br->responseSignature.signature.data)
        SECITEM_FreeItem(&br->responseSignature.signature, PR_FALSE);
    PORT_FreeArena(tmpArena, PR_FALSE);

    return result;
}

static const SEC_ASN1Template ocsp_OCSPErrorResponseTemplate[] = {
    { SEC_ASN1_SEQUENCE,
        0, NULL, sizeof(CERTOCSPResponse) },
    { SEC_ASN1_ENUMERATED,
        offsetof(CERTOCSPResponse, responseStatus) },
    { 0, 0,
        mySEC_NullTemplate },
    { 0 }
};

SECItem*
CERT_CreateEncodedOCSPErrorResponse(PLArenaPool *arena, int error)
{
    CERTOCSPResponse response;
    SECItem *result = NULL;

    switch (error) {
        case SEC_ERROR_OCSP_MALFORMED_REQUEST:
            response.statusValue = ocspResponse_malformedRequest;
            break;
        case SEC_ERROR_OCSP_SERVER_ERROR:
            response.statusValue = ocspResponse_internalError;
            break;
        case SEC_ERROR_OCSP_TRY_SERVER_LATER:
            response.statusValue = ocspResponse_tryLater;
            break;
        case SEC_ERROR_OCSP_REQUEST_NEEDS_SIG:
            response.statusValue = ocspResponse_sigRequired;
            break;
        case SEC_ERROR_OCSP_UNAUTHORIZED_REQUEST:
            response.statusValue = ocspResponse_unauthorized;
            break;
        default:
            PORT_SetError(SEC_ERROR_INVALID_ARGS);
            return NULL;
    }

    if (!SEC_ASN1EncodeInteger(NULL, &response.responseStatus,
                               response.statusValue))
        return NULL;

    result = SEC_ASN1EncodeItem(arena, NULL, &response,
                                ocsp_OCSPErrorResponseTemplate);

    SECITEM_FreeItem(&response.responseStatus, PR_FALSE);

    return result;
}
