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
 *	Dr Stephen Henson <stephen.henson@gemplus.com>
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
 * This file implements PKCS 11 on top of our existing security modules
 *
 * For more information about PKCS 11 See PKCS 11 Token Inteface Standard.
 *   This implementation has two slots:
 *	slot 1 is our generic crypto support. It does not require login.
 *   It supports Public Key ops, and all they bulk ciphers and hashes. 
 *   It can also support Private Key ops for imported Private keys. It does 
 *   not have any token storage.
 *	slot 2 is our private key support. It requires a login before use. It
 *   can store Private Keys and Certs as token objects. Currently only private
 *   keys and their associated Certificates are saved on the token.
 *
 *   In this implementation, session objects are only visible to the session
 *   that created or generated them.
 */
#include "seccomon.h"
#include "secitem.h"
#include "secport.h"
#include "blapi.h"
#include "pkcs11.h"
#include "pkcs11i.h"
#include "keylow.h"
#include "cert.h"
#include "sechash.h"
#include "secder.h"
#include "secdig.h"
#include "secpkcs5.h"	/* We do PBE below */
#include "pkcs11t.h"
#include "secoid.h"
#include "alghmac.h"
#include "softoken.h"
#include "secasn1.h"
#include "secmodi.h"

#include "certdb.h"
#include "ssl3prot.h" 	/* for SSL3_RANDOM_LENGTH */

#define __PASTE(x,y)    x##y

/*
 * we renamed all our internal functions, get the correct
 * definitions for them...
 */ 
#undef CK_PKCS11_FUNCTION_INFO
#undef CK_NEED_ARG_LIST

#define CK_EXTERN extern
#define CK_PKCS11_FUNCTION_INFO(func) \
		CK_RV __PASTE(NS,func)
#define CK_NEED_ARG_LIST	1
 
#include "pkcs11f.h"


/* forward static declaration. */
static SECStatus pk11_PRF(const SECItem *secret, const char *label, 
                          SECItem *seed, SECItem *result);  

#define PK11_OFFSETOF(str, memb) ((PRPtrdiff)(&(((str *)0)->memb)))

static void pk11_Null(void *data, PRBool freeit)
{
    return;
} 

/*
 * free routines.... Free local type  allocated data, and convert
 * other free routines to the destroy signature.
 */
static void
pk11_FreePrivKey(SECKEYLowPrivateKey *key, PRBool freeit)
{
    SECKEY_LowDestroyPrivateKey(key);
}

static void
pk11_HMAC_Destroy(HMACContext *context, PRBool freeit)
{
    HMAC_Destroy(context);
}

static void
pk11_Space(void *data, PRBool freeit)
{
    PORT_Free(data);
} 

static void pk11_FreeSignInfo(PK11HashSignInfo *data, PRBool freeit)
{
    SECKEY_LowDestroyPrivateKey(data->key);
    PORT_Free(data);
} 

static DSAPublicKey *
DSA_CreateVerifyContext(SECKEYLowPublicKey *pubKey)
{
    PLArenaPool * arena;
    DSAPublicKey * dsaKey;
    SECStatus      rv;

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (!arena) goto loser; 
    dsaKey = (DSAPublicKey *)PORT_ArenaZAlloc(arena, sizeof (DSAPublicKey));
    if (!dsaKey) goto loser;
    dsaKey->params.arena = arena;

#define COPY_DSA_ITEM(item) \
    rv = SECITEM_CopyItem(arena, &dsaKey->item, &pubKey->u.dsa.item); \
    if (rv != SECSuccess) goto loser;

    COPY_DSA_ITEM(params.prime);
    COPY_DSA_ITEM(params.subPrime);
    COPY_DSA_ITEM(params.base);
    COPY_DSA_ITEM(publicValue);
    return dsaKey;

loser:
    if (arena)
	PORT_FreeArena(arena, PR_TRUE);
    return NULL;

#undef COPY_DSA_ITEM
}

static void 
DSA_DestroyVerifyContext(DSAPublicKey * key)
{
    if (key && key->params.arena)
	PORT_FreeArena(key->params.arena, PR_TRUE);
}

static DSAPrivateKey *
DSA_CreateSignContext(SECKEYLowPrivateKey *privKey)
{
    PLArenaPool *  arena;
    DSAPrivateKey * dsaKey;
    SECStatus       rv;

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (!arena) goto loser; 
    dsaKey = (DSAPrivateKey *)PORT_ArenaZAlloc(arena, sizeof (DSAPrivateKey));
    if (!dsaKey) goto loser;
    dsaKey->params.arena = arena;

#define COPY_DSA_ITEM(item) \
    rv = SECITEM_CopyItem(arena, &dsaKey->item, &privKey->u.dsa.item); \
    if (rv != SECSuccess) goto loser;

    COPY_DSA_ITEM(params.prime);
    COPY_DSA_ITEM(params.subPrime);
    COPY_DSA_ITEM(params.base);
    COPY_DSA_ITEM(publicValue);
    COPY_DSA_ITEM(privateValue);
    return dsaKey;

loser:
    if (arena)
	PORT_FreeArena(arena, PR_TRUE);
    return NULL;

#undef COPY_DSA_ITEM
}

static void 
DSA_DestroySignContext(DSAPrivateKey * key)
{
    if (key && key->params.arena)
	PORT_FreeArena(key->params.arena, PR_TRUE);
}

/*
 * turn a CDMF key into a des key. CDMF is an old IBM scheme to export DES by
 * Deprecating a full des key to 40 bit key strenth.
 */
static CK_RV
pk11_cdmf2des(unsigned char *cdmfkey, unsigned char *deskey)
{
    unsigned char key1[8] = { 0xc4, 0x08, 0xb0, 0x54, 0x0b, 0xa1, 0xe0, 0xae };
    unsigned char key2[8] = { 0xef, 0x2c, 0x04, 0x1c, 0xe6, 0x38, 0x2f, 0xe6 };
    unsigned char enc_src[8];
    unsigned char enc_dest[8];
    unsigned int leng,i;
    DESContext *descx;
    SECStatus rv;
    
    
    /* zero the parity bits */
    for (i=0; i < 8; i++) {
	enc_src[i] = cdmfkey[i] & 0xfe;
    }

    /* encrypt with key 1 */
    descx = DES_CreateContext(key1, NULL, NSS_DES, PR_TRUE);
    if (descx == NULL) return CKR_HOST_MEMORY;
    rv = DES_Encrypt(descx, enc_dest, &leng, 8, enc_src, 8);
    DES_DestroyContext(descx,PR_TRUE);
    if (rv != SECSuccess) return CKR_DEVICE_ERROR;

    /* xor source with des, zero the parity bits and depricate the key*/
    for (i=0; i < 8; i++) {
	if (i & 1) {
	    enc_src[i] = (enc_src[i] ^ enc_dest[i]) & 0xfe;
	} else {
	    enc_src[i] = (enc_src[i] ^ enc_dest[i]) & 0x0e;
	}
    }

    /* encrypt with key 2 */
    descx = DES_CreateContext(key2, NULL, NSS_DES, PR_TRUE);
    if (descx == NULL) return CKR_HOST_MEMORY;
    rv = DES_Encrypt(descx, deskey, &leng, 8, enc_src, 8);
    DES_DestroyContext(descx,PR_TRUE);
    if (rv != SECSuccess) return CKR_DEVICE_ERROR;

    /* set the corret parity on our new des key */	
    pk11_FormatDESKey(deskey, 8);
    return CKR_OK;
}


static CK_RV
pk11_EncryptInit(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR pMechanism,
		 CK_OBJECT_HANDLE hKey, CK_ATTRIBUTE_TYPE etype,
		 PK11ContextType contextType);
/*
 * Calculate a Lynx checksum for CKM_LYNX_WRAP mechanism.
 */
static CK_RV
pk11_calcLynxChecksum(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE hWrapKey,
	unsigned char *checksum, unsigned char *key, CK_ULONG len)
{

    CK_BYTE E[10];
    CK_ULONG Elen = sizeof (E);
    CK_BYTE C[8];
    CK_ULONG Clen = sizeof (C);
    unsigned short sum1 = 0, sum2 = 0;
    CK_MECHANISM mech = { CKM_DES_ECB, NULL, 0 };
    int i;
    CK_RV crv;

    if (len != 8) return CKR_WRAPPED_KEY_LEN_RANGE;
    
    /* zero the parity bits */
    for (i=0; i < 8; i++) {
	sum1 = sum1 + key[i];
	sum2 = sum2 + sum1;
    }

    /* encrypt with key 1 */

    crv = pk11_EncryptInit(hSession,&mech,hWrapKey,CKA_WRAP, PK11_ENCRYPT);
    if (crv != CKR_OK) return crv;

    crv = NSC_Encrypt(hSession,key,len,E,&Elen);
    if (crv != CKR_OK) return crv;

    E[8] = (sum2 >> 8) & 0xff;
    E[9] = sum2 & 0xff;

    crv = pk11_EncryptInit(hSession,&mech,hWrapKey,CKA_WRAP, PK11_ENCRYPT);
    if (crv != CKR_OK) return crv;

    crv = NSC_Encrypt(hSession,&E[2],len,C,&Clen);
    if (crv != CKR_OK) return crv;

    checksum[0] = C[6];
    checksum[1] = C[7];
    
    return CKR_OK;
}
/* NSC_DestroyObject destroys an object. */
CK_RV
NSC_DestroyObject(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE hObject)
{
    PK11Slot *slot = pk11_SlotFromSessionHandle(hSession);
    PK11Session *session;
    PK11Object *object;
    PK11FreeStatus status;

    /*
     * This whole block just makes sure we really can destroy the
     * requested object.
     */
    session = pk11_SessionFromHandle(hSession);
    if (session == NULL) {
        return CKR_SESSION_HANDLE_INVALID;
    }

    object = pk11_ObjectFromHandle(hObject,session);
    if (object == NULL) {
	pk11_FreeSession(session);
	return CKR_OBJECT_HANDLE_INVALID;
    }

    /* don't destroy a private object if we aren't logged in */
    if ((!slot->isLoggedIn) && (slot->needLogin) &&
				(pk11_isTrue(object,CKA_PRIVATE))) {
	pk11_FreeSession(session);
	pk11_FreeObject(object);
	return CKR_USER_NOT_LOGGED_IN;
    }

    /* don't destroy a token object if we aren't in a rw session */

    if (((session->info.flags & CKF_RW_SESSION) == 0) &&
				(pk11_isTrue(object,CKA_TOKEN))) {
	pk11_FreeSession(session);
	pk11_FreeObject(object);
	return CKR_SESSION_READ_ONLY;
    }

    pk11_DeleteObject(session,object);

    pk11_FreeSession(session);

    /*
     * get some indication if the object is destroyed. Note: this is not
     * 100%. Someone may have an object reference outstanding (though that
     * should not be the case by here. Also note that the object is "half"
     * destroyed. Our internal representation is destroyed, but it may still
     * be in the data base.
     */
    status = pk11_FreeObject(object);

    return (status != PK11_DestroyFailure) ? CKR_OK : CKR_DEVICE_ERROR;
}


/*
 ************** Crypto Functions:     Utilities ************************
 */


/* 
 * return a context based on the PK11Context type.
 */
PK11SessionContext *
pk11_ReturnContextByType(PK11Session *session, PK11ContextType type)
{
    switch (type) {
	case PK11_ENCRYPT:
	case PK11_DECRYPT:
	    return session->enc_context;
	case PK11_HASH:
	    return session->hash_context;
	case PK11_SIGN:
	case PK11_SIGN_RECOVER:
	case PK11_VERIFY:
	case PK11_VERIFY_RECOVER:
	    return session->hash_context;
    }
    return NULL;
}

/* 
 * change a context based on the PK11Context type.
 */
void
pk11_SetContextByType(PK11Session *session, PK11ContextType type, 
						PK11SessionContext *context)
{
    switch (type) {
	case PK11_ENCRYPT:
	case PK11_DECRYPT:
	    session->enc_context = context;
	    break;
	case PK11_HASH:
	    session->hash_context = context;
	    break;
	case PK11_SIGN:
	case PK11_SIGN_RECOVER:
	case PK11_VERIFY:
	case PK11_VERIFY_RECOVER:
	    session->hash_context = context;
	    break;
    }
    return;
}

/*
 * code to grab the context. Needed by every C_XXXUpdate, C_XXXFinal,
 * and C_XXX function. The function takes a session handle, the context type,
 * and wether or not the session needs to be multipart. It returns the context,
 * and optionally returns the session pointer (if sessionPtr != NULL) if session
 * pointer is returned, the caller is responsible for freeing it.
 */
static CK_RV
pk11_GetContext(CK_SESSION_HANDLE handle,PK11SessionContext **contextPtr,
	PK11ContextType type, PRBool needMulti, PK11Session **sessionPtr)
{
    PK11Session *session;
    PK11SessionContext *context;

    session = pk11_SessionFromHandle(handle);
    if (session == NULL) return CKR_SESSION_HANDLE_INVALID;
    context = pk11_ReturnContextByType(session,type);
    /* make sure the context is valid */
    if((context==NULL)||(context->type!=type)||(needMulti&&!(context->multi))){
        pk11_FreeSession(session);
	return CKR_OPERATION_NOT_INITIALIZED;
    }
    *contextPtr = context;
    if (sessionPtr != NULL) {
	*sessionPtr = session;
    } else {
	pk11_FreeSession(session);
    }
    return CKR_OK;
}

/*
 ************** Crypto Functions:     Encrypt ************************
 */

/*
 * All the NSC_InitXXX functions have a set of common checks and processing they
 * all need to do at the beginning. This is done here.
 */
static CK_RV
pk11_InitGeneric(PK11Session *session,PK11SessionContext **contextPtr,
		 PK11ContextType ctype,PK11Object **keyPtr,
		 CK_OBJECT_HANDLE hKey, CK_KEY_TYPE *keyTypePtr,
		 CK_OBJECT_CLASS pubKeyType, CK_ATTRIBUTE_TYPE operation)
{
    PK11Object *key = NULL;
    PK11Attribute *att;
    PK11SessionContext *context;

    /* We can only init if there is not current context active */
    if (pk11_ReturnContextByType(session,ctype) != NULL) {
	return CKR_OPERATION_ACTIVE;
    }

    /* find the key */
    if (keyPtr) {
        key = pk11_ObjectFromHandle(hKey,session);
        if (key == NULL) {
	    return CKR_KEY_HANDLE_INVALID;
    	}

	/* make sure it's a valid  key for this operation */
	if (((key->objclass != CKO_SECRET_KEY) && (key->objclass != pubKeyType))
					|| !pk11_isTrue(key,operation)) {
	    pk11_FreeObject(key);
	    return CKR_KEY_TYPE_INCONSISTENT;
	}
	/* get the key type */
	att = pk11_FindAttribute(key,CKA_KEY_TYPE);
	PORT_Assert(att != NULL);
	*keyTypePtr = *(CK_KEY_TYPE *)att->attrib.pValue;
	pk11_FreeAttribute(att);
	*keyPtr = key;
    }

    /* allocate the context structure */
    context = (PK11SessionContext *)PORT_Alloc(sizeof(PK11SessionContext));
    if (context == NULL) {
	if (key) pk11_FreeObject(key);
	return CKR_HOST_MEMORY;
    }
    context->type = ctype;
    context->multi = PR_TRUE;
    context->cipherInfo = NULL;
    context->hashInfo = NULL;
    context->doPad = PR_FALSE;
    context->padDataLength = 0;

    *contextPtr = context;
    return CKR_OK;
}

/* NSC_EncryptInit initializes an encryption operation. */
/* This function is used by NSC_EncryptInit and NSC_WrapKey. The only difference
 * in their uses if whether or not etype is CKA_ENCRYPT or CKA_WRAP */
static CK_RV
pk11_EncryptInit(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR pMechanism,
		 CK_OBJECT_HANDLE hKey, CK_ATTRIBUTE_TYPE etype,
		 PK11ContextType contextType)
{
    PK11Session *session;
    PK11Object *key;
    PK11SessionContext *context;
    PK11Attribute *att;
    CK_RC2_CBC_PARAMS *rc2_param;
#if NSS_SOFTOKEN_DOES_RC5
    CK_RC5_CBC_PARAMS *rc5_param;
    SECItem rc5Key;
#endif
    CK_KEY_TYPE key_type;
    CK_RV crv = CKR_OK;
    SECKEYLowPublicKey *pubKey;
    unsigned effectiveKeyLength;
    unsigned char newdeskey[8];
    PRBool useNewKey=PR_FALSE;
    int t;

    session = pk11_SessionFromHandle(hSession);
    if (session == NULL) return CKR_SESSION_HANDLE_INVALID;

    crv = pk11_InitGeneric(session,&context,contextType,&key,hKey,&key_type,
    						CKO_PUBLIC_KEY, etype);
						
    if (crv != CKR_OK) {
	pk11_FreeSession(session);
	return crv;
    }

    context->doPad = PR_FALSE;
    switch(pMechanism->mechanism) {
    case CKM_RSA_PKCS:
    case CKM_RSA_X_509:
	if (key_type != CKK_RSA) {
	    crv = CKR_KEY_TYPE_INCONSISTENT;
	    break;
	}
	context->multi = PR_FALSE;
	pubKey = pk11_GetPubKey(key,CKK_RSA);
	if (pubKey == NULL) {
	    crv = CKR_HOST_MEMORY;
	    break;
	}
	context->cipherInfo = pubKey;
	context->update = (PK11Cipher) (pMechanism->mechanism == CKM_RSA_X_509
			? RSA_EncryptRaw : RSA_EncryptBlock);
	context->destroy = pk11_Null;
	break;
    case CKM_RC2_CBC_PAD:
	context->doPad = PR_TRUE;
	context->blockSize = 8;
	/* fall thru */
    case CKM_RC2_ECB:
    case CKM_RC2_CBC:
	if (key_type != CKK_RC2) {
	    crv = CKR_KEY_TYPE_INCONSISTENT;
	    break;
	}
	att = pk11_FindAttribute(key,CKA_VALUE);
	if (att == NULL) {
	    crv = CKR_KEY_HANDLE_INVALID;
	    break;
	}
	rc2_param = (CK_RC2_CBC_PARAMS *)pMechanism->pParameter;
	effectiveKeyLength = (rc2_param->ulEffectiveBits+7)/8;
	context->cipherInfo = 
	    RC2_CreateContext((unsigned char*)att->attrib.pValue,
			      att->attrib.ulValueLen, rc2_param->iv,
			      pMechanism->mechanism == CKM_RC2_ECB ? NSS_RC2 :
			      NSS_RC2_CBC,effectiveKeyLength);
	pk11_FreeAttribute(att);
	if (context->cipherInfo == NULL) {
	    crv = CKR_HOST_MEMORY;
	    break;
	}
	context->update = (PK11Cipher) RC2_Encrypt;
	context->destroy = (PK11Destroy) RC2_DestroyContext;
	break;
#if NSS_SOFTOKEN_DOES_RC5
    case CKM_RC5_CBC_PAD:
	context->doPad = PR_TRUE;
	/* fall thru */
    case CKM_RC5_ECB:
    case CKM_RC5_CBC:
	if (key_type != CKK_RC5) {
	    crv = CKR_KEY_TYPE_INCONSISTENT;
	    break;
	}
	att = pk11_FindAttribute(key,CKA_VALUE);
	if (att == NULL) {
	    crv = CKR_KEY_HANDLE_INVALID;
	    break;
	}
	rc5_param = (CK_RC5_CBC_PARAMS *)pMechanism->pParameter;
	if (context->doPad) {
	   context->blockSize = rc5_param->ulWordsize*2;
	}
	rc5Key.data = (unsigned char*)att->attrib.pValue;
	rc5Key.len = att->attrib.ulValueLen;
	context->cipherInfo = RC5_CreateContext(&rc5Key,rc5_param->ulRounds,
	   rc5_param->ulWordsize,rc5_param->pIv,
		 pMechanism->mechanism == CKM_RC5_ECB ? NSS_RC5 : NSS_RC5_CBC);
	pk11_FreeAttribute(att);
	if (context->cipherInfo == NULL) {
	    crv = CKR_HOST_MEMORY;
	    break;
	}
	context->update = (PK11Cipher) RC5_Encrypt;
	context->destroy = (PK11Destroy) RC5_DestroyContext;
	break;
#endif
    case CKM_RC4:
	if (key_type != CKK_RC4) {
	    crv = CKR_KEY_TYPE_INCONSISTENT;
	    break;
	}
	att = pk11_FindAttribute(key,CKA_VALUE);
	if (att == NULL) {
	    crv = CKR_KEY_HANDLE_INVALID;
	    break;
	}
	context->cipherInfo = 
	    RC4_CreateContext((unsigned char*)att->attrib.pValue,
			      att->attrib.ulValueLen);
	pk11_FreeAttribute(att);
	if (context->cipherInfo == NULL) {
	    crv = CKR_HOST_MEMORY;  /* WRONG !!! */
	    break;
	}
	context->update = (PK11Cipher) RC4_Encrypt;
	context->destroy = (PK11Destroy) RC4_DestroyContext;
	break;
    case CKM_CDMF_CBC_PAD:
	context->doPad = PR_TRUE;
	context->blockSize = 8;
	/* fall thru */
    case CKM_CDMF_ECB:
    case CKM_CDMF_CBC:
	if (key_type != CKK_CDMF) {
	    crv = CKR_KEY_TYPE_INCONSISTENT;
	    break;
	}
	t = (pMechanism->mechanism == CKM_CDMF_ECB) ? NSS_DES : NSS_DES_CBC;
	useNewKey=PR_TRUE;
	if (crv != CKR_OK) break;
	goto finish_des;
    case CKM_DES_ECB:
	if (key_type != CKK_DES) {
	    crv = CKR_KEY_TYPE_INCONSISTENT;
	    break;
	}
	t = NSS_DES;
	goto finish_des;
    case CKM_DES_CBC_PAD:
	context->doPad = PR_TRUE;
	context->blockSize = 8;
	/* fall thru */
    case CKM_DES_CBC:
	if (key_type != CKK_DES) {
	    crv = CKR_KEY_TYPE_INCONSISTENT;
	    break;
	}
	t = NSS_DES_CBC;
	goto finish_des;
    case CKM_DES3_ECB:
	if ((key_type != CKK_DES2) && (key_type != CKK_DES3)) {
	    crv = CKR_KEY_TYPE_INCONSISTENT;
	    break;
	}
	t = NSS_DES_EDE3;
	goto finish_des;
    case CKM_DES3_CBC_PAD:
	context->doPad = PR_TRUE;
	context->blockSize = 8;
	/* fall thru */
    case CKM_DES3_CBC:
	if ((key_type != CKK_DES2) && (key_type != CKK_DES3)) {
	    crv = CKR_KEY_TYPE_INCONSISTENT;
	    break;
	}
	t = NSS_DES_EDE3_CBC;
finish_des:
	att = pk11_FindAttribute(key,CKA_VALUE);
	if (att == NULL) {
	    crv = CKR_KEY_HANDLE_INVALID;
	    break;
	}
	if (useNewKey) {
	    crv = pk11_cdmf2des((unsigned char*)att->attrib.pValue,newdeskey);
	    if (crv != CKR_OK) {
		pk11_FreeAttribute(att);
		break;
	     }
	}
	context->cipherInfo = DES_CreateContext(
		useNewKey ? newdeskey : (unsigned char*)att->attrib.pValue,
		(unsigned char*)pMechanism->pParameter,t, PR_TRUE);
	pk11_FreeAttribute(att);
	if (context->cipherInfo == NULL) {
	    crv = CKR_HOST_MEMORY;
	    break;
	}
	context->update = (PK11Cipher) DES_Encrypt;
	context->destroy = (PK11Destroy) DES_DestroyContext;

	break;
    case CKM_AES_CBC_PAD:
	context->doPad = PR_TRUE;
	context->blockSize = 16;
	/* fall thru */
    case CKM_AES_ECB:
    case CKM_AES_CBC:
	if (key_type != CKK_AES) {
	    crv = CKR_KEY_TYPE_INCONSISTENT;
	    break;
	}
	att = pk11_FindAttribute(key,CKA_VALUE);
	if (att == NULL) {
	    crv = CKR_KEY_HANDLE_INVALID;
	    break;
	}
	context->cipherInfo = AES_CreateContext(
	    (unsigned char*)att->attrib.pValue,
	    (unsigned char*)pMechanism->pParameter,
	    pMechanism->mechanism == CKM_AES_ECB ? NSS_AES : NSS_AES_CBC,
	    PR_TRUE, att->attrib.ulValueLen, 16);
	pk11_FreeAttribute(att);
	if (context->cipherInfo == NULL) {
	    crv = CKR_HOST_MEMORY;
	    break;
	}
	context->update = (PK11Cipher) AES_Encrypt;
	context->destroy = (PK11Destroy) AES_DestroyContext;

	break;
    default:
	crv = CKR_MECHANISM_INVALID;
	break;
    }

    pk11_FreeObject(key);
    if (crv != CKR_OK) {
        pk11_FreeContext(context);
	pk11_FreeSession(session);
	return crv;
    }
    pk11_SetContextByType(session, contextType, context);
    pk11_FreeSession(session);
    return CKR_OK;
}

/* NSC_EncryptInit initializes an encryption operation. */
CK_RV NSC_EncryptInit(CK_SESSION_HANDLE hSession,
		 CK_MECHANISM_PTR pMechanism, CK_OBJECT_HANDLE hKey)
{
    return pk11_EncryptInit(hSession, pMechanism, hKey, CKA_ENCRYPT, 
							PK11_ENCRYPT);
}

