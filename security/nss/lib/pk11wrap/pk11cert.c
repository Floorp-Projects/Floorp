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
/*
 * This file manages PKCS #11 instances of certificates.
 */

#include "secport.h"
#include "seccomon.h"
#include "secmod.h"
#include "secmodi.h"
#include "secmodti.h"
#include "pkcs11.h"
#include "pk11func.h"
#include "cert.h"
#include "certi.h"
#include "secitem.h"
#include "key.h" 
#include "secoid.h"
#include "pkcs7t.h"
#include "cmsreclist.h"

#include "certdb.h"
#include "secerr.h"
#include "sslerr.h"

#ifndef NSS_3_4_CODE
#define NSS_3_4_CODE
#endif /* NSS_3_4_CODE */
#include "pki3hack.h"
#include "dev3hack.h"

#include "devm.h" 
#include "nsspki.h"
#include "pki.h"
#include "pkim.h"
#include "pkitm.h"
#include "pkistore.h" /* to remove temp cert */
#include "devt.h"

extern const NSSError NSS_ERROR_NOT_FOUND;
extern const NSSError NSS_ERROR_INVALID_CERTIFICATE;

struct nss3_cert_cbstr {
    SECStatus(* callback)(CERTCertificate*, void *);
    nssList *cached;
    void *arg;
};

/* Translate from NSSCertificate to CERTCertificate, then pass the latter
 * to a callback.
 */
static PRStatus convert_cert(NSSCertificate *c, void *arg)
{
    CERTCertificate *nss3cert;
    SECStatus secrv;
    struct nss3_cert_cbstr *nss3cb = (struct nss3_cert_cbstr *)arg;
    nss3cert = STAN_GetCERTCertificate(c);
    if (!nss3cert) return PR_FAILURE;
    secrv = (*nss3cb->callback)(nss3cert, nss3cb->arg);
    return (secrv) ? PR_FAILURE : PR_SUCCESS;
}

/*
 * build a cert nickname based on the token name and the label of the 
 * certificate If the label in NULL, build a label based on the ID.
 */
static int toHex(int x) { return (x < 10) ? (x+'0') : (x+'a'-10); }
#define MAX_CERT_ID 4
#define DEFAULT_STRING "Cert ID "
static char *
pk11_buildNickname(PK11SlotInfo *slot,CK_ATTRIBUTE *cert_label,
			CK_ATTRIBUTE *key_label, CK_ATTRIBUTE *cert_id)
{
    int prefixLen = PORT_Strlen(slot->token_name);
    int suffixLen = 0;
    char *suffix = NULL;
    char buildNew[sizeof(DEFAULT_STRING)+MAX_CERT_ID*2];
    char *next,*nickname;

    if (cert_label && (cert_label->ulValueLen)) {
	suffixLen = cert_label->ulValueLen;
	suffix = (char*)cert_label->pValue;
    } else if (key_label && (key_label->ulValueLen)) {
	suffixLen = key_label->ulValueLen;
	suffix = (char*)key_label->pValue;
    } else if (cert_id && cert_id->ulValueLen > 0) {
	int i,first = cert_id->ulValueLen - MAX_CERT_ID;
	int offset = sizeof(DEFAULT_STRING);
	char *idValue = (char *)cert_id->pValue;

	PORT_Memcpy(buildNew,DEFAULT_STRING,sizeof(DEFAULT_STRING)-1);
	next = buildNew + offset;
	if (first < 0) first = 0;
	for (i=first; i < (int) cert_id->ulValueLen; i++) {
		*next++ = toHex((idValue[i] >> 4) & 0xf);
		*next++ = toHex(idValue[i] & 0xf);
	}
	*next++ = 0;
	suffix = buildNew;
	suffixLen = PORT_Strlen(buildNew);
    } else {
	PORT_SetError( SEC_ERROR_LIBRARY_FAILURE );
	return NULL;
    }

    /* if is internal key slot, add code to skip the prefix!! */
    next = nickname = (char *)PORT_Alloc(prefixLen+1+suffixLen+1);
    if (nickname == NULL) return NULL;

    PORT_Memcpy(next,slot->token_name,prefixLen);
    next += prefixLen;
    *next++ = ':';
    PORT_Memcpy(next,suffix,suffixLen);
    next += suffixLen;
    *next++ = 0;
    return nickname;
}

PRBool
PK11_IsUserCert(PK11SlotInfo *slot, CERTCertificate *cert,
						CK_OBJECT_HANDLE certID)
{
    CK_OBJECT_CLASS theClass;

    if (slot == NULL) return PR_FALSE;
    if (cert == NULL) return PR_FALSE;

    theClass = CKO_PRIVATE_KEY;
    if (pk11_LoginStillRequired(slot,NULL)) {
	theClass = CKO_PUBLIC_KEY;
    }
    if (PK11_MatchItem(slot, certID , theClass) != CK_INVALID_HANDLE) {
	return PR_TRUE;
    }

   if (theClass == CKO_PUBLIC_KEY) {
	SECKEYPublicKey *pubKey= CERT_ExtractPublicKey(cert);
	CK_ATTRIBUTE theTemplate;

	if (pubKey == NULL) {
	   return PR_FALSE;
	}

	PK11_SETATTRS(&theTemplate,0,NULL,0);
	switch (pubKey->keyType) {
	case rsaKey:
	    PK11_SETATTRS(&theTemplate,CKA_MODULUS, pubKey->u.rsa.modulus.data,
						pubKey->u.rsa.modulus.len);
	    break;
	case dsaKey:
	    PK11_SETATTRS(&theTemplate,CKA_VALUE, pubKey->u.dsa.publicValue.data,
						pubKey->u.dsa.publicValue.len);
	    break;
	case dhKey:
	    PK11_SETATTRS(&theTemplate,CKA_VALUE, pubKey->u.dh.publicValue.data,
						pubKey->u.dh.publicValue.len);
	    break;
	case ecKey:
#ifdef NSS_ENABLE_ECC
	    PK11_SETATTRS(&theTemplate,CKA_EC_POINT, 
			  pubKey->u.ec.publicValue.data,
			  pubKey->u.ec.publicValue.len);
#endif /* NSS_ENABLE_ECC */
	    break;
	case keaKey:
	case fortezzaKey:
	case nullKey:
	    /* fall through and return false */
	    break;
	}

	if (theTemplate.ulValueLen == 0) {
	    SECKEY_DestroyPublicKey(pubKey);
	    return PR_FALSE;
	}
	pk11_SignedToUnsigned(&theTemplate);
	if (pk11_FindObjectByTemplate(slot,&theTemplate,1) != CK_INVALID_HANDLE) {
	    SECKEY_DestroyPublicKey(pubKey);
	    return PR_TRUE;
	}
	SECKEY_DestroyPublicKey(pubKey);
    }
    return PR_FALSE;
}

/*
 * Check out if a cert has ID of zero. This is a magic ID that tells
 * NSS that this cert may be an automagically trusted cert.
 * The Cert has to be self signed as well. That check is done elsewhere.
 *  
 */
PRBool
pk11_isID0(PK11SlotInfo *slot, CK_OBJECT_HANDLE certID)
{
    CK_ATTRIBUTE keyID = {CKA_ID, NULL, 0};
    PRBool isZero = PR_FALSE;
    int i;
    CK_RV crv;


    crv = PK11_GetAttributes(NULL,slot,certID,&keyID,1);
    if (crv != CKR_OK) {
	return isZero;
    }

    if (keyID.ulValueLen != 0) {
	char *value = (char *)keyID.pValue;
	isZero = PR_TRUE; /* ID exists, may be zero */
	for (i=0; i < (int) keyID.ulValueLen; i++) {
	    if (value[i] != 0) {
		isZero = PR_FALSE; /* nope */
		break;
	    }
	}
    }
    PORT_Free(keyID.pValue);
    return isZero;

}

/*
 * Create an NSSCertificate from a slot/certID pair, return it as a
 * CERTCertificate.
 */
static CERTCertificate
*pk11_fastCert(PK11SlotInfo *slot, CK_OBJECT_HANDLE certID, 
			CK_ATTRIBUTE *privateLabel, char **nickptr)
{
    NSSCertificate *c;
    nssCryptokiObject *co;
    nssPKIObject *pkio;
    NSSToken *token;
    NSSTrustDomain *td = STAN_GetDefaultTrustDomain();

    /* Get the cryptoki object from the handle */
    token = PK11Slot_GetNSSToken(slot);
    co = nssCryptokiObject_Create(token, token->defaultSession, certID);
    if (!co) {
	return NULL;
    }

    /* Create a PKI object from the cryptoki instance */
    pkio = nssPKIObject_Create(NULL, co, td, NULL);
    if (!pkio) {
	nssCryptokiObject_Destroy(co);
	return NULL;
    }

    /* Create a certificate */
    c = nssCertificate_Create(pkio);
    if (!c) {
	nssPKIObject_Destroy(pkio);
	return NULL;
    }

    nssTrustDomain_AddCertsToCache(td, &c, 1);

    /* Build the old-fashioned nickname */
    if ((nickptr) && (co->label)) {
	CK_ATTRIBUTE label, id;
	label.type = CKA_LABEL;
	label.pValue = co->label;
	label.ulValueLen = PORT_Strlen(co->label);
	id.type = CKA_ID;
	id.pValue = c->id.data;
	id.ulValueLen = c->id.size;
	*nickptr = pk11_buildNickname(slot, &label, privateLabel, &id);
    }
    return STAN_GetCERTCertificate(c);
}

/*
 * Build an CERTCertificate structure from a PKCS#11 object ID.... certID
 * Must be a CertObject. This code does not explicitly checks that.
 */
CERTCertificate *
PK11_MakeCertFromHandle(PK11SlotInfo *slot,CK_OBJECT_HANDLE certID,
						CK_ATTRIBUTE *privateLabel)
{
    char * nickname = NULL;
    CERTCertificate *cert = NULL;
    CERTCertTrust *trust;
    PRBool isFortezzaRootCA = PR_FALSE;
    PRBool swapNickname = PR_FALSE;

    cert = pk11_fastCert(slot,certID,privateLabel, &nickname);
    if (cert == NULL) goto loser;
	
    if (nickname) {
	if (cert->nickname != NULL) {
		cert->dbnickname = cert->nickname;
	} 
	cert->nickname = PORT_ArenaStrdup(cert->arena,nickname);
	PORT_Free(nickname);
	nickname = NULL;
	swapNickname = PR_TRUE;
    }

    /* remember where this cert came from.... If we have just looked
     * it up from the database and it already has a slot, don't add a new
     * one. */
    if (cert->slot == NULL) {
	cert->slot = PK11_ReferenceSlot(slot);
	cert->pkcs11ID = certID;
	cert->ownSlot = PR_TRUE;
	cert->series = slot->series;
    }

    trust = (CERTCertTrust*)PORT_ArenaAlloc(cert->arena, sizeof(CERTCertTrust));
    if (trust == NULL) goto loser;
    PORT_Memset(trust,0, sizeof(CERTCertTrust));
    cert->trust = trust;

    

    if(! pk11_HandleTrustObject(slot, cert, trust) ) {
	unsigned int type;

	/* build some cert trust flags */
	if (CERT_IsCACert(cert, &type)) {
	    unsigned int trustflags = CERTDB_VALID_CA;
	   
	    /* Allow PKCS #11 modules to give us trusted CA's. We only accept
	     * valid CA's which are self-signed here. They must have an object
	     * ID of '0'.  */ 
	    if (pk11_isID0(slot,certID) && 
		SECITEM_CompareItem(&cert->derSubject,&cert->derIssuer)
							   == SECEqual) {
		trustflags |= CERTDB_TRUSTED_CA;
		/* is the slot a fortezza card? allow the user or
		 * admin to turn on objectSigning, but don't turn
		 * full trust on explicitly */
		if (PK11_DoesMechanism(slot,CKM_KEA_KEY_DERIVE)) {
		    trust->objectSigningFlags |= CERTDB_VALID_CA;
		    isFortezzaRootCA = PR_TRUE;
		}
	    }
	    if ((type & NS_CERT_TYPE_SSL_CA) == NS_CERT_TYPE_SSL_CA) {
		trust->sslFlags |= trustflags;
	    }
	    if ((type & NS_CERT_TYPE_EMAIL_CA) == NS_CERT_TYPE_EMAIL_CA) {
		trust->emailFlags |= trustflags;
	    }
	    if ((type & NS_CERT_TYPE_OBJECT_SIGNING_CA) 
					== NS_CERT_TYPE_OBJECT_SIGNING_CA) {
		trust->objectSigningFlags |= trustflags;
	    }
	}
    }

    if (PK11_IsUserCert(slot,cert,certID)) {
	trust->sslFlags |= CERTDB_USER;
	trust->emailFlags |= CERTDB_USER;
	/*    trust->objectSigningFlags |= CERTDB_USER; */
    }

    return cert;

loser:
    if (nickname) PORT_Free(nickname);
    if (cert) CERT_DestroyCertificate(cert);
    return NULL;
}

	
/*
 * Build get a certificate from a private key
 */
