
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
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
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



/* These data structures should move to a common .h file shared between the
 * wrappers and the pkcs 12 code. */

/*
** RSA Raw Private Key structures
*/

/* member names from PKCS#1, section 7.2 */
struct SECKEYRSAPrivateKeyStr {
    PRArenaPool * arena;
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
    PRArenaPool * arena;
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


/* ASN1 Templates for new decoder/encoder */
/*
 * Attribute value for PKCS8 entries (static?)
 */
const SEC_ASN1Template SECKEY_AttributeTemplate[] = {
    { SEC_ASN1_SEQUENCE,
        0, NULL, sizeof(SECKEYAttribute) },
    { SEC_ASN1_OBJECT_ID, offsetof(SECKEYAttribute, attrType) },
    { SEC_ASN1_SET_OF, offsetof(SECKEYAttribute, attrValue),
        SEC_AnyTemplate },
    { 0 }
};

const SEC_ASN1Template SECKEY_SetOfAttributeTemplate[] = {
    { SEC_ASN1_SET_OF, 0, SECKEY_AttributeTemplate },
};

const SEC_ASN1Template SECKEY_PrivateKeyInfoTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(SECKEYPrivateKeyInfo) },
    { SEC_ASN1_INTEGER, offsetof(SECKEYPrivateKeyInfo,version) },
    { SEC_ASN1_INLINE, offsetof(SECKEYPrivateKeyInfo,algorithm),
        SECOID_AlgorithmIDTemplate },
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
    { SEC_ASN1_INLINE,
        offsetof(SECKEYEncryptedPrivateKeyInfo,algorithm),
        SECOID_AlgorithmIDTemplate },
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


SECStatus
PK11_ImportDERPrivateKeyInfo(PK11SlotInfo *slot, SECItem *derPKI, 
	SECItem *nickname, SECItem *publicValue, PRBool isPerm, 
	PRBool isPrivate, unsigned int keyUsage, void *wincx) 
{
    SECKEYPrivateKeyInfo *pki = NULL;
    PRArenaPool *temparena = NULL;
    SECStatus rv = SECFailure;

    temparena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    pki = PORT_ZNew(SECKEYPrivateKeyInfo);

    rv = SEC_ASN1DecodeItem(temparena, pki, SECKEY_PrivateKeyInfoTemplate,
		derPKI);
    if( rv != SECSuccess ) {
	goto finish;
    }

    rv = PK11_ImportPrivateKeyInfo(slot, pki, nickname, publicValue,
			isPerm, isPrivate, keyUsage, wincx);

finish:
    if( pki != NULL ) {
	SECKEY_DestroyPrivateKeyInfo(pki, PR_TRUE /*freeit*/);
    }
    if( temparena != NULL ) {
	PORT_FreeArena(temparena, PR_TRUE);
    }
    return rv;
}
        
/*
 * import a private key info into the desired slot
 */
SECStatus
PK11_ImportPrivateKey(PK11SlotInfo *slot, SECKEYRawPrivateKey *lpk, 
	SECItem *nickname, SECItem *publicValue, PRBool isPerm, 
	PRBool isPrivate, unsigned int keyUsage, void *wincx) 
{
    CK_BBOOL cktrue = CK_TRUE;
    CK_BBOOL ckfalse = CK_FALSE;
    CK_OBJECT_CLASS keyClass = CKO_PRIVATE_KEY;
    CK_KEY_TYPE keyType = CKK_RSA;
    CK_OBJECT_HANDLE objectID;
    CK_ATTRIBUTE theTemplate[20];
    int templateCount = 0;
    SECStatus rv = SECFailure;
    PRArenaPool *arena;
    CK_ATTRIBUTE *attrs;
    CK_ATTRIBUTE *signedattr = NULL;
    int signedcount = 0;
    CK_ATTRIBUTE *ap;
    SECItem *ck_id = NULL;

    arena = PORT_NewArena(2048);
    if(!arena) {
	return SECFailure;
    }

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
loser:
    if (ck_id) {
	SECITEM_ZfreeItem(ck_id, PR_TRUE);
    }
    return rv;
}

/*
 * import a private key info into the desired slot
 */
SECStatus
PK11_ImportPrivateKeyInfo(PK11SlotInfo *slot, SECKEYPrivateKeyInfo *pki, 
	SECItem *nickname, SECItem *publicValue, PRBool isPerm, 
	PRBool isPrivate, unsigned int keyUsage, void *wincx) 
{
    CK_KEY_TYPE keyType = CKK_RSA;
    SECStatus rv = SECFailure;
    SECKEYRawPrivateKey *lpk = NULL;
    const SEC_ASN1Template *keyTemplate, *paramTemplate;
    void *paramDest = NULL;
    PRArenaPool *arena;

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
	    keyTemplate = SECKEY_RSAPrivateKeyExportTemplate;
	    paramTemplate = NULL;
	    paramDest = NULL;
	    lpk->keyType = rsaKey;
	    keyType = CKK_RSA;
	    break;
	case SEC_OID_ANSIX9_DSA_SIGNATURE:
	    keyTemplate = SECKEY_DSAPrivateKeyExportTemplate;
	    paramTemplate = SECKEY_PQGParamsTemplate;
	    paramDest = &(lpk->u.dsa.params);
	    lpk->keyType = dsaKey;
	    keyType = CKK_DSA;
	    break;
	case SEC_OID_X942_DIFFIE_HELMAN_KEY:
	    if(!publicValue) {
		goto loser;
	    }
	    keyTemplate = SECKEY_DHPrivateKeyExportTemplate;
	    paramTemplate = NULL;
	    paramDest = NULL;
	    lpk->keyType = dhKey;
	    keyType = CKK_DH;
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

    rv = PK11_ImportPrivateKey(slot,lpk,nickname,publicValue, isPerm, 
	isPrivate,  keyUsage, wincx);


loser:
    if (lpk!= NULL) {
	PORT_FreeArena(arena, PR_TRUE);
    }

    return rv;
}

