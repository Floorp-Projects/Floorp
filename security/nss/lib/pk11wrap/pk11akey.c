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
 *   Dr Stephen Henson <stephen.henson@gemplus.com>
 *   Dr Vipul Gupta <vipul.gupta@sun.com>, and
 *   Douglas Stebila <douglas@stebila.ca>, Sun Microsystems Laboratories
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
 * This file contains functions to manage asymetric keys, (public and
 * private keys).
 */
#include "seccomon.h"
#include "secmod.h"
#include "secmodi.h"
#include "secmodti.h"
#include "pkcs11.h"
#include "pkcs11t.h"
#include "pk11func.h"
#include "cert.h"
#include "key.h"
#include "secitem.h"
#include "secasn1.h" 
#include "secoid.h" 
#include "secerr.h"
#include "sslerr.h"
#include "sechash.h"

#include "secpkcs5.h"  
#include "ec.h"

#define PAIRWISE_SECITEM_TYPE			siBuffer
#define PAIRWISE_DIGEST_LENGTH			SHA1_LENGTH /* 160-bits */
#define PAIRWISE_MESSAGE_LENGTH			20          /* 160-bits */

/*
 * import a public key into the desired slot
 */
CK_OBJECT_HANDLE
PK11_ImportPublicKey(PK11SlotInfo *slot, SECKEYPublicKey *pubKey, 
								PRBool isToken)
{
    CK_BBOOL cktrue = CK_TRUE;
    CK_BBOOL ckfalse = CK_FALSE;
    CK_OBJECT_CLASS keyClass = CKO_PUBLIC_KEY;
    CK_KEY_TYPE keyType = CKK_GENERIC_SECRET;
    CK_OBJECT_HANDLE objectID;
    CK_ATTRIBUTE theTemplate[10];
    CK_ATTRIBUTE *signedattr = NULL;
    CK_ATTRIBUTE *attrs = theTemplate;
    int signedcount = 0;
    int templateCount = 0;
    SECStatus rv;

    /* if we already have an object in the desired slot, use it */
    if (!isToken && pubKey->pkcs11Slot == slot) {
	return pubKey->pkcs11ID;
    }

    /* free the existing key */
    if (pubKey->pkcs11Slot != NULL) {
	PK11SlotInfo *oSlot = pubKey->pkcs11Slot;
	PK11_EnterSlotMonitor(oSlot);
	(void) PK11_GETTAB(oSlot)->C_DestroyObject(oSlot->session,
							pubKey->pkcs11ID);
	PK11_ExitSlotMonitor(oSlot);
	PK11_FreeSlot(oSlot);
	pubKey->pkcs11Slot = NULL;
    }
    PK11_SETATTRS(attrs, CKA_CLASS, &keyClass, sizeof(keyClass) ); attrs++;
    PK11_SETATTRS(attrs, CKA_KEY_TYPE, &keyType, sizeof(keyType) ); attrs++;
    PK11_SETATTRS(attrs, CKA_TOKEN, isToken ? &cktrue : &ckfalse,
						 sizeof(CK_BBOOL) ); attrs++;

    /* now import the key */
    {
        switch (pubKey->keyType) {
        case rsaKey:
	    keyType = CKK_RSA;
	    PK11_SETATTRS(attrs, CKA_WRAP, &cktrue, sizeof(CK_BBOOL) ); attrs++;
	    PK11_SETATTRS(attrs, CKA_ENCRYPT, &cktrue, 
						sizeof(CK_BBOOL) ); attrs++;
	    PK11_SETATTRS(attrs, CKA_VERIFY, &cktrue, sizeof(CK_BBOOL)); attrs++;
 	    signedattr = attrs;
	    PK11_SETATTRS(attrs, CKA_MODULUS, pubKey->u.rsa.modulus.data,
					 pubKey->u.rsa.modulus.len); attrs++;
	    PK11_SETATTRS(attrs, CKA_PUBLIC_EXPONENT, 
	     	pubKey->u.rsa.publicExponent.data,
				 pubKey->u.rsa.publicExponent.len); attrs++;
	    break;
        case dsaKey:
	    keyType = CKK_DSA;
	    PK11_SETATTRS(attrs, CKA_VERIFY, &cktrue, sizeof(CK_BBOOL));attrs++;
 	    signedattr = attrs;
	    PK11_SETATTRS(attrs, CKA_PRIME,    pubKey->u.dsa.params.prime.data,
				pubKey->u.dsa.params.prime.len); attrs++;
	    PK11_SETATTRS(attrs,CKA_SUBPRIME,pubKey->u.dsa.params.subPrime.data,
				pubKey->u.dsa.params.subPrime.len); attrs++;
	    PK11_SETATTRS(attrs, CKA_BASE,  pubKey->u.dsa.params.base.data,
					pubKey->u.dsa.params.base.len); attrs++;
	    PK11_SETATTRS(attrs, CKA_VALUE,    pubKey->u.dsa.publicValue.data, 
					pubKey->u.dsa.publicValue.len); attrs++;
	    break;
	case fortezzaKey:
	    keyType = CKK_DSA;
	    PK11_SETATTRS(attrs, CKA_VERIFY, &cktrue, sizeof(CK_BBOOL));attrs++;
 	    signedattr = attrs;
	    PK11_SETATTRS(attrs, CKA_PRIME,pubKey->u.fortezza.params.prime.data,
				pubKey->u.fortezza.params.prime.len); attrs++;
	    PK11_SETATTRS(attrs,CKA_SUBPRIME,
				pubKey->u.fortezza.params.subPrime.data,
				pubKey->u.fortezza.params.subPrime.len);attrs++;
	    PK11_SETATTRS(attrs, CKA_BASE,  pubKey->u.fortezza.params.base.data,
				pubKey->u.fortezza.params.base.len); attrs++;
	    PK11_SETATTRS(attrs, CKA_VALUE, pubKey->u.fortezza.DSSKey.data, 
				pubKey->u.fortezza.DSSKey.len); attrs++;
            break;
        case dhKey:
	    keyType = CKK_DH;
	    PK11_SETATTRS(attrs, CKA_DERIVE, &cktrue, sizeof(CK_BBOOL));attrs++;
 	    signedattr = attrs;
	    PK11_SETATTRS(attrs, CKA_PRIME,    pubKey->u.dh.prime.data,
				pubKey->u.dh.prime.len); attrs++;
	    PK11_SETATTRS(attrs, CKA_BASE,  pubKey->u.dh.base.data,
					pubKey->u.dh.base.len); attrs++;
	    PK11_SETATTRS(attrs, CKA_VALUE,    pubKey->u.dh.publicValue.data, 
					pubKey->u.dh.publicValue.len); attrs++;
	    break;
#ifdef NSS_ENABLE_ECC
        case ecKey:
	    keyType = CKK_EC;
	    PK11_SETATTRS(attrs, CKA_VERIFY, &cktrue, sizeof(CK_BBOOL));attrs++;
	    PK11_SETATTRS(attrs, CKA_DERIVE, &cktrue, sizeof(CK_BBOOL));attrs++;
 	    signedattr = attrs;
	    PK11_SETATTRS(attrs, CKA_EC_PARAMS, 
		          pubKey->u.ec.DEREncodedParams.data,
		          pubKey->u.ec.DEREncodedParams.len); attrs++;
	    PK11_SETATTRS(attrs, CKA_EC_POINT, pubKey->u.ec.publicValue.data,
			  pubKey->u.ec.publicValue.len); attrs++;
	    break;
#endif /* NSS_ENABLE_ECC */
	default:
	    PORT_SetError( SEC_ERROR_BAD_KEY );
	    return CK_INVALID_HANDLE;
	}

	templateCount = attrs - theTemplate;
	signedcount = attrs - signedattr;
	PORT_Assert(templateCount <= (sizeof(theTemplate)/sizeof(CK_ATTRIBUTE)));
	for (attrs=signedattr; signedcount; attrs++, signedcount--) {
		pk11_SignedToUnsigned(attrs);
	} 
        rv = PK11_CreateNewObject(slot, CK_INVALID_SESSION, theTemplate,
				 	templateCount, isToken, &objectID);
	if ( rv != SECSuccess) {
	    return CK_INVALID_HANDLE;
	}
    }

    pubKey->pkcs11ID = objectID;
    pubKey->pkcs11Slot = PK11_ReferenceSlot(slot);

    return objectID;
}

/*
 * take an attribute and copy it into a secitem
 */
static CK_RV
pk11_Attr2SecItem(PRArenaPool *arena, CK_ATTRIBUTE *attr, SECItem *item) 
{
    item->data = NULL;

    (void)SECITEM_AllocItem(arena, item, attr->ulValueLen);
    if (item->data == NULL) {
	return CKR_HOST_MEMORY;
    } 
    PORT_Memcpy(item->data, attr->pValue, item->len);
    return CKR_OK;
}

/*
 * extract a public key from a slot and id
 */