CERTCertificate *
PK11_GetCertFromPrivateKey(SECKEYPrivateKey *privKey)
{
    PK11SlotInfo *slot = privKey->pkcs11Slot;
    CK_OBJECT_HANDLE handle = privKey->pkcs11ID;
    CK_OBJECT_HANDLE certID = 
		PK11_MatchItem(slot,handle,CKO_CERTIFICATE);
    CERTCertificate *cert;

    if (certID == CK_INVALID_HANDLE) {
	PORT_SetError(SSL_ERROR_NO_CERTIFICATE);
	return NULL;
    }
    cert = PK11_MakeCertFromHandle(slot,certID,NULL);
    return (cert);

}

/*
 * delete a cert and it's private key (if no other certs are pointing to the
 * private key.
 */
SECStatus
PK11_DeleteTokenCertAndKey(CERTCertificate *cert,void *wincx)
{
    SECKEYPrivateKey *privKey = PK11_FindKeyByAnyCert(cert,wincx);
    CK_OBJECT_HANDLE pubKey;
    PK11SlotInfo *slot = NULL;

    pubKey = pk11_FindPubKeyByAnyCert(cert, &slot, wincx);
    if (privKey) {
#ifdef NSS_CLASSIC
    	PK11_DestroyTokenObject(cert->slot,cert->pkcs11ID);
#else
	/* For 3.4, utilize the generic cert delete function */
	SEC_DeletePermCertificate(cert);
#endif
	PK11_DeleteTokenPrivateKey(privKey, PR_FALSE);
    }
    if ((pubKey != CK_INVALID_HANDLE) && (slot != NULL)) { 
    	PK11_DestroyTokenObject(slot,pubKey);
        PK11_FreeSlot(slot);
    }
    return SECSuccess;
}

/*
 * cert callback structure
 */
typedef struct pk11DoCertCallbackStr {
	SECStatus(* callback)(PK11SlotInfo *slot, CERTCertificate*, void *);
	SECStatus(* noslotcallback)(CERTCertificate*, void *);
	SECStatus(* itemcallback)(CERTCertificate*, SECItem *, void *);
	void *callbackArg;
} pk11DoCertCallback;

#ifdef NSS_CLASSIC
/*
 * callback to map object handles to certificate structures.
 */
static SECStatus
pk11_DoCerts(PK11SlotInfo *slot, CK_OBJECT_HANDLE certID, void *arg)
{
    CERTCertificate *cert;
    pk11DoCertCallback *certcb = (pk11DoCertCallback *) arg;

    cert = PK11_MakeCertFromHandle(slot, certID, NULL);

    if (cert == NULL) {
	return SECFailure;
    }

    if (certcb ) {
	if (certcb->callback) {
	    (*certcb->callback)(slot, cert, certcb->callbackArg);
	}
	if (certcb->noslotcallback) {
	    (*certcb->noslotcallback)(cert, certcb->callbackArg);
	}
	if (certcb->itemcallback) {
	    (*certcb->itemcallback)(cert, NULL, certcb->callbackArg);
	}
    }

    CERT_DestroyCertificate(cert);
	    
    return SECSuccess;
}
#endif

typedef struct pk11CertCallbackStr {
	SECStatus(* callback)(CERTCertificate*,SECItem *,void *);
	void *callbackArg;
} pk11CertCallback;

struct fake_der_cb_argstr
{
    SECStatus(* callback)(CERTCertificate*, SECItem *, void *);
    void *arg;
};

static SECStatus fake_der_cb(CERTCertificate *c, void *a)
{
    struct fake_der_cb_argstr *fda = (struct fake_der_cb_argstr *)a;
    return (*fda->callback)(c, &c->derCert, fda->arg);
}

/*
 * Extract all the certs on a card from a slot.
 */
SECStatus
PK11_TraverseSlotCerts(SECStatus(* callback)(CERTCertificate*,SECItem *,void *),
						void *arg, void *wincx) {
#ifdef NSS_CLASSIC
    pk11DoCertCallback caller;
    pk11TraverseSlot creater;
    CK_ATTRIBUTE theTemplate;
    CK_OBJECT_CLASS certClass = CKO_CERTIFICATE;

    PK11_SETATTRS(&theTemplate, CKA_CLASS, &certClass, sizeof(certClass));

    caller.callback = NULL;
    caller.noslotcallback = NULL;
    caller.itemcallback = callback;
    caller.callbackArg = arg;
    creater.callback = pk11_DoCerts;
    creater.callbackArg = (void *) & caller;
    creater.findTemplate = &theTemplate;
    creater.templateCount = 1;

    return pk11_TraverseAllSlots(PK11_TraverseSlot, &creater, wincx);
#else
    NSSTrustDomain *defaultTD = STAN_GetDefaultTrustDomain();
    struct fake_der_cb_argstr fda;
    struct nss3_cert_cbstr pk11cb;

    /* authenticate to the tokens first */
    (void) pk11_TraverseAllSlots( NULL, NULL, wincx);

    fda.callback = callback;
    fda.arg = arg;
    pk11cb.callback = fake_der_cb;
    pk11cb.arg = &fda;
    NSSTrustDomain_TraverseCertificates(defaultTD, convert_cert, &pk11cb);
    return SECSuccess;
#endif
}

static void
transfer_token_certs_to_collection(nssList *certList, NSSToken *token, 
                                   nssPKIObjectCollection *collection)
{
    NSSCertificate **certs;
    PRUint32 i, count;
    NSSToken **tokens, **tp;
    count = nssList_Count(certList);
    if (count == 0) {
	return;
    }
    certs = nss_ZNEWARRAY(NULL, NSSCertificate *, count);
    if (!certs) {
	return;
    }
    nssList_GetArray(certList, (void **)certs, count);
    for (i=0; i<count; i++) {
	tokens = nssPKIObject_GetTokens(&certs[i]->object, NULL);
	if (tokens) {
	    for (tp = tokens; *tp; tp++) {
		if (*tp == token) {
		    nssPKIObjectCollection_AddObject(collection, 
		                                     (nssPKIObject *)certs[i]);
		}
	    }
	    nssTokenArray_Destroy(tokens);
	}
	/* *must* be a valid CERTCertificate, came from cache */
	CERT_DestroyCertificate(STAN_GetCERTCertificate(certs[i]));
    }
    nss_ZFreeIf(certs);
}

CERTCertificate *
PK11_FindCertFromNickname(char *nickname, void *wincx) 
{
#ifdef NSS_CLASSIC
    PK11SlotInfo *slot;
    int count=0;
    CK_OBJECT_HANDLE *certID = PK11_FindObjectsFromNickname(nickname,&slot,
				 		CKO_CERTIFICATE, &count, wincx);
    CERTCertificate *cert;

    if (certID == CK_INVALID_HANDLE) return NULL;
    cert = PK11_MakeCertFromHandle(slot,certID[0],NULL);
    PK11_FreeSlot(slot);
    PORT_Free(certID);
    return cert;
#else
    PRStatus status;
    CERTCertificate *rvCert = NULL;
    NSSCertificate *cert = NULL;
    NSSCertificate **certs = NULL;
    NSSUsage usage;
    NSSToken *token;
    NSSTrustDomain *defaultTD = STAN_GetDefaultTrustDomain();
    PK11SlotInfo *slot = NULL;
    SECStatus rv;
    char *nickCopy;
    char *delimit = NULL;
    char *tokenName;

    usage.anyUsage = PR_TRUE;
    nickCopy = PORT_Strdup(nickname);
    if ((delimit = PORT_Strchr(nickCopy,':')) != NULL) {
	tokenName = nickCopy;
	nickname = delimit + 1;
	*delimit = '\0';
	/* find token by name */
	token = NSSTrustDomain_FindTokenByName(defaultTD, (NSSUTF8 *)tokenName);
	if (token) {
		slot = PK11_ReferenceSlot(token->pk11slot);
	}
	*delimit = ':';
    } else {
	slot = PK11_GetInternalKeySlot();
	token = PK11Slot_GetNSSToken(slot);
    }
    if (token) {
	nssList *certList;
	nssCryptokiObject **instances;
	nssPKIObjectCollection *collection;
	nssTokenSearchType tokenOnly = nssTokenSearchType_TokenOnly;
	if (!PK11_IsPresent(slot)) {
	    goto loser;
	}
   	rv = pk11_AuthenticateUnfriendly(slot, PR_TRUE, wincx);
	if (rv != SECSuccess) {
	    goto loser;
	}
	collection = nssCertificateCollection_Create(defaultTD, NULL);
	if (!collection) {
	    goto loser;
	}
	certList = nssList_Create(NULL, PR_FALSE);
	if (!certList) {
	    nssPKIObjectCollection_Destroy(collection);
	    goto loser;
	}
	(void)nssTrustDomain_GetCertsForNicknameFromCache(defaultTD, 
	                                                  nickname, 
	                                                  certList);
	transfer_token_certs_to_collection(certList, token, collection);
	instances = nssToken_FindCertificatesByNickname(token,
	                                                NULL,
	                                                nickname,
	                                                tokenOnly,
	                                                0,
	                                                &status);
	nssPKIObjectCollection_AddInstances(collection, instances, 0);
	nss_ZFreeIf(instances);
	/* if it wasn't found, repeat the process for email address */
	if (nssPKIObjectCollection_Count(collection) == 0 &&
	    PORT_Strchr(nickname, '@') != NULL) 
	{
	    char* lowercaseName = CERT_FixupEmailAddr(nickname);
	    if (lowercaseName) {
		(void)nssTrustDomain_GetCertsForEmailAddressFromCache(defaultTD, 
								      lowercaseName, 
								      certList);
		transfer_token_certs_to_collection(certList, token, collection);
		instances = nssToken_FindCertificatesByEmail(token,
							     NULL,
							     lowercaseName,
							     tokenOnly,
							     0,
							     &status);
		nssPKIObjectCollection_AddInstances(collection, instances, 0);
		nss_ZFreeIf(instances);
		PORT_Free(lowercaseName);
	    }
	}
	certs = nssPKIObjectCollection_GetCertificates(collection, 
	                                               NULL, 0, NULL);
	nssPKIObjectCollection_Destroy(collection);
	if (certs) {
	    cert = nssCertificateArray_FindBestCertificate(certs, NULL, 
	                                                   &usage, NULL);
	    if (cert) {
		rvCert = STAN_GetCERTCertificate(cert);
	    }
	    nssCertificateArray_Destroy(certs);
	}
	nssList_Destroy(certList);
    }
    if (slot) {
	PK11_FreeSlot(slot);
    }
    if (nickCopy) PORT_Free(nickCopy);
    return rvCert;
loser:
    if (slot) {
	PK11_FreeSlot(slot);
    }
    if (nickCopy) PORT_Free(nickCopy);
    return NULL;
#endif
}

