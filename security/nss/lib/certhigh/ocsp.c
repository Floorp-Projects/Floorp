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

/*
 * Implementation of OCSP services, for both client and server.
 * (XXX, really, mostly just for client right now, but intended to do both.)
 *
 * $Id: ocsp.c,v 1.20 2004/05/22 01:03:26 nelsonb%netscape.com Exp $
 */

#include "prerror.h"
#include "prprf.h"
#include "plarena.h"
#include "prnetdb.h"

#include "seccomon.h"
#include "secitem.h"
#include "secoidt.h"
#include "secasn1.h"
#include "secder.h"
#include "cert.h"
#include "xconst.h"
#include "secerr.h"
#include "secoid.h"
#include "hasht.h"
#include "sechash.h"
#include "secasn1.h"
#include "keyhi.h"
#include "cryptohi.h"
#include "ocsp.h"
#include "ocspti.h"
#include "genname.h"
#include "certxutl.h"
#include "pk11func.h"	/* for PK11_HashBuf */
#include <stdarg.h>


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


/*
 * Forward declarations of sub-types, so I can lay out the types in the
 * same order as the ASN.1 is laid out in the OCSP spec itself.
 *
 * These are in alphabetical order (case-insensitive); please keep it that way!
 */
extern const SEC_ASN1Template ocsp_CertIDTemplate[];
extern const SEC_ASN1Template ocsp_PointerToSignatureTemplate[];
extern const SEC_ASN1Template ocsp_PointerToResponseBytesTemplate[];
extern const SEC_ASN1Template ocsp_ResponseDataTemplate[];
extern const SEC_ASN1Template ocsp_RevokedInfoTemplate[];
extern const SEC_ASN1Template ocsp_SingleRequestTemplate[];
extern const SEC_ASN1Template ocsp_SingleResponseTemplate[];
extern const SEC_ASN1Template ocsp_TBSRequestTemplate[];


/*
 * Request-related templates...
 */

/*
 * OCSPRequest	::=	SEQUENCE {
 *	tbsRequest		TBSRequest,
 *	optionalSignature	[0] EXPLICIT Signature OPTIONAL }
 */
static const SEC_ASN1Template ocsp_OCSPRequestTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	0, NULL, sizeof(CERTOCSPRequest) },
    { SEC_ASN1_POINTER,
	offsetof(CERTOCSPRequest, tbsRequest),
	ocsp_TBSRequestTemplate },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_EXPLICIT |
      SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 0,
	offsetof(CERTOCSPRequest, optionalSignature),
	ocsp_PointerToSignatureTemplate },
    { 0 }
};

/*
 * TBSRequest	::=	SEQUENCE {
 *	version			[0] EXPLICIT Version DEFAULT v1,
 *	requestorName		[1] EXPLICIT GeneralName OPTIONAL,
 *	requestList		SEQUENCE OF Request,
 *	requestExtensions	[2] EXPLICIT Extensions OPTIONAL }
 *
 * Version	::=	INTEGER { v1(0) }
 *
 * Note: this should be static but the AIX compiler doesn't like it (because it
 * was forward-declared above); it is not meant to be exported, but this
 * is the only way it will compile.
 */
const SEC_ASN1Template ocsp_TBSRequestTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	0, NULL, sizeof(ocspTBSRequest) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_EXPLICIT |		/* XXX DER_DEFAULT */
      SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 0,
	offsetof(ocspTBSRequest, version),
	SEC_IntegerTemplate },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_EXPLICIT |
      SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 1,
	offsetof(ocspTBSRequest, derRequestorName),
	SEC_PointerToAnyTemplate },
    { SEC_ASN1_SEQUENCE_OF,
	offsetof(ocspTBSRequest, requestList),
	ocsp_SingleRequestTemplate },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_EXPLICIT |
      SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 2,
	offsetof(ocspTBSRequest, requestExtensions),
	CERT_SequenceOfCertExtensionTemplate },
    { 0 }
};

/*
 * Signature	::=	SEQUENCE {
 *	signatureAlgorithm	AlgorithmIdentifier,
 *	signature		BIT STRING,
 *	certs			[0] EXPLICIT SEQUENCE OF Certificate OPTIONAL }
 */
static const SEC_ASN1Template ocsp_SignatureTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	0, NULL, sizeof(ocspSignature) },
    { SEC_ASN1_INLINE,
	offsetof(ocspSignature, signatureAlgorithm),
	SECOID_AlgorithmIDTemplate },
    { SEC_ASN1_BIT_STRING,
	offsetof(ocspSignature, signature) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_EXPLICIT |
      SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 0,
	offsetof(ocspSignature, derCerts), 
	SEC_SequenceOfAnyTemplate },
    { 0 }
};

/*
 * This template is just an extra level to use in an explicitly-tagged
 * reference to a Signature.
 *
 * Note: this should be static but the AIX compiler doesn't like it (because it
 * was forward-declared above); it is not meant to be exported, but this
 * is the only way it will compile.
 */
const SEC_ASN1Template ocsp_PointerToSignatureTemplate[] = {
    { SEC_ASN1_POINTER, 0, ocsp_SignatureTemplate }
};

/*
 * Request	::=	SEQUENCE {
 *	reqCert			CertID,
 *	singleRequestExtensions	[0] EXPLICIT Extensions OPTIONAL }
 *
 * Note: this should be static but the AIX compiler doesn't like it (because it
 * was forward-declared above); it is not meant to be exported, but this
 * is the only way it will compile.
 */
const SEC_ASN1Template ocsp_SingleRequestTemplate[] = {
    { SEC_ASN1_SEQUENCE, 
	0, NULL, sizeof(ocspSingleRequest) },
    { SEC_ASN1_POINTER,
	offsetof(ocspSingleRequest, reqCert),
	ocsp_CertIDTemplate },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_EXPLICIT |
      SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 0,
	offsetof(ocspSingleRequest, singleRequestExtensions),
	CERT_SequenceOfCertExtensionTemplate },
    { 0 }
};


/*
 * This data structure and template (CertID) is used by both OCSP
 * requests and responses.  It is the only one that is shared.
 *
 * CertID	::=	SEQUENCE {
 *	hashAlgorithm		AlgorithmIdentifier,
 *	issuerNameHash		OCTET STRING,	-- Hash of Issuer DN
 *	issuerKeyHash		OCTET STRING,	-- Hash of Issuer public key
 *	serialNumber		CertificateSerialNumber }
 *
 * CertificateSerialNumber ::=	INTEGER
 *
 * Note: this should be static but the AIX compiler doesn't like it (because it
 * was forward-declared above); it is not meant to be exported, but this
 * is the only way it will compile.
 */
const SEC_ASN1Template ocsp_CertIDTemplate[] = {
    { SEC_ASN1_SEQUENCE, 
	0, NULL, sizeof(CERTOCSPCertID) },
    { SEC_ASN1_INLINE,
	offsetof(CERTOCSPCertID, hashAlgorithm),
	SECOID_AlgorithmIDTemplate },
    { SEC_ASN1_OCTET_STRING,
	offsetof(CERTOCSPCertID, issuerNameHash) },
    { SEC_ASN1_OCTET_STRING,
	offsetof(CERTOCSPCertID, issuerKeyHash) },
    { SEC_ASN1_INTEGER, 
	offsetof(CERTOCSPCertID, serialNumber) },
    { 0 }
};


/*
 * Response-related templates...
 */

/*
 * OCSPResponse	::=	SEQUENCE {
 *	responseStatus		OCSPResponseStatus,
 *	responseBytes		[0] EXPLICIT ResponseBytes OPTIONAL }
 */
static const SEC_ASN1Template ocsp_OCSPResponseTemplate[] = {
    { SEC_ASN1_SEQUENCE, 
	0, NULL, sizeof(CERTOCSPResponse) },
    { SEC_ASN1_ENUMERATED, 
	offsetof(CERTOCSPResponse, responseStatus) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_EXPLICIT |
      SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 0,
	offsetof(CERTOCSPResponse, responseBytes),
	ocsp_PointerToResponseBytesTemplate },
    { 0 }
};

/*
 * ResponseBytes	::=	SEQUENCE {
 *	responseType		OBJECT IDENTIFIER,
 *	response		OCTET STRING }
 */
static const SEC_ASN1Template ocsp_ResponseBytesTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	0, NULL, sizeof(ocspResponseBytes) },
    { SEC_ASN1_OBJECT_ID,
	offsetof(ocspResponseBytes, responseType) },
    { SEC_ASN1_OCTET_STRING,
	offsetof(ocspResponseBytes, response) },
    { 0 }
};

/*
 * This template is just an extra level to use in an explicitly-tagged
 * reference to a ResponseBytes.
 *
 * Note: this should be static but the AIX compiler doesn't like it (because it
 * was forward-declared above); it is not meant to be exported, but this
 * is the only way it will compile.
 */
const SEC_ASN1Template ocsp_PointerToResponseBytesTemplate[] = {
    { SEC_ASN1_POINTER, 0, ocsp_ResponseBytesTemplate }
};

/*
 * BasicOCSPResponse	::=	SEQUENCE {
 *	tbsResponseData		ResponseData,
 *	signatureAlgorithm	AlgorithmIdentifier,
 *	signature		BIT STRING,
 *	certs			[0] EXPLICIT SEQUENCE OF Certificate OPTIONAL }
 */
static const SEC_ASN1Template ocsp_BasicOCSPResponseTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	0, NULL, sizeof(ocspBasicOCSPResponse) },
    { SEC_ASN1_POINTER,
	offsetof(ocspBasicOCSPResponse, tbsResponseData),
	ocsp_ResponseDataTemplate },
    { SEC_ASN1_INLINE,
	offsetof(ocspBasicOCSPResponse, responseSignature.signatureAlgorithm),
	SECOID_AlgorithmIDTemplate },
    { SEC_ASN1_BIT_STRING,
	offsetof(ocspBasicOCSPResponse, responseSignature.signature) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_EXPLICIT |
      SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 0,
	offsetof(ocspBasicOCSPResponse, responseSignature.derCerts),
	SEC_SequenceOfAnyTemplate },
    { 0 }
};

/*
 * ResponseData	::=	SEQUENCE {
 *	version			[0] EXPLICIT Version DEFAULT v1,
 *	responderID		ResponderID,
 *	producedAt		GeneralizedTime,
 *	responses		SEQUENCE OF SingleResponse,
 *	responseExtensions	[1] EXPLICIT Extensions OPTIONAL }
 *
 * Note: this should be static but the AIX compiler doesn't like it (because it
 * was forward-declared above); it is not meant to be exported, but this
 * is the only way it will compile.
 */
const SEC_ASN1Template ocsp_ResponseDataTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	0, NULL, sizeof(ocspResponseData) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_EXPLICIT |		/* XXX DER_DEFAULT */
      SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 0,
	offsetof(ocspResponseData, version),
	SEC_IntegerTemplate },
    { SEC_ASN1_ANY,
	offsetof(ocspResponseData, derResponderID) },
    { SEC_ASN1_GENERALIZED_TIME,
	offsetof(ocspResponseData, producedAt) },
    { SEC_ASN1_SEQUENCE_OF,
	offsetof(ocspResponseData, responses),
	ocsp_SingleResponseTemplate },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_EXPLICIT |
      SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 1,
	offsetof(ocspResponseData, responseExtensions),
	CERT_SequenceOfCertExtensionTemplate },
    { 0 }
};

/*
 * ResponderID	::=	CHOICE {
 *	byName			[1] EXPLICIT Name,
 *	byKey			[2] EXPLICIT KeyHash }
 *
 * KeyHash ::=	OCTET STRING -- SHA-1 hash of responder's public key
 * (excluding the tag and length fields)
 *
 * XXX Because the ASN.1 encoder and decoder currently do not provide
 * a way to automatically handle a CHOICE, we need to do it in two
 * steps, looking at the type tag and feeding the exact choice back
 * to the ASN.1 code.  Hopefully that will change someday and this
 * can all be simplified down into a single template.  Anyway, for
 * now we list each choice as its own template:
 */
static const SEC_ASN1Template ocsp_ResponderIDByNameTemplate[] = {
    { SEC_ASN1_EXPLICIT | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 1,
	offsetof(ocspResponderID, responderIDValue.name),
	CERT_NameTemplate }
};
static const SEC_ASN1Template ocsp_ResponderIDByKeyTemplate[] = {
    { SEC_ASN1_EXPLICIT | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 2,
	offsetof(ocspResponderID, responderIDValue.keyHash),
	SEC_OctetStringTemplate }
};
static const SEC_ASN1Template ocsp_ResponderIDOtherTemplate[] = {
    { SEC_ASN1_ANY,
	offsetof(ocspResponderID, responderIDValue.other) }
};

/*
 * SingleResponse	::=	SEQUENCE {
 *	certID			CertID,
 *	certStatus		CertStatus,
 *	thisUpdate		GeneralizedTime,
 *	nextUpdate		[0] EXPLICIT GeneralizedTime OPTIONAL,
 *	singleExtensions	[1] EXPLICIT Extensions OPTIONAL }
 *
 * Note: this should be static but the AIX compiler doesn't like it (because it
 * was forward-declared above); it is not meant to be exported, but this
 * is the only way it will compile.
 */
const SEC_ASN1Template ocsp_SingleResponseTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	0, NULL, sizeof(CERTOCSPSingleResponse) },
    { SEC_ASN1_POINTER,
	offsetof(CERTOCSPSingleResponse, certID),
	ocsp_CertIDTemplate },
    { SEC_ASN1_ANY,
	offsetof(CERTOCSPSingleResponse, derCertStatus) },
    { SEC_ASN1_GENERALIZED_TIME,
	offsetof(CERTOCSPSingleResponse, thisUpdate) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_EXPLICIT |
      SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 0,
	offsetof(CERTOCSPSingleResponse, nextUpdate),
	SEC_PointerToGeneralizedTimeTemplate },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_EXPLICIT |
      SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 1,
	offsetof(CERTOCSPSingleResponse, singleExtensions),
	CERT_SequenceOfCertExtensionTemplate },
    { 0 }
};

/*
 * CertStatus	::=	CHOICE {
 *	good			[0] IMPLICIT NULL,
 *	revoked			[1] IMPLICIT RevokedInfo,
 *	unknown			[2] IMPLICIT UnknownInfo }
 *
 * Because the ASN.1 encoder and decoder currently do not provide
 * a way to automatically handle a CHOICE, we need to do it in two
 * steps, looking at the type tag and feeding the exact choice back
 * to the ASN.1 code.  Hopefully that will change someday and this
 * can all be simplified down into a single template.  Anyway, for
 * now we list each choice as its own template:
 */
static const SEC_ASN1Template ocsp_CertStatusGoodTemplate[] = {
    { SEC_ASN1_POINTER | SEC_ASN1_CONTEXT_SPECIFIC | 0,
	offsetof(ocspCertStatus, certStatusInfo.goodInfo),
	SEC_NullTemplate }
};
static const SEC_ASN1Template ocsp_CertStatusRevokedTemplate[] = {
    { SEC_ASN1_POINTER | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 1, 
	offsetof(ocspCertStatus, certStatusInfo.revokedInfo),
	ocsp_RevokedInfoTemplate }
};
static const SEC_ASN1Template ocsp_CertStatusUnknownTemplate[] = {
    { SEC_ASN1_POINTER | SEC_ASN1_CONTEXT_SPECIFIC | 2,
	offsetof(ocspCertStatus, certStatusInfo.unknownInfo),
	SEC_NullTemplate }
};
static const SEC_ASN1Template ocsp_CertStatusOtherTemplate[] = {
    { SEC_ASN1_POINTER,
	offsetof(ocspCertStatus, certStatusInfo.otherInfo),
	SEC_AnyTemplate }
};

/*
 * RevokedInfo	::=	SEQUENCE {
 *	revocationTime		GeneralizedTime,
 *	revocationReason	[0] EXPLICIT CRLReason OPTIONAL }
 *
 * Note: this should be static but the AIX compiler doesn't like it (because it
 * was forward-declared above); it is not meant to be exported, but this
 * is the only way it will compile.
 */
const SEC_ASN1Template ocsp_RevokedInfoTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	0, NULL, sizeof(ocspRevokedInfo) },
    { SEC_ASN1_GENERALIZED_TIME,
	offsetof(ocspRevokedInfo, revocationTime) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_EXPLICIT |
      SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 0,
	offsetof(ocspRevokedInfo, revocationReason), 
	SEC_PointerToEnumeratedTemplate },
    { 0 }
};