SECKEYPublicKey *
PK11_ExtractPublicKey(PK11SlotInfo *slot,KeyType keyType,CK_OBJECT_HANDLE id)
{
    CK_OBJECT_CLASS keyClass = CKO_PUBLIC_KEY;
    PRArenaPool *arena;
    PRArenaPool *tmp_arena;
    SECKEYPublicKey *pubKey;
    int templateCount = 0;
    CK_KEY_TYPE pk11KeyType;
    CK_RV crv;
    CK_ATTRIBUTE template[8];
    CK_ATTRIBUTE *attrs= template;
    CK_ATTRIBUTE *modulus,*exponent,*base,*prime,*subprime,*value;
#ifdef NSS_ENABLE_ECC
    CK_ATTRIBUTE *ecparams;
#endif /* NSS_ENABLE_ECC */

    /* if we didn't know the key type, get it */
    if (keyType== nullKey) {

        pk11KeyType = PK11_ReadULongAttribute(slot,id,CKA_KEY_TYPE);
	if (pk11KeyType ==  CK_UNAVAILABLE_INFORMATION) {
	    return NULL;
	}
	switch (pk11KeyType) {
	case CKK_RSA:
	    keyType = rsaKey;
	    break;
	case CKK_DSA:
	    keyType = dsaKey;
	    break;
	case CKK_DH:
	    keyType = dhKey;
	    break;
#ifdef NSS_ENABLE_ECC
	case CKK_EC:
	    keyType = ecKey;
	    break;
#endif /* NSS_ENABLE_ECC */
	default:
	    PORT_SetError( SEC_ERROR_BAD_KEY );
	    return NULL;
	}
    }


    /* now we need to create space for the public key */
    arena = PORT_NewArena( DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL) return NULL;
    tmp_arena = PORT_NewArena( DER_DEFAULT_CHUNKSIZE);
    if (tmp_arena == NULL) {
	PORT_FreeArena (arena, PR_FALSE);
	return NULL;
    }


    pubKey = (SECKEYPublicKey *) 
			PORT_ArenaZAlloc(arena, sizeof(SECKEYPublicKey));
    if (pubKey == NULL) {
	PORT_FreeArena (arena, PR_FALSE);
	PORT_FreeArena (tmp_arena, PR_FALSE);
	return NULL;
    }

    pubKey->arena = arena;
    pubKey->keyType = keyType;
    pubKey->pkcs11Slot = PK11_ReferenceSlot(slot);
    pubKey->pkcs11ID = id;
    PK11_SETATTRS(attrs, CKA_CLASS, &keyClass, 
						sizeof(keyClass)); attrs++;
    PK11_SETATTRS(attrs, CKA_KEY_TYPE, &pk11KeyType, 
						sizeof(pk11KeyType) ); attrs++;
    switch (pubKey->keyType) {
    case rsaKey:
	modulus = attrs;
	PK11_SETATTRS(attrs, CKA_MODULUS, NULL, 0); attrs++; 
	exponent = attrs;
	PK11_SETATTRS(attrs, CKA_PUBLIC_EXPONENT, NULL, 0); attrs++; 

	templateCount = attrs - template;
	PR_ASSERT(templateCount <= sizeof(template)/sizeof(CK_ATTRIBUTE));
	crv = PK11_GetAttributes(tmp_arena,slot,id,template,templateCount);
	if (crv != CKR_OK) break;

	if ((keyClass != CKO_PUBLIC_KEY) || (pk11KeyType != CKK_RSA)) {
	    crv = CKR_OBJECT_HANDLE_INVALID;
	    break;
	} 
	crv = pk11_Attr2SecItem(arena,modulus,&pubKey->u.rsa.modulus);
	if (crv != CKR_OK) break;
	crv = pk11_Attr2SecItem(arena,exponent,&pubKey->u.rsa.publicExponent);
	if (crv != CKR_OK) break;
	break;
    case dsaKey:
	prime = attrs;
	PK11_SETATTRS(attrs, CKA_PRIME, NULL, 0); attrs++; 
	subprime = attrs;
	PK11_SETATTRS(attrs, CKA_SUBPRIME, NULL, 0); attrs++; 
	base = attrs;
	PK11_SETATTRS(attrs, CKA_BASE, NULL, 0); attrs++; 
	value = attrs;
	PK11_SETATTRS(attrs, CKA_VALUE, NULL, 0); attrs++; 
	templateCount = attrs - template;
	PR_ASSERT(templateCount <= sizeof(template)/sizeof(CK_ATTRIBUTE));
	crv = PK11_GetAttributes(tmp_arena,slot,id,template,templateCount);
	if (crv != CKR_OK) break;

	if ((keyClass != CKO_PUBLIC_KEY) || (pk11KeyType != CKK_DSA)) {
	    crv = CKR_OBJECT_HANDLE_INVALID;
	    break;
	} 
	crv = pk11_Attr2SecItem(arena,prime,&pubKey->u.dsa.params.prime);
	if (crv != CKR_OK) break;
	crv = pk11_Attr2SecItem(arena,subprime,&pubKey->u.dsa.params.subPrime);
	if (crv != CKR_OK) break;
	crv = pk11_Attr2SecItem(arena,base,&pubKey->u.dsa.params.base);
	if (crv != CKR_OK) break;
	crv = pk11_Attr2SecItem(arena,value,&pubKey->u.dsa.publicValue);
	if (crv != CKR_OK) break;
	break;
    case dhKey:
	prime = attrs;
	PK11_SETATTRS(attrs, CKA_PRIME, NULL, 0); attrs++; 
	base = attrs;
	PK11_SETATTRS(attrs, CKA_BASE, NULL, 0); attrs++; 
	value =attrs;
	PK11_SETATTRS(attrs, CKA_VALUE, NULL, 0); attrs++; 
	templateCount = attrs - template;
	PR_ASSERT(templateCount <= sizeof(template)/sizeof(CK_ATTRIBUTE));
	crv = PK11_GetAttributes(tmp_arena,slot,id,template,templateCount);
	if (crv != CKR_OK) break;

	if ((keyClass != CKO_PUBLIC_KEY) || (pk11KeyType != CKK_DH)) {
	    crv = CKR_OBJECT_HANDLE_INVALID;
	    break;
	} 
	crv = pk11_Attr2SecItem(arena,prime,&pubKey->u.dh.prime);
	if (crv != CKR_OK) break;
	crv = pk11_Attr2SecItem(arena,base,&pubKey->u.dh.base);
	if (crv != CKR_OK) break;
	crv = pk11_Attr2SecItem(arena,value,&pubKey->u.dh.publicValue);
	if (crv != CKR_OK) break;
	break;
#ifdef NSS_ENABLE_ECC
    case ecKey:
	pubKey->u.ec.size = 0;
	ecparams = attrs;
	PK11_SETATTRS(attrs, CKA_EC_PARAMS, NULL, 0); attrs++; 
	value =attrs;
	PK11_SETATTRS(attrs, CKA_EC_POINT, NULL, 0); attrs++; 
	templateCount = attrs - template;
	PR_ASSERT(templateCount <= sizeof(template)/sizeof(CK_ATTRIBUTE));
	crv = PK11_GetAttributes(tmp_arena,slot,id,template,templateCount);
	if (crv != CKR_OK) break;

	if ((keyClass != CKO_PUBLIC_KEY) || (pk11KeyType != CKK_EC)) {
	    crv = CKR_OBJECT_HANDLE_INVALID;
	    break;
	} 

	crv = pk11_Attr2SecItem(arena,ecparams,
	                        &pubKey->u.ec.DEREncodedParams);
	if (crv != CKR_OK) break;
	crv = pk11_Attr2SecItem(arena,value,&pubKey->u.ec.publicValue);
	if (crv != CKR_OK) break;
	break;
#endif /* NSS_ENABLE_ECC */
    case fortezzaKey:
    case nullKey:
    default:
	crv = CKR_OBJECT_HANDLE_INVALID;
	break;
    }

    PORT_FreeArena(tmp_arena,PR_FALSE);

    if (crv != CKR_OK) {
	PORT_FreeArena(arena,PR_FALSE);
	PK11_FreeSlot(slot);
	PORT_SetError( PK11_MapError(crv) );
	return NULL;
    }

    return pubKey;
}

/*
 * Build a Private Key structure from raw PKCS #11 information.
 */
SECKEYPrivateKey *
PK11_MakePrivKey(PK11SlotInfo *slot, KeyType keyType, 
			PRBool isTemp, CK_OBJECT_HANDLE privID, void *wincx)
{
    PRArenaPool *arena;
    SECKEYPrivateKey *privKey;
    PRBool isPrivate;
    SECStatus rv;

    /* don't know? look it up */
    if (keyType == nullKey) {
	CK_KEY_TYPE pk11Type = CKK_RSA;

	pk11Type = PK11_ReadULongAttribute(slot,privID,CKA_KEY_TYPE);
	isTemp = (PRBool)!PK11_HasAttributeSet(slot,privID,CKA_TOKEN);
	switch (pk11Type) {
	case CKK_RSA: keyType = rsaKey; break;
	case CKK_DSA: keyType = dsaKey; break;
	case CKK_DH: keyType = dhKey; break;
	case CKK_KEA: keyType = fortezzaKey; break;
#ifdef NSS_ENABLE_ECC
	case CKK_EC: keyType = ecKey; break;
#endif /* NSS_ENABLE_ECC */
	default:
		break;
	}
    }

    /* if the key is private, make sure we are authenticated to the
     * token before we try to use it */
    isPrivate = (PRBool)PK11_HasAttributeSet(slot,privID,CKA_PRIVATE);
    if (isPrivate) {
	rv = PK11_Authenticate(slot, PR_TRUE, wincx);
 	if (rv != SECSuccess) {
 	    return NULL;
 	}
    }

    /* now we need to create space for the private key */
    arena = PORT_NewArena( DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL) return NULL;

    privKey = (SECKEYPrivateKey *) 
			PORT_ArenaZAlloc(arena, sizeof(SECKEYPrivateKey));
    if (privKey == NULL) {
	PORT_FreeArena(arena, PR_FALSE);
	return NULL;
    }

    privKey->arena = arena;
    privKey->keyType = keyType;
    privKey->pkcs11Slot = PK11_ReferenceSlot(slot);
    privKey->pkcs11ID = privID;
    privKey->pkcs11IsTemp = isTemp;
    privKey->wincx = wincx;

    return privKey;
}


PK11SlotInfo *
PK11_GetSlotFromPrivateKey(SECKEYPrivateKey *key)
{
    PK11SlotInfo *slot = key->pkcs11Slot;
    slot = PK11_ReferenceSlot(slot);
    return slot;
}

/*
 * Get the modulus length for raw parsing
 */
int
PK11_GetPrivateModulusLen(SECKEYPrivateKey *key)
{
    CK_ATTRIBUTE theTemplate = { CKA_MODULUS, NULL, 0 };
    PK11SlotInfo *slot = key->pkcs11Slot;
    CK_RV crv;
    int length;

    switch (key->keyType) {
    case rsaKey:
	crv = PK11_GetAttributes(NULL, slot, key->pkcs11ID, &theTemplate, 1);
	if (crv != CKR_OK) {
	    PORT_SetError( PK11_MapError(crv) );
	    return -1;
	}
	length = theTemplate.ulValueLen;
	if ( *(unsigned char *)theTemplate.pValue == 0) {
	    length--;
	}
	if (theTemplate.pValue != NULL)
	    PORT_Free(theTemplate.pValue);
	return (int) length;
	
    case fortezzaKey:
    case dsaKey:
    case dhKey:
    default:
	break;
    }
    if (theTemplate.pValue != NULL)
	PORT_Free(theTemplate.pValue);
    PORT_SetError( SEC_ERROR_INVALID_KEY );
    return -1;
}

/*
 * PKCS #11 pairwise consistency check utilized to validate key pair.
 */
