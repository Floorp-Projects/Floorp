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
 * The Original Code is the Netscape Security Services for Java.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998-2000 Netscape Communications Corporation.  All
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

#include <stdio.h>
#include "crypto.h"
#include "sechash.h"
#include "key.h"
#include "secasn1.h"
#include "secoid.h"
#include "pk11func.h"
#include "rsa.h"
#include "secerr.h"

/*
** Decrypt signature block using public key (in place)
** XXX this is assuming that the signature algorithm has WITH_RSA_ENCRYPTION
*/
static SECStatus
DecryptSigBlock(int *tagp, unsigned char *digest, SECKEYPublicKey *key,
		SECItem *sig, char *wincx)
{
    SECItem it;
    SGNDigestInfo *di = NULL;
    unsigned char *dsig;
    SECStatus rv;
    SECOidTag tag;
    unsigned char buf[MAX_RSA_MODULUS_LEN];
    
    dsig = NULL;
    it.data = buf;
    it.len = sizeof(buf);

    if (key == NULL) goto loser;

    /* Decrypt signature block */
    dsig = (unsigned char*) PORT_Alloc(sig->len);
    if (dsig == NULL) goto loser;

    /* decrypt the block */
    rv = PK11_VerifyRecover(key, sig, &it, wincx);
    if (rv != SECSuccess) goto loser;

    di = SGN_DecodeDigestInfo(&it);
    if (di == NULL) goto sigloser;

    /*
    ** Finally we have the digest info; now we can extract the algorithm
    ** ID and the signature block
    */
    tag = SECOID_GetAlgorithmTag(&di->digestAlgorithm);
    /* XXX Check that tag is an appropriate algorithm? */
    if (di->digest.len > 32) {
	PORT_SetError(SEC_ERROR_OUTPUT_LEN);
	goto loser;
    }
    PORT_Memcpy(digest, di->digest.data, di->digest.len);
    *tagp = tag;
    goto done;

  sigloser:
    PORT_SetError(SEC_ERROR_BAD_SIGNATURE);

  loser:
    rv = SECFailure;

  done:
    if (di != NULL) SGN_DestroyDigestInfo(di);
    if (dsig != NULL) PORT_Free(dsig);
    
    return rv;
}

typedef enum { VFY_RSA, VFY_DSA} VerifyType;

/************************************************************************/
/************************************************************************/
/* Above is from secvfy.c. Here's the new stuff:                        */
/* Reverse-direction verification: get the signature at the end of the  */
/* digest instead of at the beginning.                                  */
/************************************************************************/
/************************************************************************/
#include "hclhacks.h"

struct VFYContext2Str {
    SECOidTag compositeAlg;
    SECOidTag alg; /* hash alg */
    SECKEYPublicKey *key;
    VerifyType type;
    void *hashcx;
    SECHashObject *hashobj;
};

/*
 * Pulls the hash algorithm, signing algorithm, and key type out of a
 * composite algorithm.
 *
 * alg: the composite algorithm to dissect.
 * hashalg: address of a SECOidTag which will be set with the hash algorithm.
 * signalg: address of a SECOidTag which will be set with the signing alg.
 * keyType: address of a KeyType which will be set with the key type.
 * Returns: SECSuccess if the algorithm was acceptable, SECFailure if the
 *	algorithm was not found or was not a signing algorithm.
 */
