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
#include "keylow.h"
#include "secoid.h"
#include "secitem.h"
#include "secder.h"
#include "base64.h"
#include "secasn1.h"
#include "cert.h"
#include "secerr.h"


const SEC_ASN1Template SECKEY_RSAPrivateKeyTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(SECKEYPrivateKey) },
    { SEC_ASN1_INTEGER, offsetof(SECKEYLowPrivateKey,u.rsa.version) },
    { SEC_ASN1_INTEGER, offsetof(SECKEYLowPrivateKey,u.rsa.modulus) },
    { SEC_ASN1_INTEGER, offsetof(SECKEYLowPrivateKey,u.rsa.publicExponent) },
    { SEC_ASN1_INTEGER, offsetof(SECKEYLowPrivateKey,u.rsa.privateExponent) },
    { SEC_ASN1_INTEGER, offsetof(SECKEYLowPrivateKey,u.rsa.prime1) },
    { SEC_ASN1_INTEGER, offsetof(SECKEYLowPrivateKey,u.rsa.prime2) },
    { SEC_ASN1_INTEGER, offsetof(SECKEYLowPrivateKey,u.rsa.exponent1) },
    { SEC_ASN1_INTEGER, offsetof(SECKEYLowPrivateKey,u.rsa.exponent2) },
    { SEC_ASN1_INTEGER, offsetof(SECKEYLowPrivateKey,u.rsa.coefficient) },
    { 0 }                                                                     
};                                                                            


const SEC_ASN1Template SECKEY_DSAPrivateKeyTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(SECKEYLowPrivateKey) },
    { SEC_ASN1_INTEGER, offsetof(SECKEYLowPrivateKey,u.dsa.publicValue) },
    { SEC_ASN1_INTEGER, offsetof(SECKEYLowPrivateKey,u.dsa.privateValue) },
    { 0, }
};

const SEC_ASN1Template SECKEY_DSAPrivateKeyExportTemplate[] = {
    { SEC_ASN1_INTEGER, offsetof(SECKEYLowPrivateKey,u.dsa.privateValue) },
};

const SEC_ASN1Template SECKEY_DHPrivateKeyTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(SECKEYLowPrivateKey) },
    { SEC_ASN1_INTEGER, offsetof(SECKEYLowPrivateKey,u.dh.publicValue) },
    { SEC_ASN1_INTEGER, offsetof(SECKEYLowPrivateKey,u.dh.privateValue) },
    { SEC_ASN1_INTEGER, offsetof(SECKEYLowPrivateKey,u.dh.base) },
    { SEC_ASN1_INTEGER, offsetof(SECKEYLowPrivateKey,u.dh.prime) },
    { 0, }
};

const SEC_ASN1Template SECKEY_DHPrivateKeyExportTemplate[] = {
    { SEC_ASN1_INTEGER, offsetof(SECKEYLowPrivateKey,u.dh.privateValue) },
    { SEC_ASN1_INTEGER, offsetof(SECKEYLowPrivateKey,u.dh.base) },
    { SEC_ASN1_INTEGER, offsetof(SECKEYLowPrivateKey,u.dh.prime) },
};

void
SECKEY_LowDestroyPrivateKey(SECKEYLowPrivateKey *privk)
{
    if (privk && privk->arena) {
	PORT_FreeArena(privk->arena, PR_TRUE);
    }
}

void
SECKEY_LowDestroyPublicKey(SECKEYLowPublicKey *pubk)
{
    if (pubk && pubk->arena) {
	PORT_FreeArena(pubk->arena, PR_FALSE);
    }
}
unsigned
SECKEY_LowPublicModulusLen(SECKEYLowPublicKey *pubk)
{
    unsigned char b0;

    /* interpret modulus length as key strength... in
     * fortezza that's the public key length */

    switch (pubk->keyType) {
    case rsaKey:
    	b0 = pubk->u.rsa.modulus.data[0];
    	return b0 ? pubk->u.rsa.modulus.len : pubk->u.rsa.modulus.len - 1;
    default:
	break;
    }
    return 0;
}

unsigned
SECKEY_LowPrivateModulusLen(SECKEYLowPrivateKey *privk)
{

    unsigned char b0;

    switch (privk->keyType) {
    case rsaKey:
	b0 = privk->u.rsa.modulus.data[0];
	return b0 ? privk->u.rsa.modulus.len : privk->u.rsa.modulus.len - 1;
    default:
	break;
    }
    return 0;
}

SECKEYLowPublicKey *
SECKEY_LowConvertToPublicKey(SECKEYLowPrivateKey *privk)
{
    SECKEYLowPublicKey *pubk;
    PLArenaPool *arena;


    arena = PORT_NewArena (DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL) {
        PORT_SetError (SEC_ERROR_NO_MEMORY);
        return NULL;
    }

    switch(privk->keyType) {
      case rsaKey:
      case nullKey:
	pubk = (SECKEYLowPublicKey *)PORT_ArenaZAlloc(arena,
						sizeof (SECKEYLowPublicKey));
	if (pubk != NULL) {
	    SECStatus rv;

	    pubk->arena = arena;
	    pubk->keyType = privk->keyType;
	    if (privk->keyType == nullKey) return pubk;
	    rv = SECITEM_CopyItem(arena, &pubk->u.rsa.modulus,
				  &privk->u.rsa.modulus);
	    if (rv == SECSuccess) {
		rv = SECITEM_CopyItem (arena, &pubk->u.rsa.publicExponent,
				       &privk->u.rsa.publicExponent);
		if (rv == SECSuccess)
		    return pubk;
	    }
	    SECKEY_LowDestroyPublicKey (pubk);
	} else {
	    PORT_SetError (SEC_ERROR_NO_MEMORY);
	}
	break;
      case dsaKey:
	pubk = (SECKEYLowPublicKey *)PORT_ArenaZAlloc(arena,
						    sizeof(SECKEYLowPublicKey));
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
      case dhKey:
	pubk = (SECKEYLowPublicKey *)PORT_ArenaZAlloc(arena,
						    sizeof(SECKEYLowPublicKey));
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
	/* No Fortezza in Low Key implementations (Fortezza keys aren't
	 * stored in our data base */
    default:
	break;
    }

    PORT_FreeArena (arena, PR_FALSE);
    return NULL;
}

