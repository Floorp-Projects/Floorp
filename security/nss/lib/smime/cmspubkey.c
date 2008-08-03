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
 * CMS public key crypto
 *
 * $Id: cmspubkey.c,v 1.7 2004/04/25 15:03:16 gerv%gerv.net Exp $
 */

#include "cmslocal.h"

#include "cert.h"
#include "key.h"
#include "secasn1.h"
#include "secitem.h"
#include "secoid.h"
#include "pk11func.h"
#include "secerr.h"

/* ====== RSA ======================================================================= */

/*
 * NSS_CMSUtil_EncryptSymKey_RSA - wrap a symmetric key with RSA
 *
 * this function takes a symmetric key and encrypts it using an RSA public key
 * according to PKCS#1 and RFC2633 (S/MIME)
 */
SECStatus
NSS_CMSUtil_EncryptSymKey_RSA(PLArenaPool *poolp, CERTCertificate *cert, 
                              PK11SymKey *bulkkey,
                              SECItem *encKey)
{
    SECStatus rv;
    SECKEYPublicKey *publickey;

    publickey = CERT_ExtractPublicKey(cert);
    if (publickey == NULL)
	return SECFailure;

    rv = NSS_CMSUtil_EncryptSymKey_RSAPubKey(poolp, publickey, bulkkey, encKey);
    SECKEY_DestroyPublicKey(publickey);
    return rv;
}

SECStatus
NSS_CMSUtil_EncryptSymKey_RSAPubKey(PLArenaPool *poolp, 
                                    SECKEYPublicKey *publickey, 
                                    PK11SymKey *bulkkey, SECItem *encKey)
{
    SECStatus rv;
    int data_len;
    KeyType keyType;
    void *mark = NULL;


    mark = PORT_ArenaMark(poolp);
    if (!mark)
	goto loser;

    /* sanity check */
    keyType = SECKEY_GetPublicKeyType(publickey);
    PORT_Assert(keyType == rsaKey);
    if (keyType != rsaKey) {
	goto loser;
    }
    /* allocate memory for the encrypted key */
    data_len = SECKEY_PublicKeyStrength(publickey);	/* block size (assumed to be > keylen) */
    encKey->data = (unsigned char*)PORT_ArenaAlloc(poolp, data_len);
    encKey->len = data_len;
    if (encKey->data == NULL)
	goto loser;

    /* encrypt the key now */
    rv = PK11_PubWrapSymKey(PK11_AlgtagToMechanism(SEC_OID_PKCS1_RSA_ENCRYPTION),
				publickey, bulkkey, encKey);

    if (rv != SECSuccess)
	goto loser;

    PORT_ArenaUnmark(poolp, mark);
    return SECSuccess;

loser:
    if (mark) {
	PORT_ArenaRelease(poolp, mark);
    }
    return SECFailure;
}

/*
 * NSS_CMSUtil_DecryptSymKey_RSA - unwrap a RSA-wrapped symmetric key
 *
 * this function takes an RSA-wrapped symmetric key and unwraps it, returning a symmetric
 * key handle. Please note that the actual unwrapped key data may not be allowed to leave
 * a hardware token...
 */
PK11SymKey *
NSS_CMSUtil_DecryptSymKey_RSA(SECKEYPrivateKey *privkey, SECItem *encKey, SECOidTag bulkalgtag)
{
    /* that's easy */
    CK_MECHANISM_TYPE target;
    PORT_Assert(bulkalgtag != SEC_OID_UNKNOWN);
    target = PK11_AlgtagToMechanism(bulkalgtag);
    if (bulkalgtag == SEC_OID_UNKNOWN || target == CKM_INVALID_MECHANISM) {
	PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
	return NULL;
    }
    return PK11_PubUnwrapSymKey(privkey, encKey, target, CKA_DECRYPT, 0);
}

/* ====== MISSI (Fortezza) ========================================================== */

extern const SEC_ASN1Template NSS_SMIMEKEAParamTemplateAllParams[];