static SECStatus
pk11_PairwiseConsistencyCheck(SECKEYPublicKey *pubKey, 
	SECKEYPrivateKey *privKey, CK_MECHANISM *mech, void* wincx )
{
    /* Variables used for Encrypt/Decrypt functions. */
    unsigned char *known_message = (unsigned char *)"Known Crypto Message";
    CK_BBOOL isEncryptable = CK_FALSE;
    CK_BBOOL canSignVerify = CK_FALSE;
    CK_BBOOL isDerivable = CK_FALSE;
    unsigned char plaintext[PAIRWISE_MESSAGE_LENGTH];
    CK_ULONG bytes_decrypted;
    PK11SlotInfo *slot;
    CK_OBJECT_HANDLE id;
    unsigned char *ciphertext;
    unsigned char *text_compared;
    CK_ULONG max_bytes_encrypted;
    CK_ULONG bytes_encrypted;
    CK_ULONG bytes_compared;
    CK_RV crv;

    /* Variables used for Signature/Verification functions. */
    unsigned char *known_digest = (unsigned char *)"Mozilla Rules World!";
    SECItem  signature;
    SECItem  digest;    /* always uses SHA-1 digest */
    int signature_length;
    SECStatus rv;

    /**************************************************/
    /* Pairwise Consistency Check of Encrypt/Decrypt. */
    /**************************************************/

    isEncryptable = PK11_HasAttributeSet( privKey->pkcs11Slot, 
					privKey->pkcs11ID, CKA_DECRYPT );

    /* If the encryption attribute is set; attempt to encrypt */
    /* with the public key and decrypt with the private key.  */
    if( isEncryptable ) {
	/* Find a module to encrypt against */
	slot = PK11_GetBestSlot(pk11_mapWrapKeyType(privKey->keyType),wincx);
	if (slot == NULL) {
	    PORT_SetError( SEC_ERROR_NO_MODULE );
	    return SECFailure;
	}

	id = PK11_ImportPublicKey(slot,pubKey,PR_FALSE);
	if (id == CK_INVALID_HANDLE) {
	    PK11_FreeSlot(slot);
	    return SECFailure;
	}

        /* Compute max bytes encrypted from modulus length of private key. */
	max_bytes_encrypted = PK11_GetPrivateModulusLen( privKey );


	/* Prepare for encryption using the public key. */
        PK11_EnterSlotMonitor(slot);
	crv = PK11_GETTAB( slot )->C_EncryptInit( slot->session,
						  mech, id );
        if( crv != CKR_OK ) {
	    PK11_ExitSlotMonitor(slot);
	    PORT_SetError( PK11_MapError( crv ) );
	    PK11_FreeSlot(slot);
	    return SECFailure;
	}

	/* Allocate space for ciphertext. */
	ciphertext = (unsigned char *) PORT_Alloc( max_bytes_encrypted );
	if( ciphertext == NULL ) {
	    PK11_ExitSlotMonitor(slot);
	    PORT_SetError( SEC_ERROR_NO_MEMORY );
	    PK11_FreeSlot(slot);
	    return SECFailure;
	}

	/* Initialize bytes encrypted to max bytes encrypted. */
	bytes_encrypted = max_bytes_encrypted;

	/* Encrypt using the public key. */
	crv = PK11_GETTAB( slot )->C_Encrypt( slot->session,
					      known_message,
					      PAIRWISE_MESSAGE_LENGTH,
					      ciphertext,
					      &bytes_encrypted );
	PK11_ExitSlotMonitor(slot);
	PK11_FreeSlot(slot);
	if( crv != CKR_OK ) {
	    PORT_SetError( PK11_MapError( crv ) );
	    PORT_Free( ciphertext );
	    return SECFailure;
	}

	/* Always use the smaller of these two values . . . */
	bytes_compared = ( bytes_encrypted > PAIRWISE_MESSAGE_LENGTH )
			 ? PAIRWISE_MESSAGE_LENGTH
			 : bytes_encrypted;

	/* If there was a failure, the plaintext */
	/* goes at the end, therefore . . .      */
	text_compared = ( bytes_encrypted > PAIRWISE_MESSAGE_LENGTH )
			? (ciphertext + bytes_encrypted -
			  PAIRWISE_MESSAGE_LENGTH )
			: ciphertext;

	/* Check to ensure that ciphertext does */
	/* NOT EQUAL known input message text   */
	/* per FIPS PUB 140-1 directive.        */
	if( ( bytes_encrypted != max_bytes_encrypted ) ||
	    ( PORT_Memcmp( text_compared, known_message,
			   bytes_compared ) == 0 ) ) {
	    /* Set error to Invalid PRIVATE Key. */
	    PORT_SetError( SEC_ERROR_INVALID_KEY );
	    PORT_Free( ciphertext );
	    return SECFailure;
	}

	slot = privKey->pkcs11Slot;
	/* Prepare for decryption using the private key. */
        PK11_EnterSlotMonitor(slot);
	crv = PK11_GETTAB( slot )->C_DecryptInit( slot->session,
						  mech,
						  privKey->pkcs11ID );
	if( crv != CKR_OK ) {
	    PK11_ExitSlotMonitor(slot);
	    PORT_SetError( PK11_MapError(crv) );
	    PORT_Free( ciphertext );
	    return SECFailure;
	}

	/* Initialize bytes decrypted to be the */
	/* expected PAIRWISE_MESSAGE_LENGTH.    */
	bytes_decrypted = PAIRWISE_MESSAGE_LENGTH;

	/* Decrypt using the private key.   */
	/* NOTE:  No need to reset the      */
	/*        value of bytes_encrypted. */
	crv = PK11_GETTAB( slot )->C_Decrypt( slot->session,
					      ciphertext,
					      bytes_encrypted,
					      plaintext,
					      &bytes_decrypted );
	PK11_ExitSlotMonitor(slot);

	/* Finished with ciphertext; free it. */
	PORT_Free( ciphertext );

	if( crv != CKR_OK ) {
	   PORT_SetError( PK11_MapError(crv) );
	   return SECFailure;
	}

	/* Check to ensure that the output plaintext */
	/* does EQUAL known input message text.      */
	if( ( bytes_decrypted != PAIRWISE_MESSAGE_LENGTH ) ||
	    ( PORT_Memcmp( plaintext, known_message,
			   PAIRWISE_MESSAGE_LENGTH ) != 0 ) ) {
	    /* Set error to Bad PUBLIC Key. */
	    PORT_SetError( SEC_ERROR_BAD_KEY );
	    return SECFailure;
	}
      }

    /**********************************************/
    /* Pairwise Consistency Check of Sign/Verify. */
    /**********************************************/

    canSignVerify = PK11_HasAttributeSet ( privKey->pkcs11Slot, 
					  privKey->pkcs11ID, CKA_SIGN);
    
    if (canSignVerify)
      {
	/* Initialize signature and digest data. */
	signature.data = NULL;
	digest.data = NULL;
	
	/* Determine length of signature. */
	signature_length = PK11_SignatureLen( privKey );
	if( signature_length == 0 )
	  goto failure;
	
	/* Allocate space for signature data. */
	signature.data = (unsigned char *) PORT_Alloc( signature_length );
	if( signature.data == NULL ) {
	  PORT_SetError( SEC_ERROR_NO_MEMORY );
	  goto failure;
	}
	
	/* Allocate space for known digest data. */
	digest.data = (unsigned char *) PORT_Alloc( PAIRWISE_DIGEST_LENGTH );
	if( digest.data == NULL ) {
	  PORT_SetError( SEC_ERROR_NO_MEMORY );
	  goto failure;
	}
	
	/* "Fill" signature type and length. */
	signature.type = PAIRWISE_SECITEM_TYPE;
	signature.len  = signature_length;
	
	/* "Fill" digest with known SHA-1 digest parameters. */
	digest.type = PAIRWISE_SECITEM_TYPE;
	PORT_Memcpy( digest.data, known_digest, PAIRWISE_DIGEST_LENGTH );
	digest.len = PAIRWISE_DIGEST_LENGTH;
	
	/* Sign the known hash using the private key. */
	rv = PK11_Sign( privKey, &signature, &digest );
	if( rv != SECSuccess )
	  goto failure;
	
	/* Verify the known hash using the public key. */
	rv = PK11_Verify( pubKey, &signature, &digest, wincx );
    if( rv != SECSuccess )
      goto failure;
	
	/* Free signature and digest data. */
	PORT_Free( signature.data );
	PORT_Free( digest.data );
      }



    /**********************************************/
    /* Pairwise Consistency Check for Derivation  */
    /**********************************************/

    isDerivable = PK11_HasAttributeSet ( privKey->pkcs11Slot, 
					  privKey->pkcs11ID, CKA_DERIVE);
    
    if (isDerivable)
      {   
	/* 
	 * We are not doing consistency check for Diffie-Hellman Key - 
	 * otherwise it would be here
	 * This is also true for Elliptic Curve Diffie-Hellman keys
	 * NOTE: EC keys are currently subjected to pairwise
	 * consistency check for signing/verification.
	 */

      }

    return SECSuccess;

failure:
    if( signature.data != NULL )
	PORT_Free( signature.data );
    if( digest.data != NULL )
	PORT_Free( digest.data );

    return SECFailure;
}



/*
 * take a private key in one pkcs11 module and load it into another:
 *  NOTE: the source private key is a rare animal... it can't be sensitive.
 *  This is used to do a key gen using one pkcs11 module and storing the
 *  result into another.
 */
SECKEYPrivateKey *
pk11_loadPrivKey(PK11SlotInfo *slot,SECKEYPrivateKey *privKey, 
		SECKEYPublicKey *pubKey, PRBool token, PRBool sensitive) 
{
    CK_ATTRIBUTE privTemplate[] = {
        /* class must be first */
	{ CKA_CLASS, NULL, 0 },
	{ CKA_KEY_TYPE, NULL, 0 },
	/* these three must be next */
	{ CKA_TOKEN, NULL, 0 },
	{ CKA_PRIVATE, NULL, 0 },
	{ CKA_SENSITIVE, NULL, 0 },
	{ CKA_ID, NULL, 0 },
#ifdef notdef
	{ CKA_LABEL, NULL, 0 },
	{ CKA_SUBJECT, NULL, 0 },
#endif
	/* RSA */
	{ CKA_MODULUS, NULL, 0 },
	{ CKA_PRIVATE_EXPONENT, NULL, 0 },
	{ CKA_PUBLIC_EXPONENT, NULL, 0 },
	{ CKA_PRIME_1, NULL, 0 },
	{ CKA_PRIME_2, NULL, 0 },
	{ CKA_EXPONENT_1, NULL, 0 },
	{ CKA_EXPONENT_2, NULL, 0 },
	{ CKA_COEFFICIENT, NULL, 0 },
    };
    CK_ATTRIBUTE *attrs = NULL, *ap;
    int templateSize = sizeof(privTemplate)/sizeof(privTemplate[0]);
    PRArenaPool *arena;
    CK_OBJECT_HANDLE objectID;
    int i, count = 0;
    int extra_count = 0;
    CK_RV crv;
    SECStatus rv;

    for (i=0; i < templateSize; i++) {
	if (privTemplate[i].type == CKA_MODULUS) {
	    attrs= &privTemplate[i];
	    count = i;
	    break;
	}
    }
    PORT_Assert(attrs != NULL);
    if (attrs == NULL) {
	PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
	return NULL;
    }

    ap = attrs;

    switch (privKey->keyType) {
    case rsaKey:
	count = templateSize;
	extra_count = templateSize - (attrs - privTemplate);
	break;
    case dsaKey:
	ap->type = CKA_PRIME; ap++; count++; extra_count++;
	ap->type = CKA_SUBPRIME; ap++; count++; extra_count++;
	ap->type = CKA_BASE; ap++; count++; extra_count++;
	ap->type = CKA_VALUE; ap++; count++; extra_count++;
	break;
    case dhKey:
	ap->type = CKA_PRIME; ap++; count++; extra_count++;
	ap->type = CKA_BASE; ap++; count++; extra_count++;
	ap->type = CKA_VALUE; ap++; count++; extra_count++;
	break;
#ifdef NSS_ENABLE_ECC
    case ecKey:
	ap->type = CKA_EC_PARAMS; ap++; count++; extra_count++;
	ap->type = CKA_VALUE; ap++; count++; extra_count++;
	break;
#endif /* NSS_ENABLE_ECC */       
     default:
	count = 0;
	extra_count = 0;
	break;
     }

     if (count == 0) {
	PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
	return NULL;
     }

     arena = PORT_NewArena( DER_DEFAULT_CHUNKSIZE);
     if (arena == NULL) return NULL;
     /*
      * read out the old attributes.
      */
     crv = PK11_GetAttributes(arena, privKey->pkcs11Slot, privKey->pkcs11ID,
		privTemplate,count);
     if (crv != CKR_OK) {
	PORT_SetError( PK11_MapError(crv) );
	PORT_FreeArena(arena, PR_TRUE);
	return NULL;
     }

     /* Reset sensitive, token, and private */
     *(CK_BBOOL *)(privTemplate[2].pValue) = token ? CK_TRUE : CK_FALSE;
     *(CK_BBOOL *)(privTemplate[3].pValue) = token ? CK_TRUE : CK_FALSE;
     *(CK_BBOOL *)(privTemplate[4].pValue) = sensitive ? CK_TRUE : CK_FALSE;

     /* Not everyone can handle zero padded key values, give
      * them the raw data as unsigned */
     for (ap=attrs; extra_count; ap++, extra_count--) {
	pk11_SignedToUnsigned(ap);
     }

     /* now Store the puppies */
     rv = PK11_CreateNewObject(slot, CK_INVALID_SESSION, privTemplate, 
						count, token, &objectID);
     PORT_FreeArena(arena, PR_TRUE);
     if (rv != SECSuccess) {
	return NULL;
     }

     /* try loading the public key as a token object */
     if (pubKey) {
	PK11_ImportPublicKey(slot, pubKey, PR_TRUE);
	if (pubKey->pkcs11Slot) {
	    PK11_FreeSlot(pubKey->pkcs11Slot);
	    pubKey->pkcs11Slot = NULL;
	    pubKey->pkcs11ID = CK_INVALID_HANDLE;
	}
     }

     /* build new key structure */
     return PK11_MakePrivKey(slot, privKey->keyType, (PRBool)!token, 
						objectID, privKey->wincx);
}