/* NSC_EncryptUpdate continues a multiple-part encryption operation. */
CK_RV NSC_EncryptUpdate(CK_SESSION_HANDLE hSession,
    CK_BYTE_PTR pPart, CK_ULONG ulPartLen, CK_BYTE_PTR pEncryptedPart,	
					CK_ULONG_PTR pulEncryptedPartLen)
{
    PK11SessionContext *context;
    unsigned int outlen,i;
    unsigned int padoutlen = 0;
    unsigned int maxout = *pulEncryptedPartLen;
    CK_RV crv;
    SECStatus rv;

    /* make sure we're legal */
    crv = pk11_GetContext(hSession,&context,PK11_ENCRYPT,PR_TRUE,NULL);
    if (crv != CKR_OK) return crv;

    /* do padding */
    if (context->doPad) {
	/* deal with previous buffered data */
	if (context->padDataLength != 0) {
	    /* fill in the padded to a full block size */
	    for (i=context->padDataLength; 
			(ulPartLen != 0) && i < context->blockSize; i++) {
		context->padBuf[i] = *pPart++;
		ulPartLen--;
		context->padDataLength++;
	    }

	    /* not enough data to encrypt yet? then return */
	    if (context->padDataLength != context->blockSize) {
		*pulEncryptedPartLen = 0;
		return CKR_OK;
	    }
	    /* encrypt the current padded data */
    	    rv = (*context->update)(context->cipherInfo,pEncryptedPart, 
		&outlen,context->blockSize,context->padBuf,context->blockSize);
    	    if (rv != SECSuccess) return CKR_DEVICE_ERROR;
	    pEncryptedPart += padoutlen;
	    maxout -= padoutlen;
	}
	/* save the residual */
	context->padDataLength = ulPartLen % context->blockSize;
	if (context->padDataLength) {
	    PORT_Memcpy(context->padBuf,
			&pPart[ulPartLen-context->padDataLength],
							context->padDataLength);
	    ulPartLen -= context->padDataLength;
	}
	/* if we've exhausted our new buffer, we're done */
	if (ulPartLen == 0) {
	    *pulEncryptedPartLen = padoutlen;
	    return CKR_OK;
	}
    }
	


    /* do it: NOTE: this assumes buf size in is >= buf size out! */
    rv = (*context->update)(context->cipherInfo,pEncryptedPart, 
					&outlen, maxout, pPart, ulPartLen);
    *pulEncryptedPartLen = (CK_ULONG) (outlen + padoutlen);
    return (rv == SECSuccess) ? CKR_OK : CKR_DEVICE_ERROR;
}


/* NSC_EncryptFinal finishes a multiple-part encryption operation. */
CK_RV NSC_EncryptFinal(CK_SESSION_HANDLE hSession,
    CK_BYTE_PTR pLastEncryptedPart, CK_ULONG_PTR pulLastEncryptedPartLen)
{
    PK11Session *session;
    PK11SessionContext *context;
    unsigned int outlen,i;
    unsigned int maxout = *pulLastEncryptedPartLen;
    CK_RV crv;
    SECStatus rv = SECSuccess;

    /* make sure we're legal */
    crv = pk11_GetContext(hSession,&context,PK11_ENCRYPT,PR_TRUE,&session);
    if (crv != CKR_OK) return crv;

    *pulLastEncryptedPartLen = 0;

    /* do padding */
    if (context->doPad) {
	unsigned char  padbyte = (unsigned char) 
				(context->blockSize - context->padDataLength); 
	/* fill out rest of pad buffer with pad magic*/
	for (i=context->padDataLength; i < context->blockSize; i++) {
	    context->padBuf[i] = padbyte;
	}
	rv = (*context->update)(context->cipherInfo,pLastEncryptedPart, 
			&outlen, maxout, context->padBuf, context->blockSize);
	if (rv == SECSuccess) *pulLastEncryptedPartLen = (CK_ULONG) outlen;
    }

    /* do it */
    pk11_SetContextByType(session, PK11_ENCRYPT, NULL);
    pk11_FreeContext(context);
    pk11_FreeSession(session);
    return (rv == SECSuccess) ? CKR_OK : CKR_DEVICE_ERROR;
}

/* NSC_Encrypt encrypts single-part data. */
CK_RV NSC_Encrypt (CK_SESSION_HANDLE hSession, CK_BYTE_PTR pData,
    		CK_ULONG ulDataLen, CK_BYTE_PTR pEncryptedData,
					 CK_ULONG_PTR pulEncryptedDataLen)
{
    PK11Session *session;
    PK11SessionContext *context;
    unsigned int outlen;
    unsigned int maxoutlen = *pulEncryptedDataLen;
    CK_RV crv;
    CK_RV crv2;
    SECStatus rv;

    /* make sure we're legal */
    crv = pk11_GetContext(hSession,&context,PK11_ENCRYPT,PR_FALSE,&session);
    if (crv != CKR_OK) return crv;

    if (context->doPad) {
	CK_ULONG finalLen;
	/* padding is fairly complicated, have the update and final 
	 * code deal with it */
        pk11_FreeSession(session);
	crv = NSC_EncryptUpdate(hSession,pData,ulDataLen,pEncryptedData,
							pulEncryptedDataLen);
	if (crv != CKR_OK) *pulEncryptedDataLen = 0;
	maxoutlen -= *pulEncryptedDataLen;
	pEncryptedData += *pulEncryptedDataLen;
	finalLen = maxoutlen;
	crv2 = NSC_EncryptFinal(hSession, pEncryptedData, &finalLen);
	if (crv2 == CKR_OK) { *pulEncryptedDataLen += finalLen; }
	return crv == CKR_OK ? crv2 :crv;
    }
	

    /* do it: NOTE: this assumes buf size is big enough. */
    rv = (*context->update)(context->cipherInfo, pEncryptedData, 
					&outlen, maxoutlen, pData, ulDataLen);
    *pulEncryptedDataLen = (CK_ULONG) outlen;
    pk11_FreeContext(context);
    pk11_SetContextByType(session, PK11_ENCRYPT, NULL);
    pk11_FreeSession(session);

    return (rv == SECSuccess) ? CKR_OK : CKR_DEVICE_ERROR;
}


/*
 ************** Crypto Functions:     Decrypt ************************
 */

/* NSC_DecryptInit initializes a decryption operation. */
static CK_RV pk11_DecryptInit( CK_SESSION_HANDLE hSession,
  CK_MECHANISM_PTR pMechanism, CK_OBJECT_HANDLE hKey, CK_ATTRIBUTE_TYPE dtype,
				PK11ContextType contextType)
{
    PK11Session *session;
    PK11Object *key;
    PK11Attribute *att;
    PK11SessionContext *context;
    CK_RC2_CBC_PARAMS *rc2_param;
#if NSS_SOFTOKEN_DOES_RC5
    CK_RC5_CBC_PARAMS *rc5_param;
    SECItem rc5Key;
#endif
    CK_KEY_TYPE key_type;
    CK_RV crv = CKR_OK;
    unsigned effectiveKeyLength;
    SECKEYLowPrivateKey *privKey;
    unsigned char newdeskey[8];
    PRBool useNewKey=PR_FALSE;
    int t;

    session = pk11_SessionFromHandle(hSession);
    if (session == NULL) return CKR_SESSION_HANDLE_INVALID;

    crv = pk11_InitGeneric(session,&context,contextType,&key,hKey,&key_type,
						CKO_PRIVATE_KEY,dtype);
    if (crv != CKR_OK) {
	pk11_FreeSession(session);
	return crv;
    }

    /*
     * now handle each mechanism
     */
    switch(pMechanism->mechanism) {
    case CKM_RSA_PKCS:
    case CKM_RSA_X_509:
	if (key_type != CKK_RSA) {
	    crv = CKR_KEY_TYPE_INCONSISTENT;
	    break;
	}
	context->multi = PR_FALSE;
	privKey = pk11_GetPrivKey(key,CKK_RSA);
	if (privKey == NULL) {
	    crv = CKR_HOST_MEMORY;
	    break;
	}
	context->cipherInfo = privKey;
	context->update = (PK11Cipher) (pMechanism->mechanism == CKM_RSA_X_509
			? RSA_DecryptRaw : RSA_DecryptBlock);
	context->destroy = (context->cipherInfo == key->objectInfo) ?
		(PK11Destroy) pk11_Null : (PK11Destroy) pk11_FreePrivKey;
	break;
    case CKM_RC2_CBC_PAD:
	context->doPad = PR_TRUE;
	context->blockSize = 8;
	/* fall thru */
    case CKM_RC2_ECB:
    case CKM_RC2_CBC:
	if (key_type != CKK_RC2) {
	    crv = CKR_KEY_TYPE_INCONSISTENT;
	    break;
	}
	att = pk11_FindAttribute(key,CKA_VALUE);
	rc2_param = (CK_RC2_CBC_PARAMS *)pMechanism->pParameter;
	effectiveKeyLength = (rc2_param->ulEffectiveBits+7)/8;
	context->cipherInfo = RC2_CreateContext((unsigned char*)att->attrib.pValue,
		att->attrib.ulValueLen, rc2_param->iv,
		 pMechanism->mechanism == CKM_RC2_ECB ? NSS_RC2 :
					 NSS_RC2_CBC, effectiveKeyLength);
	pk11_FreeAttribute(att);
	if (context->cipherInfo == NULL) {
	    crv = CKR_HOST_MEMORY;
	    break;
	}
	context->update = (PK11Cipher) RC2_Decrypt;
	context->destroy = (PK11Destroy) RC2_DestroyContext;
	break;
#if NSS_SOFTOKEN_DOES_RC5
    case CKM_RC5_CBC_PAD:
	context->doPad = PR_TRUE;
	/* fall thru */
    case CKM_RC5_ECB:
    case CKM_RC5_CBC:
	if (key_type != CKK_RC5) {
	    crv = CKR_KEY_TYPE_INCONSISTENT;
	    break;
	}
	att = pk11_FindAttribute(key,CKA_VALUE);
	if (att == NULL) {
	    crv = CKR_KEY_HANDLE_INVALID;
	    break;
	}
	rc5_param = (CK_RC5_CBC_PARAMS *)pMechanism->pParameter;
	if (context->doPad) {
	   context->blockSize = rc5_param->ulWordsize*2;
	}
	rc5Key.data = (unsigned char*)att->attrib.pValue;
	rc5Key.len = att->attrib.ulValueLen;
	context->cipherInfo = RC5_CreateContext(&rc5Key,rc5_param->ulRounds,
	   rc5_param->ulWordsize,rc5_param->pIv,
		 pMechanism->mechanism == CKM_RC5_ECB ? NSS_RC5 : NSS_RC5_CBC);
	pk11_FreeAttribute(att);
	if (context->cipherInfo == NULL) {
	    crv = CKR_HOST_MEMORY;
	    break;
	}
	context->update = (PK11Cipher) RC5_Decrypt;
	context->destroy = (PK11Destroy) RC5_DestroyContext;
	break;
#endif
    case CKM_RC4:
	if (key_type != CKK_RC4) {
	    crv = CKR_KEY_TYPE_INCONSISTENT;
	    break;
	}
	att = pk11_FindAttribute(key,CKA_VALUE);
	context->cipherInfo = 
	    RC4_CreateContext((unsigned char*)att->attrib.pValue,
			      att->attrib.ulValueLen);
	pk11_FreeAttribute(att);
	if (context->cipherInfo == NULL) {
	    crv = CKR_HOST_MEMORY;
	    break;
	}
	context->update = (PK11Cipher) RC4_Decrypt;
	context->destroy = (PK11Destroy) RC4_DestroyContext;
	break;
    case CKM_CDMF_CBC_PAD:
	context->doPad = PR_TRUE;
	context->blockSize = 8;
	/* fall thru */
    case CKM_CDMF_ECB:
    case CKM_CDMF_CBC:
	if (key_type != CKK_CDMF) {
	    crv = CKR_KEY_TYPE_INCONSISTENT;
	    break;
	}
	t = (pMechanism->mechanism == CKM_CDMF_ECB) ? NSS_DES : NSS_DES_CBC;
	useNewKey = PR_TRUE;
	if (crv != CKR_OK) break;
	goto finish_des;
    case CKM_DES_ECB:
	if (key_type != CKK_DES) {
	    crv = CKR_KEY_TYPE_INCONSISTENT;
	    break;
	}
	t = NSS_DES;
	goto finish_des;
    case CKM_DES_CBC_PAD:
	context->doPad = PR_TRUE;
	context->blockSize = 8;
	/* fall thru */
    case CKM_DES_CBC:
	if (key_type != CKK_DES) {
	    crv = CKR_KEY_TYPE_INCONSISTENT;
	    break;
	}
	t = NSS_DES_CBC;
	goto finish_des;
    case CKM_DES3_ECB:
	if ((key_type != CKK_DES2) && (key_type != CKK_DES3)) {
	    crv = CKR_KEY_TYPE_INCONSISTENT;
	    break;
	}
	t = NSS_DES_EDE3;
	goto finish_des;
    case CKM_DES3_CBC_PAD:
	context->doPad = PR_TRUE;
	context->blockSize = 8;
	/* fall thru */
    case CKM_DES3_CBC:
	if ((key_type != CKK_DES2) && (key_type != CKK_DES3)) {
	    crv = CKR_KEY_TYPE_INCONSISTENT;
	    break;
	}
	t = NSS_DES_EDE3_CBC;
finish_des:
	att = pk11_FindAttribute(key,CKA_VALUE);
	if (att == NULL) {
	    crv = CKR_KEY_HANDLE_INVALID;
	    break;
	}
	if (useNewKey) {
	    crv = pk11_cdmf2des((unsigned char*)att->attrib.pValue,newdeskey);
	    if (crv != CKR_OK) {
		pk11_FreeAttribute(att);
		break;
	     }
	}
	context->cipherInfo = DES_CreateContext(
		useNewKey ? newdeskey : (unsigned char*)att->attrib.pValue,
		(unsigned char*)pMechanism->pParameter,t, PR_FALSE);
	pk11_FreeAttribute(att);
	if (context->cipherInfo == NULL) {
	    crv = CKR_HOST_MEMORY;
	    break;
	}
	context->update = (PK11Cipher) DES_Decrypt;
	context->destroy = (PK11Destroy) DES_DestroyContext;

	break;
    case CKM_AES_CBC_PAD:
	context->doPad = PR_TRUE;
	context->blockSize = 16;
	/* fall thru */
    case CKM_AES_ECB:
    case CKM_AES_CBC:
	if (key_type != CKK_AES) {
	    crv = CKR_KEY_TYPE_INCONSISTENT;
	    break;
	}
	att = pk11_FindAttribute(key,CKA_VALUE);
	if (att == NULL) {
	    crv = CKR_KEY_HANDLE_INVALID;
	    break;
	}
	context->cipherInfo = AES_CreateContext(
	    (unsigned char*)att->attrib.pValue,
	    (unsigned char*)pMechanism->pParameter,
	    pMechanism->mechanism == CKM_AES_ECB ? NSS_AES : NSS_AES_CBC,
	    PR_TRUE, att->attrib.ulValueLen,16);
	pk11_FreeAttribute(att);
	if (context->cipherInfo == NULL) {
	    crv = CKR_HOST_MEMORY;
	    break;
	}
	context->update = (PK11Cipher) AES_Decrypt;
	context->destroy = (PK11Destroy) AES_DestroyContext;

	break;
    default:
	crv = CKR_MECHANISM_INVALID;
	break;
    }

    pk11_FreeObject(key);
    if (crv != CKR_OK) {
        pk11_FreeContext(context);
	pk11_FreeSession(session);
	return crv;
    }
    pk11_SetContextByType(session, contextType, context);
    pk11_FreeSession(session);
    return CKR_OK;
}

/* NSC_DecryptInit initializes a decryption operation. */
CK_RV NSC_DecryptInit( CK_SESSION_HANDLE hSession,
			 CK_MECHANISM_PTR pMechanism, CK_OBJECT_HANDLE hKey)
{
    return pk11_DecryptInit(hSession,pMechanism,hKey,CKA_DECRYPT,PK11_DECRYPT);
}

/* NSC_DecryptUpdate continues a multiple-part decryption operation. */
CK_RV NSC_DecryptUpdate(CK_SESSION_HANDLE hSession,
    CK_BYTE_PTR pEncryptedPart, CK_ULONG ulEncryptedPartLen,
    				CK_BYTE_PTR pPart, CK_ULONG_PTR pulPartLen)
{
    PK11SessionContext *context;
    unsigned int padoutlen = 0;
    unsigned int outlen;
    unsigned int maxout = *pulPartLen;
    CK_RV crv;
    SECStatus rv;

    /* make sure we're legal */
    crv = pk11_GetContext(hSession,&context,PK11_DECRYPT,PR_TRUE,NULL);
    if (crv != CKR_OK) return crv;

    if (context->doPad) {
	/* first decrypt our saved buffer */
	if (context->padDataLength != 0) {
    	    rv = (*context->update)(context->cipherInfo, pPart, &padoutlen,
		 maxout, context->padBuf, context->blockSize);
    	    if (rv != SECSuccess)  return CKR_DEVICE_ERROR;
	    pPart += padoutlen;
	    maxout -= padoutlen;
	}
	/* now save the final block for the next decrypt or the final */
	PORT_Memcpy(context->padBuf,&pEncryptedPart[ulEncryptedPartLen -
				context->blockSize], context->blockSize);
	context->padDataLength = context->blockSize;
	ulEncryptedPartLen -= context->padDataLength;
    }

    /* do it: NOTE: this assumes buf size in is >= buf size out! */
    rv = (*context->update)(context->cipherInfo,pPart, &outlen,
		 maxout, pEncryptedPart, ulEncryptedPartLen);
    *pulPartLen = (CK_ULONG) (outlen + padoutlen);
    return (rv == SECSuccess)  ? CKR_OK : CKR_DEVICE_ERROR;
}


/* NSC_DecryptFinal finishes a multiple-part decryption operation. */
CK_RV NSC_DecryptFinal(CK_SESSION_HANDLE hSession,
    CK_BYTE_PTR pLastPart, CK_ULONG_PTR pulLastPartLen)
{
    PK11Session *session;
    PK11SessionContext *context;
    unsigned int outlen;
    unsigned int maxout = *pulLastPartLen;
    CK_RV crv;
    SECStatus rv = SECSuccess;

    /* make sure we're legal */
    crv = pk11_GetContext(hSession,&context,PK11_DECRYPT,PR_TRUE,&session);
    if (crv != CKR_OK) return crv;

    *pulLastPartLen = 0;
    if (context->doPad) {
	/* decrypt our saved buffer */
	if (context->padDataLength != 0) {
	    /* this assumes that pLastPart is big enough to hold the *whole*
	     * buffer!!! */
    	    rv = (*context->update)(context->cipherInfo, pLastPart, &outlen,
		 maxout, context->padBuf, context->blockSize);
    	    if (rv == SECSuccess) {
		unsigned int padSize = 
			    (unsigned int) pLastPart[context->blockSize-1];

		*pulLastPartLen = outlen - padSize;
	    }
	}
    }

    /* do it */
    pk11_SetContextByType(session, PK11_DECRYPT, NULL);
    pk11_FreeContext(context);
    pk11_FreeSession(session);
    return (rv == SECSuccess) ? CKR_OK : CKR_DEVICE_ERROR;
}

/* NSC_Decrypt decrypts encrypted data in a single part. */
CK_RV NSC_Decrypt(CK_SESSION_HANDLE hSession,
    CK_BYTE_PTR pEncryptedData,CK_ULONG ulEncryptedDataLen,CK_BYTE_PTR pData,
    						CK_ULONG_PTR pulDataLen)
{
    PK11Session *session;
    PK11SessionContext *context;
    unsigned int outlen;
    unsigned int maxoutlen = *pulDataLen;
    CK_RV crv;
    CK_RV crv2;
    SECStatus rv;

    /* make sure we're legal */
    crv = pk11_GetContext(hSession,&context,PK11_DECRYPT,PR_FALSE,&session);
    if (crv != CKR_OK) return crv;

    if (context->doPad) {
	CK_ULONG finalLen;
	/* padding is fairly complicated, have the update and final 
	 * code deal with it */
        pk11_FreeSession(session);
	crv = NSC_DecryptUpdate(hSession,pEncryptedData,ulEncryptedDataLen,
							pData, pulDataLen);
	if (crv != CKR_OK) *pulDataLen = 0;
	maxoutlen -= *pulDataLen;
	pData += *pulDataLen;
	finalLen = maxoutlen;
	crv2 = NSC_DecryptFinal(hSession, pData, &finalLen);
	if (crv2 == CKR_OK) { *pulDataLen += finalLen; }
	return crv == CKR_OK ? crv2 :crv;
    }

    rv = (*context->update)(context->cipherInfo, pData, &outlen, maxoutlen, 
					pEncryptedData, ulEncryptedDataLen);
    *pulDataLen = (CK_ULONG) outlen;
    pk11_FreeContext(context);
    pk11_SetContextByType(session, PK11_DECRYPT, NULL);
    pk11_FreeSession(session);
    return (rv == SECSuccess)  ? CKR_OK : CKR_DEVICE_ERROR;
}



/*
 ************** Crypto Functions:     Digest (HASH)  ************************
 */

/* NSC_DigestInit initializes a message-digesting operation. */
CK_RV NSC_DigestInit(CK_SESSION_HANDLE hSession,
    					CK_MECHANISM_PTR pMechanism)
{
    PK11Session *session;
    PK11SessionContext *context;
    MD2Context *md2_context;
    MD5Context *md5_context;
    SHA1Context *sha1_context;
    CK_RV crv = CKR_OK;

    session = pk11_SessionFromHandle(hSession);
    if (session == NULL) return CKR_SESSION_HANDLE_INVALID;
    crv = pk11_InitGeneric(session,&context,PK11_HASH,NULL,0,NULL, 0, 0);
    if (crv != CKR_OK) {
	pk11_FreeSession(session);
	return crv;
    }

    switch(pMechanism->mechanism) {
    case CKM_MD2:
	md2_context = MD2_NewContext();
	context->cipherInfo = (void *)md2_context;
	context->cipherInfoLen = MD2_FlattenSize(md2_context);
	context->currentMech = CKM_MD2;
        if (context->cipherInfo == NULL) {
	    crv= CKR_HOST_MEMORY;
	    
	}
	context->hashUpdate = (PK11Hash) MD2_Update;
	context->end = (PK11End) MD2_End;
	context->destroy = (PK11Destroy) MD2_DestroyContext;
	MD2_Begin(md2_context);
	break;
    case CKM_MD5:
	md5_context = MD5_NewContext();
	context->cipherInfo = (void *)md5_context;
	context->cipherInfoLen = MD5_FlattenSize(md5_context);
	context->currentMech = CKM_MD5;
        if (context->cipherInfo == NULL) {
	    crv= CKR_HOST_MEMORY;
	    
	}
	context->hashUpdate = (PK11Hash) MD5_Update;
	context->end = (PK11End) MD5_End;
	context->destroy = (PK11Destroy) MD5_DestroyContext;
	MD5_Begin(md5_context);
	break;
    case CKM_SHA_1:
	sha1_context = SHA1_NewContext();
	context->cipherInfo = (void *)sha1_context;
	context->cipherInfoLen = SHA1_FlattenSize(sha1_context);
	context->currentMech = CKM_SHA_1;
        if (context->cipherInfo == NULL) {
	    crv= CKR_HOST_MEMORY;
	    break;
	}
	context->hashUpdate = (PK11Hash) SHA1_Update;
	context->end = (PK11End) SHA1_End;
	context->destroy = (PK11Destroy) SHA1_DestroyContext;
	SHA1_Begin(sha1_context);
	break;
    default:
	crv = CKR_MECHANISM_INVALID;
	break;
    }

    if (crv != CKR_OK) {
        pk11_FreeContext(context);
	pk11_FreeSession(session);
	return crv;
    }
    pk11_SetContextByType(session, PK11_HASH, context);
    pk11_FreeSession(session);
    return CKR_OK;
}


/* NSC_Digest digests data in a single part. */
CK_RV NSC_Digest(CK_SESSION_HANDLE hSession,
    CK_BYTE_PTR pData, CK_ULONG ulDataLen, CK_BYTE_PTR pDigest,
    						CK_ULONG_PTR pulDigestLen)
{
    PK11Session *session;
    PK11SessionContext *context;
    unsigned int digestLen;
    unsigned int maxout = *pulDigestLen;
    CK_RV crv;

    /* make sure we're legal */
    crv = pk11_GetContext(hSession,&context,PK11_HASH,PR_FALSE,&session);
    if (crv != CKR_OK) return crv;

    /* do it: */
    (*context->hashUpdate)(context->cipherInfo, pData, ulDataLen);
    /*  NOTE: this assumes buf size is bigenough for the algorithm */
    (*context->end)(context->cipherInfo, pDigest, &digestLen,maxout);
    *pulDigestLen = digestLen;

    pk11_SetContextByType(session, PK11_HASH, NULL);
    pk11_FreeContext(context);
    pk11_FreeSession(session);
    return CKR_OK;
}


/* NSC_DigestUpdate continues a multiple-part message-digesting operation. */
CK_RV NSC_DigestUpdate(CK_SESSION_HANDLE hSession,CK_BYTE_PTR pPart,
					    CK_ULONG ulPartLen)
{
    PK11SessionContext *context;
    CK_RV crv;

    /* make sure we're legal */
    crv = pk11_GetContext(hSession,&context,PK11_HASH,PR_TRUE,NULL);
    if (crv != CKR_OK) return crv;
    /* do it: */
    (*context->hashUpdate)(context->cipherInfo, pPart, ulPartLen);
    return CKR_OK;
}


/* NSC_DigestFinal finishes a multiple-part message-digesting operation. */
CK_RV NSC_DigestFinal(CK_SESSION_HANDLE hSession,CK_BYTE_PTR pDigest,
    						CK_ULONG_PTR pulDigestLen)
{
    PK11Session *session;
    PK11SessionContext *context;
    unsigned int maxout = *pulDigestLen;
    unsigned int digestLen;
    CK_RV crv;

    /* make sure we're legal */
    crv = pk11_GetContext(hSession, &context, PK11_HASH, PR_TRUE, &session);
    if (crv != CKR_OK) return crv;

    if (pDigest != NULL) {
        (*context->end)(context->cipherInfo, pDigest, &digestLen, maxout);
        *pulDigestLen = digestLen;
    } else {
	*pulDigestLen = 0;
    }

    pk11_SetContextByType(session, PK11_HASH, NULL);
    pk11_FreeContext(context);
    pk11_FreeSession(session);
    return CKR_OK;
}

/*
 * this helper functions are used by Generic Macing and Signing functions
 * that use hashes as part of their operations. 
 */
static CK_RV
pk11_doSubMD2(PK11SessionContext *context) {
	MD2Context *md2_context = MD2_NewContext();
	context->hashInfo = (void *)md2_context;
        if (context->hashInfo == NULL) {
	    return CKR_HOST_MEMORY;
	}
	context->hashUpdate = (PK11Hash) MD2_Update;
	context->end = (PK11End) MD2_End;
	context->hashdestroy = (PK11Destroy) MD2_DestroyContext;
	MD2_Begin(md2_context);
	return CKR_OK;
}

static CK_RV
pk11_doSubMD5(PK11SessionContext *context) {
	MD5Context *md5_context = MD5_NewContext();
	context->hashInfo = (void *)md5_context;
        if (context->hashInfo == NULL) {
	    return CKR_HOST_MEMORY;
	}
	context->hashUpdate = (PK11Hash) MD5_Update;
	context->end = (PK11End) MD5_End;
	context->hashdestroy = (PK11Destroy) MD5_DestroyContext;
	MD5_Begin(md5_context);
	return CKR_OK;
}

