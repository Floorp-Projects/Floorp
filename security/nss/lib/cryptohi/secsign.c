/*
 * Signature stuff.
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 *   Dr Vipul Gupta <vipul.gupta@sun.com>, Sun Microsystems Laboratories
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
/* $Id: secsign.c,v 1.26 2011/07/24 13:48:12 wtc%google.com Exp $ */

#include <stdio.h>
#include "cryptohi.h"
#include "sechash.h"
#include "secder.h"
#include "keyhi.h"
#include "secoid.h"
#include "secdig.h"
#include "pk11func.h"
#include "secerr.h"
#include "keyi.h"

struct SGNContextStr {
    SECOidTag signalg;
    SECOidTag hashalg;
    void *hashcx;
    const SECHashObject *hashobj;
    SECKEYPrivateKey *key;
};

SGNContext *
SGN_NewContext(SECOidTag alg, SECKEYPrivateKey *key)
{
    SGNContext *cx;
    SECOidTag hashalg, signalg;
    KeyType keyType;
    SECStatus rv;

    /* OK, map a PKCS #7 hash and encrypt algorithm into
     * a standard hashing algorithm. Why did we pass in the whole
     * PKCS #7 algTag if we were just going to change here you might
     * ask. Well the answer is for some cards we may have to do the
     * hashing on card. It may not support CKM_RSA_PKCS sign algorithm,
     * it may just support CKM_RSA_PKCS_WITH_SHA1 and/or CKM_RSA_PKCS_WITH_MD5.
     */
    /* we have a private key, not a public key, so don't pass it in */
    rv =  sec_DecodeSigAlg(NULL, alg, NULL, &signalg, &hashalg);
    if (rv != SECSuccess) {
	PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
	return 0;
    }
    keyType = seckey_GetKeyType(signalg);

    /* verify our key type */
    if (key->keyType != keyType &&
	!((key->keyType == dsaKey) && (keyType == fortezzaKey)) ) {
	PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
	return 0;
    }

#ifndef NSS_ECC_MORE_THAN_SUITE_B
    if (key->keyType == ecKey) {
	PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
	return 0;
    }
#endif

    cx = (SGNContext*) PORT_ZAlloc(sizeof(SGNContext));
    if (cx) {
	cx->hashalg = hashalg;
	cx->signalg = signalg;
	cx->key = key;
    }
    return cx;
}

void
SGN_DestroyContext(SGNContext *cx, PRBool freeit)
{
    if (cx) {
	if (cx->hashcx != NULL) {
	    (*cx->hashobj->destroy)(cx->hashcx, PR_TRUE);
	    cx->hashcx = NULL;
	}
	if (freeit) {
	    PORT_ZFree(cx, sizeof(SGNContext));
	}
    }
}

SECStatus
SGN_Begin(SGNContext *cx)
{
    if (cx->hashcx != NULL) {
	(*cx->hashobj->destroy)(cx->hashcx, PR_TRUE);
	cx->hashcx = NULL;
    }

    cx->hashobj = HASH_GetHashObjectByOidTag(cx->hashalg);
    if (!cx->hashobj)
	return SECFailure;	/* error code is already set */

    cx->hashcx = (*cx->hashobj->create)();
    if (cx->hashcx == NULL)
	return SECFailure;

    (*cx->hashobj->begin)(cx->hashcx);
    return SECSuccess;
}

SECStatus
SGN_Update(SGNContext *cx, const unsigned char *input, unsigned int inputLen)
{
    if (cx->hashcx == NULL) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }
    (*cx->hashobj->update)(cx->hashcx, input, inputLen);
    return SECSuccess;
}

/* XXX Old template; want to expunge it eventually. */
static DERTemplate SECAlgorithmIDTemplate[] = {
    { DER_SEQUENCE,
	  0, NULL, sizeof(SECAlgorithmID) },
    { DER_OBJECT_ID,
	  offsetof(SECAlgorithmID,algorithm), },
    { DER_OPTIONAL | DER_ANY,
	  offsetof(SECAlgorithmID,parameters), },
    { 0, }
};

/*
 * XXX OLD Template.  Once all uses have been switched over to new one,
 * remove this.
 */
static DERTemplate SGNDigestInfoTemplate[] = {
    { DER_SEQUENCE,
	  0, NULL, sizeof(SGNDigestInfo) },
    { DER_INLINE,
	  offsetof(SGNDigestInfo,digestAlgorithm),
	  SECAlgorithmIDTemplate, },
    { DER_OCTET_STRING,
	  offsetof(SGNDigestInfo,digest), },
    { 0, }
};

