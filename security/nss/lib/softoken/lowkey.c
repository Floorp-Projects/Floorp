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
#include "lowkeyi.h"
#include "secoid.h"
#include "secitem.h"
#include "secder.h"
#include "base64.h"
#include "secasn1.h"
#include "pcert.h"
#include "secerr.h"


const SEC_ASN1Template nsslowkey_PQGParamsTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(PQGParams) },
    { SEC_ASN1_INTEGER, offsetof(PQGParams,prime) },
    { SEC_ASN1_INTEGER, offsetof(PQGParams,subPrime) },
    { SEC_ASN1_INTEGER, offsetof(PQGParams,base) },
    { 0, }
};

const SEC_ASN1Template nsslowkey_RSAPrivateKeyTemplate[] = {
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


const SEC_ASN1Template nsslowkey_DSAPrivateKeyTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSSLOWKEYPrivateKey) },
    { SEC_ASN1_INTEGER, offsetof(NSSLOWKEYPrivateKey,u.dsa.publicValue) },
    { SEC_ASN1_INTEGER, offsetof(NSSLOWKEYPrivateKey,u.dsa.privateValue) },
    { 0, }
};

const SEC_ASN1Template nsslowkey_DSAPrivateKeyExportTemplate[] = {
    { SEC_ASN1_INTEGER, offsetof(NSSLOWKEYPrivateKey,u.dsa.privateValue) },
};

const SEC_ASN1Template nsslowkey_DHPrivateKeyTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(NSSLOWKEYPrivateKey) },
    { SEC_ASN1_INTEGER, offsetof(NSSLOWKEYPrivateKey,u.dh.publicValue) },
    { SEC_ASN1_INTEGER, offsetof(NSSLOWKEYPrivateKey,u.dh.privateValue) },
    { SEC_ASN1_INTEGER, offsetof(NSSLOWKEYPrivateKey,u.dh.base) },
    { SEC_ASN1_INTEGER, offsetof(NSSLOWKEYPrivateKey,u.dh.prime) },
    { 0, }
};

void
nsslowkey_DestroyPrivateKey(NSSLOWKEYPrivateKey *privk)
{
    if (privk && privk->arena) {
	PORT_FreeArena(privk->arena, PR_TRUE);
    }
}

void
nsslowkey_DestroyPublicKey(NSSLOWKEYPublicKey *pubk)
{
    if (pubk && pubk->arena) {
	PORT_FreeArena(pubk->arena, PR_FALSE);
    }
}
unsigned
nsslowkey_PublicModulusLen(NSSLOWKEYPublicKey *pubk)
{
    unsigned char b0;

    /* interpret modulus length as key strength... in
     * fortezza that's the public key length */

    switch (pubk->keyType) {
    case NSSLOWKEYRSAKey:
    	b0 = pubk->u.rsa.modulus.data[0];
    	return b0 ? pubk->u.rsa.modulus.len : pubk->u.rsa.modulus.len - 1;
    default:
	break;
    }
    return 0;
}

unsigned
nsslowkey_PrivateModulusLen(NSSLOWKEYPrivateKey *privk)
{

    unsigned char b0;

    switch (privk->keyType) {
    case NSSLOWKEYRSAKey:
	b0 = privk->u.rsa.modulus.data[0];
	return b0 ? privk->u.rsa.modulus.len : privk->u.rsa.modulus.len - 1;
    default:
	break;
    }
    return 0;
}

NSSLOWKEYPublicKey *
nsslowkey_ConvertToPublicKey(NSSLOWKEYPrivateKey *privk)
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
	    nsslowkey_DestroyPublicKey (pubk);
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
	/* No Fortezza in Low Key implementations (Fortezza keys aren't
	 * stored in our data base */
    default:
	break;
    }

    PORT_FreeArena (arena, PR_FALSE);
    return NULL;
}

NSSLOWKEYPrivateKey *
nsslowkey_CopyPrivateKey(NSSLOWKEYPrivateKey *privKey)
{
    NSSLOWKEYPrivateKey *returnKey = NULL;
    SECStatus rv = SECFailure;
    PLArenaPool *poolp;

    if(!privKey) {
	return NULL;
    }

    poolp = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if(!poolp) {
	return NULL;
    }

    returnKey = (NSSLOWKEYPrivateKey*)PORT_ArenaZAlloc(poolp, sizeof(NSSLOWKEYPrivateKey));
    if(!returnKey) {
	rv = SECFailure;
	goto loser;
    }

    returnKey->keyType = privKey->keyType;
    returnKey->arena = poolp;

    switch(privKey->keyType) {
	case NSSLOWKEYRSAKey:
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
	case NSSLOWKEYDSAKey:
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
	case NSSLOWKEYDHKey:
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