CERTCertList *
PK11_FindCertsFromNickname(char *nickname, void *wincx) {
#ifdef NSS_CLASSIC
    PK11SlotInfo *slot;
    int i,count = 0;
    CK_OBJECT_HANDLE *certID = PK11_FindObjectsFromNickname(nickname,&slot,
				 		CKO_CERTIFICATE, &count, wincx);
    CERTCertList *certList = NULL;

    if (certID == NULL) return NULL;

    certList= CERT_NewCertList();

    for (i=0; i < count; i++) {
    	CERTCertificate *cert = PK11_MakeCertFromHandle(slot,certID[i],NULL);

	if (cert) CERT_AddCertToListTail(certList,cert);
    }

    if (CERT_LIST_HEAD(certList) == NULL) {
	CERT_DestroyCertList(certList);
	certList = NULL;
    }
    PK11_FreeSlot(slot);
    PORT_Free(certID);
    return certList;
#else
    char *nickCopy;
    char *delimit = NULL;
    char *tokenName;
    int i;
    CERTCertList *certList = NULL;
    nssPKIObjectCollection *collection = NULL;
    NSSCertificate **foundCerts = NULL;
    NSSTrustDomain *defaultTD = STAN_GetDefaultTrustDomain();
    NSSCertificate *c;
    NSSToken *token;
    PK11SlotInfo *slot;
    SECStatus rv;

    nickCopy = PORT_Strdup(nickname);
    if ((delimit = PORT_Strchr(nickCopy,':')) != NULL) {
	tokenName = nickCopy;
	nickname = delimit + 1;
	*delimit = '\0';
	/* find token by name */
	token = NSSTrustDomain_FindTokenByName(defaultTD, (NSSUTF8 *)tokenName);
	if (token) {
	    slot = PK11_ReferenceSlot(token->pk11slot);
	} else {
	    slot = NULL;
	}
	*delimit = ':';
    } else {
	slot = PK11_GetInternalKeySlot();
	token = PK11Slot_GetNSSToken(slot);
    }
    if (token) {
	PRStatus status;
	nssList *nameList;
	nssCryptokiObject **instances;
	nssTokenSearchType tokenOnly = nssTokenSearchType_TokenOnly;
   	rv = pk11_AuthenticateUnfriendly(slot, PR_TRUE, wincx);
	if (rv != SECSuccess) {
	    PK11_FreeSlot(slot);
    	    if (nickCopy) PORT_Free(nickCopy);
	    return NULL;
	}
	collection = nssCertificateCollection_Create(defaultTD, NULL);
	if (!collection) {
	    PK11_FreeSlot(slot);
	    if (nickCopy) PORT_Free(nickCopy);
	    return NULL;
	}
	nameList = nssList_Create(NULL, PR_FALSE);
	if (!nameList) {
	    PK11_FreeSlot(slot);
	    if (nickCopy) PORT_Free(nickCopy);
	    return NULL;
	}
	(void)nssTrustDomain_GetCertsForNicknameFromCache(defaultTD,
	                                                  nickname, 
	                                                  nameList);
	transfer_token_certs_to_collection(nameList, token, collection);
	instances = nssToken_FindCertificatesByNickname(token,
	                                                NULL,
	                                                nickname,
	                                                tokenOnly,
	                                                0,
	                                                &status);
	nssPKIObjectCollection_AddInstances(collection, instances, 0);
	nss_ZFreeIf(instances);
	nssList_Destroy(nameList);
	foundCerts = nssPKIObjectCollection_GetCertificates(collection,
	                                                    NULL, 0, NULL);
	nssPKIObjectCollection_Destroy(collection);
    }
    if (slot) {
	PK11_FreeSlot(slot);
    }
    if (nickCopy) PORT_Free(nickCopy);
    if (foundCerts) {
	PRTime now = PR_Now();
	certList = CERT_NewCertList();
	for (i=0, c = *foundCerts; c; c = foundCerts[++i]) {
	    CERTCertificate *certCert = STAN_GetCERTCertificate(c);
	    if (certCert) {
		CERT_AddCertToListSorted(certList, certCert,
			CERT_SortCBValidity, &now);
	    }
	}
	if (CERT_LIST_HEAD(certList) == NULL) {
	    CERT_DestroyCertList(certList);
	    certList = NULL;
	}
	nss_ZFreeIf(foundCerts);
    }
    return certList;
#endif
}

/*
 * extract a key ID for a certificate...
 * NOTE: We call this function from PKCS11.c If we ever use
 * pkcs11 to extract the public key (we currently do not), this will break.
 */
SECItem *
PK11_GetPubIndexKeyID(CERTCertificate *cert) {
    SECKEYPublicKey *pubk;
    SECItem *newItem = NULL;

    pubk = CERT_ExtractPublicKey(cert);
    if (pubk == NULL) return NULL;

    switch (pubk->keyType) {
    case rsaKey:
	newItem = SECITEM_DupItem(&pubk->u.rsa.modulus);
	break;
    case dsaKey:
        newItem = SECITEM_DupItem(&pubk->u.dsa.publicValue);
	break;
    case dhKey:
        newItem = SECITEM_DupItem(&pubk->u.dh.publicValue);
	break;
    case ecKey:
#ifdef NSS_ENABLE_ECC
        newItem = SECITEM_DupItem(&pubk->u.ec.publicValue);
#endif /* NSS_ENABLE_ECC */
	break;
    case fortezzaKey:
    default:
	newItem = NULL; /* Fortezza Fix later... */
    }
    SECKEY_DestroyPublicKey(pubk);
    /* make hash of it */
    return newItem;
}

/*
 * generate a CKA_ID from a certificate.
 */
SECItem *
pk11_mkcertKeyID(CERTCertificate *cert) {
    SECItem *pubKeyData = PK11_GetPubIndexKeyID(cert) ;
    SECItem *certCKA_ID;

    if (pubKeyData == NULL) return NULL;
    
    certCKA_ID = PK11_MakeIDFromPubKey(pubKeyData);
    SECITEM_FreeItem(pubKeyData,PR_TRUE);
    return certCKA_ID;
}

/*
 * Write the cert into the token.
 */