SECStatus
NSS_CMSUtil_EncryptSymKey_MISSI(PLArenaPool *poolp, CERTCertificate *cert, PK11SymKey *bulkkey,
			SECOidTag symalgtag, SECItem *encKey, SECItem **pparams, void *pwfn_arg)
{
    SECOidTag certalgtag;	/* the certificate's encryption algorithm */
    SECOidTag encalgtag;	/* the algorithm used for key exchange/agreement */
    SECStatus rv = SECFailure;
    SECItem *params = NULL;
    SECStatus err;
    PK11SymKey *tek;
    CERTCertificate *ourCert;
    SECKEYPublicKey *ourPubKey, *publickey = NULL;
    SECKEYPrivateKey *ourPrivKey = NULL;
    NSSCMSKEATemplateSelector whichKEA = NSSCMSKEAInvalid;
    NSSCMSSMIMEKEAParameters keaParams;
    PLArenaPool *arena = NULL;
    extern const SEC_ASN1Template *nss_cms_get_kea_template(NSSCMSKEATemplateSelector whichTemplate);

    /* Clear keaParams, since cleanup code checks the lengths */
    (void) memset(&keaParams, 0, sizeof(keaParams));

    certalgtag = SECOID_GetAlgorithmTag(&(cert->subjectPublicKeyInfo.algorithm));
    PORT_Assert(certalgtag == SEC_OID_MISSI_KEA_DSS_OLD ||
		certalgtag == SEC_OID_MISSI_KEA_DSS ||
		certalgtag == SEC_OID_MISSI_KEA);

#define SMIME_FORTEZZA_RA_LENGTH 128
#define SMIME_FORTEZZA_IV_LENGTH 24
#define SMIME_FORTEZZA_MAX_KEY_SIZE 256

    /* We really want to show our KEA tag as the key exchange algorithm tag. */
    encalgtag = SEC_OID_NETSCAPE_SMIME_KEA;

    /* Get the public key of the recipient. */
    publickey = CERT_ExtractPublicKey(cert);
    if (publickey == NULL) goto loser;

    /* Find our own cert, and extract its keys. */
    ourCert = PK11_FindBestKEAMatch(cert, pwfn_arg);
    if (ourCert == NULL) goto loser;

    arena = PORT_NewArena(1024);
    if (arena == NULL)
	goto loser;

    ourPubKey = CERT_ExtractPublicKey(ourCert);
    if (ourPubKey == NULL) {
	CERT_DestroyCertificate(ourCert);
	goto loser;
    }

    /* While we're here, copy the public key into the outgoing
     * KEA parameters. */
    SECITEM_CopyItem(arena, &(keaParams.originatorKEAKey), &(ourPubKey->u.fortezza.KEAKey));
    SECKEY_DestroyPublicKey(ourPubKey);
    ourPubKey = NULL;

    /* Extract our private key in order to derive the KEA key. */
    ourPrivKey = PK11_FindKeyByAnyCert(ourCert, pwfn_arg);
    CERT_DestroyCertificate(ourCert); /* we're done with this */
    if (!ourPrivKey)
	goto loser;

    /* Prepare raItem with 128 bytes (filled with zeros). */
    keaParams.originatorRA.data = (unsigned char *)PORT_ArenaAlloc(arena,SMIME_FORTEZZA_RA_LENGTH);
    keaParams.originatorRA.len = SMIME_FORTEZZA_RA_LENGTH;

    /* Generate the TEK (token exchange key) which we use
     * to wrap the bulk encryption key. (keaparams.originatorRA) will be
     * filled with a random seed which we need to send to
     * the recipient. (user keying material in RFC2630/DSA speak) */
    tek = PK11_PubDerive(ourPrivKey, publickey, PR_TRUE,
			 &keaParams.originatorRA, NULL,
			 CKM_KEA_KEY_DERIVE, CKM_SKIPJACK_WRAP,
			 CKA_WRAP, 0,  pwfn_arg);

    SECKEY_DestroyPublicKey(publickey);
    SECKEY_DestroyPrivateKey(ourPrivKey);
    publickey = NULL;
    ourPrivKey = NULL;
    
    if (!tek)
	goto loser;

    /* allocate space for the wrapped key data */
    encKey->data = (unsigned char *)PORT_ArenaAlloc(poolp, SMIME_FORTEZZA_MAX_KEY_SIZE);
    encKey->len = SMIME_FORTEZZA_MAX_KEY_SIZE;

    if (encKey->data == NULL) {
	PK11_FreeSymKey(tek);
	goto loser;
    }

    /* Wrap the bulk key. What we do with the resulting data
       depends on whether we're using Skipjack to wrap the key. */
    switch (PK11_AlgtagToMechanism(symalgtag)) {
    case CKM_SKIPJACK_CBC64:
    case CKM_SKIPJACK_ECB64:
    case CKM_SKIPJACK_OFB64:
    case CKM_SKIPJACK_CFB64:
    case CKM_SKIPJACK_CFB32:
    case CKM_SKIPJACK_CFB16:
    case CKM_SKIPJACK_CFB8:
	/* SKIPJACK, we use the wrap mechanism because we can do it on the hardware */
	err = PK11_WrapSymKey(CKM_SKIPJACK_WRAP, NULL, tek, bulkkey, encKey);
	whichKEA = NSSCMSKEAUsesSkipjack;
	break;
    default:
	/* Not SKIPJACK, we encrypt the raw key data */
	keaParams.nonSkipjackIV.data = 
	  (unsigned char *)PORT_ArenaAlloc(arena, SMIME_FORTEZZA_IV_LENGTH);
	keaParams.nonSkipjackIV.len = SMIME_FORTEZZA_IV_LENGTH;
	err = PK11_WrapSymKey(CKM_SKIPJACK_CBC64, &keaParams.nonSkipjackIV, tek, bulkkey, encKey);
	if (err != SECSuccess)
	    goto loser;

	if (encKey->len != PK11_GetKeyLength(bulkkey)) {
	    /* The size of the encrypted key is not the same as
	       that of the original bulk key, presumably due to
	       padding. Encode and store the real size of the
	       bulk key. */
	    if (SEC_ASN1EncodeInteger(arena, &keaParams.bulkKeySize, PK11_GetKeyLength(bulkkey)) == NULL)
		err = (SECStatus)PORT_GetError();
	    else
		/* use full template for encoding */
		whichKEA = NSSCMSKEAUsesNonSkipjackWithPaddedEncKey;
	}
	else
	    /* enc key length == bulk key length */
	    whichKEA = NSSCMSKEAUsesNonSkipjack; 
	break;
    }

    PK11_FreeSymKey(tek);

    if (err != SECSuccess)
	goto loser;

    PORT_Assert(whichKEA != NSSCMSKEAInvalid);

    /* Encode the KEA parameters into the recipient info. */
    params = SEC_ASN1EncodeItem(poolp, NULL, &keaParams, nss_cms_get_kea_template(whichKEA));
    if (params == NULL)
	goto loser;

    /* pass back the algorithm params */
    *pparams = params;

    rv = SECSuccess;

loser:
    if (arena)
	PORT_FreeArena(arena, PR_FALSE);
    if (publickey)
        SECKEY_DestroyPublicKey(publickey);
    if (ourPrivKey)
        SECKEY_DestroyPrivateKey(ourPrivKey);
    return rv;
}