/*
 * OCSP-specific extension templates:
 */

/*
 * ServiceLocator	::=	SEQUENCE {
 *	issuer			Name,
 *	locator			AuthorityInfoAccessSyntax OPTIONAL }
 */
static const SEC_ASN1Template ocsp_ServiceLocatorTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	0, NULL, sizeof(ocspServiceLocator) },
    { SEC_ASN1_POINTER,
	offsetof(ocspServiceLocator, issuer),
	CERT_NameTemplate },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_ANY,
	offsetof(ocspServiceLocator, locator) },
    { 0 }
};


/*
 * REQUEST SUPPORT FUNCTIONS (encode/create/decode/destroy):
 */

/* 
 * FUNCTION: CERT_EncodeOCSPRequest
 *   DER encodes an OCSP Request, possibly adding a signature as well.
 *   XXX Signing is not yet supported, however; see comments in code.
 * INPUTS: 
 *   PRArenaPool *arena
 *     The return value is allocated from here.
 *     If a NULL is passed in, allocation is done from the heap instead.
 *   CERTOCSPRequest *request
 *     The request to be encoded.
 *   void *pwArg
 *     Pointer to argument for password prompting, if needed.  (Definitely
 *     not needed if not signing.)
 * RETURN:
 *   Returns a NULL on error and a pointer to the SECItem with the
 *   encoded value otherwise.  Any error is likely to be low-level
 *   (e.g. no memory).
 */
SECItem *
CERT_EncodeOCSPRequest(PRArenaPool *arena, CERTOCSPRequest *request, 
		       void *pwArg)
{
    ocspTBSRequest *tbsRequest;
    SECStatus rv;

    /* XXX All of these should generate errors if they fail. */
    PORT_Assert(request);
    PORT_Assert(request->tbsRequest);

    tbsRequest = request->tbsRequest;

    if (request->tbsRequest->extensionHandle != NULL) {
	rv = CERT_FinishExtensions(request->tbsRequest->extensionHandle);
	request->tbsRequest->extensionHandle = NULL;
	if (rv != SECSuccess)
	    return NULL;
    }

    /*
     * XXX When signed requests are supported and request->optionalSignature
     * is not NULL:
     *  - need to encode tbsRequest->requestorName
     *  - need to encode tbsRequest
     *  - need to sign that encoded result (using cert in sig), filling in the
     *    request->optionalSignature structure with the result, the signing
     *    algorithm and (perhaps?) the cert (and its chain?) in derCerts
     */

    return SEC_ASN1EncodeItem(arena, NULL, request, ocsp_OCSPRequestTemplate);
}


/*
 * FUNCTION: CERT_DecodeOCSPRequest
 *   Decode a DER encoded OCSP Request.
 * INPUTS:
 *   SECItem *src
 *     Pointer to a SECItem holding DER encoded OCSP Request.
 * RETURN:
 *   Returns a pointer to a CERTOCSPRequest containing the decoded request.
 *   On error, returns NULL.  Most likely error is trouble decoding
 *   (SEC_ERROR_OCSP_MALFORMED_REQUEST), or low-level problem (no memory).
 */
CERTOCSPRequest *
CERT_DecodeOCSPRequest(SECItem *src)
{
    PRArenaPool *arena = NULL;
    SECStatus rv = SECFailure;
    CERTOCSPRequest *dest = NULL;
    int i;
    SECItem newSrc;

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL) {
	goto loser;
    }
    dest = (CERTOCSPRequest *) PORT_ArenaZAlloc(arena, 
						sizeof(CERTOCSPRequest));
    if (dest == NULL) {
	goto loser;
    }
    dest->arena = arena;

    /* copy the DER into the arena, since Quick DER returns data that points
       into the DER input, which may get freed by the caller */
    rv = SECITEM_CopyItem(arena, &newSrc, src);
    if ( rv != SECSuccess ) {
	goto loser;
    }

    rv = SEC_QuickDERDecodeItem(arena, dest, ocsp_OCSPRequestTemplate, &newSrc);
    if (rv != SECSuccess) {
	if (PORT_GetError() == SEC_ERROR_BAD_DER)
	    PORT_SetError(SEC_ERROR_OCSP_MALFORMED_REQUEST);
	goto loser;
    }

    /*
     * XXX I would like to find a way to get rid of the necessity
     * of doing this copying of the arena pointer.
     */
    for (i = 0; dest->tbsRequest->requestList[i] != NULL; i++) {
	dest->tbsRequest->requestList[i]->arena = arena;
    }

    return dest;

loser:
    if (arena != NULL) {
	PORT_FreeArena(arena, PR_FALSE);
    }
    return NULL;
}

SECStatus
CERT_DestroyOCSPCertID(CERTOCSPCertID* certID)
{
    if (certID->poolp) {
	PORT_FreeArena(certID->poolp, PR_FALSE);
	return SECSuccess;
    }
    return SECFailure;
}


/*
 * Create and fill-in a CertID.  This function fills in the hash values
 * (issuerNameHash and issuerKeyHash), and is hardwired to use SHA1.
 * Someday it might need to be more flexible about hash algorithm, but
 * for now we have no intention/need to create anything else.
 *
 * Error causes a null to be returned; most likely cause is trouble
 * finding the certificate issuer (SEC_ERROR_UNKNOWN_ISSUER).
 * Other errors are low-level problems (no memory, bad database, etc.).
 */
static CERTOCSPCertID *
ocsp_CreateCertID(PRArenaPool *arena, CERTCertificate *cert, int64 time)
{
    CERTOCSPCertID *certID;
    CERTCertificate *issuerCert = NULL;
    SECItem *tempItem = NULL;
    void *mark = PORT_ArenaMark(arena);
    SECStatus rv;

    PORT_Assert(arena != NULL);

    certID = PORT_ArenaZNew(arena, CERTOCSPCertID);
    if (certID == NULL) {
	goto loser;
    }

    rv = SECOID_SetAlgorithmID(arena, &certID->hashAlgorithm, SEC_OID_SHA1,
			       NULL);
    if (rv != SECSuccess) {
	goto loser; 
    }

    issuerCert = CERT_FindCertIssuer(cert, time, certUsageAnyCA);
    if (issuerCert == NULL) {
	goto loser;
    }

    tempItem = SEC_ASN1EncodeItem(NULL, NULL, &issuerCert->subject,
				  CERT_NameTemplate);
    if (tempItem == NULL) {
	goto loser;
    }

    if (SECITEM_AllocItem(arena, &(certID->issuerNameHash),
			  SHA1_LENGTH) == NULL) {
	goto loser;
    }
    rv = PK11_HashBuf(SEC_OID_SHA1, certID->issuerNameHash.data,
		      tempItem->data, tempItem->len);
    if (rv != SECSuccess) {
	goto loser; 
    }
    certID->issuerSHA1NameHash.data = certID->issuerNameHash.data;
    certID->issuerSHA1NameHash.len = certID->issuerNameHash.len;
    /* cache the other two hash algorithms as well */
    if (SECITEM_AllocItem(arena, &(certID->issuerMD5NameHash),
			  MD5_LENGTH) == NULL) {
	goto loser;
    }
    rv = PK11_HashBuf(SEC_OID_MD5, certID->issuerMD5NameHash.data,
		      tempItem->data, tempItem->len);
    if (rv != SECSuccess) {
	goto loser; 
    }
    if (SECITEM_AllocItem(arena, &(certID->issuerMD2NameHash),
			  MD2_LENGTH) == NULL) {
	goto loser;
    }
    rv = PK11_HashBuf(SEC_OID_MD2, certID->issuerMD2NameHash.data,
		      tempItem->data, tempItem->len);
    if (rv != SECSuccess) {
	goto loser; 
    }

    SECITEM_FreeItem(tempItem, PR_TRUE);
    tempItem = NULL;

    if (CERT_SPKDigestValueForCert(arena, issuerCert, SEC_OID_SHA1,
				   &(certID->issuerKeyHash)) == NULL) {
	goto loser;
    }
    certID->issuerSHA1KeyHash.data = certID->issuerKeyHash.data;
    certID->issuerSHA1KeyHash.len = certID->issuerKeyHash.len;
    /* cache the other two hash algorithms as well */
    if (CERT_SPKDigestValueForCert(arena, issuerCert, SEC_OID_MD5,
				   &(certID->issuerMD5KeyHash)) == NULL) {
	goto loser;
    }
    if (CERT_SPKDigestValueForCert(arena, issuerCert, SEC_OID_MD2,
				   &(certID->issuerMD2KeyHash)) == NULL) {
	goto loser;
    }


    /* now we are done with issuerCert */
    CERT_DestroyCertificate(issuerCert);
    issuerCert = NULL;

    rv = SECITEM_CopyItem(arena, &certID->serialNumber, &cert->serialNumber);
    if (rv != SECSuccess) {
	goto loser; 
    }

    PORT_ArenaUnmark(arena, mark);
    return certID;

loser:
    if (issuerCert != NULL) {
	CERT_DestroyCertificate(issuerCert);
    }
    if (tempItem != NULL) {
	SECITEM_FreeItem(tempItem, PR_TRUE);
    }
    PORT_ArenaRelease(arena, mark);
    return NULL;
}

CERTOCSPCertID*
CERT_CreateOCSPCertID(CERTCertificate *cert, int64 time)
{
    PRArenaPool *arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    CERTOCSPCertID *certID;
    PORT_Assert(arena != NULL);
    if (!arena)
	return NULL;
    
    certID = ocsp_CreateCertID(arena, cert, time);
    if (!certID) {
	PORT_FreeArena(arena, PR_FALSE);
	return NULL;
    }
    certID->poolp = arena;
    return certID;
}



/*
 * Callback to set Extensions in request object
 */
void SetSingleReqExts(void *object, CERTCertExtension **exts)
{
  ocspSingleRequest *singleRequest =
    (ocspSingleRequest *)object;

  singleRequest->singleRequestExtensions = exts;
}

/*
 * Add the Service Locator extension to the singleRequestExtensions
 * for the given singleRequest.
 *
 * All errors are internal or low-level problems (e.g. no memory).
 */
static SECStatus
ocsp_AddServiceLocatorExtension(ocspSingleRequest *singleRequest,
				CERTCertificate *cert)
{
    ocspServiceLocator *serviceLocator = NULL;
    void *extensionHandle = NULL;
    SECStatus rv = SECFailure;

    serviceLocator = PORT_ZNew(ocspServiceLocator);
    if (serviceLocator == NULL)
	goto loser;

    /*
     * Normally it would be a bad idea to do a direct reference like
     * this rather than allocate and copy the name *or* at least dup
     * a reference of the cert.  But all we need is to be able to read
     * the issuer name during the encoding we are about to do, so a
     * copy is just a waste of time.
     */
    serviceLocator->issuer = &cert->issuer;

    rv = CERT_FindCertExtension(cert, SEC_OID_X509_AUTH_INFO_ACCESS,
				&serviceLocator->locator);
    if (rv != SECSuccess) {
	if (PORT_GetError() != SEC_ERROR_EXTENSION_NOT_FOUND)
	    goto loser;
    }

    /* prepare for following loser gotos */
    rv = SECFailure;

    extensionHandle = cert_StartExtensions(singleRequest,
                       singleRequest->arena, SetSingleReqExts);
    if (extensionHandle == NULL)
	goto loser;

    rv = CERT_EncodeAndAddExtension(extensionHandle,
				    SEC_OID_PKIX_OCSP_SERVICE_LOCATOR,
				    serviceLocator, PR_FALSE,
				    ocsp_ServiceLocatorTemplate);

loser:
    if (extensionHandle != NULL) {
	/*
	 * Either way we have to finish out the extension context (so it gets
	 * freed).  But careful not to override any already-set bad status.
	 */
	SECStatus tmprv = CERT_FinishExtensions(extensionHandle);
	if (rv == SECSuccess)
	    rv = tmprv;
    }

    /*
     * Finally, free the serviceLocator structure itself and we are done.
     */
    if (serviceLocator != NULL) {
	if (serviceLocator->locator.data != NULL)
	    SECITEM_FreeItem(&serviceLocator->locator, PR_FALSE);
	PORT_Free(serviceLocator);
    }

    return rv;
}

/*
 * Creates an array of ocspSingleRequest based on a list of certs.
 * Note that the code which later compares the request list with the
 * response expects this array to be in the exact same order as the
 * certs are found in the list.  It would be harder to change that
 * order than preserve it, but since the requirement is not obvious,
 * it deserves to be mentioned.
 *
 * Any problem causes a null return and error set:
 *	SEC_ERROR_UNKNOWN_ISSUER
 * Other errors are low-level problems (no memory, bad database, etc.).
 */
static ocspSingleRequest **
ocsp_CreateSingleRequestList(PRArenaPool *arena, CERTCertList *certList,
			     int64 time, PRBool includeLocator)
{
    ocspSingleRequest **requestList = NULL;
    CERTCertListNode *node;
    int i, count;
    void *mark = PORT_ArenaMark(arena);
 
    node = CERT_LIST_HEAD(certList);
    for (count = 0; !CERT_LIST_END(node, certList); count++) {
	node = CERT_LIST_NEXT(node);
    }

    if (count == 0)
	goto loser;

    requestList = PORT_ArenaNewArray(arena, ocspSingleRequest *, count + 1);
    if (requestList == NULL)
	goto loser;

    node = CERT_LIST_HEAD(certList);
    for (i = 0; !CERT_LIST_END(node, certList); i++) {
        requestList[i] = PORT_ArenaZNew(arena, ocspSingleRequest);
	if (requestList[i] == NULL)
	    goto loser;

	requestList[i]->arena = arena;
	requestList[i]->reqCert = ocsp_CreateCertID(arena, node->cert, time);
	if (requestList[i]->reqCert == NULL)
	    goto loser;

	if (includeLocator == PR_TRUE) {
	    SECStatus rv;

	    rv = ocsp_AddServiceLocatorExtension(requestList[i], node->cert);
	    if (rv != SECSuccess)
		goto loser;
	}

	node = CERT_LIST_NEXT(node);
    }

    PORT_Assert(i == count);

    PORT_ArenaUnmark(arena, mark);
    requestList[i] = NULL;
    return requestList;

loser:
    PORT_ArenaRelease(arena, mark);
    return NULL;
}


/*
 * FUNCTION: CERT_CreateOCSPRequest
 *   Creates a CERTOCSPRequest, requesting the status of the certs in 
 *   the given list.
 * INPUTS:
 *   CERTCertList *certList
 *     A list of certs for which status will be requested.
 *     Note that all of these certificates should have the same issuer,
 *     or it's expected the response will be signed by a trusted responder.
 *     If the certs need to be broken up into multiple requests, that
 *     must be handled by the caller (and thus by having multiple calls
 *     to this routine), who knows about where the request(s) are being
 *     sent and whether there are any trusted responders in place.
 *   int64 time
 *     Indicates the time for which the certificate status is to be 
 *     determined -- this may be used in the search for the cert's issuer
 *     but has no effect on the request itself.
 *   PRBool addServiceLocator
 *     If true, the Service Locator extension should be added to the
 *     single request(s) for each cert.
 *   CERTCertificate *signerCert
 *     If non-NULL, means sign the request using this cert.  Otherwise,
 *     do not sign.
 *     XXX note that request signing is not yet supported; see comment in code
 * RETURN:
 *   A pointer to a CERTOCSPRequest structure containing an OCSP request
 *   for the cert list.  On error, null is returned, with an error set
 *   indicating the reason.  This is likely SEC_ERROR_UNKNOWN_ISSUER.
 *   (The issuer is needed to create a request for the certificate.)
 *   Other errors are low-level problems (no memory, bad database, etc.).
 */
