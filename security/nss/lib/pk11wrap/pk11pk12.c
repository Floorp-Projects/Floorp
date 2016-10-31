
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * This file PKCS #12 fuctions that should really be moved to the
 * PKCS #12 directory, however we can't do that in a point release
 * because that will break binary compatibility, so we keep them here for now.
 */

#include "seccomon.h"
#include "secmod.h"
#include "secmodi.h"
#include "pkcs11.h"
#include "pk11func.h"
#include "secitem.h"
#include "key.h"
#include "secoid.h"
#include "secasn1.h"
#include "secerr.h"
#include "prerror.h"



/* These data structures should move to a common .h file shared between the
 * wrappers and the pkcs 12 code. */

/*
** RSA Raw Private Key structures
*/

/* member names from PKCS#1, section 7.2 */
struct SECKEYRSAPrivateKeyStr {
    PLArenaPool * arena;
    SECItem version;
    SECItem modulus;
    SECItem publicExponent;
    SECItem privateExponent;
    SECItem prime1;
    SECItem prime2;
    SECItem exponent1;
    SECItem exponent2;
    SECItem coefficient;
};
typedef struct SECKEYRSAPrivateKeyStr SECKEYRSAPrivateKey;


/*
** DSA Raw Private Key structures
*/

struct SECKEYDSAPrivateKeyStr {
    SECKEYPQGParams params;
    SECItem privateValue;
};
typedef struct SECKEYDSAPrivateKeyStr SECKEYDSAPrivateKey;

/*
** Diffie-Hellman Raw Private Key structures
** Structure member names suggested by PKCS#3.
*/
struct SECKEYDHPrivateKeyStr {
    PLArenaPool * arena;
    SECItem prime;
    SECItem base;
    SECItem privateValue;
};
typedef struct SECKEYDHPrivateKeyStr SECKEYDHPrivateKey;

/*
** raw private key object
*/
struct SECKEYRawPrivateKeyStr {
    PLArenaPool *arena;
    KeyType keyType;
    union {
        SECKEYRSAPrivateKey rsa;
        SECKEYDSAPrivateKey dsa;
        SECKEYDHPrivateKey  dh;
    } u;
};
typedef struct SECKEYRawPrivateKeyStr SECKEYRawPrivateKey;

SEC_ASN1_MKSUB(SEC_AnyTemplate)
SEC_ASN1_MKSUB(SECOID_AlgorithmIDTemplate)

/* ASN1 Templates for new decoder/encoder */
/*
 * Attribute value for PKCS8 entries (static?)
 */
const SEC_ASN1Template SECKEY_AttributeTemplate[] = {
    { SEC_ASN1_SEQUENCE,
        0, NULL, sizeof(SECKEYAttribute) },
    { SEC_ASN1_OBJECT_ID, offsetof(SECKEYAttribute, attrType) },
    { SEC_ASN1_SET_OF | SEC_ASN1_XTRN, offsetof(SECKEYAttribute, attrValue),
        SEC_ASN1_SUB(SEC_AnyTemplate) },
    { 0 }
};

const SEC_ASN1Template SECKEY_SetOfAttributeTemplate[] = {
    { SEC_ASN1_SET_OF, 0, SECKEY_AttributeTemplate },
};

const SEC_ASN1Template SECKEY_PrivateKeyInfoTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(SECKEYPrivateKeyInfo) },
    { SEC_ASN1_INTEGER, offsetof(SECKEYPrivateKeyInfo,version) },
    { SEC_ASN1_INLINE | SEC_ASN1_XTRN,
        offsetof(SECKEYPrivateKeyInfo,algorithm),
        SEC_ASN1_SUB(SECOID_AlgorithmIDTemplate) },
    { SEC_ASN1_OCTET_STRING, offsetof(SECKEYPrivateKeyInfo,privateKey) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 0,
        offsetof(SECKEYPrivateKeyInfo,attributes),
        SECKEY_SetOfAttributeTemplate },
    { 0 }
};

const SEC_ASN1Template SECKEY_PointerToPrivateKeyInfoTemplate[] = {
    { SEC_ASN1_POINTER, 0, SECKEY_PrivateKeyInfoTemplate }
};