PK11SymKey *
NSS_CMSUtil_DecryptSymKey_MISSI(SECKEYPrivateKey *privkey, SECItem *encKey, SECAlgorithmID *keyEncAlg, SECOidTag bulkalgtag, void *pwfn_arg)
{
    /* fortezza: do a key exchange */
    SECStatus err;
    CK_MECHANISM_TYPE bulkType;
    PK11SymKey *tek;
    SECKEYPublicKey *originatorPubKey;
    NSSCMSSMIMEKEAParameters keaParams;
    PK11SymKey *bulkkey;
    int bulkLength;

    (void) memset(&keaParams, 0, sizeof(keaParams));

    /* NOTE: this uses the SMIME v2 recipientinfo for compatibility.
       All additional KEA parameters are DER-encoded in the encryption algorithm parameters */

    /* Decode the KEA algorithm parameters. */
    err = SEC_ASN1DecodeItem(NULL, &keaParams, NSS_SMIMEKEAParamTemplateAllParams,
			     &(keyEncAlg->parameters));
    if (err != SECSuccess)
	goto loser;

    /* get originator's public key */
   originatorPubKey = PK11_MakeKEAPubKey(keaParams.originatorKEAKey.data,
			   keaParams.originatorKEAKey.len);
   if (originatorPubKey == NULL)
	  goto loser;
    
   /* Generate the TEK (token exchange key) which we use to unwrap the bulk encryption key.
      The Derive function generates a shared secret and combines it with the originatorRA
      data to come up with an unique session key */
   tek = PK11_PubDerive(privkey, originatorPubKey, PR_FALSE,
			 &keaParams.originatorRA, NULL,
			 CKM_KEA_KEY_DERIVE, CKM_SKIPJACK_WRAP,
			 CKA_WRAP, 0, pwfn_arg);
   SECKEY_DestroyPublicKey(originatorPubKey);	/* not needed anymore */
   if (tek == NULL)
	goto loser;
    
    /* Now that we have the TEK, unwrap the bulk key
       with which to decrypt the message. We have to
       do one of two different things depending on 
       whether Skipjack was used for *bulk* encryption 
       of the message. */
    bulkType = PK11_AlgtagToMechanism(bulkalgtag);
    switch (bulkType) {
    case CKM_SKIPJACK_CBC64:
    case CKM_SKIPJACK_ECB64:
    case CKM_SKIPJACK_OFB64:
    case CKM_SKIPJACK_CFB64:
    case CKM_SKIPJACK_CFB32:
    case CKM_SKIPJACK_CFB16:
    case CKM_SKIPJACK_CFB8:
	/* Skipjack is being used as the bulk encryption algorithm.*/
	/* Unwrap the bulk key. */
	bulkkey = PK11_UnwrapSymKey(tek, CKM_SKIPJACK_WRAP, NULL,
				    encKey, CKM_SKIPJACK_CBC64, CKA_DECRYPT, 0);
	break;
    default:
	/* Skipjack was not used for bulk encryption of this
	   message. Use Skipjack CBC64, with the nonSkipjackIV
	   part of the KEA key parameters, to decrypt 
	   the bulk key. If the optional parameter bulkKeySize is present,
	   bulk key size is different than the encrypted key size */
	if (keaParams.bulkKeySize.len > 0) {
	    err = SEC_ASN1DecodeItem(NULL, &bulkLength,
				     SEC_ASN1_GET(SEC_IntegerTemplate),
				     &keaParams.bulkKeySize);
	    if (err != SECSuccess)
		goto loser;
	}
	
	bulkkey = PK11_UnwrapSymKey(tek, CKM_SKIPJACK_CBC64, &keaParams.nonSkipjackIV, 
				    encKey, bulkType, CKA_DECRYPT, bulkLength);
	break;
    }
    return bulkkey;
loser:
    return NULL;
}