/*
 * export this for PSM
 */
SECKEYPrivateKey *
PK11_LoadPrivKey(PK11SlotInfo *slot,SECKEYPrivateKey *privKey, 
		SECKEYPublicKey *pubKey, PRBool token, PRBool sensitive) 
{
    return pk11_loadPrivKey(slot,privKey,pubKey,token,sensitive);
}


/*
 * Use the token to Generate a key. keySize must be 'zero' for fixed key
 * length algorithms. NOTE: this means we can never generate a DES2 key
 * from this interface!
 */
SECKEYPrivateKey *
PK11_GenerateKeyPair(PK11SlotInfo *slot,CK_MECHANISM_TYPE type, 
   void *param, SECKEYPublicKey **pubKey, PRBool token, 
					PRBool sensitive, void *wincx)
{
    /* we have to use these native types because when we call PKCS 11 modules
     * we have to make sure that we are using the correct sizes for all the
     * parameters. */
    CK_BBOOL ckfalse = CK_FALSE;
    CK_BBOOL cktrue = CK_TRUE;
    CK_ULONG modulusBits;
    CK_BYTE publicExponent[4];
    CK_ATTRIBUTE privTemplate[] = {
	{ CKA_SENSITIVE, NULL, 0},
	{ CKA_TOKEN,  NULL, 0},
	{ CKA_PRIVATE,  NULL, 0},
	{ CKA_DERIVE,  NULL, 0},
	{ CKA_UNWRAP,  NULL, 0},
	{ CKA_SIGN,  NULL, 0},
	{ CKA_DECRYPT,  NULL, 0},
    };
    CK_ATTRIBUTE rsaPubTemplate[] = {
	{ CKA_MODULUS_BITS, NULL, 0},
	{ CKA_PUBLIC_EXPONENT, NULL, 0},
	{ CKA_TOKEN,  NULL, 0},
	{ CKA_DERIVE,  NULL, 0},
	{ CKA_WRAP,  NULL, 0},
	{ CKA_VERIFY,  NULL, 0},
	{ CKA_VERIFY_RECOVER,  NULL, 0},
	{ CKA_ENCRYPT,  NULL, 0},
    };
    CK_ATTRIBUTE dsaPubTemplate[] = {
	{ CKA_PRIME, NULL, 0 },
	{ CKA_SUBPRIME, NULL, 0 },
	{ CKA_BASE, NULL, 0 },
	{ CKA_TOKEN,  NULL, 0},
	{ CKA_DERIVE,  NULL, 0},
	{ CKA_WRAP,  NULL, 0},
	{ CKA_VERIFY,  NULL, 0},
	{ CKA_VERIFY_RECOVER,  NULL, 0},
	{ CKA_ENCRYPT,  NULL, 0},
    };
    CK_ATTRIBUTE dhPubTemplate[] = {
      { CKA_PRIME, NULL, 0 }, 
      { CKA_BASE, NULL, 0 }, 
      { CKA_TOKEN,  NULL, 0},
      { CKA_DERIVE,  NULL, 0},
      { CKA_WRAP,  NULL, 0},
      { CKA_VERIFY,  NULL, 0},
      { CKA_VERIFY_RECOVER,  NULL, 0},
      { CKA_ENCRYPT,  NULL, 0},
    };
#ifdef NSS_ENABLE_ECC
    CK_ATTRIBUTE ecPubTemplate[] = {
      { CKA_EC_PARAMS, NULL, 0 }, 
      { CKA_TOKEN,  NULL, 0},
      { CKA_DERIVE,  NULL, 0},
      { CKA_WRAP,  NULL, 0},
      { CKA_VERIFY,  NULL, 0},
      { CKA_VERIFY_RECOVER,  NULL, 0},
      { CKA_ENCRYPT,  NULL, 0},
    };
    int ecPubCount = sizeof(ecPubTemplate)/sizeof(ecPubTemplate[0]);
    SECKEYECParams * ecParams;
#endif /* NSS_ENABLE_ECC */

    int dsaPubCount = sizeof(dsaPubTemplate)/sizeof(dsaPubTemplate[0]);
    /*CK_ULONG key_size = 0;*/
    CK_ATTRIBUTE *pubTemplate;
    int privCount = sizeof(privTemplate)/sizeof(privTemplate[0]);
    int rsaPubCount = sizeof(rsaPubTemplate)/sizeof(rsaPubTemplate[0]);
    int dhPubCount = sizeof(dhPubTemplate)/sizeof(dhPubTemplate[0]);
    int pubCount = 0;
    PK11RSAGenParams *rsaParams;
    SECKEYPQGParams *dsaParams;
    SECKEYDHParams * dhParams;
    CK_MECHANISM mechanism;
    CK_MECHANISM test_mech;
    CK_SESSION_HANDLE session_handle;
    CK_RV crv;
    CK_OBJECT_HANDLE privID,pubID;
    SECKEYPrivateKey *privKey;
    KeyType keyType;
    PRBool restore;
    int peCount,i;
    CK_ATTRIBUTE *attrs;
    CK_ATTRIBUTE *privattrs;
    SECItem *pubKeyIndex;
    CK_ATTRIBUTE setTemplate;
    SECStatus rv;
    CK_MECHANISM_INFO mechanism_info;
    CK_OBJECT_CLASS keyClass;
    SECItem *cka_id;
    PRBool haslock = PR_FALSE;
    PRBool pubIsToken = PR_FALSE;

    PORT_Assert(slot != NULL);
    if (slot == NULL) {
	PORT_SetError( SEC_ERROR_NO_MODULE);
	return NULL;
    }

    /* if our slot really doesn't do this mechanism, Generate the key
     * in our internal token and write it out */
    if (!PK11_DoesMechanism(slot,type)) {
	PK11SlotInfo *int_slot = PK11_GetInternalSlot();

	/* don't loop forever looking for a slot */
	if (slot == int_slot) {
	    PK11_FreeSlot(int_slot);
	    PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
	    return NULL;
	}

	/* if there isn't a suitable slot, then we can't do the keygen */
	if (int_slot == NULL) {
	    PORT_SetError( SEC_ERROR_NO_MODULE );
	    return NULL;
	}

	/* generate the temporary key to load */
	privKey = PK11_GenerateKeyPair(int_slot,type, param, pubKey, PR_FALSE, 
							PR_FALSE, wincx);
	PK11_FreeSlot(int_slot);

	/* if successful, load the temp key into the new token */
	if (privKey != NULL) {
	    SECKEYPrivateKey *newPrivKey = pk11_loadPrivKey(slot,privKey,
						*pubKey,token,sensitive);
	    SECKEY_DestroyPrivateKey(privKey);
	    if (newPrivKey == NULL) {
		SECKEY_DestroyPublicKey(*pubKey);
		*pubKey = NULL;
	    }
	    return newPrivKey;
	}
	return NULL;
   }


    mechanism.mechanism = type;
    mechanism.pParameter = NULL;
    mechanism.ulParameterLen = 0;
    test_mech.pParameter = NULL;
    test_mech.ulParameterLen = 0;

    /* set up the private key template */
    privattrs = privTemplate;
    PK11_SETATTRS(privattrs, CKA_SENSITIVE, sensitive ? &cktrue : &ckfalse, 
					sizeof(CK_BBOOL)); privattrs++;
    PK11_SETATTRS(privattrs, CKA_TOKEN, token ? &cktrue : &ckfalse,
					 sizeof(CK_BBOOL)); privattrs++;
    PK11_SETATTRS(privattrs, CKA_PRIVATE, sensitive ? &cktrue : &ckfalse,
					 sizeof(CK_BBOOL)); privattrs++;

    /* set up the mechanism specific info */
    switch (type) {
    case CKM_RSA_PKCS_KEY_PAIR_GEN:
	rsaParams = (PK11RSAGenParams *)param;
	modulusBits = rsaParams->keySizeInBits;
	peCount = 0;

	/* convert pe to a PKCS #11 string */
	for (i=0; i < 4; i++) {
	    if (peCount || (rsaParams->pe & 
				((unsigned long)0xff000000L >> (i*8)))) {
		publicExponent[peCount] = 
				(CK_BYTE)((rsaParams->pe >> (3-i)*8) & 0xff);
		peCount++;
	    }
	}
	PORT_Assert(peCount != 0);
	attrs = rsaPubTemplate;
	PK11_SETATTRS(attrs, CKA_MODULUS_BITS, 
				&modulusBits, sizeof(modulusBits)); attrs++;
	PK11_SETATTRS(attrs, CKA_PUBLIC_EXPONENT, 
				publicExponent, peCount);attrs++;
	pubTemplate = rsaPubTemplate;
	pubCount = rsaPubCount;
	keyType = rsaKey;
	test_mech.mechanism = CKM_RSA_PKCS;
	break;
    case CKM_DSA_KEY_PAIR_GEN:
	dsaParams = (SECKEYPQGParams *)param;
	attrs = dsaPubTemplate;
	PK11_SETATTRS(attrs, CKA_PRIME, dsaParams->prime.data,
				dsaParams->prime.len); attrs++;
	PK11_SETATTRS(attrs, CKA_SUBPRIME, dsaParams->subPrime.data,
					dsaParams->subPrime.len); attrs++;
	PK11_SETATTRS(attrs, CKA_BASE, dsaParams->base.data,
						dsaParams->base.len); attrs++;
	pubTemplate = dsaPubTemplate;
	pubCount = dsaPubCount;
	keyType = dsaKey;
	test_mech.mechanism = CKM_DSA;
	break;
    case CKM_DH_PKCS_KEY_PAIR_GEN:
        dhParams = (SECKEYDHParams *)param;
        attrs = dhPubTemplate;
        PK11_SETATTRS(attrs, CKA_PRIME, dhParams->prime.data,
                      dhParams->prime.len);   attrs++;
        PK11_SETATTRS(attrs, CKA_BASE, dhParams->base.data,
                      dhParams->base.len);    attrs++;
        pubTemplate = dhPubTemplate;
	pubCount = dhPubCount;
        keyType = dhKey;
        test_mech.mechanism = CKM_DH_PKCS_DERIVE;
	break;
#ifdef NSS_ENABLE_ECC
    case CKM_EC_KEY_PAIR_GEN:
        ecParams = (SECKEYECParams *)param;
        attrs = ecPubTemplate;
        PK11_SETATTRS(attrs, CKA_EC_PARAMS, ecParams->data, 
	              ecParams->len);   attrs++;
        pubTemplate = ecPubTemplate;
        pubCount = ecPubCount;
        keyType = ecKey;
	/* XXX An EC key can be used for other mechanisms too such
	 * as CKM_ECDSA and CKM_ECDSA_SHA1. How can we reflect
	 * that in test_mech.mechanism so the CKA_SIGN, CKA_VERIFY
	 * attributes are set correctly? 
	 */
        test_mech.mechanism = CKM_ECDH1_DERIVE;
        break;
#endif /* NSS_ENABLE_ECC */
    default:
	PORT_SetError( SEC_ERROR_BAD_KEY );
	return NULL;
    }

    /* now query the slot to find out how "good" a key we can generate */
    if (!slot->isThreadSafe) PK11_EnterSlotMonitor(slot);
    crv = PK11_GETTAB(slot)->C_GetMechanismInfo(slot->slotID,
				test_mech.mechanism,&mechanism_info);
    if (!slot->isThreadSafe) PK11_ExitSlotMonitor(slot);
    if ((crv != CKR_OK) || (mechanism_info.flags == 0)) {
	/* must be old module... guess what it should be... */
	switch (test_mech.mechanism) {
	case CKM_RSA_PKCS:
		mechanism_info.flags = (CKF_SIGN | CKF_DECRYPT | 
			CKF_WRAP | CKF_VERIFY_RECOVER | CKF_ENCRYPT | CKF_WRAP);;
		break;
	case CKM_DSA:
		mechanism_info.flags = CKF_SIGN | CKF_VERIFY;
		break;
	case CKM_DH_PKCS_DERIVE:
		mechanism_info.flags = CKF_DERIVE;
		break;
#ifdef NSS_ENABLE_ECC
	case CKM_ECDH1_DERIVE:
		mechanism_info.flags = CKF_DERIVE;
		break;
	case CKM_ECDSA:
	case CKM_ECDSA_SHA1:
		mechanism_info.flags = CKF_SIGN | CKF_VERIFY;
		break;
#endif /* NSS_ENABLE_ECC */
	default:
	       break;
	}
    }
    /* set the public key objects */
    PK11_SETATTRS(attrs, CKA_TOKEN, token ? &cktrue : &ckfalse,
					 sizeof(CK_BBOOL)); attrs++;
    PK11_SETATTRS(attrs, CKA_DERIVE, 
		mechanism_info.flags & CKF_DERIVE ? &cktrue : &ckfalse,
					 sizeof(CK_BBOOL)); attrs++;
    PK11_SETATTRS(attrs, CKA_WRAP, 
		mechanism_info.flags & CKF_WRAP ? &cktrue : &ckfalse,
					 sizeof(CK_BBOOL)); attrs++;
    PK11_SETATTRS(attrs, CKA_VERIFY, 
		mechanism_info.flags & CKF_VERIFY ? &cktrue : &ckfalse,
					 sizeof(CK_BBOOL)); attrs++;
    PK11_SETATTRS(attrs, CKA_VERIFY_RECOVER, 
		mechanism_info.flags & CKF_VERIFY_RECOVER ? &cktrue : &ckfalse,
					 sizeof(CK_BBOOL)); attrs++;
    PK11_SETATTRS(attrs, CKA_ENCRYPT, 
		mechanism_info.flags & CKF_ENCRYPT? &cktrue : &ckfalse,
					 sizeof(CK_BBOOL)); attrs++;
    PK11_SETATTRS(privattrs, CKA_DERIVE, 
		mechanism_info.flags & CKF_DERIVE ? &cktrue : &ckfalse,
					 sizeof(CK_BBOOL)); privattrs++;
    PK11_SETATTRS(privattrs, CKA_UNWRAP, 
		mechanism_info.flags & CKF_UNWRAP ? &cktrue : &ckfalse,
					 sizeof(CK_BBOOL)); privattrs++;
    PK11_SETATTRS(privattrs, CKA_SIGN, 
		mechanism_info.flags & CKF_SIGN ? &cktrue : &ckfalse,
					 sizeof(CK_BBOOL)); privattrs++;
    PK11_SETATTRS(privattrs, CKA_DECRYPT, 
		mechanism_info.flags & CKF_DECRYPT ? &cktrue : &ckfalse,
					 sizeof(CK_BBOOL)); privattrs++;

    if (token) {
	session_handle = PK11_GetRWSession(slot);
	haslock = PK11_RWSessionHasLock(slot,session_handle);
	restore = PR_TRUE;
    } else {
        PK11_EnterSlotMonitor(slot); /* gross!! */
	session_handle = slot->session;
	restore = PR_FALSE;
	haslock = PR_TRUE;
    }

    crv = PK11_GETTAB(slot)->C_GenerateKeyPair(session_handle, &mechanism,
	pubTemplate,pubCount,privTemplate,privCount,&pubID,&privID);

    if (crv != CKR_OK) {
	if (restore)  {
	    PK11_RestoreROSession(slot,session_handle);
	} else PK11_ExitSlotMonitor(slot);
	PORT_SetError( PK11_MapError(crv) );
	return NULL;
    }
    /* This locking code is dangerous and needs to be more thought
     * out... the real problem is that we're holding the mutex open this long
     */
    if (haslock) { PK11_ExitSlotMonitor(slot); }

    /* swap around the ID's for older PKCS #11 modules */
    keyClass = PK11_ReadULongAttribute(slot,pubID,CKA_CLASS);
    if (keyClass != CKO_PUBLIC_KEY) {
	CK_OBJECT_HANDLE tmp = pubID;
	pubID = privID;
	privID = tmp;
    }

    *pubKey = PK11_ExtractPublicKey(slot, keyType, pubID);
    if (*pubKey == NULL) {
	if (restore)  {
	    /* we may have to restore the mutex so it get's exited properly
	     * in RestoreROSession */
            if (haslock)  PK11_EnterSlotMonitor(slot); 
	    PK11_RestoreROSession(slot,session_handle);
	} 
	PK11_DestroyObject(slot,pubID);
	PK11_DestroyObject(slot,privID);
	return NULL;
    }

    /* set the ID to the public key so we can find it again */
    pubKeyIndex =  NULL;
    switch (type) {
    case CKM_RSA_PKCS_KEY_PAIR_GEN:
      pubKeyIndex = &(*pubKey)->u.rsa.modulus;
      break;
    case CKM_DSA_KEY_PAIR_GEN:
      pubKeyIndex = &(*pubKey)->u.dsa.publicValue;
      break;
    case CKM_DH_PKCS_KEY_PAIR_GEN:
      pubKeyIndex = &(*pubKey)->u.dh.publicValue;
      break;      
#ifdef NSS_ENABLE_ECC
    case CKM_EC_KEY_PAIR_GEN:
      pubKeyIndex = &(*pubKey)->u.ec.publicValue;
      break;      
#endif /* NSS_ENABLE_ECC */
    }
    PORT_Assert(pubKeyIndex != NULL);

    cka_id = PK11_MakeIDFromPubKey(pubKeyIndex);
    pubIsToken = (PRBool)PK11_HasAttributeSet(slot,pubID, CKA_TOKEN);

    PK11_SETATTRS(&setTemplate, CKA_ID, cka_id->data, cka_id->len);

    if (haslock) { PK11_EnterSlotMonitor(slot); }
    crv = PK11_GETTAB(slot)->C_SetAttributeValue(session_handle, privID,
		&setTemplate, 1);
   
    if (crv == CKR_OK && pubIsToken) {
    	crv = PK11_GETTAB(slot)->C_SetAttributeValue(session_handle, pubID,
		&setTemplate, 1);
    }


    if (restore) {
	PK11_RestoreROSession(slot,session_handle);
    } else {
	PK11_ExitSlotMonitor(slot);
    }
    SECITEM_FreeItem(cka_id,PR_TRUE);


    if (crv != CKR_OK) {
	PK11_DestroyObject(slot,pubID);
	PK11_DestroyObject(slot,privID);
	PORT_SetError( PK11_MapError(crv) );
	*pubKey = NULL;
	return NULL;
    }

    privKey = PK11_MakePrivKey(slot,keyType,(PRBool)!token,privID,wincx);
    if (privKey == NULL) {
	SECKEY_DestroyPublicKey(*pubKey);
	PK11_DestroyObject(slot,privID);
	*pubKey = NULL;
	return NULL;  /* due to pairwise consistency check */
    }

    /* Perform PKCS #11 pairwise consistency check. */
    rv = pk11_PairwiseConsistencyCheck( *pubKey, privKey, &test_mech, wincx );
    if( rv != SECSuccess ) {
	SECKEY_DestroyPublicKey( *pubKey );
	SECKEY_DestroyPrivateKey( privKey );
	*pubKey = NULL;
	privKey = NULL;
	return NULL;
    }

    return privKey;
}