static CK_RV
pk11_doSubSHA1(PK11SessionContext *context) {
	SHA1Context *sha1_context = SHA1_NewContext();
	context->hashInfo = (void *)sha1_context;
        if (context->hashInfo == NULL) {
	    return CKR_HOST_MEMORY;
	}
	context->hashUpdate = (PK11Hash) SHA1_Update;
	context->end = (PK11End) SHA1_End;
	context->hashdestroy = (PK11Destroy) SHA1_DestroyContext;
	SHA1_Begin(sha1_context);
	return CKR_OK;
}

/*
 * HMAC General copies only a portion of the result. This update routine likes
 * the final HMAC output with the signature.
 */
static SECStatus
pk11_HMACCopy(CK_ULONG *copyLen,unsigned char *sig,unsigned int *sigLen,
		unsigned int maxLen,unsigned char *hash, unsigned int hashLen)
{
    if (maxLen < *copyLen) return SECFailure;
    PORT_Memcpy(sig,hash,*copyLen);
    *sigLen = *copyLen;
    return SECSuccess;
}

/* Verify is just a compare for HMAC */
static SECStatus
pk11_HMACCmp(CK_ULONG *copyLen,unsigned char *sig,unsigned int sigLen,
				unsigned char *hash, unsigned int hashLen)
{
    return PORT_Memcmp(sig,hash,*copyLen) ? SECSuccess : SECFailure ;
}

/*
 * common HMAC initalization routine
 */
static CK_RV
pk11_doHMACInit(PK11SessionContext *context,SECOidTag oid,
					PK11Object *key, CK_ULONG mac_size)
{
    PK11Attribute *keyval;
    HMACContext *HMACcontext;
    CK_ULONG *intpointer;

    keyval = pk11_FindAttribute(key,CKA_VALUE);
    if (keyval == NULL) return CKR_KEY_SIZE_RANGE;

    HMACcontext = HMAC_Create(oid, (const unsigned char*)keyval->attrib.pValue,
						 keyval->attrib.ulValueLen);
    context->hashInfo = HMACcontext;
    context->multi = PR_TRUE;
    pk11_FreeAttribute(keyval);
    if (context->hashInfo == NULL) {
	return CKR_HOST_MEMORY;
    }
    context->hashUpdate = (PK11Hash) HMAC_Update;
    context->end = (PK11End) HMAC_Finish;

    context->hashdestroy = (PK11Destroy) pk11_HMAC_Destroy;
    intpointer = (CK_ULONG *) PORT_Alloc(sizeof(CK_ULONG));
    if (intpointer == NULL) {
	return CKR_HOST_MEMORY;
    }
    *intpointer = mac_size;
    context->cipherInfo = (void *) intpointer;
    context->destroy = (PK11Destroy) pk11_Space;
    context->update = (PK11Cipher) pk11_HMACCopy;
    context->verify = (PK11Verify) pk11_HMACCmp;
    HMAC_Begin(HMACcontext);
    return CKR_OK;
}

/*
 *  SSL Macing support. SSL Macs are inited, then update with the base
 * hashing algorithm, then finalized in sign and verify
 */

/*
 * FROM SSL:
 * 60 bytes is 3 times the maximum length MAC size that is supported.
 * We probably should have one copy of this table. We still need this table
 * in ssl to 'sign' the handshake hashes.
 */
static unsigned char ssl_pad_1 [60] = {
    0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
    0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
    0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
    0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
    0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
    0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
    0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
    0x36, 0x36, 0x36, 0x36
};
static unsigned char ssl_pad_2 [60] = {
    0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c,
    0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c,
    0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c,
    0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c,
    0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c,
    0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c,
    0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c,
    0x5c, 0x5c, 0x5c, 0x5c
};

static SECStatus
pk11_SSLMACSign(PK11SSLMACInfo *info,unsigned char *sig,unsigned int *sigLen,
		unsigned int maxLen,unsigned char *hash, unsigned int hashLen)
{
    unsigned char tmpBuf[PK11_MAX_MAC_LENGTH];
    unsigned int out;

    info->begin(info->hashContext);
    info->update(info->hashContext,info->key,info->keySize);
    info->update(info->hashContext,ssl_pad_2,info->padSize);
    info->update(info->hashContext,hash,hashLen);
    info->end(info->hashContext,tmpBuf,&out,PK11_MAX_MAC_LENGTH);
    PORT_Memcpy(sig,tmpBuf,info->macSize);
    *sigLen = info->macSize;
    return SECSuccess;
}

static SECStatus
pk11_SSLMACVerify(PK11SSLMACInfo *info,unsigned char *sig,unsigned int sigLen,
		unsigned char *hash, unsigned int hashLen)
{
    unsigned char tmpBuf[PK11_MAX_MAC_LENGTH];
    unsigned int out;

    info->begin(info->hashContext);
    info->update(info->hashContext,info->key,info->keySize);
    info->update(info->hashContext,ssl_pad_2,info->padSize);
    info->update(info->hashContext,hash,hashLen);
    info->end(info->hashContext,tmpBuf,&out,PK11_MAX_MAC_LENGTH);
    return (PORT_Memcmp(sig,tmpBuf,info->macSize) == 0) ? 
						SECSuccess : SECFailure;
}

/*
 * common HMAC initalization routine
 */
static CK_RV
pk11_doSSLMACInit(PK11SessionContext *context,SECOidTag oid,
					PK11Object *key, CK_ULONG mac_size)
{
    PK11Attribute *keyval;
    PK11Begin begin;
    int padSize;
    PK11SSLMACInfo *sslmacinfo;
    CK_RV crv = CKR_MECHANISM_INVALID;

    if (oid == SEC_OID_SHA1) {
	crv = pk11_doSubSHA1(context);
	begin = (PK11Begin) SHA1_Begin;
	if (crv != CKR_OK) return crv;
	padSize = 40;
    } else {
	crv = pk11_doSubMD5(context);
	if (crv != CKR_OK) return crv;
	begin = (PK11Begin) MD5_Begin;
	padSize = 48;
    }
    context->multi = PR_TRUE;

    keyval = pk11_FindAttribute(key,CKA_VALUE);
    if (keyval == NULL) return CKR_KEY_SIZE_RANGE;

    context->hashUpdate(context->hashInfo,keyval->attrib.pValue,
						 keyval->attrib.ulValueLen);
    context->hashUpdate(context->hashInfo,ssl_pad_1,padSize);
    sslmacinfo = (PK11SSLMACInfo *) PORT_Alloc(sizeof(PK11SSLMACInfo));
    if (sslmacinfo == NULL) {
        pk11_FreeAttribute(keyval);
	return CKR_HOST_MEMORY;
    }
    sslmacinfo->macSize = mac_size;
    sslmacinfo->hashContext = context->hashInfo;
    PORT_Memcpy(sslmacinfo->key,keyval->attrib.pValue,
					keyval->attrib.ulValueLen);
    sslmacinfo->keySize = keyval->attrib.ulValueLen;
    sslmacinfo->begin = begin;
    sslmacinfo->end = context->end;
    sslmacinfo->update = context->hashUpdate;
    sslmacinfo->padSize = padSize;
    pk11_FreeAttribute(keyval);
    context->cipherInfo = (void *) sslmacinfo;
    context->destroy = (PK11Destroy) pk11_Space;
    context->update = (PK11Cipher) pk11_SSLMACSign;
    context->verify = (PK11Verify) pk11_SSLMACVerify;
    return CKR_OK;
}

typedef struct {
    PRUint32	cxSize;		/* size of allocated block, in bytes.        */
    PRUint32	cxKeyLen;	/* number of bytes of cxBuf containing key.  */
    PRUint32	cxDataLen;	/* number of bytes of cxBuf containing data. */
    SECStatus	cxRv;		/* records failure of void functions.        */
    unsigned char cxBuf[512];	/* actual size may be larger than 512.       */
} TLSPRFContext;

static void
pk11_TLSPRFHashUpdate(TLSPRFContext *cx, const unsigned char *data, 
                        unsigned int data_len)
{
    PRUint32 bytesUsed = PK11_OFFSETOF(TLSPRFContext, cxBuf) + 
                         cx->cxKeyLen + cx->cxDataLen;

    if (cx->cxRv != SECSuccess)	/* function has previously failed. */
    	return;
    if (bytesUsed + data_len > cx->cxSize) {
	/* We don't use realloc here because 
	** (a) realloc doesn't zero out the old block, and 
	** (b) if realloc fails, we lose the old block.
	*/
	PRUint32 blockSize = bytesUsed + data_len + 512;
    	TLSPRFContext *new_cx = (TLSPRFContext *)PORT_Alloc(blockSize);
	if (!new_cx) {
	   cx->cxRv = SECFailure;
	   return;
	}
	PORT_Memcpy(new_cx, cx, cx->cxSize);
	new_cx->cxSize = blockSize;

	PORT_ZFree(cx, cx->cxSize);
	cx = new_cx;
    }
    PORT_Memcpy(cx->cxBuf + cx->cxKeyLen + cx->cxDataLen, data, data_len);
    cx->cxDataLen += data_len;
}

static void 
pk11_TLSPRFEnd(TLSPRFContext *ctx, unsigned char *hashout,
	 unsigned int *pDigestLen, unsigned int maxDigestLen)
{
    *pDigestLen = 0; /* tells Verify that no data has been input yet. */
}

/* Compute the PRF values from the data previously input. */
static SECStatus
pk11_TLSPRFUpdate(TLSPRFContext *cx, 
                  unsigned char *sig,		/* output goes here. */
		  unsigned int * sigLen, 	/* how much output.  */
		  unsigned int   maxLen, 	/* output buffer size */
		  unsigned char *hash, 		/* unused. */
		  unsigned int   hashLen)	/* unused. */
{
    SECStatus rv;
    SECItem sigItem;
    SECItem seedItem;
    SECItem secretItem;

    if (cx->cxRv != SECSuccess)
    	return cx->cxRv;

    secretItem.data = cx->cxBuf;
    secretItem.len  = cx->cxKeyLen;

    seedItem.data = cx->cxBuf + cx->cxKeyLen;
    seedItem.len  = cx->cxDataLen;

    sigItem.data = sig;
    sigItem.len  = maxLen;

    rv = pk11_PRF(&secretItem, NULL, &seedItem, &sigItem);
    if (rv == SECSuccess && sigLen != NULL)
    	*sigLen = sigItem.len;
    return rv;

}

static SECStatus
pk11_TLSPRFVerify(TLSPRFContext *cx, 
                  unsigned char *sig, 		/* input, for comparison. */
		  unsigned int   sigLen,	/* length of sig.         */
		  unsigned char *hash, 		/* data to be verified.   */
		  unsigned int   hashLen)	/* size of hash data.     */
{
    unsigned char * tmp    = (unsigned char *)PORT_Alloc(sigLen);
    unsigned int    tmpLen = sigLen;
    SECStatus       rv;

    if (!tmp)
    	return SECFailure;
    if (hashLen) {
    	/* hashLen is non-zero when the user does a one-step verify.
	** In this case, none of the data has been input yet.
	*/
    	pk11_TLSPRFHashUpdate(cx, hash, hashLen);
    }
    rv = pk11_TLSPRFUpdate(cx, tmp, &tmpLen, sigLen, NULL, 0);
    if (rv == SECSuccess) {
    	rv = (SECStatus)(1 - !PORT_Memcmp(tmp, sig, sigLen));
    }
    PORT_ZFree(tmp, sigLen);
    return rv;
}

static void
pk11_TLSPRFHashDestroy(TLSPRFContext *cx, PRBool freeit)
{
    if (freeit)
	PORT_ZFree(cx, cx->cxSize);
}

static CK_RV
pk11_TLSPRFInit(PK11SessionContext *context, 
		  PK11Object *        key, 
		  CK_KEY_TYPE         key_type)
{
    PK11Attribute * keyVal;
    TLSPRFContext * prf_cx;
    CK_RV           crv = CKR_HOST_MEMORY;
    PRUint32        keySize;
    PRUint32        blockSize;

    if (key_type != CKK_GENERIC_SECRET)
    	return CKR_KEY_TYPE_INCONSISTENT; /* CKR_KEY_FUNCTION_NOT_PERMITTED */

    context->multi = PR_TRUE;

    keyVal = pk11_FindAttribute(key, CKA_VALUE);
    keySize = (!keyVal) ? 0 : keyVal->attrib.ulValueLen;
    blockSize = keySize + sizeof(TLSPRFContext);
    prf_cx = (TLSPRFContext *)PORT_Alloc(blockSize);
    if (!prf_cx) 
    	goto done;
    prf_cx->cxSize    = blockSize;
    prf_cx->cxKeyLen  = keySize;
    prf_cx->cxDataLen = 0;
    prf_cx->cxRv        = SECSuccess;
    if (keySize)
	PORT_Memcpy(prf_cx->cxBuf, keyVal->attrib.pValue, keySize);

    context->hashInfo    = (void *) prf_cx;
    context->cipherInfo  = (void *) prf_cx;
    context->hashUpdate  = (PK11Hash)    pk11_TLSPRFHashUpdate;
    context->end         = (PK11End)     pk11_TLSPRFEnd;
    context->update      = (PK11Cipher)  pk11_TLSPRFUpdate;
    context->verify      = (PK11Verify)  pk11_TLSPRFVerify;
    context->destroy     = (PK11Destroy) pk11_Null;
    context->hashdestroy = (PK11Destroy) pk11_TLSPRFHashDestroy;
    crv = CKR_OK;

done:
    if (keyVal) 
	pk11_FreeAttribute(keyVal);
    return crv;
}

/*
 ************** Crypto Functions:     Sign  ************************
 */

/*
 * Check if We're using CBCMacing and initialize the session context if we are.
 */
static CK_RV
pk11_InitCBCMac(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR pMechanism,
 	CK_OBJECT_HANDLE hKey, CK_ATTRIBUTE_TYPE keyUsage,
						 PK11ContextType contextType)
	
{
    CK_MECHANISM cbc_mechanism;
    CK_ULONG mac_bytes = PK11_INVALID_MAC_SIZE;
    CK_RC2_CBC_PARAMS rc2_params;
#if NSS_SOFTOKEN_DOES_RC5
    CK_RC5_CBC_PARAMS rc5_params;
    CK_RC5_MAC_GENERAL_PARAMS *rc5_mac;
#endif
    unsigned char ivBlock[PK11_MAX_BLOCK_SIZE];
    PK11SessionContext *context;
    CK_RV crv;
    int blockSize;

    switch (pMechanism->mechanism) {
    case CKM_RC2_MAC_GENERAL:
	mac_bytes = 
	    ((CK_RC2_MAC_GENERAL_PARAMS *)pMechanism->pParameter)->ulMacLength;
	/* fall through */
    case CKM_RC2_MAC:
	/* this works because ulEffectiveBits is in the same place in both the
	 * CK_RC2_MAC_GENERAL_PARAMS and CK_RC2_CBC_PARAMS */
	rc2_params.ulEffectiveBits = ((CK_RC2_MAC_GENERAL_PARAMS *)
				pMechanism->pParameter)->ulEffectiveBits;
	PORT_Memset(rc2_params.iv,0,sizeof(rc2_params.iv));
	cbc_mechanism.mechanism = CKM_RC2_CBC;
	cbc_mechanism.pParameter = &rc2_params;
	cbc_mechanism.ulParameterLen = sizeof(rc2_params);
	blockSize = 8;
	break;
#if NSS_SOFTOKEN_DOES_RC5
    case CKM_RC5_MAC_GENERAL:
	mac_bytes = 
	    ((CK_RC5_MAC_GENERAL_PARAMS *)pMechanism->pParameter)->ulMacLength;
	/* fall through */
    case CKM_RC5_MAC:
	/* this works because ulEffectiveBits is in the same place in both the
	 * CK_RC5_MAC_GENERAL_PARAMS and CK_RC5_CBC_PARAMS */
	rc5_mac = (CK_RC5_MAC_GENERAL_PARAMS *)pMechanism->pParameter;
	rc5_params.ulWordsize = rc5_mac->ulWordsize;
	rc5_params.ulRounds = rc5_mac->ulRounds;
	rc5_params.pIv = ivBlock;
	blockSize = rc5_mac->ulWordsize*2;
	rc5_params.ulIvLen = blockSize;
	PORT_Memset(ivBlock,0,blockSize);
	cbc_mechanism.mechanism = CKM_RC5_CBC;
	cbc_mechanism.pParameter = &rc5_params;
	cbc_mechanism.ulParameterLen = sizeof(rc5_params);
	break;
#endif
    /* add cast and idea later */
    case CKM_DES_MAC_GENERAL:
	mac_bytes = *(CK_ULONG *)pMechanism->pParameter;
	/* fall through */
    case CKM_DES_MAC:
	blockSize = 8;
	PORT_Memset(ivBlock,0,blockSize);
	cbc_mechanism.mechanism = CKM_DES_CBC;
	cbc_mechanism.pParameter = &ivBlock;
	cbc_mechanism.ulParameterLen = blockSize;
	break;
    case CKM_DES3_MAC_GENERAL:
	mac_bytes = *(CK_ULONG *)pMechanism->pParameter;
	/* fall through */
    case CKM_DES3_MAC:
	blockSize = 8;
	PORT_Memset(ivBlock,0,blockSize);
	cbc_mechanism.mechanism = CKM_DES3_CBC;
	cbc_mechanism.pParameter = &ivBlock;
	cbc_mechanism.ulParameterLen = blockSize;
	break;
    case CKM_CDMF_MAC_GENERAL:
	mac_bytes = *(CK_ULONG *)pMechanism->pParameter;
	/* fall through */
    case CKM_CDMF_MAC:
	blockSize = 8;
	PORT_Memset(ivBlock,0,blockSize);
	cbc_mechanism.mechanism = CKM_CDMF_CBC;
	cbc_mechanism.pParameter = &ivBlock;
	cbc_mechanism.ulParameterLen = blockSize;
	break;
    case CKM_AES_MAC_GENERAL:
	mac_bytes = *(CK_ULONG *)pMechanism->pParameter;
	/* fall through */
    case CKM_AES_MAC:
	blockSize = 16;
	PORT_Memset(ivBlock,0,blockSize);
	cbc_mechanism.mechanism = CKM_AES_CBC;
	cbc_mechanism.pParameter = &ivBlock;
	cbc_mechanism.ulParameterLen = blockSize;
	break;
    default:
	return CKR_FUNCTION_NOT_SUPPORTED;
    }

    crv = pk11_EncryptInit(hSession, &cbc_mechanism, hKey, keyUsage,
								contextType);
    if (crv != CKR_OK) return crv;
    crv = pk11_GetContext(hSession,&context,contextType,PR_TRUE,NULL);

    /* this shouldn't happen! */
    PORT_Assert(crv == CKR_OK);
    if (crv != CKR_OK) return crv;
    context->blockSize = blockSize;
    if (mac_bytes == PK11_INVALID_MAC_SIZE) mac_bytes = blockSize/2;
    context->macSize = mac_bytes;
    return CKR_OK;
}

/*
 * encode RSA PKCS #1 Signature data before signing... 
 */
static SECStatus
pk11_HashSign(PK11HashSignInfo *info,unsigned char *sig,unsigned int *sigLen,
		unsigned int maxLen,unsigned char *hash, unsigned int hashLen)
{
    
    SECStatus rv = SECFailure;
    SECItem digder;
    PLArenaPool *arena = NULL;
    SGNDigestInfo *di = NULL;

    digder.data = NULL;

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if ( !arena ) { goto loser; }
    
    /* Construct digest info */
    di = SGN_CreateDigestInfo(info->hashOid, hash, hashLen);
    if (!di) { goto loser; }

    /* Der encode the digest as a DigestInfo */
    rv = DER_Encode(arena, &digder, SGNDigestInfoTemplate, di);
    if (rv != SECSuccess) {
	goto loser;
    }

    /*
    ** Encrypt signature after constructing appropriate PKCS#1 signature
    ** block
    */
    rv = RSA_Sign(info->key,sig,sigLen,maxLen,digder.data,digder.len);

  loser:
    SGN_DestroyDigestInfo(di);
    if (arena != NULL) {
	PORT_FreeArena(arena, PR_FALSE);
    }
    return rv;
}

static SECStatus
nsc_DSA_Verify_Stub(void *ctx, void *sigBuf, unsigned int sigLen,
                               void *dataBuf, unsigned int dataLen)
{
    SECItem signature, digest;
    SECKEYLowPublicKey *key = (SECKEYLowPublicKey *)ctx;

    signature.data = (unsigned char *)sigBuf;
    signature.len = sigLen;
    digest.data = (unsigned char *)dataBuf;
    digest.len = dataLen;
    return DSA_VerifyDigest(&(key->u.dsa), &signature, &digest);
}

static SECStatus
nsc_DSA_Sign_Stub(void *ctx, void *sigBuf,
                  unsigned int *sigLen, unsigned int maxSigLen,
                  void *dataBuf, unsigned int dataLen)
{
    SECItem signature = { 0 }, digest;
    SECStatus rv;
    SECKEYLowPrivateKey *key = (SECKEYLowPrivateKey *)ctx;

    (void)SECITEM_AllocItem(NULL, &signature, maxSigLen);
    digest.data = (unsigned char *)dataBuf;
    digest.len = dataLen;
    rv = DSA_SignDigest(&(key->u.dsa), &signature, &digest);
    *sigLen = signature.len;
    PORT_Memcpy(sigBuf, signature.data, signature.len);
    SECITEM_FreeItem(&signature, PR_FALSE);
    return rv;
}

/* NSC_SignInit setups up the signing operations. There are three basic
 * types of signing:
 *	(1) the tradition single part, where "Raw RSA" or "Raw DSA" is applied
 *  to data in a single Sign operation (which often looks a lot like an
 *  encrypt, with data coming in and data going out).
 *	(2) Hash based signing, where we continually hash the data, then apply
 *  some sort of signature to the end.
 *	(3) Block Encryption CBC MAC's, where the Data is encrypted with a key,
 *  and only the final block is part of the mac.
 *
 *  For case number 3, we initialize a context much like the Encryption Context
 *  (in fact we share code). We detect case 3 in C_SignUpdate, C_Sign, and 
 *  C_Final by the following method... if it's not multi-part, and it's doesn't
 *  have a hash context, it must be a block Encryption CBC MAC.
 *
 *  For case number 2, we initialize a hash structure, as well as make it 
 *  multi-part. Updates are simple calls to the hash update function. Final
 *  calls the hashend, then passes the result to the 'update' function (which
 *  operates as a final signature function). In some hash based MAC'ing (as
 *  opposed to hash base signatures), the update function is can be simply a 
 *  copy (as is the case with HMAC).
 */
CK_RV NSC_SignInit(CK_SESSION_HANDLE hSession,
		 CK_MECHANISM_PTR pMechanism, CK_OBJECT_HANDLE hKey)
{
    PK11Session *session;
    PK11Object *key;
    PK11SessionContext *context;
    CK_KEY_TYPE key_type;
    CK_RV crv = CKR_OK;
    SECKEYLowPrivateKey *privKey;
    PK11HashSignInfo *info = NULL;

    /* Block Cipher MACing Algorithms use a different Context init method..*/
    crv = pk11_InitCBCMac(hSession, pMechanism, hKey, CKA_SIGN, PK11_SIGN);
    if (crv != CKR_FUNCTION_NOT_SUPPORTED) return crv;

    /* we're not using a block cipher mac */
    session = pk11_SessionFromHandle(hSession);
    if (session == NULL) return CKR_SESSION_HANDLE_INVALID;
    crv = pk11_InitGeneric(session,&context,PK11_SIGN,&key,hKey,&key_type,
						CKO_PRIVATE_KEY,CKA_SIGN);
    if (crv != CKR_OK) {
	pk11_FreeSession(session);
	return crv;
    }

    context->multi = PR_FALSE;

    switch(pMechanism->mechanism) {
    case CKM_MD5_RSA_PKCS:
        context->multi = PR_TRUE;
	crv = pk11_doSubMD5(context);
	if (crv != CKR_OK) break;
	context->update = (PK11Cipher) pk11_HashSign;
	info = (PK11HashSignInfo *)PORT_Alloc(sizeof(PK11HashSignInfo));
	if (info == NULL) {
	   crv = CKR_HOST_MEMORY;
	   break;
	}
	info->hashOid = SEC_OID_MD5;
	goto finish_rsa;
    case CKM_MD2_RSA_PKCS:
        context->multi = PR_TRUE;
	crv = pk11_doSubMD2(context);
	if (crv != CKR_OK) break;
	context->update = (PK11Cipher) pk11_HashSign;
	info = (PK11HashSignInfo *)PORT_Alloc(sizeof(PK11HashSignInfo));
	if (info == NULL) {
	   crv = CKR_HOST_MEMORY;
	   break;
	}
	info->hashOid = SEC_OID_MD2;
	goto finish_rsa;
    case CKM_SHA1_RSA_PKCS:
        context->multi = PR_TRUE;
	crv = pk11_doSubSHA1(context);
	if (crv != CKR_OK) break;
	context->update = (PK11Cipher) pk11_HashSign;
	info = (PK11HashSignInfo *)PORT_Alloc(sizeof(PK11HashSignInfo));
	if (info == NULL) {
	   crv = CKR_HOST_MEMORY;
	   break;
	}
	info->hashOid = SEC_OID_SHA1;
	goto finish_rsa;
    case CKM_RSA_PKCS:
	context->update = (PK11Cipher) RSA_Sign;
	goto finish_rsa;
    case CKM_RSA_X_509:
	context->update = (PK11Cipher)  RSA_SignRaw;
finish_rsa:
	if (key_type != CKK_RSA) {
	    if (info) PORT_Free(info);
	    crv = CKR_KEY_TYPE_INCONSISTENT;
	    break;
	}
	context->multi = PR_FALSE;
	privKey = pk11_GetPrivKey(key,CKK_RSA);
	if (privKey == NULL) {
	    if (info) PORT_Free(info);
	    crv = CKR_HOST_MEMORY;
	    break;
	}
	/* OK, info is allocated only if we're doing hash and sign mechanism.
	 * It's necessary to be able to set the correct OID in the final 
	 * signature.
	 * Second, what's special about privKey == key->objectInfo?
	 * Well we don't 'cache' token versions
	 * of private keys because (1) it's sensitive data, and (2) it never
	 * gets destroyed. Instead we grab the key out of the database as
	 * necessary, but now the key is our context, and we need to free
	 * it when we are done. Non-token private keys will get freed when
	 * the user destroys the session object (or the session the session
	 * object lives in) */
	if (info) {
	    info->key = privKey;
	    context->cipherInfo = info;
	    context->destroy = (privKey == key->objectInfo) ?
		(PK11Destroy)pk11_Space:(PK11Destroy)pk11_FreeSignInfo;
	} else {
	    context->cipherInfo = privKey;
	    context->destroy = (privKey == key->objectInfo) ?
		(PK11Destroy)pk11_Null:(PK11Destroy)pk11_FreePrivKey;
	}
	break;

    case CKM_DSA_SHA1:
        context->multi = PR_TRUE;
	crv = pk11_doSubSHA1(context);
	if (crv != CKR_OK) break;
	/* fall through */
    case CKM_DSA:
	if (key_type != CKK_DSA) {
	    crv = CKR_KEY_TYPE_INCONSISTENT;
	    break;
	}
	privKey = pk11_GetPrivKey(key,CKK_DSA);
	if (privKey == NULL) {
	    crv = CKR_HOST_MEMORY;
	    break;
	}
	context->cipherInfo = privKey;
	context->update     = (PK11Cipher) nsc_DSA_Sign_Stub;
	context->destroy    = (privKey == key->objectInfo) ?
		(PK11Destroy) pk11_Null:(PK11Destroy)pk11_FreePrivKey;

	break;
    case CKM_MD2_HMAC_GENERAL:
	crv = pk11_doHMACInit(context,SEC_OID_MD2,key,
				*(CK_ULONG *)pMechanism->pParameter);
	break;
    case CKM_MD2_HMAC:
	crv = pk11_doHMACInit(context,SEC_OID_MD2,key,MD2_LENGTH);
	break;
    case CKM_MD5_HMAC_GENERAL:
	crv = pk11_doHMACInit(context,SEC_OID_MD5,key,
				*(CK_ULONG *)pMechanism->pParameter);
	break;
    case CKM_MD5_HMAC:
	crv = pk11_doHMACInit(context,SEC_OID_MD5,key,MD5_LENGTH);
	break;
    case CKM_SHA_1_HMAC_GENERAL:
	crv = pk11_doHMACInit(context,SEC_OID_SHA1,key,
				*(CK_ULONG *)pMechanism->pParameter);
	break;
    case CKM_SHA_1_HMAC:
	crv = pk11_doHMACInit(context,SEC_OID_SHA1,key,SHA1_LENGTH);
	break;
    case CKM_SSL3_MD5_MAC:
	crv = pk11_doSSLMACInit(context,SEC_OID_MD5,key,
					*(CK_ULONG *)pMechanism->pParameter);
	break;
    case CKM_SSL3_SHA1_MAC:
	crv = pk11_doSSLMACInit(context,SEC_OID_SHA1,key,
					*(CK_ULONG *)pMechanism->pParameter);
	break;
    case CKM_TLS_PRF_GENERAL:
	crv = pk11_TLSPRFInit(context, key, key_type);
	break;
    default:
	crv = CKR_MECHANISM_INVALID;
	break;
    }

    pk11_FreeObject(key);
    if (crv != CKR_OK) {
        PORT_Free(context);
	pk11_FreeSession(session);
	return crv;
    }
    pk11_SetContextByType(session, PK11_SIGN, context);
    pk11_FreeSession(session);
    return CKR_OK;
}


