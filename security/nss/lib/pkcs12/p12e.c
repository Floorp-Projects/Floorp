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

#include "nssrenam.h"
#include "p12t.h"
#include "p12.h"
#include "plarena.h"
#include "secitem.h"
#include "secoid.h"
#include "seccomon.h"
#include "secport.h"
#include "cert.h"
#include "secpkcs7.h"
#include "secasn1.h"
#include "secerr.h"
#include "pk11func.h"
#include "p12plcy.h"
#include "p12local.h"
#include "alghmac.h"
#include "prcpucfg.h"

/*********************************
 * Structures used in exporting the PKCS 12 blob
 *********************************/

/* A SafeInfo is used for each ContentInfo which makes up the
 * sequence of safes in the AuthenticatedSafe portion of the
 * PFX structure.
 */
struct SEC_PKCS12SafeInfoStr {
    PRArenaPool *arena;

    /* information for setting up password encryption */
    SECItem pwitem;
    SECOidTag algorithm;
    PK11SymKey *encryptionKey;

    /* how many items have been stored in this safe,
     * we will skip any safe which does not contain any
     * items
      */
    unsigned int itemCount;

    /* the content info for the safe */
    SEC_PKCS7ContentInfo *cinfo;

    sec_PKCS12SafeContents *safe;
};

/* An opaque structure which contains information needed for exporting
 * certificates and keys through PKCS 12.
 */
struct SEC_PKCS12ExportContextStr {
    PRArenaPool *arena;
    PK11SlotInfo *slot;
    void *wincx;

    /* integrity information */
    PRBool integrityEnabled;
    PRBool	pwdIntegrity;
    union {
	struct sec_PKCS12PasswordModeInfo pwdInfo;
	struct sec_PKCS12PublicKeyModeInfo pubkeyInfo;
    } integrityInfo; 

    /* helper functions */
    /* retrieve the password call back */
    SECKEYGetPasswordKey pwfn;
    void *pwfnarg;

    /* safe contents bags */
    SEC_PKCS12SafeInfo **safeInfos;
    unsigned int safeInfoCount;

    /* the sequence of safes */
    sec_PKCS12AuthenticatedSafe authSafe;

    /* information needing deletion */
    CERTCertificate **certList;
};

/* structures for passing information to encoder callbacks when processing
 * data through the ASN1 engine.
 */
struct sec_pkcs12_encoder_output {
    SEC_PKCS12EncoderOutputCallback outputfn;
    void *outputarg;
};

struct sec_pkcs12_hmac_and_output_info {
    void *arg;
    struct sec_pkcs12_encoder_output output;
};

/* An encoder context which is used for the actual encoding
 * portion of PKCS 12. 
 */
typedef struct sec_PKCS12EncoderContextStr {
    PRArenaPool *arena;
    SEC_PKCS12ExportContext *p12exp;
    PK11SymKey *encryptionKey;

    /* encoder information - this is set up based on whether 
     * password based or public key pased privacy is being used
     */
    SEC_ASN1EncoderContext *ecx;
    union {
	struct sec_pkcs12_hmac_and_output_info hmacAndOutputInfo;
	struct sec_pkcs12_encoder_output encOutput;
    } output;

    /* structures for encoding of PFX and MAC */
    sec_PKCS12PFXItem pfx;
    sec_PKCS12MacData mac;

    /* authenticated safe encoding tracking information */
    SEC_PKCS7ContentInfo *aSafeCinfo;
    SEC_PKCS7EncoderContext *aSafeP7Ecx;
    SEC_ASN1EncoderContext *aSafeEcx;
    unsigned int currentSafe;

    /* hmac context */
    PK11Context *hmacCx;
} sec_PKCS12EncoderContext;


/*********************************
 * Export setup routines
 *********************************/

/* SEC_PKCS12CreateExportContext 
 *   Creates an export context and sets the unicode and password retrieval
 *   callbacks.  This is the first call which must be made when exporting
 *   a PKCS 12 blob.
 *
 * pwfn, pwfnarg - password retrieval callback and argument.  these are
 * 		   required for password-authentication mode.
 */
SEC_PKCS12ExportContext *
SEC_PKCS12CreateExportContext(SECKEYGetPasswordKey pwfn, void *pwfnarg,  
			      PK11SlotInfo *slot, void *wincx)
{
    PRArenaPool *arena = NULL;
    SEC_PKCS12ExportContext *p12ctxt = NULL;

    /* allocate the arena and create the context */
    arena = PORT_NewArena(4096);
    if(!arena) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	return NULL;
    }

    p12ctxt = (SEC_PKCS12ExportContext *)PORT_ArenaZAlloc(arena, 
					sizeof(SEC_PKCS12ExportContext));
    if(!p12ctxt) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	goto loser;
    }

    /* password callback for key retrieval */
    p12ctxt->pwfn = pwfn;
    p12ctxt->pwfnarg = pwfnarg;

    p12ctxt->integrityEnabled = PR_FALSE;
    p12ctxt->arena = arena;
    p12ctxt->wincx = wincx;
    p12ctxt->slot = (slot) ? slot : PK11_GetInternalSlot();

    return p12ctxt;

loser:
    if(arena) {
	PORT_FreeArena(arena, PR_TRUE);
    }

    return NULL;
}

/* 
 * Adding integrity mode
 */

/* SEC_PKCS12AddPasswordIntegrity 
 *	Add password integrity to the exported data.  If an integrity method
 *	has already been set, then return an error.
 *	
 *	p12ctxt - the export context
 * 	pwitem - the password for integrity mode
 *	integAlg - the integrity algorithm to use for authentication.
 */
SECStatus
SEC_PKCS12AddPasswordIntegrity(SEC_PKCS12ExportContext *p12ctxt,
			       SECItem *pwitem, SECOidTag integAlg) 
{			       
    if(!p12ctxt || p12ctxt->integrityEnabled) {
	return SECFailure;
    }
   
    /* set up integrity information */
    p12ctxt->pwdIntegrity = PR_TRUE;
    p12ctxt->integrityInfo.pwdInfo.password = 
        (SECItem*)PORT_ArenaZAlloc(p12ctxt->arena, sizeof(SECItem));
    if(!p12ctxt->integrityInfo.pwdInfo.password) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	return SECFailure;
    }
    if(SECITEM_CopyItem(p12ctxt->arena, 
			p12ctxt->integrityInfo.pwdInfo.password, pwitem)
		!= SECSuccess) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	return SECFailure;
    }
    p12ctxt->integrityInfo.pwdInfo.algorithm = integAlg;
    p12ctxt->integrityEnabled = PR_TRUE;

    return SECSuccess;
}

/* SEC_PKCS12AddPublicKeyIntegrity
 *	Add public key integrity to the exported data.  If an integrity method
 *	has already been set, then return an error.  The certificate must be
 *	allowed to be used as a signing cert.
 *	
 *	p12ctxt - the export context
 *	cert - signer certificate
 *	certDb - the certificate database
 *	algorithm - signing algorithm
 *	keySize - size of the signing key (?)
 */
SECStatus
SEC_PKCS12AddPublicKeyIntegrity(SEC_PKCS12ExportContext *p12ctxt,
				CERTCertificate *cert, CERTCertDBHandle *certDb,
				SECOidTag algorithm, int keySize)
{
    if(!p12ctxt) {
	return SECFailure;
    }
    
    p12ctxt->integrityInfo.pubkeyInfo.cert = cert;
    p12ctxt->integrityInfo.pubkeyInfo.certDb = certDb;
    p12ctxt->integrityInfo.pubkeyInfo.algorithm = algorithm;
    p12ctxt->integrityInfo.pubkeyInfo.keySize = keySize;
    p12ctxt->integrityEnabled = PR_TRUE;

    return SECSuccess;
}


/*
 * Adding safes - encrypted (password/public key) or unencrypted
 *	Each of the safe creation routines return an opaque pointer which
 *	are later passed into the routines for exporting certificates and
 *	keys.
 */

/* append the newly created safeInfo to list of safeInfos in the export
 * context.  
 */
static SECStatus
sec_pkcs12_append_safe_info(SEC_PKCS12ExportContext *p12ctxt, SEC_PKCS12SafeInfo *info)
{
    void *mark = NULL, *dummy1 = NULL, *dummy2 = NULL;

    if(!p12ctxt || !info) {
	return SECFailure;
    }

    mark = PORT_ArenaMark(p12ctxt->arena);

    /* if no safeInfos have been set, create the list, otherwise expand it. */
    if(!p12ctxt->safeInfoCount) {
	p12ctxt->safeInfos = (SEC_PKCS12SafeInfo **)PORT_ArenaZAlloc(p12ctxt->arena, 
					      2 * sizeof(SEC_PKCS12SafeInfo *));
	dummy1 = p12ctxt->safeInfos;
	p12ctxt->authSafe.encodedSafes = (SECItem **)PORT_ArenaZAlloc(p12ctxt->arena, 
					2 * sizeof(SECItem *));
	dummy2 = p12ctxt->authSafe.encodedSafes;
    } else {
	dummy1 = PORT_ArenaGrow(p12ctxt->arena, p12ctxt->safeInfos, 
			       (p12ctxt->safeInfoCount + 1) * sizeof(SEC_PKCS12SafeInfo *),
			       (p12ctxt->safeInfoCount + 2) * sizeof(SEC_PKCS12SafeInfo *));
	p12ctxt->safeInfos = (SEC_PKCS12SafeInfo **)dummy1;
	dummy2 = PORT_ArenaGrow(p12ctxt->arena, p12ctxt->authSafe.encodedSafes, 
			       (p12ctxt->authSafe.safeCount + 1) * sizeof(SECItem *),
			       (p12ctxt->authSafe.safeCount + 2) * sizeof(SECItem *));
	p12ctxt->authSafe.encodedSafes = (SECItem**)dummy2;
    }
    if(!dummy1 || !dummy2) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	goto loser;
    }

    /* append the new safeInfo and null terminate the list */
    p12ctxt->safeInfos[p12ctxt->safeInfoCount] = info;
    p12ctxt->safeInfos[++p12ctxt->safeInfoCount] = NULL;
    p12ctxt->authSafe.encodedSafes[p12ctxt->authSafe.safeCount] = 
        (SECItem*)PORT_ArenaZAlloc(p12ctxt->arena, sizeof(SECItem));
    if(!p12ctxt->authSafe.encodedSafes[p12ctxt->authSafe.safeCount]) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	goto loser;
    }
    p12ctxt->authSafe.encodedSafes[++p12ctxt->authSafe.safeCount] = NULL;

    PORT_ArenaUnmark(p12ctxt->arena, mark);
    return SECSuccess;