/* build a public KEA key from the public value */
SECKEYPublicKey *
PK11_MakeKEAPubKey(unsigned char *keyData,int length)
{
    SECKEYPublicKey *pubk;
    SECItem pkData;
    SECStatus rv;
    PRArenaPool *arena;

    pkData.data = keyData;
    pkData.len = length;

    arena = PORT_NewArena (DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL)
	return NULL;

    pubk = (SECKEYPublicKey *) PORT_ArenaZAlloc(arena, sizeof(SECKEYPublicKey));
    if (pubk == NULL) {
	PORT_FreeArena (arena, PR_FALSE);
	return NULL;
    }

    pubk->arena = arena;
    pubk->pkcs11Slot = 0;
    pubk->pkcs11ID = CK_INVALID_HANDLE;
    pubk->keyType = fortezzaKey;
    rv = SECITEM_CopyItem(arena, &pubk->u.fortezza.KEAKey, &pkData);
    if (rv != SECSuccess) {
	PORT_FreeArena (arena, PR_FALSE);
	return NULL;
    }
    return pubk;
}

SECStatus 
PK11_ImportEncryptedPrivateKeyInfo(PK11SlotInfo *slot,
			SECKEYEncryptedPrivateKeyInfo *epki, SECItem *pwitem,
			SECItem *nickname, SECItem *publicValue, PRBool isPerm,
			PRBool isPrivate, KeyType keyType, 
			unsigned int keyUsage, void *wincx)
{
    CK_MECHANISM_TYPE mechanism;
    SECItem *pbe_param, crypto_param;
    PK11SymKey *key = NULL;
    SECStatus rv = SECSuccess;
    CK_MECHANISM cryptoMech, pbeMech;
    CK_RV crv;
    SECKEYPrivateKey *privKey = NULL;
    PRBool faulty3DES = PR_FALSE;
    int usageCount = 0;
    CK_KEY_TYPE key_type;
    CK_ATTRIBUTE_TYPE *usage = NULL;
    CK_ATTRIBUTE_TYPE rsaUsage[] = {
		 CKA_UNWRAP, CKA_DECRYPT, CKA_SIGN, CKA_SIGN_RECOVER };
    CK_ATTRIBUTE_TYPE dsaUsage[] = { CKA_SIGN };
    CK_ATTRIBUTE_TYPE dhUsage[] = { CKA_DERIVE };
#ifdef NSS_ENABLE_ECC
    CK_ATTRIBUTE_TYPE ecUsage[] = { CKA_SIGN, CKA_DERIVE };
#endif /* NSS_ENABLE_ECC */
    if((epki == NULL) || (pwitem == NULL))
	return SECFailure;

    crypto_param.data = NULL;

    mechanism = PK11_AlgtagToMechanism(SECOID_FindOIDTag(
					&epki->algorithm.algorithm));

    switch (keyType) {
    default:
    case rsaKey:
	key_type = CKK_RSA;
	switch  (keyUsage & (KU_KEY_ENCIPHERMENT|KU_DIGITAL_SIGNATURE)) {
	case KU_KEY_ENCIPHERMENT:
	    usage = rsaUsage;
	    usageCount = 2;
	    break;
	case KU_DIGITAL_SIGNATURE:
	    usage = &rsaUsage[2];
	    usageCount = 2;
	    break;
	case KU_KEY_ENCIPHERMENT|KU_DIGITAL_SIGNATURE:
	case 0: /* default to everything */
	    usage = rsaUsage;
	    usageCount = 4;
	    break;
	}
        break;
    case dhKey:
	key_type = CKK_DH;
	usage = dhUsage;
	usageCount = sizeof(dhUsage)/sizeof(dhUsage[0]);
	break;
    case dsaKey:
	key_type = CKK_DSA;
	usage = dsaUsage;
	usageCount = sizeof(dsaUsage)/sizeof(dsaUsage[0]);
	break;
#ifdef NSS_ENABLE_ECC
    case ecKey:
	key_type = CKK_EC;
	switch  (keyUsage & (KU_DIGITAL_SIGNATURE|KU_KEY_AGREEMENT)) {
	case KU_DIGITAL_SIGNATURE:
	    usage = ecUsage;
	    usageCount = 1;
	    break;
	case KU_KEY_AGREEMENT:
	    usage = &ecUsage[1];
	    usageCount = 1;
	    break;
	case KU_DIGITAL_SIGNATURE|KU_KEY_AGREEMENT:
	default: /* default to everything */
	    usage = ecUsage;
	    usageCount = 2;
	    break;
	}
	break;	
#endif /* NSS_ENABLE_ECC */
    }

try_faulty_3des:
    pbe_param = PK11_ParamFromAlgid(&epki->algorithm);

    key = PK11_RawPBEKeyGen(slot, mechanism, pbe_param, pwitem, 
							faulty3DES, wincx);
    if((key == NULL) || (pbe_param == NULL)) {
	rv = SECFailure;
	goto done;
    }

    pbeMech.mechanism = mechanism;
    pbeMech.pParameter = pbe_param->data;
    pbeMech.ulParameterLen = pbe_param->len;

    crv = PK11_MapPBEMechanismToCryptoMechanism(&pbeMech, &cryptoMech, 
					        pwitem, faulty3DES);
    if(crv != CKR_OK) {
	rv = SECFailure;
	goto done;
    }

    cryptoMech.mechanism = PK11_GetPadMechanism(cryptoMech.mechanism);
    crypto_param.data = (unsigned char*)cryptoMech.pParameter;
    crypto_param.len = cryptoMech.ulParameterLen;

    PORT_Assert(usage != NULL);
    PORT_Assert(usageCount != 0);
    privKey = PK11_UnwrapPrivKey(slot, key, cryptoMech.mechanism, 
				 &crypto_param, &epki->encryptedData, 
				 nickname, publicValue, isPerm, isPrivate,
				 key_type, usage, usageCount, wincx);
    if(privKey) {
	SECKEY_DestroyPrivateKey(privKey);
	privKey = NULL;
	rv = SECSuccess;
	goto done;
    }

    /* if we are unable to import the key and the mechanism is 
     * CKM_NETSCAPE_PBE_SHA1_TRIPLE_DES_CBC, then it is possible that
     * the encrypted blob was created with a buggy key generation method
     * which is described in the PKCS 12 implementation notes.  So we
     * need to try importing via that method.
     */ 
    if((mechanism == CKM_NETSCAPE_PBE_SHA1_TRIPLE_DES_CBC) && (!faulty3DES)) {
	/* clean up after ourselves before redoing the key generation. */

	PK11_FreeSymKey(key);
	key = NULL;

	if(pbe_param) {
	    SECITEM_ZfreeItem(pbe_param, PR_TRUE);
	    pbe_param = NULL;
	}

	if(crypto_param.data) {
	    SECITEM_ZfreeItem(&crypto_param, PR_FALSE);
	    crypto_param.data = NULL;
	    cryptoMech.pParameter = NULL;
	    crypto_param.len = cryptoMech.ulParameterLen = 0;
	}

	faulty3DES = PR_TRUE;
	goto try_faulty_3des;
    }

    /* key import really did fail */
    rv = SECFailure;

done:
    if(pbe_param != NULL) {
	SECITEM_ZfreeItem(pbe_param, PR_TRUE);
	pbe_param = NULL;
    }

    if(crypto_param.data != NULL) {
	SECITEM_ZfreeItem(&crypto_param, PR_FALSE);
    }

    if(key != NULL) {
    	PK11_FreeSymKey(key);
    }

    return rv;
}