const SEC_ASN1Template SECKEY_RSAPrivateKeyExportTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(SECKEYRawPrivateKey) },
    { SEC_ASN1_INTEGER, offsetof(SECKEYRawPrivateKey,u.rsa.version) },
    { SEC_ASN1_INTEGER, offsetof(SECKEYRawPrivateKey,u.rsa.modulus) },
    { SEC_ASN1_INTEGER, offsetof(SECKEYRawPrivateKey,u.rsa.publicExponent) },
    { SEC_ASN1_INTEGER, offsetof(SECKEYRawPrivateKey,u.rsa.privateExponent) },
    { SEC_ASN1_INTEGER, offsetof(SECKEYRawPrivateKey,u.rsa.prime1) },
    { SEC_ASN1_INTEGER, offsetof(SECKEYRawPrivateKey,u.rsa.prime2) },
    { SEC_ASN1_INTEGER, offsetof(SECKEYRawPrivateKey,u.rsa.exponent1) },
    { SEC_ASN1_INTEGER, offsetof(SECKEYRawPrivateKey,u.rsa.exponent2) },
    { SEC_ASN1_INTEGER, offsetof(SECKEYRawPrivateKey,u.rsa.coefficient) },
    { 0 }
};

const SEC_ASN1Template SECKEY_DSAPrivateKeyExportTemplate[] = {
    { SEC_ASN1_INTEGER, offsetof(SECKEYRawPrivateKey,u.dsa.privateValue) },
};

const SEC_ASN1Template SECKEY_DHPrivateKeyExportTemplate[] = {
    { SEC_ASN1_INTEGER, offsetof(SECKEYRawPrivateKey,u.dh.privateValue) },
    { SEC_ASN1_INTEGER, offsetof(SECKEYRawPrivateKey,u.dh.base) },
    { SEC_ASN1_INTEGER, offsetof(SECKEYRawPrivateKey,u.dh.prime) },
};

const SEC_ASN1Template SECKEY_EncryptedPrivateKeyInfoTemplate[] = {
    { SEC_ASN1_SEQUENCE,
        0, NULL, sizeof(SECKEYEncryptedPrivateKeyInfo) },
    { SEC_ASN1_INLINE | SEC_ASN1_XTRN,
        offsetof(SECKEYEncryptedPrivateKeyInfo,algorithm),
        SEC_ASN1_SUB(SECOID_AlgorithmIDTemplate) },
    { SEC_ASN1_OCTET_STRING,
        offsetof(SECKEYEncryptedPrivateKeyInfo,encryptedData) },
    { 0 }
};

const SEC_ASN1Template SECKEY_PointerToEncryptedPrivateKeyInfoTemplate[] = {
        { SEC_ASN1_POINTER, 0, SECKEY_EncryptedPrivateKeyInfoTemplate }
};

SEC_ASN1_CHOOSER_IMPLEMENT(SECKEY_EncryptedPrivateKeyInfoTemplate)
SEC_ASN1_CHOOSER_IMPLEMENT(SECKEY_PointerToEncryptedPrivateKeyInfoTemplate)
SEC_ASN1_CHOOSER_IMPLEMENT(SECKEY_PrivateKeyInfoTemplate)
SEC_ASN1_CHOOSER_IMPLEMENT(SECKEY_PointerToPrivateKeyInfoTemplate)

/*
 * See bugzilla bug 125359
 * Since NSS (via PKCS#11) wants to handle big integers as unsigned ints,
 * all of the templates above that en/decode into integers must be converted
 * from ASN.1's signed integer type.  This is done by marking either the
 * source or destination (encoding or decoding, respectively) type as
 * siUnsignedInteger.
 */

static void
prepare_rsa_priv_key_export_for_asn1(SECKEYRawPrivateKey *key)
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

static void
prepare_dsa_priv_key_export_for_asn1(SECKEYRawPrivateKey *key)
{
    key->u.dsa.privateValue.type = siUnsignedInteger;
    key->u.dsa.params.prime.type = siUnsignedInteger;
    key->u.dsa.params.subPrime.type = siUnsignedInteger;
    key->u.dsa.params.base.type = siUnsignedInteger;
}

