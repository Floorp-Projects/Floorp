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
#include "lowkeyi.h" 
#include "secoid.h" 
#include "secitem.h"
#include "secder.h" 
#include "secasn1.h"
#include "secerr.h" 

SEC_ASN1_MKSUB(SEC_AnyTemplate)
SEC_ASN1_MKSUB(SEC_BitStringTemplate)
SEC_ASN1_MKSUB(SEC_ObjectIDTemplate)
SEC_ASN1_MKSUB(SECOID_AlgorithmIDTemplate)

static const SEC_ASN1Template nsslowkey_AttributeTemplate[] = {
    { SEC_ASN1_SEQUENCE, 
	0, NULL, sizeof(NSSLOWKEYAttribute) },
    { SEC_ASN1_OBJECT_ID, offsetof(NSSLOWKEYAttribute, attrType) },
    { SEC_ASN1_SET_OF | SEC_ASN1_XTRN, offsetof(NSSLOWKEYAttribute, attrValue), 
	SEC_ASN1_SUB(SEC_AnyTemplate) },
    { 0 }
};

static const SEC_ASN1Template nsslowkey_SetOfAttributeTemplate[] = {
    { SEC_ASN1_SET_OF, 0, nsslowkey_AttributeTemplate },
};
/* ASN1 Templates for new decoder/encoder */
const SEC_ASN1Template lg_nsslowkey_PrivateKeyInfoTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	0, NULL, sizeof(NSSLOWKEYPrivateKeyInfo) },
    { SEC_ASN1_INTEGER,
	offsetof(NSSLOWKEYPrivateKeyInfo,version) },
    { SEC_ASN1_INLINE | SEC_ASN1_XTRN,
	offsetof(NSSLOWKEYPrivateKeyInfo,algorithm),
	SEC_ASN1_SUB(SECOID_AlgorithmIDTemplate) },
    { SEC_ASN1_OCTET_STRING,
	offsetof(NSSLOWKEYPrivateKeyInfo,privateKey) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 0,
	offsetof(NSSLOWKEYPrivateKeyInfo, attributes),
	nsslowkey_SetOfAttributeTemplate },
    { 0 }
};

const SEC_ASN1Template lg_nsslowkey_PQGParamsTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(PQGParams) },
    { SEC_ASN1_INTEGER, offsetof(PQGParams,prime) },
    { SEC_ASN1_INTEGER, offsetof(PQGParams,subPrime) },
    { SEC_ASN1_INTEGER, offsetof(PQGParams,base) },
    { 0, }
};

const SEC_ASN1Template lg_nsslowkey_RSAPrivateKeyTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSSLOWKEYPrivateKey) },
    { SEC_ASN1_INTEGER, offsetof(NSSLOWKEYPrivateKey,u.rsa.version) },
    { SEC_ASN1_INTEGER, offsetof(NSSLOWKEYPrivateKey,u.rsa.modulus) },
    { SEC_ASN1_INTEGER, offsetof(NSSLOWKEYPrivateKey,u.rsa.publicExponent) },
    { SEC_ASN1_INTEGER, offsetof(NSSLOWKEYPrivateKey,u.rsa.privateExponent) },
    { SEC_ASN1_INTEGER, offsetof(NSSLOWKEYPrivateKey,u.rsa.prime1) },
    { SEC_ASN1_INTEGER, offsetof(NSSLOWKEYPrivateKey,u.rsa.prime2) },
    { SEC_ASN1_INTEGER, offsetof(NSSLOWKEYPrivateKey,u.rsa.exponent1) },
    { SEC_ASN1_INTEGER, offsetof(NSSLOWKEYPrivateKey,u.rsa.exponent2) },
    { SEC_ASN1_INTEGER, offsetof(NSSLOWKEYPrivateKey,u.rsa.coefficient) },
    { 0 }                                                                     
};                                                                            

/*
 * Allows u.rsa.modulus to be zero length for secret keys with an empty
 * CKA_ID incorrectly generated in NSS 3.13.3 or earlier.  Only used for
 * decoding.  See bug 715073.
 */