/* MACUpdate is the common implementation for SignUpdate and VerifyUpdate.
 * (sign and verify only very in their setup and final operations) */
static CK_RV 
pk11_MACUpdate(CK_SESSION_HANDLE hSession,CK_BYTE_PTR pPart,
    					CK_ULONG ulPartLen,PK11ContextType type)
{
    unsigned int outlen;
    PK11SessionContext *context;
    CK_RV crv;
    SECStatus rv;

    /* make sure we're legal */
    crv = pk11_GetContext(hSession,&context,type,PR_FALSE,NULL);
    if (crv != CKR_OK) return crv;

    if (context->hashInfo) {
	(*context->hashUpdate)(context->hashInfo, pPart, ulPartLen);
	return CKR_OK;
    }

    /* must be block cipher macing */

    /* deal with previous buffered data */
    if (context->padDataLength != 0) {
	int i;
	/* fill in the padded to a full block size */
	for (i=context->padDataLength; (ulPartLen != 0) && 
					i < (int)context->blockSize; i++) {
	    context->padBuf[i] = *pPart++;
	    ulPartLen--;
	    context->padDataLength++;
	}

	/* not enough data to encrypt yet? then return */
	if (context->padDataLength != context->blockSize)  return CKR_OK;
	/* encrypt the current padded data */
    	rv = (*context->update)(context->cipherInfo,context->macBuf,&outlen,
			PK11_MAX_BLOCK_SIZE,context->padBuf,context->blockSize);
    	if (rv != SECSuccess) return CKR_DEVICE_ERROR;
    }

    /* save the residual */
    context->padDataLength = ulPartLen % context->blockSize;
    if (context->padDataLength) {
	    PORT_Memcpy(context->padBuf,
			&pPart[ulPartLen-context->padDataLength],
							context->padDataLength);
	    ulPartLen -= context->padDataLength;
    }

    /* if we've exhausted our new buffer, we're done */
    if (ulPartLen == 0) { return CKR_OK; }

    /* run the data through out encrypter */	
    while (ulPartLen) {
    	rv = (*context->update)(context->cipherInfo, context->padBuf, &outlen, 
			PK11_MAX_BLOCK_SIZE, pPart, context->blockSize);
	if (rv != SECSuccess) return CKR_DEVICE_ERROR;
	/* paranoia.. make sure we exit the loop */
	PORT_Assert(ulPartLen >= context->blockSize);
	if (ulPartLen < context->blockSize) break;
	ulPartLen -= context->blockSize;
    }

    return CKR_OK;
	
}

/* NSC_SignUpdate continues a multiple-part signature operation,
 * where the signature is (will be) an appendix to the data, 
 * and plaintext cannot be recovered from the signature */
CK_RV NSC_SignUpdate(CK_SESSION_HANDLE hSession,CK_BYTE_PTR pPart,
    							CK_ULONG ulPartLen)
{
    return pk11_MACUpdate(hSession, pPart, ulPartLen, PK11_SIGN);
}


/* NSC_SignFinal finishes a multiple-part signature operation, 
 * returning the signature. */
CK_RV NSC_SignFinal(CK_SESSION_HANDLE hSession,CK_BYTE_PTR pSignature,
					    CK_ULONG_PTR pulSignatureLen)
{
    PK11Session *session;
    PK11SessionContext *context;
    unsigned int outlen;
    unsigned int digestLen;
    unsigned int maxoutlen = *pulSignatureLen;
    unsigned char tmpbuf[PK11_MAX_MAC_LENGTH];
    CK_RV crv;
    SECStatus rv = SECSuccess;

    /* make sure we're legal */
    *pulSignatureLen = 0;
    crv = pk11_GetContext(hSession,&context,PK11_SIGN,PR_TRUE,&session);
    if (crv != CKR_OK) return crv;

    if (context->hashInfo) {
        (*context->end)(context->hashInfo, tmpbuf, &digestLen, sizeof(tmpbuf));
	rv = (*context->update)(context->cipherInfo, pSignature,
					&outlen, maxoutlen, tmpbuf, digestLen);
        *pulSignatureLen = (CK_ULONG) outlen;
    } else {
	/* deal with the last block if any residual */
	if (context->padDataLength) {
	    /* fill out rest of pad buffer with pad magic*/
	    int i;
	    for (i=context->padDataLength; i < (int)context->blockSize; i++) {
		context->padBuf[i] = 0;
	    }
	    rv = (*context->update)(context->cipherInfo,context->macBuf,
		&outlen,PK11_MAX_BLOCK_SIZE,context->padBuf,context->blockSize);
	}
	if (rv == SECSuccess) {
	    PORT_Memcpy(pSignature,context->macBuf,context->macSize);
	    *pulSignatureLen = context->macSize;
	}
    }

    pk11_FreeContext(context);
    pk11_SetContextByType(session, PK11_SIGN, NULL);
    pk11_FreeSession(session);

    return (rv == SECSuccess) ? CKR_OK : CKR_DEVICE_ERROR;
}

/* NSC_Sign signs (encrypts with private key) data in a single part,
 * where the signature is (will be) an appendix to the data, 
 * and plaintext cannot be recovered from the signature */
CK_RV NSC_Sign(CK_SESSION_HANDLE hSession,
    CK_BYTE_PTR pData,CK_ULONG ulDataLen,CK_BYTE_PTR pSignature,
    					CK_ULONG_PTR pulSignatureLen)
{
    PK11Session *session;
    PK11SessionContext *context;
    unsigned int outlen;
    unsigned int maxoutlen = *pulSignatureLen;
    CK_RV crv,crv2;
    SECStatus rv;

    /* make sure we're legal */
    crv = pk11_GetContext(hSession,&context,PK11_SIGN,PR_FALSE,&session);
    if (crv != CKR_OK) return crv;

    /* multi part Signing are completely implemented by SignUpdate and
     * sign Final */
    if (context->multi) {
        pk11_FreeSession(session);
	crv = NSC_SignUpdate(hSession,pData,ulDataLen);
	if (crv != CKR_OK) *pulSignatureLen = 0;
	crv2 = NSC_SignFinal(hSession, pSignature, pulSignatureLen);
	return crv == CKR_OK ? crv2 :crv;
    }

    rv = (*context->update)(context->cipherInfo, pSignature,
					&outlen, maxoutlen, pData, ulDataLen);
    *pulSignatureLen = (CK_ULONG) outlen;
    pk11_FreeContext(context);
    pk11_SetContextByType(session, PK11_SIGN, NULL);
    pk11_FreeSession(session);

    return (rv == SECSuccess) ? CKR_OK : CKR_DEVICE_ERROR;
}


/*
 ************** Crypto Functions:     Sign Recover  ************************
 */
/* NSC_SignRecoverInit initializes a signature operation,
 * where the (digest) data can be recovered from the signature. 
 * E.g. encryption with the user's private key */
CK_RV NSC_SignRecoverInit(CK_SESSION_HANDLE hSession,
			 CK_MECHANISM_PTR pMechanism,CK_OBJECT_HANDLE hKey)
{
    switch (pMechanism->mechanism) {
    case CKM_RSA_PKCS:
    case CKM_RSA_X_509:
	return NSC_SignInit(hSession,pMechanism,hKey);
    default:
	break;
    }
    return CKR_MECHANISM_INVALID;
}


/* NSC_SignRecover signs data in a single operation
 * where the (digest) data can be recovered from the signature. 
 * E.g. encryption with the user's private key */
CK_RV NSC_SignRecover(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pData,
  CK_ULONG ulDataLen, CK_BYTE_PTR pSignature, CK_ULONG_PTR pulSignatureLen)
{
    return NSC_Sign(hSession,pData,ulDataLen,pSignature,pulSignatureLen);
}

/*
 ************** Crypto Functions:     verify  ************************
 */

/* Handle RSA Signature formating */
static SECStatus
pk11_hashCheckSign(PK11HashVerifyInfo *info, unsigned char *sig, 
	unsigned int sigLen, unsigned char *digest, unsigned int digestLen)
{

    SECItem it;
    SGNDigestInfo *di = NULL;
    SECStatus rv = SECSuccess;
    
    it.data = NULL;

    if (info->key == NULL) goto loser;

    it.len = SECKEY_LowPublicModulusLen(info->key); 
    if (!it.len) goto loser;

    it.data = (unsigned char *) PORT_Alloc(it.len);
    if (it.data == NULL) goto loser;

    /* decrypt the block */
    rv = RSA_CheckSignRecover(info->key, it.data, &it.len, it.len, sig, sigLen);
    if (rv != SECSuccess) goto loser;

    di = SGN_DecodeDigestInfo(&it);
    if (di == NULL) goto loser;
    if (di->digest.len != digestLen)  goto loser; 

    /* make sure the tag is OK */
    if (SECOID_GetAlgorithmTag(&di->digestAlgorithm) != info->hashOid) {
	goto loser;
    }
    /* Now check the signature */
    if (PORT_Memcmp(digest, di->digest.data, di->digest.len) == 0) {
	goto done;
    }

  loser:
    rv = SECFailure;

  done:
    if (it.data != NULL) PORT_Free(it.data);
    if (di != NULL) SGN_DestroyDigestInfo(di);
    
    return rv;
}

/* NSC_VerifyInit initializes a verification operation, 
 * where the signature is an appendix to the data, 
 * and plaintext cannot be recovered from the signature (e.g. DSA) */
CK_RV NSC_VerifyInit(CK_SESSION_HANDLE hSession,
			   CK_MECHANISM_PTR pMechanism,CK_OBJECT_HANDLE hKey) 
{
    PK11Session *session;
    PK11Object *key;
    PK11SessionContext *context;
    CK_KEY_TYPE key_type;
    CK_RV crv = CKR_OK;
    SECKEYLowPublicKey *pubKey;
    PK11HashVerifyInfo *info = NULL;

    /* Block Cipher MACing Algorithms use a different Context init method..*/
    crv = pk11_InitCBCMac(hSession, pMechanism, hKey, CKA_VERIFY, PK11_VERIFY);
    if (crv != CKR_FUNCTION_NOT_SUPPORTED) return crv;

    session = pk11_SessionFromHandle(hSession);
    if (session == NULL) return CKR_SESSION_HANDLE_INVALID;
    crv = pk11_InitGeneric(session,&context,PK11_VERIFY,&key,hKey,&key_type,
						CKO_PUBLIC_KEY,CKA_VERIFY);
    if (crv != CKR_OK) {
	pk11_FreeSession(session);
	return crv;
    }

    context->multi = PR_FALSE;

    switch(pMechanism->mechanism) {
    case CKM_MD5_RSA_PKCS:
        context->multi = PR_TRUE;
	crv = pk11_doSubMD5(context);
	if (crv != CKR_OK) break;
	context->verify = (PK11Verify) pk11_hashCheckSign;
	info = (PK11HashVerifyInfo *)PORT_Alloc(sizeof(PK11HashVerifyInfo));
	if (info == NULL) {
	   crv = CKR_HOST_MEMORY;
	   break;
	}
	info->hashOid = SEC_OID_MD5;
	goto finish_rsa;
    case CKM_MD2_RSA_PKCS:
        context->multi = PR_TRUE;
	crv = pk11_doSubMD2(context);
	if (crv != CKR_OK) break;
	context->verify = (PK11Verify) pk11_hashCheckSign;
	info = (PK11HashVerifyInfo *)PORT_Alloc(sizeof(PK11HashVerifyInfo));
	if (info == NULL) {
	   crv = CKR_HOST_MEMORY;
	   break;
	}
	info->hashOid = SEC_OID_MD2;
	goto finish_rsa;
    case CKM_SHA1_RSA_PKCS:
        context->multi = PR_TRUE;
	crv = pk11_doSubSHA1(context);
	if (crv != CKR_OK) break;
	context->verify = (PK11Verify) pk11_hashCheckSign;
	info = (PK11HashVerifyInfo *)PORT_Alloc(sizeof(PK11HashVerifyInfo));
	if (info == NULL) {
	   crv = CKR_HOST_MEMORY;
	   break;
	}
	info->hashOid = SEC_OID_SHA1;
	goto finish_rsa;
    case CKM_RSA_PKCS:
	context->verify = (PK11Verify) RSA_CheckSign;
	goto finish_rsa;
    case CKM_RSA_X_509:
	context->verify = (PK11Verify) RSA_CheckSignRaw;
finish_rsa:
	if (key_type != CKK_RSA) {
	    crv = CKR_KEY_TYPE_INCONSISTENT;
	    break;
	}
	pubKey = pk11_GetPubKey(key,CKK_RSA);
	if (pubKey == NULL) {
	    crv = CKR_HOST_MEMORY;
	    break;
	}
	if (info) {
	    info->key = pubKey;
	    context->cipherInfo = info;
	    context->destroy = pk11_Space;
	} else {
	    context->cipherInfo = pubKey;
	    context->destroy = pk11_Null;
	}
	break;
    case CKM_DSA_SHA1:
        context->multi = PR_TRUE;
	crv = pk11_doSubSHA1(context);
	if (crv != CKR_OK) break;
	/* fall through */
    case CKM_DSA:
	if (key_type != CKK_DSA) {
	    crv = CKR_KEY_TYPE_INCONSISTENT;
	    break;
	}
	context->multi = PR_FALSE;
	pubKey = pk11_GetPubKey(key,CKK_DSA);
	if (pubKey == NULL) {
	    crv = CKR_HOST_MEMORY;
	    break;
	}
	context->cipherInfo = pubKey;
	context->verify     = (PK11Verify) nsc_DSA_Verify_Stub;
	context->destroy    = pk11_Null;
	break;

    case CKM_MD2_HMAC_GENERAL:
	crv = pk11_doHMACInit(context,SEC_OID_MD2,key,
				*(CK_ULONG *)pMechanism->pParameter);
	break;
    case CKM_MD2_HMAC:
	crv = pk11_doHMACInit(context,SEC_OID_MD2,key,MD2_LENGTH);
	break;
    case CKM_MD5_HMAC_GENERAL:
	crv = pk11_doHMACInit(context,SEC_OID_MD5,key,
				*(CK_ULONG *)pMechanism->pParameter);
	break;
    case CKM_MD5_HMAC:
	crv = pk11_doHMACInit(context,SEC_OID_MD5,key,MD5_LENGTH);
	break;
    case CKM_SHA_1_HMAC_GENERAL:
	crv = pk11_doHMACInit(context,SEC_OID_SHA1,key,
				*(CK_ULONG *)pMechanism->pParameter);
	break;
    case CKM_SHA_1_HMAC:
	crv = pk11_doHMACInit(context,SEC_OID_SHA1,key,SHA1_LENGTH);
	break;
    case CKM_SSL3_MD5_MAC:
	crv = pk11_doSSLMACInit(context,SEC_OID_MD5,key,
					*(CK_ULONG *)pMechanism->pParameter);
	break;
    case CKM_SSL3_SHA1_MAC:
	crv = pk11_doSSLMACInit(context,SEC_OID_SHA1,key,
					*(CK_ULONG *)pMechanism->pParameter);
	break;
    case CKM_TLS_PRF_GENERAL:
	crv = pk11_TLSPRFInit(context, key, key_type);

    default:
	crv = CKR_MECHANISM_INVALID;
	break;
    }

    pk11_FreeObject(key);
    if (crv != CKR_OK) {
        PORT_Free(context);
	pk11_FreeSession(session);
	return crv;
    }
    pk11_SetContextByType(session, PK11_VERIFY, context);
    pk11_FreeSession(session);
    return CKR_OK;
}

/* NSC_Verify verifies a signature in a single-part operation, 
 * where the signature is an appendix to the data, 
 * and plaintext cannot be recovered from the signature */
CK_RV NSC_Verify(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pData,
    CK_ULONG ulDataLen, CK_BYTE_PTR pSignature, CK_ULONG ulSignatureLen)
{
    PK11Session *session;
    PK11SessionContext *context;
    CK_RV crv;
    SECStatus rv;

    /* make sure we're legal */
    crv = pk11_GetContext(hSession,&context,PK11_VERIFY,PR_FALSE,&session);
    if (crv != CKR_OK) return crv;

    rv = (*context->verify)(context->cipherInfo,pSignature, ulSignatureLen,
							 pData, ulDataLen);
    pk11_FreeContext(context);
    pk11_SetContextByType(session, PK11_VERIFY, NULL);
    pk11_FreeSession(session);

    return (rv == SECSuccess) ? CKR_OK : CKR_SIGNATURE_INVALID;

}


/* NSC_VerifyUpdate continues a multiple-part verification operation, 
 * where the signature is an appendix to the data, 
 * and plaintext cannot be recovered from the signature */
CK_RV NSC_VerifyUpdate( CK_SESSION_HANDLE hSession, CK_BYTE_PTR pPart,
						CK_ULONG ulPartLen)
{
    return pk11_MACUpdate(hSession, pPart, ulPartLen, PK11_VERIFY);
}


/* NSC_VerifyFinal finishes a multiple-part verification operation, 
 * checking the signature. */
CK_RV NSC_VerifyFinal(CK_SESSION_HANDLE hSession,
			CK_BYTE_PTR pSignature,CK_ULONG ulSignatureLen)
{
    PK11Session *session;
    PK11SessionContext *context;
    unsigned int outlen;
    unsigned int digestLen;
    unsigned char tmpbuf[PK11_MAX_MAC_LENGTH];
    CK_RV crv;
    SECStatus rv = SECSuccess;

    /* make sure we're legal */
    crv = pk11_GetContext(hSession,&context,PK11_VERIFY,PR_TRUE,&session);
    if (crv != CKR_OK) return crv;

    if (context->hashInfo) {
        (*context->end)(context->hashInfo, tmpbuf, &digestLen, sizeof(tmpbuf));
	rv = (*context->verify)(context->cipherInfo, pSignature, 
					ulSignatureLen, tmpbuf, digestLen);
    } else {
	if (context->padDataLength) {
	    /* fill out rest of pad buffer with pad magic*/
	    int i;
	    for (i=context->padDataLength; i < (int)context->blockSize; i++) {
		context->padBuf[i] = 0;
	    }
	    rv = (*context->update)(context->cipherInfo,context->macBuf, 
		&outlen,PK11_MAX_BLOCK_SIZE,context->padBuf,context->blockSize);
	}
	if (rv == SECSuccess) {
	    rv =(PORT_Memcmp(pSignature,context->macBuf,context->macSize) == 0)
				 ? SECSuccess : SECFailure;
	}
    }

    pk11_FreeContext(context);
    pk11_SetContextByType(session, PK11_VERIFY, NULL);
    pk11_FreeSession(session);
    return (rv == SECSuccess) ? CKR_OK : CKR_SIGNATURE_INVALID;

}

/*
 ************** Crypto Functions:     Verify  Recover ************************
 */

/* NSC_VerifyRecoverInit initializes a signature verification operation, 
 * where the data is recovered from the signature. 
 * E.g. Decryption with the user's public key */
CK_RV NSC_VerifyRecoverInit(CK_SESSION_HANDLE hSession,
			CK_MECHANISM_PTR pMechanism,CK_OBJECT_HANDLE hKey)
{
    PK11Session *session;
    PK11Object *key;
    PK11SessionContext *context;
    CK_KEY_TYPE key_type;
    CK_RV crv = CKR_OK;
    SECKEYLowPublicKey *pubKey;

    session = pk11_SessionFromHandle(hSession);
    if (session == NULL) return CKR_SESSION_HANDLE_INVALID;
    crv = pk11_InitGeneric(session,&context,PK11_VERIFY_RECOVER,
			&key,hKey,&key_type,CKO_PUBLIC_KEY,CKA_VERIFY_RECOVER);
    if (crv != CKR_OK) {
	pk11_FreeSession(session);
	return crv;
    }

    context->multi = PR_TRUE;

    switch(pMechanism->mechanism) {
    case CKM_RSA_PKCS:
    case CKM_RSA_X_509:
	if (key_type != CKK_RSA) {
	    crv = CKR_KEY_TYPE_INCONSISTENT;
	    break;
	}
	context->multi = PR_FALSE;
	pubKey = pk11_GetPubKey(key,CKK_RSA);
	if (pubKey == NULL) {
	    crv = CKR_HOST_MEMORY;
	    break;
	}
	context->cipherInfo = pubKey;
	context->update = (PK11Cipher) (pMechanism->mechanism == CKM_RSA_X_509
			? RSA_CheckSignRecoverRaw : RSA_CheckSignRecover);
	context->destroy = pk11_Null;
	break;
    default:
	crv = CKR_MECHANISM_INVALID;
	break;
    }

    pk11_FreeObject(key);
    if (crv != CKR_OK) {
        PORT_Free(context);
	pk11_FreeSession(session);
	return crv;
    }
    pk11_SetContextByType(session, PK11_VERIFY_RECOVER, context);
    pk11_FreeSession(session);
    return CKR_OK;
}


/* NSC_VerifyRecover verifies a signature in a single-part operation, 
 * where the data is recovered from the signature. 
 * E.g. Decryption with the user's public key */
CK_RV NSC_VerifyRecover(CK_SESSION_HANDLE hSession,
		 CK_BYTE_PTR pSignature,CK_ULONG ulSignatureLen,
    				CK_BYTE_PTR pData,CK_ULONG_PTR pulDataLen)
{
    PK11Session *session;
    PK11SessionContext *context;
    unsigned int outlen;
    unsigned int maxoutlen = *pulDataLen;
    CK_RV crv;
    SECStatus rv;

    /* make sure we're legal */
    crv = pk11_GetContext(hSession,&context,PK11_VERIFY_RECOVER,
							PR_FALSE,&session);
    if (crv != CKR_OK) return crv;

    rv = (*context->update)(context->cipherInfo, pData, &outlen, maxoutlen, 
						pSignature, ulSignatureLen);
    *pulDataLen = (CK_ULONG) outlen;
    pk11_FreeContext(context);
    pk11_SetContextByType(session, PK11_VERIFY_RECOVER, NULL);
    pk11_FreeSession(session);
    return (rv == SECSuccess)  ? CKR_OK : CKR_DEVICE_ERROR;
}

/*
 **************************** Random Functions:  ************************
 */

/* NSC_SeedRandom mixes additional seed material into the token's random number 
 * generator. */
CK_RV NSC_SeedRandom(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pSeed,
    CK_ULONG ulSeedLen) 
{
    SECStatus rv;

    rv = RNG_RandomUpdate(pSeed, ulSeedLen);
    return (rv == SECSuccess) ? CKR_OK : CKR_DEVICE_ERROR;
}

/* NSC_GenerateRandom generates random data. */
CK_RV NSC_GenerateRandom(CK_SESSION_HANDLE hSession,
    CK_BYTE_PTR	pRandomData, CK_ULONG ulRandomLen)
{
    SECStatus rv;

    rv = RNG_GenerateGlobalRandomBytes(pRandomData, ulRandomLen);
    return (rv == SECSuccess) ? CKR_OK : CKR_DEVICE_ERROR;
}

/*
 **************************** Key Functions:  ************************
 */

CK_RV
pk11_pbe_hmac_key_gen(CK_MECHANISM_PTR pMechanism, char *buf, 
                      unsigned long *len, PRBool faultyPBE3DES)
{
    PBEBitGenContext *pbeCx;
    SECItem pwd, salt, *key;
    SECOidTag hashAlg;
    unsigned long keylenbits;
    CK_PBE_PARAMS *pbe_params = NULL;
    pbe_params = (CK_PBE_PARAMS *)pMechanism->pParameter;
    pwd.data = (unsigned char *)pbe_params->pPassword;
    pwd.len = (unsigned int)pbe_params->ulPasswordLen;
    salt.data = (unsigned char *)pbe_params->pSalt;
    salt.len = (unsigned int)pbe_params->ulSaltLen;
    switch (pMechanism->mechanism) {
    case CKM_NETSCAPE_PBE_SHA1_HMAC_KEY_GEN:
	hashAlg = SEC_OID_SHA1; keylenbits = 160; break;
    case CKM_NETSCAPE_PBE_MD5_HMAC_KEY_GEN:
	hashAlg = SEC_OID_MD5;  keylenbits = 128; break;
    case CKM_NETSCAPE_PBE_MD2_HMAC_KEY_GEN:
	hashAlg = SEC_OID_MD2;  keylenbits = 128; break;
    default:
	return CKR_MECHANISM_INVALID;
    }
    pbeCx = PBE_CreateContext(hashAlg, pbeBitGenIntegrityKey, &pwd,
                              &salt, keylenbits, pbe_params->ulIteration);
    key = PBE_GenerateBits(pbeCx);
    PORT_Memcpy(buf, key->data, key->len);
    *len = key->len;
    PBE_DestroyContext(pbeCx);
    SECITEM_ZfreeItem(key, PR_TRUE);
    return CKR_OK;
}