SECStatus
PK11_ImportCert(PK11SlotInfo *slot, CERTCertificate *cert, 
		CK_OBJECT_HANDLE key, char *nickname, PRBool includeTrust) 
{
#ifdef NSS_CLASSIC
    int len = 0;
    SECItem *keyID = pk11_mkcertKeyID(cert);
    CK_ATTRIBUTE keyAttrs[] = {
	{ CKA_LABEL, NULL, 0},
	{ CKA_SUBJECT, NULL, 0},
    };
    CK_OBJECT_CLASS certc = CKO_CERTIFICATE;
    CK_CERTIFICATE_TYPE certType = CKC_X_509;
    CK_OBJECT_HANDLE certID;
    CK_SESSION_HANDLE rwsession;
    CK_BBOOL cktrue = CK_TRUE;
    SECStatus rv = SECFailure;
    CK_ATTRIBUTE certAttrs[] = {
	{ CKA_ID, NULL, 0 },
	{ CKA_LABEL, NULL, 0},
	{ CKA_CLASS,  NULL, 0},
	{ CKA_TOKEN,  NULL, 0},
	{ CKA_CERTIFICATE_TYPE, NULL, 0},
	{ CKA_SUBJECT, NULL, 0},
	{ CKA_ISSUER, NULL, 0},
	{ CKA_SERIAL_NUMBER,  NULL, 0},
	{ CKA_VALUE,  NULL, 0},
	{ CKA_NETSCAPE_TRUST,  NULL, 0},
	{ CKA_NETSCAPE_EMAIL,  NULL, 0},
    };
    int certCount = sizeof(certAttrs)/sizeof(certAttrs[0]), keyCount = 2;
    int realCount = 0;
    CK_ATTRIBUTE *attrs;
    CK_RV crv;
    SECCertUsage *certUsage = NULL;
    SECItem derSerial = { 0 };
    NSSToken *token;

    if (keyID == NULL) {
	PORT_SetError(SEC_ERROR_ADDING_CERT);
	return rv;
    }

    len = ((nickname) ? PORT_Strlen(nickname) : 0);
    
    attrs = certAttrs;
    PK11_SETATTRS(attrs,CKA_ID, keyID->data, keyID->len); attrs++;
    if (nickname) {
	PK11_SETATTRS(attrs,CKA_LABEL, nickname, len ); attrs++;
    }
    PK11_SETATTRS(attrs,CKA_CLASS, &certc, sizeof(certc) ); attrs++;
    PK11_SETATTRS(attrs,CKA_TOKEN, &cktrue, sizeof(cktrue) ); attrs++;
    PK11_SETATTRS(attrs,CKA_CERTIFICATE_TYPE, &certType,
						sizeof(certType)); attrs++;
    PK11_SETATTRS(attrs,CKA_SUBJECT, cert->derSubject.data,
					 cert->derSubject.len ); attrs++;
    PK11_SETATTRS(attrs,CKA_ISSUER, cert->derIssuer.data,
					 cert->derIssuer.len ); attrs++;
    if (PR_TRUE) {
	/* CERTCertificate stores serial numbers decoded.  I need the DER
	* here.  sigh.
	*/
	CERT_SerialNumberFromDERCert(&cert->derCert, &derSerial);
	PK11_SETATTRS(attrs,CKA_SERIAL_NUMBER, derSerial.data, derSerial.len); 
	attrs++;
    }
    PK11_SETATTRS(attrs,CKA_VALUE, cert->derCert.data, 
						cert->derCert.len); attrs++;
    if (includeTrust && PK11_IsInternal(slot)) {
	certUsage = (SECCertUsage*)PORT_Alloc(sizeof(SECCertUsage));
	if(!certUsage) {
	    SECITEM_FreeItem(keyID,PR_TRUE);
	    PORT_SetError(SEC_ERROR_NO_MEMORY);
	    return rv;
	}
	*certUsage = certUsageUserCertImport;
	PK11_SETATTRS(attrs,CKA_NETSCAPE_TRUST, certUsage,
							 sizeof(SECCertUsage));
	attrs++;
	if (cert->emailAddr && cert->emailAddr[0]) {
	    PK11_SETATTRS(attrs,CKA_NETSCAPE_EMAIL, cert->emailAddr,
						PORT_Strlen(cert->emailAddr);
	    attrs++;
	}
    }
    realCount = attrs - certAttrs;
    PORT_Assert(realCount <= certCount);

    attrs = keyAttrs;
    if(nickname) {
	PK11_SETATTRS(attrs,CKA_LABEL, nickname, len ); attrs++;
    }
    PK11_SETATTRS(attrs,CKA_SUBJECT, cert->derSubject.data,
					 cert->derSubject.len );

    if(!nickname) {
	certCount--;
	keyCount--;
    }

    rwsession = PK11_GetRWSession(slot);
    if (key != CK_INVALID_HANDLE) {
	crv = PK11_GETTAB(slot)->C_SetAttributeValue(rwsession,key,keyAttrs,
								keyCount);
	if (crv != CKR_OK) {
	    PORT_SetError( PK11_MapError(crv) );
	    goto done;
	}
    }

    crv = PK11_GETTAB(slot)->
			C_CreateObject(rwsession,certAttrs,realCount,&certID);
    if (crv == CKR_OK) {
	rv = SECSuccess;
    } else {
	PORT_SetError( PK11_MapError(crv) );
    }

    if (!cert->nickname && nickname) {
	cert->nickname = PORT_ArenaStrdup(cert->arena, nickname);
    }

    cert->pkcs11ID = certID;
    cert->dbhandle = STAN_GetDefaultTrustDomain();
    if (cert->slot == NULL) {
	cert->slot = PK11_ReferenceSlot(slot);
	cert->series = slot->series;
	cert->ownSlot = PR_TRUE;
	if (cert->nssCertificate) {
	    nssCryptokiInstance *instance;
	    NSSCertificate *c = cert->nssCertificate;
	    instance = nss_ZNEW(c->object.arena, nssCryptokiInstance);
	    instance->token = slot->nssToken;
	    instance->handle = cert->pkcs11ID;
	    instance->isTokenObject = PR_TRUE;
	    nssPKIObject_AddInstance(&c->object, instance);
	} else {
	    cert->nssCertificate = STAN_GetNSSCertificate(cert);
	}
    }
    cert->trust = nssTrust_GetCERTCertTrustForCert(cert->nssCertificate, cert);
    token = PK11Slot_GetNSSToken(slot);

done:
    if (derSerial.data) PORT_Free(derSerial.data);
    SECITEM_FreeItem(keyID,PR_TRUE);
    PK11_RestoreROSession(slot,rwsession);
    if(certUsage) {
	PORT_Free(certUsage);
    }
    return rv;
#else
    PRStatus status;
    NSSCertificate *c;
    nssCryptokiObject *keyobj, *certobj;
    NSSToken *token = PK11Slot_GetNSSToken(slot);
    SECItem *keyID = pk11_mkcertKeyID(cert);
    char *emailAddr = NULL;

    if (keyID == NULL) {
	goto loser;
    }

    if (PK11_IsInternal(slot) && cert->emailAddr && cert->emailAddr[0]) {
	emailAddr = cert->emailAddr;
    }

    /* need to get the cert as a stan cert */
    if (cert->nssCertificate) {
	c = cert->nssCertificate;
    } else {
	c = STAN_GetNSSCertificate(cert);
    }

    if (c->object.cryptoContext) {
	/* Delete the temp instance */
	NSSCryptoContext *cc = c->object.cryptoContext;
	nssCertificateStore_Lock(cc->certStore);
	nssCertificateStore_RemoveCertLOCKED(cc->certStore, c);
	nssCertificateStore_Unlock(cc->certStore);
	c->object.cryptoContext = NULL;
	cert->istemp = PR_FALSE;
	cert->isperm = PR_TRUE;
    }

    /* set the id for the cert */
    nssItem_Create(c->object.arena, &c->id, keyID->len, keyID->data);
    if (!c->id.data) {
	goto loser;
    }

    if (key != CK_INVALID_HANDLE) {
	/* create an object for the key, ... */
	keyobj = nss_ZNEW(NULL, nssCryptokiObject);
	if (!keyobj) {
	    goto loser;
	}
	keyobj->token = nssToken_AddRef(token);
	keyobj->handle = key;
	keyobj->isTokenObject = PR_TRUE;

	/* ... in order to set matching attributes for the key */
	status = nssCryptokiPrivateKey_SetCertificate(keyobj, NULL, nickname, 
	                                              &c->id, &c->subject);
	nssCryptokiObject_Destroy(keyobj);
	if (status != PR_SUCCESS) {
	    goto loser;
	}
    }

    /* do the token import */
    certobj = nssToken_ImportCertificate(token, NULL,
                                         NSSCertificateType_PKIX,
                                         &c->id,
                                         nickname,
                                         &c->encoding,
                                         &c->issuer,
                                         &c->subject,
                                         &c->serial,
					 emailAddr,
                                         PR_TRUE);
    if (!certobj) {
	if (NSS_GetError() == NSS_ERROR_INVALID_CERTIFICATE) {
	    PORT_SetError(SEC_ERROR_REUSED_ISSUER_AND_SERIAL);
	    SECITEM_FreeItem(keyID,PR_TRUE);
	    return SECFailure;
	}
	goto loser;
    }
    /* add the new instance to the cert, force an update of the
     * CERTCertificate, and finish
     */
    nssPKIObject_AddInstance(&c->object, certobj);
    nssTrustDomain_AddCertsToCache(STAN_GetDefaultTrustDomain(), &c, 1);
    (void)STAN_ForceCERTCertificateUpdate(c);
    SECITEM_FreeItem(keyID,PR_TRUE);
    return SECSuccess;
loser:
    SECITEM_FreeItem(keyID,PR_TRUE);
    PORT_SetError(SEC_ERROR_ADDING_CERT);
    return SECFailure;
#endif
}

SECStatus
PK11_ImportDERCert(PK11SlotInfo *slot, SECItem *derCert,
		CK_OBJECT_HANDLE key, char *nickname, PRBool includeTrust) {
    CERTCertificate *cert;
    SECStatus rv;

    cert = CERT_NewTempCertificate(CERT_GetDefaultCertDB(),
                                   derCert, NULL, PR_FALSE, PR_TRUE);
    if (cert == NULL) return SECFailure;

    rv = PK11_ImportCert(slot, cert, key, nickname, includeTrust);
    CERT_DestroyCertificate (cert);
    return rv;
}

/*
 * get a certificate handle, look at the cached handle first..
 */
CK_OBJECT_HANDLE
pk11_getcerthandle(PK11SlotInfo *slot, CERTCertificate *cert, 
					CK_ATTRIBUTE *theTemplate,int tsize)
{
    CK_OBJECT_HANDLE certh;

    if (cert->slot == slot) {
	certh = cert->pkcs11ID;
	if ((certh == CK_INVALID_HANDLE) ||
			(cert->series != slot->series)) {
    	     certh = pk11_FindObjectByTemplate(slot,theTemplate,tsize);
	     cert->pkcs11ID = certh;
	     cert->series = slot->series;
	}
    } else {
    	certh = pk11_FindObjectByTemplate(slot,theTemplate,tsize);
    }
    return certh;
}

/*
 * return the private key From a given Cert
 */
SECKEYPrivateKey *
PK11_FindPrivateKeyFromCert(PK11SlotInfo *slot, CERTCertificate *cert,
								 void *wincx) {
    CK_OBJECT_CLASS certClass = CKO_CERTIFICATE;
    CK_ATTRIBUTE theTemplate[] = {
	{ CKA_VALUE, NULL, 0 },
	{ CKA_CLASS, NULL, 0 }
    };
    /* if you change the array, change the variable below as well */
    int tsize = sizeof(theTemplate)/sizeof(theTemplate[0]);
    CK_OBJECT_HANDLE certh;
    CK_OBJECT_HANDLE keyh;
    CK_ATTRIBUTE *attrs = theTemplate;
    SECStatus rv;

    PK11_SETATTRS(attrs, CKA_VALUE, cert->derCert.data, 
						cert->derCert.len); attrs++;
    PK11_SETATTRS(attrs, CKA_CLASS, &certClass, sizeof(certClass));

    /*
     * issue the find
     */
    rv = pk11_AuthenticateUnfriendly(slot, PR_TRUE, wincx);
    if (rv != SECSuccess) {
	return NULL;
    }

    certh = pk11_getcerthandle(slot,cert,theTemplate,tsize);
    if (certh == CK_INVALID_HANDLE) {
	return NULL;
    }
    keyh = PK11_MatchItem(slot,certh,CKO_PRIVATE_KEY);
    if ((keyh == CK_INVALID_HANDLE) && 
			(PORT_GetError() == SSL_ERROR_NO_CERTIFICATE) && 
			pk11_LoginStillRequired(slot, wincx)) {
	/* try it again authenticated */
	rv = PK11_Authenticate(slot, PR_TRUE, wincx);
	if (rv != SECSuccess) {
	    return NULL;
	}
	keyh = PK11_MatchItem(slot,certh,CKO_PRIVATE_KEY);
    }
    if (keyh == CK_INVALID_HANDLE) { 
	return NULL; 
    }
    return PK11_MakePrivKey(slot, nullKey, PR_TRUE, keyh, wincx);
} 

/*
 * import a cert for a private key we have already generated. Set the label
 * on both to be the nickname. This is for the Key Gen, orphaned key case.
 */
PK11SlotInfo *
PK11_KeyForCertExists(CERTCertificate *cert, CK_OBJECT_HANDLE *keyPtr, 
								void *wincx) {
    PK11SlotList *list;
    PK11SlotListElement *le;
    SECItem *keyID;
    CK_OBJECT_HANDLE key;
    PK11SlotInfo *slot = NULL;
    SECStatus rv;

    keyID = pk11_mkcertKeyID(cert);
    /* get them all! */
    list = PK11_GetAllTokens(CKM_INVALID_MECHANISM,PR_FALSE,PR_TRUE,wincx);
    if ((keyID == NULL) || (list == NULL)) {
	if (keyID) SECITEM_FreeItem(keyID,PR_TRUE);
	if (list) PK11_FreeSlotList(list);
    	return NULL;
    }

    /* Look for the slot that holds the Key */
    for (le = list->head ; le; le = le->next) {
	key = pk11_FindPrivateKeyFromCertID(le->slot,keyID);
	if ((key == CK_INVALID_HANDLE) && 
			(PORT_GetError() == SSL_ERROR_NO_CERTIFICATE) && 
			pk11_LoginStillRequired(le->slot,wincx)) {
	    /* authenticate and try again */
	    rv = PK11_Authenticate(le->slot, PR_TRUE, wincx);
	    if (rv != SECSuccess) continue;
	    key = pk11_FindPrivateKeyFromCertID(le->slot,keyID);
	}
	if (key != CK_INVALID_HANDLE) {
	    slot = PK11_ReferenceSlot(le->slot);
	    if (keyPtr) *keyPtr = key;
	    break;
	}
    }

    SECITEM_FreeItem(keyID,PR_TRUE);
    PK11_FreeSlotList(list);
    return slot;

}
/*
 * import a cert for a private key we have already generated. Set the label
 * on both to be the nickname. This is for the Key Gen, orphaned key case.
 */
PK11SlotInfo *
PK11_KeyForDERCertExists(SECItem *derCert, CK_OBJECT_HANDLE *keyPtr, 
								void *wincx) {
    CERTCertificate *cert;
    PK11SlotInfo *slot = NULL;

    /* letting this use go -- the only thing that the cert is used for is
     * to get the ID attribute.
     */
    cert = CERT_DecodeDERCertificate(derCert, PR_FALSE, NULL);
    if (cert == NULL) return NULL;

    slot = PK11_KeyForCertExists(cert, keyPtr, wincx);
    CERT_DestroyCertificate (cert);
    return slot;
}

PK11SlotInfo *
PK11_ImportCertForKey(CERTCertificate *cert, char *nickname,void *wincx) {
    PK11SlotInfo *slot = NULL;
    CK_OBJECT_HANDLE key;

    slot = PK11_KeyForCertExists(cert,&key,wincx);

    if (slot) {
	if (PK11_ImportCert(slot,cert,key,nickname,PR_FALSE) != SECSuccess) {
	    PK11_FreeSlot(slot);
	    slot = NULL;
	}
    } else {
	PORT_SetError(SEC_ERROR_ADDING_CERT);
    }

    return slot;
}

PK11SlotInfo *
PK11_ImportDERCertForKey(SECItem *derCert, char *nickname,void *wincx) {
    CERTCertificate *cert;
    PK11SlotInfo *slot = NULL;

    cert = CERT_NewTempCertificate(CERT_GetDefaultCertDB(),
                                   derCert, NULL, PR_FALSE, PR_TRUE);
    if (cert == NULL) return NULL;

    slot = PK11_ImportCertForKey(cert, nickname, wincx);
    CERT_DestroyCertificate (cert);
    return slot;
}

static CK_OBJECT_HANDLE
pk11_FindCertObjectByTemplate(PK11SlotInfo **slotPtr, 
		CK_ATTRIBUTE *searchTemplate, int count, void *wincx) {
    PK11SlotList *list;
    PK11SlotListElement *le;
    CK_OBJECT_HANDLE certHandle = CK_INVALID_HANDLE;
    PK11SlotInfo *slot = NULL;
    SECStatus rv;

    *slotPtr = NULL;

    /* get them all! */
    list = PK11_GetAllTokens(CKM_INVALID_MECHANISM,PR_FALSE,PR_TRUE,wincx);
    if (list == NULL) {
	if (list) PK11_FreeSlotList(list);
    	return CK_INVALID_HANDLE;
    }


    /* Look for the slot that holds the Key */
    for (le = list->head ; le; le = le->next) {
	rv = pk11_AuthenticateUnfriendly(le->slot, PR_TRUE, wincx);
	if (rv != SECSuccess) continue;

	certHandle = pk11_FindObjectByTemplate(le->slot,searchTemplate,count);
	if (certHandle != CK_INVALID_HANDLE) {
	    slot = PK11_ReferenceSlot(le->slot);
	    break;
	}
    }

    PK11_FreeSlotList(list);

    if (slot == NULL) {
	return CK_INVALID_HANDLE;
    }
    *slotPtr = slot;
    return certHandle;
}

CERTCertificate *
PK11_FindCertByIssuerAndSNOnToken(PK11SlotInfo *slot, 
				CERTIssuerAndSN *issuerSN, void *wincx)
{
    CERTCertificate *rvCert = NULL;
    NSSCertificate *cert = NULL;
    NSSDER issuer, serial;
    NSSTrustDomain *td = STAN_GetDefaultTrustDomain();
    NSSToken *token = slot->nssToken;
    nssSession *session;
    nssCryptokiObject *instance = NULL;
    nssPKIObject *object = NULL;
    SECItem *derSerial;
    PRStatus status;

    /* Paranoia */
    if (token == NULL) {
	PORT_SetError(SEC_ERROR_NO_TOKEN);
	return NULL;
    }


    /* PKCS#11 needs to use DER-encoded serial numbers.  Create a
     * CERTIssuerAndSN that actually has the encoded value and pass that
     * to PKCS#11 (and the crypto context).
     */
    derSerial = SEC_ASN1EncodeItem(NULL, NULL,
                                   &issuerSN->serialNumber,
                                   SEC_IntegerTemplate);
    if (!derSerial) {
	return NULL;
    }

    NSSITEM_FROM_SECITEM(&issuer, &issuerSN->derIssuer);
    NSSITEM_FROM_SECITEM(&serial, derSerial);

    session = nssToken_GetDefaultSession(token);
    if (!session) {
	goto loser;
    }

    instance = nssToken_FindCertificateByIssuerAndSerialNumber(token,session,
		&issuer, &serial, nssTokenSearchType_TokenForced, &status);

    SECITEM_FreeItem(derSerial, PR_TRUE);

    if (!instance) {
	goto loser;
    }
    object = nssPKIObject_Create(NULL, instance, td, NULL);
    if (!object) {
	goto loser;
    }
    instance = NULL; /* adopted by the previous call */
    cert = nssCertificate_Create(object);
    if (!cert) {
	goto loser;
    }
    object = NULL; /* adopted by the previous call */
    nssTrustDomain_AddCertsToCache(td, &cert,1);
    rvCert = STAN_GetCERTCertificate(cert);
    if (!rvCert) {
	goto loser;
    }
    return rvCert;

loser:
    if (instance) {
	nssCryptokiObject_Destroy(instance);
    }
    if (object) {
	nssPKIObject_Destroy(object);
    }
    if (cert) {
	nssCertificate_Destroy(cert);
    }
    return NULL;
}

/*
 * We're looking for a cert which we have the private key for that's on the
 * list of recipients. This searches one slot.
 * this is the new version for NSS SMIME code
 * this stuff should REALLY be in the SMIME code, but some things in here are not public
 * (they should be!)
 */
static CERTCertificate *
pk11_FindCertObjectByRecipientNew(PK11SlotInfo *slot, NSSCMSRecipient **recipientlist, int *rlIndex, void *pwarg)
{
    NSSCMSRecipient *ri = NULL;
    int i;

    for (i=0; (ri = recipientlist[i]) != NULL; i++) {
	CERTCertificate *cert = NULL;
	if (ri->kind == RLSubjKeyID) {
	    SECItem *derCert = cert_FindDERCertBySubjectKeyID(ri->id.subjectKeyID);
	    if (derCert) {
		cert = PK11_FindCertFromDERCertItem(slot, derCert, pwarg);
		SECITEM_FreeItem(derCert, PR_TRUE);
	    }
	} else {
	    cert = PK11_FindCertByIssuerAndSNOnToken(slot, ri->id.issuerAndSN, 
						     pwarg);
	}
	if (cert) {
	    /* this isn't our cert */
	    if ((cert->trust == NULL) ||
       		((cert->trust->emailFlags & CERTDB_USER) != CERTDB_USER)) {
		 CERT_DestroyCertificate(cert);
		continue;
	    }
	    ri->slot = PK11_ReferenceSlot(slot);
	    *rlIndex = i;
	    return cert;
	}
    }
    *rlIndex = -1;
    return NULL;
}

/*
 * This function is the same as above, but it searches all the slots.
 * this is the new version for NSS SMIME code
 * this stuff should REALLY be in the SMIME code, but some things in here are not public
 * (they should be!)
 */
static CERTCertificate *
pk11_AllFindCertObjectByRecipientNew(NSSCMSRecipient **recipientlist, void *wincx, int *rlIndex)
{
    PK11SlotList *list;
    PK11SlotListElement *le;
    CERTCertificate *cert = NULL;
    SECStatus rv;

    /* get them all! */
    list = PK11_GetAllTokens(CKM_INVALID_MECHANISM,PR_FALSE,PR_TRUE,wincx);
    if (list == NULL) {
	if (list) PK11_FreeSlotList(list);
    	return CK_INVALID_HANDLE;
    }

    /* Look for the slot that holds the Key */
    for (le = list->head ; le; le = le->next) {
	rv = pk11_AuthenticateUnfriendly(le->slot, PR_TRUE, wincx);
	if (rv != SECSuccess) continue;

	cert = pk11_FindCertObjectByRecipientNew(le->slot, 
					recipientlist, rlIndex, wincx);
	if (cert)
	    break;
    }

    PK11_FreeSlotList(list);

    return cert;
}

/*
 * We're looking for a cert which we have the private key for that's on the
 * list of recipients. This searches one slot.
 */
static CERTCertificate *
pk11_FindCertObjectByRecipient(PK11SlotInfo *slot, 
	SEC_PKCS7RecipientInfo **recipientArray,
	SEC_PKCS7RecipientInfo **rip, void *pwarg)
{
    SEC_PKCS7RecipientInfo *ri = NULL;
    int i;

    for (i=0; (ri = recipientArray[i]) != NULL; i++) {
	CERTCertificate *cert;

	cert = PK11_FindCertByIssuerAndSNOnToken(slot, ri->issuerAndSN, 
								pwarg);
        if (cert) {
	    /* this isn't our cert */
	    if ((cert->trust == NULL) ||
       		((cert->trust->emailFlags & CERTDB_USER) != CERTDB_USER)) {
		 CERT_DestroyCertificate(cert);
		continue;
	    }
	    *rip = ri;
	    return cert;
	}

    }
    *rip = NULL;
    return NULL;
}

/*
 * This function is the same as above, but it searches all the slots.
 */
static CERTCertificate *
pk11_AllFindCertObjectByRecipient(PK11SlotInfo **slotPtr, 
	SEC_PKCS7RecipientInfo **recipientArray,SEC_PKCS7RecipientInfo **rip,
							void *wincx) {
    PK11SlotList *list;
    PK11SlotListElement *le;
    CERTCertificate * cert = NULL;
    PK11SlotInfo *slot = NULL;
    SECStatus rv;

    *slotPtr = NULL;

    /* get them all! */
    list = PK11_GetAllTokens(CKM_INVALID_MECHANISM,PR_FALSE,PR_TRUE,wincx);
    if (list == NULL) {
	if (list) PK11_FreeSlotList(list);
    	return CK_INVALID_HANDLE;
    }

    *rip = NULL;

    /* Look for the slot that holds the Key */
    for (le = list->head ; le; le = le->next) {
	rv = pk11_AuthenticateUnfriendly(le->slot, PR_TRUE, wincx);
	if (rv != SECSuccess) continue;

	cert = pk11_FindCertObjectByRecipient(le->slot, recipientArray, 
							rip, wincx);
	if (cert) {
	    slot = PK11_ReferenceSlot(le->slot);
	    break;
	}
    }

    PK11_FreeSlotList(list);

    if (slot == NULL) {
	return NULL;
    }
    *slotPtr = slot;
    PORT_Assert(cert != NULL);
    return cert;
}

/*
 * We need to invert the search logic for PKCS 7 because if we search for
 * each cert on the list over all the slots, we wind up with lots of spurious
 * password prompts. This way we get only one password prompt per slot, at
 * the max, and most of the time we can find the cert, and only prompt for
 * the key...
 */
CERTCertificate *
PK11_FindCertAndKeyByRecipientList(PK11SlotInfo **slotPtr, 
	SEC_PKCS7RecipientInfo **array, SEC_PKCS7RecipientInfo **rip,
				SECKEYPrivateKey**privKey, void *wincx)
{
    CERTCertificate *cert = NULL;

    *privKey = NULL;
    *slotPtr = NULL;
    cert = pk11_AllFindCertObjectByRecipient(slotPtr,array,rip,wincx);
    if (!cert) {
	return NULL;
    }

    *privKey = PK11_FindKeyByAnyCert(cert, wincx);
    if (*privKey == NULL) {
	goto loser;
    }

    return cert;
loser:
    if (cert) CERT_DestroyCertificate(cert);
    if (*slotPtr) PK11_FreeSlot(*slotPtr);
    *slotPtr = NULL;
    return NULL;
}

static PRCallOnceType keyIDHashCallOnce;

static PRStatus PR_CALLBACK
pk11_keyIDHash_populate(void *wincx)
{
    CERTCertList     *certList;
    CERTCertListNode *node = NULL;
    SECItem           subjKeyID = {siBuffer, NULL, 0};

    certList = PK11_ListCerts(PK11CertListUser, wincx);
    if (!certList) {
	return PR_FAILURE;
    }

    for (node = CERT_LIST_HEAD(certList);
         !CERT_LIST_END(node, certList);
         node = CERT_LIST_NEXT(node)) {
	if (CERT_FindSubjectKeyIDExtension(node->cert, 
	                                   &subjKeyID) == SECSuccess && 
	    subjKeyID.data != NULL) {
	    cert_AddSubjectKeyIDMapping(&subjKeyID, node->cert);
	    SECITEM_FreeItem(&subjKeyID, PR_FALSE);
	}
    }
    CERT_DestroyCertList(certList);
    return PR_SUCCESS;
}

/*
 * This is the new version of the above function for NSS SMIME code
 * this stuff should REALLY be in the SMIME code, but some things in here are not public
 * (they should be!)
 */
int
PK11_FindCertAndKeyByRecipientListNew(NSSCMSRecipient **recipientlist, void *wincx)
{
    CERTCertificate *cert;
    NSSCMSRecipient *rl;
    PRStatus rv;
    int rlIndex;

    rv = PR_CallOnceWithArg(&keyIDHashCallOnce, pk11_keyIDHash_populate, wincx);
    if (rv != PR_SUCCESS)
	return -1;

    cert = pk11_AllFindCertObjectByRecipientNew(recipientlist, wincx, &rlIndex);
    if (!cert) {
	return -1;
    }

    rl = recipientlist[rlIndex];

    /* at this point, rl->slot is set */

    rl->privkey = PK11_FindKeyByAnyCert(cert, wincx);
    if (rl->privkey == NULL) {
	goto loser;
    }

    /* make a cert from the cert handle */
    rl->cert = cert;
    return rlIndex;

loser:
    if (cert) CERT_DestroyCertificate(cert);
    if (rl->slot) PK11_FreeSlot(rl->slot);
    rl->slot = NULL;
    return -1;
}

CERTCertificate *
PK11_FindCertByIssuerAndSN(PK11SlotInfo **slotPtr, CERTIssuerAndSN *issuerSN,
							 void *wincx)
{
#ifdef NSS_CLASSIC
    CK_OBJECT_HANDLE certHandle;
    CERTCertificate *cert = NULL;
    CK_OBJECT_CLASS certClass = CKO_CERTIFICATE;
    CK_ATTRIBUTE searchTemplate[] = {
 	{ CKA_CLASS, NULL, 0 },
	{ CKA_ISSUER, NULL, 0 },
	{ CKA_SERIAL_NUMBER, NULL, 0}
    };
    int count = sizeof(searchTemplate)/sizeof(CK_ATTRIBUTE);
    CK_ATTRIBUTE *attrs = searchTemplate;

    PK11_SETATTRS(attrs, CKA_CLASS, &certClass, sizeof(certClass)); attrs++;
    PK11_SETATTRS(attrs, CKA_ISSUER, issuerSN->derIssuer.data, 
					issuerSN->derIssuer.len); attrs++;
    PK11_SETATTRS(attrs, CKA_SERIAL_NUMBER, issuerSN->serialNumber.data, 
						issuerSN->serialNumber.len);

    certHandle = pk11_FindCertObjectByTemplate
					(slotPtr,searchTemplate,count,wincx);
    if (certHandle == CK_INVALID_HANDLE) {
	return NULL;
    }
    cert = PK11_MakeCertFromHandle(*slotPtr,certHandle,NULL);
    if (cert == NULL) {
	PK11_FreeSlot(*slotPtr);
	return NULL;
    }
    return cert;
#else
    CERTCertificate *rvCert = NULL;
    NSSCertificate *cert;
    NSSDER issuer, serial;
    NSSCryptoContext *cc;
    SECItem *derSerial;

    if (slotPtr) *slotPtr = NULL;

    /* PKCS#11 needs to use DER-encoded serial numbers.  Create a
     * CERTIssuerAndSN that actually has the encoded value and pass that
     * to PKCS#11 (and the crypto context).
     */
    derSerial = SEC_ASN1EncodeItem(NULL, NULL,
                                   &issuerSN->serialNumber,
                                   SEC_IntegerTemplate);
    if (!derSerial) {
	return NULL;
    }

    NSSITEM_FROM_SECITEM(&issuer, &issuerSN->derIssuer);
    NSSITEM_FROM_SECITEM(&serial, derSerial);

    cc = STAN_GetDefaultCryptoContext();
    cert = NSSCryptoContext_FindCertificateByIssuerAndSerialNumber(cc, 
                                                                &issuer, 
                                                                &serial);
    if (cert) {
	SECITEM_FreeItem(derSerial, PR_TRUE);
	return STAN_GetCERTCertificate(cert);
    }
retry:
    cert = NSSTrustDomain_FindCertificateByIssuerAndSerialNumber(
                                                  STAN_GetDefaultTrustDomain(),
                                                  &issuer,
                                                  &serial);
    if (cert) {
	rvCert = STAN_GetCERTCertificate(cert);
	/* Check to see if the cert's token is still there */
	if (!PK11_IsPresent(rvCert->slot)) {
	    CERT_DestroyCertificate(rvCert);
	    goto retry;
	}
	if (slotPtr) *slotPtr = PK11_ReferenceSlot(rvCert->slot);
    }
    SECITEM_FreeItem(derSerial, PR_TRUE);
    return rvCert;
#endif
}

CK_OBJECT_HANDLE
PK11_FindObjectForCert(CERTCertificate *cert, void *wincx, PK11SlotInfo **pSlot)
{
    CK_OBJECT_HANDLE certHandle;
    CK_ATTRIBUTE searchTemplate	= { CKA_VALUE, NULL, 0 };
    
    PK11_SETATTRS(&searchTemplate, CKA_VALUE, cert->derCert.data,
		  cert->derCert.len);

    if (cert->slot) {
	certHandle = pk11_getcerthandle(cert->slot,cert,&searchTemplate,1);
	if (certHandle != CK_INVALID_HANDLE) {
	    *pSlot = PK11_ReferenceSlot(cert->slot);
	    return certHandle;
	}
    }

    certHandle = pk11_FindCertObjectByTemplate(pSlot,&searchTemplate,1,wincx);
    if (certHandle != CK_INVALID_HANDLE) {
	if (cert->slot == NULL) {
	    cert->slot = PK11_ReferenceSlot(*pSlot);
	    cert->pkcs11ID = certHandle;
	    cert->ownSlot = PR_TRUE;
	    cert->series = cert->slot->series;
	}
    }

    return(certHandle);
}

SECKEYPrivateKey *
PK11_FindKeyByAnyCert(CERTCertificate *cert, void *wincx)
{
    CK_OBJECT_HANDLE certHandle;
    CK_OBJECT_HANDLE keyHandle;
    PK11SlotInfo *slot = NULL;
    SECKEYPrivateKey *privKey = NULL;
    SECStatus rv;

    certHandle = PK11_FindObjectForCert(cert, wincx, &slot);
    if (certHandle == CK_INVALID_HANDLE) {
	 return NULL;
    }
    keyHandle = PK11_MatchItem(slot,certHandle,CKO_PRIVATE_KEY);
    if ((keyHandle == CK_INVALID_HANDLE) && 
			(PORT_GetError() == SSL_ERROR_NO_CERTIFICATE) && 
			pk11_LoginStillRequired(slot,wincx)) {
	/* authenticate and try again */
	rv = PK11_Authenticate(slot, PR_TRUE, wincx);
	if (rv != SECSuccess) {
	    goto loser;
	}
	keyHandle = PK11_MatchItem(slot,certHandle,CKO_PRIVATE_KEY);
	if (keyHandle == CK_INVALID_HANDLE) { 
	    goto loser;
	}
    }
    privKey =  PK11_MakePrivKey(slot, nullKey, PR_TRUE, keyHandle, wincx);
loser:
    if (slot) {
	PK11_FreeSlot(slot);
    }
    return privKey;
}

CK_OBJECT_HANDLE
pk11_FindPubKeyByAnyCert(CERTCertificate *cert, PK11SlotInfo **slot, void *wincx)
{
    CK_OBJECT_HANDLE certHandle;
    CK_OBJECT_HANDLE keyHandle;

    certHandle = PK11_FindObjectForCert(cert, wincx, slot);
    if (certHandle == CK_INVALID_HANDLE) {
	 return CK_INVALID_HANDLE;
    }
    keyHandle = PK11_MatchItem(*slot,certHandle,CKO_PUBLIC_KEY);
    if (keyHandle == CK_INVALID_HANDLE) { 
	PK11_FreeSlot(*slot);
	return CK_INVALID_HANDLE;
    }
    return keyHandle;
}

/*
 * find the number of certs in the slot with the same subject name
 */
int
PK11_NumberCertsForCertSubject(CERTCertificate *cert)
{
    CK_OBJECT_CLASS certClass = CKO_CERTIFICATE;
    CK_ATTRIBUTE theTemplate[] = {
	{ CKA_CLASS, NULL, 0 },
	{ CKA_SUBJECT, NULL, 0 },
    };
    CK_ATTRIBUTE *attr = theTemplate;
   int templateSize = sizeof(theTemplate)/sizeof(theTemplate[0]);

    PK11_SETATTRS(attr,CKA_CLASS, &certClass, sizeof(certClass)); attr++;
    PK11_SETATTRS(attr,CKA_SUBJECT,cert->derSubject.data,cert->derSubject.len);

    if (cert->slot == NULL) {
	PK11SlotList *list = PK11_GetAllTokens(CKM_INVALID_MECHANISM,
							PR_FALSE,PR_TRUE,NULL);
	PK11SlotListElement *le;
	int count = 0;

	/* loop through all the fortezza tokens */
	for (le = list->head; le; le = le->next) {
	    count += PK11_NumberObjectsFor(le->slot,theTemplate,templateSize);
	}
	PK11_FreeSlotList(list);
	return count;
    }

    return PK11_NumberObjectsFor(cert->slot,theTemplate,templateSize);
}

/*
 *  Walk all the certs with the same subject
 */
SECStatus
PK11_TraverseCertsForSubject(CERTCertificate *cert,
        SECStatus(* callback)(CERTCertificate*, void *), void *arg)
{
    if(!cert) {
	return SECFailure;
    }
    if (cert->slot == NULL) {
	PK11SlotList *list = PK11_GetAllTokens(CKM_INVALID_MECHANISM,
							PR_FALSE,PR_TRUE,NULL);
	PK11SlotListElement *le;

	/* loop through all the fortezza tokens */
	for (le = list->head; le; le = le->next) {
	    PK11_TraverseCertsForSubjectInSlot(cert,le->slot,callback,arg);
	}
	PK11_FreeSlotList(list);
	return SECSuccess;

    }

    return PK11_TraverseCertsForSubjectInSlot(cert, cert->slot, callback, arg);
}

SECStatus
PK11_TraverseCertsForSubjectInSlot(CERTCertificate *cert, PK11SlotInfo *slot,
	SECStatus(* callback)(CERTCertificate*, void *), void *arg)
{
#ifdef NSS_CLASSIC
    pk11DoCertCallback caller;
    pk11TraverseSlot callarg;
    CK_OBJECT_CLASS certClass = CKO_CERTIFICATE;
    CK_ATTRIBUTE theTemplate[] = {
	{ CKA_CLASS, NULL, 0 },
	{ CKA_SUBJECT, NULL, 0 },
    };
    CK_ATTRIBUTE *attr = theTemplate;
   int templateSize = sizeof(theTemplate)/sizeof(theTemplate[0]);

    PK11_SETATTRS(attr,CKA_CLASS, &certClass, sizeof(certClass)); attr++;
    PK11_SETATTRS(attr,CKA_SUBJECT,cert->derSubject.data,cert->derSubject.len);

    if (slot == NULL)  {
	return SECSuccess;
    }
    caller.noslotcallback = callback;
    caller.callback = NULL;
    caller.itemcallback = NULL;
    caller.callbackArg = arg;
    callarg.callback = pk11_DoCerts;
    callarg.callbackArg = (void *) & caller;
    callarg.findTemplate = theTemplate;
    callarg.templateCount = templateSize;
    
    return PK11_TraverseSlot(slot, &callarg);
#else
    PRStatus nssrv = PR_SUCCESS;
    NSSToken *token;
    NSSDER subject;
    NSSTrustDomain *td;
    nssList *subjectList;
    nssPKIObjectCollection *collection;
    nssCryptokiObject **instances;
    NSSCertificate **certs;
    nssTokenSearchType tokenOnly = nssTokenSearchType_TokenOnly;
    td = STAN_GetDefaultTrustDomain();
    NSSITEM_FROM_SECITEM(&subject, &cert->derSubject);
    token = PK11Slot_GetNSSToken(slot);
    if (!nssToken_IsPresent(token)) {
	return SECSuccess;
    }
    collection = nssCertificateCollection_Create(td, NULL);
    if (!collection) {
	return SECFailure;
    }
    subjectList = nssList_Create(NULL, PR_FALSE);
    if (!subjectList) {
	nssPKIObjectCollection_Destroy(collection);
	return SECFailure;
    }
    (void)nssTrustDomain_GetCertsForSubjectFromCache(td, &subject, 
                                                     subjectList);
    transfer_token_certs_to_collection(subjectList, token, collection);
    instances = nssToken_FindCertificatesBySubject(token, NULL,
	                                           &subject, 
	                                           tokenOnly, 0, &nssrv);
    nssPKIObjectCollection_AddInstances(collection, instances, 0);
    nss_ZFreeIf(instances);
    nssList_Destroy(subjectList);
    certs = nssPKIObjectCollection_GetCertificates(collection,
                                                   NULL, 0, NULL);
    nssPKIObjectCollection_Destroy(collection);
    if (certs) {
	CERTCertificate *oldie;
	NSSCertificate **cp;
	for (cp = certs; *cp; cp++) {
	    oldie = STAN_GetCERTCertificate(*cp);
	    if ((*callback)(oldie, arg) != SECSuccess) {
		nssrv = PR_FAILURE;
		break;
	    }
	}
	nssCertificateArray_Destroy(certs);
    }
    return (nssrv == PR_SUCCESS) ? SECSuccess : SECFailure;
#endif
}

SECStatus
PK11_TraverseCertsForNicknameInSlot(SECItem *nickname, PK11SlotInfo *slot,
	SECStatus(* callback)(CERTCertificate*, void *), void *arg)
{
#ifdef NSS_CLASSIC
    pk11DoCertCallback caller;
    pk11TraverseSlot callarg;
    CK_OBJECT_CLASS certClass = CKO_CERTIFICATE;
    CK_ATTRIBUTE theTemplate[] = {
	{ CKA_CLASS, NULL, 0 },
	{ CKA_LABEL, NULL, 0 },
    };
    CK_ATTRIBUTE *attr = theTemplate;
    int templateSize = sizeof(theTemplate)/sizeof(theTemplate[0]);

    if(!nickname) {
	return SECSuccess;
    }

    PK11_SETATTRS(attr,CKA_CLASS, &certClass, sizeof(certClass)); attr++;
    PK11_SETATTRS(attr,CKA_LABEL,nickname->data,nickname->len);

    if (slot == NULL) {
	return SECSuccess;
    }

    caller.noslotcallback = callback;
    caller.callback = NULL;
    caller.itemcallback = NULL;
    caller.callbackArg = arg;
    callarg.callback = pk11_DoCerts;
    callarg.callbackArg = (void *) & caller;
    callarg.findTemplate = theTemplate;
    callarg.templateCount = templateSize;

    return PK11_TraverseSlot(slot, &callarg);
#else
    struct nss3_cert_cbstr pk11cb;
    PRStatus nssrv = PR_SUCCESS;
    NSSToken *token;
    NSSTrustDomain *td;
    NSSUTF8 *nick;
    PRBool created = PR_FALSE;
    nssCryptokiObject **instances;
    nssPKIObjectCollection *collection = NULL;
    NSSCertificate **certs;
    nssList *nameList = NULL;
    nssTokenSearchType tokenOnly = nssTokenSearchType_TokenOnly;
    pk11cb.callback = callback;
    pk11cb.arg = arg;
    token = PK11Slot_GetNSSToken(slot);
    if (!nssToken_IsPresent(token)) {
	return SECSuccess;
    }
    if (nickname->data[nickname->len-1] != '\0') {
	nick = nssUTF8_Create(NULL, nssStringType_UTF8String, 
	                      nickname->data, nickname->len);
	created = PR_TRUE;
    } else {
	nick = (NSSUTF8 *)nickname->data;
    }
    td = STAN_GetDefaultTrustDomain();
    collection = nssCertificateCollection_Create(td, NULL);
    if (!collection) {
	goto loser;
    }
    nameList = nssList_Create(NULL, PR_FALSE);
    if (!nameList) {
	goto loser;
    }
    (void)nssTrustDomain_GetCertsForNicknameFromCache(td, nick, nameList);
    transfer_token_certs_to_collection(nameList, token, collection);
    instances = nssToken_FindCertificatesByNickname(token, NULL,
	                                            nick,
	                                            tokenOnly, 0, &nssrv);
    nssPKIObjectCollection_AddInstances(collection, instances, 0);
    nss_ZFreeIf(instances);
    nssList_Destroy(nameList);
    certs = nssPKIObjectCollection_GetCertificates(collection,
                                                   NULL, 0, NULL);
    nssPKIObjectCollection_Destroy(collection);
    if (certs) {
	CERTCertificate *oldie;
	NSSCertificate **cp;
	for (cp = certs; *cp; cp++) {
	    oldie = STAN_GetCERTCertificate(*cp);
	    if ((*callback)(oldie, arg) != SECSuccess) {
		nssrv = PR_FAILURE;
		break;
	    }
	}
	nssCertificateArray_Destroy(certs);
    }
    if (created) nss_ZFreeIf(nick);
    return (nssrv == PR_SUCCESS) ? SECSuccess : SECFailure;
loser:
    if (created) {
	nss_ZFreeIf(nick);
    }
    if (collection) {
	nssPKIObjectCollection_Destroy(collection);
    }
    if (nameList) {
	nssList_Destroy(nameList);
    }
    return SECFailure;
#endif
}

SECStatus
PK11_TraverseCertsInSlot(PK11SlotInfo *slot,
	SECStatus(* callback)(CERTCertificate*, void *), void *arg)
{
#ifdef NSS_CLASSIC
    pk11DoCertCallback caller;
    pk11TraverseSlot callarg;
    CK_OBJECT_CLASS certClass = CKO_CERTIFICATE;
    CK_ATTRIBUTE theTemplate[] = {
	{ CKA_CLASS, NULL, 0 },
    };
    CK_ATTRIBUTE *attr = theTemplate;
    int templateSize = sizeof(theTemplate)/sizeof(theTemplate[0]);

    PK11_SETATTRS(attr,CKA_CLASS, &certClass, sizeof(certClass)); attr++;

    if (slot == NULL) {
	return SECSuccess;
    }

    caller.noslotcallback = callback;
    caller.callback = NULL;
    caller.itemcallback = NULL;
    caller.callbackArg = arg;
    callarg.callback = pk11_DoCerts;
    callarg.callbackArg = (void *) & caller;
    callarg.findTemplate = theTemplate;
    callarg.templateCount = templateSize;
    return PK11_TraverseSlot(slot, &callarg);
#else
    PRStatus nssrv;
    NSSTrustDomain *td = STAN_GetDefaultTrustDomain();
    NSSToken *tok;
    nssList *certList = NULL;
    nssCryptokiObject **instances;
    nssPKIObjectCollection *collection;
    NSSCertificate **certs;
    nssTokenSearchType tokenOnly = nssTokenSearchType_TokenOnly;
    tok = PK11Slot_GetNSSToken(slot);
    if (!nssToken_IsPresent(tok)) {
	return SECSuccess;
    }
    collection = nssCertificateCollection_Create(td, NULL);
    if (!collection) {
	return SECFailure;
    }
    certList = nssList_Create(NULL, PR_FALSE);
    if (!certList) {
	nssPKIObjectCollection_Destroy(collection);
	return SECFailure;
    }
    (void *)nssTrustDomain_GetCertsFromCache(td, certList);
    transfer_token_certs_to_collection(certList, tok, collection);
    instances = nssToken_FindCertificates(tok, NULL,
                                          tokenOnly, 0, &nssrv);
    nssPKIObjectCollection_AddInstances(collection, instances, 0);
    nss_ZFreeIf(instances);
    nssList_Destroy(certList);
    certs = nssPKIObjectCollection_GetCertificates(collection,
                                                   NULL, 0, NULL);
    nssPKIObjectCollection_Destroy(collection);
    if (certs) {
	CERTCertificate *oldie;
	NSSCertificate **cp;
	for (cp = certs; *cp; cp++) {
	    oldie = STAN_GetCERTCertificate(*cp);
	    if ((*callback)(oldie, arg) != SECSuccess) {
		nssrv = PR_FAILURE;
		break;
	    }
	}
	nssCertificateArray_Destroy(certs);
    }
    return (nssrv == PR_SUCCESS) ? SECSuccess : SECFailure;
#endif
}

/*
 * return the certificate associated with a derCert 
 */
CERTCertificate *
PK11_FindCertFromDERCert(PK11SlotInfo *slot, CERTCertificate *cert,
								 void *wincx)
{
    return PK11_FindCertFromDERCertItem(slot, &cert->derCert, wincx);
}

CERTCertificate *
PK11_FindCertFromDERCertItem(PK11SlotInfo *slot, SECItem *inDerCert,
								 void *wincx)

{
    CERTCertificate *rvCert = NULL;
    NSSCertificate *c;
    NSSDER derCert;
    NSSToken *tok;
    NSSTrustDomain *td = STAN_GetDefaultTrustDomain();
    SECStatus rv;

    tok = PK11Slot_GetNSSToken(slot);
    NSSITEM_FROM_SECITEM(&derCert, inDerCert);
    rv = pk11_AuthenticateUnfriendly(slot, PR_TRUE, wincx);
    if (rv != SECSuccess) {
	PK11_FreeSlot(slot);
	return NULL;
    }
    c = NSSTrustDomain_FindCertificateByEncodedCertificate(td, &derCert);
    if (c) {
	PRBool isToken = PR_FALSE;
	NSSToken **tp;
	NSSToken **tokens = nssPKIObject_GetTokens(&c->object, NULL);
	if (tokens) {
	    for (tp = tokens; *tp; tp++) {
		if (*tp == tok) {
		    isToken = PR_TRUE;
		    break;
		}
	    }
	    if (!isToken) {
		NSSCertificate_Destroy(c);
		c = NULL;
	    }
	    nssTokenArray_Destroy(tokens);
	}
    }
    if (c) {
	rvCert = STAN_GetCERTCertificate(c);
    }
    return rvCert;
} 

/* mcgreer 3.4 -- nobody uses this, ignoring */
/*
 * return the certificate associated with a derCert 
 */
CERTCertificate *
PK11_FindCertFromDERSubjectAndNickname(PK11SlotInfo *slot, 
					CERTCertificate *cert, 
					char *nickname, void *wincx)
{
    CK_OBJECT_CLASS certClass = CKO_CERTIFICATE;
    CK_ATTRIBUTE theTemplate[] = {
	{ CKA_SUBJECT, NULL, 0 },
	{ CKA_LABEL, NULL, 0 },
	{ CKA_CLASS, NULL, 0 }
    };
    /* if you change the array, change the variable below as well */
    int tsize = sizeof(theTemplate)/sizeof(theTemplate[0]);
    CK_OBJECT_HANDLE certh;
    CK_ATTRIBUTE *attrs = theTemplate;
    SECStatus rv;

    PK11_SETATTRS(attrs, CKA_SUBJECT, cert->derSubject.data, 
						cert->derSubject.len); attrs++;
    PK11_SETATTRS(attrs, CKA_LABEL, nickname, PORT_Strlen(nickname));
    PK11_SETATTRS(attrs, CKA_CLASS, &certClass, sizeof(certClass));

    /*
     * issue the find
     */
    rv = pk11_AuthenticateUnfriendly(slot, PR_TRUE, wincx);
    if (rv != SECSuccess) return NULL;

    certh = pk11_getcerthandle(slot,cert,theTemplate,tsize);
    if (certh == CK_INVALID_HANDLE) {
	return NULL;
    }

    return PK11_MakeCertFromHandle(slot, certh, NULL);
}

/*
 * import a cert for a private key we have already generated. Set the label
 * on both to be the nickname.
 */
static CK_OBJECT_HANDLE 
pk11_findKeyObjectByDERCert(PK11SlotInfo *slot, CERTCertificate *cert, 
								void *wincx)
{
    SECItem *keyID;
    CK_OBJECT_HANDLE key;
    SECStatus rv;

    if((slot == NULL) || (cert == NULL)) {
	return CK_INVALID_HANDLE;
    }

    keyID = pk11_mkcertKeyID(cert);
    if(keyID == NULL) {
	return CK_INVALID_HANDLE;
    }

    key = pk11_FindPrivateKeyFromCertID(slot, keyID);
    if ((key == CK_INVALID_HANDLE) && 
			(PORT_GetError() == SSL_ERROR_NO_CERTIFICATE) && 
			pk11_LoginStillRequired(slot,wincx)) {
	/* authenticate and try again */
	rv = PK11_Authenticate(slot, PR_TRUE, wincx);
	if (rv != SECSuccess) goto loser;
	key = pk11_FindPrivateKeyFromCertID(slot, keyID);
   }

loser:
    SECITEM_ZfreeItem(keyID, PR_TRUE);
    return key;
}

SECKEYPrivateKey *
PK11_FindKeyByDERCert(PK11SlotInfo *slot, CERTCertificate *cert, 
								void *wincx)
{
    CK_OBJECT_HANDLE keyHandle;

    if((slot == NULL) || (cert == NULL)) {
	return NULL;
    }

    keyHandle = pk11_findKeyObjectByDERCert(slot, cert, wincx);
    if (keyHandle == CK_INVALID_HANDLE) {
	return NULL;
    }

    return PK11_MakePrivKey(slot,nullKey,PR_TRUE,keyHandle,wincx);
}

SECStatus
PK11_ImportCertForKeyToSlot(PK11SlotInfo *slot, CERTCertificate *cert, 
						char *nickname, 
						PRBool addCertUsage,void *wincx)
{
    CK_OBJECT_HANDLE keyHandle;

    if((slot == NULL) || (cert == NULL) || (nickname == NULL)) {
	return SECFailure;
    }

    keyHandle = pk11_findKeyObjectByDERCert(slot, cert, wincx);
    if (keyHandle == CK_INVALID_HANDLE) {
	return SECFailure;
    }

    return PK11_ImportCert(slot, cert, keyHandle, nickname, addCertUsage);
}   


/* remove when the real version comes out */
#define SEC_OID_MISSI_KEA 300  /* until we have v3 stuff merged */
PRBool
KEAPQGCompare(CERTCertificate *server,CERTCertificate *cert) {

    if ( SECKEY_KEAParamCompare(server,cert) == SECEqual ) {
        return PR_TRUE;
    } else {
	return PR_FALSE;
    }
}

PRBool
PK11_FortezzaHasKEA(CERTCertificate *cert) {
   /* look at the subject and see if it is a KEA for MISSI key */
   SECOidData *oid;

   if ((cert->trust == NULL) ||
       ((cert->trust->sslFlags & CERTDB_USER) != CERTDB_USER)) {
       return PR_FALSE;
   }

   oid = SECOID_FindOID(&cert->subjectPublicKeyInfo.algorithm.algorithm);


   return (PRBool)((oid->offset == SEC_OID_MISSI_KEA_DSS_OLD) || 
		(oid->offset == SEC_OID_MISSI_KEA_DSS) ||
				(oid->offset == SEC_OID_MISSI_KEA)) ;
}

/*
 * Find a kea cert on this slot that matches the domain of it's peer
 */
static CERTCertificate
*pk11_GetKEAMate(PK11SlotInfo *slot,CERTCertificate *peer)
{
    int i;
    CERTCertificate *returnedCert = NULL;

    for (i=0; i < slot->cert_count; i++) {
	CERTCertificate *cert = slot->cert_array[i];

	if (PK11_FortezzaHasKEA(cert) && KEAPQGCompare(peer,cert)) {
		returnedCert = CERT_DupCertificate(cert);
		break;
	}
    }
    return returnedCert;
}

/*
 * The following is a FORTEZZA only Certificate request. We call this when we
 * are doing a non-client auth SSL connection. We are only interested in the
 * fortezza slots, and we are only interested in certs that share the same root
 * key as the server.
 */
CERTCertificate *
PK11_FindBestKEAMatch(CERTCertificate *server, void *wincx)
{
    PK11SlotList *keaList = PK11_GetAllTokens(CKM_KEA_KEY_DERIVE,
							PR_FALSE,PR_TRUE,wincx);
    PK11SlotListElement *le;
    CERTCertificate *returnedCert = NULL;
    SECStatus rv;

    /* loop through all the fortezza tokens */
    for (le = keaList->head; le; le = le->next) {
        rv = PK11_Authenticate(le->slot, PR_TRUE, wincx);
        if (rv != SECSuccess) continue;
	if (le->slot->session == CK_INVALID_SESSION) {
	    continue;
	}
	returnedCert = pk11_GetKEAMate(le->slot,server);
	if (returnedCert) break;
    }
    PK11_FreeSlotList(keaList);

    return returnedCert;
}

/*
 * find a matched pair of kea certs to key exchange parameters from one 
 * fortezza card to another as necessary.
 */
SECStatus
PK11_GetKEAMatchedCerts(PK11SlotInfo *slot1, PK11SlotInfo *slot2,
	CERTCertificate **cert1, CERTCertificate **cert2)
{
    CERTCertificate *returnedCert = NULL;
    int i;

    for (i=0; i < slot1->cert_count; i++) {
	CERTCertificate *cert = slot1->cert_array[i];

	if (PK11_FortezzaHasKEA(cert)) {
	    returnedCert = pk11_GetKEAMate(slot2,cert);
	    if (returnedCert != NULL) {
		*cert2 = returnedCert;
		*cert1 = CERT_DupCertificate(cert);
		return SECSuccess;
	    }
	}
    }
    return SECFailure;
}

/*
 * return the private key From a given Cert
 */
CK_OBJECT_HANDLE
PK11_FindCertInSlot(PK11SlotInfo *slot, CERTCertificate *cert, void *wincx)
{
    CK_OBJECT_CLASS certClass = CKO_CERTIFICATE;
    CK_ATTRIBUTE theTemplate[] = {
	{ CKA_VALUE, NULL, 0 },
	{ CKA_CLASS, NULL, 0 }
    };
    /* if you change the array, change the variable below as well */
    int tsize = sizeof(theTemplate)/sizeof(theTemplate[0]);
    CK_ATTRIBUTE *attrs = theTemplate;
    SECStatus rv;

    PK11_SETATTRS(attrs, CKA_VALUE, cert->derCert.data,
						cert->derCert.len); attrs++;
    PK11_SETATTRS(attrs, CKA_CLASS, &certClass, sizeof(certClass));

    /*
     * issue the find
     */
    rv = pk11_AuthenticateUnfriendly(slot, PR_TRUE, wincx);
    if (rv != SECSuccess) {
	return CK_INVALID_HANDLE;
    }

    return pk11_getcerthandle(slot,cert,theTemplate,tsize);
}

SECItem *
PK11_GetKeyIDFromCert(CERTCertificate *cert, void *wincx)
{
    CK_OBJECT_HANDLE handle;
    PK11SlotInfo *slot = NULL;
    CK_ATTRIBUTE theTemplate[] = {
	{ CKA_ID, NULL, 0 },
    };
    int tsize = sizeof(theTemplate)/sizeof(theTemplate[0]);
    SECItem *item = NULL;
    CK_RV crv;

    handle = PK11_FindObjectForCert(cert,wincx,&slot);
    if (handle == CK_INVALID_HANDLE) {
	goto loser;
    }

    crv = PK11_GetAttributes(NULL,slot,handle,theTemplate,tsize);
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
    PK11_FreeSlot(slot);
    return item;
}

struct listCertsStr {
    PK11CertListType type;
    CERTCertList *certList;
};

static PRStatus
pk11ListCertCallback(NSSCertificate *c, void *arg)
{
    struct listCertsStr *listCertP = (struct listCertsStr *)arg;
    CERTCertificate *newCert = NULL;
    PK11CertListType type = listCertP->type;
    CERTCertList *certList = listCertP->certList;
    PRBool isUnique = PR_FALSE;
    PRBool isCA = PR_FALSE;
    char *nickname = NULL;
    unsigned int certType;

    if ((type == PK11CertListUnique) || (type == PK11CertListRootUnique) ||
        (type == PK11CertListCAUnique) || (type == PK11CertListUserUnique) ) {
        /* only list one instance of each certificate, even if several exist */
	isUnique = PR_TRUE;
    }
    if ((type == PK11CertListCA) || (type == PK11CertListRootUnique) ||
        (type == PK11CertListCAUnique)) {
	isCA = PR_TRUE;
    }

    /* if we want user certs and we don't have one skip this cert */
    if ( ( (type == PK11CertListUser) || (type == PK11CertListUserUnique) ) && 
		!NSSCertificate_IsPrivateKeyAvailable(c, NULL,NULL)) {
	return PR_SUCCESS;
    }

    /* PK11CertListRootUnique means we want CA certs without a private key.
     * This is for legacy app support . PK11CertListCAUnique should be used
     * instead to get all CA certs, regardless of private key
     */
    if ((type == PK11CertListRootUnique) && 
		NSSCertificate_IsPrivateKeyAvailable(c, NULL,NULL)) {
	return PR_SUCCESS;
    }

    newCert = STAN_GetCERTCertificate(c);
    if (!newCert) {
	return PR_SUCCESS;
    }
    /* if we want CA certs and it ain't one, skip it */
    if( isCA  && (!CERT_IsCACert(newCert, &certType)) ) {
	return PR_SUCCESS;
    }
    if (isUnique) {
	CERT_DupCertificate(newCert);

	nickname = STAN_GetCERTCertificateName(certList->arena, c);

	/* put slot certs at the end */
	if (newCert->slot && !PK11_IsInternal(newCert->slot)) {
	    CERT_AddCertToListTailWithData(certList,newCert,nickname);
	} else {
	    CERT_AddCertToListHeadWithData(certList,newCert,nickname);
	}
    } else {
	/* add multiple instances to the cert list */
	nssCryptokiObject **ip;
	nssCryptokiObject **instances = nssPKIObject_GetInstances(&c->object);
	if (!instances) {
	    return PR_SUCCESS;
	}
	for (ip = instances; *ip; ip++) {
	    nssCryptokiObject *instance = *ip;
	    PK11SlotInfo *slot = instance->token->pk11slot;

	    /* put the same CERTCertificate in the list for all instances */
	    CERT_DupCertificate(newCert);

	    nickname = STAN_GetCERTCertificateNameForInstance(
			certList->arena, c, instance);

	    /* put slot certs at the end */
	    if (slot && !PK11_IsInternal(slot)) {
		CERT_AddCertToListTailWithData(certList,newCert,nickname);
	    } else {
		CERT_AddCertToListHeadWithData(certList,newCert,nickname);
	    }
	}
	nssCryptokiObjectArray_Destroy(instances);
    }
    return PR_SUCCESS;
}


CERTCertList *
PK11_ListCerts(PK11CertListType type, void *pwarg)
{
#ifdef NSS_CLASSIC
    CERTCertList *certList = NULL;
    struct listCertsStr listCerts;
    
    certList= CERT_NewCertList();
    listCerts.type = type;
    listCerts.certList = certList;

    PK11_TraverseSlotCerts(pk11ListCertCallback,&listCerts,pwarg);

    if (CERT_LIST_HEAD(certList) == NULL) {
	CERT_DestroyCertList(certList);
	certList = NULL;
    }
    return certList;
#else
    NSSTrustDomain *defaultTD = STAN_GetDefaultTrustDomain();
    CERTCertList *certList = NULL;
    struct listCertsStr listCerts;
    certList = CERT_NewCertList();
    listCerts.type = type;
    listCerts.certList = certList;

    /* authenticate to the slots */
    (void) pk11_TraverseAllSlots( NULL, NULL, pwarg);
    NSSTrustDomain_TraverseCertificates(defaultTD, pk11ListCertCallback,
								 &listCerts);
    return certList;
#endif
}
    
SECItem *
PK11_GetLowLevelKeyIDForCert(PK11SlotInfo *slot,
					CERTCertificate *cert, void *wincx)
{
    CK_ATTRIBUTE theTemplate[] = {
	{ CKA_VALUE, NULL, 0 },
	{ CKA_CLASS, NULL, 0 }
    };
    /* if you change the array, change the variable below as well */
    int tsize = sizeof(theTemplate)/sizeof(theTemplate[0]);
    CK_OBJECT_HANDLE certHandle;
    CK_ATTRIBUTE *attrs = theTemplate;
    PK11SlotInfo *slotRef = NULL;
    SECItem *item;
    SECStatus rv;

    if (slot) {
	PK11_SETATTRS(attrs, CKA_VALUE, cert->derCert.data, 
						cert->derCert.len); attrs++;
 
	rv = pk11_AuthenticateUnfriendly(slot, PR_TRUE, wincx);
	if (rv != SECSuccess) {
	    return NULL;
	}
        certHandle = pk11_getcerthandle(slot,cert,theTemplate,tsize);
    } else {
    	certHandle = PK11_FindObjectForCert(cert, wincx, &slotRef);
	if (certHandle == CK_INVALID_HANDLE) {
	   return pk11_mkcertKeyID(cert);
	}
	slot = slotRef;
    }

    if (certHandle == CK_INVALID_HANDLE) {
	 return NULL;
    }

    item = pk11_GetLowLevelKeyFromHandle(slot,certHandle);
    if (slotRef) PK11_FreeSlot(slotRef);
    return item;
}

/* argument type for listCertsCallback */
typedef struct {
    CERTCertList *list;
    PK11SlotInfo *slot;
} ListCertsArg;

static SECStatus
listCertsCallback(CERTCertificate* cert, void*arg)
{
    ListCertsArg *cdata = (ListCertsArg*)arg;
    char *nickname = NULL;
    nssCryptokiObject *instance, **ci;
    nssCryptokiObject **instances;
    NSSCertificate *c = STAN_GetNSSCertificate(cert);

    instances = nssPKIObject_GetInstances(&c->object);
    instance = NULL;
    for (ci = instances; *ci; ci++) {
	if ((*ci)->token->pk11slot == cdata->slot) {
	    instance = *ci;
	    break;
	}
    }
    PORT_Assert(instance != NULL);
    if (!instance) {
	nssCryptokiObjectArray_Destroy(instances);
	PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
	return SECFailure;
    }
    nickname = STAN_GetCERTCertificateNameForInstance(cdata->list->arena,
	c, instance);
    nssCryptokiObjectArray_Destroy(instances);

    return CERT_AddCertToListTailWithData(cdata->list, 
				CERT_DupCertificate(cert),nickname);
}

CERTCertList *
PK11_ListCertsInSlot(PK11SlotInfo *slot)
{
    SECStatus status;
    CERTCertList *certs;
    ListCertsArg cdata;

    certs = CERT_NewCertList();
    if(certs == NULL) return NULL;
    cdata.list = certs;
    cdata.slot = slot;

    status = PK11_TraverseCertsInSlot(slot, listCertsCallback,
		&cdata);

    if( status != SECSuccess ) {
	CERT_DestroyCertList(certs);
	certs = NULL;
    }

    return certs;
}