const SEC_ASN1Template lg_nsslowkey_RSAPrivateKeyTemplate2[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSSLOWKEYPrivateKey) },
    { SEC_ASN1_INTEGER, offsetof(NSSLOWKEYPrivateKey,u.rsa.version) },
    { SEC_ASN1_ANY, offsetof(NSSLOWKEYPrivateKey,u.rsa.modulus) },
    { SEC_ASN1_INTEGER, offsetof(NSSLOWKEYPrivateKey,u.rsa.publicExponent) },
    { SEC_ASN1_INTEGER, offsetof(NSSLOWKEYPrivateKey,u.rsa.privateExponent) },
    { SEC_ASN1_INTEGER, offsetof(NSSLOWKEYPrivateKey,u.rsa.prime1) },
    { SEC_ASN1_INTEGER, offsetof(NSSLOWKEYPrivateKey,u.rsa.prime2) },
    { SEC_ASN1_INTEGER, offsetof(NSSLOWKEYPrivateKey,u.rsa.exponent1) },
    { SEC_ASN1_INTEGER, offsetof(NSSLOWKEYPrivateKey,u.rsa.exponent2) },
    { SEC_ASN1_INTEGER, offsetof(NSSLOWKEYPrivateKey,u.rsa.coefficient) },
    { 0 }
};

const SEC_ASN1Template lg_nsslowkey_DSAPrivateKeyTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSSLOWKEYPrivateKey) },
    { SEC_ASN1_INTEGER, offsetof(NSSLOWKEYPrivateKey,u.dsa.publicValue) },
    { SEC_ASN1_INTEGER, offsetof(NSSLOWKEYPrivateKey,u.dsa.privateValue) },
    { 0, }
};

const SEC_ASN1Template lg_nsslowkey_DHPrivateKeyTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSSLOWKEYPrivateKey) },
    { SEC_ASN1_INTEGER, offsetof(NSSLOWKEYPrivateKey,u.dh.publicValue) },
    { SEC_ASN1_INTEGER, offsetof(NSSLOWKEYPrivateKey,u.dh.privateValue) },
    { SEC_ASN1_INTEGER, offsetof(NSSLOWKEYPrivateKey,u.dh.base) },
    { SEC_ASN1_INTEGER, offsetof(NSSLOWKEYPrivateKey,u.dh.prime) },
    { 0, }
};

#ifdef NSS_ENABLE_ECC

/* XXX This is just a placeholder for later when we support
 * generic curves and need full-blown support for parsing EC
 * parameters. For now, we only support named curves in which
 * EC params are simply encoded as an object ID and we don't
 * use lg_nsslowkey_ECParamsTemplate.
 */
const SEC_ASN1Template lg_nsslowkey_ECParamsTemplate[] = {
    { SEC_ASN1_CHOICE, offsetof(ECParams,type), NULL, sizeof(ECParams) },
    { SEC_ASN1_OBJECT_ID, offsetof(ECParams,curveOID), NULL, ec_params_named },
    { 0, }
};


/* NOTE: The SECG specification allows the private key structure
 * to contain curve parameters but recommends that they be stored
 * in the PrivateKeyAlgorithmIdentifier field of the PrivateKeyInfo
 * instead.
 */
const SEC_ASN1Template lg_nsslowkey_ECPrivateKeyTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSSLOWKEYPrivateKey) },
    { SEC_ASN1_INTEGER, offsetof(NSSLOWKEYPrivateKey,u.ec.version) },
    { SEC_ASN1_OCTET_STRING, 
      offsetof(NSSLOWKEYPrivateKey,u.ec.privateValue) },
    /* XXX The following template works for now since we only
     * support named curves for which the parameters are
     * encoded as an object ID. When we support generic curves,
     * we'll need to define lg_nsslowkey_ECParamsTemplate
     */
#if 1
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED |
      SEC_ASN1_EXPLICIT | SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_XTRN | 0, 
      offsetof(NSSLOWKEYPrivateKey,u.ec.ecParams.curveOID), 
      SEC_ASN1_SUB(SEC_ObjectIDTemplate) }, 
#else
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED |
      SEC_ASN1_EXPLICIT | SEC_ASN1_CONTEXT_SPECIFIC | 0, 
      offsetof(NSSLOWKEYPrivateKey,u.ec.ecParams), 
      lg_nsslowkey_ECParamsTemplate }, 
#endif
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED |
      SEC_ASN1_EXPLICIT | SEC_ASN1_CONTEXT_SPECIFIC |
      SEC_ASN1_XTRN | 1, 
      offsetof(NSSLOWKEYPrivateKey,u.ec.publicValue),
      SEC_ASN1_SUB(SEC_BitStringTemplate) }, 
    { 0, }
};


/*
 * smaller version of EC_FillParams. In this code, we only need
 * oid and DER data.
 */