static void
prepare_dh_priv_key_export_for_asn1(SECKEYRawPrivateKey *key)
{
    key->u.dh.privateValue.type = siUnsignedInteger;
    key->u.dh.prime.type = siUnsignedInteger;
    key->u.dh.base.type = siUnsignedInteger;
}


SECStatus
PK11_ImportDERPrivateKeyInfo(PK11SlotInfo *slot, SECItem *derPKI, 
	SECItem *nickname, SECItem *publicValue, PRBool isPerm, 
	PRBool isPrivate, unsigned int keyUsage, void *wincx) 
{
    return PK11_ImportDERPrivateKeyInfoAndReturnKey(slot, derPKI,
	nickname, publicValue, isPerm, isPrivate, keyUsage, NULL, wincx);
}

SECStatus
PK11_ImportDERPrivateKeyInfoAndReturnKey(PK11SlotInfo *slot, SECItem *derPKI, 
	SECItem *nickname, SECItem *publicValue, PRBool isPerm, 
	PRBool isPrivate, unsigned int keyUsage, SECKEYPrivateKey** privk,
	void *wincx) 
{
    SECKEYPrivateKeyInfo *pki = NULL;
    PLArenaPool *temparena = NULL;
    SECStatus rv = SECFailure;

    temparena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (!temparena)
        return rv;
    pki = PORT_ArenaZNew(temparena, SECKEYPrivateKeyInfo);
    if (!pki) {
        PORT_FreeArena(temparena, PR_FALSE);
        return rv;
    }
    pki->arena = temparena;

    rv = SEC_ASN1DecodeItem(pki->arena, pki, SECKEY_PrivateKeyInfoTemplate,
		derPKI);
    if (rv != SECSuccess) {
        /* If SEC_ASN1DecodeItem fails, we cannot assume anything about the
         * validity of the data in pki. The best we can do is free the arena
         * and return. */
        PORT_FreeArena(temparena, PR_TRUE);
        return rv;
    }
    if (pki->privateKey.data == NULL) {
        /* If SEC_ASN1DecodeItems succeeds but SECKEYPrivateKeyInfo.privateKey
         * is a zero-length octet string, free the arena and return a failure
         * to avoid trying to zero the corresponding SECItem in
         * SECKEY_DestroyPrivateKeyInfo(). */
        PORT_FreeArena(temparena, PR_TRUE);
        PORT_SetError(SEC_ERROR_BAD_KEY);
        return SECFailure;
    }

    rv = PK11_ImportPrivateKeyInfoAndReturnKey(slot, pki, nickname,
		publicValue, isPerm, isPrivate, keyUsage, privk, wincx);

    /* this zeroes the key and frees the arena */
    SECKEY_DestroyPrivateKeyInfo(pki, PR_TRUE /*freeit*/);
    return rv;
}
        