/* ====== ESDH (Ephemeral-Static Diffie-Hellman) ==================================== */

SECStatus
NSS_CMSUtil_EncryptSymKey_ESDH(PLArenaPool *poolp, CERTCertificate *cert, PK11SymKey *key,
			SECItem *encKey, SECItem **ukm, SECAlgorithmID *keyEncAlg,
			SECItem *pubKey)
{
#if 0 /* not yet done */
    SECOidTag certalgtag;	/* the certificate's encryption algorithm */
    SECOidTag encalgtag;	/* the algorithm used for key exchange/agreement */
    SECStatus rv;
    SECItem *params = NULL;
    int data_len;
    SECStatus err;
    PK11SymKey *tek;
    CERTCertificate *ourCert;
    SECKEYPublicKey *ourPubKey;
    NSSCMSKEATemplateSelector whichKEA = NSSCMSKEAInvalid;

    certalgtag = SECOID_GetAlgorithmTag(&(cert->subjectPublicKeyInfo.algorithm));
    PORT_Assert(certalgtag == SEC_OID_X942_DIFFIE_HELMAN_KEY);

    /* We really want to show our KEA tag as the key exchange algorithm tag. */
    encalgtag = SEC_OID_CMS_EPHEMERAL_STATIC_DIFFIE_HELLMAN;

    /* Get the public key of the recipient. */
    publickey = CERT_ExtractPublicKey(cert);
    if (publickey == NULL) goto loser;

    /* XXXX generate a DH key pair on a PKCS11 module (XXX which parameters?) */
    /* XXXX */ourCert = PK11_FindBestKEAMatch(cert, wincx);
    if (ourCert == NULL) goto loser;

    arena = PORT_NewArena(1024);
    if (arena == NULL) goto loser;

    /* While we're here, extract the key pair's public key data and copy it into */
    /* the outgoing parameters. */
    /* XXXX */ourPubKey = CERT_ExtractPublicKey(ourCert);
    if (ourPubKey == NULL)
    {
	goto loser;
    }
    SECITEM_CopyItem(arena, pubKey, /* XXX */&(ourPubKey->u.fortezza.KEAKey));
    SECKEY_DestroyPublicKey(ourPubKey); /* we only need the private key from now on */
    ourPubKey = NULL;

    /* Extract our private key in order to derive the KEA key. */
    ourPrivKey = PK11_FindKeyByAnyCert(ourCert,wincx);
    CERT_DestroyCertificate(ourCert); /* we're done with this */
    if (!ourPrivKey) goto loser;

    /* If ukm desired, prepare it - allocate enough space (filled with zeros). */
    if (ukm) {
	ukm->data = (unsigned char*)PORT_ArenaZAlloc(arena,/* XXXX */);
	ukm->len = /* XXXX */;
    }

    /* Generate the KEK (key exchange key) according to RFC2631 which we use
     * to wrap the bulk encryption key. */
    kek = PK11_PubDerive(ourPrivKey, publickey, PR_TRUE,
			 ukm, NULL,
			 /* XXXX */CKM_KEA_KEY_DERIVE, /* XXXX */CKM_SKIPJACK_WRAP,
			 CKA_WRAP, 0, wincx);

    SECKEY_DestroyPublicKey(publickey);
    SECKEY_DestroyPrivateKey(ourPrivKey);
    publickey = NULL;
    ourPrivKey = NULL;
    
    if (!kek)
	goto loser;

    /* allocate space for the encrypted CEK (bulk key) */
    encKey->data = (unsigned char*)PORT_ArenaAlloc(poolp, SMIME_FORTEZZA_MAX_KEY_SIZE);
    encKey->len = SMIME_FORTEZZA_MAX_KEY_SIZE;

    if (encKey->data == NULL)
    {
	PK11_FreeSymKey(kek);
	goto loser;
    }


    /* Wrap the bulk key using CMSRC2WRAP or CMS3DESWRAP, depending on the */
    /* bulk encryption algorithm */
    switch (/* XXXX */PK11_AlgtagToMechanism(enccinfo->encalg))
    {
    case /* XXXX */CKM_SKIPJACK_CFB8:
	err = PK11_WrapSymKey(/* XXXX */CKM_CMS3DES_WRAP, NULL, kek, bulkkey, encKey);
	whichKEA = NSSCMSKEAUsesSkipjack;
	break;
    case /* XXXX */CKM_SKIPJACK_CFB8:
	err = PK11_WrapSymKey(/* XXXX */CKM_CMSRC2_WRAP, NULL, kek, bulkkey, encKey);
	whichKEA = NSSCMSKEAUsesSkipjack;
	break;
    default:
	/* XXXX what do we do here? Neither RC2 nor 3DES... */
        err = SECFailure;
        /* set error */
	break;
    }

    PK11_FreeSymKey(kek);	/* we do not need the KEK anymore */
    if (err != SECSuccess)
	goto loser;

    PORT_Assert(whichKEA != NSSCMSKEAInvalid);

    /* see RFC2630 12.3.1.1 "keyEncryptionAlgorithm must be ..." */
    /* params is the DER encoded key wrap algorithm (with parameters!) (XXX) */
    params = SEC_ASN1EncodeItem(arena, NULL, &keaParams, sec_pkcs7_get_kea_template(whichKEA));
    if (params == NULL)
	goto loser;

    /* now set keyEncAlg */
    rv = SECOID_SetAlgorithmID(poolp, keyEncAlg, SEC_OID_CMS_EPHEMERAL_STATIC_DIFFIE_HELLMAN, params);
    if (rv != SECSuccess)
	goto loser;

    /* XXXXXXX this is not right yet */
loser:
    if (arena) {
	PORT_FreeArena(arena, PR_FALSE);
    }
    if (publickey) {
        SECKEY_DestroyPublicKey(publickey);
    }
    if (ourPrivKey) {
        SECKEY_DestroyPrivateKey(ourPrivKey);
    }
#endif
    return SECFailure;
}