CERTOCSPRequest *
CERT_CreateOCSPRequest(CERTCertList *certList, int64 time, 
		       PRBool addServiceLocator,
		       CERTCertificate *signerCert)
{
    PRArenaPool *arena = NULL;
    CERTOCSPRequest *request = NULL;
    ocspTBSRequest *tbsRequest = NULL;

    /*
     * XXX This should set an error, but since it is only temporary and
     * since PSM will not initially provide a way to turn on signing of
     * requests anyway, I figure we can just skip defining an error that
     * will be obsolete in the next release.  When we are prepared to
     * put signing of requests back in, this entire check will go away,
     * and later in this function we will need to allocate a signature
     * structure for the request, fill in the "derCerts" field in it,
     * save the signerCert there, as well as fill in the "requestorName"
     * field of the tbsRequest.
     */
    if (signerCert != NULL) {
	return NULL;
    }

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL) {
	goto loser;
    }

    request = PORT_ArenaZNew(arena, CERTOCSPRequest);
    if (request == NULL) {
	goto loser;
    }
    request->arena = arena;

    tbsRequest = PORT_ArenaZNew(arena, ocspTBSRequest);
    if (tbsRequest == NULL) {
	goto loser;
    }
    request->tbsRequest = tbsRequest;

    /* version 1 is the default, so we need not fill in a version number */

    /*
     * Now create the list of single requests, one for each cert.
     */
    tbsRequest->requestList = ocsp_CreateSingleRequestList(arena, certList,
							   time,
							   addServiceLocator);
    if (tbsRequest->requestList == NULL) {
	goto loser;
    }
    return request;

loser:
    if (arena != NULL) {
	PORT_FreeArena(arena, PR_FALSE);
    }
    return NULL;
}


/*
 * FUNCTION: CERT_AddOCSPAcceptableResponses
 *   Add the AcceptableResponses extension to an OCSP Request.
 * INPUTS:
 *   CERTOCSPRequest *request
 *     The request to which the extension should be added.
 *   ...
 *     A list (of one or more) of SECOidTag -- each of the response types
 *     to be added.  The last OID *must* be SEC_OID_PKIX_OCSP_BASIC_RESPONSE.
 *     (This marks the end of the list, and it must be specified because a
 *     client conforming to the OCSP standard is required to handle the basic
 *     response type.)  The OIDs are not checked in any way.
 * RETURN:
 *   SECSuccess if the extension is added; SECFailure if anything goes wrong.
 *   All errors are internal or low-level problems (e.g. no memory).
 */

void SetRequestExts(void *object, CERTCertExtension **exts)
{
  CERTOCSPRequest *request = (CERTOCSPRequest *)object;

  request->tbsRequest->requestExtensions = exts;
}

SECStatus
CERT_AddOCSPAcceptableResponses(CERTOCSPRequest *request,
				SECOidTag responseType0, ...)
{
    void *extHandle;
    va_list ap;
    int i, count;
    SECOidTag responseType;
    SECOidData *responseOid;
    SECItem **acceptableResponses = NULL;
    SECStatus rv = SECFailure;

    extHandle = request->tbsRequest->extensionHandle;
    if (extHandle == NULL) {
	extHandle = cert_StartExtensions(request, request->arena, SetRequestExts);
	if (extHandle == NULL)
	    goto loser;
    }

    /* Count number of OIDS going into the extension value. */
    count = 1;
    if (responseType0 != SEC_OID_PKIX_OCSP_BASIC_RESPONSE) {
	va_start(ap, responseType0);
	do {
	    count++;
	    responseType = va_arg(ap, SECOidTag);
	} while (responseType != SEC_OID_PKIX_OCSP_BASIC_RESPONSE);
	va_end(ap);
    }

    acceptableResponses = PORT_NewArray(SECItem *, count + 1);
    if (acceptableResponses == NULL)
	goto loser;

    i = 0;
    responseOid = SECOID_FindOIDByTag(responseType0);
    acceptableResponses[i++] = &(responseOid->oid);
    if (count > 1) {
	va_start(ap, responseType0);
	for ( ; i < count; i++) {
	    responseType = va_arg(ap, SECOidTag);
	    responseOid = SECOID_FindOIDByTag(responseType);
	    acceptableResponses[i] = &(responseOid->oid);
	}
	va_end(ap);
    }
    acceptableResponses[i] = NULL;

    rv = CERT_EncodeAndAddExtension(extHandle, SEC_OID_PKIX_OCSP_RESPONSE,
				    &acceptableResponses, PR_FALSE,
				    SEC_SequenceOfObjectIDTemplate);
    if (rv != SECSuccess)
	goto loser;

    PORT_Free(acceptableResponses);
    if (request->tbsRequest->extensionHandle == NULL)
	request->tbsRequest->extensionHandle = extHandle;
    return SECSuccess;

loser:
    if (acceptableResponses != NULL)
	PORT_Free(acceptableResponses);
    if (extHandle != NULL)
	(void) CERT_FinishExtensions(extHandle);
    return rv;
}


/*
 * FUNCTION: CERT_DestroyOCSPRequest
 *   Frees an OCSP Request structure.
 * INPUTS:
 *   CERTOCSPRequest *request
 *     Pointer to CERTOCSPRequest to be freed.
 * RETURN:
 *   No return value; no errors.
 */
void
CERT_DestroyOCSPRequest(CERTOCSPRequest *request)
{
    if (request == NULL)
	return;

    if (request->tbsRequest != NULL) {
	if (request->tbsRequest->requestorName != NULL)
	    CERT_DestroyGeneralNameList(request->tbsRequest->requestorName);
	if (request->tbsRequest->extensionHandle != NULL)
	    (void) CERT_FinishExtensions(request->tbsRequest->extensionHandle);
    }

    if (request->optionalSignature != NULL) {
	if (request->optionalSignature->cert != NULL)
	    CERT_DestroyCertificate(request->optionalSignature->cert);

	/*
	 * XXX Need to free derCerts?  Or do they come out of arena?
	 * (Currently we never fill in derCerts, which is why the
	 * answer is not obvious.  Once we do, add any necessary code
	 * here and remove this comment.)
	 */
    }

    /*
     * We should actually never have a request without an arena,
     * but check just in case.  (If there isn't one, there is not
     * much we can do about it...)
     */
    PORT_Assert(request->arena != NULL);
    if (request->arena != NULL)
	PORT_FreeArena(request->arena, PR_FALSE);
}


/*
 * RESPONSE SUPPORT FUNCTIONS (encode/create/decode/destroy):
 */

/*
 * Helper function for encoding or decoding a ResponderID -- based on the
 * given type, return the associated template for that choice.
 */
static const SEC_ASN1Template *
ocsp_ResponderIDTemplateByType(ocspResponderIDType responderIDType)
{
    const SEC_ASN1Template *responderIDTemplate;

    switch (responderIDType) {
	case ocspResponderID_byName:
	    responderIDTemplate = ocsp_ResponderIDByNameTemplate;
	    break;
	case ocspResponderID_byKey:
	    responderIDTemplate = ocsp_ResponderIDByKeyTemplate;
	    break;
	case ocspResponderID_other:
	default:
	    PORT_Assert(responderIDType == ocspResponderID_other);
	    responderIDTemplate = ocsp_ResponderIDOtherTemplate;
	    break;
    }

    return responderIDTemplate;
}

/*
 * Helper function for encoding or decoding a CertStatus -- based on the
 * given type, return the associated template for that choice.
 */
static const SEC_ASN1Template *
ocsp_CertStatusTemplateByType(ocspCertStatusType certStatusType)
{
    const SEC_ASN1Template *certStatusTemplate;

    switch (certStatusType) {
	case ocspCertStatus_good:
	    certStatusTemplate = ocsp_CertStatusGoodTemplate;
	    break;
	case ocspCertStatus_revoked:
	    certStatusTemplate = ocsp_CertStatusRevokedTemplate;
	    break;
	case ocspCertStatus_unknown:
	    certStatusTemplate = ocsp_CertStatusUnknownTemplate;
	    break;
	case ocspCertStatus_other:
	default:
	    PORT_Assert(certStatusType == ocspCertStatus_other);
	    certStatusTemplate = ocsp_CertStatusOtherTemplate;
	    break;
    }

    return certStatusTemplate;
}

/*
 * Helper function for decoding a certStatus -- turn the actual DER tag
 * into our local translation.
 */
static ocspCertStatusType
ocsp_CertStatusTypeByTag(int derTag)
{
    ocspCertStatusType certStatusType;

    switch (derTag) {
	case 0:
	    certStatusType = ocspCertStatus_good;
	    break;
	case 1:
	    certStatusType = ocspCertStatus_revoked;
	    break;
	case 2:
	    certStatusType = ocspCertStatus_unknown;
	    break;
	default:
	    certStatusType = ocspCertStatus_other;
	    break;
    }

    return certStatusType;
}

/*
 * Helper function for decoding SingleResponses -- they each contain
 * a status which is encoded as CHOICE, which needs to be decoded "by hand".
 *
 * Note -- on error, this routine does not release the memory it may
 * have allocated; it expects its caller to do that.
 */
static SECStatus
ocsp_FinishDecodingSingleResponses(PRArenaPool *arena,
				   CERTOCSPSingleResponse **responses)
{
    ocspCertStatus *certStatus;
    ocspCertStatusType certStatusType;
    const SEC_ASN1Template *certStatusTemplate;
    int derTag;
    int i;
    SECStatus rv = SECFailure;

    if (responses == NULL)			/* nothing to do */
	return SECSuccess;

    for (i = 0; responses[i] != NULL; i++) {
	/*
	 * The following assert points out internal errors (problems in
	 * the template definitions or in the ASN.1 decoder itself, etc.).
	 */
	PORT_Assert(responses[i]->derCertStatus.data != NULL);

	derTag = responses[i]->derCertStatus.data[0] & SEC_ASN1_TAGNUM_MASK;
	certStatusType = ocsp_CertStatusTypeByTag(derTag);
	certStatusTemplate = ocsp_CertStatusTemplateByType(certStatusType);

	certStatus = PORT_ArenaZAlloc(arena, sizeof(ocspCertStatus));
	if (certStatus == NULL) {
	    goto loser;
	}
	rv = SEC_ASN1DecodeItem(arena, certStatus, certStatusTemplate,
				&responses[i]->derCertStatus);
	if (rv != SECSuccess) {
	    if (PORT_GetError() == SEC_ERROR_BAD_DER)
		PORT_SetError(SEC_ERROR_OCSP_MALFORMED_RESPONSE);
	    goto loser;
	}

	certStatus->certStatusType = certStatusType;
	responses[i]->certStatus = certStatus;
    }

    return SECSuccess;

loser:
    return rv;
}

/*
 * Helper function for decoding a responderID -- turn the actual DER tag
 * into our local translation.
 */
static ocspResponderIDType
ocsp_ResponderIDTypeByTag(int derTag)
{
    ocspResponderIDType responderIDType;

    switch (derTag) {
	case 1:
	    responderIDType = ocspResponderID_byName;
	    break;
	case 2:
	    responderIDType = ocspResponderID_byKey;
	    break;
	default:
	    responderIDType = ocspResponderID_other;
	    break;
    }

    return responderIDType;
}

/*
 * Decode "src" as a BasicOCSPResponse, returning the result.
 */
static ocspBasicOCSPResponse *
ocsp_DecodeBasicOCSPResponse(PRArenaPool *arena, SECItem *src)
{
    void *mark;
    ocspBasicOCSPResponse *basicResponse;
    ocspResponseData *responseData;
    ocspResponderID *responderID;
    ocspResponderIDType responderIDType;
    const SEC_ASN1Template *responderIDTemplate;
    int derTag;
    SECStatus rv;
    SECItem newsrc;

    mark = PORT_ArenaMark(arena);

    basicResponse = PORT_ArenaZAlloc(arena, sizeof(ocspBasicOCSPResponse));
    if (basicResponse == NULL) {
	goto loser;
    }

    /* copy the DER into the arena, since Quick DER returns data that points
       into the DER input, which may get freed by the caller */
    rv = SECITEM_CopyItem(arena, &newsrc, src);
    if ( rv != SECSuccess ) {
	goto loser;
    }

    rv = SEC_QuickDERDecodeItem(arena, basicResponse,
			    ocsp_BasicOCSPResponseTemplate, &newsrc);
    if (rv != SECSuccess) {
	if (PORT_GetError() == SEC_ERROR_BAD_DER)
	    PORT_SetError(SEC_ERROR_OCSP_MALFORMED_RESPONSE);
	goto loser;
    }

    responseData = basicResponse->tbsResponseData;

    /*
     * The following asserts point out internal errors (problems in
     * the template definitions or in the ASN.1 decoder itself, etc.).
     */
    PORT_Assert(responseData != NULL);
    PORT_Assert(responseData->derResponderID.data != NULL);

    /*
     * XXX Because responderID is a CHOICE, which is not currently handled
     * by our ASN.1 decoder, we have to decode it "by hand".
     */
    derTag = responseData->derResponderID.data[0] & SEC_ASN1_TAGNUM_MASK;
    responderIDType = ocsp_ResponderIDTypeByTag(derTag);
    responderIDTemplate = ocsp_ResponderIDTemplateByType(responderIDType);

    responderID = PORT_ArenaZAlloc(arena, sizeof(ocspResponderID));
    if (responderID == NULL) {
	goto loser;
    }

    rv = SEC_QuickDERDecodeItem(arena, responderID, responderIDTemplate,
			    &responseData->derResponderID);
    if (rv != SECSuccess) {
	if (PORT_GetError() == SEC_ERROR_BAD_DER)
	    PORT_SetError(SEC_ERROR_OCSP_MALFORMED_RESPONSE);
	goto loser;
    }

    responderID->responderIDType = responderIDType;
    responseData->responderID = responderID;

    /*
     * XXX Each SingleResponse also contains a CHOICE, which has to be
     * fixed up by hand.
     */
    rv = ocsp_FinishDecodingSingleResponses(arena, responseData->responses);
    if (rv != SECSuccess) {
	goto loser;
    }

    PORT_ArenaUnmark(arena, mark);
    return basicResponse;

loser:
    PORT_ArenaRelease(arena, mark);
    return NULL;
}


/*
 * Decode the responseBytes based on the responseType found in "rbytes",
 * leaving the resulting translated/decoded information in there as well.
 */
static SECStatus
ocsp_DecodeResponseBytes(PRArenaPool *arena, ocspResponseBytes *rbytes)
{
    PORT_Assert(rbytes != NULL);		/* internal error, really */
    if (rbytes == NULL)
	PORT_SetError(SEC_ERROR_INVALID_ARGS);	/* XXX set better error? */

    rbytes->responseTypeTag = SECOID_FindOIDTag(&rbytes->responseType);
    switch (rbytes->responseTypeTag) {
	case SEC_OID_PKIX_OCSP_BASIC_RESPONSE:
	    {
		ocspBasicOCSPResponse *basicResponse;

		basicResponse = ocsp_DecodeBasicOCSPResponse(arena,
							     &rbytes->response);
		if (basicResponse == NULL)
		    return SECFailure;

		rbytes->decodedResponse.basic = basicResponse;
	    }
	    break;

	/*
	 * Add new/future response types here.
	 */

	default:
	    PORT_SetError(SEC_ERROR_OCSP_UNKNOWN_RESPONSE_TYPE);
	    return SECFailure;
    }

    return SECSuccess;
}


/*
 * FUNCTION: CERT_DecodeOCSPResponse
 *   Decode a DER encoded OCSP Response.
 * INPUTS:
 *   SECItem *src
 *     Pointer to a SECItem holding DER encoded OCSP Response.
 * RETURN:
 *   Returns a pointer to a CERTOCSPResponse (the decoded OCSP Response);
 *   the caller is responsible for destroying it.  Or NULL if error (either
 *   response could not be decoded (SEC_ERROR_OCSP_MALFORMED_RESPONSE),
 *   it was of an unexpected type (SEC_ERROR_OCSP_UNKNOWN_RESPONSE_TYPE),
 *   or a low-level or internal error occurred).
 */
CERTOCSPResponse *
CERT_DecodeOCSPResponse(SECItem *src)
{
    PRArenaPool *arena = NULL;
    CERTOCSPResponse *response = NULL;
    SECStatus rv = SECFailure;
    ocspResponseStatus sv;
    SECItem newSrc;

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL) {
	goto loser;
    }
    response = (CERTOCSPResponse *) PORT_ArenaZAlloc(arena,
						     sizeof(CERTOCSPResponse));
    if (response == NULL) {
	goto loser;
    }
    response->arena = arena;

    /* copy the DER into the arena, since Quick DER returns data that points
       into the DER input, which may get freed by the caller */
    rv = SECITEM_CopyItem(arena, &newSrc, src);
    if ( rv != SECSuccess ) {
	goto loser;
    }

    rv = SEC_QuickDERDecodeItem(arena, response, ocsp_OCSPResponseTemplate, &newSrc);
    if (rv != SECSuccess) {
	if (PORT_GetError() == SEC_ERROR_BAD_DER)
	    PORT_SetError(SEC_ERROR_OCSP_MALFORMED_RESPONSE);
	goto loser;
    }

    sv = (ocspResponseStatus) DER_GetInteger(&response->responseStatus);
    response->statusValue = sv;
    if (sv != ocspResponse_successful) {
	/*
	 * If the response status is anything but successful, then we
	 * are all done with decoding; the status is all there is.
	 */
	return response;
    }

    /*
     * A successful response contains much more information, still encoded.
     * Now we need to decode that.
     */
    rv = ocsp_DecodeResponseBytes(arena, response->responseBytes);
    if (rv != SECSuccess) {
	goto loser;
    }

    return response;