loser:
    PORT_ArenaRelease(p12ctxt->arena, mark);
    return SECFailure;
}

/* SEC_PKCS12CreatePasswordPrivSafe
 *	Create a password privacy safe to store exported information in.
 *
 * 	p12ctxt - export context
 *	pwitem - password for encryption
 *	privAlg - pbe algorithm through which encryption is done.
 */
SEC_PKCS12SafeInfo *
SEC_PKCS12CreatePasswordPrivSafe(SEC_PKCS12ExportContext *p12ctxt, 
				 SECItem *pwitem, SECOidTag privAlg)
{
    SEC_PKCS12SafeInfo *safeInfo = NULL;
    void *mark = NULL;
    PK11SlotInfo *slot;
    SECAlgorithmID *algId;
    SECItem uniPwitem = {siBuffer, NULL, 0};

    if(!p12ctxt) {
	return NULL;
    }

    /* allocate the safe info */
    mark = PORT_ArenaMark(p12ctxt->arena);
    safeInfo = (SEC_PKCS12SafeInfo *)PORT_ArenaZAlloc(p12ctxt->arena, 
    						sizeof(SEC_PKCS12SafeInfo));
    if(!safeInfo) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	PORT_ArenaRelease(p12ctxt->arena, mark);
	return NULL;
    }

    safeInfo->itemCount = 0;

    /* create the encrypted safe */
    safeInfo->cinfo = SEC_PKCS7CreateEncryptedData(privAlg, 0, p12ctxt->pwfn, 
    						   p12ctxt->pwfnarg);
    if(!safeInfo->cinfo) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	goto loser;
    }
    safeInfo->arena = p12ctxt->arena;

    /* convert the password to unicode */ 
    if(!sec_pkcs12_convert_item_to_unicode(NULL, &uniPwitem, pwitem,
					       PR_TRUE, PR_TRUE, PR_TRUE)) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	goto loser;
    }
    if(SECITEM_CopyItem(p12ctxt->arena, &safeInfo->pwitem, &uniPwitem) != SECSuccess) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	goto loser;
    }

    /* generate the encryption key */
    slot = p12ctxt->slot;
    if(!slot) {
	slot = PK11_GetInternalKeySlot();
	if(!slot) {
	    PORT_SetError(SEC_ERROR_NO_MEMORY);
	    goto loser;
	}
    }

    algId = SEC_PKCS7GetEncryptionAlgorithm(safeInfo->cinfo);
    safeInfo->encryptionKey = PK11_PBEKeyGen(slot, algId, &uniPwitem, 
					     PR_FALSE, p12ctxt->wincx);
    if(!safeInfo->encryptionKey) {
	goto loser;
    }

    safeInfo->arena = p12ctxt->arena;
    safeInfo->safe = NULL;
    if(sec_pkcs12_append_safe_info(p12ctxt, safeInfo) != SECSuccess) {
	goto loser;
    }

    if(uniPwitem.data) {
	SECITEM_ZfreeItem(&uniPwitem, PR_FALSE);
    }
    PORT_ArenaUnmark(p12ctxt->arena, mark);
    return safeInfo;

loser:
    if(safeInfo->cinfo) {
	SEC_PKCS7DestroyContentInfo(safeInfo->cinfo);
    }

    if(uniPwitem.data) {
	SECITEM_ZfreeItem(&uniPwitem, PR_FALSE);
    }

    PORT_ArenaRelease(p12ctxt->arena, mark);
    return NULL;
}

/* SEC_PKCS12CreateUnencryptedSafe 
 *	Creates an unencrypted safe within the export context.
 *
 *	p12ctxt - the export context 
 */
SEC_PKCS12SafeInfo *
SEC_PKCS12CreateUnencryptedSafe(SEC_PKCS12ExportContext *p12ctxt)
{
    SEC_PKCS12SafeInfo *safeInfo = NULL;
    void *mark = NULL;

    if(!p12ctxt) {
	return NULL;
    }

    /* create the safe info */
    mark = PORT_ArenaMark(p12ctxt->arena);
    safeInfo = (SEC_PKCS12SafeInfo *)PORT_ArenaZAlloc(p12ctxt->arena, 
    					      sizeof(SEC_PKCS12SafeInfo));
    if(!safeInfo) {
	PORT_ArenaRelease(p12ctxt->arena, mark);
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	return NULL;
    }

    safeInfo->itemCount = 0;

    /* create the safe content */
    safeInfo->cinfo = SEC_PKCS7CreateData();
    if(!safeInfo->cinfo) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	goto loser;
    }

    if(sec_pkcs12_append_safe_info(p12ctxt, safeInfo) != SECSuccess) {
	goto loser;
    }

    PORT_ArenaUnmark(p12ctxt->arena, mark);
    return safeInfo;

loser:
    if(safeInfo->cinfo) {
	SEC_PKCS7DestroyContentInfo(safeInfo->cinfo);
    }

    PORT_ArenaRelease(p12ctxt->arena, mark);
    return NULL;
}

/* SEC_PKCS12CreatePubKeyEncryptedSafe
 *	Creates a safe which is protected by public key encryption.  
 *
 *	p12ctxt - the export context
 *	certDb - the certificate database
 *	signer - the signer's certificate
 *	recipients - the list of recipient certificates.
 *	algorithm - the encryption algorithm to use
 *	keysize - the algorithms key size (?)
 */
SEC_PKCS12SafeInfo *
SEC_PKCS12CreatePubKeyEncryptedSafe(SEC_PKCS12ExportContext *p12ctxt,
				    CERTCertDBHandle *certDb,
				    CERTCertificate *signer,
				    CERTCertificate **recipients,
				    SECOidTag algorithm, int keysize) 
{
    SEC_PKCS12SafeInfo *safeInfo = NULL;
    void *mark = NULL;

    if(!p12ctxt || !signer || !recipients || !(*recipients)) {
	return NULL;
    }

    /* allocate the safeInfo */
    mark = PORT_ArenaMark(p12ctxt->arena);
    safeInfo = (SEC_PKCS12SafeInfo *)PORT_ArenaZAlloc(p12ctxt->arena, 
    						      sizeof(SEC_PKCS12SafeInfo));
    if(!safeInfo) {
	PORT_ArenaRelease(p12ctxt->arena, mark);
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	return NULL;
    }

    safeInfo->itemCount = 0;
    safeInfo->arena = p12ctxt->arena;

    /* create the enveloped content info using certUsageEmailSigner currently.
     * XXX We need to eventually use something other than certUsageEmailSigner
     */
    safeInfo->cinfo = SEC_PKCS7CreateEnvelopedData(signer, certUsageEmailSigner,
					certDb, algorithm, keysize, 
					p12ctxt->pwfn, p12ctxt->pwfnarg);
    if(!safeInfo->cinfo) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	goto loser;
    }

    /* add recipients */
    if(recipients) {
	unsigned int i = 0;
	while(recipients[i] != NULL) {
	    SECStatus rv = SEC_PKCS7AddRecipient(safeInfo->cinfo, recipients[i],
					       certUsageEmailRecipient, certDb);
	    if(rv != SECSuccess) {
		goto loser;
	    }
	    i++;
	}
    }

    if(sec_pkcs12_append_safe_info(p12ctxt, safeInfo) != SECSuccess) {
	goto loser;
    }

    PORT_ArenaUnmark(p12ctxt->arena, mark);
    return safeInfo;

loser:
    if(safeInfo->cinfo) {
	SEC_PKCS7DestroyContentInfo(safeInfo->cinfo);
	safeInfo->cinfo = NULL;
    }

    PORT_ArenaRelease(p12ctxt->arena, mark);
    return NULL;
} 

/*********************************
 * Routines to handle the exporting of the keys and certificates
 *********************************/

/* creates a safe contents which safeBags will be appended to */
sec_PKCS12SafeContents *
sec_PKCS12CreateSafeContents(PRArenaPool *arena)
{
    sec_PKCS12SafeContents *safeContents;

    if(arena == NULL) {
	return NULL; 
    }

    /* create the safe contents */
    safeContents = (sec_PKCS12SafeContents *)PORT_ArenaZAlloc(arena,
					    sizeof(sec_PKCS12SafeContents));
    if(!safeContents) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	goto loser;
    }

    /* set up the internal contents info */
    safeContents->safeBags = NULL;
    safeContents->arena = arena;
    safeContents->bagCount = 0;

    return safeContents;

loser:
    return NULL;
}   

/* appends a safe bag to a safeContents using the specified arena. 
 */