SECStatus
SGN_End(SGNContext *cx, SECItem *result)
{
    unsigned char digest[HASH_LENGTH_MAX];
    unsigned part1;
    int signatureLen;
    SECStatus rv;
    SECItem digder, sigitem;
    PRArenaPool *arena = 0;
    SECKEYPrivateKey *privKey = cx->key;
    SGNDigestInfo *di = 0;

    result->data = 0;
    digder.data = 0;

    /* Finish up digest function */
    if (cx->hashcx == NULL) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }
    (*cx->hashobj->end)(cx->hashcx, digest, &part1, sizeof(digest));


    if (privKey->keyType == rsaKey) {

	arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
	if ( !arena ) {
	    rv = SECFailure;
	    goto loser;
	}
    
	/* Construct digest info */
	di = SGN_CreateDigestInfo(cx->hashalg, digest, part1);
	if (!di) {
	    rv = SECFailure;
	    goto loser;
	}

	/* Der encode the digest as a DigestInfo */
        rv = DER_Encode(arena, &digder, SGNDigestInfoTemplate,
                        di);
	if (rv != SECSuccess) {
	    goto loser;
	}
    } else {
	digder.data = digest;
	digder.len = part1;
    }

    /*
    ** Encrypt signature after constructing appropriate PKCS#1 signature
    ** block
    */
    signatureLen = PK11_SignatureLen(privKey);
    if (signatureLen <= 0) {
	PORT_SetError(SEC_ERROR_INVALID_KEY);
	rv = SECFailure;
	goto loser;
    }
    sigitem.len = signatureLen;
    sigitem.data = (unsigned char*) PORT_Alloc(signatureLen);

    if (sigitem.data == NULL) {
	rv = SECFailure;
	goto loser;
    }

    rv = PK11_Sign(privKey, &sigitem, &digder);
    if (rv != SECSuccess) {
	PORT_Free(sigitem.data);
	sigitem.data = NULL;
	goto loser;
    }

    if ((cx->signalg == SEC_OID_ANSIX9_DSA_SIGNATURE) ||
        (cx->signalg == SEC_OID_ANSIX962_EC_PUBLIC_KEY)) {
        /* DSAU_EncodeDerSigWithLen works for DSA and ECDSA */
	rv = DSAU_EncodeDerSigWithLen(result, &sigitem, sigitem.len); 
	PORT_Free(sigitem.data);
	if (rv != SECSuccess)
	    goto loser;
    } else {
	result->len = sigitem.len;
	result->data = sigitem.data;
    }

  loser:
    SGN_DestroyDigestInfo(di);
    if (arena != NULL) {
	PORT_FreeArena(arena, PR_FALSE);
    }
    return rv;
}

/************************************************************************/

/*
** Sign a block of data returning in result a bunch of bytes that are the
** signature. Returns zero on success, an error code on failure.
*/
SECStatus
SEC_SignData(SECItem *res, const unsigned char *buf, int len,
	     SECKEYPrivateKey *pk, SECOidTag algid)
{
    SECStatus rv;
    SGNContext *sgn;


    sgn = SGN_NewContext(algid, pk);

    if (sgn == NULL)
	return SECFailure;

    rv = SGN_Begin(sgn);
    if (rv != SECSuccess)
	goto loser;

    rv = SGN_Update(sgn, buf, len);
    if (rv != SECSuccess)
	goto loser;

    rv = SGN_End(sgn, res);

  loser:
    SGN_DestroyContext(sgn, PR_TRUE);
    return rv;
}

/************************************************************************/
    
DERTemplate CERTSignedDataTemplate[] =
{
    { DER_SEQUENCE,
	  0, NULL, sizeof(CERTSignedData) },
    { DER_ANY,
	  offsetof(CERTSignedData,data), },
    { DER_INLINE,
	  offsetof(CERTSignedData,signatureAlgorithm),
	  SECAlgorithmIDTemplate, },
    { DER_BIT_STRING,
	  offsetof(CERTSignedData,signature), },
    { 0, }
};

SEC_ASN1_MKSUB(SECOID_AlgorithmIDTemplate)

const SEC_ASN1Template CERT_SignedDataTemplate[] =
{
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(CERTSignedData) },
    { SEC_ASN1_ANY,
	  offsetof(CERTSignedData,data), },
    { SEC_ASN1_INLINE | SEC_ASN1_XTRN,
	  offsetof(CERTSignedData,signatureAlgorithm),
	  SEC_ASN1_SUB(SECOID_AlgorithmIDTemplate), },
    { SEC_ASN1_BIT_STRING,
	  offsetof(CERTSignedData,signature), },
    { 0, }
};

SEC_ASN1_CHOOSER_IMPLEMENT(CERT_SignedDataTemplate)


SECStatus
SEC_DerSignData(PRArenaPool *arena, SECItem *result, 
	const unsigned char *buf, int len, SECKEYPrivateKey *pk,
	SECOidTag algID)
{
    SECItem it;
    CERTSignedData sd;
    SECStatus rv;

    it.data = 0;

    /* XXX We should probably have some asserts here to make sure the key type
     * and algID match
     */

    if (algID == SEC_OID_UNKNOWN) {
	switch(pk->keyType) {
	  case rsaKey:
	    algID = SEC_OID_PKCS1_SHA1_WITH_RSA_ENCRYPTION;
	    break;
	  case dsaKey:
	    algID = SEC_OID_ANSIX9_DSA_SIGNATURE_WITH_SHA1_DIGEST;
	    break;
	  case ecKey:
	    algID = SEC_OID_ANSIX962_ECDSA_SIGNATURE_WITH_SHA1_DIGEST;
	    break;
	  default:
	    PORT_SetError(SEC_ERROR_INVALID_KEY);
	    return SECFailure;
	}
    }

    /* Sign input buffer */
    rv = SEC_SignData(&it, buf, len, pk, algID);
    if (rv) goto loser;

    /* Fill out SignedData object */
    PORT_Memset(&sd, 0, sizeof(sd));
    sd.data.data = (unsigned char*) buf;
    sd.data.len = len;
    sd.signature.data = it.data;
    sd.signature.len = it.len << 3;		/* convert to bit string */
    rv = SECOID_SetAlgorithmID(arena, &sd.signatureAlgorithm, algID, 0);
    if (rv) goto loser;

    /* DER encode the signed data object */
    rv = DER_Encode(arena, result, CERTSignedDataTemplate, &sd);
    /* FALL THROUGH */

  loser:
    PORT_Free(it.data);
    return rv;
}