/*
 * generate a password based encryption key. This code uses
 * PKCS5 to do the work. Note that it calls PBE_PK11ParamToAlgid, which is
 * a utility function in secpkcs5.c.  This function is used in here
 * and in PK11_ParamToAlgid.
 */
CK_RV
pk11_pbe_key_gen(SECOidTag algtag,CK_MECHANISM_PTR pMechanism,
				char *buf,CK_ULONG *key_length, PRBool faulty3DES)
{
    SECAlgorithmID algid;
    SECItem *pbe_key = NULL, mech;
    CK_PBE_PARAMS *pbe_params = NULL;
    SECStatus pbe_rv;

    *key_length = 0;

    mech.data = (unsigned char *)pMechanism->pParameter;
    mech.len = (unsigned int)pMechanism->ulParameterLen;

    /* A common routine to map from Params to AlgIDs for PBE
     * algorithms was created in secpkcs5.c.  This function is
     * called both by PK11_ParamToAlgid and this function.
     */
    pbe_rv = PBE_PK11ParamToAlgid(algtag, &mech, NULL, &algid);
    if (pbe_rv != SECSuccess) {
	return CKR_DATA_INVALID;
    }
    pbe_params = (CK_PBE_PARAMS *)pMechanism->pParameter;
    mech.data = (unsigned char *)pbe_params->pPassword;
    mech.len = (unsigned int)pbe_params->ulPasswordLen;
    pbe_key = SEC_PKCS5GetKey(&algid, &mech, faulty3DES);
    if (pbe_key == NULL) {
	SECOID_DestroyAlgorithmID(&algid, PR_FALSE);
	return CKR_HOST_MEMORY;
    }
    PORT_Memcpy(buf, pbe_key->data, pbe_key->len);
    *key_length = pbe_key->len;
    SECITEM_ZfreeItem(pbe_key, PR_TRUE);
    pbe_key = NULL;

    if (pbe_params->pInitVector == NULL) {
	pbe_key = SEC_PKCS5GetIV(&algid, &mech, faulty3DES);
	if (pbe_key == NULL) {
	    SECOID_DestroyAlgorithmID(&algid, PR_FALSE);
	    SECITEM_ZfreeItem(pbe_key, PR_TRUE);
	    return CKR_HOST_MEMORY;
	}
	pbe_params->pInitVector = (CK_CHAR_PTR)PORT_ZAlloc(pbe_key->len);
	if (pbe_params->pInitVector == NULL) {
	    SECOID_DestroyAlgorithmID(&algid, PR_FALSE);
	    SECITEM_ZfreeItem(pbe_key, PR_TRUE);
	    return CKR_HOST_MEMORY;
	}
	PORT_Memcpy(pbe_params->pInitVector, pbe_key->data, pbe_key->len);
    }
    SECITEM_ZfreeItem(pbe_key, PR_TRUE);
    SECOID_DestroyAlgorithmID(&algid, PR_FALSE);
    return CKR_OK;
}


static CK_RV
nsc_SetupBulkKeyGen(CK_MECHANISM_TYPE mechanism,
			CK_KEY_TYPE *key_type,CK_ULONG *key_length) {
    CK_RV crv = CKR_OK;

    switch (mechanism) {
    case CKM_RC2_KEY_GEN:
	*key_type = CKK_RC2;
	if (*key_length == 0) crv = CKR_TEMPLATE_INCOMPLETE;
	break;
#if NSS_SOFTOKEN_DOES_RC5
    case CKM_RC5_KEY_GEN:
	*key_type = CKK_RC5;
	if (*key_length == 0) crv = CKR_TEMPLATE_INCOMPLETE;
	break;
#endif
    case CKM_RC4_KEY_GEN:
	*key_type = CKK_RC4;
	if (*key_length == 0) crv = CKR_TEMPLATE_INCOMPLETE;
	break;
    case CKM_GENERIC_SECRET_KEY_GEN:
	*key_type = CKK_GENERIC_SECRET;
	if (*key_length == 0) crv = CKR_TEMPLATE_INCOMPLETE;
	break;
    case CKM_CDMF_KEY_GEN:
	*key_type = CKK_CDMF;
	*key_length = 8;
	break;
    case CKM_DES_KEY_GEN:
	*key_type = CKK_DES;
	*key_length = 8;
	break;
    case CKM_DES2_KEY_GEN:
	*key_type = CKK_DES2;
	*key_length = 16;
	break;
    case CKM_DES3_KEY_GEN:
	*key_type = CKK_DES3;
	*key_length = 24;
	break;
    case CKM_AES_KEY_GEN:
	*key_type = CKK_AES;
	if (*key_length == 0) crv = CKR_TEMPLATE_INCOMPLETE;
	break;
    default:
	PORT_Assert(0);
	crv = CKR_MECHANISM_INVALID;
	break;
    }

    return crv;
}

static CK_RV
nsc_SetupPBEKeyGen(CK_MECHANISM_TYPE mechanism,SECOidTag *algtag,
			CK_KEY_TYPE *key_type,CK_ULONG *key_length) {
    CK_RV crv = CKR_OK;

    switch (mechanism) {
    case CKM_PBE_MD2_DES_CBC:
	*algtag = SEC_OID_PKCS5_PBE_WITH_MD2_AND_DES_CBC;
	*key_type = CKK_DES;
	break;
    case CKM_PBE_MD5_DES_CBC:
	*algtag = SEC_OID_PKCS5_PBE_WITH_MD5_AND_DES_CBC;
	*key_type = CKK_DES;
	break;
    case CKM_PBE_SHA1_RC4_40:
	*algtag = SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_40_BIT_RC4;
	*key_length = 5;
	*key_type = CKK_RC4;
	break;
    case CKM_PBE_SHA1_RC4_128:
	*algtag = SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_128_BIT_RC4;
	*key_length = 16;
	*key_type = CKK_RC4;
	break;
    case CKM_PBE_SHA1_RC2_40_CBC:
	*algtag = SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_40_BIT_RC2_CBC;
	*key_length = 5;
	*key_type = CKK_RC2;
	break;
    case CKM_PBE_SHA1_RC2_128_CBC:
	*algtag = SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_128_BIT_RC2_CBC;
	*key_length = 16;
	*key_type = CKK_RC2;
	break;
    case CKM_PBE_SHA1_DES3_EDE_CBC:
	*algtag = SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_3KEY_TRIPLE_DES_CBC;
	*key_length = 24;
	*key_type = CKK_DES3;
	break;
    case CKM_PBE_SHA1_DES2_EDE_CBC:
	*algtag = SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_2KEY_TRIPLE_DES_CBC;
	*key_length = 16;
	*key_type = CKK_DES2;
	break;
    case CKM_NETSCAPE_PBE_SHA1_DES_CBC:
	*algtag = SEC_OID_PKCS5_PBE_WITH_SHA1_AND_DES_CBC;
	*key_type = CKK_DES;
	break;
    case CKM_NETSCAPE_PBE_SHA1_FAULTY_3DES_CBC:
    case CKM_NETSCAPE_PBE_SHA1_TRIPLE_DES_CBC:
	*algtag = SEC_OID_PKCS12_PBE_WITH_SHA1_AND_TRIPLE_DES_CBC;
	*key_type = CKK_DES3;
	break;
    case CKM_NETSCAPE_PBE_SHA1_40_BIT_RC2_CBC:
	*algtag = SEC_OID_PKCS12_PBE_WITH_SHA1_AND_40_BIT_RC2_CBC;
	*key_type = CKK_RC2;
	break;
    case CKM_NETSCAPE_PBE_SHA1_128_BIT_RC2_CBC:
	*algtag = SEC_OID_PKCS12_PBE_WITH_SHA1_AND_128_BIT_RC2_CBC;
	*key_type = CKK_RC2;
	break;
    case CKM_NETSCAPE_PBE_SHA1_40_BIT_RC4:
	*algtag = SEC_OID_PKCS12_PBE_WITH_SHA1_AND_40_BIT_RC4;
	*key_type = CKK_RC4;
	break;
    case CKM_NETSCAPE_PBE_SHA1_128_BIT_RC4:
	*algtag = SEC_OID_PKCS12_PBE_WITH_SHA1_AND_128_BIT_RC4;
	*key_type = CKK_RC4;
	break;
    default:
	PORT_Assert(0);
	crv = CKR_MECHANISM_INVALID;
	break;
    }

    return crv;
}

/* NSC_GenerateKey generates a secret key, creating a new key object. */
CK_RV NSC_GenerateKey(CK_SESSION_HANDLE hSession,
    CK_MECHANISM_PTR pMechanism,CK_ATTRIBUTE_PTR pTemplate,CK_ULONG ulCount,
    						CK_OBJECT_HANDLE_PTR phKey)
{
    PK11Object *key;
    PK11Session *session;
    PRBool checkWeak = PR_FALSE;
    CK_ULONG key_length = 0;
    CK_KEY_TYPE key_type = -1;
    CK_OBJECT_CLASS objclass = CKO_SECRET_KEY;
    CK_RV crv = CKR_OK;
    CK_BBOOL cktrue = CK_TRUE;
    int i;
    PK11Slot *slot = pk11_SlotFromSessionHandle(hSession);
    char buf[MAX_KEY_LEN];
    enum {pk11_pbe, pk11_pbe_hmac, pk11_ssl, pk11_bulk} key_gen_type;
    SECOidTag algtag = SEC_OID_UNKNOWN;
    SSL3RSAPreMasterSecret *rsa_pms;
    CK_VERSION *version;
    /* in very old versions of NSS, there were implementation errors with key 
     * generation methods.  We want to beable to read these, but not 
     * produce them any more.  The affected algorithm was 3DES.
     */
    PRBool faultyPBE3DES = PR_FALSE;


    /*
     * now lets create an object to hang the attributes off of
     */
    key = pk11_NewObject(slot); /* fill in the handle later */
    if (key == NULL) {
	return CKR_HOST_MEMORY;
    }

    /*
     * load the template values into the object
     */
    for (i=0; i < (int) ulCount; i++) {
	if (pTemplate[i].type == CKA_VALUE_LEN) {
	    key_length = *(CK_ULONG *)pTemplate[i].pValue;
	    continue;
	}

	crv = pk11_AddAttributeType(key,pk11_attr_expand(&pTemplate[i]));
	if (crv != CKR_OK) break;
    }
    if (crv != CKR_OK) {
	pk11_FreeObject(key);
	return crv;
    }

    /* make sure we don't have any class, key_type, or value fields */
    pk11_DeleteAttributeType(key,CKA_CLASS);
    pk11_DeleteAttributeType(key,CKA_KEY_TYPE);
    pk11_DeleteAttributeType(key,CKA_VALUE);

    /* Now Set up the parameters to generate the key (based on mechanism) */
    key_gen_type = pk11_bulk; /* bulk key by default */
    switch (pMechanism->mechanism) {
    case CKM_CDMF_KEY_GEN:
    case CKM_DES_KEY_GEN:
    case CKM_DES2_KEY_GEN:
    case CKM_DES3_KEY_GEN:
	checkWeak = PR_TRUE;
    case CKM_RC2_KEY_GEN:
    case CKM_RC4_KEY_GEN:
    case CKM_GENERIC_SECRET_KEY_GEN:
    case CKM_AES_KEY_GEN:
#if NSS_SOFTOKEN_DOES_RC5
    case CKM_RC5_KEY_GEN:
#endif
	crv = nsc_SetupBulkKeyGen(pMechanism->mechanism,&key_type,&key_length);
	break;
    case CKM_SSL3_PRE_MASTER_KEY_GEN:
	key_type = CKK_GENERIC_SECRET;
	key_length = 48;
	key_gen_type = pk11_ssl;
	break;
    case CKM_NETSCAPE_PBE_SHA1_FAULTY_3DES_CBC:
	faultyPBE3DES = PR_TRUE;
    case CKM_NETSCAPE_PBE_SHA1_HMAC_KEY_GEN:
    case CKM_NETSCAPE_PBE_MD5_HMAC_KEY_GEN:
    case CKM_NETSCAPE_PBE_MD2_HMAC_KEY_GEN:
	key_gen_type = pk11_pbe_hmac;
	key_type = CKK_GENERIC_SECRET;
	break;
    case CKM_NETSCAPE_PBE_SHA1_TRIPLE_DES_CBC:
    case CKM_NETSCAPE_PBE_SHA1_40_BIT_RC2_CBC:
    case CKM_NETSCAPE_PBE_SHA1_DES_CBC:
    case CKM_NETSCAPE_PBE_SHA1_128_BIT_RC2_CBC:
    case CKM_NETSCAPE_PBE_SHA1_40_BIT_RC4:
    case CKM_NETSCAPE_PBE_SHA1_128_BIT_RC4:
    case CKM_PBE_SHA1_DES3_EDE_CBC:
    case CKM_PBE_SHA1_DES2_EDE_CBC:
    case CKM_PBE_SHA1_RC2_128_CBC:
    case CKM_PBE_SHA1_RC2_40_CBC:
    case CKM_PBE_SHA1_RC4_128:
    case CKM_PBE_SHA1_RC4_40:
    case CKM_PBE_MD5_DES_CBC:
    case CKM_PBE_MD2_DES_CBC:
	key_gen_type = pk11_pbe;
	crv = nsc_SetupPBEKeyGen(pMechanism->mechanism,&algtag,
							&key_type,&key_length);
	break;
    default:
	crv = CKR_MECHANISM_INVALID;
	break;
    }

    /* make sure we aren't going to overflow the buffer */
    if (sizeof(buf) < key_length) {
	/* someone is getting pretty optimistic about how big their key can
	 * be... */
        crv = CKR_TEMPLATE_INCONSISTENT;
    }

    if (crv != CKR_OK) { pk11_FreeObject(key); return crv; }

    /* if there was no error,
     * key_type *MUST* be set in the switch statement above */
    PORT_Assert( key_type != -1 );

    /*
     * now to the actual key gen.
     */
    switch (key_gen_type) {
    case pk11_pbe_hmac:
	crv = pk11_pbe_hmac_key_gen(pMechanism, buf, &key_length,
	                            faultyPBE3DES);
	break;
    case pk11_pbe:
	crv = pk11_pbe_key_gen(algtag, pMechanism, buf, &key_length,
			       faultyPBE3DES);
	break;
    case pk11_ssl:
	rsa_pms = (SSL3RSAPreMasterSecret *)buf;
	version = (CK_VERSION *)pMechanism->pParameter;
	rsa_pms->client_version[0] = version->major;
        rsa_pms->client_version[1] = version->minor;
        crv = 
	    NSC_GenerateRandom(0,&rsa_pms->random[0], sizeof(rsa_pms->random));
	break;
    case pk11_bulk:
	/* get the key, check for weak keys and repeat if found */
	do {
            crv = NSC_GenerateRandom(0, (unsigned char *)buf, key_length);
	} while (crv == CKR_OK && checkWeak && 
			pk11_IsWeakKey((unsigned char *)buf,key_type));
	break;
    }

    if (crv != CKR_OK) { pk11_FreeObject(key); return crv; }

    /* Add the class, key_type, and value */
    crv = pk11_AddAttributeType(key,CKA_CLASS,&objclass,sizeof(CK_OBJECT_CLASS));
    if (crv != CKR_OK) { pk11_FreeObject(key); return crv; }
    crv = pk11_AddAttributeType(key,CKA_KEY_TYPE,&key_type,sizeof(CK_KEY_TYPE));
    if (crv != CKR_OK) { pk11_FreeObject(key); return crv; }
    crv = pk11_AddAttributeType(key,CKA_CLASS,&objclass,sizeof(CK_OBJECT_CLASS));
    if (crv != CKR_OK) { pk11_FreeObject(key); return crv; }
    crv = pk11_AddAttributeType(key,CKA_VALUE,buf,key_length);
    if (crv != CKR_OK) { pk11_FreeObject(key); return crv; }

    /* get the session */
    session = pk11_SessionFromHandle(hSession);
    if (session == NULL) {
	pk11_FreeObject(key);
        return CKR_SESSION_HANDLE_INVALID;
    }

    /*
     * handle the base object stuff
     */
    crv = pk11_handleObject(key,session);
    pk11_FreeSession(session);
    if (crv != CKR_OK) {
	pk11_FreeObject(key);
	return crv;
    }
    if (pk11_isTrue(key,CKA_SENSITIVE)) {
	pk11_forceAttribute(key,CKA_ALWAYS_SENSITIVE,&cktrue,sizeof(CK_BBOOL));
    }
    if (!pk11_isTrue(key,CKA_EXTRACTABLE)) {
	pk11_forceAttribute(key,CKA_NEVER_EXTRACTABLE,&cktrue,sizeof(CK_BBOOL));
    }

    *phKey = key->handle;
    return CKR_OK;
}



/* NSC_GenerateKeyPair generates a public-key/private-key pair, 
 * creating new key objects. */
CK_RV NSC_GenerateKeyPair (CK_SESSION_HANDLE hSession,
    CK_MECHANISM_PTR pMechanism, CK_ATTRIBUTE_PTR pPublicKeyTemplate,
    CK_ULONG ulPublicKeyAttributeCount, CK_ATTRIBUTE_PTR pPrivateKeyTemplate,
    CK_ULONG ulPrivateKeyAttributeCount, CK_OBJECT_HANDLE_PTR phPrivateKey,
    					CK_OBJECT_HANDLE_PTR phPublicKey)
{
    PK11Object *	publicKey,*privateKey;
    PK11Session *	session;
    CK_KEY_TYPE 	key_type;
    CK_RV 		crv 	= CKR_OK;
    CK_BBOOL 		cktrue 	= CK_TRUE;
    SECStatus 		rv;
    CK_OBJECT_CLASS 	pubClass = CKO_PUBLIC_KEY;
    CK_OBJECT_CLASS 	privClass = CKO_PRIVATE_KEY;
    int 		i;
    PK11Slot *		slot 	= pk11_SlotFromSessionHandle(hSession);

    /* RSA */
    int 		public_modulus_bits = 0;
    SECItem 		pubExp;
    RSAPrivateKey *	rsaPriv;

    /* DSA */
    PQGParams 		pqgParam;
    DHParams  		dhParam;
    DSAPrivateKey *	dsaPriv;

    /* Diffie Hellman */
    int 		private_value_bits = 0;
    DHPrivateKey *	dhPriv;

    /*
     * now lets create an object to hang the attributes off of
     */
    publicKey = pk11_NewObject(slot); /* fill in the handle later */
    if (publicKey == NULL) {
	return CKR_HOST_MEMORY;
    }

    /*
     * load the template values into the publicKey
     */
    for (i=0; i < (int) ulPublicKeyAttributeCount; i++) {
	if (pPublicKeyTemplate[i].type == CKA_MODULUS_BITS) {
	    public_modulus_bits = *(CK_ULONG *)pPublicKeyTemplate[i].pValue;
	    continue;
	}

	crv = pk11_AddAttributeType(publicKey,
				    pk11_attr_expand(&pPublicKeyTemplate[i]));
	if (crv != CKR_OK) break;
    }

    if (crv != CKR_OK) {
	pk11_FreeObject(publicKey);
	return CKR_HOST_MEMORY;
    }

    privateKey = pk11_NewObject(slot); /* fill in the handle later */
    if (privateKey == NULL) {
	pk11_FreeObject(publicKey);
	return CKR_HOST_MEMORY;
    }
    /*
     * now load the private key template
     */
    for (i=0; i < (int) ulPrivateKeyAttributeCount; i++) {
	if (pPrivateKeyTemplate[i].type == CKA_VALUE_BITS) {
	    private_value_bits = *(CK_ULONG *)pPrivateKeyTemplate[i].pValue;
	    continue;
	}

	crv = pk11_AddAttributeType(privateKey,
				    pk11_attr_expand(&pPrivateKeyTemplate[i]));
	if (crv != CKR_OK) break;
    }

    if (crv != CKR_OK) {
	pk11_FreeObject(publicKey);
	pk11_FreeObject(privateKey);
	return CKR_HOST_MEMORY;
    }
    pk11_DeleteAttributeType(privateKey,CKA_CLASS);
    pk11_DeleteAttributeType(privateKey,CKA_KEY_TYPE);
    pk11_DeleteAttributeType(privateKey,CKA_VALUE);
    pk11_DeleteAttributeType(publicKey,CKA_CLASS);
    pk11_DeleteAttributeType(publicKey,CKA_KEY_TYPE);
    pk11_DeleteAttributeType(publicKey,CKA_VALUE);

    /* Now Set up the parameters to generate the key (based on mechanism) */
    switch (pMechanism->mechanism) {
    case CKM_RSA_PKCS_KEY_PAIR_GEN:
	/* format the keys */
    	pk11_DeleteAttributeType(publicKey,CKA_MODULUS);
    	pk11_DeleteAttributeType(privateKey,CKA_NETSCAPE_DB);
    	pk11_DeleteAttributeType(privateKey,CKA_MODULUS);
    	pk11_DeleteAttributeType(privateKey,CKA_PRIVATE_EXPONENT);
    	pk11_DeleteAttributeType(privateKey,CKA_PUBLIC_EXPONENT);
    	pk11_DeleteAttributeType(privateKey,CKA_PRIME_1);
    	pk11_DeleteAttributeType(privateKey,CKA_PRIME_2);
    	pk11_DeleteAttributeType(privateKey,CKA_EXPONENT_1);
    	pk11_DeleteAttributeType(privateKey,CKA_EXPONENT_2);
    	pk11_DeleteAttributeType(privateKey,CKA_COEFFICIENT);
	key_type = CKK_RSA;
	if (public_modulus_bits == 0) {
	    crv = CKR_TEMPLATE_INCOMPLETE;
	    break;
	}

	/* extract the exponent */
	crv=pk11_Attribute2SSecItem(NULL,&pubExp,publicKey,CKA_PUBLIC_EXPONENT);
	if (crv != CKR_OK) break;
        crv = pk11_AddAttributeType(privateKey,CKA_PUBLIC_EXPONENT,
				    		    pk11_item_expand(&pubExp));
	if (crv != CKR_OK) {
	    PORT_Free(pubExp.data);
	    break;
	}

	rsaPriv = RSA_NewKey(public_modulus_bits, &pubExp);
	PORT_Free(pubExp.data);
	if (rsaPriv == NULL) {
	    crv = CKR_DEVICE_ERROR;
	    break;
	}
        /* now fill in the RSA dependent paramenters in the public key */
        crv = pk11_AddAttributeType(publicKey,CKA_MODULUS,
			   pk11_item_expand(&rsaPriv->modulus));
	if (crv != CKR_OK) goto kpg_done;
        /* now fill in the RSA dependent paramenters in the private key */
        crv = pk11_AddAttributeType(privateKey,CKA_NETSCAPE_DB,
			   pk11_item_expand(&rsaPriv->modulus));
	if (crv != CKR_OK) goto kpg_done;
        crv = pk11_AddAttributeType(privateKey,CKA_MODULUS,
			   pk11_item_expand(&rsaPriv->modulus));
	if (crv != CKR_OK) goto kpg_done;
        crv = pk11_AddAttributeType(privateKey,CKA_PRIVATE_EXPONENT,
			   pk11_item_expand(&rsaPriv->privateExponent));
	if (crv != CKR_OK) goto kpg_done;
        crv = pk11_AddAttributeType(privateKey,CKA_PRIME_1,
			   pk11_item_expand(&rsaPriv->prime1));
	if (crv != CKR_OK) goto kpg_done;
        crv = pk11_AddAttributeType(privateKey,CKA_PRIME_2,
			   pk11_item_expand(&rsaPriv->prime2));
	if (crv != CKR_OK) goto kpg_done;
        crv = pk11_AddAttributeType(privateKey,CKA_EXPONENT_1,
			   pk11_item_expand(&rsaPriv->exponent1));
	if (crv != CKR_OK) goto kpg_done;
        crv = pk11_AddAttributeType(privateKey,CKA_EXPONENT_2,
			   pk11_item_expand(&rsaPriv->exponent2));
	if (crv != CKR_OK) goto kpg_done;
        crv = pk11_AddAttributeType(privateKey,CKA_COEFFICIENT,
			   pk11_item_expand(&rsaPriv->coefficient));
kpg_done:
	/* Should zeroize the contents first, since this func doesn't. */
	PORT_FreeArena(rsaPriv->arena, PR_TRUE);
	break;
    case CKM_DSA_KEY_PAIR_GEN:
    	pk11_DeleteAttributeType(publicKey,CKA_VALUE);
    	pk11_DeleteAttributeType(privateKey,CKA_NETSCAPE_DB);
	pk11_DeleteAttributeType(privateKey,CKA_PRIME);
	pk11_DeleteAttributeType(privateKey,CKA_SUBPRIME);
	pk11_DeleteAttributeType(privateKey,CKA_BASE);
	key_type = CKK_DSA;

	/* extract the necessary paramters and copy them to the private key */
	crv=pk11_Attribute2SSecItem(NULL,&pqgParam.prime,publicKey,CKA_PRIME);
	if (crv != CKR_OK) break;
	crv=pk11_Attribute2SSecItem(NULL,&pqgParam.subPrime,publicKey,
	                            CKA_SUBPRIME);
	if (crv != CKR_OK) {
	    PORT_Free(pqgParam.prime.data);
	    break;
	}
	crv=pk11_Attribute2SSecItem(NULL,&pqgParam.base,publicKey,CKA_BASE);
	if (crv != CKR_OK) {
	    PORT_Free(pqgParam.prime.data);
	    PORT_Free(pqgParam.subPrime.data);
	    break;
	}
        crv = pk11_AddAttributeType(privateKey,CKA_PRIME,
				    pk11_item_expand(&pqgParam.prime));
	if (crv != CKR_OK) {
	    PORT_Free(pqgParam.prime.data);
	    PORT_Free(pqgParam.subPrime.data);
	    PORT_Free(pqgParam.base.data);
	    break;
	}
        crv = pk11_AddAttributeType(privateKey,CKA_SUBPRIME,
			    	    pk11_item_expand(&pqgParam.subPrime));
	if (crv != CKR_OK) {
	    PORT_Free(pqgParam.prime.data);
	    PORT_Free(pqgParam.subPrime.data);
	    PORT_Free(pqgParam.base.data);
	    break;
	}
        crv = pk11_AddAttributeType(privateKey,CKA_BASE,
			    	    pk11_item_expand(&pqgParam.base));
	if (crv != CKR_OK) {
	    PORT_Free(pqgParam.prime.data);
	    PORT_Free(pqgParam.subPrime.data);
	    PORT_Free(pqgParam.base.data);
	    break;
	}

	/* Generate the key */
	rv = DSA_NewKey(&pqgParam, &dsaPriv);

	PORT_Free(pqgParam.prime.data);
	PORT_Free(pqgParam.subPrime.data);
	PORT_Free(pqgParam.base.data);

	if (rv != SECSuccess) { crv = CKR_DEVICE_ERROR; break; }

	/* store the generated key into the attributes */
        crv = pk11_AddAttributeType(publicKey,CKA_VALUE,
			   pk11_item_expand(&dsaPriv->publicValue));
	if (crv != CKR_OK) goto dsagn_done;

        /* now fill in the RSA dependent paramenters in the private key */
        crv = pk11_AddAttributeType(privateKey,CKA_NETSCAPE_DB,
			   pk11_item_expand(&dsaPriv->publicValue));
	if (crv != CKR_OK) goto dsagn_done;
        crv = pk11_AddAttributeType(privateKey,CKA_VALUE,
			   pk11_item_expand(&dsaPriv->privateValue));

dsagn_done:
	/* should zeroize, since this function doesn't. */
	PORT_FreeArena(dsaPriv->params.arena, PR_TRUE);
	break;

    case CKM_DH_PKCS_KEY_PAIR_GEN:
	pk11_DeleteAttributeType(privateKey,CKA_PRIME);
	pk11_DeleteAttributeType(privateKey,CKA_BASE);
	pk11_DeleteAttributeType(privateKey,CKA_VALUE);
	key_type = CKK_DH;

	/* extract the necessary parameters and copy them to private keys */
        crv = pk11_Attribute2SSecItem(NULL, &dhParam.prime, publicKey, 
				      CKA_PRIME);
	if (crv != CKR_OK) break;
	crv = pk11_Attribute2SSecItem(NULL, &dhParam.base, publicKey, CKA_BASE);
	if (crv != CKR_OK) {
	  PORT_Free(dhParam.prime.data);
	  break;
	}
	crv = pk11_AddAttributeType(privateKey, CKA_PRIME, 
				    pk11_item_expand(&dhParam.prime));
	if (crv != CKR_OK) {
	  PORT_Free(dhParam.prime.data);
	  PORT_Free(dhParam.base.data);
	  break;
	}
	crv = pk11_AddAttributeType(privateKey, CKA_BASE, 
				    pk11_item_expand(&dhParam.base));
	if (crv != CKR_OK) goto dhgn_done;

	rv = DH_NewKey(&dhParam, &dhPriv);
	PORT_Free(dhParam.prime.data);
	PORT_Free(dhParam.base.data);
	if (rv != SECSuccess) { 
	  crv = CKR_DEVICE_ERROR;
	  break;
	}

	crv=pk11_AddAttributeType(publicKey, CKA_VALUE, 
				pk11_item_expand(&dhPriv->publicValue));
	if (crv != CKR_OK) goto dhgn_done;

	crv=pk11_AddAttributeType(privateKey, CKA_VALUE, 
			      pk11_item_expand(&dhPriv->privateValue));

dhgn_done:
	/* should zeroize, since this function doesn't. */
	PORT_FreeArena(dhPriv->arena, PR_TRUE);
	break;

    default:
	crv = CKR_MECHANISM_INVALID;
    }

    if (crv != CKR_OK) {
	pk11_FreeObject(privateKey);
	pk11_FreeObject(publicKey);
	return crv;
    }


    /* Add the class, key_type The loop lets us check errors blow out
     *  on errors and clean up at the bottom */
    session = NULL; /* make pedtantic happy... session cannot leave the*/
		    /* loop below NULL unless an error is set... */
    do {
	crv = pk11_AddAttributeType(privateKey,CKA_CLASS,&privClass,
						sizeof(CK_OBJECT_CLASS));
        if (crv != CKR_OK) break;
	crv = pk11_AddAttributeType(publicKey,CKA_CLASS,&pubClass,
						sizeof(CK_OBJECT_CLASS));
        if (crv != CKR_OK) break;
	crv = pk11_AddAttributeType(privateKey,CKA_KEY_TYPE,&key_type,
						sizeof(CK_KEY_TYPE));
        if (crv != CKR_OK) break;
	crv = pk11_AddAttributeType(publicKey,CKA_KEY_TYPE,&key_type,
						sizeof(CK_KEY_TYPE));
        if (crv != CKR_OK) break;
        session = pk11_SessionFromHandle(hSession);
        if (session == NULL) crv = CKR_SESSION_HANDLE_INVALID;
    } while (0);

    if (crv != CKR_OK) {
	 pk11_FreeObject(privateKey);
	 pk11_FreeObject(publicKey);
	 return crv;
    }

    /*
     * handle the base object cleanup for the public Key
     */
    crv = pk11_handleObject(publicKey,session);
    if (crv != CKR_OK) {
        pk11_FreeSession(session);
	pk11_FreeObject(privateKey);
	pk11_FreeObject(publicKey);
	return crv;
    }

    /*
     * handle the base object cleanup for the private Key
     * If we have any problems, we destroy the public Key we've
     * created and linked.
     */
    crv = pk11_handleObject(privateKey,session);
    pk11_FreeSession(session);
    if (crv != CKR_OK) {
	pk11_FreeObject(privateKey);
	NSC_DestroyObject(hSession,publicKey->handle);
	return crv;
    }
    if (pk11_isTrue(privateKey,CKA_SENSITIVE)) {
	pk11_forceAttribute(privateKey,CKA_ALWAYS_SENSITIVE,
						&cktrue,sizeof(CK_BBOOL));
    }
    if (pk11_isTrue(publicKey,CKA_SENSITIVE)) {
	pk11_forceAttribute(publicKey,CKA_ALWAYS_SENSITIVE,
						&cktrue,sizeof(CK_BBOOL));
    }
    if (!pk11_isTrue(privateKey,CKA_EXTRACTABLE)) {
	pk11_forceAttribute(privateKey,CKA_NEVER_EXTRACTABLE,
						&cktrue,sizeof(CK_BBOOL));
    }
    if (!pk11_isTrue(publicKey,CKA_EXTRACTABLE)) {
	pk11_forceAttribute(publicKey,CKA_NEVER_EXTRACTABLE,
						&cktrue,sizeof(CK_BBOOL));
    }
    *phPrivateKey = privateKey->handle;
    *phPublicKey = publicKey->handle;

    return CKR_OK;
}