SECStatus
LGEC_FillParams(PRArenaPool *arena, const SECItem *encodedParams, 
    ECParams *params)
{
    SECOidTag tag;
    SECItem oid = { siBuffer, NULL, 0};

#if EC_DEBUG
    int i;

    printf("Encoded params in EC_DecodeParams: ");
    for (i = 0; i < encodedParams->len; i++) {
	    printf("%02x:", encodedParams->data[i]);
    }
    printf("\n");
#endif

    oid.len = encodedParams->len - 2;
    oid.data = encodedParams->data + 2;
    if ((encodedParams->data[0] != SEC_ASN1_OBJECT_ID) ||
	((tag = SECOID_FindOIDTag(&oid)) == SEC_OID_UNKNOWN)) { 
	    PORT_SetError(SEC_ERROR_UNSUPPORTED_ELLIPTIC_CURVE);
	    return SECFailure;
    }

    params->arena = arena;

    /* For named curves, fill out curveOID */
    params->curveOID.len = oid.len;
    params->curveOID.data = (unsigned char *) PORT_ArenaAlloc(arena, oid.len);
    if (params->curveOID.data == NULL)  {
	return SECFailure;
    }
    memcpy(params->curveOID.data, oid.data, oid.len);

    return SECSuccess;
}

/* Copy all of the fields from srcParams into dstParams
 */
SECStatus
LGEC_CopyParams(PRArenaPool *arena, ECParams *dstParams,
	      const ECParams *srcParams)
{
    SECStatus rv = SECFailure;

    dstParams->arena = arena;
    rv = SECITEM_CopyItem(arena, &dstParams->DEREncoding,
				 &srcParams->DEREncoding);
    if (rv != SECSuccess) {
	goto loser;
    }
    rv =SECITEM_CopyItem(arena, &dstParams->curveOID,
				&srcParams->curveOID);
    if (rv != SECSuccess) {
	goto loser;
    }

    return SECSuccess;

loser:
    return SECFailure;
}
#endif /* NSS_ENABLE_ECC */
/*
 * See bugzilla bug 125359
 * Since NSS (via PKCS#11) wants to handle big integers as unsigned ints,
 * all of the templates above that en/decode into integers must be converted
 * from ASN.1's signed integer type.  This is done by marking either the
 * source or destination (encoding or decoding, respectively) type as
 * siUnsignedInteger.
 */

void
lg_prepare_low_rsa_priv_key_for_asn1(NSSLOWKEYPrivateKey *key)
{
    key->u.rsa.modulus.type = siUnsignedInteger;
    key->u.rsa.publicExponent.type = siUnsignedInteger;
    key->u.rsa.privateExponent.type = siUnsignedInteger;
    key->u.rsa.prime1.type = siUnsignedInteger;
    key->u.rsa.prime2.type = siUnsignedInteger;
    key->u.rsa.exponent1.type = siUnsignedInteger;
    key->u.rsa.exponent2.type = siUnsignedInteger;
    key->u.rsa.coefficient.type = siUnsignedInteger;
}

void
lg_prepare_low_pqg_params_for_asn1(PQGParams *params)
{
    params->prime.type = siUnsignedInteger;
    params->subPrime.type = siUnsignedInteger;
    params->base.type = siUnsignedInteger;
}

void
lg_prepare_low_dsa_priv_key_for_asn1(NSSLOWKEYPrivateKey *key)
{
    key->u.dsa.publicValue.type = siUnsignedInteger;
    key->u.dsa.privateValue.type = siUnsignedInteger;
    key->u.dsa.params.prime.type = siUnsignedInteger;
    key->u.dsa.params.subPrime.type = siUnsignedInteger;
    key->u.dsa.params.base.type = siUnsignedInteger;
}

void
lg_prepare_low_dh_priv_key_for_asn1(NSSLOWKEYPrivateKey *key)
{
    key->u.dh.prime.type = siUnsignedInteger;
    key->u.dh.base.type = siUnsignedInteger;
    key->u.dh.publicValue.type = siUnsignedInteger;
    key->u.dh.privateValue.type = siUnsignedInteger;
}

#ifdef NSS_ENABLE_ECC
void
lg_prepare_low_ecparams_for_asn1(ECParams *params)
{
    params->DEREncoding.type = siUnsignedInteger;
    params->curveOID.type = siUnsignedInteger;
}

void
lg_prepare_low_ec_priv_key_for_asn1(NSSLOWKEYPrivateKey *key)
{
    key->u.ec.version.type = siUnsignedInteger;
    key->u.ec.ecParams.DEREncoding.type = siUnsignedInteger;
    key->u.ec.ecParams.curveOID.type = siUnsignedInteger;
    key->u.ec.privateValue.type = siUnsignedInteger;
    key->u.ec.publicValue.type = siUnsignedInteger;
}
#endif /* NSS_ENABLE_ECC */

void
lg_nsslowkey_DestroyPrivateKey(NSSLOWKEYPrivateKey *privk)
{
    if (privk && privk->arena) {
	PORT_FreeArena(privk->arena, PR_TRUE);
    }
}