PK11SymKey *
NSS_CMSUtil_DecryptSymKey_ESDH(SECKEYPrivateKey *privkey, SECItem *encKey, SECAlgorithmID *keyEncAlg, SECOidTag bulkalgtag, void *pwfn_arg)
{
#if 0 /* not yet done */
    SECStatus err;
    CK_MECHANISM_TYPE bulkType;
    PK11SymKey *tek;
    SECKEYPublicKey *originatorPubKey;
    NSSCMSSMIMEKEAParameters keaParams;

   /* XXXX get originator's public key */
   originatorPubKey = PK11_MakeKEAPubKey(keaParams.originatorKEAKey.data,
			   keaParams.originatorKEAKey.len);
   if (originatorPubKey == NULL)
      goto loser;
    
   /* Generate the TEK (token exchange key) which we use to unwrap the bulk encryption key.
      The Derive function generates a shared secret and combines it with the originatorRA
      data to come up with an unique session key */
   tek = PK11_PubDerive(privkey, originatorPubKey, PR_FALSE,
			 &keaParams.originatorRA, NULL,
			 CKM_KEA_KEY_DERIVE, CKM_SKIPJACK_WRAP,
			 CKA_WRAP, 0, pwfn_arg);
   SECKEY_DestroyPublicKey(originatorPubKey);	/* not needed anymore */
   if (tek == NULL)
	goto loser;
    
    /* Now that we have the TEK, unwrap the bulk key
       with which to decrypt the message. */
    /* Skipjack is being used as the bulk encryption algorithm.*/
    /* Unwrap the bulk key. */
    bulkkey = PK11_UnwrapSymKey(tek, CKM_SKIPJACK_WRAP, NULL,
				encKey, CKM_SKIPJACK_CBC64, CKA_DECRYPT, 0);

    return bulkkey;

loser:
#endif
    return NULL;
}