SECStatus
PK11_ImportAndReturnPrivateKey(PK11SlotInfo *slot, SECKEYRawPrivateKey *lpk, 
	SECItem *nickname, SECItem *publicValue, PRBool isPerm, 
	PRBool isPrivate, unsigned int keyUsage, SECKEYPrivateKey **privk,
	void *wincx) 
{
    CK_BBOOL cktrue = CK_TRUE;
    CK_BBOOL ckfalse = CK_FALSE;
    CK_OBJECT_CLASS keyClass = CKO_PRIVATE_KEY;
    CK_KEY_TYPE keyType = CKK_RSA;
    CK_OBJECT_HANDLE objectID;
    CK_ATTRIBUTE theTemplate[20];
    int templateCount = 0;
    SECStatus rv = SECFailure;
    CK_ATTRIBUTE *attrs;
    CK_ATTRIBUTE *signedattr = NULL;
    int signedcount = 0;
    CK_ATTRIBUTE *ap;
    SECItem *ck_id = NULL;

    attrs = theTemplate;


    PK11_SETATTRS(attrs, CKA_CLASS, &keyClass, sizeof(keyClass) ); attrs++;
    PK11_SETATTRS(attrs, CKA_KEY_TYPE, &keyType, sizeof(keyType) ); attrs++;
    PK11_SETATTRS(attrs, CKA_TOKEN, isPerm ? &cktrue : &ckfalse, 
						sizeof(CK_BBOOL) ); attrs++;
    PK11_SETATTRS(attrs, CKA_SENSITIVE, isPrivate ? &cktrue : &ckfalse, 
						sizeof(CK_BBOOL) ); attrs++;
    PK11_SETATTRS(attrs, CKA_PRIVATE, isPrivate ? &cktrue : &ckfalse,
						 sizeof(CK_BBOOL) ); attrs++;

    switch (lpk->keyType) {
    case rsaKey:
	    keyType = CKK_RSA;
	    PK11_SETATTRS(attrs, CKA_UNWRAP, (keyUsage & KU_KEY_ENCIPHERMENT) ?
				&cktrue : &ckfalse, sizeof(CK_BBOOL) ); attrs++;
	    PK11_SETATTRS(attrs, CKA_DECRYPT, (keyUsage & KU_DATA_ENCIPHERMENT) ?
				&cktrue : &ckfalse, sizeof(CK_BBOOL) ); attrs++;
	    PK11_SETATTRS(attrs, CKA_SIGN, (keyUsage & KU_DIGITAL_SIGNATURE) ? 
				&cktrue : &ckfalse, sizeof(CK_BBOOL) ); attrs++;
	    PK11_SETATTRS(attrs, CKA_SIGN_RECOVER, 
				(keyUsage & KU_DIGITAL_SIGNATURE) ? 
				&cktrue : &ckfalse, sizeof(CK_BBOOL) ); attrs++;
	    ck_id = PK11_MakeIDFromPubKey(&lpk->u.rsa.modulus);
	    if (ck_id == NULL) {
		goto loser;
	    }
	    PK11_SETATTRS(attrs, CKA_ID, ck_id->data,ck_id->len); attrs++;
	    if (nickname) {
		PK11_SETATTRS(attrs, CKA_LABEL, nickname->data, nickname->len); attrs++; 
	    } 
	    signedattr = attrs;
	    PK11_SETATTRS(attrs, CKA_MODULUS, lpk->u.rsa.modulus.data,
						lpk->u.rsa.modulus.len); attrs++;
	    PK11_SETATTRS(attrs, CKA_PUBLIC_EXPONENT, 
	     			lpk->u.rsa.publicExponent.data,
				lpk->u.rsa.publicExponent.len); attrs++;
	    PK11_SETATTRS(attrs, CKA_PRIVATE_EXPONENT, 
	     			lpk->u.rsa.privateExponent.data,
				lpk->u.rsa.privateExponent.len); attrs++;
	    PK11_SETATTRS(attrs, CKA_PRIME_1, 
	     			lpk->u.rsa.prime1.data,
				lpk->u.rsa.prime1.len); attrs++;
	    PK11_SETATTRS(attrs, CKA_PRIME_2, 
	     			lpk->u.rsa.prime2.data,
				lpk->u.rsa.prime2.len); attrs++;
	    PK11_SETATTRS(attrs, CKA_EXPONENT_1, 
	     			lpk->u.rsa.exponent1.data,
				lpk->u.rsa.exponent1.len); attrs++;
	    PK11_SETATTRS(attrs, CKA_EXPONENT_2, 
	     			lpk->u.rsa.exponent2.data,
				lpk->u.rsa.exponent2.len); attrs++;
	    PK11_SETATTRS(attrs, CKA_COEFFICIENT, 
	     			lpk->u.rsa.coefficient.data,
				lpk->u.rsa.coefficient.len); attrs++;
	    break;
    case dsaKey:
	    keyType = CKK_DSA;
	    /* To make our intenal PKCS #11 module work correctly with 
	     * our database, we need to pass in the public key value for 
	     * this dsa key. We have a netscape only CKA_ value to do this.
	     * Only send it to internal slots */
	    if( publicValue == NULL ) {
		goto loser;
	    }
	    if (PK11_IsInternal(slot)) {
	        PK11_SETATTRS(attrs, CKA_NETSCAPE_DB,
				publicValue->data, publicValue->len); attrs++;
	    }
	    PK11_SETATTRS(attrs, CKA_SIGN, &cktrue, sizeof(CK_BBOOL)); attrs++;
	    PK11_SETATTRS(attrs, CKA_SIGN_RECOVER, &cktrue, sizeof(CK_BBOOL)); attrs++;
	    if(nickname) {
		PK11_SETATTRS(attrs, CKA_LABEL, nickname->data, nickname->len);
		attrs++; 
	    } 
	    ck_id = PK11_MakeIDFromPubKey(publicValue);
	    if (ck_id == NULL) {
		goto loser;
	    }
	    PK11_SETATTRS(attrs, CKA_ID, ck_id->data,ck_id->len); attrs++;
	    signedattr = attrs;
	    PK11_SETATTRS(attrs, CKA_PRIME,    lpk->u.dsa.params.prime.data,
				lpk->u.dsa.params.prime.len); attrs++;
	    PK11_SETATTRS(attrs,CKA_SUBPRIME,lpk->u.dsa.params.subPrime.data,
				lpk->u.dsa.params.subPrime.len); attrs++;
	    PK11_SETATTRS(attrs, CKA_BASE,  lpk->u.dsa.params.base.data,
					lpk->u.dsa.params.base.len); attrs++;
	    PK11_SETATTRS(attrs, CKA_VALUE,    lpk->u.dsa.privateValue.data, 
					lpk->u.dsa.privateValue.len); attrs++;
	    break;
     case dhKey:
	    keyType = CKK_DH;
	    /* To make our intenal PKCS #11 module work correctly with 
	     * our database, we need to pass in the public key value for 
	     * this dh key. We have a netscape only CKA_ value to do this.
	     * Only send it to internal slots */
	    if (PK11_IsInternal(slot)) {
	        PK11_SETATTRS(attrs, CKA_NETSCAPE_DB,
				publicValue->data, publicValue->len); attrs++;
	    }
	    PK11_SETATTRS(attrs, CKA_DERIVE, &cktrue, sizeof(CK_BBOOL)); attrs++;
	    if(nickname) {
		PK11_SETATTRS(attrs, CKA_LABEL, nickname->data, nickname->len);
		attrs++; 
	    } 
	    ck_id = PK11_MakeIDFromPubKey(publicValue);
	    if (ck_id == NULL) {
		goto loser;
	    }
	    PK11_SETATTRS(attrs, CKA_ID, ck_id->data,ck_id->len); attrs++;
	    signedattr = attrs;
	    PK11_SETATTRS(attrs, CKA_PRIME,    lpk->u.dh.prime.data,
				lpk->u.dh.prime.len); attrs++;
	    PK11_SETATTRS(attrs, CKA_BASE,  lpk->u.dh.base.data,
					lpk->u.dh.base.len); attrs++;
	    PK11_SETATTRS(attrs, CKA_VALUE,    lpk->u.dh.privateValue.data, 
					lpk->u.dh.privateValue.len); attrs++;
	    break;
	/* what about fortezza??? */
    default:
	    PORT_SetError(SEC_ERROR_BAD_KEY);
	    goto loser;
    }
    templateCount = attrs - theTemplate;
    PORT_Assert(templateCount <= sizeof(theTemplate)/sizeof(CK_ATTRIBUTE));
    PORT_Assert(signedattr != NULL);
    signedcount = attrs - signedattr;

    for (ap=signedattr; signedcount; ap++, signedcount--) {
	pk11_SignedToUnsigned(ap);
    }

    rv = PK11_CreateNewObject(slot, CK_INVALID_SESSION,
			theTemplate, templateCount, isPerm, &objectID);

    /* create and return a SECKEYPrivateKey */
    if( rv == SECSuccess && privk != NULL) {
	*privk = PK11_MakePrivKey(slot, lpk->keyType, !isPerm, objectID, wincx);
	if( *privk == NULL ) {
	    rv = SECFailure;
	}
    }
loser:
    if (ck_id) {
	SECITEM_ZfreeItem(ck_id, PR_TRUE);
    }
    return rv;
}