static SECStatus
dissectSigAlg(SECOidTag alg, SECOidTag *hashalg, SECOidTag *signalg,
		KeyType *keyType)
{
    PR_ASSERT(hashalg!=NULL && signalg!=NULL && keyType!=NULL);

    switch (alg) {
      /* We probably shouldn't be generating MD2 signatures either */
      case SEC_OID_PKCS1_MD2_WITH_RSA_ENCRYPTION:
        *hashalg = SEC_OID_MD2;
        *signalg = SEC_OID_PKCS1_RSA_ENCRYPTION;
        *keyType = rsaKey;
	return SECSuccess;
      case SEC_OID_PKCS1_MD5_WITH_RSA_ENCRYPTION:
        *hashalg = SEC_OID_MD5;
        *signalg = SEC_OID_PKCS1_RSA_ENCRYPTION;
        *keyType = rsaKey;
	return SECSuccess;
      case SEC_OID_PKCS1_SHA1_WITH_RSA_ENCRYPTION:
      case SEC_OID_ISO_SHA_WITH_RSA_SIGNATURE:
        *hashalg = SEC_OID_SHA1;
        *signalg = SEC_OID_PKCS1_RSA_ENCRYPTION;
        *keyType = rsaKey;
	return SECSuccess;
      /* what about normal DSA? */
      case SEC_OID_ANSIX9_DSA_SIGNATURE_WITH_SHA1_DIGEST:
      case SEC_OID_BOGUS_DSA_SIGNATURE_WITH_SHA1_DIGEST:
        *hashalg = SEC_OID_SHA1;
        *signalg = SEC_OID_ANSIX9_DSA_SIGNATURE;
        *keyType = dsaKey;
	return SECSuccess;
      case SEC_OID_MISSI_DSS:
      case SEC_OID_MISSI_KEA_DSS:
      case SEC_OID_MISSI_KEA_DSS_OLD:
      case SEC_OID_MISSI_DSS_OLD:
        *hashalg = SEC_OID_SHA1;
        *signalg = SEC_OID_MISSI_DSS; /* XXX Is there a better algid? */
        *keyType = fortezzaKey;
	return SECSuccess;
      /* we don't implement MD4 hashes */
      case SEC_OID_PKCS1_MD4_WITH_RSA_ENCRYPTION:
      default:
        return SECFailure;
    }
    PR_ASSERT(PR_FALSE); /* shouldn't get here */
}

/*
 * algid: The composite signature algorithm, for example
 *	SEC_OID_PKCS1_MD5_WITH_RSA_ENCRYPTION.
 * Returns: a new VFYContext2, or NULL if an error occurred.
 */
VFYContext2 *
VFY_CreateContext2(SECKEYPublicKey *key, SECOidTag algid)
{
    VFYContext2 *cx=NULL;
    SECOidTag hashAlg;
    SECOidTag sigAlg;
    KeyType keyType;

    PR_ASSERT(key!=NULL);

    /*
     * validate key type and hash algorithm
     */
    if( dissectSigAlg(algid, &hashAlg, &sigAlg, &keyType) != SECSuccess) {
	PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
	goto loser;
    }
    if( keyType != key->keyType ) {
	/* key type of algorithm is not the type of the key that
	 * was passed in */
	PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
	goto loser;
    }
    switch(hashAlg) {
      case SEC_OID_MD2:
      case SEC_OID_MD5:
      case SEC_OID_SHA1:
	break;
      default:
	PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
	goto loser;
    }

    /*
     * Create the context
     */
    cx = (VFYContext2*) PORT_ZAlloc(sizeof(VFYContext2));
    if (cx == NULL) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	goto loser;
    }
    cx->compositeAlg = algid;
    cx->alg = hashAlg;
    switch(keyType) {
      case rsaKey:
        cx->type = VFY_RSA;
        break;
      case dsaKey:
      case fortezzaKey:
        cx->type = VFY_DSA;
        break;
      default:
        PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
        goto loser;
    }
    cx->key = SECKEY_CopyPublicKey(key);
    PR_ASSERT(cx->hashcx == NULL);
    PR_ASSERT(cx->hashobj == NULL);

    return cx;

  loser:
    if(cx) {
	VFY_DestroyContext2(cx, PR_TRUE);
    }
    return NULL;
}

void
VFY_DestroyContext2(VFYContext2 *cx, PRBool freeit)
{
    if (cx) {
	if (cx->hashcx != NULL) {
	    (*cx->hashobj->destroy)(cx->hashcx, PR_TRUE);
	    cx->hashcx = NULL;
	}
	if (cx->key) {
	    SECKEY_DestroyPublicKey(cx->key);
	}
	if (freeit) {
	    PORT_ZFree(cx, sizeof(VFYContext2));
	}
    }
}