SECKEYPrivateKeyInfo *
PK11_ExportPrivateKeyInfo(CERTCertificate *cert, void *wincx)
{
    return NULL;
}

static int
pk11_private_key_encrypt_buffer_length(SECKEYPrivateKey *key)
				
{
    CK_ATTRIBUTE rsaTemplate = { CKA_MODULUS, NULL, 0 };
    CK_ATTRIBUTE dsaTemplate = { CKA_PRIME, NULL, 0 };
#ifdef NSS_ENABLE_ECC
    /* XXX We should normally choose an attribute such that
     * factor times its size is enough to hold the private key.
     * For EC keys, we have no choice but to use CKA_EC_PARAMS,
     * CKA_VALUE is not available for token keys. But for named
     * curves, the number of bytes needed to represent the params
     * is quite small so we bump up factor from 10 to 15.
     */
    CK_ATTRIBUTE ecTemplate = { CKA_EC_PARAMS, NULL, 0 };
#endif /* NSS_ENABLE_ECC */
    CK_ATTRIBUTE_PTR pTemplate;
    CK_RV crv;
    int length;
    int factor = 10;

    if(!key) {
	return -1;
    }

    switch (key->keyType) {
	case rsaKey:
	    pTemplate = &rsaTemplate;
	    break;
	case dsaKey:
	case dhKey:
	    pTemplate = &dsaTemplate;
	    break;
#ifdef NSS_ENABLE_ECC
        case ecKey:
	    pTemplate = &ecTemplate;
	    factor = 15;
	    break;
#endif /* NSS_ENABLE_ECC */
	case fortezzaKey:
	default:
	    pTemplate = NULL;
    }

    if(!pTemplate) {
	return -1;
    }

    crv = PK11_GetAttributes(NULL, key->pkcs11Slot, key->pkcs11ID, 
								pTemplate, 1);
    if(crv != CKR_OK) {
	PORT_SetError( PK11_MapError(crv) );
	return -1;
    }

    length = pTemplate->ulValueLen;
    length *= factor;


    if(pTemplate->pValue != NULL) {
	PORT_Free(pTemplate->pValue);
    }

    return length;
}