SECKEYLowPrivateKey *
SECKEY_CopyLowPrivateKey(SECKEYLowPrivateKey *privKey)
{
    SECKEYLowPrivateKey *returnKey = NULL;
    SECStatus rv = SECFailure;
    PLArenaPool *poolp;

    if(!privKey) {
	return NULL;
    }

    poolp = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if(!poolp) {
	return NULL;
    }

    returnKey = (SECKEYLowPrivateKey*)PORT_ArenaZAlloc(poolp, sizeof(SECKEYLowPrivateKey));
    if(!returnKey) {
	rv = SECFailure;
	goto loser;
    }

    returnKey->keyType = privKey->keyType;
    returnKey->arena = poolp;

    switch(privKey->keyType) {
	case rsaKey:
	    rv = SECITEM_CopyItem(poolp, &(returnKey->u.rsa.modulus), 
	    				&(privKey->u.rsa.modulus));
	    if(rv != SECSuccess) break;
	    rv = SECITEM_CopyItem(poolp, &(returnKey->u.rsa.version), 
	    				&(privKey->u.rsa.version));
	    if(rv != SECSuccess) break;
	    rv = SECITEM_CopyItem(poolp, &(returnKey->u.rsa.publicExponent), 
	    				&(privKey->u.rsa.publicExponent));
	    if(rv != SECSuccess) break;
	    rv = SECITEM_CopyItem(poolp, &(returnKey->u.rsa.privateExponent), 
	    				&(privKey->u.rsa.privateExponent));
	    if(rv != SECSuccess) break;
	    rv = SECITEM_CopyItem(poolp, &(returnKey->u.rsa.prime1), 
	    				&(privKey->u.rsa.prime1));
	    if(rv != SECSuccess) break;
	    rv = SECITEM_CopyItem(poolp, &(returnKey->u.rsa.prime2), 
	    				&(privKey->u.rsa.prime2));
	    if(rv != SECSuccess) break;
	    rv = SECITEM_CopyItem(poolp, &(returnKey->u.rsa.exponent1), 
	    				&(privKey->u.rsa.exponent1));
	    if(rv != SECSuccess) break;
	    rv = SECITEM_CopyItem(poolp, &(returnKey->u.rsa.exponent2), 
	    				&(privKey->u.rsa.exponent2));
	    if(rv != SECSuccess) break;
	    rv = SECITEM_CopyItem(poolp, &(returnKey->u.rsa.coefficient), 
	    				&(privKey->u.rsa.coefficient));
	    if(rv != SECSuccess) break;
	    break;
	case dsaKey:
	    rv = SECITEM_CopyItem(poolp, &(returnKey->u.dsa.publicValue),
	    				&(privKey->u.dsa.publicValue));
	    if(rv != SECSuccess) break;
	    rv = SECITEM_CopyItem(poolp, &(returnKey->u.dsa.privateValue),
	    				&(privKey->u.dsa.privateValue));
	    if(rv != SECSuccess) break;
	    returnKey->u.dsa.params.arena = poolp;
	    rv = SECITEM_CopyItem(poolp, &(returnKey->u.dsa.params.prime),
					&(privKey->u.dsa.params.prime));
	    if(rv != SECSuccess) break;
	    rv = SECITEM_CopyItem(poolp, &(returnKey->u.dsa.params.subPrime),
					&(privKey->u.dsa.params.subPrime));
	    if(rv != SECSuccess) break;
	    rv = SECITEM_CopyItem(poolp, &(returnKey->u.dsa.params.base),
					&(privKey->u.dsa.params.base));
	    if(rv != SECSuccess) break;
	    break;
	case dhKey:
	    rv = SECITEM_CopyItem(poolp, &(returnKey->u.dh.publicValue),
	    				&(privKey->u.dh.publicValue));
	    if(rv != SECSuccess) break;
	    rv = SECITEM_CopyItem(poolp, &(returnKey->u.dh.privateValue),
	    				&(privKey->u.dh.privateValue));
	    if(rv != SECSuccess) break;
	    returnKey->u.dsa.params.arena = poolp;
	    rv = SECITEM_CopyItem(poolp, &(returnKey->u.dh.prime),
					&(privKey->u.dh.prime));
	    if(rv != SECSuccess) break;
	    rv = SECITEM_CopyItem(poolp, &(returnKey->u.dh.base),
					&(privKey->u.dh.base));
	    if(rv != SECSuccess) break;
	    break;
	case fortezzaKey:
	    returnKey->u.fortezza.certificate = 
	    				privKey->u.fortezza.certificate;
	    returnKey->u.fortezza.socket = 
	    				privKey->u.fortezza.socket;
	    PORT_Memcpy(returnKey->u.fortezza.serial, 
	    				privKey->u.fortezza.serial, 8);
	    rv = SECSuccess;
	    break;
	default:
	    rv = SECFailure;
    }

loser:

    if(rv != SECSuccess) {
	PORT_FreeArena(poolp, PR_TRUE);
	returnKey = NULL;
    }

    return returnKey;
}