SECStatus
sec_pkcs12_append_bag_to_safe_contents(PRArenaPool *arena, 
				       sec_PKCS12SafeContents *safeContents,
				       sec_PKCS12SafeBag *safeBag)
{
    void *mark = NULL, *dummy = NULL;

    if(!arena || !safeBag || !safeContents) {
	return SECFailure;
    }

    mark = PORT_ArenaMark(arena);
    if(!mark) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	return SECFailure;
    }

    /* allocate space for the list, or reallocate to increase space */
    if(!safeContents->safeBags) {
	safeContents->safeBags = (sec_PKCS12SafeBag **)PORT_ArenaZAlloc(arena, 
						(2 * sizeof(sec_PKCS12SafeBag *)));
	dummy = safeContents->safeBags;
	safeContents->bagCount = 0;
    } else {
	dummy = PORT_ArenaGrow(arena, safeContents->safeBags, 
			(safeContents->bagCount + 1) * sizeof(sec_PKCS12SafeBag *),
			(safeContents->bagCount + 2) * sizeof(sec_PKCS12SafeBag *));
	safeContents->safeBags = (sec_PKCS12SafeBag **)dummy;
    }

    if(!dummy) {
	PORT_ArenaRelease(arena, mark);
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	return SECFailure;
    }

    /* append the bag at the end and null terminate the list */
    safeContents->safeBags[safeContents->bagCount++] = safeBag;
    safeContents->safeBags[safeContents->bagCount] = NULL;

    PORT_ArenaUnmark(arena, mark);

    return SECSuccess;
}

/* appends a safeBag to a specific safeInfo.
 */
SECStatus
sec_pkcs12_append_bag(SEC_PKCS12ExportContext *p12ctxt, 
		      SEC_PKCS12SafeInfo *safeInfo, sec_PKCS12SafeBag *safeBag)
{
    sec_PKCS12SafeContents *dest;
    SECStatus rv = SECFailure;

    if(!p12ctxt || !safeBag || !safeInfo) {
	return SECFailure;
    }

    if(!safeInfo->safe) {
	safeInfo->safe = sec_PKCS12CreateSafeContents(p12ctxt->arena);
	if(!safeInfo->safe) {
	    return SECFailure;
	}
    }

    dest = safeInfo->safe;
    rv = sec_pkcs12_append_bag_to_safe_contents(p12ctxt->arena, dest, safeBag);
    if(rv == SECSuccess) {
	safeInfo->itemCount++;
    }
    
    return rv;
} 

/* Creates a safeBag of the specified type, and if bagData is specified,
 * the contents are set.  The contents could be set later by the calling
 * routine.
 */
sec_PKCS12SafeBag *
sec_PKCS12CreateSafeBag(SEC_PKCS12ExportContext *p12ctxt, SECOidTag bagType, 
			void *bagData)
{
    sec_PKCS12SafeBag *safeBag;
    PRBool setName = PR_TRUE;
    void *mark = NULL;
    SECStatus rv = SECSuccess;
    SECOidData *oidData = NULL;

    if(!p12ctxt) {
	return NULL;
    }

    mark = PORT_ArenaMark(p12ctxt->arena);
    if(!mark) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	return NULL;
    }

    safeBag = (sec_PKCS12SafeBag *)PORT_ArenaZAlloc(p12ctxt->arena, 
    						    sizeof(sec_PKCS12SafeBag));
    if(!safeBag) {
	PORT_ArenaRelease(p12ctxt->arena, mark);
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	return NULL;
    }

    /* set the bags content based upon bag type */
    switch(bagType) {
	case SEC_OID_PKCS12_V1_KEY_BAG_ID:
	    safeBag->safeBagContent.pkcs8KeyBag =
	        (SECKEYPrivateKeyInfo *)bagData;
	    break;
	case SEC_OID_PKCS12_V1_CERT_BAG_ID:
	    safeBag->safeBagContent.certBag = (sec_PKCS12CertBag *)bagData;
	    break;
	case SEC_OID_PKCS12_V1_CRL_BAG_ID:
	    safeBag->safeBagContent.crlBag = (sec_PKCS12CRLBag *)bagData;
	    break;
	case SEC_OID_PKCS12_V1_SECRET_BAG_ID:
	    safeBag->safeBagContent.secretBag = (sec_PKCS12SecretBag *)bagData;
	    break;
	case SEC_OID_PKCS12_V1_PKCS8_SHROUDED_KEY_BAG_ID:
	    safeBag->safeBagContent.pkcs8ShroudedKeyBag = 
	        (SECKEYEncryptedPrivateKeyInfo *)bagData;
	    break;
	case SEC_OID_PKCS12_V1_SAFE_CONTENTS_BAG_ID:
	    safeBag->safeBagContent.safeContents = 
	        (sec_PKCS12SafeContents *)bagData;
	    setName = PR_FALSE;
	    break;
	default:
	    goto loser;
    }

    oidData = SECOID_FindOIDByTag(bagType);
    if(oidData) {
	rv = SECITEM_CopyItem(p12ctxt->arena, &safeBag->safeBagType, &oidData->oid);
	if(rv != SECSuccess) {
	    PORT_SetError(SEC_ERROR_NO_MEMORY);
	    goto loser;
	}
    } else {
	goto loser;
    }
    
    safeBag->arena = p12ctxt->arena;
    PORT_ArenaUnmark(p12ctxt->arena, mark);

    return safeBag;

loser:
    if(mark) {
	PORT_ArenaRelease(p12ctxt->arena, mark);
    }

    return NULL;
}

/* Creates a new certificate bag and returns a pointer to it.  If an error
 * occurs NULL is returned.
 */
sec_PKCS12CertBag *
sec_PKCS12NewCertBag(PRArenaPool *arena, SECOidTag certType)
{
    sec_PKCS12CertBag *certBag = NULL;
    SECOidData *bagType = NULL;
    SECStatus rv;
    void *mark = NULL;

    if(!arena) {
	return NULL;
    }

    mark = PORT_ArenaMark(arena);
    certBag = (sec_PKCS12CertBag *)PORT_ArenaZAlloc(arena, 
    						    sizeof(sec_PKCS12CertBag));
    if(!certBag) {
	PORT_ArenaRelease(arena, mark);
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	return NULL;
    }

    bagType = SECOID_FindOIDByTag(certType);
    if(!bagType) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	goto loser;
    }

    rv = SECITEM_CopyItem(arena, &certBag->bagID, &bagType->oid);
    if(rv != SECSuccess) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	goto loser;
    }
	
    PORT_ArenaUnmark(arena, mark);
    return certBag;

loser:
    PORT_ArenaRelease(arena, mark);
    return NULL;
}

/* Creates a new CRL bag and returns a pointer to it.  If an error
 * occurs NULL is returned.
 */
sec_PKCS12CRLBag *
sec_PKCS12NewCRLBag(PRArenaPool *arena, SECOidTag crlType)
{
    sec_PKCS12CRLBag *crlBag = NULL;
    SECOidData *bagType = NULL;
    SECStatus rv;
    void *mark = NULL;

    if(!arena) {
	return NULL;
    }

    mark = PORT_ArenaMark(arena);
    crlBag = (sec_PKCS12CRLBag *)PORT_ArenaZAlloc(arena, 
    						  sizeof(sec_PKCS12CRLBag));
    if(!crlBag) {
	PORT_ArenaRelease(arena, mark);
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	return NULL;
    }

    bagType = SECOID_FindOIDByTag(crlType);
    if(!bagType) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	goto loser;
    }

    rv = SECITEM_CopyItem(arena, &crlBag->bagID, &bagType->oid);
    if(rv != SECSuccess) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	goto loser;
    }
	
    PORT_ArenaUnmark(arena, mark);
    return crlBag;

loser:
    PORT_ArenaRelease(arena, mark);
    return NULL;
}

/* sec_PKCS12AddAttributeToBag
 * adds an attribute to a safeBag.  currently, the only attributes supported
 * are those which are specified within PKCS 12.  
 *
 *	p12ctxt - the export context 
 *	safeBag - the safeBag to which attributes are appended
 *	attrType - the attribute type
 * 	attrData - the attribute data
 */