SECStatus
PK11_ImportPrivateKeyInfoAndReturnKey(PK11SlotInfo *slot,
	SECKEYPrivateKeyInfo *pki, SECItem *nickname, SECItem *publicValue,
	PRBool isPerm, PRBool isPrivate, unsigned int keyUsage,
	SECKEYPrivateKey **privk, void *wincx) 
{
    SECStatus rv = SECFailure;
    SECKEYRawPrivateKey *lpk = NULL;
    const SEC_ASN1Template *keyTemplate, *paramTemplate;
    void *paramDest = NULL;
    PLArenaPool *arena = NULL;

    arena = PORT_NewArena(2048);
    if(!arena) {
	return SECFailure;
    }

    /* need to change this to use RSA/DSA keys */
    lpk = (SECKEYRawPrivateKey *)PORT_ArenaZAlloc(arena,
						  sizeof(SECKEYRawPrivateKey));
    if(lpk == NULL) {
	goto loser;
    }
    lpk->arena = arena;

    switch(SECOID_GetAlgorithmTag(&pki->algorithm)) {
	case SEC_OID_PKCS1_RSA_ENCRYPTION:
	    prepare_rsa_priv_key_export_for_asn1(lpk);
	    keyTemplate = SECKEY_RSAPrivateKeyExportTemplate;
	    paramTemplate = NULL;
	    paramDest = NULL;
	    lpk->keyType = rsaKey;
	    break;
	case SEC_OID_ANSIX9_DSA_SIGNATURE:
	    prepare_dsa_priv_key_export_for_asn1(lpk);
	    keyTemplate = SECKEY_DSAPrivateKeyExportTemplate;
	    paramTemplate = SECKEY_PQGParamsTemplate;
	    paramDest = &(lpk->u.dsa.params);
	    lpk->keyType = dsaKey;
	    break;
	case SEC_OID_X942_DIFFIE_HELMAN_KEY:
	    if(!publicValue) {
		goto loser;
	    }
	    prepare_dh_priv_key_export_for_asn1(lpk);
	    keyTemplate = SECKEY_DHPrivateKeyExportTemplate;
	    paramTemplate = NULL;
	    paramDest = NULL;
	    lpk->keyType = dhKey;
	    break;

	default:
	    keyTemplate   = NULL;
	    paramTemplate = NULL;
	    paramDest     = NULL;
	    break;
    }

    if(!keyTemplate) {
	goto loser;
    }

    /* decode the private key and any algorithm parameters */
    rv = SEC_ASN1DecodeItem(arena, lpk, keyTemplate, &pki->privateKey);
    if(rv != SECSuccess) {
	goto loser;
    }
    if(paramDest && paramTemplate) {
	rv = SEC_ASN1DecodeItem(arena, paramDest, paramTemplate, 
				 &(pki->algorithm.parameters));
	if(rv != SECSuccess) {
	    goto loser;
	}
    }

    rv = PK11_ImportAndReturnPrivateKey(slot,lpk,nickname,publicValue, isPerm, 
	isPrivate,  keyUsage, privk, wincx);


loser:
    if (arena != NULL) {
	PORT_FreeArena(arena, PR_TRUE);
    }

    return rv;
}