void
lg_nsslowkey_DestroyPublicKey(NSSLOWKEYPublicKey *pubk)
{
    if (pubk && pubk->arena) {
	PORT_FreeArena(pubk->arena, PR_FALSE);
    }
}

NSSLOWKEYPublicKey *
lg_nsslowkey_ConvertToPublicKey(NSSLOWKEYPrivateKey *privk)
{
    NSSLOWKEYPublicKey *pubk;
    PLArenaPool *arena;


    arena = PORT_NewArena (DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL) {
        PORT_SetError (SEC_ERROR_NO_MEMORY);
        return NULL;
    }

    switch(privk->keyType) {
      case NSSLOWKEYRSAKey:
      case NSSLOWKEYNullKey:
	pubk = (NSSLOWKEYPublicKey *)PORT_ArenaZAlloc(arena,
						sizeof (NSSLOWKEYPublicKey));
	if (pubk != NULL) {
	    SECStatus rv;

	    pubk->arena = arena;
	    pubk->keyType = privk->keyType;
	    if (privk->keyType == NSSLOWKEYNullKey) return pubk;
	    rv = SECITEM_CopyItem(arena, &pubk->u.rsa.modulus,
				  &privk->u.rsa.modulus);
	    if (rv == SECSuccess) {
		rv = SECITEM_CopyItem (arena, &pubk->u.rsa.publicExponent,
				       &privk->u.rsa.publicExponent);
		if (rv == SECSuccess)
		    return pubk;
	    }
	} else {
	    PORT_SetError (SEC_ERROR_NO_MEMORY);
	}
	break;
      case NSSLOWKEYDSAKey:
	pubk = (NSSLOWKEYPublicKey *)PORT_ArenaZAlloc(arena,
						    sizeof(NSSLOWKEYPublicKey));
	if (pubk != NULL) {
	    SECStatus rv;

	    pubk->arena = arena;
	    pubk->keyType = privk->keyType;
	    rv = SECITEM_CopyItem(arena, &pubk->u.dsa.publicValue,
				  &privk->u.dsa.publicValue);
	    if (rv != SECSuccess) break;
	    rv = SECITEM_CopyItem(arena, &pubk->u.dsa.params.prime,
				  &privk->u.dsa.params.prime);
	    if (rv != SECSuccess) break;
	    rv = SECITEM_CopyItem(arena, &pubk->u.dsa.params.subPrime,
				  &privk->u.dsa.params.subPrime);
	    if (rv != SECSuccess) break;
	    rv = SECITEM_CopyItem(arena, &pubk->u.dsa.params.base,
				  &privk->u.dsa.params.base);
	    if (rv == SECSuccess) return pubk;
	}
	break;
      case NSSLOWKEYDHKey:
	pubk = (NSSLOWKEYPublicKey *)PORT_ArenaZAlloc(arena,
						    sizeof(NSSLOWKEYPublicKey));
	if (pubk != NULL) {
	    SECStatus rv;

	    pubk->arena = arena;
	    pubk->keyType = privk->keyType;
	    rv = SECITEM_CopyItem(arena, &pubk->u.dh.publicValue,
				  &privk->u.dh.publicValue);
	    if (rv != SECSuccess) break;
	    rv = SECITEM_CopyItem(arena, &pubk->u.dh.prime,
				  &privk->u.dh.prime);
	    if (rv != SECSuccess) break;
	    rv = SECITEM_CopyItem(arena, &pubk->u.dh.base,
				  &privk->u.dh.base);
	    if (rv == SECSuccess) return pubk;
	}
	break;
#ifdef NSS_ENABLE_ECC
      case NSSLOWKEYECKey:
	pubk = (NSSLOWKEYPublicKey *)PORT_ArenaZAlloc(arena,
						    sizeof(NSSLOWKEYPublicKey));
	if (pubk != NULL) {
	    SECStatus rv;

	    pubk->arena = arena;
	    pubk->keyType = privk->keyType;
	    rv = SECITEM_CopyItem(arena, &pubk->u.ec.publicValue,
				  &privk->u.ec.publicValue);
	    if (rv != SECSuccess) break;
	    pubk->u.ec.ecParams.arena = arena;
	    /* Copy the rest of the params */
	    rv = LGEC_CopyParams(arena, &(pubk->u.ec.ecParams),
			       &(privk->u.ec.ecParams));
	    if (rv == SECSuccess) return pubk;
	}
	break;
#endif /* NSS_ENABLE_ECC */
	/* No Fortezza in Low Key implementations (Fortezza keys aren't
	 * stored in our data base */
    default:
	break;
    }

    PORT_FreeArena (arena, PR_FALSE);
    return NULL;
}