SECStatus
sec_PKCS12AddAttributeToBag(SEC_PKCS12ExportContext *p12ctxt, 
			    sec_PKCS12SafeBag *safeBag, SECOidTag attrType,
			    SECItem *attrData)
{
    sec_PKCS12Attribute *attribute;
    void *mark = NULL, *dummy = NULL;
    SECOidData *oiddata = NULL;
    SECItem unicodeName = { siBuffer, NULL, 0};
    void *src = NULL;
    unsigned int nItems = 0;
    SECStatus rv;

    if(!safeBag || !p12ctxt) {
	return SECFailure;
    }

    mark = PORT_ArenaMark(safeBag->arena);

    /* allocate the attribute */
    attribute = (sec_PKCS12Attribute *)PORT_ArenaZAlloc(safeBag->arena, 
    						sizeof(sec_PKCS12Attribute));
    if(!attribute) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	goto loser;
    }

    /* set up the attribute */
    oiddata = SECOID_FindOIDByTag(attrType);
    if(!oiddata) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	goto loser;
    }
    if(SECITEM_CopyItem(p12ctxt->arena, &attribute->attrType, &oiddata->oid) !=
    		SECSuccess) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	goto loser;
    }

    nItems = 1;
    switch(attrType) {
	case SEC_OID_PKCS9_LOCAL_KEY_ID:
	    {
		src = attrData;
		break;
	    }
	case SEC_OID_PKCS9_FRIENDLY_NAME:
	    {
		if(!sec_pkcs12_convert_item_to_unicode(p12ctxt->arena, 
					&unicodeName, attrData, PR_FALSE, 
					PR_FALSE, PR_TRUE)) {
		    goto loser;
		}
		src = &unicodeName;
		break;
	    }
	default:
	    goto loser;
    }

    /* append the attribute to the attribute value list  */
    attribute->attrValue = (SECItem **)PORT_ArenaZAlloc(p12ctxt->arena, 
    					    ((nItems + 1) * sizeof(SECItem *)));
    if(!attribute->attrValue) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	goto loser;
    }

    /* XXX this will need to be changed if attributes requiring more than
     * one element are ever used.
     */
    attribute->attrValue[0] = (SECItem *)PORT_ArenaZAlloc(p12ctxt->arena, 
    							  sizeof(SECItem));
    if(!attribute->attrValue[0]) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	goto loser;
    }
    attribute->attrValue[1] = NULL;

    rv = SECITEM_CopyItem(p12ctxt->arena, attribute->attrValue[0], 
			  (SECItem*)src);
    if(rv != SECSuccess) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	goto loser;
    }

    /* append the attribute to the safeBag attributes */
    if(safeBag->nAttribs) {
	dummy = PORT_ArenaGrow(p12ctxt->arena, safeBag->attribs, 
			((safeBag->nAttribs + 1) * sizeof(sec_PKCS12Attribute *)),
			((safeBag->nAttribs + 2) * sizeof(sec_PKCS12Attribute *)));
	safeBag->attribs = (sec_PKCS12Attribute **)dummy;
    } else {
	safeBag->attribs = (sec_PKCS12Attribute **)PORT_ArenaZAlloc(p12ctxt->arena, 
						2 * sizeof(sec_PKCS12Attribute *));
	dummy = safeBag->attribs;
    }
    if(!dummy) {
	goto loser;
    }

    safeBag->attribs[safeBag->nAttribs] = attribute;
    safeBag->attribs[++safeBag->nAttribs] = NULL;

    PORT_ArenaUnmark(p12ctxt->arena, mark);
    return SECSuccess;

loser:
    if(mark) {
	PORT_ArenaRelease(p12ctxt->arena, mark);
    }

    return SECFailure;
}

/* SEC_PKCS12AddCert
 * 	Adds a certificate to the data being exported.  
 *
 *	p12ctxt - the export context
 *	safe - the safeInfo to which the certificate is placed 
 *	nestedDest - if the cert is to be placed within a nested safeContents then,
 *		     this value is to be specified with the destination
 *	cert - the cert to export
 *	certDb - the certificate database handle
 *	keyId - a unique identifier to associate a certificate/key pair
 *	includeCertChain - PR_TRUE if the certificate chain is to be included.
 */
SECStatus
SEC_PKCS12AddCert(SEC_PKCS12ExportContext *p12ctxt, SEC_PKCS12SafeInfo *safe, 
		  void *nestedDest, CERTCertificate *cert, 
		  CERTCertDBHandle *certDb, SECItem *keyId,
		  PRBool includeCertChain)
{
    sec_PKCS12CertBag *certBag;
    sec_PKCS12SafeBag *safeBag;
    void *mark;
    SECStatus rv;
    SECItem nick = {siBuffer, NULL,0};

    if(!p12ctxt || !cert) {
	return SECFailure;
    }
    mark = PORT_ArenaMark(p12ctxt->arena);

    /* allocate the cert bag */
    certBag = sec_PKCS12NewCertBag(p12ctxt->arena, 
    				   SEC_OID_PKCS9_X509_CERT);
    if(!certBag) {
	goto loser;
    }

    if(SECITEM_CopyItem(p12ctxt->arena, &certBag->value.x509Cert, 
    			&cert->derCert) != SECSuccess) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	goto loser;
    }

    /* if the cert chain is to be included, we should only be exporting
     * the cert from our internal database.
     */
    if(includeCertChain) {
	CERTCertificateList *certList = CERT_CertChainFromCert(cert,
							       certUsageSSLClient,
							       PR_TRUE);
	unsigned int count = 0;
	if(!certList) {
	    PORT_SetError(SEC_ERROR_NO_MEMORY);
	    goto loser;
	}

	/* add cert chain */
	for(count = 0; count < (unsigned int)certList->len; count++) {
	    if(SECITEM_CompareItem(&certList->certs[count], &cert->derCert)
	    			!= SECEqual) {
	    	CERTCertificate *tempCert;

		/* decode the certificate */
	    	tempCert = CERT_NewTempCertificate(certDb, 
	    					&certList->certs[count], NULL,
	    					PR_FALSE, PR_TRUE);
	    	if(!tempCert) {
		    CERT_DestroyCertificateList(certList);
		    goto loser;
		}

		/* add the certificate */
	    	if(SEC_PKCS12AddCert(p12ctxt, safe, nestedDest, tempCert, certDb,
				     NULL, PR_FALSE) != SECSuccess) {
		    CERT_DestroyCertificate(tempCert);
		    CERT_DestroyCertificateList(certList);
		    goto loser;
		}
		CERT_DestroyCertificate(tempCert);
	    }
	}
	CERT_DestroyCertificateList(certList);
    }

    /* if the certificate has a nickname, we will set the friendly name
     * to that.
     */
    if(cert->nickname) {
        if (cert->slot && !PK11_IsInternal(cert->slot)) {
	  /*
	   * The cert is coming off of an external token, 
	   * let's strip the token name from the nickname
	   * and only add what comes after the colon as the
	   * nickname. -javi
	   */
	    char *delimit;
	    
	    delimit = PORT_Strchr(cert->nickname,':');
	    if (delimit == NULL) {
	        nick.data = (unsigned char *)cert->nickname;
		nick.len = PORT_Strlen(cert->nickname);
	    } else {
	        delimit++;
	        nick.data = (unsigned char *)PORT_ArenaStrdup(p12ctxt->arena,
							      delimit);
		nick.len = PORT_Strlen(delimit);
	    }
	} else {
	    nick.data = (unsigned char *)cert->nickname;
	    nick.len = PORT_Strlen(cert->nickname);
	}
    }

    safeBag = sec_PKCS12CreateSafeBag(p12ctxt, SEC_OID_PKCS12_V1_CERT_BAG_ID, 
    				      certBag);
    if(!safeBag) {
	goto loser;
    }

    /* add the friendly name and keyId attributes, if necessary */
    if(nick.data) {
	if(sec_PKCS12AddAttributeToBag(p12ctxt, safeBag, 
				       SEC_OID_PKCS9_FRIENDLY_NAME, &nick) 
				       != SECSuccess) {
	    goto loser;
	}
    }
	   
    if(keyId) {
	if(sec_PKCS12AddAttributeToBag(p12ctxt, safeBag, SEC_OID_PKCS9_LOCAL_KEY_ID,
				       keyId) != SECSuccess) {
	    goto loser;
	}
    }

    /* append the cert safeBag */
    if(nestedDest) {
	rv = sec_pkcs12_append_bag_to_safe_contents(p12ctxt->arena, 
					  (sec_PKCS12SafeContents*)nestedDest, 
					   safeBag);
    } else {
	rv = sec_pkcs12_append_bag(p12ctxt, safe, safeBag);
    }

    if(rv != SECSuccess) {
	goto loser;
    }

    PORT_ArenaUnmark(p12ctxt->arena, mark);
    return SECSuccess;

loser:
    if(mark) {
	PORT_ArenaRelease(p12ctxt->arena, mark);
    }

    return SECFailure;
}

/* SEC_PKCS12AddEncryptedKey
 *	Extracts the key associated with a particular certificate and exports
 *	it.
 *
 *	p12ctxt - the export context 
 *	safe - the safeInfo to place the key in
 *	nestedDest - the nested safeContents to place a key
 *	cert - the certificate which the key belongs to
 *	shroudKey - encrypt the private key for export.  This value should 
 *		always be true.  lower level code will not allow the export
 *		of unencrypted private keys.
 *	algorithm - the algorithm with which to encrypt the private key
 *	pwitem - the password to encrypted the private key with
 *	keyId - the keyID attribute
 *	nickName - the nickname attribute
 */
static SECStatus
SEC_PKCS12AddEncryptedKey(SEC_PKCS12ExportContext *p12ctxt, 
		SECKEYEncryptedPrivateKeyInfo *epki, SEC_PKCS12SafeInfo *safe,
		void *nestedDest, SECItem *keyId, SECItem *nickName)
{
    void *mark;
    void *keyItem;
    SECOidTag keyType;
    SECStatus rv = SECFailure;
    sec_PKCS12SafeBag *returnBag;

    if(!p12ctxt || !safe || !epki) {
	return SECFailure;
    }

    mark = PORT_ArenaMark(p12ctxt->arena);

    keyItem = PORT_ArenaZAlloc(p12ctxt->arena, 
				sizeof(SECKEYEncryptedPrivateKeyInfo));
    if(!keyItem) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	goto loser;
    }

    rv = SECKEY_CopyEncryptedPrivateKeyInfo(p12ctxt->arena, 
					(SECKEYEncryptedPrivateKeyInfo *)keyItem,
					epki);
    keyType = SEC_OID_PKCS12_V1_PKCS8_SHROUDED_KEY_BAG_ID;

    if(rv != SECSuccess) {
	goto loser;
    }
	
    /* create the safe bag and set any attributes */
    returnBag = sec_PKCS12CreateSafeBag(p12ctxt, keyType, keyItem);
    if(!returnBag) {
	rv = SECFailure;
	goto loser;
    }

    if(nickName) {
	if(sec_PKCS12AddAttributeToBag(p12ctxt, returnBag, 
				       SEC_OID_PKCS9_FRIENDLY_NAME, nickName) 
				       != SECSuccess) {
	    goto loser;
	}
    }
	   
    if(keyId) {
	if(sec_PKCS12AddAttributeToBag(p12ctxt, returnBag, SEC_OID_PKCS9_LOCAL_KEY_ID,
				       keyId) != SECSuccess) {
	    goto loser;
	}
    }

    if(nestedDest) {
	rv = sec_pkcs12_append_bag_to_safe_contents(p12ctxt->arena, 
					   (sec_PKCS12SafeContents*)nestedDest,
					   returnBag);
    } else {
	rv = sec_pkcs12_append_bag(p12ctxt, safe, returnBag);
    }