loser:
    if (arena != NULL) {
	PORT_FreeArena(arena, PR_FALSE);
    }
    return NULL;
}

/*
 * The way an OCSPResponse is defined, there are many levels to descend
 * before getting to the actual response information.  And along the way
 * we need to check that the response *type* is recognizable, which for
 * now means that it is a BasicOCSPResponse, because that is the only
 * type currently defined.  Rather than force all routines to perform
 * a bunch of sanity checking every time they want to work on a response,
 * this function isolates that and gives back the interesting part.
 * Note that no copying is done, this just returns a pointer into the
 * substructure of the response which is passed in.
 *
 * XXX This routine only works when a valid response structure is passed
 * into it; this is checked with many assertions.  Assuming the response
 * was creating by decoding, it wouldn't make it this far without being
 * okay.  That is a sufficient assumption since the entire OCSP interface
 * is only used internally.  When this interface is officially exported,
 * each assertion below will need to be followed-up with setting an error
 * and returning (null).
 */
static ocspResponseData *
ocsp_GetResponseData(CERTOCSPResponse *response)
{
    ocspBasicOCSPResponse *basic;
    ocspResponseData *responseData;

    PORT_Assert(response != NULL);

    PORT_Assert(response->responseBytes != NULL);

    PORT_Assert(response->responseBytes->responseTypeTag
		== SEC_OID_PKIX_OCSP_BASIC_RESPONSE);

    basic = response->responseBytes->decodedResponse.basic;
    PORT_Assert(basic != NULL);

    responseData = basic->tbsResponseData;
    PORT_Assert(responseData != NULL);

    return responseData;
}

/*
 * Much like the routine above, except it returns the response signature.
 * Again, no copy is done.
 */
static ocspSignature *
ocsp_GetResponseSignature(CERTOCSPResponse *response)
{
    ocspBasicOCSPResponse *basic;

    PORT_Assert(response != NULL);
    if (NULL == response->responseBytes) {
        return NULL;
    }
    PORT_Assert(response->responseBytes != NULL);
    PORT_Assert(response->responseBytes->responseTypeTag
		== SEC_OID_PKIX_OCSP_BASIC_RESPONSE);

    basic = response->responseBytes->decodedResponse.basic;
    PORT_Assert(basic != NULL);

    return &(basic->responseSignature);
}


/*
 * FUNCTION: CERT_DestroyOCSPResponse
 *   Frees an OCSP Response structure.
 * INPUTS:
 *   CERTOCSPResponse *request
 *     Pointer to CERTOCSPResponse to be freed.
 * RETURN:
 *   No return value; no errors.
 */
void
CERT_DestroyOCSPResponse(CERTOCSPResponse *response)
{
    if (response != NULL) {
	ocspSignature *signature = ocsp_GetResponseSignature(response);
	if (signature && signature->cert != NULL)
	    CERT_DestroyCertificate(signature->cert);

	/*
	 * We should actually never have a response without an arena,
	 * but check just in case.  (If there isn't one, there is not
	 * much we can do about it...)
	 */
	PORT_Assert(response->arena != NULL);
	if (response->arena != NULL) {
	    PORT_FreeArena(response->arena, PR_FALSE);
	}
    }
}


/*
 * OVERALL OCSP CLIENT SUPPORT (make and send a request, verify a response):
 */


/*
 * Pick apart a URL, saving the important things in the passed-in pointers.
 *
 * We expect to find "http://<hostname>[:<port>]/[path]", though we will
 * tolerate that final slash character missing, as well as beginning and
 * trailing whitespace, and any-case-characters for "http".  All of that
 * tolerance is what complicates this routine.  What we want is just to
 * pick out the hostname, the port, and the path.
 *
 * On a successful return, the caller will need to free the output pieces
 * of hostname and path, which are copies of the values found in the url.
 */
static SECStatus
ocsp_ParseURL(char *url, char **pHostname, PRUint16 *pPort, char **pPath)
{
    unsigned short port = 80;		/* default, in case not in url */
    char *hostname = NULL;
    char *path = NULL;
    char *save;
    char c;
    int len;

    if (url == NULL)
	goto loser;

    /*
     * Skip beginning whitespace.
     */
    c = *url;
    while ((c == ' ' || c == '\t') && c != '\0') {
	url++;
	c = *url;
    }
    if (c == '\0')
	goto loser;

    /*
     * Confirm, then skip, protocol.  (Since we only know how to do http,
     * that is all we will accept).
     */
    if (PORT_Strncasecmp(url, "http://", 7) != 0)
	goto loser;
    url += 7;

    /*
     * Whatever comes next is the hostname (or host IP address).  We just
     * save it aside and then search for its end so we can determine its
     * length and copy it.
     *
     * XXX Note that because we treat a ':' as a terminator character
     * (and below, we expect that to mean there is a port specification
     * immediately following), we will not handle IPv6 addresses.  That is
     * apparently an acceptable limitation, for the time being.  Some day,
     * when there is a clear way to specify a URL with an IPv6 address that
     * can be parsed unambiguously, this code should be made to do that.
     */
    save = url;
    c = *url;
    while (c != '/' && c != ':' && c != '\0' && c != ' ' && c != '\t') {
	url++;
	c = *url;
    }
    len = url - save;
    hostname = PORT_Alloc(len + 1);
    if (hostname == NULL)
	goto loser;
    PORT_Memcpy(hostname, save, len);
    hostname[len] = '\0';

    /*
     * Now we figure out if there was a port specified or not.
     * If so, we need to parse it (as a number) and skip it.
     */
    if (c == ':') {
	url++;
	port = (unsigned short) PORT_Atoi(url);
	c = *url;
	while (c != '/' && c != '\0' && c != ' ' && c != '\t') {
	    if (c < '0' || c > '9')
		goto loser;
	    url++;
	    c = *url;
	}
    }

    /*
     * Last thing to find is a path.  There *should* be a slash,
     * if nothing else -- but if there is not we provide one.
     */
    if (c == '/') {
	save = url;
	while (c != '\0' && c != ' ' && c != '\t') {
	    url++;
	    c = *url;
	}
	len = url - save;
	path = PORT_Alloc(len + 1);
	if (path == NULL)
	    goto loser;
	PORT_Memcpy(path, save, len);
	path[len] = '\0';
    } else {
	path = PORT_Strdup("/");
    }

    *pHostname = hostname;
    *pPort = port;
    *pPath = path;
    return SECSuccess;

loser:
    if (hostname != NULL)
	PORT_Free(hostname);
    if (path != NULL)
	PORT_Free(path);
    PORT_SetError(SEC_ERROR_CERT_BAD_ACCESS_LOCATION);
    return SECFailure;
}

/*
 * Open a socket to the specified host on the specified port, and return it.
 * The host is either a hostname or an IP address.
 */
static PRFileDesc *
ocsp_ConnectToHost(const char *host, PRUint16 port)
{
    PRFileDesc *sock = NULL;
    PRIntervalTime timeout;
    PRNetAddr addr;
    char *netdbbuf = NULL;

    sock = PR_NewTCPSocket();
    if (sock == NULL)
	goto loser;

    /* XXX Some day need a way to set (and get?) the following value */
    timeout = PR_SecondsToInterval(30);

    /*
     * If the following converts an IP address string in "dot notation"
     * into a PRNetAddr.  If it fails, we assume that is because we do not
     * have such an address, but instead a host *name*.  In that case we
     * then lookup the host by name.  Using the NSPR function this way
     * means we do not have to have our own logic for distinguishing a
     * valid numerical IP address from a hostname.
     */
    if (PR_StringToNetAddr(host, &addr) != PR_SUCCESS) {
	PRIntn hostIndex;
	PRHostEnt hostEntry;

	netdbbuf = PORT_Alloc(PR_NETDB_BUF_SIZE);
	if (netdbbuf == NULL)
	    goto loser;

	if (PR_GetHostByName(host, netdbbuf, PR_NETDB_BUF_SIZE,
			     &hostEntry) != PR_SUCCESS)
	    goto loser;

	hostIndex = 0;
	do {
	    hostIndex = PR_EnumerateHostEnt(hostIndex, &hostEntry, port, &addr);
	    if (hostIndex <= 0)
		goto loser;
	} while (PR_Connect(sock, &addr, timeout) != PR_SUCCESS);

	PORT_Free(netdbbuf);
    } else {
	/*
	 * First put the port into the address, then connect.
	 */
	if (PR_InitializeNetAddr(PR_IpAddrNull, port, &addr) != PR_SUCCESS)
	    goto loser;
	if (PR_Connect(sock, &addr, timeout) != PR_SUCCESS)
	    goto loser;
    }

    return sock;

loser:
    if (sock != NULL)
	PR_Close(sock);
    if (netdbbuf != NULL)
	PORT_Free(netdbbuf);
    return NULL;
}

/*
 * Sends an encoded OCSP request to the server identified by "location",
 * and returns the socket on which it was sent (so can listen for the reply).
 * "location" is expected to be a valid URL -- an error parsing it produces
 * SEC_ERROR_CERT_BAD_ACCESS_LOCATION.  Other errors are likely problems
 * connecting to it, or writing to it, or allocating memory, and the low-level
 * errors appropriate to the problem will be set.
 */
static PRFileDesc *
ocsp_SendEncodedRequest(char *location, SECItem *encodedRequest)
{
    char *hostname = NULL;
    char *path = NULL;
    PRUint16 port;
    SECStatus rv;
    PRFileDesc *sock = NULL;
    PRFileDesc *returnSock = NULL;
    char *header = NULL;

    /*
     * Take apart the location, getting the hostname, port, and path.
     */
    rv = ocsp_ParseURL(location, &hostname, &port, &path);
    if (rv != SECSuccess)
	goto loser;

    PORT_Assert(hostname != NULL);
    PORT_Assert(path != NULL);

    sock = ocsp_ConnectToHost(hostname, port);
    if (sock == NULL)
	goto loser;

    header = PR_smprintf("POST %s HTTP/1.0\r\n"
			 "Host: %s:%d\r\n"
			 "Content-Type: application/ocsp-request\r\n"
			 "Content-Length: %u\r\n\r\n",
			 path, hostname, port, encodedRequest->len);
    if (header == NULL)
	goto loser;

    /*
     * The NSPR documentation promises that if it can, it will write the full
     * amount; this will not return a partial value expecting us to loop.
     */
    if (PR_Write(sock, header, (PRInt32) PORT_Strlen(header)) < 0)
	goto loser;

    if (PR_Write(sock, encodedRequest->data,
		 (PRInt32) encodedRequest->len) < 0)
	goto loser;

    returnSock = sock;
    sock = NULL;

loser:
    if (header != NULL)
	PORT_Free(header);
    if (sock != NULL)
	PR_Close(sock);
    if (path != NULL)
	PORT_Free(path);
    if (hostname != NULL)
	PORT_Free(hostname);

    return returnSock;
}

/*
 * Read from "fd" into "buf" -- expect/attempt to read a given number of bytes
 * Obviously, stop if hit end-of-stream. Timeout is passed in.
 */

static int
ocsp_read(PRFileDesc *fd, char *buf, int toread, PRIntervalTime timeout)
{
    int total = 0;

    while (total < toread)
    {
        PRInt32 got;

        got = PR_Recv(fd, buf + total, (PRInt32) (toread - total), 0, timeout);
        if (got < 0)
        {
            if (0 == total)
            {
                total = -1; /* report the error if we didn't read anything yet */
            }
            break;
        }
        else
        if (got == 0)
        {			/* EOS */
            break;
        }

        total += got;
    }

    return total;
}

#define OCSP_BUFSIZE 1024

#define AbortHttpDecode(error) \
{ \
        if (inBuffer) \
            PORT_Free(inBuffer); \
        PORT_SetError(error); \
        return NULL; \
}


/*
 * Reads on the given socket and returns an encoded response when received.
 * Properly formatted HTTP/1.0 response headers are expected to be read
 * from the socket, preceding a binary-encoded OCSP response.  Problems
 * with parsing cause the error SEC_ERROR_OCSP_BAD_HTTP_RESPONSE to be
 * set; any other problems are likely low-level i/o or memory allocation
 * errors.
 */
static SECItem *
ocsp_GetEncodedResponse(PRArenaPool *arena, PRFileDesc *sock)
{
    /* first read HTTP status line and headers */

    char* inBuffer = NULL;
    PRInt32 offset = 0;
    PRInt32 inBufsize = 0;
    const PRInt32 bufSizeIncrement = OCSP_BUFSIZE; /* 1 KB at a time */
    const PRInt32 maxBufSize = 8 * bufSizeIncrement ; /* 8 KB max */
    const char* CRLF = "\r\n";
    const PRInt32 CRLFlen = strlen(CRLF);
    const char* headerEndMark = "\r\n\r\n";
    const PRInt32 markLen = strlen(headerEndMark);
    const PRIntervalTime ocsptimeout =
        PR_SecondsToInterval(30); /* hardcoded to 30s for now */
    char* headerEnd = NULL;
    PRBool EOS = PR_FALSE;
    const char* httpprotocol = "HTTP/";
    const PRInt32 httplen = strlen(httpprotocol);
    const char* httpcode = NULL;
    const char* contenttype = NULL;
    PRInt32 contentlength = 0;
    PRInt32 bytesRead = 0;
    char* statusLineEnd = NULL;
    char* space = NULL;
    char* nextHeader = NULL;
    SECItem* result = NULL;

    /* read up to at least the end of the HTTP headers */
    do
    {
        inBufsize += bufSizeIncrement;
        inBuffer = PORT_Realloc(inBuffer, inBufsize+1);
        if (NULL == inBuffer)
        {
            AbortHttpDecode(SEC_ERROR_NO_MEMORY);
        }
        bytesRead = ocsp_read(sock, inBuffer + offset, bufSizeIncrement,
            ocsptimeout);
        if (bytesRead > 0)
        {
            PRInt32 searchOffset = (offset - markLen) >0 ? offset-markLen : 0;
            offset += bytesRead;
            *(inBuffer + offset) = '\0'; /* NULL termination */
            headerEnd = strstr((const char*)inBuffer + searchOffset, headerEndMark);
            if (bytesRead < bufSizeIncrement)
            {
                /* we read less data than requested, therefore we are at
                   EOS or there was a read error */
                EOS = PR_TRUE;
            }
        }
        else
        {
            /* recv error or EOS */
            EOS = PR_TRUE;
        }
    } while ( (!headerEnd) && (PR_FALSE == EOS) &&
              (inBufsize < maxBufSize) );

    if (!headerEnd)
    {
        AbortHttpDecode(SEC_ERROR_OCSP_BAD_HTTP_RESPONSE);
    }

    /* parse the HTTP status line  */
    statusLineEnd = strstr((const char*)inBuffer, CRLF);
    if (!statusLineEnd)
    {
        AbortHttpDecode(SEC_ERROR_OCSP_BAD_HTTP_RESPONSE);
    }
    *statusLineEnd = '\0';

    /* check for HTTP/ response */
    space = strchr((const char*)inBuffer, ' ');
    if (!space || PORT_Strncasecmp((const char*)inBuffer, httpprotocol, httplen) != 0 )
    {
        AbortHttpDecode(SEC_ERROR_OCSP_BAD_HTTP_RESPONSE);
    }

    /* check the HTTP status code of 200 */
    httpcode = space +1;
    space = strchr(httpcode, ' ');
    if (!space)
    {
        AbortHttpDecode(SEC_ERROR_OCSP_BAD_HTTP_RESPONSE);
    }
    *space = 0;
    if (0 != strcmp(httpcode, "200"))
    {
        AbortHttpDecode(SEC_ERROR_OCSP_BAD_HTTP_RESPONSE);
    }

    /* parse the HTTP headers in the buffer . We only care about
       content-type and content-length
    */

    nextHeader = statusLineEnd + CRLFlen;
    *headerEnd = '\0'; /* terminate */
    do
    {
        char* thisHeaderEnd = NULL;
        char* value = NULL;
        char* colon = strchr(nextHeader, ':');
        
        if (!colon)
        {
            AbortHttpDecode(SEC_ERROR_OCSP_BAD_HTTP_RESPONSE);
        }

        *colon = '\0';
        value = colon + 1;

        /* jpierre - note : the following code will only handle the basic form
           of HTTP/1.0 response headers, of the form "name: value" . Headers
           split among multiple lines are not supported. This is not common
           and should not be an issue, but it could become one in the
           future */

        if (*value != ' ')
        {
            AbortHttpDecode(SEC_ERROR_OCSP_BAD_HTTP_RESPONSE);
        }

        value++;
        thisHeaderEnd  = strstr(value, CRLF);
        if (thisHeaderEnd )
        {
            *thisHeaderEnd  = '\0';
        }

        if (0 == PORT_Strcasecmp(nextHeader, "content-type"))
        {
            contenttype = value;
        }
        else
        if (0 == PORT_Strcasecmp(nextHeader, "content-length"))
        {
            contentlength = atoi(value);
        }

        if (thisHeaderEnd )
        {
            nextHeader = thisHeaderEnd + CRLFlen;
        }
        else
        {
            nextHeader = NULL;
        }

    } while (nextHeader && (nextHeader < (headerEnd + CRLFlen) ) );

    /* check content-type */
    if (!contenttype ||
        (0 != PORT_Strcasecmp(contenttype, "application/ocsp-response")) )
    {
        AbortHttpDecode(SEC_ERROR_OCSP_BAD_HTTP_RESPONSE);
    }

    /* read the body of the OCSP response */
    offset = offset - (PRInt32) (headerEnd - (const char*)inBuffer) - markLen;
    if (offset)
    {
        /* move all data to the beginning of the buffer */
        PORT_Memmove(inBuffer, headerEnd + markLen, offset);
    }

    /* resize buffer to only what's needed to hold the current response */
    inBufsize = (1 + (offset-1) / bufSizeIncrement ) * bufSizeIncrement ;

    while ( (PR_FALSE == EOS) &&
            ( (contentlength == 0) || (offset < contentlength) ) &&
            (inBufsize < maxBufSize)
            )
    {
        /* we still need to receive more body data */
        inBufsize += bufSizeIncrement;
        inBuffer = PORT_Realloc(inBuffer, inBufsize+1);
        if (NULL == inBuffer)
        {
            AbortHttpDecode(SEC_ERROR_NO_MEMORY);
        }
        bytesRead = ocsp_read(sock, inBuffer + offset, bufSizeIncrement,
                              ocsptimeout);
        if (bytesRead > 0)
        {
            offset += bytesRead;
            if (bytesRead < bufSizeIncrement)
            {
                /* we read less data than requested, therefore we are at
                   EOS or there was a read error */
                EOS = PR_TRUE;
            }
        }
        else
        {
            /* recv error or EOS */
            EOS = PR_TRUE;
        }
    }

    if (0 == offset)
    {
        AbortHttpDecode(SEC_ERROR_OCSP_BAD_HTTP_RESPONSE);
    }

    /*
     * Now allocate the item to hold the data.
     */
    result = SECITEM_AllocItem(arena, NULL, offset);
    if (NULL == result)
    {
        AbortHttpDecode(SEC_ERROR_NO_MEMORY);
    }

    /*
     * And copy the data left in the buffer.
    */
    PORT_Memcpy(result->data, inBuffer, offset);

    /* and free the temporary buffer */
    PORT_Free(inBuffer);
    return result;
}