static SECItem *pk11_PackagePrivateKey(PK11Object *key)
{
    SECKEYLowPrivateKey *lk = NULL;
    SECKEYPrivateKeyInfo *pki = NULL;
    PK11Attribute *attribute = NULL;
    PLArenaPool *arena = NULL;
    SECOidTag algorithm = SEC_OID_UNKNOWN;
    void *dummy, *param = NULL;
    SECStatus rv = SECSuccess;
    SECItem *encodedKey = NULL;

    if(!key) {
	return NULL;
    }

    attribute = pk11_FindAttribute(key, CKA_KEY_TYPE);
    if(!attribute) {
	return NULL;
    }

    lk = pk11_GetPrivKey(key, *(CK_KEY_TYPE *)attribute->attrib.pValue);
    pk11_FreeAttribute(attribute);
    if(!lk) {
	return NULL;
    }

    arena = PORT_NewArena(2048); 	/* XXX different size? */
    if(!arena) {
	rv = SECFailure;
	goto loser;
    }

    pki = (SECKEYPrivateKeyInfo*)PORT_ArenaZAlloc(arena, 
						sizeof(SECKEYPrivateKeyInfo));
    if(!pki) {
	rv = SECFailure;
	goto loser;
    }
    pki->arena = arena;

    param = NULL;
    switch(lk->keyType) {
	case rsaKey:
	    dummy = SEC_ASN1EncodeItem(arena, &pki->privateKey, lk,
				       SECKEY_RSAPrivateKeyTemplate);
	    algorithm = SEC_OID_PKCS1_RSA_ENCRYPTION;
	    break;
	case dsaKey:
	    dummy = SEC_ASN1EncodeItem(arena, &pki->privateKey, lk,
				       SECKEY_DSAPrivateKeyExportTemplate);
	    param = SEC_ASN1EncodeItem(NULL, NULL, &(lk->u.dsa.params),
				       SECKEY_PQGParamsTemplate);
	    algorithm = SEC_OID_ANSIX9_DSA_SIGNATURE;
	    break;
	case fortezzaKey:
	case dhKey:
	default:
	    dummy = NULL;
	    break;
    }
 
    if(!dummy || ((lk->keyType == dsaKey) && !param)) {
	goto loser;
    }

    rv = SECOID_SetAlgorithmID(arena, &pki->algorithm, algorithm, 
			       (SECItem*)param);
    if(rv != SECSuccess) {
	rv = SECFailure;
	goto loser;
    }

    dummy = SEC_ASN1EncodeInteger(arena, &pki->version,
				  SEC_PRIVATE_KEY_INFO_VERSION);
    if(!dummy) {
	rv = SECFailure;
	goto loser;
    }

    encodedKey = SEC_ASN1EncodeItem(NULL, NULL, pki, 
				    SECKEY_PrivateKeyInfoTemplate);

loser:
    if(arena) {
	PORT_FreeArena(arena, PR_TRUE);
    }

    if(lk && (lk != key->objectInfo)) {
	SECKEY_LowDestroyPrivateKey(lk);
    }
 
    if(param) {
	SECITEM_ZfreeItem((SECItem*)param, PR_TRUE);
    }

    if(rv != SECSuccess) {
	return NULL;
    }

    return encodedKey;
}
    
/* it doesn't matter yet, since we colapse error conditions in the
 * level above, but we really should map those few key error differences */
CK_RV pk11_mapWrap(CK_RV crv) { return crv; }

/* NSC_WrapKey wraps (i.e., encrypts) a key. */
CK_RV NSC_WrapKey(CK_SESSION_HANDLE hSession,
    CK_MECHANISM_PTR pMechanism, CK_OBJECT_HANDLE hWrappingKey,
    CK_OBJECT_HANDLE hKey, CK_BYTE_PTR pWrappedKey,
					 CK_ULONG_PTR pulWrappedKeyLen)
{
    PK11Session *session;
    PK11Attribute *attribute;
    PK11Object *key;
    CK_RV crv;
    PRBool isLynks = PR_FALSE;
    CK_ULONG len = 0;

    session = pk11_SessionFromHandle(hSession);
    if (session == NULL) {
    	return CKR_SESSION_HANDLE_INVALID;
    }

    key = pk11_ObjectFromHandle(hKey,session);
    pk11_FreeSession(session);
    if (key == NULL) {
	return CKR_KEY_HANDLE_INVALID;
    }

    switch(key->objclass) {
	case CKO_SECRET_KEY:
	    attribute = pk11_FindAttribute(key,CKA_VALUE);

	    if (attribute == NULL) {
		crv = CKR_KEY_TYPE_INCONSISTENT;
		break;
	    }
	    if (pMechanism->mechanism == CKM_KEY_WRAP_LYNKS) {
		isLynks = PR_TRUE;
		pMechanism->mechanism = CKM_DES_ECB;
		len = *pulWrappedKeyLen;
	    }
     
	    crv = pk11_EncryptInit(hSession, pMechanism, hWrappingKey, 
						CKA_WRAP, PK11_ENCRYPT);
	    if (crv != CKR_OK) {
		pk11_FreeAttribute(attribute);
		break;
	    }
	    crv = NSC_Encrypt(hSession, (CK_BYTE_PTR)attribute->attrib.pValue, 
		    attribute->attrib.ulValueLen,pWrappedKey,pulWrappedKeyLen);

	    if (isLynks && (crv == CKR_OK)) {
		unsigned char buf[2];
		crv = pk11_calcLynxChecksum(hSession,hWrappingKey,buf,
			(unsigned char*)attribute->attrib.pValue,
			attribute->attrib.ulValueLen);
		if (len >= 10) {
		    pWrappedKey[8] = buf[0];
		    pWrappedKey[9] = buf[1];
		    *pulWrappedKeyLen = 10;
		}
	    }
	    pk11_FreeAttribute(attribute);
	    break;

	case CKO_PRIVATE_KEY:
	    {
		SECItem *bpki = pk11_PackagePrivateKey(key);

		if(!bpki) {
		    crv = CKR_KEY_TYPE_INCONSISTENT;
		    break;
		}

		crv = pk11_EncryptInit(hSession, pMechanism, hWrappingKey,
						CKA_WRAP, PK11_ENCRYPT);
		if(crv != CKR_OK) {
		    SECITEM_ZfreeItem(bpki, PR_TRUE);
		    crv = CKR_KEY_TYPE_INCONSISTENT;
		    break;
		}

		crv = NSC_Encrypt(hSession, bpki->data, bpki->len,
					pWrappedKey, pulWrappedKeyLen);
		SECITEM_ZfreeItem(bpki, PR_TRUE);
		break;
	    }

	default:
	    crv = CKR_KEY_TYPE_INCONSISTENT;
	    break;
    }
    pk11_FreeObject(key);

    return pk11_mapWrap(crv);
}

/*
 * import a pprivate key info into the desired slot
 */