SECStatus
VFY_Begin2(VFYContext2 *cx)
{
    if (cx->hashcx != NULL) {
	(*cx->hashobj->destroy)(cx->hashcx, PR_TRUE);
	cx->hashcx = NULL;
    }

    switch (cx->alg) {
      case SEC_OID_MD2:
	cx->hashobj = &SECHashObjects[HASH_AlgMD2];
	break;
      case SEC_OID_MD5:
	cx->hashobj = &SECHashObjects[HASH_AlgMD5];
	break;
      case SEC_OID_SHA1:
	cx->hashobj = &SECHashObjects[HASH_AlgSHA1];
	break;
      default:
	PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
	return SECFailure;
    }

    cx->hashcx = (*cx->hashobj->create)();
    if (cx->hashcx == NULL)
	return SECFailure;

    (*cx->hashobj->begin)(cx->hashcx);
    return SECSuccess;
}

SECStatus
VFY_Update2(VFYContext2 *cx, unsigned char *input, unsigned inputLen)
{
    if (cx->hashcx == NULL) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }
    (*cx->hashobj->update)(cx->hashcx, input, inputLen);
    return SECSuccess;
}


SECStatus
VFY_End2(VFYContext2 *cx, SECItem *sig, void *wincx)
{
    unsigned char final[32];
    unsigned part;
    int hashAlg;
    SECItem hash, mysig;
    SECItem *dsasig = NULL;
    unsigned char digest[DSA_SIGNATURE_LEN];
    SECStatus status = SECFailure;

    if (cx->hashcx == NULL) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	goto finish;
    }
    (*cx->hashobj->end)(cx->hashcx, final, &part, sizeof(final));
    switch (cx->type) {
      case VFY_DSA:
	/* if this is a DER encoded signature, decode it first */
	if ((cx->compositeAlg==SEC_OID_ANSIX9_DSA_SIGNATURE_WITH_SHA1_DIGEST) ||
	    (cx->compositeAlg==SEC_OID_BOGUS_DSA_SIGNATURE_WITH_SHA1_DIGEST) ||
	    (cx->compositeAlg==SEC_OID_ANSIX9_DSA_SIGNATURE)) {
	    dsasig = DSAU_DecodeDerSig(sig);
            if ((dsasig == NULL) || (dsasig->len != DSA_SIGNATURE_LEN)) {
                PORT_SetError(SEC_ERROR_BAD_SIGNATURE);
            	goto finish;
            }
            PORT_Memcpy(&digest[0], dsasig->data, dsasig->len);
	} else {
            if (sig->len != DSA_SIGNATURE_LEN) {
                PORT_SetError(SEC_ERROR_BAD_SIGNATURE);
            	goto finish;
            }
            PORT_Memcpy(&digest[0], sig->data, sig->len);
        }
	mysig.data = digest;
	mysig.len = DSA_SIGNATURE_LEN; /* magic size of dsa signature */
	hash.data = final;
	hash.len = part;
	if (PK11_Verify(cx->key,&mysig,&hash,wincx) != SECSuccess) {
		PORT_SetError(SEC_ERROR_BAD_SIGNATURE);
		goto finish;
	}
	break;
      case VFY_RSA:
	if( DecryptSigBlock(&hashAlg, &digest[0], cx->key, sig, wincx)
			!= SECSuccess)
	{
	    PORT_SetError(SEC_ERROR_BAD_SIGNATURE);
	    goto finish;
	}
	if (PORT_Memcmp(final, digest, part)) {
	    PORT_SetError(SEC_ERROR_BAD_SIGNATURE);
	    goto finish;
	}
	break;
      default:
	PR_ASSERT(PR_FALSE); /* shouldn't happen */
	PORT_SetError(SEC_ERROR_BAD_SIGNATURE);
	goto finish;
    }
    status = SECSuccess;

finish:
    if (dsasig != NULL) {
	SECITEM_FreeItem(dsasig, PR_TRUE);
    }
    return status;
}