loser:

    if (rv != SECSuccess) {
	PORT_ArenaRelease(p12ctxt->arena, mark);
    } else {
	PORT_ArenaUnmark(p12ctxt->arena, mark);
    }

    return rv;
}

/* SEC_PKCS12AddKeyForCert
 *	Extracts the key associated with a particular certificate and exports
 *	it.
 *
 *	p12ctxt - the export context 
 *	safe - the safeInfo to place the key in
 *	nestedDest - the nested safeContents to place a key
 *	cert - the certificate which the key belongs to
 *	shroudKey - encrypt the private key for export.  This value should 
 *		always be true.  lower level code will not allow the export
 *		of unencrypted private keys.
 *	algorithm - the algorithm with which to encrypt the private key
 *	pwitem - the password to encrypt the private key with
 *	keyId - the keyID attribute
 *	nickName - the nickname attribute
 */
SECStatus
SEC_PKCS12AddKeyForCert(SEC_PKCS12ExportContext *p12ctxt, SEC_PKCS12SafeInfo *safe, 
			void *nestedDest, CERTCertificate *cert,
			PRBool shroudKey, SECOidTag algorithm, SECItem *pwitem,
			SECItem *keyId, SECItem *nickName)
{
    void *mark;
    void *keyItem;
    SECOidTag keyType;
    SECStatus rv = SECFailure;
    SECItem nickname = {siBuffer,NULL,0}, uniPwitem = {siBuffer, NULL, 0};
    sec_PKCS12SafeBag *returnBag;

    if(!p12ctxt || !cert || !safe) {
	return SECFailure;
    }

    mark = PORT_ArenaMark(p12ctxt->arena);

    /* retrieve the key based upon the type that it is and 
     * specify the type of safeBag to store the key in
     */	   
    if(!shroudKey) {

	/* extract the key unencrypted.  this will most likely go away */
	SECKEYPrivateKeyInfo *pki = PK11_ExportPrivateKeyInfo(cert, 
							      p12ctxt->wincx);
	if(!pki) {
	    PORT_ArenaRelease(p12ctxt->arena, mark);
	    PORT_SetError(SEC_ERROR_PKCS12_UNABLE_TO_EXPORT_KEY);
	    return SECFailure;
	}   
	keyItem = PORT_ArenaZAlloc(p12ctxt->arena, sizeof(SECKEYPrivateKeyInfo));
	if(!keyItem) {
	    PORT_SetError(SEC_ERROR_NO_MEMORY);
	    goto loser;
	}
	rv = SECKEY_CopyPrivateKeyInfo(p12ctxt->arena, 
				       (SECKEYPrivateKeyInfo *)keyItem, pki);
	keyType = SEC_OID_PKCS12_V1_KEY_BAG_ID;
	SECKEY_DestroyPrivateKeyInfo(pki, PR_TRUE);
    } else {

	/* extract the key encrypted */
	SECKEYEncryptedPrivateKeyInfo *epki = NULL;
	PK11SlotInfo *slot = p12ctxt->slot;

	if(!sec_pkcs12_convert_item_to_unicode(p12ctxt->arena, &uniPwitem,
				 pwitem, PR_TRUE, PR_TRUE, PR_TRUE)) {
	    PORT_SetError(SEC_ERROR_NO_MEMORY);
	    goto loser;
	}

	/* we want to make sure to take the key out of the key slot */
	if(PK11_IsInternal(p12ctxt->slot)) {
	    slot = PK11_GetInternalKeySlot();
	}

	epki = PK11_ExportEncryptedPrivateKeyInfo(slot, algorithm, 
						  &uniPwitem, cert, 1, 
						  p12ctxt->wincx);
	if(PK11_IsInternal(p12ctxt->slot)) {
	    PK11_FreeSlot(slot);
	}
	
	keyItem = PORT_ArenaZAlloc(p12ctxt->arena, 
				  sizeof(SECKEYEncryptedPrivateKeyInfo));
	if(!keyItem) {
	    PORT_SetError(SEC_ERROR_NO_MEMORY);
	    goto loser;
	}
	if(!epki) {
	    PORT_SetError(SEC_ERROR_PKCS12_UNABLE_TO_EXPORT_KEY);
	    return SECFailure;
	}   
	rv = SECKEY_CopyEncryptedPrivateKeyInfo(p12ctxt->arena, 
					(SECKEYEncryptedPrivateKeyInfo *)keyItem,
					epki);
	keyType = SEC_OID_PKCS12_V1_PKCS8_SHROUDED_KEY_BAG_ID;
	SECKEY_DestroyEncryptedPrivateKeyInfo(epki, PR_TRUE);
    }

    if(rv != SECSuccess) {
	goto loser;
    }
	
    /* if no nickname specified, let's see if the certificate has a 
     * nickname.
     */					  
    if(!nickName) {
	if(cert->nickname) {
	    nickname.data = (unsigned char *)cert->nickname;
	    nickname.len = PORT_Strlen(cert->nickname);
	    nickName = &nickname;
	}
    }

    /* create the safe bag and set any attributes */
    returnBag = sec_PKCS12CreateSafeBag(p12ctxt, keyType, keyItem);
    if(!returnBag) {
	rv = SECFailure;
	goto loser;
    }

    if(nickName) {
	if(sec_PKCS12AddAttributeToBag(p12ctxt, returnBag, 
				       SEC_OID_PKCS9_FRIENDLY_NAME, nickName) 
				       != SECSuccess) {
	    goto loser;
	}
    }
	   
    if(keyId) {
	if(sec_PKCS12AddAttributeToBag(p12ctxt, returnBag, SEC_OID_PKCS9_LOCAL_KEY_ID,
				       keyId) != SECSuccess) {
	    goto loser;
	}
    }

    if(nestedDest) {
	rv = sec_pkcs12_append_bag_to_safe_contents(p12ctxt->arena,
					  (sec_PKCS12SafeContents*)nestedDest, 
					  returnBag);
    } else {
	rv = sec_pkcs12_append_bag(p12ctxt, safe, returnBag);
    }

loser:

    if (rv != SECSuccess) {
	PORT_ArenaRelease(p12ctxt->arena, mark);
    } else {
	PORT_ArenaUnmark(p12ctxt->arena, mark);
    }

    return rv;
}

/* SEC_PKCS12AddCertAndEncryptedKey
 *	Add a certificate and key pair to be exported.
 *
 *	p12ctxt - the export context 
 * 	certSafe - the safeInfo where the cert is stored
 *	certNestedDest - the nested safeContents to store the cert
 *	keySafe - the safeInfo where the key is stored
 *	keyNestedDest - the nested safeContents to store the key
 *	shroudKey - extract the private key encrypted?
 *	pwitem - the password with which the key is encrypted
 *	algorithm - the algorithm with which the key is encrypted
 */
SECStatus
SEC_PKCS12AddDERCertAndEncryptedKey(SEC_PKCS12ExportContext *p12ctxt, 
		void *certSafe, void *certNestedDest, 
		SECItem *derCert, void *keySafe, 
		void *keyNestedDest, SECKEYEncryptedPrivateKeyInfo *epki,
		char *nickname)
{		
    SECStatus rv = SECFailure;
    SGNDigestInfo *digest = NULL;
    void *mark = NULL;
    CERTCertificate *cert;
    SECItem nick = {siBuffer, NULL,0}, *nickPtr = NULL; 

    if(!p12ctxt || !certSafe || !keySafe || !derCert) {
	return SECFailure;
    }

    mark = PORT_ArenaMark(p12ctxt->arena);

    cert = CERT_NewTempCertificate(CERT_GetDefaultCertDB(), derCert,
				   NULL, PR_FALSE, PR_TRUE);
    if(!cert) {
	PORT_ArenaRelease(p12ctxt->arena, mark);
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	return SECFailure;
    }
    cert->nickname = nickname;

    /* generate the thumbprint of the cert to use as a keyId */
    digest = sec_pkcs12_compute_thumbprint(&cert->derCert);
    if(!digest) {
	CERT_DestroyCertificate(cert);
	return SECFailure;
    }

    /* add the certificate */
    rv = SEC_PKCS12AddCert(p12ctxt, (SEC_PKCS12SafeInfo*)certSafe, 
			   certNestedDest, cert, NULL,
    			   &digest->digest, PR_FALSE);
    if(rv != SECSuccess) {
	goto loser;
    }

    if(nickname) {
	nick.data = (unsigned char *)nickname;
	nick.len = PORT_Strlen(nickname);
	nickPtr = &nick;
    } else {
	nickPtr = NULL;
    }

    /* add the key */
    rv = SEC_PKCS12AddEncryptedKey(p12ctxt, epki, (SEC_PKCS12SafeInfo*)keySafe,
				   keyNestedDest, &digest->digest, nickPtr );
    if(rv != SECSuccess) {
	goto loser;
    }

    SGN_DestroyDigestInfo(digest);

    PORT_ArenaUnmark(p12ctxt->arena, mark);
    return SECSuccess;

loser:
    SGN_DestroyDigestInfo(digest);
    CERT_DestroyCertificate(cert);
    PORT_ArenaRelease(p12ctxt->arena, mark);
    
    return SECFailure; 
}