static SECStatus
pk11_unwrapPrivateKey(PK11Object *key, SECItem *bpki)
{
    CK_BBOOL cktrue = CK_TRUE; 
    CK_KEY_TYPE keyType = CKK_RSA;
    SECStatus rv = SECFailure;
    const SEC_ASN1Template *keyTemplate, *paramTemplate;
    void *paramDest = NULL;
    PLArenaPool *arena;
    SECKEYLowPrivateKey *lpk = NULL;
    SECKEYPrivateKeyInfo *pki = NULL;
    SECItem *ck_id = NULL;
    CK_RV crv = CKR_KEY_TYPE_INCONSISTENT;

    arena = PORT_NewArena(2048);
    if(!arena) {
	return SECFailure;
    }

    pki = (SECKEYPrivateKeyInfo*)PORT_ArenaZAlloc(arena, 
						sizeof(SECKEYPrivateKeyInfo));
    if(!pki) {
	PORT_FreeArena(arena, PR_TRUE);
	return SECFailure;
    }

    if(SEC_ASN1DecodeItem(arena, pki, SECKEY_PrivateKeyInfoTemplate, bpki) 
				!= SECSuccess) {
	PORT_FreeArena(arena, PR_FALSE);
	return SECFailure;
    }

    lpk = (SECKEYLowPrivateKey *)PORT_ArenaZAlloc(arena,
						  sizeof(SECKEYLowPrivateKey));
    if(lpk == NULL) {
	goto loser;
    }
    lpk->arena = arena;

    switch(SECOID_GetAlgorithmTag(&pki->algorithm)) {
	case SEC_OID_PKCS1_RSA_ENCRYPTION:
	    keyTemplate = SECKEY_RSAPrivateKeyTemplate;
	    paramTemplate = NULL;
	    paramDest = NULL;
	    lpk->keyType = rsaKey;
	    break;
	case SEC_OID_ANSIX9_DSA_SIGNATURE:
	    keyTemplate = SECKEY_DSAPrivateKeyExportTemplate;
	    paramTemplate = SECKEY_PQGParamsTemplate;
	    paramDest = &(lpk->u.dsa.params);
	    lpk->keyType = dsaKey;
	    break;
	/* case dhKey: */
	/* case fortezzaKey: */
	default:
	    keyTemplate = NULL;
	    paramTemplate = NULL;
	    paramDest = NULL;
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

    rv = SECFailure;

    switch (lpk->keyType) {
        case rsaKey:
	    keyType = CKK_RSA;
	    if(pk11_hasAttribute(key, CKA_NETSCAPE_DB)) {
		pk11_DeleteAttributeType(key, CKA_NETSCAPE_DB);
	    }
	    crv = pk11_AddAttributeType(key, CKA_KEY_TYPE, &keyType, 
					sizeof(keyType));
	    if(crv != CKR_OK) break;
	    crv = pk11_AddAttributeType(key, CKA_UNWRAP, &cktrue, 
					sizeof(CK_BBOOL));
	    if(crv != CKR_OK) break;
	    crv = pk11_AddAttributeType(key, CKA_DECRYPT, &cktrue, 
					sizeof(CK_BBOOL));
	    if(crv != CKR_OK) break;
	    crv = pk11_AddAttributeType(key, CKA_SIGN, &cktrue, 
					sizeof(CK_BBOOL));
	    if(crv != CKR_OK) break;
	    crv = pk11_AddAttributeType(key, CKA_SIGN_RECOVER, &cktrue, 
				    sizeof(CK_BBOOL));
	    if(crv != CKR_OK) break;
	    crv = pk11_AddAttributeType(key, CKA_MODULUS, 
				pk11_item_expand(&lpk->u.rsa.modulus));
	    if(crv != CKR_OK) break;
	    crv = pk11_AddAttributeType(key, CKA_PUBLIC_EXPONENT, 
	     			pk11_item_expand(&lpk->u.rsa.publicExponent));
	    if(crv != CKR_OK) break;
	    crv = pk11_AddAttributeType(key, CKA_PRIVATE_EXPONENT, 
	     			pk11_item_expand(&lpk->u.rsa.privateExponent));
	    if(crv != CKR_OK) break;
	    crv = pk11_AddAttributeType(key, CKA_PRIME_1, 
				pk11_item_expand(&lpk->u.rsa.prime1));
	    if(crv != CKR_OK) break;
	    crv = pk11_AddAttributeType(key, CKA_PRIME_2, 
	     			pk11_item_expand(&lpk->u.rsa.prime2));
	    if(crv != CKR_OK) break;
	    crv = pk11_AddAttributeType(key, CKA_EXPONENT_1, 
	     			pk11_item_expand(&lpk->u.rsa.exponent1));
	    if(crv != CKR_OK) break;
	    crv = pk11_AddAttributeType(key, CKA_EXPONENT_2, 
	     			pk11_item_expand(&lpk->u.rsa.exponent2));
	    if(crv != CKR_OK) break;
	    crv = pk11_AddAttributeType(key, CKA_COEFFICIENT, 
	     			pk11_item_expand(&lpk->u.rsa.coefficient));
	    break;
        case dsaKey:
	    keyType = CKK_DSA;
	    crv = (pk11_hasAttribute(key, CKA_NETSCAPE_DB)) ? CKR_OK :
						CKR_KEY_TYPE_INCONSISTENT;
	    if(crv != CKR_OK) break;
	    crv = pk11_AddAttributeType(key, CKA_KEY_TYPE, &keyType, 
						sizeof(keyType));
	    if(crv != CKR_OK) break;
	    crv = pk11_AddAttributeType(key, CKA_SIGN, &cktrue, 
						sizeof(CK_BBOOL));
	    if(crv != CKR_OK) break;
	    crv = pk11_AddAttributeType(key, CKA_SIGN_RECOVER, &cktrue, 
						sizeof(CK_BBOOL)); 
	    if(crv != CKR_OK) break;
	    crv = pk11_AddAttributeType(key, CKA_PRIME,    
				    pk11_item_expand(&lpk->u.dsa.params.prime));
	    if(crv != CKR_OK) break;
	    crv = pk11_AddAttributeType(key, CKA_SUBPRIME,
				 pk11_item_expand(&lpk->u.dsa.params.subPrime));
	    if(crv != CKR_OK) break;
	    crv = pk11_AddAttributeType(key, CKA_BASE,  
				    pk11_item_expand(&lpk->u.dsa.params.base));
	    if(crv != CKR_OK) break;
	    crv = pk11_AddAttributeType(key, CKA_VALUE, 
			pk11_item_expand(&lpk->u.dsa.privateValue));
	    if(crv != CKR_OK) break;
	    break;
#ifdef notdef
        case dhKey:
	    template = dhTemplate;
	    templateCount = sizeof(dhTemplate)/sizeof(CK_ATTRIBUTE);
	    keyType = CKK_DH;
	    break;
#endif
	/* what about fortezza??? */
	default:
	    crv = CKR_KEY_TYPE_INCONSISTENT;
	    break;
    }

loser:
    if(ck_id) {
	SECITEM_ZfreeItem(ck_id, PR_TRUE);
    }

    if(lpk) {
	SECKEY_LowDestroyPrivateKey(lpk);
    }

    if(crv != CKR_OK) {
	return SECFailure;
    }

    return SECSuccess;
}


/* NSC_UnwrapKey unwraps (decrypts) a wrapped key, creating a new key object. */
CK_RV NSC_UnwrapKey(CK_SESSION_HANDLE hSession,
    CK_MECHANISM_PTR pMechanism, CK_OBJECT_HANDLE hUnwrappingKey,
    CK_BYTE_PTR pWrappedKey, CK_ULONG ulWrappedKeyLen,
    CK_ATTRIBUTE_PTR pTemplate, CK_ULONG ulAttributeCount,
						 CK_OBJECT_HANDLE_PTR phKey)
{
    PK11Object *key = NULL;
    PK11Session *session;
    int key_length = 0;
    unsigned char * buf = NULL;
    CK_RV crv = CKR_OK;
    int i;
    CK_ULONG bsize = ulWrappedKeyLen;
    PK11Slot *slot = pk11_SlotFromSessionHandle(hSession);
    PK11Attribute *attr = NULL;
    SECItem bpki;
    CK_OBJECT_CLASS target_type = CKO_SECRET_KEY;
    PRBool isLynks = PR_FALSE;

    /*
     * now lets create an object to hang the attributes off of
     */
    key = pk11_NewObject(slot); /* fill in the handle later */
    if (key == NULL) {
	return CKR_HOST_MEMORY;
    }

    /*
     * load the template values into the object
     */
    for (i=0; i < (int) ulAttributeCount; i++) {
	if (pTemplate[i].type == CKA_VALUE_LEN) {
	    key_length = *(CK_ULONG *)pTemplate[i].pValue;
	    continue;
	}
        if (pTemplate[i].type == CKA_CLASS) {
	    target_type = *(CK_OBJECT_CLASS *)pTemplate[i].pValue;
	}
	crv = pk11_AddAttributeType(key,pk11_attr_expand(&pTemplate[i]));
	if (crv != CKR_OK) break;
    }
    if (crv != CKR_OK) {
	pk11_FreeObject(key);
	return crv;
    }

    /* LYNKS is a special key wrapping mechanism */
    if (pMechanism->mechanism == CKM_KEY_WRAP_LYNKS) {
	isLynks = PR_TRUE;
	pMechanism->mechanism = CKM_DES_ECB;
	ulWrappedKeyLen -= 2; /* don't decrypt the checksum */
    }

    crv = pk11_DecryptInit(hSession,pMechanism,hUnwrappingKey,CKA_UNWRAP,
								PK11_DECRYPT);
    if (crv != CKR_OK) {
	pk11_FreeObject(key);
	return pk11_mapWrap(crv);
    }

    /* allocate the buffer to decrypt into 
     * this assumes the unwrapped key is never larger than the
     * wrapped key. For all the mechanisms we support this is true */
    buf = (unsigned char *)PORT_Alloc( ulWrappedKeyLen);
    bsize = ulWrappedKeyLen;

    crv = NSC_Decrypt(hSession, pWrappedKey, ulWrappedKeyLen, buf, &bsize);
    if (crv != CKR_OK) {
	pk11_FreeObject(key);
	PORT_Free(buf);
	return pk11_mapWrap(crv);
    }

    switch(target_type) {
	case CKO_SECRET_KEY:
	    if (!pk11_hasAttribute(key,CKA_KEY_TYPE)) {
		crv = CKR_TEMPLATE_INCOMPLETE;
		break;
	    }

	    /* verify the Lynx checksum */
	    if (isLynks) {
		unsigned char checkSum[2];
		crv = pk11_calcLynxChecksum(hSession,hUnwrappingKey,checkSum,
								buf,bsize);
		if (crv != CKR_OK) break;
		if ((ulWrappedKeyLen != 8) || (pWrappedKey[8] != checkSum[0]) 
			|| (pWrappedKey[9] != checkSum[1])) {
		    crv = CKR_WRAPPED_KEY_INVALID;
		    break;
		}
	    }

	    if(key_length == 0) {
		key_length = bsize;
	    }
	    if (key_length > MAX_KEY_LEN) {
		crv = CKR_TEMPLATE_INCONSISTENT;
		break;
	    }
    
	    /* add the value */
	    crv = pk11_AddAttributeType(key,CKA_VALUE,buf,key_length);
	    break;
	case CKO_PRIVATE_KEY:
	    bpki.data = (unsigned char *)buf;
	    bpki.len = bsize;
	    crv = CKR_OK;
	    if(pk11_unwrapPrivateKey(key, &bpki) != SECSuccess) {
		crv = CKR_TEMPLATE_INCOMPLETE;
	    }
	    break;
	default:
	    crv = CKR_TEMPLATE_INCONSISTENT;
	    break;
    }

    PORT_ZFree(buf, bsize);
    if (crv != CKR_OK) { pk11_FreeObject(key); return crv; }

    /* get the session */
    session = pk11_SessionFromHandle(hSession);
    if (session == NULL) {
	pk11_FreeObject(key);
        return CKR_SESSION_HANDLE_INVALID;
    }

    /*
     * handle the base object stuff
     */
    crv = pk11_handleObject(key,session);
    pk11_FreeSession(session);
    if (crv != CKR_OK) {
	pk11_FreeObject(key);
	return crv;
    }

    *phKey = key->handle;
    return CKR_OK;

}

/*
 * The SSL key gen mechanism create's lots of keys. This function handles the
 * details of each of these key creation.
 */
static CK_RV
pk11_buildSSLKey(CK_SESSION_HANDLE hSession, PK11Object *baseKey, 
    PRBool isMacKey, unsigned char *keyBlock, unsigned int keySize,
						 CK_OBJECT_HANDLE *keyHandle)
{
    PK11Object *key;
    PK11Session *session;
    CK_KEY_TYPE keyType = CKK_GENERIC_SECRET;
    CK_BBOOL cktrue = CK_TRUE;
    CK_BBOOL ckfalse = CK_FALSE;
    CK_RV crv = CKR_HOST_MEMORY;

    /*
     * now lets create an object to hang the attributes off of
     */
    *keyHandle = CK_INVALID_KEY;
    key = pk11_NewObject(baseKey->slot); 
    if (key == NULL) return CKR_HOST_MEMORY;
    key->wasDerived = PR_TRUE;

    crv = pk11_CopyObject(key,baseKey);
    if (crv != CKR_OK) goto loser;
    if (isMacKey) {
	crv = pk11_forceAttribute(key,CKA_KEY_TYPE,&keyType,sizeof(keyType));
	if (crv != CKR_OK) goto loser;
	crv = pk11_forceAttribute(key,CKA_DERIVE,&cktrue,sizeof(CK_BBOOL));
	if (crv != CKR_OK) goto loser;
	crv = pk11_forceAttribute(key,CKA_ENCRYPT,&ckfalse,sizeof(CK_BBOOL));
	if (crv != CKR_OK) goto loser;
	crv = pk11_forceAttribute(key,CKA_DECRYPT,&ckfalse,sizeof(CK_BBOOL));
	if (crv != CKR_OK) goto loser;
	crv = pk11_forceAttribute(key,CKA_SIGN,&cktrue,sizeof(CK_BBOOL));
	if (crv != CKR_OK) goto loser;
	crv = pk11_forceAttribute(key,CKA_VERIFY,&cktrue,sizeof(CK_BBOOL));
	if (crv != CKR_OK) goto loser;
	crv = pk11_forceAttribute(key,CKA_WRAP,&ckfalse,sizeof(CK_BBOOL));
	if (crv != CKR_OK) goto loser;
	crv = pk11_forceAttribute(key,CKA_UNWRAP,&ckfalse,sizeof(CK_BBOOL));
	if (crv != CKR_OK) goto loser;
    }
    crv = pk11_forceAttribute(key,CKA_VALUE,keyBlock,keySize);
    if (crv != CKR_OK) goto loser;

    /* get the session */
    crv = CKR_HOST_MEMORY;
    session = pk11_SessionFromHandle(hSession);
    if (session == NULL) { goto loser; }

    crv = pk11_handleObject(key,session);
    pk11_FreeSession(session);
    if (crv == CKR_OK) {
	*keyHandle = key->handle;
	return crv;
    }
loser:
    if (key) pk11_FreeObject(key);
    return crv;
}

/*
 * if there is an error, we need to free the keys we already created in SSL
 * This is the routine that will do it..
 */
static void
pk11_freeSSLKeys(CK_SESSION_HANDLE session,
				CK_SSL3_KEY_MAT_OUT *returnedMaterial ) {
	if (returnedMaterial->hClientMacSecret != CK_INVALID_KEY) {
	   NSC_DestroyObject(session,returnedMaterial->hClientMacSecret);
	}
	if (returnedMaterial->hServerMacSecret != CK_INVALID_KEY) {
	   NSC_DestroyObject(session, returnedMaterial->hServerMacSecret);
	}
	if (returnedMaterial->hClientKey != CK_INVALID_KEY) {
	   NSC_DestroyObject(session, returnedMaterial->hClientKey);
	}
	if (returnedMaterial->hServerKey != CK_INVALID_KEY) {
	   NSC_DestroyObject(session, returnedMaterial->hServerKey);
	}
}

/*
 * when deriving from sensitive and extractable keys, we need to preserve some
 * of the semantics in the derived key. This helper routine maintains these
 * semantics.
 */
static CK_RV
pk11_DeriveSensitiveCheck(PK11Object *baseKey,PK11Object *destKey) {
    PRBool hasSensitive;
    PRBool sensitive;
    PRBool hasExtractable;
    PRBool extractable;
    CK_RV crv = PR_TRUE;
    PK11Attribute *att;

    hasSensitive = PR_FALSE;
    att = pk11_FindAttribute(destKey,CKA_SENSITIVE);
    if (att) {
        hasSensitive = PR_FALSE;
	sensitive = (PRBool) *(CK_BBOOL *)att->attrib.pValue;
	pk11_FreeAttribute(att);
    }

    hasExtractable = PR_FALSE;
    att = pk11_FindAttribute(destKey,CKA_EXTRACTABLE);
    if (att) {
        hasExtractable = PR_FALSE;
	extractable = (PRBool) *(CK_BBOOL *)att->attrib.pValue;
	pk11_FreeAttribute(att);
    }


    /* don't make a key more accessible */
    if (pk11_isTrue(baseKey,CKA_SENSITIVE) && hasSensitive && 
						(sensitive == PR_FALSE)) {
	return CKR_KEY_FUNCTION_NOT_PERMITTED;
    }
    if (!pk11_isTrue(baseKey,CKA_EXTRACTABLE) && hasExtractable && 
						(extractable == PR_TRUE)) {
	return CKR_KEY_FUNCTION_NOT_PERMITTED;
    }

    /* inherit parent's sensitivity */
    if (!hasSensitive) {
        att = pk11_FindAttribute(baseKey,CKA_SENSITIVE);
	if (att == NULL) return CKR_KEY_TYPE_INCONSISTENT;
	crv = pk11_defaultAttribute(destKey,pk11_attr_expand(&att->attrib));
	pk11_FreeAttribute(att);
	if (crv != CKR_OK) return crv;
    }
    if (!hasExtractable) {
        att = pk11_FindAttribute(baseKey,CKA_EXTRACTABLE);
	if (att == NULL) return CKR_KEY_TYPE_INCONSISTENT;
	crv = pk11_defaultAttribute(destKey,pk11_attr_expand(&att->attrib));
	pk11_FreeAttribute(att);
	if (crv != CKR_OK) return crv;
    }

    /* we should inherit the parent's always extractable/ never sensitive info,
     * but handleObject always forces this attributes, so we would need to do
     * something special. */
    return CKR_OK;
}

/*
 * make known fixed PKCS #11 key types to their sizes in bytes
 */	
static unsigned long
pk11_MapKeySize(CK_KEY_TYPE keyType) {
    switch (keyType) {
    case CKK_CDMF:
	return 8;
    case CKK_DES:
	return 8;
    case CKK_DES2:
	return 16;
    case CKK_DES3:
	return 24;
    /* IDEA and CAST need to be added */
    default:
	break;
    }
    return 0;
}

#define PHASH_STATE_MAX_LEN 20

/* TLS P_hash function */
static SECStatus
pk11_P_hash(SECOidTag alg, const SECItem *secret, const char *label, 
	SECItem *seed, SECItem *result)
{
    unsigned char state[PHASH_STATE_MAX_LEN];
    unsigned char outbuf[PHASH_STATE_MAX_LEN];
    unsigned int state_len = 0, label_len = 0, outbuf_len = 0, chunk_size;
    unsigned int remaining;
    unsigned char *res;
    SECStatus status;
    HMACContext *cx;
    SECStatus rv = SECFailure;

    PORT_Assert((secret != NULL) && (secret->data != NULL || !secret->len));
    PORT_Assert((seed != NULL) && (seed->data != NULL));
    PORT_Assert((result != NULL) && (result->data != NULL));

    remaining = result->len;
    res = result->data;

    if (label != NULL)
	label_len = PORT_Strlen(label);

    cx = HMAC_Create(alg, secret->data, secret->len);
    if (cx == NULL)
	goto loser;

    /* initialize the state = A(1) = HMAC_hash(secret, seed) */
    HMAC_Begin(cx);
    HMAC_Update(cx, (unsigned char *)label, label_len);
    HMAC_Update(cx, seed->data, seed->len);
    status = HMAC_Finish(cx, state, &state_len, PHASH_STATE_MAX_LEN);
    if (status != SECSuccess)
	goto loser;

    /* generate a block at a time until we're done */
    while (remaining > 0) {

	HMAC_Begin(cx);
	HMAC_Update(cx, state, state_len);
	if (label_len)
	    HMAC_Update(cx, (unsigned char *)label, label_len);
	HMAC_Update(cx, seed->data, seed->len);
	status = HMAC_Finish(cx, outbuf, &outbuf_len, PHASH_STATE_MAX_LEN);
	if (status != SECSuccess)
	    goto loser;

        /* Update the state = A(i) = HMAC_hash(secret, A(i-1)) */
	HMAC_Begin(cx); 
	HMAC_Update(cx, state, state_len); 
	status = HMAC_Finish(cx, state, &state_len, PHASH_STATE_MAX_LEN);
	if (status != SECSuccess)
	    goto loser;

	chunk_size = PR_MIN(outbuf_len, remaining);
	PORT_Memcpy(res, &outbuf, chunk_size);
	res += chunk_size;
	remaining -= chunk_size;
    }

    rv = SECSuccess;

loser:
    /* if (cx) HMAC_Destroy(cx); */
    /* clear out state so it's not left on the stack */
    if (cx) HMAC_Destroy(cx);
    PORT_Memset(state, 0, sizeof(state));
    PORT_Memset(outbuf, 0, sizeof(outbuf));
    return rv;
}

static SECStatus
pk11_PRF(const SECItem *secret, const char *label, SECItem *seed, 
         SECItem *result)
{
    SECStatus rv = SECFailure, status;
    unsigned int i;
    SECItem tmp = { siBuffer, NULL, 0};
    SECItem S1;
    SECItem S2;

    PORT_Assert((secret != NULL) && (secret->data != NULL || !secret->len));
    PORT_Assert((seed != NULL) && (seed->data != NULL));
    PORT_Assert((result != NULL) && (result->data != NULL));

    S1.type = siBuffer;
    S1.len  = (secret->len / 2) + (secret->len & 1);
    S1.data = secret->data;

    S2.type = siBuffer;
    S2.len  = S1.len;
    S2.data = secret->data + (secret->len - S2.len);

    tmp.data = (unsigned char*)PORT_Alloc(result->len);
    if (tmp.data == NULL)
	goto loser;
    tmp.len = result->len;

    status = pk11_P_hash(SEC_OID_MD5, &S1, label, seed, result);
    if (status != SECSuccess)
	goto loser;

    status = pk11_P_hash(SEC_OID_SHA1, &S2, label, seed, &tmp);
    if (status != SECSuccess)
	goto loser;

    for (i = 0; i < result->len; i++)
	result->data[i] ^= tmp.data[i];

    rv = SECSuccess;

loser:
    if (tmp.data != NULL)
	PORT_ZFree(tmp.data, tmp.len);
    return rv;
}

/*
 * SSL Key generation given pre master secret
 */
static char *mixers[] = { "A", "BB", "CCC", "DDDD", "EEEEE", "FFFFFF", "GGGGGGG"};
#define NUM_MIXERS 7
#define SSL3_PMS_LENGTH 48
#define SSL3_MASTER_SECRET_LENGTH 48


/* NSC_DeriveKey derives a key from a base key, creating a new key object. */
CK_RV NSC_DeriveKey( CK_SESSION_HANDLE hSession,
	 CK_MECHANISM_PTR pMechanism, CK_OBJECT_HANDLE hBaseKey,
	 CK_ATTRIBUTE_PTR pTemplate, CK_ULONG ulAttributeCount, 
						CK_OBJECT_HANDLE_PTR phKey)
{
    PK11Session *   session;
    PK11Slot    *   slot	= pk11_SlotFromSessionHandle(hSession);
    PK11Object  *   key;
    PK11Object  *   sourceKey;
    PK11Attribute * att;
    PK11Attribute * att2;
    unsigned char * buf;
    SHA1Context *   sha;
    MD5Context *    md5;
    MD2Context *    md2;
    CK_ULONG        macSize;
    CK_ULONG        tmpKeySize;
    CK_ULONG        IVSize;
    CK_ULONG        keySize	= 0;
    CK_RV           crv 	= CKR_OK;
    CK_BBOOL        cktrue	= CK_TRUE;
    CK_KEY_TYPE     keyType	= CKK_GENERIC_SECRET;
    CK_OBJECT_CLASS classType	= CKO_SECRET_KEY;
    CK_KEY_DERIVATION_STRING_DATA *stringPtr;
    PRBool          isTLS = PR_FALSE;
    PRBool          isDH = PR_FALSE;
    SECStatus       rv;
    int             i;
    unsigned int    outLen;
    unsigned char   sha_out[SHA1_LENGTH];
    unsigned char   key_block[NUM_MIXERS * MD5_LENGTH];
    unsigned char   key_block2[MD5_LENGTH];

    /*
     * now lets create an object to hang the attributes off of
     */
    if (phKey) *phKey = CK_INVALID_KEY;

    key = pk11_NewObject(slot); /* fill in the handle later */
    if (key == NULL) {
	return CKR_HOST_MEMORY;
    }

    /*
     * load the template values into the object
     */
    for (i=0; i < (int) ulAttributeCount; i++) {
	crv = pk11_AddAttributeType(key,pk11_attr_expand(&pTemplate[i]));
	if (crv != CKR_OK) break;

	if (pTemplate[i].type == CKA_KEY_TYPE) {
	    keyType = *(CK_KEY_TYPE *)pTemplate[i].pValue;
	}
	if (pTemplate[i].type == CKA_VALUE_LEN) {
	    keySize = *(CK_ULONG *)pTemplate[i].pValue;
	}
    }
    if (crv != CKR_OK) { pk11_FreeObject(key); return crv; }

    if (keySize == 0) {
	keySize = pk11_MapKeySize(keyType);
    }

    /* Derive can only create SECRET KEY's currently... */
    classType = CKO_SECRET_KEY;
    crv = pk11_forceAttribute (key,CKA_CLASS,&classType,sizeof(classType));
    if (crv != CKR_OK) {
	pk11_FreeObject(key);
	return crv;
    }

    /* look up the base key we're deriving with */ 
    session = pk11_SessionFromHandle(hSession);
    if (session == NULL) {
	pk11_FreeObject(key);
        return CKR_SESSION_HANDLE_INVALID;
    }

    sourceKey = pk11_ObjectFromHandle(hBaseKey,session);
    pk11_FreeSession(session);
    if (sourceKey == NULL) {
	pk11_FreeObject(key);
        return CKR_KEY_HANDLE_INVALID;
    }

    /* don't use key derive to expose sensitive keys */
    crv = pk11_DeriveSensitiveCheck(sourceKey,key);
    if (crv != CKR_OK) {
	pk11_FreeObject(key);
	pk11_FreeObject(sourceKey);
        return crv;
    }

    /* get the value of the base key */
    att = pk11_FindAttribute(sourceKey,CKA_VALUE);
    if (att == NULL) {
	pk11_FreeObject(key);
	pk11_FreeObject(sourceKey);
        return CKR_KEY_HANDLE_INVALID;
    }

    switch (pMechanism->mechanism) {
    /*
     * generate the master secret 
     */
    case CKM_TLS_MASTER_KEY_DERIVE:
    case CKM_TLS_MASTER_KEY_DERIVE_DH:
	isTLS = PR_TRUE;
	/* fall thru */
    case CKM_SSL3_MASTER_KEY_DERIVE:
    case CKM_SSL3_MASTER_KEY_DERIVE_DH:
      {
	CK_SSL3_MASTER_KEY_DERIVE_PARAMS *ssl3_master;
	SSL3RSAPreMasterSecret *rsa_pms;
        if ((pMechanism->mechanism == CKM_SSL3_MASTER_KEY_DERIVE_DH) ||
            (pMechanism->mechanism == CKM_TLS_MASTER_KEY_DERIVE_DH))
		isDH = PR_TRUE;

	/* first do the consistancy checks */
	if (!isDH && (att->attrib.ulValueLen != SSL3_PMS_LENGTH)) {
	    crv = CKR_KEY_TYPE_INCONSISTENT;
	    break;
	}
	att2 = pk11_FindAttribute(sourceKey,CKA_KEY_TYPE);
	if ((att2 == NULL) || (*(CK_KEY_TYPE *)att2->attrib.pValue !=
					CKK_GENERIC_SECRET)) {
	    if (att2) pk11_FreeAttribute(att2);
	    crv = CKR_KEY_FUNCTION_NOT_PERMITTED;
	    break;
	}
	pk11_FreeAttribute(att2);
	if (keyType != CKK_GENERIC_SECRET) {
	    crv = CKR_KEY_FUNCTION_NOT_PERMITTED;
	    break;
	}
	if ((keySize != 0) && (keySize != SSL3_MASTER_SECRET_LENGTH)) {
	    crv = CKR_KEY_FUNCTION_NOT_PERMITTED;
	    break;
	}


	/* finally do the key gen */
	ssl3_master = (CK_SSL3_MASTER_KEY_DERIVE_PARAMS *)
					pMechanism->pParameter;
	if (ssl3_master->pVersion) {
	    rsa_pms = (SSL3RSAPreMasterSecret *) att->attrib.pValue;
	    /* don't leak more key material then necessary for SSL to work */
	    if (key->wasDerived) {
		ssl3_master->pVersion->major = 0xff;
		ssl3_master->pVersion->minor = 0xff;
	    } else {
		ssl3_master->pVersion->major = rsa_pms->client_version[0];
		ssl3_master->pVersion->minor = rsa_pms->client_version[1];
	    }
	}
	if (ssl3_master->RandomInfo.ulClientRandomLen != SSL3_RANDOM_LENGTH) {
	   crv = CKR_MECHANISM_PARAM_INVALID;
	   break;
	}
	if (ssl3_master->RandomInfo.ulServerRandomLen != SSL3_RANDOM_LENGTH) {
	   crv = CKR_MECHANISM_PARAM_INVALID;
	   break;
	}

        if (isTLS) {
	    unsigned char crsrdata[SSL3_RANDOM_LENGTH * 2];
	    SECItem crsr = { siBuffer, NULL, 0 };
	    SECItem master = { siBuffer, NULL, 0 };
	    SECItem pms = { siBuffer, NULL, 0 };
	    SECStatus status;

	    pms.data = (unsigned char*)att->attrib.pValue;
	    pms.len = att->attrib.ulValueLen;
	    master.data = key_block;
	    master.len = SSL3_MASTER_SECRET_LENGTH;
	    crsr.data = crsrdata;
	    crsr.len = sizeof(crsrdata);

	    PORT_Memcpy(crsrdata, ssl3_master->RandomInfo.pClientRandom,
							 SSL3_RANDOM_LENGTH);
	    PORT_Memcpy(crsrdata + SSL3_RANDOM_LENGTH, 
		ssl3_master->RandomInfo.pServerRandom, SSL3_RANDOM_LENGTH);

	    status = pk11_PRF(&pms, "master secret", &crsr, &master);
	    if (status != SECSuccess) {
	    	crv = CKR_FUNCTION_FAILED;
		break;
	    }
	} else {
	    /* now allocate the hash contexts */
	    md5 = MD5_NewContext();
	    if (md5 == NULL) { 
		crv = CKR_HOST_MEMORY;
		break;
	    }
	    sha = SHA1_NewContext();
	    if (sha == NULL) { 
		PORT_Free(md5);
		crv = CKR_HOST_MEMORY;
		break;
	    }
            for (i = 0; i < 3; i++) {
              SHA1_Begin(sha);
              SHA1_Update(sha, (unsigned char*) mixers[i], strlen(mixers[i]));
              SHA1_Update(sha, (const unsigned char*)att->attrib.pValue, 
			  att->attrib.ulValueLen);
              SHA1_Update(sha, ssl3_master->RandomInfo.pClientRandom,
				ssl3_master->RandomInfo.ulClientRandomLen);
              SHA1_Update(sha, ssl3_master->RandomInfo.pServerRandom, 
				ssl3_master->RandomInfo.ulServerRandomLen);
              SHA1_End(sha, sha_out, &outLen, SHA1_LENGTH);
              PORT_Assert(outLen == SHA1_LENGTH);
              MD5_Begin(md5);
              MD5_Update(md5, (const unsigned char*)att->attrib.pValue, 
			 att->attrib.ulValueLen);
              MD5_Update(md5, sha_out, outLen);
              MD5_End(md5, &key_block[i*MD5_LENGTH], &outLen, MD5_LENGTH);
              PORT_Assert(outLen == MD5_LENGTH);
            }
	    PORT_Free(md5);
	    PORT_Free(sha);
	}

	/* store the results */
	crv = pk11_forceAttribute
			(key,CKA_VALUE,key_block,SSL3_MASTER_SECRET_LENGTH);
	if (crv != CKR_OK) break;
	keyType = CKK_GENERIC_SECRET;
	crv = pk11_forceAttribute (key,CKA_KEY_TYPE,&keyType,sizeof(keyType));
	if (isTLS) {
	    /* TLS's master secret is used to "sign" finished msgs with PRF. */
	    /* XXX This seems like a hack.   But PK11_Derive only accepts 
	     * one "operation" argument. */
	    crv = pk11_forceAttribute(key,CKA_SIGN,  &cktrue,sizeof(CK_BBOOL));
	    if (crv != CKR_OK) break;
	    crv = pk11_forceAttribute(key,CKA_VERIFY,&cktrue,sizeof(CK_BBOOL));
	    if (crv != CKR_OK) break;
	    /* While we're here, we might as well force this, too. */
	    crv = pk11_forceAttribute(key,CKA_DERIVE,&cktrue,sizeof(CK_BBOOL));
	    if (crv != CKR_OK) break;
	}
	break;
      }

    case CKM_TLS_KEY_AND_MAC_DERIVE:
	isTLS = PR_TRUE;
	/* fall thru */
    case CKM_SSL3_KEY_AND_MAC_DERIVE:
      {
	CK_SSL3_KEY_MAT_PARAMS *ssl3_keys;
	CK_SSL3_KEY_MAT_OUT *   ssl3_keys_out;
	CK_ULONG                effKeySize;

	crv = pk11_DeriveSensitiveCheck(sourceKey,key);
	if (crv != CKR_OK) break;

	if (att->attrib.ulValueLen != SSL3_MASTER_SECRET_LENGTH) {
	    crv = CKR_KEY_FUNCTION_NOT_PERMITTED;
	    break;
	}
	att2 = pk11_FindAttribute(sourceKey,CKA_KEY_TYPE);
	if ((att2 == NULL) || (*(CK_KEY_TYPE *)att2->attrib.pValue !=
					CKK_GENERIC_SECRET)) {
	    if (att2) pk11_FreeAttribute(att2);
	    crv = CKR_KEY_FUNCTION_NOT_PERMITTED;
	    break;
	}
	pk11_FreeAttribute(att2);
	md5 = MD5_NewContext();
	if (md5 == NULL) { 
	    crv = CKR_HOST_MEMORY;
	    break;
	}
	sha = SHA1_NewContext();
	if (sha == NULL) { 
	    PORT_Free(md5);
	    crv = CKR_HOST_MEMORY;
	    break;
	}
	ssl3_keys = (CK_SSL3_KEY_MAT_PARAMS *) pMechanism->pParameter;
	/*
	 * clear out our returned keys so we can recover on failure
	 */
	ssl3_keys_out = ssl3_keys->pReturnedKeyMaterial;
	ssl3_keys_out->hClientMacSecret = CK_INVALID_KEY;
	ssl3_keys_out->hServerMacSecret = CK_INVALID_KEY;
	ssl3_keys_out->hClientKey       = CK_INVALID_KEY;
	ssl3_keys_out->hServerKey       = CK_INVALID_KEY;

	/*
	 * generate the key material: This looks amazingly similar to the
	 * PMS code, and is clearly crying out for a function to provide it.
	 */
	if (isTLS) {
	    SECStatus     status;
	    SECItem       master = { siBuffer, NULL, 0 };
	    SECItem       srcr   = { siBuffer, NULL, 0 };
	    SECItem       keyblk = { siBuffer, NULL, 0 };
	    unsigned char srcrdata[SSL3_RANDOM_LENGTH * 2];

	    master.data = (unsigned char*)att->attrib.pValue;
	    master.len  = att->attrib.ulValueLen;
	    srcr.data   = srcrdata;
	    srcr.len    = sizeof srcrdata;
	    keyblk.data = key_block;
	    keyblk.len  = sizeof key_block;

	    PORT_Memcpy(srcrdata, 
	                ssl3_keys->RandomInfo.pServerRandom,
			SSL3_RANDOM_LENGTH);
	    PORT_Memcpy(srcrdata + SSL3_RANDOM_LENGTH, 
		        ssl3_keys->RandomInfo.pClientRandom, 
			SSL3_RANDOM_LENGTH);

	    status = pk11_PRF(&master, "key expansion", &srcr, &keyblk);
	    if (status != SECSuccess) {
		goto key_and_mac_derive_fail;
	    }
	} else {
	    /* key_block = 
	     *     MD5(master_secret + SHA('A' + master_secret + 
	     *                      ServerHello.random + ClientHello.random)) +
	     *     MD5(master_secret + SHA('BB' + master_secret + 
	     *                      ServerHello.random + ClientHello.random)) +
	     *     MD5(master_secret + SHA('CCC' + master_secret + 
	     *                      ServerHello.random + ClientHello.random)) +
	     *     [...];
	     */
	    for (i = 0; i < NUM_MIXERS; i++) {
	      SHA1_Begin(sha);
	      SHA1_Update(sha, (unsigned char*) mixers[i], strlen(mixers[i]));
	      SHA1_Update(sha, (const unsigned char*)att->attrib.pValue, 
			  att->attrib.ulValueLen);
              SHA1_Update(sha, ssl3_keys->RandomInfo.pServerRandom, 
				ssl3_keys->RandomInfo.ulServerRandomLen);
              SHA1_Update(sha, ssl3_keys->RandomInfo.pClientRandom,
				ssl3_keys->RandomInfo.ulClientRandomLen);
	      SHA1_End(sha, sha_out, &outLen, SHA1_LENGTH);
	      PORT_Assert(outLen == SHA1_LENGTH);
	      MD5_Begin(md5);
	      MD5_Update(md5, (const unsigned char*)att->attrib.pValue,
			 att->attrib.ulValueLen);
	      MD5_Update(md5, sha_out, outLen);
	      MD5_End(md5, &key_block[i*MD5_LENGTH], &outLen, MD5_LENGTH);
	      PORT_Assert(outLen == MD5_LENGTH);
	    }
	}

	/*
	 * Put the key material where it goes.
	 */
	i = 0;			/* now shows how much consumed */
	macSize    = ssl3_keys->ulMacSizeInBits/8;
	effKeySize = ssl3_keys->ulKeySizeInBits/8;
	IVSize     = ssl3_keys->ulIVSizeInBits/8;
	if (keySize == 0) {
	    effKeySize = keySize;
	}

	/* 
	 * The key_block is partitioned as follows:
	 * client_write_MAC_secret[CipherSpec.hash_size]
	 */
	crv = pk11_buildSSLKey(hSession,key,PR_TRUE,&key_block[i],macSize,
					 &ssl3_keys_out->hClientMacSecret);
	if (crv != CKR_OK)
	    goto key_and_mac_derive_fail;

	i += macSize;

	/* 
	 * server_write_MAC_secret[CipherSpec.hash_size]
	 */
	crv = pk11_buildSSLKey(hSession,key,PR_TRUE,&key_block[i],macSize,
					    &ssl3_keys_out->hServerMacSecret);
	if (crv != CKR_OK) {
	    goto key_and_mac_derive_fail;
	}
	i += macSize;

	if (keySize) {
	    if (!ssl3_keys->bIsExport) {
		/* 
		** Generate Domestic write keys and IVs.
		** client_write_key[CipherSpec.key_material]
		*/
		crv = pk11_buildSSLKey(hSession,key,PR_FALSE,&key_block[i],
					keySize, &ssl3_keys_out->hClientKey);
		if (crv != CKR_OK) {
		    goto key_and_mac_derive_fail;
		}
		i += keySize;

		/* 
		** server_write_key[CipherSpec.key_material]
		*/
		crv = pk11_buildSSLKey(hSession,key,PR_FALSE,&key_block[i],
					keySize, &ssl3_keys_out->hServerKey);
		if (crv != CKR_OK) {
		    goto key_and_mac_derive_fail;
		}
		i += keySize;

		/* 
		** client_write_IV[CipherSpec.IV_size]
		*/
		PORT_Memcpy(ssl3_keys_out->pIVClient, &key_block[i], IVSize);
		i += IVSize;

		/* 
		** server_write_IV[CipherSpec.IV_size]
		*/
		PORT_Memcpy(ssl3_keys_out->pIVServer, &key_block[i], IVSize);
	    	i += IVSize;

	    } else if (!isTLS) {

		/*
		** Generate SSL3 Export write keys and IVs.
		** client_write_key[CipherSpec.key_material]
		** final_client_write_key = MD5(client_write_key +
		**                   ClientHello.random + ServerHello.random);
		*/
		MD5_Begin(md5);
		MD5_Update(md5, &key_block[i], effKeySize);
            	MD5_Update(md5, ssl3_keys->RandomInfo.pClientRandom,
				ssl3_keys->RandomInfo.ulClientRandomLen);
            	MD5_Update(md5, ssl3_keys->RandomInfo.pServerRandom, 
				ssl3_keys->RandomInfo.ulServerRandomLen);
		MD5_End(md5, key_block2, &outLen, MD5_LENGTH);
		i += effKeySize;
		crv = pk11_buildSSLKey(hSession,key,PR_FALSE,key_block2,
				 	keySize,&ssl3_keys_out->hClientKey);
		if (crv != CKR_OK) {
		    goto key_and_mac_derive_fail;
		}

		/*
		** server_write_key[CipherSpec.key_material]
		** final_server_write_key = MD5(server_write_key +
		**                    ServerHello.random + ClientHello.random);
		*/
		MD5_Begin(md5);
		MD5_Update(md5, &key_block[i], effKeySize);
            	MD5_Update(md5, ssl3_keys->RandomInfo.pServerRandom, 
				ssl3_keys->RandomInfo.ulServerRandomLen);
            	MD5_Update(md5, ssl3_keys->RandomInfo.pClientRandom,
				ssl3_keys->RandomInfo.ulClientRandomLen);
		MD5_End(md5, key_block2, &outLen, MD5_LENGTH);
		i += effKeySize;
		crv = pk11_buildSSLKey(hSession,key,PR_FALSE,key_block2,
					 keySize,&ssl3_keys_out->hServerKey);
		if (crv != CKR_OK) {
		    goto key_and_mac_derive_fail;
		}

		/*
		** client_write_IV = 
		**	MD5(ClientHello.random + ServerHello.random);
		*/
		MD5_Begin(md5);
            	MD5_Update(md5, ssl3_keys->RandomInfo.pClientRandom,
				ssl3_keys->RandomInfo.ulClientRandomLen);
            	MD5_Update(md5, ssl3_keys->RandomInfo.pServerRandom, 
				ssl3_keys->RandomInfo.ulServerRandomLen);
		MD5_End(md5, key_block2, &outLen, MD5_LENGTH);
		PORT_Memcpy(ssl3_keys_out->pIVClient, key_block2, IVSize);

		/*
		** server_write_IV = 
		**	MD5(ServerHello.random + ClientHello.random);
		*/
		MD5_Begin(md5);
            	MD5_Update(md5, ssl3_keys->RandomInfo.pServerRandom, 
				ssl3_keys->RandomInfo.ulServerRandomLen);
            	MD5_Update(md5, ssl3_keys->RandomInfo.pClientRandom,
				ssl3_keys->RandomInfo.ulClientRandomLen);
		MD5_End(md5, key_block2, &outLen, MD5_LENGTH);
		PORT_Memcpy(ssl3_keys_out->pIVServer, key_block2, IVSize);

	    } else {

		/*
		** Generate TLS Export write keys and IVs.
		*/
		SECStatus     status;
		SECItem       secret = { siBuffer, NULL, 0 };
		SECItem       crsr   = { siBuffer, NULL, 0 };
		SECItem       keyblk = { siBuffer, NULL, 0 };
		unsigned char crsrdata[SSL3_RANDOM_LENGTH * 2];

		crsr.data   = crsrdata;
		crsr.len    = sizeof crsrdata;

		PORT_Memcpy(crsrdata, 
		            ssl3_keys->RandomInfo.pClientRandom, 
			    SSL3_RANDOM_LENGTH);
		PORT_Memcpy(crsrdata + SSL3_RANDOM_LENGTH, 
			    ssl3_keys->RandomInfo.pServerRandom,
			    SSL3_RANDOM_LENGTH);


		/*
		** client_write_key[CipherSpec.key_material]
		** final_client_write_key = PRF(client_write_key, 
		**                              "client write key",
		**                              client_random + server_random);
		*/
		secret.data = &key_block[i];
		secret.len  = effKeySize;
		i          += effKeySize;
		keyblk.data = key_block2;
		keyblk.len  = sizeof key_block2;
		status = pk11_PRF(&secret, "client write key", &crsr, &keyblk);
		if (status != SECSuccess) {
		    goto key_and_mac_derive_fail;
		}
		crv = pk11_buildSSLKey(hSession, key, PR_FALSE, key_block2, 
				       keySize, &ssl3_keys_out->hClientKey);
		if (crv != CKR_OK) {
		    goto key_and_mac_derive_fail;
		}

		/*
		** server_write_key[CipherSpec.key_material]
		** final_server_write_key = PRF(server_write_key,
		**                              "server write key",
		**                              client_random + server_random);
		*/
		secret.data = &key_block[i];
		secret.len  = effKeySize;
		i          += effKeySize;
		keyblk.data = key_block2;
		keyblk.len  = sizeof key_block2;
		status = pk11_PRF(&secret, "server write key", &crsr, &keyblk);
		if (status != SECSuccess) {
		    goto key_and_mac_derive_fail;
		}
		crv = pk11_buildSSLKey(hSession, key, PR_FALSE, key_block2, 
				       keySize, &ssl3_keys_out->hServerKey);
		if (crv != CKR_OK) {
		    goto key_and_mac_derive_fail;
		}

		/*
		** iv_block = PRF("", "IV block", 
		**                    client_random + server_random);
		** client_write_IV[SecurityParameters.IV_size]
		** server_write_IV[SecurityParameters.IV_size]
		*/
		if (IVSize) {
		    secret.data = NULL;
		    secret.len  = 0;
		    keyblk.data = &key_block[i];
		    keyblk.len  = 2 * IVSize;
		    status = pk11_PRF(&secret, "IV block", &crsr, &keyblk);
		    if (status != SECSuccess) {
			goto key_and_mac_derive_fail;
		    }
		    PORT_Memcpy(ssl3_keys_out->pIVClient, keyblk.data, IVSize);
		    PORT_Memcpy(ssl3_keys_out->pIVServer, keyblk.data + IVSize,
                                IVSize);
		}
	    }
	}

	crv = CKR_OK;

	if (0) {
key_and_mac_derive_fail:
	    if (crv == CKR_OK)
	    	crv = CKR_FUNCTION_FAILED;
	    pk11_freeSSLKeys(hSession, ssl3_keys_out);
	}
	MD5_DestroyContext(md5, PR_TRUE);
	SHA1_DestroyContext(sha, PR_TRUE);
	pk11_FreeObject(key);
	key = NULL;
	break;
      }

    case CKM_CONCATENATE_BASE_AND_KEY:
      {
	PK11Object *newKey;

	crv = pk11_DeriveSensitiveCheck(sourceKey,key);
	if (crv != CKR_OK) break;

	session = pk11_SessionFromHandle(hSession);
	if (session == NULL) {
            crv = CKR_SESSION_HANDLE_INVALID;
	    break;
    	}

	newKey = pk11_ObjectFromHandle(*(CK_OBJECT_HANDLE *)
					pMechanism->pParameter,session);
	pk11_FreeSession(session);
	if ( newKey == NULL) {
            crv = CKR_KEY_HANDLE_INVALID;
	    break;
	}

	if (pk11_isTrue(newKey,CKA_SENSITIVE)) {
	    crv = pk11_forceAttribute(newKey,CKA_SENSITIVE,&cktrue,
							sizeof(CK_BBOOL));
	    if (crv != CKR_OK) {
		pk11_FreeObject(newKey);
		break;
	    }
	}

	att2 = pk11_FindAttribute(newKey,CKA_VALUE);
	if (att2 == NULL) {
	    pk11_FreeObject(newKey);
            crv = CKR_KEY_HANDLE_INVALID;
	    break;
	}
	tmpKeySize = att->attrib.ulValueLen+att2->attrib.ulValueLen;
	if (keySize == 0) keySize = tmpKeySize;
	if (keySize > tmpKeySize) {
	    pk11_FreeObject(newKey);
	    pk11_FreeAttribute(att2);
	    crv = CKR_TEMPLATE_INCONSISTENT;
	    break;
	}
	buf = (unsigned char*)PORT_Alloc(tmpKeySize);
	if (buf == NULL) {
	    pk11_FreeAttribute(att2);
	    pk11_FreeObject(newKey);
	    crv = CKR_HOST_MEMORY;	
	    break;
	}

	PORT_Memcpy(buf,att->attrib.pValue,att->attrib.ulValueLen);
	PORT_Memcpy(buf+att->attrib.ulValueLen,
				att2->attrib.pValue,att2->attrib.ulValueLen);

	crv = pk11_forceAttribute (key,CKA_VALUE,buf,keySize);
	PORT_ZFree(buf,tmpKeySize);
	pk11_FreeAttribute(att2);
	pk11_FreeObject(newKey);
	break;
      }

    case CKM_CONCATENATE_BASE_AND_DATA:
	stringPtr = (CK_KEY_DERIVATION_STRING_DATA *) pMechanism->pParameter;
	tmpKeySize = att->attrib.ulValueLen+stringPtr->ulLen;
	if (keySize == 0) keySize = tmpKeySize;
	if (keySize > tmpKeySize) {
	    crv = CKR_TEMPLATE_INCONSISTENT;
	    break;
	}
	buf = (unsigned char*)PORT_Alloc(tmpKeySize);
	if (buf == NULL) {
	    crv = CKR_HOST_MEMORY;
	    break;
	}

	PORT_Memcpy(buf,att->attrib.pValue,att->attrib.ulValueLen);
	PORT_Memcpy(buf+att->attrib.ulValueLen,stringPtr->pData,
							stringPtr->ulLen);

	crv = pk11_forceAttribute (key,CKA_VALUE,buf,keySize);
	PORT_ZFree(buf,tmpKeySize);
	break;
    case CKM_CONCATENATE_DATA_AND_BASE:
	stringPtr = (CK_KEY_DERIVATION_STRING_DATA *)pMechanism->pParameter;
	tmpKeySize = att->attrib.ulValueLen+stringPtr->ulLen;
	if (keySize == 0) keySize = tmpKeySize;
	if (keySize > tmpKeySize) {
	    crv = CKR_TEMPLATE_INCONSISTENT;
	    break;
	}
	buf = (unsigned char*)PORT_Alloc(tmpKeySize);
	if (buf == NULL) {
	    crv = CKR_HOST_MEMORY;
	    break;
	}

	PORT_Memcpy(buf,stringPtr->pData,stringPtr->ulLen);
	PORT_Memcpy(buf+stringPtr->ulLen,att->attrib.pValue,
							att->attrib.ulValueLen);

	crv = pk11_forceAttribute (key,CKA_VALUE,buf,keySize);
	PORT_ZFree(buf,tmpKeySize);
	break;
    case CKM_XOR_BASE_AND_DATA:
	stringPtr = (CK_KEY_DERIVATION_STRING_DATA *)pMechanism->pParameter;
	tmpKeySize = PR_MIN(att->attrib.ulValueLen,stringPtr->ulLen);
	if (keySize == 0) keySize = tmpKeySize;
	if (keySize > tmpKeySize) {
	    crv = CKR_TEMPLATE_INCONSISTENT;
	    break;
	}
	buf = (unsigned char*)PORT_Alloc(keySize);
	if (buf == NULL) {
	    crv = CKR_HOST_MEMORY;
	    break;
	}

	
	PORT_Memcpy(buf,att->attrib.pValue,keySize);
	for (i=0; i < (int)keySize; i++) {
	    buf[i] ^= stringPtr->pData[i];
	}

	crv = pk11_forceAttribute (key,CKA_VALUE,buf,keySize);
	PORT_ZFree(buf,keySize);
	break;

    case CKM_EXTRACT_KEY_FROM_KEY:
      {
	/* the following assumes 8 bits per byte */
	CK_ULONG extract = *(CK_EXTRACT_PARAMS *)pMechanism->pParameter;
	CK_ULONG shift   = extract & 0x7;      /* extract mod 8 the fast way */
	CK_ULONG offset  = extract >> 3;       /* extract div 8 the fast way */

	if (keySize == 0)  {
	    crv = CKR_TEMPLATE_INCOMPLETE;
	    break;
	}
	/* make sure we have enough bits in the original key */
	if (att->attrib.ulValueLen < 
			(offset + keySize + ((shift != 0)? 1 :0)) ) {
	    crv = CKR_MECHANISM_PARAM_INVALID;
	    break;
	}
	buf = (unsigned char*)PORT_Alloc(keySize);
	if (buf == NULL) {
	    crv = CKR_HOST_MEMORY;
	    break;
	}

	/* copy the bits we need into the new key */	
	for (i=0; i < (int)keySize; i++) {
	    unsigned char *value =
			 ((unsigned char *)att->attrib.pValue)+offset+i;
	    if (shift) {
	        buf[i] = (value[0] << (shift)) | (value[1] >> (8 - shift));
	    } else {
		buf[i] = value[0];
	    }
	}

	crv = pk11_forceAttribute (key,CKA_VALUE,buf,keySize);
	PORT_ZFree(buf,keySize);
	break;
      }
    case CKM_MD2_KEY_DERIVATION:
	if (keySize == 0) keySize = MD2_LENGTH;
	if (keySize > MD2_LENGTH) {
	    crv = CKR_TEMPLATE_INCONSISTENT;
	    break;
	}
	/* now allocate the hash contexts */
	md2 = MD2_NewContext();
	if (md2 == NULL) { 
	    crv = CKR_HOST_MEMORY;
	    break;
	}
	MD2_Begin(md2);
	MD2_Update(md2,(const unsigned char*)att->attrib.pValue,
		   att->attrib.ulValueLen);
	MD2_End(md2,key_block,&outLen,MD2_LENGTH);
	MD2_DestroyContext(md2, PR_TRUE);

	crv = pk11_forceAttribute (key,CKA_VALUE,key_block,keySize);
	break;
    case CKM_MD5_KEY_DERIVATION:
	if (keySize == 0) keySize = MD5_LENGTH;
	if (keySize > MD5_LENGTH) {
	    crv = CKR_TEMPLATE_INCONSISTENT;
	    break;
	}
	/* now allocate the hash contexts */
	md5 = MD5_NewContext();
	if (md5 == NULL) { 
	    crv = CKR_HOST_MEMORY;
	    break;
	}
	MD5_Begin(md5);
	MD5_Update(md5,(const unsigned char*)att->attrib.pValue,
		   att->attrib.ulValueLen);
	MD5_End(md5,key_block,&outLen,MD5_LENGTH);
	MD5_DestroyContext(md5, PR_TRUE);

	crv = pk11_forceAttribute (key,CKA_VALUE,key_block,keySize);
	break;
     case CKM_SHA1_KEY_DERIVATION:
	if (keySize == 0) keySize = SHA1_LENGTH;
	if (keySize > SHA1_LENGTH) {
	    crv = CKR_TEMPLATE_INCONSISTENT;
	    break;
	}
	/* now allocate the hash contexts */
	sha = SHA1_NewContext();
	if (sha == NULL) { 
	    crv = CKR_HOST_MEMORY;
	    break;
	}
	SHA1_Begin(sha);
	SHA1_Update(sha,(const unsigned char*)att->attrib.pValue,
		    att->attrib.ulValueLen);
	SHA1_End(sha,key_block,&outLen,SHA1_LENGTH);
	SHA1_DestroyContext(sha, PR_TRUE);

	crv = pk11_forceAttribute(key,CKA_VALUE,key_block,keySize);
	break;

    case CKM_DH_PKCS_DERIVE:
      {
	SECItem  derived,  dhPublic;
	SECItem  dhPrime,  dhValue;
	/* sourceKey - values for the local existing low key */
	/* get prime and value attributes */
	crv = pk11_Attribute2SecItem(NULL, &dhPrime, sourceKey, CKA_PRIME); 
	if (crv != SECSuccess) break;
	crv = pk11_Attribute2SecItem(NULL, &dhValue, sourceKey, CKA_VALUE); 
	if (crv != SECSuccess) {
 	    PORT_Free(dhPrime.data);
	    break;
	}

	dhPublic.data = pMechanism->pParameter;
	dhPublic.len  = pMechanism->ulParameterLen;

	/* calculate private value - oct */
	rv = DH_Derive(&dhPublic, &dhPrime, &dhValue, &derived, keySize); 

	PORT_Free(dhPrime.data);
	PORT_Free(dhValue.data);
     
	if (rv == SECSuccess) {
	    pk11_forceAttribute(key, CKA_VALUE, derived.data, derived.len);
	    PORT_ZFree(derived.data, derived.len);
	} else
	    crv = CKR_HOST_MEMORY;
	    
	break;
      }

    default:
	crv = CKR_MECHANISM_INVALID;
    }
    pk11_FreeAttribute(att);
    pk11_FreeObject(sourceKey);
    if (crv != CKR_OK) { 
	if (key) pk11_FreeObject(key);
	return crv;
    }

    /* link the key object into the list */
    if (key) {
	/* get the session */
	key->wasDerived = PR_TRUE;
	session = pk11_SessionFromHandle(hSession);
	if (session == NULL) {
	    pk11_FreeObject(key);
	    return CKR_HOST_MEMORY;
	}

	crv = pk11_handleObject(key,session);
	pk11_FreeSession(session);
	if (crv == CKR_OK) {
	    *phKey = key->handle;
	     return crv;
	}
	pk11_FreeObject(key);
    }
    return crv;
}


/* NSC_GetFunctionStatus obtains an updated status of a function running 
 * in parallel with an application. */
CK_RV NSC_GetFunctionStatus(CK_SESSION_HANDLE hSession)
{
    return CKR_FUNCTION_NOT_PARALLEL;
}

/* NSC_CancelFunction cancels a function running in parallel */
CK_RV NSC_CancelFunction(CK_SESSION_HANDLE hSession)
{
    return CKR_FUNCTION_NOT_PARALLEL;
}

/* NSC_GetOperationState saves the state of the cryptographic 
 *operation in a session.
 * NOTE: This code only works for digest functions for now. eventually need
 * to add full flatten/resurect to our state stuff so that all types of state
 * can be saved */
CK_RV NSC_GetOperationState(CK_SESSION_HANDLE hSession, 
	CK_BYTE_PTR  pOperationState, CK_ULONG_PTR pulOperationStateLen)
{
    PK11SessionContext *context;
    PK11Session *session;
    CK_RV crv;

    /* make sure we're legal */
    crv = pk11_GetContext(hSession, &context, PK11_HASH, PR_TRUE, &session);
    if (crv != CKR_OK) return crv;

    *pulOperationStateLen = context->cipherInfoLen + sizeof(CK_MECHANISM_TYPE)
				+ sizeof(PK11ContextType);
    if (pOperationState == NULL) {
        pk11_FreeSession(session);
	return CKR_OK;
    }
    PORT_Memcpy(pOperationState,&context->type,sizeof(PK11ContextType));
    pOperationState += sizeof(PK11ContextType);
    PORT_Memcpy(pOperationState,&context->currentMech,
						sizeof(CK_MECHANISM_TYPE));
    pOperationState += sizeof(CK_MECHANISM_TYPE);
    PORT_Memcpy(pOperationState,context->cipherInfo,context->cipherInfoLen);
    pk11_FreeSession(session);
    return CKR_OK;
}


#define pk11_Decrement(stateSize,len) \
	stateSize = ((stateSize) > (CK_ULONG)(len)) ? \
				((stateSize) - (CK_ULONG)(len)) : 0;

/* NSC_SetOperationState restores the state of the cryptographic 
 * operation in a session. This is coded like it can restore lots of
 * states, but it only works for truly flat cipher structures. */
CK_RV NSC_SetOperationState(CK_SESSION_HANDLE hSession, 
	CK_BYTE_PTR  pOperationState, CK_ULONG  ulOperationStateLen,
        CK_OBJECT_HANDLE hEncryptionKey, CK_OBJECT_HANDLE hAuthenticationKey)
{
    PK11SessionContext *context;
    PK11Session *session;
    PK11ContextType type;
    CK_MECHANISM mech;
    CK_RV crv = CKR_OK;

    while (ulOperationStateLen != 0) {
	/* get what type of state we're dealing with... */
	PORT_Memcpy(&type,pOperationState, sizeof(PK11ContextType));

	/* fix up session contexts based on type */
	session = pk11_SessionFromHandle(hSession);
	if (session == NULL) return CKR_SESSION_HANDLE_INVALID;
	context = pk11_ReturnContextByType(session, type);
	pk11_SetContextByType(session, type, NULL);
	if (context) { 
	     pk11_FreeContext(context);
	}
	pOperationState += sizeof(PK11ContextType);
	pk11_Decrement(ulOperationStateLen,sizeof(PK11ContextType));


	/* get the mechanism structure */
	PORT_Memcpy(&mech.mechanism,pOperationState,sizeof(CK_MECHANISM_TYPE));
	pOperationState += sizeof(CK_MECHANISM_TYPE);
	pk11_Decrement(ulOperationStateLen, sizeof(CK_MECHANISM_TYPE));
	/* should be filled in... but not necessary for hash */
	mech.pParameter = NULL;
	mech.ulParameterLen = 0;
	switch (type) {
	case PK11_HASH:
	    crv = NSC_DigestInit(hSession,&mech);
	    if (crv != CKR_OK) break;
	    crv = pk11_GetContext(hSession, &context, PK11_HASH, PR_TRUE, 
								NULL);
	    if (crv != CKR_OK) break;
	    PORT_Memcpy(context->cipherInfo,pOperationState,
						context->cipherInfoLen);
	    pOperationState += context->cipherInfoLen;
	    pk11_Decrement(ulOperationStateLen,context->cipherInfoLen);
	    break;
	default:
	    /* do sign/encrypt/decrypt later */
	    crv = CKR_SAVED_STATE_INVALID;
         }
         pk11_FreeSession(session);
	 if (crv != CKR_OK) break;
    }
    return crv;
}

/* Dual-function cryptographic operations */

/* NSC_DigestEncryptUpdate continues a multiple-part digesting and encryption 
 * operation. */
CK_RV NSC_DigestEncryptUpdate(CK_SESSION_HANDLE hSession, CK_BYTE_PTR  pPart,
    CK_ULONG  ulPartLen, CK_BYTE_PTR  pEncryptedPart,
					 CK_ULONG_PTR pulEncryptedPartLen)
{
    CK_RV crv;

    crv = NSC_EncryptUpdate(hSession,pPart,ulPartLen, pEncryptedPart,	
						      pulEncryptedPartLen);
    if (crv != CKR_OK) return crv;
    crv = NSC_DigestUpdate(hSession,pPart,ulPartLen);

    return crv;
}


/* NSC_DecryptDigestUpdate continues a multiple-part decryption and 
 * digesting operation. */
CK_RV NSC_DecryptDigestUpdate(CK_SESSION_HANDLE hSession,
    CK_BYTE_PTR  pEncryptedPart, CK_ULONG  ulEncryptedPartLen,
    				CK_BYTE_PTR  pPart, CK_ULONG_PTR pulPartLen)
{
    CK_RV crv;

    crv = NSC_DecryptUpdate(hSession,pEncryptedPart, ulEncryptedPartLen, 
							pPart,	pulPartLen);
    if (crv != CKR_OK) return crv;
    crv = NSC_DigestUpdate(hSession,pPart,*pulPartLen);

    return crv;
}


/* NSC_SignEncryptUpdate continues a multiple-part signing and 
 * encryption operation. */
CK_RV NSC_SignEncryptUpdate(CK_SESSION_HANDLE hSession, CK_BYTE_PTR  pPart,
	 CK_ULONG  ulPartLen, CK_BYTE_PTR  pEncryptedPart,
					 CK_ULONG_PTR pulEncryptedPartLen)
{
    CK_RV crv;

    crv = NSC_EncryptUpdate(hSession,pPart,ulPartLen, pEncryptedPart,	
						      pulEncryptedPartLen);
    if (crv != CKR_OK) return crv;
    crv = NSC_SignUpdate(hSession,pPart,ulPartLen);

    return crv;
}


/* NSC_DecryptVerifyUpdate continues a multiple-part decryption 
 * and verify operation. */
CK_RV NSC_DecryptVerifyUpdate(CK_SESSION_HANDLE hSession, 
	CK_BYTE_PTR  pEncryptedData, CK_ULONG  ulEncryptedDataLen, 
				CK_BYTE_PTR  pData, CK_ULONG_PTR pulDataLen)
{
    CK_RV crv;

    crv = NSC_DecryptUpdate(hSession,pEncryptedData, ulEncryptedDataLen, 
							pData,	pulDataLen);
    if (crv != CKR_OK) return crv;
    crv = NSC_VerifyUpdate(hSession, pData, *pulDataLen);

    return crv;
}

/* NSC_DigestKey continues a multi-part message-digesting operation,
 * by digesting the value of a secret key as part of the data already digested.
 */
CK_RV NSC_DigestKey(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE hKey) 
{
    PK11Session *session = NULL;
    PK11Object *key = NULL;
    PK11Attribute *att;
    CK_RV crv;

    session = pk11_SessionFromHandle(hSession);
    if (session == NULL) return CKR_SESSION_HANDLE_INVALID;

    key = pk11_ObjectFromHandle(hKey,session);
    pk11_FreeSession(session);
    if (key == NULL)  return CKR_KEY_HANDLE_INVALID;

    /* PUT ANY DIGEST KEY RESTRICTION CHECKS HERE */

    /* make sure it's a valid  key for this operation */
    if (key->objclass != CKO_SECRET_KEY) {
	pk11_FreeObject(key);
	return CKR_KEY_TYPE_INCONSISTENT;
    }
    /* get the key value */
    att = pk11_FindAttribute(key,CKA_VALUE);
    pk11_FreeObject(key);

    crv = NSC_DigestUpdate(hSession,(CK_BYTE_PTR)att->attrib.pValue,
			   att->attrib.ulValueLen);
    pk11_FreeAttribute(att);
    return crv;
}