/*
 * FUNCTION: CERT_GetEncodedOCSPResponse
 *   Creates and sends a request to an OCSP responder, then reads and
 *   returns the (encoded) response.
 * INPUTS:
 *   PRArenaPool *arena
 *     Pointer to arena from which return value will be allocated.
 *     If NULL, result will be allocated from the heap (and thus should
 *     be freed via SECITEM_FreeItem).
 *   CERTCertList *certList
 *     A list of certs for which status will be requested.
 *     Note that all of these certificates should have the same issuer,
 *     or it's expected the response will be signed by a trusted responder.
 *     If the certs need to be broken up into multiple requests, that
 *     must be handled by the caller (and thus by having multiple calls
 *     to this routine), who knows about where the request(s) are being
 *     sent and whether there are any trusted responders in place.
 *   char *location
 *     The location of the OCSP responder (a URL).
 *   int64 time
 *     Indicates the time for which the certificate status is to be 
 *     determined -- this may be used in the search for the cert's issuer
 *     but has no other bearing on the operation.
 *   PRBool addServiceLocator
 *     If true, the Service Locator extension should be added to the
 *     single request(s) for each cert.
 *   CERTCertificate *signerCert
 *     If non-NULL, means sign the request using this cert.  Otherwise,
 *     do not sign.
 *   void *pwArg
 *     Pointer to argument for password prompting, if needed.  (Definitely
 *     not needed if not signing.)
 * OUTPUTS:
 *   CERTOCSPRequest **pRequest
 *     Pointer in which to store the OCSP request created for the given
 *     list of certificates.  It is only filled in if the entire operation
 *     is successful and the pointer is not null -- and in that case the
 *     caller is then reponsible for destroying it.
 * RETURN:
 *   Returns a pointer to the SECItem holding the response.
 *   On error, returns null with error set describing the reason:
 *	SEC_ERROR_UNKNOWN_ISSUER
 *	SEC_ERROR_CERT_BAD_ACCESS_LOCATION
 *	SEC_ERROR_OCSP_BAD_HTTP_RESPONSE
 *   Other errors are low-level problems (no memory, bad database, etc.).
 */
SECItem *
CERT_GetEncodedOCSPResponse(PRArenaPool *arena, CERTCertList *certList,
			    char *location, int64 time,
			    PRBool addServiceLocator,
			    CERTCertificate *signerCert, void *pwArg,
			    CERTOCSPRequest **pRequest)
{
    CERTOCSPRequest *request = NULL;
    SECItem *encodedRequest = NULL;
    SECItem *encodedResponse = NULL;
    PRFileDesc *sock = NULL;
    SECStatus rv;

    request = CERT_CreateOCSPRequest(certList, time, addServiceLocator,
				     signerCert);
    if (request == NULL)
	goto loser;

    rv = CERT_AddOCSPAcceptableResponses(request,
					 SEC_OID_PKIX_OCSP_BASIC_RESPONSE);
    if (rv != SECSuccess)
	goto loser;

    encodedRequest = CERT_EncodeOCSPRequest(NULL, request, pwArg);
    if (encodedRequest == NULL)
	goto loser;

    sock = ocsp_SendEncodedRequest(location, encodedRequest);
    if (sock == NULL)
	goto loser;

    encodedResponse = ocsp_GetEncodedResponse(arena, sock);
    if (encodedResponse != NULL && pRequest != NULL) {
	*pRequest = request;
	request = NULL;			/* avoid destroying below */
    }

loser:
    if (request != NULL)
	CERT_DestroyOCSPRequest(request);
    if (encodedRequest != NULL)
	SECITEM_FreeItem(encodedRequest, PR_TRUE);
    if (sock != NULL)
	PR_Close(sock);

    return encodedResponse;
}


/* Checks a certificate for the key usage extension of OCSP signer. */
static PRBool
ocsp_CertIsOCSPSigner(CERTCertificate *cert)
{
    SECStatus rv;
    SECItem extItem;
    SECItem **oids;
    SECItem *oid;
    SECOidTag oidTag;
    PRBool retval;
    CERTOidSequence *oidSeq = NULL;


    extItem.data = NULL;
    rv = CERT_FindCertExtension(cert, SEC_OID_X509_EXT_KEY_USAGE, &extItem);
    if ( rv != SECSuccess ) {
	goto loser;
    }

    oidSeq = CERT_DecodeOidSequence(&extItem);
    if ( oidSeq == NULL ) {
	goto loser;
    }

    oids = oidSeq->oids;
    while ( *oids != NULL ) {
	oid = *oids;
	
	oidTag = SECOID_FindOIDTag(oid);
	
	if ( oidTag == SEC_OID_OCSP_RESPONDER ) {
	    goto success;
	}
	
	oids++;
    }

loser:
    retval = PR_FALSE;
    goto done;
success:
    retval = PR_TRUE;
done:
    if ( extItem.data != NULL ) {
	PORT_Free(extItem.data);
    }
    if ( oidSeq != NULL ) {
	CERT_DestroyOidSequence(oidSeq);
    }
    
    return(retval);
}


#ifdef LATER	/*
		 * XXX This function is not currently used, but will
		 * be needed later when we do revocation checking of
		 * the responder certificate.  Of course, it may need
		 * revising then, if the cert extension interface has
		 * changed.  (Hopefully it will!)
		 */

/* Checks a certificate to see if it has the OCSP no check extension. */
static PRBool
ocsp_CertHasNoCheckExtension(CERTCertificate *cert)
{
    SECStatus rv;
    SECItem extItem;
    
    extItem.data = NULL;
    rv = CERT_FindCertExtension(cert, SEC_OID_PKIX_OCSP_NO_CHECK, 
				&extItem);
    if (extItem.data != NULL) {
	PORT_Free(extItem.data);
    }
    if (rv == SECSuccess) {
	return PR_TRUE;
    }
    return PR_FALSE;
}
#endif	/* LATER */

static PRBool
ocsp_matchcert(SECItem *certIndex,CERTCertificate *testCert)
{
    SECItem item;
    unsigned char buf[HASH_LENGTH_MAX];

    item.data = buf;
    item.len = SHA1_LENGTH;

    if (CERT_SPKDigestValueForCert(NULL,testCert,SEC_OID_SHA1, &item) == NULL) {
	return PR_FALSE;
    }
    if  (SECITEM_ItemsAreEqual(certIndex,&item)) {
	return PR_TRUE;
    }
    if (CERT_SPKDigestValueForCert(NULL,testCert,SEC_OID_MD5, &item) == NULL) {
	return PR_FALSE;
    }
    if  (SECITEM_ItemsAreEqual(certIndex,&item)) {
	return PR_TRUE;
    }
    if (CERT_SPKDigestValueForCert(NULL,testCert,SEC_OID_MD2, &item) == NULL) {
	return PR_FALSE;
    }
    if  (SECITEM_ItemsAreEqual(certIndex,&item)) {
	return PR_TRUE;
    }

    return PR_FALSE;
}

static CERTCertificate *
ocsp_CertGetDefaultResponder(CERTCertDBHandle *handle,CERTOCSPCertID *certID);

/*
 * Check the signature on some OCSP data.  This is a helper function that
 * can be used to check either a request or a response.  The result is
 * saved in the signature structure itself for future reference (to avoid
 * repeating the expensive verification operation), as well as returned.
 * In addition to checking the signature, the certificate (and its chain)
 * are also checked for validity (at the specified time) and usage.
 *
 * The type of cert lookup to be performed is specified by "lookupByName":
 * if true, then "certIndex" is actually a CERTName; otherwise it is a
 * SECItem which contains a key hash.
 *
 * If the signature verifies okay, and the argument "pSignerCert" is not
 * null, that parameter will be filled-in with a pointer to the signer's
 * certificate.  The caller is then responsible for destroying the cert.
 *
 * A return of SECSuccess means the verification succeeded.  If not,
 * an error will be set with the reason.  Most likely are:
 *	SEC_ERROR_UNKNOWN_SIGNER - signer's cert could not be found
 *	SEC_ERROR_BAD_SIGNATURE - the signature did not verify
 * Other errors are any of the many possible failures in cert verification
 * (e.g. SEC_ERROR_REVOKED_CERTIFICATE, SEC_ERROR_UNTRUSTED_ISSUER) when
 * verifying the signer's cert, or low-level problems (no memory, etc.)
 */
static SECStatus
ocsp_CheckSignature(ocspSignature *signature, void *tbs,
		    const SEC_ASN1Template *encodeTemplate,
		    CERTCertDBHandle *handle, SECCertUsage certUsage,
		    int64 checkTime, PRBool lookupByName, void *certIndex,
		    void *pwArg, CERTCertificate **pSignerCert,
		    CERTCertificate *issuer)
{
    SECItem rawSignature;
    SECItem *encodedTBS = NULL;
    CERTCertificate *responder = NULL;
    CERTCertificate *signerCert = NULL;
    SECKEYPublicKey *signerKey = NULL;
    CERTCertificate **certs = NULL;
    SECStatus rv = SECFailure;
    int certCount;
    int i;

    /*
     * If this signature has already gone through verification, just
     * return the cached result.
     */
    if (signature->wasChecked) {
	if (signature->status == SECSuccess) {
	    if (pSignerCert != NULL)
		*pSignerCert = CERT_DupCertificate(signature->cert);
	} else {
	    PORT_SetError(signature->failureReason);
	}
	return signature->status;
    }

    /*
     * If the signature contains some certificates as well, temporarily
     * import them in case they are needed for verification.
     *
     * Note that the result of this is that each cert in "certs" needs
     * to be destroyed.
     */
    certCount = 0;
    if (signature->derCerts != NULL) {
	for (; signature->derCerts[certCount] != NULL; certCount++) {
	    /* just counting */
	    /*IMPORT CERT TO SPKI TABLE */
	}
    }
    rv = CERT_ImportCerts(handle, certUsage, certCount,
			  signature->derCerts, &certs,
			  PR_FALSE, PR_FALSE, NULL);
    if (rv != SECSuccess)
	goto finish;

    /*
     * Now look up the certificate that did the signing.
     * The signer can be specified either by name or by key hash.
     */
    if (lookupByName) {
	SECItem *encodedName;

	encodedName = SEC_ASN1EncodeItem(NULL, NULL, certIndex,
					 CERT_NameTemplate);
	if (encodedName == NULL)
	    goto finish;

	signerCert = CERT_FindCertByName(handle, encodedName);
	SECITEM_FreeItem(encodedName, PR_TRUE);
    } else {
	/*
	 * The signer is either 1) a known issuer CA we passed in,
	 * 2) the default OCSP responder, or 3) an intermediate CA
	 * passed in the cert list to use. Figure out which it is.
	 */
	responder = ocsp_CertGetDefaultResponder(handle,NULL);
	if (responder && ocsp_matchcert(certIndex,responder)) {
	    signerCert = CERT_DupCertificate(responder);
	} else if (issuer && ocsp_matchcert(certIndex,issuer)) {
	    signerCert = CERT_DupCertificate(issuer);
	} 
	for (i=0; (signerCert == NULL) && (i < certCount); i++) {
	    if (ocsp_matchcert(certIndex,certs[i])) {
		signerCert = CERT_DupCertificate(certs[i]);
	    }
	}
    }

    if (signerCert == NULL) {
	rv = SECFailure;
	if (PORT_GetError() == SEC_ERROR_UNKNOWN_CERT) {
	    /* Make the error a little more specific. */
	    PORT_SetError(SEC_ERROR_UNKNOWN_SIGNER);
	}
	goto finish;
    }

    /*
     * We could mark this true at the top of this function, or always
     * below at "finish", but if the problem was just that we could not
     * find the signer's cert, leave that as if the signature hasn't
     * been checked in case a subsequent call might have better luck.
     */
    signature->wasChecked = PR_TRUE;

    /*
     * Just because we have a cert does not mean it is any good; check
     * it for validity, trust and usage.
     */
    rv = CERT_VerifyCert(handle, signerCert, PR_TRUE, certUsage, checkTime,
			 pwArg, NULL);
    if (rv != SECSuccess) {
        PORT_SetError(SEC_ERROR_OCSP_INVALID_SIGNING_CERT);
	goto finish;
    }

    /*
     * Now get the public key from the signer's certificate; we need
     * it to perform the verification.
     */
    signerKey = CERT_ExtractPublicKey(signerCert);
    if (signerKey == NULL)
	goto finish;

    /*
     * Prepare the data to be verified; it needs to be DER encoded first.
     */
    encodedTBS = SEC_ASN1EncodeItem(NULL, NULL, tbs, encodeTemplate);
    if (encodedTBS == NULL)
	goto finish;

    /*
     * We copy the signature data *pointer* and length, so that we can
     * modify the length without damaging the original copy.  This is a
     * simple copy, not a dup, so no destroy/free is necessary.
     */
    rawSignature = signature->signature;
    /*
     * The raw signature is a bit string, but we need to represent its
     * length in bytes, because that is what the verify function expects.
     */
    DER_ConvertBitString(&rawSignature);

    rv = VFY_VerifyData(encodedTBS->data, encodedTBS->len, signerKey,
			&rawSignature,
			SECOID_GetAlgorithmTag(&signature->signatureAlgorithm),
			pwArg);

finish:
    if (signature->wasChecked)
	signature->status = rv;

    if (rv != SECSuccess) {
	signature->failureReason = PORT_GetError();
	if (signerCert != NULL)
	    CERT_DestroyCertificate(signerCert);
    } else {
	/*
	 * Save signer's certificate in signature.
	 */
	signature->cert = signerCert;
	if (pSignerCert != NULL) {
	    /*
	     * Pass pointer to signer's certificate back to our caller,
	     * who is also now responsible for destroying it.
	     */
	    *pSignerCert = CERT_DupCertificate(signerCert);
	}
    }

    if (encodedTBS != NULL)
	SECITEM_FreeItem(encodedTBS, PR_TRUE);

    if (signerKey != NULL)
	SECKEY_DestroyPublicKey(signerKey);

    if (certs != NULL)
	CERT_DestroyCertArray(certs, certCount);
	/* Free CERTS from SPKDigest Table */

    return rv;
}