SECStatus
PK11_ImportPrivateKeyInfo(PK11SlotInfo *slot, SECKEYPrivateKeyInfo *pki, 
	SECItem *nickname, SECItem *publicValue, PRBool isPerm, 
	PRBool isPrivate, unsigned int keyUsage, void *wincx) 
{
    return PK11_ImportPrivateKeyInfoAndReturnKey(slot, pki, nickname,
	publicValue, isPerm, isPrivate, keyUsage, NULL, wincx);

}

SECItem *
PK11_ExportDERPrivateKeyInfo(SECKEYPrivateKey *pk, void *wincx)
{
    SECKEYPrivateKeyInfo *pki = PK11_ExportPrivKeyInfo(pk, wincx);
    SECItem *derPKI;

    if (!pki) {
        return NULL;
    }
    derPKI = SEC_ASN1EncodeItem(NULL, NULL, pki,
                                SECKEY_PrivateKeyInfoTemplate);
    SECKEY_DestroyPrivateKeyInfo(pki, PR_TRUE);
    return derPKI;
}

static PRBool
ReadAttribute(SECKEYPrivateKey *key, CK_ATTRIBUTE_TYPE type,
              PLArenaPool *arena, SECItem *output)
{
    SECStatus rv = PK11_ReadAttribute(key->pkcs11Slot, key->pkcs11ID, type,
                                      arena, output);
    return rv == SECSuccess;
}