SECKEYEncryptedPrivateKeyInfo * 
PK11_ExportEncryptedPrivKeyInfo(
   PK11SlotInfo     *slot,      /* optional, encrypt key in this slot */
   SECOidTag         algTag,    /* encrypt key with this algorithm */
   SECItem          *pwitem,    /* password for PBE encryption */
   SECKEYPrivateKey *pk,        /* encrypt this private key */
   int               iteration, /* interations for PBE alg */
   void             *wincx)     /* context for password callback ? */
{
    SECKEYEncryptedPrivateKeyInfo *epki      = NULL;
    PRArenaPool                   *arena     = NULL;
    SECAlgorithmID                *algid;
    SECItem                       *pbe_param = NULL;
    PK11SymKey                    *key       = NULL;
    SECStatus                      rv        = SECSuccess;
    int                            encryptBufLen;
    CK_RV                          crv;
    CK_ULONG                       encBufLenPtr;
    CK_MECHANISM_TYPE              mechanism;
    CK_MECHANISM                   pbeMech;
    CK_MECHANISM                   cryptoMech;
    SECItem                        crypto_param;
    SECItem                        encryptedKey = {siBuffer, NULL, 0};

    if (!pwitem || !pk) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return NULL;
    }

    algid = SEC_PKCS5CreateAlgorithmID(algTag, NULL, iteration);
    if (algid == NULL) {
	return NULL;
    }

    crypto_param.data = NULL;

    arena = PORT_NewArena(2048);
    if (arena)
	epki = PORT_ArenaZNew(arena, SECKEYEncryptedPrivateKeyInfo);
    if(epki == NULL) {
	rv = SECFailure;
	goto loser;
    }
    epki->arena = arena;

    mechanism = PK11_AlgtagToMechanism(algTag);
    pbe_param = PK11_ParamFromAlgid(algid);
    if (!pbe_param || mechanism == CKM_INVALID_MECHANISM) {
	rv = SECFailure;
	goto loser;
    }
    pbeMech.mechanism = mechanism;
    pbeMech.pParameter = pbe_param->data;
    pbeMech.ulParameterLen = pbe_param->len;

    /* if we didn't specify a slot, use the slot the private key was in */
    if (!slot) {
	slot = pk->pkcs11Slot;
    }

    /* if we specified a different slot, and the private key slot can do the
     * pbe key gen, generate the key in the private key slot so we don't have 
     * to move it later */
    if (slot != pk->pkcs11Slot) {
	if (PK11_DoesMechanism(pk->pkcs11Slot,mechanism)) {
	    slot = pk->pkcs11Slot;
	}
    }
    key = PK11_RawPBEKeyGen(slot, mechanism, pbe_param, pwitem, 
							PR_FALSE, wincx);

    if((key == NULL) || (pbe_param == NULL)) {
	rv = SECFailure;
	goto loser;
    }

    crv = PK11_MapPBEMechanismToCryptoMechanism(&pbeMech, &cryptoMech, 
						pwitem, PR_FALSE);
    if(crv != CKR_OK) {
	rv = SECFailure;
	goto loser;
    }
    cryptoMech.mechanism = PK11_GetPadMechanism(cryptoMech.mechanism);
    crypto_param.data = (unsigned char *)cryptoMech.pParameter;
    crypto_param.len = cryptoMech.ulParameterLen;


    encryptBufLen = pk11_private_key_encrypt_buffer_length(pk); 
    if(encryptBufLen == -1) {
	rv = SECFailure;
	goto loser;
    }
    encryptedKey.len = (unsigned int)encryptBufLen;
    encBufLenPtr = (CK_ULONG) encryptBufLen;
    encryptedKey.data = (unsigned char *)PORT_ZAlloc(encryptedKey.len);
    if(!encryptedKey.data) {
	rv = SECFailure;
	goto loser;
    }

    /* If the key isn't in the private key slot, move it */
    if (key->slot != pk->pkcs11Slot) {
	PK11SymKey *newkey = pk11_CopyToSlot(pk->pkcs11Slot,
						key->type, CKA_WRAP, key);
	if (newkey == NULL) {
	    rv= SECFailure;
	    goto loser;
	}

	/* free the old key and use the new key */
	PK11_FreeSymKey(key);
	key = newkey;
    }
	
    /* we are extracting an encrypted privateKey structure.
     * which needs to be freed along with the buffer into which it is
     * returned.  eventually, we should retrieve an encrypted key using
     * pkcs8/pkcs5.
     */
    PK11_EnterSlotMonitor(pk->pkcs11Slot);
    crv = PK11_GETTAB(pk->pkcs11Slot)->C_WrapKey(pk->pkcs11Slot->session, 
    		&cryptoMech, key->objectID, pk->pkcs11ID, encryptedKey.data, 
		&encBufLenPtr); 
    PK11_ExitSlotMonitor(pk->pkcs11Slot);
    encryptedKey.len = (unsigned int) encBufLenPtr;
    if(crv != CKR_OK) {
	rv = SECFailure;
	goto loser;
    }

    if(!encryptedKey.len) {
	rv = SECFailure;
	goto loser;
    }
    
    rv = SECITEM_CopyItem(arena, &epki->encryptedData, &encryptedKey);
    if(rv != SECSuccess) {
	goto loser;
    }

    rv = SECOID_CopyAlgorithmID(arena, &epki->algorithm, algid);

loser:
    if(pbe_param != NULL) {
	SECITEM_ZfreeItem(pbe_param, PR_TRUE);
	pbe_param = NULL;
    }

    if(crypto_param.data != NULL) {
	SECITEM_ZfreeItem(&crypto_param, PR_FALSE);
	crypto_param.data = NULL;
    }

    if(key != NULL) {
    	PK11_FreeSymKey(key);
    }
    SECOID_DestroyAlgorithmID(algid, PR_TRUE);

    if(rv == SECFailure) {
	if(arena != NULL) {
	    PORT_FreeArena(arena, PR_TRUE);
	}
	epki = NULL;
    }

    return epki;
}

SECKEYEncryptedPrivateKeyInfo * 
PK11_ExportEncryptedPrivateKeyInfo(
   PK11SlotInfo    *slot,      /* optional, encrypt key in this slot */
   SECOidTag        algTag,    /* encrypt key with this algorithm */
   SECItem         *pwitem,    /* password for PBE encryption */
   CERTCertificate *cert,      /* wrap priv key for this user cert */
   int              iteration, /* interations for PBE alg */
   void            *wincx)     /* context for password callback ? */
{
    SECKEYEncryptedPrivateKeyInfo *epki = NULL;
    SECKEYPrivateKey              *pk   = PK11_FindKeyByAnyCert(cert, wincx);
    if (pk != NULL) {
	epki = PK11_ExportEncryptedPrivKeyInfo(slot, algTag, pwitem, pk, 
	                                       iteration, wincx);
	SECKEY_DestroyPrivateKey(pk);
    }
    return epki;
}

SECItem*
PK11_DEREncodePublicKey(SECKEYPublicKey *pubk)
{
    CERTSubjectPublicKeyInfo *spki=NULL;
    SECItem *spkiDER = NULL;

    if( pubk == NULL ) {
        return NULL;
    }

    /* get the subjectpublickeyinfo */
    spki = SECKEY_CreateSubjectPublicKeyInfo(pubk);
    if( spki == NULL ) {
        goto finish;
    }

    /* DER-encode the subjectpublickeyinfo */
    spkiDER = SEC_ASN1EncodeItem(NULL /*arena*/, NULL/*dest*/, spki,
                    CERT_SubjectPublicKeyInfoTemplate);

finish:
    return spkiDER;
}

char *
PK11_GetPrivateKeyNickname(SECKEYPrivateKey *privKey)
{
    return PK11_GetObjectNickname(privKey->pkcs11Slot,privKey->pkcs11ID);
}

char *
PK11_GetPublicKeyNickname(SECKEYPublicKey *pubKey)
{
    return PK11_GetObjectNickname(pubKey->pkcs11Slot,pubKey->pkcs11ID);
}

SECStatus
PK11_SetPrivateKeyNickname(SECKEYPrivateKey *privKey, const char *nickname)
{
    return PK11_SetObjectNickname(privKey->pkcs11Slot,
					privKey->pkcs11ID,nickname);
}

SECStatus
PK11_SetPublicKeyNickname(SECKEYPublicKey *pubKey, const char *nickname)
{
    return PK11_SetObjectNickname(pubKey->pkcs11Slot,
					pubKey->pkcs11ID,nickname);
}

SECKEYPQGParams *
PK11_GetPQGParamsFromPrivateKey(SECKEYPrivateKey *privKey)
{
    CK_ATTRIBUTE pTemplate[] = {
	{ CKA_PRIME, NULL, 0 },
	{ CKA_SUBPRIME, NULL, 0 },
	{ CKA_BASE, NULL, 0 },
    };
    int pTemplateLen = sizeof(pTemplate)/sizeof(pTemplate[0]);
    PRArenaPool *arena = NULL;
    SECKEYPQGParams *params;
    CK_RV crv;


    arena = PORT_NewArena(2048);
    if (arena == NULL) {
	goto loser;
    }
    params=(SECKEYPQGParams *)PORT_ArenaZAlloc(arena,sizeof(SECKEYPQGParams));
    if (params == NULL) {
	goto loser;
    }

    crv = PK11_GetAttributes(arena, privKey->pkcs11Slot, privKey->pkcs11ID, 
						pTemplate, pTemplateLen);
    if (crv != CKR_OK) {
        PORT_SetError( PK11_MapError(crv) );
	goto loser;
    }

    params->arena = arena;
    params->prime.data = pTemplate[0].pValue;
    params->prime.len = pTemplate[0].ulValueLen;
    params->subPrime.data = pTemplate[1].pValue;
    params->subPrime.len = pTemplate[1].ulValueLen;
    params->base.data = pTemplate[2].pValue;
    params->base.len = pTemplate[2].ulValueLen;

    return params;

loser:
    if (arena != NULL) {
	PORT_FreeArena(arena,PR_FALSE);
    }
    return NULL;
}

SECKEYPrivateKey*
PK11_ConvertSessionPrivKeyToTokenPrivKey(SECKEYPrivateKey *privk, void* wincx)
{
    PK11SlotInfo* slot = privk->pkcs11Slot;
    CK_ATTRIBUTE template[1];
    CK_ATTRIBUTE *attrs = template;
    CK_BBOOL cktrue = CK_TRUE;
    CK_RV crv;
    CK_OBJECT_HANDLE newKeyID;
    CK_SESSION_HANDLE rwsession;

    PK11_SETATTRS(attrs, CKA_TOKEN, &cktrue, sizeof(cktrue)); attrs++;

    PK11_Authenticate(slot, PR_TRUE, wincx);
    rwsession = PK11_GetRWSession(slot);
    crv = PK11_GETTAB(slot)->C_CopyObject(rwsession, privk->pkcs11ID,
        template, 1, &newKeyID);
    PK11_RestoreROSession(slot, rwsession);

    if (crv != CKR_OK) {
        PORT_SetError( PK11_MapError(crv) );
        return NULL;
    }

    return PK11_MakePrivKey(slot, nullKey /*KeyType*/, PR_FALSE /*isTemp*/,
        newKeyID, NULL /*wincx*/);
}
/*
 * destroy a private key if there are no matching certs.
 * this function also frees the privKey structure.
 */
SECStatus
PK11_DeleteTokenPrivateKey(SECKEYPrivateKey *privKey, PRBool force)
{
    CERTCertificate *cert=PK11_GetCertFromPrivateKey(privKey);

    /* found a cert matching the private key?. */
    if (!force  && cert != NULL) {
	/* yes, don't delete the key */
        CERT_DestroyCertificate(cert);
	SECKEY_DestroyPrivateKey(privKey);
	return SECWouldBlock;
    }
    /* now, then it's safe for the key to go away */
    PK11_DestroyTokenObject(privKey->pkcs11Slot,privKey->pkcs11ID);
    SECKEY_DestroyPrivateKey(privKey);
    return SECSuccess;
}

/*
 * destroy a private key if there are no matching certs.
 * this function also frees the privKey structure.
 */
SECStatus
PK11_DeleteTokenPublicKey(SECKEYPublicKey *pubKey)
{
    /* now, then it's safe for the key to go away */
    if (pubKey->pkcs11Slot == NULL) {
	return SECFailure;
    }
    PK11_DestroyTokenObject(pubKey->pkcs11Slot,pubKey->pkcs11ID);
    SECKEY_DestroyPublicKey(pubKey);
    return SECSuccess;
}

/*
 * key call back structure.
 */
typedef struct pk11KeyCallbackStr {
	SECStatus (* callback)(SECKEYPrivateKey *,void *);
	void *callbackArg;
	void *wincx;
} pk11KeyCallback;

/*
 * callback to map Object Handles to Private Keys;
 */