/*
 * FUNCTION: CERT_VerifyOCSPResponseSignature
 *   Check the signature on an OCSP Response.  Will also perform a
 *   verification of the signer's certificate.  Note, however, that a
 *   successful verification does not make any statement about the
 *   signer's *authority* to provide status for the certificate(s),
 *   that must be checked individually for each certificate.
 * INPUTS:
 *   CERTOCSPResponse *response
 *     Pointer to response structure with signature to be checked.
 *   CERTCertDBHandle *handle
 *     Pointer to CERTCertDBHandle for certificate DB to use for verification.
 *   void *pwArg
 *     Pointer to argument for password prompting, if needed.
 * OUTPUTS:
 *   CERTCertificate **pSignerCert
 *     Pointer in which to store signer's certificate; only filled-in if
 *     non-null.
 * RETURN:
 *   Returns SECSuccess when signature is valid, anything else means invalid.
 *   Possible errors set:
 *	SEC_ERROR_OCSP_MALFORMED_RESPONSE - unknown type of ResponderID
 *	SEC_ERROR_INVALID_TIME - bad format of "ProducedAt" time
 *	SEC_ERROR_UNKNOWN_SIGNER - signer's cert could not be found
 *	SEC_ERROR_BAD_SIGNATURE - the signature did not verify
 *   Other errors are any of the many possible failures in cert verification
 *   (e.g. SEC_ERROR_REVOKED_CERTIFICATE, SEC_ERROR_UNTRUSTED_ISSUER) when
 *   verifying the signer's cert, or low-level problems (no memory, etc.)
 */
SECStatus
CERT_VerifyOCSPResponseSignature(CERTOCSPResponse *response,	
				 CERTCertDBHandle *handle, void *pwArg,
				 CERTCertificate **pSignerCert,
				 CERTCertificate *issuer)
{
    ocspResponseData *tbsData;		/* this is what is signed */
    PRBool byName;
    void *certIndex;
    int64 producedAt;
    SECStatus rv;

    tbsData = ocsp_GetResponseData(response);

    PORT_Assert(tbsData->responderID != NULL);
    switch (tbsData->responderID->responderIDType) {
      case ocspResponderID_byName:
	byName = PR_TRUE;
	certIndex = &tbsData->responderID->responderIDValue.name;
	break;
      case ocspResponderID_byKey:
	byName = PR_FALSE;
	certIndex = &tbsData->responderID->responderIDValue.keyHash;
	break;
      case ocspResponderID_other:
      default:
	PORT_Assert(0);
	PORT_SetError(SEC_ERROR_OCSP_MALFORMED_RESPONSE);
	return SECFailure;
    }

    /*
     * ocsp_CheckSignature will also verify the signer certificate; we
     * need to tell it *when* that certificate must be valid -- for our
     * purposes we expect it to be valid when the response was signed.
     * The value of "producedAt" is the signing time.
     */
    rv = DER_GeneralizedTimeToTime(&producedAt, &tbsData->producedAt);
    if (rv != SECSuccess)
	return rv;

    return ocsp_CheckSignature(ocsp_GetResponseSignature(response),
			       tbsData, ocsp_ResponseDataTemplate,
			       handle, certUsageStatusResponder, producedAt,
			       byName, certIndex, pwArg, pSignerCert, issuer);
}

/*
 * See if two certIDs match.  This can be easy or difficult, depending
 * on whether the same hash algorithm was used.
 */
static PRBool
ocsp_CertIDsMatch(CERTCertDBHandle *handle,
		  CERTOCSPCertID *certID1, CERTOCSPCertID *certID2)
{
    PRBool match = PR_FALSE;
    SECItem *foundHash = NULL;
    SECOidTag hashAlg;
    SECItem *keyHash = NULL;
    SECItem *nameHash = NULL;

    /*
     * In order to match, they must have the same issuer and the same
     * serial number.
     *
     * We just compare the easier things first.
     */
    if (SECITEM_CompareItem(&certID1->serialNumber,
			    &certID2->serialNumber) != SECEqual) {
	goto done;
    }

    if (SECOID_CompareAlgorithmID(&certID1->hashAlgorithm,
				  &certID2->hashAlgorithm) == SECEqual) {
	/*
	 * If the hash algorithms match then we can do a simple compare
	 * of the hash values themselves.
	 */
	if ((SECITEM_CompareItem(&certID1->issuerNameHash,
				 &certID2->issuerNameHash) == SECEqual)
	    && (SECITEM_CompareItem(&certID1->issuerKeyHash,
				    &certID2->issuerKeyHash) == SECEqual)) {
	    match = PR_TRUE;
	}
	goto done;
    }

    hashAlg = SECOID_FindOIDTag(&certID2->hashAlgorithm.algorithm);
    switch (hashAlg) {
    case SEC_OID_SHA1:
	keyHash = &certID1->issuerSHA1KeyHash;
	nameHash = &certID1->issuerSHA1NameHash;
	break;
    case SEC_OID_MD5:
	keyHash = &certID1->issuerMD5KeyHash;
	nameHash = &certID1->issuerMD5NameHash;
	break;
    case SEC_OID_MD2:
	keyHash = &certID1->issuerMD2KeyHash;
	nameHash = &certID1->issuerMD2NameHash;
	break;
    default:
	foundHash = NULL;
	break;
    }

    if (foundHash == NULL) {
	goto done;
    }
    PORT_Assert(keyHash && nameHash);

    if ((SECITEM_CompareItem(nameHash, &certID2->issuerNameHash) == SECEqual)
      && (SECITEM_CompareItem(keyHash, &certID2->issuerKeyHash) == SECEqual)) {
	match = PR_TRUE;
    }

done:
    return match;
}

/*
 * Find the single response for the cert specified by certID.
 * No copying is done; this just returns a pointer to the appropriate
 * response within responses, if it is found (and null otherwise).
 * This is fine, of course, since this function is internal-use only.
 */
static CERTOCSPSingleResponse *
ocsp_GetSingleResponseForCertID(CERTOCSPSingleResponse **responses,
				CERTCertDBHandle *handle,
				CERTOCSPCertID *certID)
{
    CERTOCSPSingleResponse *single;
    int i;

    if (responses == NULL)
	return NULL;

    for (i = 0; responses[i] != NULL; i++) {
	single = responses[i];
	if (ocsp_CertIDsMatch(handle, certID, single->certID) == PR_TRUE) {
	    return single;
	}
    }

    /*
     * The OCSP server should have included a response even if it knew
     * nothing about the certificate in question.  Since it did not,
     * this will make it look as if it had.
     * 
     * XXX Should we make this a separate error to notice the server's
     * bad behavior?
     */
    PORT_SetError(SEC_ERROR_OCSP_UNKNOWN_CERT);
    return NULL;
}

static ocspCheckingContext *
ocsp_GetCheckingContext(CERTCertDBHandle *handle)
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

    if (ocspcx == NULL)
	PORT_SetError(SEC_ERROR_OCSP_NOT_ENABLED);

    return ocspcx;
}
/*
 * Return true if the given signerCert is the default responder for
 * the given certID.  If not, or if any error, return false.
 */
static CERTCertificate *
ocsp_CertGetDefaultResponder(CERTCertDBHandle *handle,CERTOCSPCertID *certID)
{
    ocspCheckingContext *ocspcx;

    ocspcx = ocsp_GetCheckingContext(handle);
    if (ocspcx == NULL)
	goto loser;

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

/*
 * Return true if the given signerCert is the default responder for
 * the given certID.  If not, or if any error, return false.
 */
static PRBool
ocsp_CertIsDefaultResponderForCertID(CERTCertDBHandle *handle,
				     CERTCertificate *signerCert,
				     CERTOCSPCertID *certID)
{
    CERTCertificate *defaultResponderCert;

    defaultResponderCert = ocsp_CertGetDefaultResponder(handle, certID);
    return (PRBool) (defaultResponderCert == signerCert);
}

/*
 * Check that the given signer certificate is authorized to sign status
 * information for the given certID.  Return true if it is, false if not
 * (or if there is any error along the way).  If false is returned because
 * the signer is not authorized, the following error will be set:
 *	SEC_ERROR_OCSP_UNAUTHORIZED_RESPONSE
 * Other errors are low-level problems (no memory, bad database, etc.).
 *
 * There are three ways to be authorized.  In the order in which we check,
 * using the terms used in the OCSP spec, the signer must be one of:
 *  1.  A "trusted responder" -- it matches a local configuration
 *      of OCSP signing authority for the certificate in question.
 *  2.  The CA who issued the certificate in question.
 *  3.  A "CA designated responder", aka an "authorized responder" -- it
 *      must be represented by a special cert issued by the CA who issued
 *      the certificate in question.
 */
static PRBool
ocsp_AuthorizedResponderForCertID(CERTCertDBHandle *handle,
				  CERTCertificate *signerCert,
				  CERTOCSPCertID *certID,
				  int64 thisUpdate)
{
    CERTCertificate *issuerCert = NULL;
    SECItem *issuerKeyHash = NULL;
    SECOidTag hashAlg;
    PRBool okay = PR_FALSE;

    /*
     * Check first for a trusted responder, which overrides everything else.
     */
    if (ocsp_CertIsDefaultResponderForCertID(handle, signerCert, certID))
	return PR_TRUE;

    /*
     * In the other two cases, we need to do an issuer comparison.
     * How we do it depends on whether the signer certificate has the
     * special extension (for a designated responder) or not.
     */

    if (ocsp_CertIsOCSPSigner(signerCert)) {
	/*
	 * The signer is a designated responder.  Its issuer must match
	 * the issuer of the cert being checked.
	 */
	issuerCert = CERT_FindCertIssuer(signerCert, thisUpdate,
					 certUsageAnyCA);
	if (issuerCert == NULL) {
	    /*
	     * We could leave the SEC_ERROR_UNKNOWN_ISSUER error alone,
	     * but the following will give slightly more information.
	     * Once we have an error stack, things will be much better.
	     */
	    PORT_SetError(SEC_ERROR_OCSP_UNAUTHORIZED_RESPONSE);
	    goto loser;
	}
    } else {
	/*
	 * The signer must *be* the issuer of the cert being checked.
	 */
	issuerCert = signerCert;
    }

    hashAlg = SECOID_FindOIDTag(&certID->hashAlgorithm.algorithm);
    issuerKeyHash = CERT_SPKDigestValueForCert(NULL, issuerCert, hashAlg, NULL);
    if (issuerKeyHash == NULL)
	goto loser;

    if (SECITEM_CompareItem(issuerKeyHash,
			    &certID->issuerKeyHash) != SECEqual) {
	PORT_SetError(SEC_ERROR_OCSP_UNAUTHORIZED_RESPONSE);
	goto loser;
    }

    okay = PR_TRUE;

loser:
    if (issuerKeyHash != NULL)
	SECITEM_FreeItem(issuerKeyHash, PR_TRUE);

    if (issuerCert != NULL && issuerCert != signerCert)
	CERT_DestroyCertificate(issuerCert);

    return okay;
}

/*
 * We need to check that a responder gives us "recent" information.
 * Since a responder can pre-package responses, we need to pick an amount
 * of time that is acceptable to us, and reject any response that is
 * older than that.
 *
 * XXX This *should* be based on some configuration parameter, so that
 * different usages could specify exactly what constitutes "sufficiently
 * recent".  But that is not going to happen right away.  For now, we
 * want something from within the last 24 hours.  This macro defines that
 * number in seconds.
 */
#define OCSP_ALLOWABLE_LAPSE_SECONDS	(24L * 60L * 60L)

static PRBool
ocsp_TimeIsRecent(int64 checkTime)
{
    int64 now = PR_Now();
    int64 lapse, tmp;

    LL_I2L(lapse, OCSP_ALLOWABLE_LAPSE_SECONDS);
    LL_I2L(tmp, PR_USEC_PER_SEC);
    LL_MUL(lapse, lapse, tmp);		/* allowable lapse in microseconds */

    LL_ADD(checkTime, checkTime, lapse);
    if (LL_CMP(now, >, checkTime))
	return PR_FALSE;

    return PR_TRUE;
}

#define OCSP_SLOP (5L*60L) /* OCSP responses are allowed to be 5 minutes
                              in the future by default */

static PRUint32 ocspsloptime = OCSP_SLOP;	/* seconds */

/*
 * Check that this single response is okay.  A return of SECSuccess means:
 *   1. The signer (represented by "signerCert") is authorized to give status
 *	for the cert represented by the individual response in "single".
 *   2. The value of thisUpdate is earlier than now.
 *   3. The value of producedAt is later than or the same as thisUpdate.
 *   4. If nextUpdate is given:
 *	- The value of nextUpdate is later than now.
 *	- The value of producedAt is earlier than nextUpdate.
 *	Else if no nextUpdate:
 *	- The value of thisUpdate is fairly recent.
 *	- The value of producedAt is fairly recent.
 *	However we do not need to perform an explicit check for this last
 *	constraint because it is already guaranteed by checking that
 *	producedAt is later than thisUpdate and thisUpdate is recent.
 * Oh, and any responder is "authorized" to say that a cert is unknown to it.
 *
 * If any of those checks fail, SECFailure is returned and an error is set:
 *	SEC_ERROR_OCSP_FUTURE_RESPONSE
 *	SEC_ERROR_OCSP_OLD_RESPONSE
 *	SEC_ERROR_OCSP_UNAUTHORIZED_RESPONSE
 * Other errors are low-level problems (no memory, bad database, etc.).
 */ 
static SECStatus
ocsp_VerifySingleResponse(CERTOCSPSingleResponse *single,
			  CERTCertDBHandle *handle,
			  CERTCertificate *signerCert,
			  int64 producedAt)
{
    CERTOCSPCertID *certID = single->certID;
    int64 now, thisUpdate, nextUpdate, tmstamp, tmp;
    SECStatus rv;

    /*
     * If all the responder said was that the given cert was unknown to it,
     * that is a valid response.  Not very interesting to us, of course,
     * but all this function is concerned with is validity of the response,
     * not the status of the cert.
     */
    PORT_Assert(single->certStatus != NULL);
    if (single->certStatus->certStatusType == ocspCertStatus_unknown)
	return SECSuccess;

    /*
     * We need to extract "thisUpdate" for use below and to pass along
     * to AuthorizedResponderForCertID in case it needs it for doing an
     * issuer look-up.
     */
    rv = DER_GeneralizedTimeToTime(&thisUpdate, &single->thisUpdate);
    if (rv != SECSuccess)
	return rv;

    /*
     * First confirm that signerCert is authorized to give this status.
     */
    if (ocsp_AuthorizedResponderForCertID(handle, signerCert, certID,
					  thisUpdate) != PR_TRUE)
	return SECFailure;

    /*
     * Now check the time stuff, as described above.
     */
    now = PR_Now();
    /* allow slop time for future response */
    LL_UI2L(tmstamp, ocspsloptime); /* get slop time in seconds */
    LL_UI2L(tmp, PR_USEC_PER_SEC);
    LL_MUL(tmp, tmstamp, tmp); /* convert the slop time to PRTime */
    LL_ADD(tmstamp, tmp, now); /* add current time to it */

    if (LL_CMP(thisUpdate, >, tmstamp) || LL_CMP(producedAt, <, thisUpdate)) {
	PORT_SetError(SEC_ERROR_OCSP_FUTURE_RESPONSE);
	return SECFailure;
    }
    if (single->nextUpdate != NULL) {
	rv = DER_GeneralizedTimeToTime(&nextUpdate, single->nextUpdate);
	if (rv != SECSuccess)
	    return rv;

	LL_ADD(tmp, tmp, nextUpdate);
	if (LL_CMP(tmp, <, now) || LL_CMP(producedAt, >, nextUpdate)) {
	    PORT_SetError(SEC_ERROR_OCSP_OLD_RESPONSE);
	    return SECFailure;
	}
    } else if (ocsp_TimeIsRecent(thisUpdate) != PR_TRUE) {
	PORT_SetError(SEC_ERROR_OCSP_OLD_RESPONSE);
	return SECFailure;
    }

    return SECSuccess;
}


/*
 * FUNCTION: CERT_GetOCSPAuthorityInfoAccessLocation
 *   Get the value of the URI of the OCSP responder for the given cert.
 *   This is found in the (optional) Authority Information Access extension
 *   in the cert.
 * INPUTS:
 *   CERTCertificate *cert
 *     The certificate being examined.
 * RETURN:
 *   char *
 *     A copy of the URI for the OCSP method, if found.  If either the
 *     extension is not present or it does not contain an entry for OCSP,
 *     SEC_ERROR_EXTENSION_NOT_FOUND will be set and a NULL returned.
 *     Any other error will also result in a NULL being returned.
 *     
 *     This result should be freed (via PORT_Free) when no longer in use.
 */
char *
CERT_GetOCSPAuthorityInfoAccessLocation(CERTCertificate *cert)
{
    CERTGeneralName *locname = NULL;
    SECItem *location = NULL;
    SECItem *encodedAuthInfoAccess = NULL;
    CERTAuthInfoAccess **authInfoAccess = NULL;
    char *locURI = NULL;
    PRArenaPool *arena = NULL;
    SECStatus rv;
    int i;

    /*
     * Allocate this one from the heap because it will get filled in
     * by CERT_FindCertExtension which will also allocate from the heap,
     * and we can free the entire thing on our way out.
     */
    encodedAuthInfoAccess = SECITEM_AllocItem(NULL, NULL, 0);
    if (encodedAuthInfoAccess == NULL)
	goto loser;

    rv = CERT_FindCertExtension(cert, SEC_OID_X509_AUTH_INFO_ACCESS,
				encodedAuthInfoAccess);
    if (rv == SECFailure)
	goto loser;

    /*
     * The rest of the things allocated in the routine will come out of
     * this arena, which is temporary just for us to decode and get at the
     * AIA extension.  The whole thing will be destroyed on our way out,
     * after we have copied the location string (url) itself (if found).
     */
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL)
	goto loser;

    authInfoAccess = CERT_DecodeAuthInfoAccessExtension(arena,
							encodedAuthInfoAccess);
    if (authInfoAccess == NULL)
	goto loser;

    for (i = 0; authInfoAccess[i] != NULL; i++) {
	if (SECOID_FindOIDTag(&authInfoAccess[i]->method) == SEC_OID_PKIX_OCSP)
	    locname = authInfoAccess[i]->location;
    }

    /*
     * If we found an AIA extension, but it did not include an OCSP method,
     * that should look to our caller as if we did not find the extension
     * at all, because it is only an OCSP method that we care about.
     * So set the same error that would be set if the AIA extension was
     * not there at all.
     */
    if (locname == NULL) {
	PORT_SetError(SEC_ERROR_EXTENSION_NOT_FOUND);
	goto loser;
    }

    /*
     * The following is just a pointer back into locname (i.e. not a copy);
     * thus it should not be freed.
     */
    location = CERT_GetGeneralNameByType(locname, certURI, PR_FALSE);
    if (location == NULL) {
	/*
	 * XXX Appears that CERT_GetGeneralNameByType does not set an
	 * error if there is no name by that type.  For lack of anything
	 * better, act as if the extension was not found.  In the future
	 * this should probably be something more like the extension was
	 * badly formed.
	 */
	PORT_SetError(SEC_ERROR_EXTENSION_NOT_FOUND);
	goto loser;
    }

    /*
     * That location is really a string, but it has a specified length
     * without a null-terminator.  We need a real string that does have
     * a null-terminator, and we need a copy of it anyway to return to
     * our caller -- so allocate and copy.
     */
    locURI = PORT_Alloc(location->len + 1);
    if (locURI == NULL) {
	goto loser;
    }
    PORT_Memcpy(locURI, location->data, location->len);
    locURI[location->len] = '\0';

loser:
    if (arena != NULL)
	PORT_FreeArena(arena, PR_FALSE);

    if (encodedAuthInfoAccess != NULL)
	SECITEM_FreeItem(encodedAuthInfoAccess, PR_TRUE);

    return locURI;
}