/* SEC_PKCS12AddCertAndKey
 *	Add a certificate and key pair to be exported.
 *
 *	p12ctxt - the export context 
 * 	certSafe - the safeInfo where the cert is stored
 *	certNestedDest - the nested safeContents to store the cert
 *	keySafe - the safeInfo where the key is stored
 *	keyNestedDest - the nested safeContents to store the key
 *	shroudKey - extract the private key encrypted?
 *	pwitem - the password with which the key is encrypted
 *	algorithm - the algorithm with which the key is encrypted
 */
SECStatus
SEC_PKCS12AddCertAndKey(SEC_PKCS12ExportContext *p12ctxt, 
			void *certSafe, void *certNestedDest, 
			CERTCertificate *cert, CERTCertDBHandle *certDb,
			void *keySafe, void *keyNestedDest, 
			PRBool shroudKey, SECItem *pwitem, SECOidTag algorithm)
{		
    SECStatus rv = SECFailure;
    SGNDigestInfo *digest = NULL;
    void *mark = NULL;

    if(!p12ctxt || !certSafe || !keySafe || !cert) {
	return SECFailure;
    }

    mark = PORT_ArenaMark(p12ctxt->arena);

    /* generate the thumbprint of the cert to use as a keyId */
    digest = sec_pkcs12_compute_thumbprint(&cert->derCert);
    if(!digest) {
	PORT_ArenaRelease(p12ctxt->arena, mark);
	return SECFailure;
    }

    /* add the certificate */
    rv = SEC_PKCS12AddCert(p12ctxt, (SEC_PKCS12SafeInfo*)certSafe, 
			   (SEC_PKCS12SafeInfo*)certNestedDest, cert, certDb,
    			   &digest->digest, PR_TRUE);
    if(rv != SECSuccess) {
	goto loser;
    }

    /* add the key */
    rv = SEC_PKCS12AddKeyForCert(p12ctxt, (SEC_PKCS12SafeInfo*)keySafe, 
				 keyNestedDest, cert, 
    				 shroudKey, algorithm, pwitem, 
    				 &digest->digest, NULL );
    if(rv != SECSuccess) {
	goto loser;
    }

    SGN_DestroyDigestInfo(digest);

    PORT_ArenaUnmark(p12ctxt->arena, mark);
    return SECSuccess;

loser:
    SGN_DestroyDigestInfo(digest);
    PORT_ArenaRelease(p12ctxt->arena, mark);
    
    return SECFailure; 
}

/* SEC_PKCS12CreateNestedSafeContents
 * 	Allows nesting of safe contents to be implemented.  No limit imposed on 
 *	depth.  
 *
 *	p12ctxt - the export context 
 *	baseSafe - the base safeInfo 
 *	nestedDest - a parent safeContents (?)
 */
void *
SEC_PKCS12CreateNestedSafeContents(SEC_PKCS12ExportContext *p12ctxt,
				   void *baseSafe, void *nestedDest)
{
    sec_PKCS12SafeContents *newSafe;
    sec_PKCS12SafeBag *safeContentsBag;
    void *mark;
    SECStatus rv;

    if(!p12ctxt || !baseSafe) {
	return NULL;
    }

    mark = PORT_ArenaMark(p12ctxt->arena);

    newSafe = sec_PKCS12CreateSafeContents(p12ctxt->arena);
    if(!newSafe) {
	PORT_ArenaRelease(p12ctxt->arena, mark);
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	return NULL;
    }

    /* create the safeContents safeBag */
    safeContentsBag = sec_PKCS12CreateSafeBag(p12ctxt, 
					SEC_OID_PKCS12_V1_SAFE_CONTENTS_BAG_ID,
					newSafe);
    if(!safeContentsBag) {
	goto loser;
    }

    /* append the safeContents to the appropriate area */
    if(nestedDest) {
	rv = sec_pkcs12_append_bag_to_safe_contents(p12ctxt->arena, 
					   (sec_PKCS12SafeContents*)nestedDest,
					   safeContentsBag);
    } else {
	rv = sec_pkcs12_append_bag(p12ctxt, (SEC_PKCS12SafeInfo*)baseSafe, 
				   safeContentsBag);
    }
    if(rv != SECSuccess) {
	goto loser;
    }

    PORT_ArenaUnmark(p12ctxt->arena, mark);
    return newSafe;

loser:
    PORT_ArenaRelease(p12ctxt->arena, mark);
    return NULL;
}

/*********************************
 * Encoding routines
 *********************************/

/* set up the encoder context based on information in the export context
 * and return the newly allocated enocoder context.  A return of NULL 
 * indicates an error occurred. 
 */
sec_PKCS12EncoderContext *
sec_pkcs12_encoder_start_context(SEC_PKCS12ExportContext *p12exp)
{
    sec_PKCS12EncoderContext *p12enc = NULL;
    unsigned int i, nonEmptyCnt;
    SECStatus rv;
    SECItem ignore = {0};
    void *mark;

    if(!p12exp || !p12exp->safeInfos) {
	return NULL;
    }

    /* check for any empty safes and skip them */
    i = nonEmptyCnt = 0;
    while(p12exp->safeInfos[i]) {
	if(p12exp->safeInfos[i]->itemCount) {
	    nonEmptyCnt++;
	}
	i++;
    }
    if(nonEmptyCnt == 0) {
	return NULL;
    }
    p12exp->authSafe.encodedSafes[nonEmptyCnt] = NULL;

    /* allocate the encoder context */
    mark = PORT_ArenaMark(p12exp->arena);
    p12enc = (sec_PKCS12EncoderContext*)PORT_ArenaZAlloc(p12exp->arena, 
    			      sizeof(sec_PKCS12EncoderContext));
    if(!p12enc) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	return NULL;
    }

    p12enc->arena = p12exp->arena;
    p12enc->p12exp = p12exp;

    /* set up the PFX version and information */
    PORT_Memset(&p12enc->pfx, 0, sizeof(sec_PKCS12PFXItem));
    if(!SEC_ASN1EncodeInteger(p12exp->arena, &(p12enc->pfx.version), 
    			      SEC_PKCS12_VERSION) ) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
    	goto loser;
    }

    /* set up the authenticated safe content info based on the 
     * type of integrity being used.  this should be changed to
     * enforce integrity mode, but will not be implemented until
     * it is confirmed that integrity must be in place
     */
    if(p12exp->integrityEnabled && !p12exp->pwdIntegrity) {
	SECStatus rv;

	/* create public key integrity mode */
	p12enc->aSafeCinfo = SEC_PKCS7CreateSignedData(
				p12exp->integrityInfo.pubkeyInfo.cert,
				certUsageEmailSigner,
				p12exp->integrityInfo.pubkeyInfo.certDb,
				p12exp->integrityInfo.pubkeyInfo.algorithm,
				NULL,
				p12exp->pwfn,
				p12exp->pwfnarg);
	if(!p12enc->aSafeCinfo) {
	    goto loser;
	}
	if(SEC_PKCS7IncludeCertChain(p12enc->aSafeCinfo,NULL) != SECSuccess) {
	    goto loser;
	}
	rv = SEC_PKCS7AddSigningTime(p12enc->aSafeCinfo);
	PORT_Assert(rv == SECSuccess);
    } else {
	p12enc->aSafeCinfo = SEC_PKCS7CreateData();

	/* init password pased integrity mode */
	if(p12exp->integrityEnabled) {
	    SECItem  pwd = {siBuffer,NULL, 0};
	    SECItem *salt = sec_pkcs12_generate_salt();
	    PK11SymKey *symKey;
	    SECItem *params;
	    CK_MECHANISM_TYPE integrityMech;

	    /* zero out macData and set values */
	    PORT_Memset(&p12enc->mac, 0, sizeof(sec_PKCS12MacData));

	    if(!salt) {
		PORT_SetError(SEC_ERROR_NO_MEMORY);
		goto loser;
	    }
	    if(SECITEM_CopyItem(p12exp->arena, &(p12enc->mac.macSalt), salt) 
			!= SECSuccess) {
		PORT_SetError(SEC_ERROR_NO_MEMORY);
		goto loser;
	    }   

	    /* generate HMAC key */
	    if(!sec_pkcs12_convert_item_to_unicode(NULL, &pwd, 
			p12exp->integrityInfo.pwdInfo.password, PR_TRUE, 
			PR_TRUE, PR_TRUE)) {
		goto loser;
	    }

	    params = PK11_CreatePBEParams(salt, &pwd, 1);
	    SECITEM_ZfreeItem(salt, PR_TRUE);
	    SECITEM_ZfreeItem(&pwd, PR_FALSE);

	    switch (p12exp->integrityInfo.pwdInfo.algorithm) {
	    case SEC_OID_SHA1:
		integrityMech = CKM_NETSCAPE_PBE_SHA1_HMAC_KEY_GEN; break;
	    case SEC_OID_MD5:
		integrityMech = CKM_NETSCAPE_PBE_MD5_HMAC_KEY_GEN;  break;
	    case SEC_OID_MD2:
		integrityMech = CKM_NETSCAPE_PBE_MD2_HMAC_KEY_GEN;  break;
	    default:
		goto loser;
	    }

	    symKey = PK11_KeyGen(NULL, integrityMech, params, 20, NULL);
	    PK11_DestroyPBEParams(params);
	    if(!symKey) {
		goto loser;
	    }

	    /* initialize hmac */
	    p12enc->hmacCx = PK11_CreateContextBySymKey(
	     sec_pkcs12_algtag_to_mech(p12exp->integrityInfo.pwdInfo.algorithm),
	                                           CKA_SIGN, symKey, &ignore);
	    if(!p12enc->hmacCx) {
		PORT_SetError(SEC_ERROR_NO_MEMORY);
		goto loser;
	    }
	    rv = PK11_DigestBegin(p12enc->hmacCx);
	    if (rv != SECSuccess)
		goto loser;
	}
    }

    if(!p12enc->aSafeCinfo) {
	goto loser;
    }

    PORT_ArenaUnmark(p12exp->arena, mark);

    return p12enc;