/*
 * The caller is responsible for freeing the return value by passing it to
 * SECKEY_DestroyPrivateKeyInfo(..., PR_TRUE).
 */
SECKEYPrivateKeyInfo *
PK11_ExportPrivKeyInfo(SECKEYPrivateKey *pk, void *wincx)
{
    /* PrivateKeyInfo version (always zero) */
    const unsigned char pkiVersion = 0;
    /* RSAPrivateKey version (always zero) */
    const unsigned char rsaVersion = 0;
    PLArenaPool *arena = NULL;
    SECKEYRawPrivateKey rawKey;
    SECKEYPrivateKeyInfo *pki;
    SECItem *encoded;
    SECStatus rv;

    if (pk->keyType != rsaKey) {
        PORT_SetError(PR_NOT_IMPLEMENTED_ERROR);
        goto loser;
    }

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (!arena) {
        goto loser;
    }
    memset(&rawKey, 0, sizeof(rawKey));
    rawKey.keyType = pk->keyType;
    rawKey.u.rsa.version.type = siUnsignedInteger;
    rawKey.u.rsa.version.data = (unsigned char *)PORT_ArenaAlloc(arena, 1);
    if (!rawKey.u.rsa.version.data) {
        goto loser;
    }
    rawKey.u.rsa.version.data[0] = rsaVersion;
    rawKey.u.rsa.version.len = 1;

    /* Read the component attributes of the private key */
    prepare_rsa_priv_key_export_for_asn1(&rawKey);
    if (!ReadAttribute(pk, CKA_MODULUS, arena, &rawKey.u.rsa.modulus) ||
        !ReadAttribute(pk, CKA_PUBLIC_EXPONENT, arena,
                       &rawKey.u.rsa.publicExponent) ||
        !ReadAttribute(pk, CKA_PRIVATE_EXPONENT, arena,
                       &rawKey.u.rsa.privateExponent) ||
        !ReadAttribute(pk, CKA_PRIME_1, arena, &rawKey.u.rsa.prime1) ||
        !ReadAttribute(pk, CKA_PRIME_2, arena, &rawKey.u.rsa.prime2) ||
        !ReadAttribute(pk, CKA_EXPONENT_1, arena,
                       &rawKey.u.rsa.exponent1) ||
        !ReadAttribute(pk, CKA_EXPONENT_2, arena,
                       &rawKey.u.rsa.exponent2) ||
        !ReadAttribute(pk, CKA_COEFFICIENT, arena,
                       &rawKey.u.rsa.coefficient)) {
        goto loser;
    }

    pki = PORT_ArenaZNew(arena, SECKEYPrivateKeyInfo);
    if (!pki) {
        goto loser;
    }
    encoded = SEC_ASN1EncodeItem(arena, &pki->privateKey, &rawKey,
                                 SECKEY_RSAPrivateKeyExportTemplate);
    if (!encoded) {
        goto loser;
    }
    rv = SECOID_SetAlgorithmID(arena, &pki->algorithm,
                               SEC_OID_PKCS1_RSA_ENCRYPTION, NULL);
    if (rv != SECSuccess) {
        goto loser;
    }
    pki->version.type = siUnsignedInteger;
    pki->version.data = (unsigned char *)PORT_ArenaAlloc(arena, 1);
    if (!pki->version.data) {
        goto loser;
    }
    pki->version.data[0] = pkiVersion;
    pki->version.len = 1;
    pki->arena = arena;

    return pki;

loser:
    if (arena) {
        PORT_FreeArena(arena, PR_TRUE);
    }
    return NULL;
}