/*
 * Figure out where we should go to find out the status of the given cert
 * via OCSP.  If a default responder is set up, that is our answer.
 * If not, see if the certificate has an Authority Information Access (AIA)
 * extension for OCSP, and return the value of that.  Otherwise return NULL.
 * We also let our caller know whether or not the responder chosen was
 * a default responder or not through the output variable isDefault;
 * its value has no meaning unless a good (non-null) value is returned
 * for the location.
 *
 * The result needs to be freed (PORT_Free) when no longer in use.
 */
static char *
ocsp_GetResponderLocation(CERTCertDBHandle *handle, CERTCertificate *cert,
			  PRBool *isDefault)
{
    ocspCheckingContext *ocspcx;

    ocspcx = ocsp_GetCheckingContext(handle);
    if (ocspcx != NULL && ocspcx->useDefaultResponder) {
	/*
	 * A default responder wins out, if specified.
	 * XXX Someday this may be a more complicated determination based
	 * on the cert's issuer.  (That is, we could have different default
	 * responders configured for different issuers.)
	 */
	PORT_Assert(ocspcx->defaultResponderURI != NULL);
	*isDefault = PR_TRUE;
	return (PORT_Strdup(ocspcx->defaultResponderURI));
    }

    /*
     * No default responder set up, so go see if we can find an AIA
     * extension that has a value for OCSP, and get the url from that.
     */
    *isDefault = PR_FALSE;
    return CERT_GetOCSPAuthorityInfoAccessLocation(cert);
}

/*
 * Return SECSuccess if the cert was revoked *after* "time",
 * SECFailure otherwise.
 */
static SECStatus
ocsp_CertRevokedAfter(ocspRevokedInfo *revokedInfo, int64 time)
{
    int64 revokedTime;
    SECStatus rv;

    rv = DER_GeneralizedTimeToTime(&revokedTime, &revokedInfo->revocationTime);
    if (rv != SECSuccess)
	return rv;

    /*
     * Set the error even if we will return success; someone might care.
     */
    PORT_SetError(SEC_ERROR_REVOKED_CERTIFICATE);

    if (LL_CMP(revokedTime, >, time))
	return SECSuccess;

    return SECFailure;
}

/*
 * See if the cert represented in the single response had a good status
 * at the specified time.
 */
static SECStatus
ocsp_CertHasGoodStatus(CERTOCSPSingleResponse *single, int64 time)
{
    ocspCertStatus *status;
    SECStatus rv;

    status = single->certStatus;

    switch (status->certStatusType) {
      case ocspCertStatus_good:
	rv = SECSuccess;
	break;
      case ocspCertStatus_revoked:
	rv = ocsp_CertRevokedAfter(status->certStatusInfo.revokedInfo, time);
	break;
      case ocspCertStatus_unknown:
	PORT_SetError(SEC_ERROR_OCSP_UNKNOWN_CERT);
	rv = SECFailure;
	break;
      case ocspCertStatus_other:
      default:
	PORT_Assert(0);
	PORT_SetError(SEC_ERROR_OCSP_MALFORMED_RESPONSE);
	rv = SECFailure;
	break;
    }

    return rv;
}


/*
 * FUNCTION: CERT_CheckOCSPStatus
 *   Checks the status of a certificate via OCSP.  Will only check status for
 *   a certificate that has an AIA (Authority Information Access) extension
 *   for OCSP *or* when a "default responder" is specified and enabled.
 *   (If no AIA extension for OCSP and no default responder in place, the
 *   cert is considered to have a good status and SECSuccess is returned.)
 * INPUTS:
 *   CERTCertDBHandle *handle
 *     certificate DB of the cert that is being checked
 *   CERTCertificate *cert
 *     the certificate being checked
 *   XXX in the long term also need a boolean parameter that specifies
 *	whether to check the cert chain, as well; for now we check only
 *	the leaf (the specified certificate)
 *   int64 time
 *     time for which status is to be determined
 *   void *pwArg
 *     argument for password prompting, if needed
 * RETURN:
 *   Returns SECSuccess if an approved OCSP responder "knows" the cert
 *   *and* returns a non-revoked status for it; SECFailure otherwise,
 *   with an error set describing the reason:
 *
 *	SEC_ERROR_OCSP_BAD_HTTP_RESPONSE
 *	SEC_ERROR_OCSP_FUTURE_RESPONSE
 *	SEC_ERROR_OCSP_MALFORMED_REQUEST
 *	SEC_ERROR_OCSP_MALFORMED_RESPONSE
 *	SEC_ERROR_OCSP_OLD_RESPONSE
 *	SEC_ERROR_OCSP_REQUEST_NEEDS_SIG
 *	SEC_ERROR_OCSP_SERVER_ERROR
 *	SEC_ERROR_OCSP_TRY_SERVER_LATER
 *	SEC_ERROR_OCSP_UNAUTHORIZED_REQUEST
 *	SEC_ERROR_OCSP_UNAUTHORIZED_RESPONSE
 *	SEC_ERROR_OCSP_UNKNOWN_CERT
 *	SEC_ERROR_OCSP_UNKNOWN_RESPONSE_STATUS
 *	SEC_ERROR_OCSP_UNKNOWN_RESPONSE_TYPE
 *
 *	SEC_ERROR_BAD_SIGNATURE
 *	SEC_ERROR_CERT_BAD_ACCESS_LOCATION
 *	SEC_ERROR_INVALID_TIME
 *	SEC_ERROR_REVOKED_CERTIFICATE
 *	SEC_ERROR_UNKNOWN_ISSUER
 *	SEC_ERROR_UNKNOWN_SIGNER
 *
 *   Other errors are any of the many possible failures in cert verification
 *   (e.g. SEC_ERROR_REVOKED_CERTIFICATE, SEC_ERROR_UNTRUSTED_ISSUER) when
 *   verifying the signer's cert, or low-level problems (error allocating
 *   memory, error performing ASN.1 decoding, etc.).
 */    
SECStatus 
CERT_CheckOCSPStatus(CERTCertDBHandle *handle, CERTCertificate *cert,
		     int64 time, void *pwArg)
{
    char *location = NULL;
    PRBool locationIsDefault;
    CERTCertList *certList = NULL;
    SECItem *encodedResponse = NULL;
    CERTOCSPRequest *request = NULL;
    CERTOCSPResponse *response = NULL;
    CERTCertificate *signerCert = NULL;
    CERTCertificate *issuerCert = NULL;
    CERTOCSPCertID *certID;
    SECStatus rv = SECFailure;


    /*
     * The first thing we need to do is find the location of the responder.
     * This will be the value of the default responder (if enabled), else
     * it will come out of the AIA extension in the cert (if present).
     * If we have no such location, then this cert does not "deserve" to
     * be checked -- that is, we consider it a success and just return.
     * The way we tell that is by looking at the error number to see if
     * the problem was no AIA extension was found; any other error was
     * a true failure that we unfortunately have to treat as an overall
     * failure here.
     */
    location = ocsp_GetResponderLocation(handle, cert, &locationIsDefault);
    if (location == NULL) {
	if (PORT_GetError() == SEC_ERROR_EXTENSION_NOT_FOUND)
	    return SECSuccess;
	else
	    return SECFailure;
    }

    /*
     * For now, create a cert-list of one.
     * XXX In the fullness of time, we will want/need to handle a
     * certificate chain.  This will be done either when a new parameter
     * tells us to, or some configuration variable tells us to.  In any
     * case, handling it is complicated because we may need to send as
     * many requests (and receive as many responses) as we have certs
     * in the chain.  If we are going to talk to a default responder,
     * and we only support one default responder, we can put all of the
     * certs together into one request.  Otherwise, we must break them up
     * into multiple requests.  (Even if all of the requests will go to
     * the same location, the signature on each response will be different,
     * because each issuer is different.  Carefully read the OCSP spec
     * if you do not understand this.)
     */

    certList = CERT_NewCertList();
    if (certList == NULL)
	goto loser;

    /* dup it because freeing the list will destroy the cert, too */
    cert = CERT_DupCertificate(cert);
    if (cert == NULL)
	goto loser;

    if (CERT_AddCertToListTail(certList, cert) != SECSuccess) {
	CERT_DestroyCertificate(cert);
	goto loser;
    }

    /*
     * XXX If/when signing of requests is supported, that second NULL
     * should be changed to be the signer certificate.  Not sure if that
     * should be passed into this function or retrieved via some operation
     * on the handle/context.
     */
    encodedResponse = CERT_GetEncodedOCSPResponse(NULL, certList, location,
						  time, locationIsDefault,
						  NULL, pwArg, &request);
    if (encodedResponse == NULL) {
	goto loser;
    }

    response = CERT_DecodeOCSPResponse(encodedResponse);
    if (response == NULL) {
	goto loser;
    }

    /*
     * Okay, we at least have a response that *looks* like a response!
     * Now see if the overall response status value is good or not.
     * If not, we set an error and give up.  (It means that either the
     * server had a problem, or it didn't like something about our
     * request.  Either way there is nothing to do but give up.)
     * Otherwise, we continue to find the actual per-cert status
     * in the response.
     */
    if (CERT_GetOCSPResponseStatus(response) != SECSuccess) {
	goto loser;
    }

    /*
     * If we've made it this far, we expect a response with a good signature.
     * So, check for that.
     */
    issuerCert = CERT_FindCertIssuer(cert, time, certUsageAnyCA);
    rv = CERT_VerifyOCSPResponseSignature(response, handle, pwArg, &signerCert,
			issuerCert);
    if (rv != SECSuccess)
	goto loser;

    PORT_Assert(signerCert != NULL);	/* internal consistency check */
    /* XXX probably should set error, return failure if signerCert is null */


    /*
     * Again, we are only doing one request for one cert.
     * XXX When we handle cert chains, the following code will obviously
     * have to be modified, in coordation with the code above that will
     * have to determine how to make multiple requests, etc.  It will need
     * to loop, and for each certID in the request, find the matching
     * single response and check the status specified by it.
     *
     * We are helped here in that we know that the requests are made with
     * the request list in the same order as the order of the certs we hand
     * to it.  This is why I can directly access the first member of the
     * single request array for the one cert I care about.
     */

    certID = request->tbsRequest->requestList[0]->reqCert;
    rv = CERT_GetOCSPStatusForCertID(handle, response, certID, 
                                     signerCert, time);
loser:
    if (issuerCert != NULL)
	CERT_DestroyCertificate(issuerCert);
    if (signerCert != NULL)
	CERT_DestroyCertificate(signerCert);
    if (response != NULL)
	CERT_DestroyOCSPResponse(response);
    if (request != NULL)
	CERT_DestroyOCSPRequest(request);
    if (encodedResponse != NULL)
	SECITEM_FreeItem(encodedResponse, PR_TRUE);
    if (certList != NULL)
	CERT_DestroyCertList(certList);
    if (location != NULL)
	PORT_Free(location);
    return rv;
}

SECStatus
CERT_GetOCSPStatusForCertID(CERTCertDBHandle *handle, 
                            CERTOCSPResponse *response, 
                            CERTOCSPCertID   *certID,
                            CERTCertificate  *signerCert,
                            int64             time)
{
    SECStatus rv;
    ocspResponseData *responseData;
    int64 producedAt;
    CERTOCSPSingleResponse *single;

    /*
     * The ResponseData part is the real guts of the response.
     */
    responseData = ocsp_GetResponseData(response);
    if (responseData == NULL) {
	rv = SECFailure;
	goto loser;
    }

    /*
     * There is one producedAt time for the entire response (and a separate
     * thisUpdate time for each individual single response).  We need to
     * compare them, so get the overall time to pass into the check of each
     * single response.
     */
    rv = DER_GeneralizedTimeToTime(&producedAt, &responseData->producedAt);
    if (rv != SECSuccess)
	goto loser;

    single = ocsp_GetSingleResponseForCertID(responseData->responses,
					     handle, certID);
    if (single == NULL) {
	rv = SECFailure;
	goto loser;
    }

    rv = ocsp_VerifySingleResponse(single, handle, signerCert, producedAt);
    if (rv != SECSuccess)
	goto loser;

    /*
     * Okay, the last step is to check whether the status says revoked,
     * and if so how that compares to the time value passed into this routine.
     */

    rv = ocsp_CertHasGoodStatus(single, time);
loser:
    return rv;
}