loser:
    if(p12enc) {
	if(p12enc->aSafeCinfo) {
	    SEC_PKCS7DestroyContentInfo(p12enc->aSafeCinfo);
	}
	if(p12enc->hmacCx) {
	    PK11_DestroyContext(p12enc->hmacCx, PR_TRUE);
	}
    }
    if (p12exp->arena != NULL)
	PORT_ArenaRelease(p12exp->arena, mark);

    return NULL;
}

/* callback wrapper to allow the ASN1 engine to call the PKCS 12 
 * output routines.
 */
static void
sec_pkcs12_encoder_out(void *arg, const char *buf, unsigned long len,
		       int depth, SEC_ASN1EncodingPart data_kind)
{
    struct sec_pkcs12_encoder_output *output;

    output = (struct sec_pkcs12_encoder_output*)arg;
    (* output->outputfn)(output->outputarg, buf, len);
}

/* callback wrapper to wrap SEC_PKCS7EncoderUpdate for ASN1 encoder
 */
static void 
sec_pkcs12_wrap_pkcs7_encoder_update(void *arg, const char *buf, 
				     unsigned long len, int depth, 
				     SEC_ASN1EncodingPart data_kind)
{
    SEC_PKCS7EncoderContext *ecx;
    if(!buf || !len) {
	return;
    }

    ecx = (SEC_PKCS7EncoderContext*)arg;
    SEC_PKCS7EncoderUpdate(ecx, buf, len);
}

/* callback wrapper to wrap SEC_ASN1EncoderUpdate for PKCS 7 encoding
 */
static void
sec_pkcs12_wrap_asn1_update_for_p7_update(void *arg, const char *buf,
					  unsigned long len)
{
    if(!buf && !len) return;

    SEC_ASN1EncoderUpdate((SEC_ASN1EncoderContext*)arg, buf, len);
}

/* callback wrapper which updates the HMAC and passes on bytes to the 
 * appropriate output function.
 */
static void
sec_pkcs12_asafe_update_hmac_and_encode_bits(void *arg, const char *buf,
				      unsigned long len, int depth,
				      SEC_ASN1EncodingPart data_kind)
{
    sec_PKCS12EncoderContext *p12ecx;

    p12ecx = (sec_PKCS12EncoderContext*)arg;
    PK11_DigestOp(p12ecx->hmacCx, (unsigned char *)buf, len);
    sec_pkcs12_wrap_pkcs7_encoder_update(p12ecx->aSafeP7Ecx, buf, len,
    					 depth, data_kind);
}

/* this function encodes content infos which are part of the
 * sequence of content infos labeled AuthenticatedSafes 
 */
static SECStatus 
sec_pkcs12_encoder_asafe_process(sec_PKCS12EncoderContext *p12ecx)
{ 
    SECStatus rv = SECSuccess;
    SEC_PKCS5KeyAndPassword keyPwd;
    SEC_PKCS7EncoderContext *p7ecx;
    SEC_PKCS7ContentInfo *cinfo;
    SEC_ASN1EncoderContext *ecx = NULL;

    void *arg = NULL;

    if(p12ecx->currentSafe < p12ecx->p12exp->authSafe.safeCount) {
	SEC_PKCS12SafeInfo *safeInfo;
	SECOidTag cinfoType;

	safeInfo = p12ecx->p12exp->safeInfos[p12ecx->currentSafe];

	/* skip empty safes */
	if(safeInfo->itemCount == 0) {
	    return SECSuccess;
	}

	cinfo = safeInfo->cinfo;
	cinfoType = SEC_PKCS7ContentType(cinfo);

	/* determine the safe type and set the appropriate argument */
	switch(cinfoType) {
	    case SEC_OID_PKCS7_DATA:
		arg = NULL;
		break;
	    case SEC_OID_PKCS7_ENCRYPTED_DATA:
		keyPwd.pwitem = &safeInfo->pwitem;
		keyPwd.key = safeInfo->encryptionKey;
		arg = &keyPwd;
		break;
	    case SEC_OID_PKCS7_ENVELOPED_DATA:
		arg = NULL;
		break;
	    default:
		return SECFailure;

	}

	/* start the PKCS7 encoder */
	p7ecx = SEC_PKCS7EncoderStart(cinfo, 
				      sec_pkcs12_wrap_asn1_update_for_p7_update,
				      p12ecx->aSafeEcx, (PK11SymKey *)arg);
	if(!p7ecx) {
	    goto loser;
	}

	/* encode safe contents */
	ecx = SEC_ASN1EncoderStart(safeInfo->safe, sec_PKCS12SafeContentsTemplate,
				   sec_pkcs12_wrap_pkcs7_encoder_update, p7ecx);
	if(!ecx) {
	    goto loser;
	}   
	rv = SEC_ASN1EncoderUpdate(ecx, NULL, 0);
	SEC_ASN1EncoderFinish(ecx);
	ecx = NULL;
	if(rv != SECSuccess) {
	    goto loser;
	}


	/* finish up safe content info */
	rv = SEC_PKCS7EncoderFinish(p7ecx, p12ecx->p12exp->pwfn, 
				    p12ecx->p12exp->pwfnarg);
    }

    return SECSuccess;

loser:
    if(p7ecx) {
	SEC_PKCS7EncoderFinish(p7ecx, p12ecx->p12exp->pwfn, 
			       p12ecx->p12exp->pwfnarg);
    }

    if(ecx) {
	SEC_ASN1EncoderFinish(ecx);
    }

    return SECFailure;
}

/* finish the HMAC and encode the macData so that it can be
 * encoded.
 */
static SECStatus
sec_pkcs12_update_mac(sec_PKCS12EncoderContext *p12ecx)
{
    SECItem hmac = { siBuffer, NULL, 0 };
    SECStatus rv;
    SGNDigestInfo *di = NULL;
    void *dummy;

    if(!p12ecx) {
	return SECFailure;
    }

    /* make sure we are using password integrity mode */
    if(!p12ecx->p12exp->integrityEnabled) {
	return SECSuccess;
    }

    if(!p12ecx->p12exp->pwdIntegrity) {
	return SECSuccess;
    }

    /* finish the hmac */
    hmac.data = (unsigned char *)PORT_ZAlloc(SHA1_LENGTH);
    if(!hmac.data) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	return SECFailure;
    }

    rv = PK11_DigestFinal(p12ecx->hmacCx, hmac.data, &hmac.len, SHA1_LENGTH);

    if(rv != SECSuccess) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	goto loser;
    }

    /* create the digest info */
    di = SGN_CreateDigestInfo(p12ecx->p12exp->integrityInfo.pwdInfo.algorithm,
    			      hmac.data, hmac.len);
    if(!di) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	rv = SECFailure;
	goto loser;
    }

    rv = SGN_CopyDigestInfo(p12ecx->arena, &p12ecx->mac.safeMac, di);
    if(rv != SECSuccess) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	goto loser;
    }

    /* encode the mac data */
    dummy = SEC_ASN1EncodeItem(p12ecx->arena, &p12ecx->pfx.encodedMacData, 
    			    &p12ecx->mac, sec_PKCS12MacDataTemplate);
    if(!dummy) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	rv = SECFailure;
    }

loser:
    if(di) {
	SGN_DestroyDigestInfo(di);
    }
    if(hmac.data) {
	SECITEM_ZfreeItem(&hmac, PR_FALSE);
    }
    PK11_DestroyContext(p12ecx->hmacCx, PR_TRUE);
    p12ecx->hmacCx = NULL;

    return rv;
}

/* wraps the ASN1 encoder update for PKCS 7 encoder */
static void
sec_pkcs12_wrap_asn1_encoder_update(void *arg, const char *buf, 
				    unsigned long len)
{
    SEC_ASN1EncoderContext *cx;

    cx = (SEC_ASN1EncoderContext*)arg;
    SEC_ASN1EncoderUpdate(cx, buf, len);
}

/* pfx notify function for ASN1 encoder.  we want to stop encoding, once we reach
 * the authenticated safe.  at that point, the encoder will be updated via streaming
 * as the authenticated safe is  encoded. 
 */
static void
sec_pkcs12_encoder_pfx_notify(void *arg, PRBool before, void *dest, int real_depth)
{
    sec_PKCS12EncoderContext *p12ecx;

    if(!before) {
	return;
    }

    /* look for authenticated safe */
    p12ecx = (sec_PKCS12EncoderContext*)arg;
    if(dest != &p12ecx->pfx.encodedAuthSafe) {
	return;
    }

    SEC_ASN1EncoderSetTakeFromBuf(p12ecx->ecx);
    SEC_ASN1EncoderSetStreaming(p12ecx->ecx);
    SEC_ASN1EncoderClearNotifyProc(p12ecx->ecx);
}

/* SEC_PKCS12Encode
 *	Encodes the PFX item and returns it to the output function, via
 *	callback.  the output function must be capable of multiple updates.
 *	
 *	p12exp - the export context 
 *	output - the output function callback, will be called more than once,
 *		 must be able to accept streaming data.
 *	outputarg - argument for the output callback.
 */