SECStatus
pk11_DoKeys(PK11SlotInfo *slot, CK_OBJECT_HANDLE keyHandle, void *arg)
{
    SECStatus rv = SECSuccess;
    SECKEYPrivateKey *privKey;
    pk11KeyCallback *keycb = (pk11KeyCallback *) arg;

    privKey = PK11_MakePrivKey(slot,nullKey,PR_TRUE,keyHandle,keycb->wincx);

    if (privKey == NULL) {
	return SECFailure;
    }

    if (keycb && (keycb->callback)) {
	rv = (*keycb->callback)(privKey,keycb->callbackArg);
    }

    SECKEY_DestroyPrivateKey(privKey);	    
    return rv;
}

/***********************************************************************
 * PK11_TraversePrivateKeysInSlot
 *
 * Traverses all the private keys on a slot.
 *
 * INPUTS
 *      slot
 *          The PKCS #11 slot whose private keys you want to traverse.
 *      callback
 *          A callback function that will be called for each key.
 *      arg
 *          An argument that will be passed to the callback function.
 */
SECStatus
PK11_TraversePrivateKeysInSlot( PK11SlotInfo *slot,
    SECStatus(* callback)(SECKEYPrivateKey*, void*), void *arg)
{
    pk11KeyCallback perKeyCB;
    pk11TraverseSlot perObjectCB;
    CK_OBJECT_CLASS privkClass = CKO_PRIVATE_KEY;
    CK_BBOOL ckTrue = CK_TRUE;
    CK_ATTRIBUTE theTemplate[2];
    int templateSize = 2;

    theTemplate[0].type = CKA_CLASS;
    theTemplate[0].pValue = &privkClass;
    theTemplate[0].ulValueLen = sizeof(privkClass);
    theTemplate[1].type = CKA_TOKEN;
    theTemplate[1].pValue = &ckTrue;
    theTemplate[1].ulValueLen = sizeof(ckTrue);

    if(slot==NULL) {
        return SECSuccess;
    }

    perObjectCB.callback = pk11_DoKeys;
    perObjectCB.callbackArg = &perKeyCB;
    perObjectCB.findTemplate = theTemplate;
    perObjectCB.templateCount = templateSize;
    perKeyCB.callback = callback;
    perKeyCB.callbackArg = arg;
    perKeyCB.wincx = NULL;

    return PK11_TraverseSlot(slot, &perObjectCB);
}

/*
 * return the private key with the given ID
 */
CK_OBJECT_HANDLE
pk11_FindPrivateKeyFromCertID(PK11SlotInfo *slot, SECItem *keyID)
{
    CK_OBJECT_CLASS privKey = CKO_PRIVATE_KEY;
    CK_ATTRIBUTE theTemplate[] = {
	{ CKA_ID, NULL, 0 },
	{ CKA_CLASS, NULL, 0 },
    };
    /* if you change the array, change the variable below as well */
    int tsize = sizeof(theTemplate)/sizeof(theTemplate[0]);
    CK_ATTRIBUTE *attrs = theTemplate;

    PK11_SETATTRS(attrs, CKA_ID, keyID->data, keyID->len ); attrs++;
    PK11_SETATTRS(attrs, CKA_CLASS, &privKey, sizeof(privKey));

    return pk11_FindObjectByTemplate(slot,theTemplate,tsize);
} 


SECKEYPrivateKey *
PK11_FindKeyByKeyID(PK11SlotInfo *slot, SECItem *keyID, void *wincx)
{
    CK_OBJECT_HANDLE keyHandle;
    SECKEYPrivateKey *privKey;

    keyHandle = pk11_FindPrivateKeyFromCertID(slot, keyID);
    if (keyHandle == CK_INVALID_HANDLE) { 
	return NULL;
    }
    privKey =  PK11_MakePrivKey(slot, nullKey, PR_TRUE, keyHandle, wincx);
    return privKey;
}

/*
 * Generate a CKA_ID from the relevant public key data. The CKA_ID is generated
 * from the pubKeyData by SHA1_Hashing it to produce a smaller CKA_ID (to make
 * smart cards happy.
 */
SECItem *
PK11_MakeIDFromPubKey(SECItem *pubKeyData) 
{
    PK11Context *context;
    SECItem *certCKA_ID;
    SECStatus rv;

    context = PK11_CreateDigestContext(SEC_OID_SHA1);
    if (context == NULL) {
	return NULL;
    }

    rv = PK11_DigestBegin(context);
    if (rv == SECSuccess) {
    	rv = PK11_DigestOp(context,pubKeyData->data,pubKeyData->len);
    }
    if (rv != SECSuccess) {
	PK11_DestroyContext(context,PR_TRUE);
	return NULL;
    }

    certCKA_ID = (SECItem *)PORT_Alloc(sizeof(SECItem));
    if (certCKA_ID == NULL) {
	PK11_DestroyContext(context,PR_TRUE);
	return NULL;
    }

    certCKA_ID->len = SHA1_LENGTH;
    certCKA_ID->data = (unsigned char*)PORT_Alloc(certCKA_ID->len);
    if (certCKA_ID->data == NULL) {
	PORT_Free(certCKA_ID);
	PK11_DestroyContext(context,PR_TRUE);
        return NULL;
    }

    rv = PK11_DigestFinal(context,certCKA_ID->data,&certCKA_ID->len,
								SHA1_LENGTH);
    PK11_DestroyContext(context,PR_TRUE);
    if (rv != SECSuccess) {
    	SECITEM_FreeItem(certCKA_ID,PR_TRUE);
	return NULL;
    }

    return certCKA_ID;
}

SECItem *
PK11_GetKeyIDFromPrivateKey(SECKEYPrivateKey *key, void *wincx)
{
    CK_ATTRIBUTE theTemplate[] = {
	{ CKA_ID, NULL, 0 },
    };
    int tsize = sizeof(theTemplate)/sizeof(theTemplate[0]);
    SECItem *item = NULL;
    CK_RV crv;

    crv = PK11_GetAttributes(NULL,key->pkcs11Slot,key->pkcs11ID,
							theTemplate,tsize);
    if (crv != CKR_OK) {
	PORT_SetError( PK11_MapError(crv) );
	goto loser;
    }

    item = PORT_ZNew(SECItem);
    if (item) {
        item->data = (unsigned char*) theTemplate[0].pValue;
        item->len = theTemplate[0].ulValueLen;
    }

loser:
    return item;
}

SECItem *
PK11_GetLowLevelKeyIDForPrivateKey(SECKEYPrivateKey *privKey)
{
    return pk11_GetLowLevelKeyFromHandle(privKey->pkcs11Slot,privKey->pkcs11ID);
}

static SECStatus
privateKeyListCallback(SECKEYPrivateKey *key, void *arg)
{
    SECKEYPrivateKeyList *list = (SECKEYPrivateKeyList*)arg;
    return SECKEY_AddPrivateKeyToListTail(list, SECKEY_CopyPrivateKey(key));
}

SECKEYPrivateKeyList*
PK11_ListPrivateKeysInSlot(PK11SlotInfo *slot)
{
    SECStatus status;
    SECKEYPrivateKeyList *keys;

    keys = SECKEY_NewPrivateKeyList();
    if(keys == NULL) return NULL;

    status = PK11_TraversePrivateKeysInSlot(slot, privateKeyListCallback,
		(void*)keys);

    if( status != SECSuccess ) {
	SECKEY_DestroyPrivateKeyList(keys);
	keys = NULL;
    }

    return keys;
}

SECKEYPublicKeyList*
PK11_ListPublicKeysInSlot(PK11SlotInfo *slot, char *nickname)
{
    CK_ATTRIBUTE findTemp[4];
    CK_ATTRIBUTE *attrs;
    CK_BBOOL ckTrue = CK_TRUE;
    CK_OBJECT_CLASS keyclass = CKO_PUBLIC_KEY;
    int tsize = 0;
    int objCount = 0;
    CK_OBJECT_HANDLE *key_ids;
    SECKEYPublicKeyList *keys;
    int i,len;


    attrs = findTemp;
    PK11_SETATTRS(attrs, CKA_CLASS, &keyclass, sizeof(keyclass)); attrs++;
    PK11_SETATTRS(attrs, CKA_TOKEN, &ckTrue, sizeof(ckTrue)); attrs++;
    if (nickname) {
	len = PORT_Strlen(nickname)-1;
	PK11_SETATTRS(attrs, CKA_LABEL, nickname, len); attrs++;
    }
    tsize = attrs - findTemp;
    PORT_Assert(tsize <= sizeof(findTemp)/sizeof(CK_ATTRIBUTE));

    key_ids = pk11_FindObjectsByTemplate(slot,findTemp,tsize,&objCount);
    if (key_ids == NULL) {
	return NULL;
    }
    keys = SECKEY_NewPublicKeyList();
    if (keys == NULL) {
	PORT_Free(key_ids);
    }

    for (i=0; i < objCount ; i++) {
	SECKEYPublicKey *pubKey = 
				PK11_ExtractPublicKey(slot,nullKey,key_ids[i]);
	if (pubKey) {
	    SECKEY_AddPublicKeyToListTail(keys, pubKey);
	}
   }

   PORT_Free(key_ids);
   return keys;
}

SECKEYPrivateKeyList*
PK11_ListPrivKeysInSlot(PK11SlotInfo *slot, char *nickname, void *wincx)
{
    CK_ATTRIBUTE findTemp[4];
    CK_ATTRIBUTE *attrs;
    CK_BBOOL ckTrue = CK_TRUE;
    CK_OBJECT_CLASS keyclass = CKO_PRIVATE_KEY;
    int tsize = 0;
    int objCount = 0;
    CK_OBJECT_HANDLE *key_ids;
    SECKEYPrivateKeyList *keys;
    int i,len;


    attrs = findTemp;
    PK11_SETATTRS(attrs, CKA_CLASS, &keyclass, sizeof(keyclass)); attrs++;
    PK11_SETATTRS(attrs, CKA_TOKEN, &ckTrue, sizeof(ckTrue)); attrs++;
    if (nickname) {
	len = PORT_Strlen(nickname)-1;
	PK11_SETATTRS(attrs, CKA_LABEL, nickname, len); attrs++;
    }
    tsize = attrs - findTemp;
    PORT_Assert(tsize <= sizeof(findTemp)/sizeof(CK_ATTRIBUTE));

    key_ids = pk11_FindObjectsByTemplate(slot,findTemp,tsize,&objCount);
    if (key_ids == NULL) {
	return NULL;
    }
    keys = SECKEY_NewPrivateKeyList();
    if (keys == NULL) {
	PORT_Free(key_ids);
    }

    for (i=0; i < objCount ; i++) {
	SECKEYPrivateKey *privKey = 
		PK11_MakePrivKey(slot,nullKey,PR_TRUE,key_ids[i],wincx);
	SECKEY_AddPrivateKeyToListTail(keys, privKey);
   }

   PORT_Free(key_ids);
   return keys;
}