SECStatus
SGN_Digest(SECKEYPrivateKey *privKey,
		SECOidTag algtag, SECItem *result, SECItem *digest)
{
    int modulusLen;
    SECStatus rv;
    SECItem digder;
    PRArenaPool *arena = 0;
    SGNDigestInfo *di = 0;


    result->data = 0;

    if (privKey->keyType == rsaKey) {

	arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
	if ( !arena ) {
	    rv = SECFailure;
	    goto loser;
	}
    
	/* Construct digest info */
	di = SGN_CreateDigestInfo(algtag, digest->data, digest->len);
	if (!di) {
	    rv = SECFailure;
	    goto loser;
	}

	/* Der encode the digest as a DigestInfo */
        rv = DER_Encode(arena, &digder, SGNDigestInfoTemplate,
                        di);
	if (rv != SECSuccess) {
	    goto loser;
	}
    } else {
	digder.data = digest->data;
	digder.len = digest->len;
    }

    /*
    ** Encrypt signature after constructing appropriate PKCS#1 signature
    ** block
    */
    modulusLen = PK11_SignatureLen(privKey);
    if (modulusLen <= 0) {
	PORT_SetError(SEC_ERROR_INVALID_KEY);
	rv = SECFailure;
	goto loser;
    }
    result->len = modulusLen;
    result->data = (unsigned char*) PORT_Alloc(modulusLen);

    if (result->data == NULL) {
	rv = SECFailure;
	goto loser;
    }

    rv = PK11_Sign(privKey, result, &digder);
    if (rv != SECSuccess) {
	PORT_Free(result->data);
	result->data = NULL;
    }

  loser:
    SGN_DestroyDigestInfo(di);
    if (arena != NULL) {
	PORT_FreeArena(arena, PR_FALSE);
    }
    return rv;
}

SECOidTag
SEC_GetSignatureAlgorithmOidTag(KeyType keyType, SECOidTag hashAlgTag)
{
    SECOidTag sigTag = SEC_OID_UNKNOWN;

    switch (keyType) {
    case rsaKey:
	switch (hashAlgTag) {
	case SEC_OID_MD2:
	    sigTag = SEC_OID_PKCS1_MD2_WITH_RSA_ENCRYPTION;	break;
	case SEC_OID_MD5:
	    sigTag = SEC_OID_PKCS1_MD5_WITH_RSA_ENCRYPTION;	break;
	case SEC_OID_UNKNOWN:	/* default for RSA if not specified */
	case SEC_OID_SHA1:
	    sigTag = SEC_OID_PKCS1_SHA1_WITH_RSA_ENCRYPTION;	break;
	case SEC_OID_SHA256:
	    sigTag = SEC_OID_PKCS1_SHA256_WITH_RSA_ENCRYPTION;	break;
	case SEC_OID_SHA384:
	    sigTag = SEC_OID_PKCS1_SHA384_WITH_RSA_ENCRYPTION;	break;
	case SEC_OID_SHA512:
	    sigTag = SEC_OID_PKCS1_SHA512_WITH_RSA_ENCRYPTION;	break;
	default:
	    break;
	}
	break;
    case dsaKey:
	switch (hashAlgTag) {
	case SEC_OID_UNKNOWN:	/* default for DSA if not specified */
	case SEC_OID_SHA1:
	    sigTag = SEC_OID_ANSIX9_DSA_SIGNATURE_WITH_SHA1_DIGEST; break;
	default:
	    break;
	}
	break;
    case ecKey:
	switch (hashAlgTag) {
	case SEC_OID_UNKNOWN:	/* default for ECDSA if not specified */
	case SEC_OID_SHA1:
            sigTag = SEC_OID_ANSIX962_ECDSA_SHA1_SIGNATURE; break;
	case SEC_OID_SHA256:
            sigTag = SEC_OID_ANSIX962_ECDSA_SHA256_SIGNATURE; break;
	case SEC_OID_SHA384:
            sigTag = SEC_OID_ANSIX962_ECDSA_SHA384_SIGNATURE; break;
	case SEC_OID_SHA512:
            sigTag = SEC_OID_ANSIX962_ECDSA_SHA512_SIGNATURE; break;
	default:
	break;
	}
    default:
    	break;
    }
    return sigTag;
}