/*
 * Disable status checking and destroy related structures/data.
 */
static SECStatus
ocsp_DestroyStatusChecking(CERTStatusConfig *statusConfig)
{
    ocspCheckingContext *statusContext;

    /*
     * Disable OCSP checking
     */
    statusConfig->statusChecker = NULL;

    statusContext = statusConfig->statusContext;
    PORT_Assert(statusContext != NULL);
    if (statusContext == NULL)
	return SECFailure;

    if (statusContext->defaultResponderURI != NULL)
	PORT_Free(statusContext->defaultResponderURI);
    if (statusContext->defaultResponderNickname != NULL)
	PORT_Free(statusContext->defaultResponderNickname);

    PORT_Free(statusContext);
    statusConfig->statusContext = NULL;

    PORT_Free(statusConfig);

    return SECSuccess;
}


/*
 * FUNCTION: CERT_DisableOCSPChecking
 *   Turns off OCSP checking for the given certificate database.
 *   This routine disables OCSP checking.  Though it will return
 *   SECFailure if OCSP checking is not enabled, it is "safe" to
 *   call it that way and just ignore the return value, if it is
 *   easier to just call it than to "remember" whether it is enabled.
 * INPUTS:
 *   CERTCertDBHandle *handle
 *     Certificate database for which OCSP checking will be disabled.
 * RETURN:
 *   Returns SECFailure if an error occurred (usually means that OCSP
 *   checking was not enabled or status contexts were not initialized --
 *   error set will be SEC_ERROR_OCSP_NOT_ENABLED); SECSuccess otherwise.
 */
SECStatus
CERT_DisableOCSPChecking(CERTCertDBHandle *handle)
{
    CERTStatusConfig *statusConfig;
    ocspCheckingContext *statusContext;

    if (handle == NULL) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }

    statusConfig = CERT_GetStatusConfig(handle);
    statusContext = ocsp_GetCheckingContext(handle);
    if (statusContext == NULL)
	return SECFailure;

    if (statusConfig->statusChecker != CERT_CheckOCSPStatus) {
	/*
	 * Status configuration is present, but either not currently
	 * enabled or not for OCSP.
	 */
	PORT_SetError(SEC_ERROR_OCSP_NOT_ENABLED);
	return SECFailure;
    }

    /*
     * This is how we disable status checking.  Everything else remains
     * in place in case we are enabled again.
     */
    statusConfig->statusChecker = NULL;

    return SECSuccess;
}

/*
 * Allocate and initialize the informational structures for status checking.
 * This is done when some configuration of OCSP is being done or when OCSP
 * checking is being turned on, whichever comes first.
 */
static SECStatus
ocsp_InitStatusChecking(CERTCertDBHandle *handle)
{
    CERTStatusConfig *statusConfig = NULL;
    ocspCheckingContext *statusContext = NULL;

    PORT_Assert(CERT_GetStatusConfig(handle) == NULL);
    if (CERT_GetStatusConfig(handle) != NULL) {
	/* XXX or call statusConfig->statusDestroy and continue? */
	return SECFailure;
    }

    statusConfig = PORT_ZNew(CERTStatusConfig);
    if (statusConfig == NULL)
	goto loser;

    statusContext = PORT_ZNew(ocspCheckingContext);
    if (statusContext == NULL)
	goto loser;

    statusConfig->statusDestroy = ocsp_DestroyStatusChecking;
    statusConfig->statusContext = statusContext;

    CERT_SetStatusConfig(handle, statusConfig);

    return SECSuccess;

loser:
    if (statusContext != NULL)
	PORT_Free(statusContext);
    if (statusConfig != NULL)
	PORT_Free(statusConfig);
    return SECFailure;
}


/*
 * FUNCTION: CERT_EnableOCSPChecking
 *   Turns on OCSP checking for the given certificate database.
 * INPUTS:
 *   CERTCertDBHandle *handle
 *     Certificate database for which OCSP checking will be enabled.
 * RETURN:
 *   Returns SECFailure if an error occurred (likely only problem
 *   allocating memory); SECSuccess otherwise.
 */
SECStatus
CERT_EnableOCSPChecking(CERTCertDBHandle *handle)
{
    CERTStatusConfig *statusConfig;
    
    SECStatus rv;

    if (handle == NULL) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }

    statusConfig = CERT_GetStatusConfig(handle);
    if (statusConfig == NULL) {
	rv = ocsp_InitStatusChecking(handle);
	if (rv != SECSuccess)
	    return rv;

	/* Get newly established value */
	statusConfig = CERT_GetStatusConfig(handle);
	PORT_Assert(statusConfig != NULL);
    }

    /*
     * Setting the checker function is what really enables the checking
     * when each cert verification is done.
     */
    statusConfig->statusChecker = CERT_CheckOCSPStatus;

    return SECSuccess;
}


/*
 * FUNCTION: CERT_SetOCSPDefaultResponder
 *   Specify the location and cert of the default responder.
 *   If OCSP checking is already enabled *and* use of a default responder
 *   is also already enabled, all OCSP checking from now on will go directly
 *   to the specified responder.  If OCSP checking is not enabled, or if
 *   it is but use of a default responder is not enabled, the information
 *   will be recorded and take effect whenever both are enabled.
 * INPUTS:
 *   CERTCertDBHandle *handle
 *     Cert database on which OCSP checking should use the default responder.
 *   char *url
 *     The location of the default responder (e.g. "http://foo.com:80/ocsp")
 *     Note that the location will not be tested until the first attempt
 *     to send a request there.
 *   char *name
 *     The nickname of the cert to trust (expected) to sign the OCSP responses.
 *     If the corresponding cert cannot be found, SECFailure is returned.
 * RETURN:
 *   Returns SECFailure if an error occurred; SECSuccess otherwise.
 *   The most likely error is that the cert for "name" could not be found
 *   (probably SEC_ERROR_UNKNOWN_CERT).  Other errors are low-level (no memory,
 *   bad database, etc.).
 */
SECStatus
CERT_SetOCSPDefaultResponder(CERTCertDBHandle *handle,
			     const char *url, const char *name)
{
    CERTCertificate *cert;
    ocspCheckingContext *statusContext;
    char *url_copy = NULL;
    char *name_copy = NULL;
    SECStatus rv;

    if (handle == NULL || url == NULL || name == NULL) {
	/*
	 * XXX When interface is exported, probably want better errors;
	 * perhaps different one for each parameter.
	 */
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }

    /*
     * Find the certificate for the specified nickname.  Do this first
     * because it seems the most likely to fail.
     *
     * XXX Shouldn't need that cast if the FindCertByNickname interface
     * used const to convey that it does not modify the name.  Maybe someday.
     */
    cert = CERT_FindCertByNickname(handle, (char *) name);
    if (cert == NULL) {
      /*
       * look for the cert on an external token.
       */
      cert = PK11_FindCertFromNickname((char *)name, NULL);
    }
    if (cert == NULL)
	return SECFailure;

    /*
     * Make a copy of the url and nickname.
     */
    url_copy = PORT_Strdup(url);
    name_copy = PORT_Strdup(name);
    if (url_copy == NULL || name_copy == NULL) {
	rv = SECFailure;
	goto loser;
    }

    statusContext = ocsp_GetCheckingContext(handle);

    /*
     * Allocate and init the context if it doesn't already exist.
     */
    if (statusContext == NULL) {
	rv = ocsp_InitStatusChecking(handle);
	if (rv != SECSuccess)
	    goto loser;

	statusContext = ocsp_GetCheckingContext(handle);
	PORT_Assert(statusContext != NULL);	/* extreme paranoia */
    }

    /*
     * Note -- we do not touch the status context until after all of
     * the steps which could cause errors.  If something goes wrong,
     * we want to leave things as they were.
     */

    /*
     * Get rid of old url and name if there.
     */
    if (statusContext->defaultResponderNickname != NULL)
	PORT_Free(statusContext->defaultResponderNickname);
    if (statusContext->defaultResponderURI != NULL)
	PORT_Free(statusContext->defaultResponderURI);

    /*
     * And replace them with the new ones.
     */
    statusContext->defaultResponderURI = url_copy;
    statusContext->defaultResponderNickname = name_copy;

    /*
     * If there was already a cert in place, get rid of it and replace it.
     * Otherwise, we are not currently enabled, so we don't want to save it;
     * it will get re-found and set whenever use of a default responder is
     * enabled.
     */
    if (statusContext->defaultResponderCert != NULL) {
	CERT_DestroyCertificate(statusContext->defaultResponderCert);
	statusContext->defaultResponderCert = cert;
    } else {
	PORT_Assert(statusContext->useDefaultResponder == PR_FALSE);
	CERT_DestroyCertificate(cert);
    }

    return SECSuccess;

loser:
    CERT_DestroyCertificate(cert);
    if (url_copy != NULL)
	PORT_Free(url_copy);
    if (name_copy != NULL)
	PORT_Free(name_copy);
    return rv;
}


/*
 * FUNCTION: CERT_EnableOCSPDefaultResponder
 *   Turns on use of a default responder when OCSP checking.
 *   If OCSP checking is already enabled, this will make subsequent checks
 *   go directly to the default responder.  (The location of the responder
 *   and the nickname of the responder cert must already be specified.)
 *   If OCSP checking is not enabled, this will be recorded and take effect
 *   whenever it is enabled.
 * INPUTS:
 *   CERTCertDBHandle *handle
 *     Cert database on which OCSP checking should use the default responder.
 * RETURN:
 *   Returns SECFailure if an error occurred; SECSuccess otherwise.
 *   No errors are especially likely unless the caller did not previously
 *   perform a successful call to SetOCSPDefaultResponder (in which case
 *   the error set will be SEC_ERROR_OCSP_NO_DEFAULT_RESPONDER).
 */
SECStatus
CERT_EnableOCSPDefaultResponder(CERTCertDBHandle *handle)
{
    ocspCheckingContext *statusContext;
    CERTCertificate *cert;

    if (handle == NULL) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }

    statusContext = ocsp_GetCheckingContext(handle);

    if (statusContext == NULL) {
	/*
	 * Strictly speaking, the error already set is "correct",
	 * but cover over it with one more helpful in this context.
	 */
	PORT_SetError(SEC_ERROR_OCSP_NO_DEFAULT_RESPONDER);
	return SECFailure;
    }

    if (statusContext->defaultResponderURI == NULL) {
	PORT_SetError(SEC_ERROR_OCSP_NO_DEFAULT_RESPONDER);
	return SECFailure;
    }

    if (statusContext->defaultResponderNickname == NULL) {
	PORT_SetError(SEC_ERROR_OCSP_NO_DEFAULT_RESPONDER);
	return SECFailure;
    }

    /*
     * Find the cert for the nickname.
     */
    cert = CERT_FindCertByNickname(handle,
				   statusContext->defaultResponderNickname);
    if (cert == NULL) {
        cert = PK11_FindCertFromNickname(statusContext->defaultResponderNickname,
                                         NULL);
    }
    /*
     * We should never have trouble finding the cert, because its
     * existence should have been proven by SetOCSPDefaultResponder.
     */
    PORT_Assert(cert != NULL);
    if (cert == NULL)
	return SECFailure;

    /*
     * And hang onto it.
     */
    statusContext->defaultResponderCert = cert;

    /*
     * Finally, record the fact that we now have a default responder enabled.
     */
    statusContext->useDefaultResponder = PR_TRUE;
    return SECSuccess;
}


/*
 * FUNCTION: CERT_DisableOCSPDefaultResponder
 *   Turns off use of a default responder when OCSP checking.
 *   (Does nothing if use of a default responder is not enabled.)
 * INPUTS:
 *   CERTCertDBHandle *handle
 *     Cert database on which OCSP checking should stop using a default
 *     responder.
 * RETURN:
 *   Returns SECFailure if an error occurred; SECSuccess otherwise.
 *   Errors very unlikely (like random memory corruption...).
 */
SECStatus
CERT_DisableOCSPDefaultResponder(CERTCertDBHandle *handle)
{
    CERTStatusConfig *statusConfig;
    ocspCheckingContext *statusContext;

    if (handle == NULL) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }

    statusConfig = CERT_GetStatusConfig(handle);
    if (statusConfig == NULL)
	return SECSuccess;

    statusContext = ocsp_GetCheckingContext(handle);
    PORT_Assert(statusContext != NULL);
    if (statusContext == NULL)
	return SECFailure;

    if (statusContext->defaultResponderCert != NULL) {
	CERT_DestroyCertificate(statusContext->defaultResponderCert);
	statusContext->defaultResponderCert = NULL;
    }

    /*
     * Finally, record the fact.
     */
    statusContext->useDefaultResponder = PR_FALSE;
    return SECSuccess;
}

/*
 * Digest the cert's subject public key using the specified algorithm.
 * The necessary storage for the digest data is allocated.  If "fill" is
 * non-null, the data is put there, otherwise a SECItem is allocated.
 * Allocation from "arena" if it is non-null, heap otherwise.  Any problem
 * results in a NULL being returned (and an appropriate error set).
 */
SECItem *
CERT_SPKDigestValueForCert(PRArenaPool *arena, CERTCertificate *cert,
                           SECOidTag digestAlg, SECItem *fill)
{
    const SECHashObject *digestObject;
    void *digestContext;
    SECItem *result = NULL;
    void *mark = NULL;
    SECItem spk;

    if ( arena != NULL ) {
        mark = PORT_ArenaMark(arena);
    }

    digestObject = HASH_GetHashObjectByOidTag(digestAlg);
    if ( digestObject == NULL ) {
        goto loser;
    }

    if ((fill == NULL) || (fill->data == NULL)) {
	result = SECITEM_AllocItem(arena, fill, digestObject->length);
	if ( result == NULL ) {
	   goto loser;
	}
	fill = result;
    }

    /*
     * Copy just the length and data pointer (nothing needs to be freed)
     * of the subject public key so we can convert the length from bits
     * to bytes, which is what the digest function expects.
     */
    spk = cert->subjectPublicKeyInfo.subjectPublicKey;
    DER_ConvertBitString(&spk);

    /*
     * Now digest the value, using the specified algorithm.
     */
    digestContext = digestObject->create();
    if ( digestContext == NULL ) {
        goto loser;
    }
    digestObject->begin(digestContext);
    digestObject->update(digestContext, spk.data, spk.len);
    digestObject->end(digestContext, fill->data, &(fill->len), fill->len);
    digestObject->destroy(digestContext, PR_TRUE);

    if ( arena != NULL ) {
        PORT_ArenaUnmark(arena, mark);
    }
    return(fill);

loser:
    if ( arena != NULL ) {
        PORT_ArenaRelease(arena, mark);
    } else {
        if ( result != NULL ) {
            SECITEM_FreeItem(result, (fill == NULL) ? PR_TRUE : PR_FALSE);
        }
    }
    return(NULL);
}

SECStatus
CERT_GetOCSPResponseStatus(CERTOCSPResponse *response)
{
    PORT_Assert(response);
    if (response->statusValue == ocspResponse_successful)
	return SECSuccess;

    switch (response->statusValue) {
      case ocspResponse_malformedRequest:
	PORT_SetError(SEC_ERROR_OCSP_MALFORMED_REQUEST);
	break;
      case ocspResponse_internalError:
	PORT_SetError(SEC_ERROR_OCSP_SERVER_ERROR);
	break;
      case ocspResponse_tryLater:
	PORT_SetError(SEC_ERROR_OCSP_TRY_SERVER_LATER);
	break;
      case ocspResponse_sigRequired:
	/* XXX We *should* retry with a signature, if possible. */
	PORT_SetError(SEC_ERROR_OCSP_REQUEST_NEEDS_SIG);
	break;
      case ocspResponse_unauthorized:
	PORT_SetError(SEC_ERROR_OCSP_UNAUTHORIZED_REQUEST);
	break;
      case ocspResponse_other:
      case ocspResponse_unused:
      default:
	PORT_SetError(SEC_ERROR_OCSP_UNKNOWN_RESPONSE_STATUS);
	break;
    }
    return SECFailure;
}