SECStatus
SEC_PKCS12Encode(SEC_PKCS12ExportContext *p12exp, 
		 SEC_PKCS12EncoderOutputCallback output, void *outputarg)
{
    sec_PKCS12EncoderContext *p12enc;
    struct sec_pkcs12_encoder_output outInfo;
    SECStatus rv;

    if(!p12exp || !output) {
	return SECFailure;
    }

    /* get the encoder context */
    p12enc = sec_pkcs12_encoder_start_context(p12exp);
    if(!p12enc) {
	return SECFailure;
    }

    outInfo.outputfn = output;
    outInfo.outputarg = outputarg;

    /* set up PFX encoder.  Set it for streaming */
    p12enc->ecx = SEC_ASN1EncoderStart(&p12enc->pfx, sec_PKCS12PFXItemTemplate,
				       sec_pkcs12_encoder_out, 
				       &outInfo);
    if(!p12enc->ecx) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	rv = SECFailure;
	goto loser;
    }
    SEC_ASN1EncoderSetStreaming(p12enc->ecx);
    SEC_ASN1EncoderSetNotifyProc(p12enc->ecx, sec_pkcs12_encoder_pfx_notify, p12enc);
    rv = SEC_ASN1EncoderUpdate(p12enc->ecx, NULL, 0);
    if(rv != SECSuccess) {
	rv = SECFailure;
	goto loser;
    }

    /* set up asafe cinfo - the output of the encoder feeds the PFX encoder */
    p12enc->aSafeP7Ecx = SEC_PKCS7EncoderStart(p12enc->aSafeCinfo, 
    					       sec_pkcs12_wrap_asn1_encoder_update,
    					       p12enc->ecx, NULL);
    if(!p12enc->aSafeP7Ecx) {
	rv = SECFailure;
	goto loser;
    }

    /* encode asafe */
    if(p12enc->p12exp->integrityEnabled && p12enc->p12exp->pwdIntegrity) {
	p12enc->aSafeEcx = SEC_ASN1EncoderStart(&p12enc->p12exp->authSafe,
					sec_PKCS12AuthenticatedSafeTemplate, 
					sec_pkcs12_asafe_update_hmac_and_encode_bits,
    					p12enc);
    } else {
	p12enc->aSafeEcx = SEC_ASN1EncoderStart(&p12enc->p12exp->authSafe,
				sec_PKCS12AuthenticatedSafeTemplate,
				sec_pkcs12_wrap_pkcs7_encoder_update,
				p12enc->aSafeP7Ecx);
    }
    if(!p12enc->aSafeEcx) {
	rv = SECFailure;
	goto loser;
    }
    SEC_ASN1EncoderSetStreaming(p12enc->aSafeEcx);
    SEC_ASN1EncoderSetTakeFromBuf(p12enc->aSafeEcx); 
	
    /* encode each of the safes */			 
    while(p12enc->currentSafe != p12enc->p12exp->safeInfoCount) {
	sec_pkcs12_encoder_asafe_process(p12enc);
	p12enc->currentSafe++;
    }
    SEC_ASN1EncoderClearTakeFromBuf(p12enc->aSafeEcx);
    SEC_ASN1EncoderClearStreaming(p12enc->aSafeEcx);
    SEC_ASN1EncoderUpdate(p12enc->aSafeEcx, NULL, 0);
    SEC_ASN1EncoderFinish(p12enc->aSafeEcx);

    /* finish the encoding of the authenticated safes */
    rv = SEC_PKCS7EncoderFinish(p12enc->aSafeP7Ecx, p12exp->pwfn, 
    				p12exp->pwfnarg);
    if(rv != SECSuccess) {
	goto loser;
    }

    SEC_ASN1EncoderClearTakeFromBuf(p12enc->ecx);
    SEC_ASN1EncoderClearStreaming(p12enc->ecx);

    /* update the mac, if necessary */
    rv = sec_pkcs12_update_mac(p12enc);
    if(rv != SECSuccess) {
	goto loser;
    }
   
    /* finish encoding the pfx */ 
    rv = SEC_ASN1EncoderUpdate(p12enc->ecx, NULL, 0);

    SEC_ASN1EncoderFinish(p12enc->ecx);

loser:
    return rv;
}

void
SEC_PKCS12DestroyExportContext(SEC_PKCS12ExportContext *p12ecx)
{
    int i = 0;

    if(!p12ecx) {
	return;
    }

    if(p12ecx->safeInfos) {
	i = 0;
	while(p12ecx->safeInfos[i] != NULL) {
	    if(p12ecx->safeInfos[i]->encryptionKey) {
		PK11_FreeSymKey(p12ecx->safeInfos[i]->encryptionKey);
	    }
	    if(p12ecx->safeInfos[i]->cinfo) {
		SEC_PKCS7DestroyContentInfo(p12ecx->safeInfos[i]->cinfo);
	    }
	    i++;
	}
    }

    PORT_FreeArena(p12ecx->arena, PR_TRUE);
}


/*********************************
 * All-in-one routines for exporting certificates 
 *********************************/
struct inPlaceEncodeInfo {
    PRBool error;
    SECItem outItem;
};

static void 
sec_pkcs12_in_place_encoder_output(void *arg, const char *buf, unsigned long len)
{
    struct inPlaceEncodeInfo *outInfo = (struct inPlaceEncodeInfo*)arg;

    if(!outInfo || !len || outInfo->error) {
	return;
    }

    if(!outInfo->outItem.data) {
	outInfo->outItem.data = (unsigned char*)PORT_ZAlloc(len);
	outInfo->outItem.len = 0;
    } else {
	if(!PORT_Realloc(&(outInfo->outItem.data), (outInfo->outItem.len + len))) {
	    SECITEM_ZfreeItem(&(outInfo->outItem), PR_FALSE);
	    outInfo->outItem.data = NULL;
	    PORT_SetError(SEC_ERROR_NO_MEMORY);
	    outInfo->error = PR_TRUE;
	    return;
	}
    }

    PORT_Memcpy(&(outInfo->outItem.data[outInfo->outItem.len]), buf, len);
    outInfo->outItem.len += len;

    return;
}

/*
 * SEC_PKCS12ExportCertifcateAndKeyUsingPassword
 *	Exports a certificate/key pair using password-based encryption and
 *	authentication.
 *
 * pwfn, pwfnarg - password function and argument for the key database
 * cert - the certificate to export
 * certDb - certificate database
 * pwitem - the password to use
 * shroudKey - encrypt the key externally, 
 * keyShroudAlg - encryption algorithm for key
 * encryptionAlg - the algorithm with which data is encrypted
 * integrityAlg - the algorithm for integrity
 */
SECItem *
SEC_PKCS12ExportCertificateAndKeyUsingPassword(
				SECKEYGetPasswordKey pwfn, void *pwfnarg,
				CERTCertificate *cert, PK11SlotInfo *slot,
				CERTCertDBHandle *certDb, SECItem *pwitem,
				PRBool shroudKey, SECOidTag shroudAlg,
				PRBool encryptCert, SECOidTag certEncAlg,
				SECOidTag integrityAlg, void *wincx)
{
    struct inPlaceEncodeInfo outInfo;
    SEC_PKCS12ExportContext *p12ecx = NULL;
    SEC_PKCS12SafeInfo *keySafe, *certSafe;
    SECItem *returnItem = NULL;

    if(!cert || !pwitem || !slot) {
	return NULL;
    }

    outInfo.error = PR_FALSE;
    outInfo.outItem.data = NULL;
    outInfo.outItem.len = 0;

    p12ecx = SEC_PKCS12CreateExportContext(pwfn, pwfnarg, slot, wincx);
    if(!p12ecx) {
	return NULL;
    }

    /* set up cert safe */
    if(encryptCert) {
	certSafe = SEC_PKCS12CreatePasswordPrivSafe(p12ecx, pwitem, certEncAlg);
    } else {
	certSafe = SEC_PKCS12CreateUnencryptedSafe(p12ecx);
    }
    if(!certSafe) {
	goto loser;
    }

    /* set up key safe */
    if(shroudKey) {
	keySafe = SEC_PKCS12CreateUnencryptedSafe(p12ecx);
    } else {
	keySafe = certSafe;
    }
    if(!keySafe) {
	goto loser;
    }

    /* add integrity mode */
    if(SEC_PKCS12AddPasswordIntegrity(p12ecx, pwitem, integrityAlg) 
		!= SECSuccess) {
	goto loser;
    }

    /* add cert and key pair */
    if(SEC_PKCS12AddCertAndKey(p12ecx, certSafe, NULL, cert, certDb, 
			       keySafe, NULL, shroudKey, pwitem, shroudAlg)
		!= SECSuccess) {
	goto loser;
    }

    /* encode the puppy */
    if(SEC_PKCS12Encode(p12ecx, sec_pkcs12_in_place_encoder_output, &outInfo)
		!= SECSuccess) {
	goto loser;
    }
    if(outInfo.error) {
	goto loser;
    }

    SEC_PKCS12DestroyExportContext(p12ecx);
	
    returnItem = SECITEM_DupItem(&outInfo.outItem);
    SECITEM_ZfreeItem(&outInfo.outItem, PR_FALSE);

    return returnItem;

loser:
    if(outInfo.outItem.data) {
	SECITEM_ZfreeItem(&(outInfo.outItem), PR_TRUE);
    }

    if(p12ecx) {
	SEC_PKCS12DestroyExportContext(p12ecx);
    }

    return NULL;
}


