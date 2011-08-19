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
#include "secitem.h"
#include "pkcs11.h"
#include "lgdb.h" 
#include "pcert.h"
#include "lowkeyi.h"
#include "blapi.h"
#include "secder.h"
#include "secasn1.h"

#include "keydbi.h" 

/*
 * ******************** Object Creation Utilities ***************************
 */

/*
 * check the consistancy and initialize a Certificate Object 
 */
static CK_RV
lg_createCertObject(SDB *sdb, CK_OBJECT_HANDLE *handle,
			const CK_ATTRIBUTE *templ, CK_ULONG count)
{
    SECItem derCert;
    NSSLOWCERTCertificate *cert;
    NSSLOWCERTCertTrust *trust = NULL;
    NSSLOWCERTCertTrust userTrust = 
		{ CERTDB_USER, CERTDB_USER, CERTDB_USER };
    NSSLOWCERTCertTrust defTrust = 
		{ CERTDB_TRUSTED_UNKNOWN, 
			CERTDB_TRUSTED_UNKNOWN, CERTDB_TRUSTED_UNKNOWN };
    char *label = NULL;
    char *email = NULL;
    SECStatus rv;
    CK_RV crv;
    PRBool inDB = PR_TRUE;
    NSSLOWCERTCertDBHandle *certHandle = lg_getCertDB(sdb);
    NSSLOWKEYDBHandle *keyHandle = NULL;
    CK_CERTIFICATE_TYPE type;
    const CK_ATTRIBUTE *attribute;

    /* we can't store any certs private */
    if (lg_isTrue(CKA_PRIVATE, templ, count)) {
	return CKR_ATTRIBUTE_VALUE_INVALID;
    }
	
    /* We only support X.509 Certs for now */
    crv = lg_GetULongAttribute(CKA_CERTIFICATE_TYPE, templ, count, &type);
    if (crv != CKR_OK) {
	return crv;
    }

    if (type != CKC_X_509) {
	return CKR_ATTRIBUTE_VALUE_INVALID;
    }

    /* X.509 Certificate */
    

    if (certHandle == NULL) {
	return CKR_TOKEN_WRITE_PROTECTED;
    }

    /* get the der cert */ 
    attribute = lg_FindAttribute(CKA_VALUE, templ, count);
    if (!attribute) {
	return CKR_ATTRIBUTE_VALUE_INVALID;
    }

    derCert.type = 0;
    derCert.data = (unsigned char *)attribute->pValue;
    derCert.len = attribute->ulValueLen ;

    label = lg_getString(CKA_LABEL, templ, count);

    cert =  nsslowcert_FindCertByDERCert(certHandle, &derCert);
    if (cert == NULL) {
	cert = nsslowcert_DecodeDERCertificate(&derCert, label);
	inDB = PR_FALSE;
    }
    if (cert == NULL) {
	if (label) PORT_Free(label);
	return CKR_ATTRIBUTE_VALUE_INVALID;
    }

    keyHandle = lg_getKeyDB(sdb);
    if (keyHandle) {
	if (nsslowkey_KeyForCertExists(keyHandle,cert)) {
	    trust = &userTrust;
	}
    }

    if (!inDB) {
	if (!trust) trust = &defTrust;
	rv = nsslowcert_AddPermCert(certHandle, cert, label, trust);
    } else {
	rv = trust ? nsslowcert_ChangeCertTrust(certHandle,cert,trust) :
				SECSuccess;
    }

    if (label) PORT_Free(label);

    if (rv != SECSuccess) {
	nsslowcert_DestroyCertificate(cert);
	return CKR_DEVICE_ERROR;
    }

    /*
     * Add a NULL S/MIME profile if necessary.
     */
    email = lg_getString(CKA_NSS_EMAIL, templ, count);
    if (email) {
	certDBEntrySMime *entry;

	entry = nsslowcert_ReadDBSMimeEntry(certHandle,email);
	if (!entry) {
	    nsslowcert_SaveSMimeProfile(certHandle, email, 
						&cert->derSubject, NULL, NULL);
	} else {
	    nsslowcert_DestroyDBEntry((certDBEntry *)entry);
	}
	PORT_Free(email);
    }
    *handle=lg_mkHandle(sdb,&cert->certKey,LG_TOKEN_TYPE_CERT);
    nsslowcert_DestroyCertificate(cert);

    return CKR_OK;
}

unsigned int
lg_MapTrust(CK_TRUST trust, PRBool clientAuth)
{
    unsigned int trustCA = clientAuth ? CERTDB_TRUSTED_CLIENT_CA :
							CERTDB_TRUSTED_CA;
    switch (trust) {
    case CKT_NSS_TRUSTED:
	return CERTDB_TERMINAL_RECORD|CERTDB_TRUSTED;
    case CKT_NSS_TRUSTED_DELEGATOR:
	return CERTDB_VALID_CA|trustCA;
    case CKT_NSS_MUST_VERIFY_TRUST:
	return CERTDB_MUST_VERIFY;
    case CKT_NSS_NOT_TRUSTED:
	return CERTDB_TERMINAL_RECORD;
    case CKT_NSS_VALID_DELEGATOR: /* implies must verify */
	return CERTDB_VALID_CA;
    default:
	break;
    }
    return CERTDB_TRUSTED_UNKNOWN;
}
    
	
/*
 * check the consistancy and initialize a Trust Object 
 */
static CK_RV
lg_createTrustObject(SDB *sdb, CK_OBJECT_HANDLE *handle,
			const CK_ATTRIBUTE *templ, CK_ULONG count)
{
    const CK_ATTRIBUTE *issuer = NULL;
    const CK_ATTRIBUTE *serial = NULL;
    NSSLOWCERTCertificate *cert = NULL;
    const CK_ATTRIBUTE *trust;
    CK_TRUST sslTrust = CKT_NSS_TRUST_UNKNOWN;
    CK_TRUST clientTrust = CKT_NSS_TRUST_UNKNOWN;
    CK_TRUST emailTrust = CKT_NSS_TRUST_UNKNOWN;
    CK_TRUST signTrust = CKT_NSS_TRUST_UNKNOWN;
    CK_BBOOL stepUp;
    NSSLOWCERTCertTrust dbTrust = { 0 };
    SECStatus rv;
    NSSLOWCERTCertDBHandle *certHandle = lg_getCertDB(sdb);
    NSSLOWCERTIssuerAndSN issuerSN;

    /* we can't store any certs private */
    if (lg_isTrue(CKA_PRIVATE, templ, count)) {
	return CKR_ATTRIBUTE_VALUE_INVALID;
    }

    if (certHandle == NULL) {
	return CKR_TOKEN_WRITE_PROTECTED;
    }

    issuer = lg_FindAttribute(CKA_ISSUER, templ, count);
    serial = lg_FindAttribute(CKA_SERIAL_NUMBER, templ, count);

    if (issuer && serial) {
	issuerSN.derIssuer.data = (unsigned char *)issuer->pValue;
	issuerSN.derIssuer.len = issuer->ulValueLen ;

	issuerSN.serialNumber.data = (unsigned char *)serial->pValue;
	issuerSN.serialNumber.len = serial->ulValueLen ;

	cert = nsslowcert_FindCertByIssuerAndSN(certHandle,&issuerSN);
    }

    if (cert == NULL) {
	return CKR_ATTRIBUTE_VALUE_INVALID;
    }
	
    lg_GetULongAttribute(CKA_TRUST_SERVER_AUTH, templ, count, &sslTrust);
    lg_GetULongAttribute(CKA_TRUST_CLIENT_AUTH, templ, count, &clientTrust);
    lg_GetULongAttribute(CKA_TRUST_EMAIL_PROTECTION, templ, count, &emailTrust);
    lg_GetULongAttribute(CKA_TRUST_CODE_SIGNING, templ, count, &signTrust);
    stepUp = CK_FALSE;
    trust = lg_FindAttribute(CKA_TRUST_STEP_UP_APPROVED, templ, count);
    if (trust) {
	if (trust->ulValueLen == sizeof(CK_BBOOL)) {
	    stepUp = *(CK_BBOOL*)trust->pValue;
	}
    }

    /* preserve certain old fields */
    if (cert->trust) {
	dbTrust.sslFlags = cert->trust->sslFlags & CERTDB_PRESERVE_TRUST_BITS;
	dbTrust.emailFlags=
			cert->trust->emailFlags & CERTDB_PRESERVE_TRUST_BITS;
	dbTrust.objectSigningFlags = 
		cert->trust->objectSigningFlags & CERTDB_PRESERVE_TRUST_BITS;
    }

    dbTrust.sslFlags |= lg_MapTrust(sslTrust,PR_FALSE);
    dbTrust.sslFlags |= lg_MapTrust(clientTrust,PR_TRUE);
    dbTrust.emailFlags |= lg_MapTrust(emailTrust,PR_FALSE);
    dbTrust.objectSigningFlags |= lg_MapTrust(signTrust,PR_FALSE);
    if (stepUp) {
	dbTrust.sslFlags |= CERTDB_GOVT_APPROVED_CA;
    }

    rv = nsslowcert_ChangeCertTrust(certHandle,cert,&dbTrust);
    *handle=lg_mkHandle(sdb,&cert->certKey,LG_TOKEN_TYPE_TRUST);
    nsslowcert_DestroyCertificate(cert);
    if (rv != SECSuccess) {
	return CKR_DEVICE_ERROR;
    }

    return CKR_OK;
}

/*
 * check the consistancy and initialize a Trust Object 
 */
static CK_RV
lg_createSMimeObject(SDB *sdb, CK_OBJECT_HANDLE *handle,
			const CK_ATTRIBUTE *templ, CK_ULONG count)
{
    SECItem derSubj,rawProfile,rawTime,emailKey;
    SECItem *pRawProfile = NULL;
    SECItem *pRawTime = NULL;
    char *email = NULL;
    const CK_ATTRIBUTE *subject = NULL,
		 *profile = NULL,
		 *time    = NULL;
    SECStatus rv;
    NSSLOWCERTCertDBHandle *certHandle;
    CK_RV ck_rv = CKR_OK;

    /* we can't store any certs private */
    if (lg_isTrue(CKA_PRIVATE,templ,count)) {
	return CKR_ATTRIBUTE_VALUE_INVALID;
    }

    certHandle = lg_getCertDB(sdb);
    if (certHandle == NULL) {
	return CKR_TOKEN_WRITE_PROTECTED;
    }

    /* lookup SUBJECT */
    subject = lg_FindAttribute(CKA_SUBJECT,templ,count);
    PORT_Assert(subject);
    if (!subject) {
	ck_rv = CKR_ATTRIBUTE_VALUE_INVALID;
	goto loser;
    }

    derSubj.data = (unsigned char *)subject->pValue;
    derSubj.len = subject->ulValueLen ;
    derSubj.type = 0;

    /* lookup VALUE */
    profile = lg_FindAttribute(CKA_VALUE,templ,count);
    if (profile) {
	rawProfile.data = (unsigned char *)profile->pValue;
	rawProfile.len = profile->ulValueLen ;
	rawProfile.type = siBuffer;
	pRawProfile = &rawProfile;
    }

    /* lookup Time */
    time = lg_FindAttribute(CKA_NSS_SMIME_TIMESTAMP,templ,count);
    if (time) {
	rawTime.data = (unsigned char *)time->pValue;
	rawTime.len = time->ulValueLen ;
	rawTime.type = siBuffer;
	pRawTime = &rawTime;
    }


    email = lg_getString(CKA_NSS_EMAIL,templ,count);
    if (!email) {
	ck_rv = CKR_ATTRIBUTE_VALUE_INVALID;
	goto loser;
    }

    /* Store S/MIME Profile by SUBJECT */
    rv = nsslowcert_SaveSMimeProfile(certHandle, email, &derSubj, 
				pRawProfile,pRawTime);
    if (rv != SECSuccess) {
	ck_rv = CKR_DEVICE_ERROR;
	goto loser;
    }
    emailKey.data = (unsigned char *)email;
    emailKey.len = PORT_Strlen(email)+1;

    *handle = lg_mkHandle(sdb, &emailKey, LG_TOKEN_TYPE_SMIME);

loser:
    if (email)   PORT_Free(email);

    return ck_rv;
}

/*
 * check the consistancy and initialize a Trust Object 
 */
static CK_RV
lg_createCrlObject(SDB *sdb, CK_OBJECT_HANDLE *handle,
			const CK_ATTRIBUTE *templ, CK_ULONG count)
{
    PRBool isKRL = PR_FALSE;
    SECItem derSubj,derCrl;
    char *url = NULL;
    const CK_ATTRIBUTE *subject,*crl;
    SECStatus rv;
    NSSLOWCERTCertDBHandle *certHandle;

    certHandle = lg_getCertDB(sdb);

    /* we can't store any private crls */
    if (lg_isTrue(CKA_PRIVATE,templ,count)) {
	return CKR_ATTRIBUTE_VALUE_INVALID;
    }

    if (certHandle == NULL) {
	return CKR_TOKEN_WRITE_PROTECTED;
    }

    /* lookup SUBJECT */
    subject = lg_FindAttribute(CKA_SUBJECT,templ,count);
    if (!subject) {
	 return CKR_ATTRIBUTE_VALUE_INVALID;
    }

    derSubj.data = (unsigned char *)subject->pValue;
    derSubj.len = subject->ulValueLen ;

    /* lookup VALUE */
    crl = lg_FindAttribute(CKA_VALUE,templ,count);
    PORT_Assert(crl);
    if (!crl) {
	 return CKR_ATTRIBUTE_VALUE_INVALID;
    }
    derCrl.data = (unsigned char *)crl->pValue;
    derCrl.len = crl->ulValueLen ;

    url = lg_getString(CKA_NSS_URL,templ,count);
    isKRL = lg_isTrue(CKA_NSS_KRL,templ,count);

    /* Store CRL by SUBJECT */
    rv = nsslowcert_AddCrl(certHandle, &derCrl, &derSubj, url, isKRL);

    if (url) {
	PORT_Free(url);
    }
    if (rv != SECSuccess) {
	return CKR_DEVICE_ERROR;
    }

    /* if we overwrote the existing CRL, poison the handle entry so we get
     * a new object handle */
    (void) lg_poisonHandle(sdb, &derSubj,
			isKRL ? LG_TOKEN_KRL_HANDLE : LG_TOKEN_TYPE_CRL);
    *handle = lg_mkHandle(sdb, &derSubj,
			isKRL ? LG_TOKEN_KRL_HANDLE : LG_TOKEN_TYPE_CRL);

    return CKR_OK;
}

/*
 * check the consistancy and initialize a Public Key Object 
 */
static CK_RV
lg_createPublicKeyObject(SDB *sdb, CK_KEY_TYPE key_type,
     CK_OBJECT_HANDLE *handle, const CK_ATTRIBUTE *templ, CK_ULONG count)
{
    CK_ATTRIBUTE_TYPE pubKeyAttr = CKA_VALUE;
    CK_RV crv = CKR_OK;
    NSSLOWKEYPrivateKey *priv;
    SECItem pubKeySpace = {siBuffer, NULL, 0};
    SECItem *pubKey;
#ifdef NSS_ENABLE_ECC
    SECItem pubKey2Space = {siBuffer, NULL, 0};
    PRArenaPool *arena = NULL;
#endif /* NSS_ENABLE_ECC */
    NSSLOWKEYDBHandle *keyHandle = NULL;
	

    switch (key_type) {
    case CKK_RSA:
	pubKeyAttr = CKA_MODULUS;
	break;
#ifdef NSS_ENABLE_ECC
    case CKK_EC:
	pubKeyAttr = CKA_EC_POINT;
	break;
#endif /* NSS_ENABLE_ECC */
    case CKK_DSA:
    case CKK_DH:
	break;
    default:
	return CKR_ATTRIBUTE_VALUE_INVALID;
    }


    pubKey = &pubKeySpace;
    crv = lg_Attribute2SSecItem(NULL,pubKeyAttr,templ,count,pubKey);
    if (crv != CKR_OK) return crv;

#ifdef NSS_ENABLE_ECC
    if (key_type == CKK_EC) {
	SECStatus rv;
	/*
	 * for ECC, use the decoded key first.
	 */
	arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
	if (arena == NULL) {
	    crv = CKR_HOST_MEMORY;
	    goto done;
	}
	rv= SEC_QuickDERDecodeItem(arena, &pubKey2Space, 
				   SEC_ASN1_GET(SEC_OctetStringTemplate), 
				   pubKey);
	if (rv != SECSuccess) {
	    /* decode didn't work, just try the pubKey */
	    PORT_FreeArena(arena, PR_FALSE);
	    arena = NULL;
	} else {
	    /* try the decoded pub key first */
	    pubKey = &pubKey2Space;
	}
    }
#endif /* NSS_ENABLE_ECC */

    PORT_Assert(pubKey->data);
    if (pubKey->data == NULL) {
	crv = CKR_ATTRIBUTE_VALUE_INVALID;
	goto done;
    }
    keyHandle = lg_getKeyDB(sdb);
    if (keyHandle == NULL) {
	crv = CKR_TOKEN_WRITE_PROTECTED;
	goto done;
    }
    if (keyHandle->version != 3) {
	unsigned char buf[SHA1_LENGTH];
	SHA1_HashBuf(buf,pubKey->data,pubKey->len);
	PORT_Memcpy(pubKey->data,buf,sizeof(buf));
	pubKey->len = sizeof(buf);
    }
    /* make sure the associated private key already exists */
    /* only works if we are logged in */
    priv = nsslowkey_FindKeyByPublicKey(keyHandle, pubKey, sdb /*password*/);
#ifdef NSS_ENABLE_ECC
    if (priv == NULL && pubKey == &pubKey2Space) {
	/* no match on the decoded key, match the original pubkey */
	pubKey = &pubKeySpace;
    	priv = nsslowkey_FindKeyByPublicKey(keyHandle, pubKey, 
					    sdb /*password*/);
    }
#endif
    if (priv == NULL) {
	/* the legacy database can only 'store' public keys which already
	 * have their corresponding private keys in the database */
	crv = CKR_ATTRIBUTE_VALUE_INVALID;
	goto done;
    }
    lg_nsslowkey_DestroyPrivateKey(priv);
    crv = CKR_OK;

    *handle = lg_mkHandle(sdb, pubKey, LG_TOKEN_TYPE_PUB);

done:
    PORT_Free(pubKeySpace.data);
#ifdef NSS_ENABLE_ECC
    if (arena) 
	PORT_FreeArena(arena, PR_FALSE);
#endif

    return crv;
}

/* make a private key from a verified object */
static NSSLOWKEYPrivateKey *
lg_mkPrivKey(SDB *sdb, const CK_ATTRIBUTE *templ, CK_ULONG count,
	     CK_KEY_TYPE key_type, CK_RV *crvp)
{
    NSSLOWKEYPrivateKey *privKey;
    PLArenaPool *arena;
    CK_RV crv = CKR_OK;
    SECStatus rv;

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL) {
	*crvp = CKR_HOST_MEMORY;
	return NULL;
    }

    privKey = (NSSLOWKEYPrivateKey *)
			PORT_ArenaZAlloc(arena,sizeof(NSSLOWKEYPrivateKey));
    if (privKey == NULL)  {
	PORT_FreeArena(arena,PR_FALSE);
	*crvp = CKR_HOST_MEMORY;
	return NULL;
    }

    /* in future this would be a switch on key_type */
    privKey->arena = arena;
    switch (key_type) {
    case CKK_RSA:
	privKey->keyType = NSSLOWKEYRSAKey;
	crv=lg_Attribute2SSecItem(arena,CKA_MODULUS,templ,count,
					&privKey->u.rsa.modulus);
	if (crv != CKR_OK) break;
	crv=lg_Attribute2SSecItem(arena,CKA_PUBLIC_EXPONENT,templ,count,
					&privKey->u.rsa.publicExponent);
	if (crv != CKR_OK) break;
	crv=lg_PrivAttr2SSecItem(arena,CKA_PRIVATE_EXPONENT,templ,count,
				&privKey->u.rsa.privateExponent, sdb);
	if (crv != CKR_OK) break;
	crv=lg_PrivAttr2SSecItem(arena,CKA_PRIME_1,templ,count,
					&privKey->u.rsa.prime1, sdb);
	if (crv != CKR_OK) break;
	crv=lg_PrivAttr2SSecItem(arena,CKA_PRIME_2,templ,count,
					&privKey->u.rsa.prime2, sdb);
	if (crv != CKR_OK) break;
	crv=lg_PrivAttr2SSecItem(arena,CKA_EXPONENT_1,templ,count,
					&privKey->u.rsa.exponent1, sdb);
	if (crv != CKR_OK) break;
	crv=lg_PrivAttr2SSecItem(arena,CKA_EXPONENT_2,templ,count,
					&privKey->u.rsa.exponent2, sdb);
	if (crv != CKR_OK) break;
	crv=lg_PrivAttr2SSecItem(arena,CKA_COEFFICIENT,templ,count,
					&privKey->u.rsa.coefficient, sdb);
	if (crv != CKR_OK) break;
        rv = DER_SetUInteger(privKey->arena, &privKey->u.rsa.version,
                          NSSLOWKEY_VERSION);
	if (rv != SECSuccess) crv = CKR_HOST_MEMORY;
	break;

    case CKK_DSA:
	privKey->keyType = NSSLOWKEYDSAKey;
	crv = lg_Attribute2SSecItem(arena,CKA_PRIME,templ,count,
					&privKey->u.dsa.params.prime);
    	if (crv != CKR_OK) break;
	crv = lg_Attribute2SSecItem(arena,CKA_SUBPRIME,templ,count,
					&privKey->u.dsa.params.subPrime);
    	if (crv != CKR_OK) break;
	crv = lg_Attribute2SSecItem(arena,CKA_BASE,templ,count,
					&privKey->u.dsa.params.base);
    	if (crv != CKR_OK) break;
    	crv = lg_PrivAttr2SSecItem(arena,CKA_VALUE,templ,count,
					&privKey->u.dsa.privateValue, sdb);
    	if (crv != CKR_OK) break;
	if (lg_hasAttribute(CKA_NETSCAPE_DB, templ,count)) {
	    crv = lg_Attribute2SSecItem(arena, CKA_NETSCAPE_DB,templ,count,
						&privKey->u.dsa.publicValue);
	    /* privKey was zero'd so public value is already set to NULL, 0
	     * if we don't set it explicitly */
	}
	break;

    case CKK_DH:
	privKey->keyType = NSSLOWKEYDHKey;
	crv = lg_Attribute2SSecItem(arena,CKA_PRIME,templ,count,
					&privKey->u.dh.prime);
    	if (crv != CKR_OK) break;
	crv = lg_Attribute2SSecItem(arena,CKA_BASE,templ,count,
					&privKey->u.dh.base);
    	if (crv != CKR_OK) break;
    	crv = lg_PrivAttr2SSecItem(arena,CKA_VALUE,templ,count,
					&privKey->u.dh.privateValue, sdb);
    	if (crv != CKR_OK) break;
	if (lg_hasAttribute(CKA_NETSCAPE_DB, templ, count)) {
	    crv = lg_Attribute2SSecItem(arena, CKA_NETSCAPE_DB,templ,count,
					&privKey->u.dh.publicValue);
	    /* privKey was zero'd so public value is already set to NULL, 0
	     * if we don't set it explicitly */
	}
	break;

#ifdef NSS_ENABLE_ECC
    case CKK_EC:
	privKey->keyType = NSSLOWKEYECKey;
	crv = lg_Attribute2SSecItem(arena, CKA_EC_PARAMS,templ,count,
	                              &privKey->u.ec.ecParams.DEREncoding);
    	if (crv != CKR_OK) break;

	/* Fill out the rest of the ecParams structure
	 * based on the encoded params
	 */
	if (LGEC_FillParams(arena, &privKey->u.ec.ecParams.DEREncoding,
		    &privKey->u.ec.ecParams) != SECSuccess) {
	    crv = CKR_DOMAIN_PARAMS_INVALID;
	    break;
	}
	crv = lg_PrivAttr2SSecItem(arena,CKA_VALUE,templ,count,
					&privKey->u.ec.privateValue, sdb);
	if (crv != CKR_OK) break;
	if (lg_hasAttribute(CKA_NETSCAPE_DB,templ,count)) {
	    crv = lg_Attribute2SSecItem(arena, CKA_NETSCAPE_DB,templ,count,
					&privKey->u.ec.publicValue);
	    if (crv != CKR_OK) break;
	    /* privKey was zero'd so public value is already set to NULL, 0
	     * if we don't set it explicitly */
	}
        rv = DER_SetUInteger(privKey->arena, &privKey->u.ec.version,
                          NSSLOWKEY_EC_PRIVATE_KEY_VERSION);
	if (rv != SECSuccess) crv = CKR_HOST_MEMORY;
	break;
#endif /* NSS_ENABLE_ECC */

    default:
	crv = CKR_KEY_TYPE_INCONSISTENT;
	break;
    }
    *crvp = crv;
    if (crv != CKR_OK) {
	PORT_FreeArena(arena,PR_FALSE);
	return NULL;
    }
    return privKey;
}

/*
 * check the consistancy and initialize a Private Key Object 
 */
static CK_RV
lg_createPrivateKeyObject(SDB *sdb, CK_KEY_TYPE key_type,
     CK_OBJECT_HANDLE *handle, const CK_ATTRIBUTE *templ, CK_ULONG count)
{
    NSSLOWKEYPrivateKey *privKey;
    char *label;
    SECStatus rv = SECSuccess;
    CK_RV crv = CKR_DEVICE_ERROR;
    SECItem pubKey;
    NSSLOWKEYDBHandle *keyHandle = lg_getKeyDB(sdb);

    if (keyHandle == NULL) {
	return CKR_TOKEN_WRITE_PROTECTED;
    }

    privKey=lg_mkPrivKey(sdb, templ,count,key_type,&crv);
    if (privKey == NULL) return crv;
    label = lg_getString(CKA_LABEL,templ,count);

    crv = lg_Attribute2SSecItem(NULL,CKA_NETSCAPE_DB,templ,count,&pubKey);
    if (crv != CKR_OK) {
	crv = CKR_TEMPLATE_INCOMPLETE;
	rv = SECFailure;
	goto fail;
    }
#ifdef notdef
    if (keyHandle->version != 3) {
	unsigned char buf[SHA1_LENGTH];
	SHA1_HashBuf(buf,pubKey.data,pubKey.len);
	PORT_Memcpy(pubKey.data,buf,sizeof(buf));
	pubKey.len = sizeof(buf);
    }
#endif
    /* get the key type */
    if (key_type == CKK_RSA) {
	rv = RSA_PrivateKeyCheck(&privKey->u.rsa);
	if (rv == SECFailure) {
	    goto fail;
	}
    }
    rv = nsslowkey_StoreKeyByPublicKey(keyHandle, privKey, &pubKey, 
					   label, sdb /*->password*/);

fail:
    if (label) PORT_Free(label);
    *handle = lg_mkHandle(sdb,&pubKey,LG_TOKEN_TYPE_PRIV);
    if (pubKey.data) PORT_Free(pubKey.data);
    lg_nsslowkey_DestroyPrivateKey(privKey);
    if (rv != SECSuccess) return crv;

    return CKR_OK;
}


#define LG_KEY_MAX_RETRIES 10 /* don't hang if we are having problems with the rng */
#define LG_KEY_ID_SIZE 18 /* don't use either SHA1 or MD5 sizes */
/*
 * Secret keys must have a CKA_ID value to be stored in the database. This code
 * will generate one if there wasn't one already. 
 */
static CK_RV
lg_GenerateSecretCKA_ID(NSSLOWKEYDBHandle *handle, SECItem *id, char *label)
{
    unsigned int retries;
    SECStatus rv = SECSuccess;
    CK_RV crv = CKR_OK;

    id->data = NULL;
    if (label) {
	id->data = (unsigned char *)PORT_Strdup(label);
	if (id->data == NULL) {
	    return CKR_HOST_MEMORY;
	}
	id->len = PORT_Strlen(label)+1;
	if (!nsslowkey_KeyForIDExists(handle,id)) { 
	    return CKR_OK;
	}
	PORT_Free(id->data);
	id->data = NULL;
	id->len = 0;
    }
    id->data = (unsigned char *)PORT_Alloc(LG_KEY_ID_SIZE);
    if (id->data == NULL) {
	return CKR_HOST_MEMORY;
    }
    id->len = LG_KEY_ID_SIZE;

    retries = 0;
    do {
	rv = RNG_GenerateGlobalRandomBytes(id->data,id->len);
    } while (rv == SECSuccess && nsslowkey_KeyForIDExists(handle,id) && 
				(++retries <= LG_KEY_MAX_RETRIES));

    if ((rv != SECSuccess) || (retries > LG_KEY_MAX_RETRIES)) {
	crv = CKR_DEVICE_ERROR; /* random number generator is bad */
	PORT_Free(id->data);
	id->data = NULL;
	id->len = 0;
    }
    return crv;
}


static NSSLOWKEYPrivateKey *lg_mkSecretKeyRep(const CK_ATTRIBUTE *templ,
		 CK_ULONG count, CK_KEY_TYPE key_type, 
		 SECItem *pubkey, SDB *sdbpw)
{
    NSSLOWKEYPrivateKey *privKey = 0;
    PLArenaPool *arena = 0;
    CK_KEY_TYPE keyType;
    PRUint32 keyTypeStorage;
    SECItem keyTypeItem;
    CK_RV crv;
    SECStatus rv;
    static unsigned char derZero[1] = { 0 };

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL) { crv = CKR_HOST_MEMORY; goto loser; }

    privKey = (NSSLOWKEYPrivateKey *)
			PORT_ArenaZAlloc(arena,sizeof(NSSLOWKEYPrivateKey));
    if (privKey == NULL) { crv = CKR_HOST_MEMORY; goto loser; }

    privKey->arena = arena;

    /* Secret keys are represented in the database as "fake" RSA keys.  
     * The RSA key is marked as a secret key representation by setting the 
     * public exponent field to 0, which is an invalid RSA exponent.  
     * The other fields are set as follows:
     *   modulus - CKA_ID value for the secret key
     *   private exponent - CKA_VALUE (the key itself)
     *   coefficient - CKA_KEY_TYPE, which indicates what encryption algorithm
     *      is used for the key.
     *   all others - set to integer 0
     */
    privKey->keyType = NSSLOWKEYRSAKey;

    /* The modulus is set to the key id of the symmetric key */
    crv = lg_Attribute2SecItem(arena, CKA_ID, templ, count, 
				&privKey->u.rsa.modulus);
    if (crv != CKR_OK) goto loser;

    /* The public exponent is set to 0 length to indicate a special key */
    privKey->u.rsa.publicExponent.len = sizeof derZero;
    privKey->u.rsa.publicExponent.data = derZero;

    /* The private exponent is the actual key value */
    crv = lg_PrivAttr2SecItem(arena, CKA_VALUE, templ, count,
				&privKey->u.rsa.privateExponent, sdbpw);
    if (crv != CKR_OK) goto loser;

    /* All other fields empty - needs testing */
    privKey->u.rsa.prime1.len = sizeof derZero;
    privKey->u.rsa.prime1.data = derZero;

    privKey->u.rsa.prime2.len = sizeof derZero;
    privKey->u.rsa.prime2.data = derZero;

    privKey->u.rsa.exponent1.len = sizeof derZero;
    privKey->u.rsa.exponent1.data = derZero;

    privKey->u.rsa.exponent2.len = sizeof derZero;
    privKey->u.rsa.exponent2.data = derZero;

    /* Coeficient set to KEY_TYPE */
    crv = lg_GetULongAttribute(CKA_KEY_TYPE, templ, count, &keyType);
    if (crv != CKR_OK) goto loser; 
    /* on 64 bit platforms, we still want to store 32 bits of keyType (This is
     * safe since the PKCS #11 defines for all types are 32 bits or less). */
    keyTypeStorage = (PRUint32) keyType;
    keyTypeStorage = PR_htonl(keyTypeStorage);
    keyTypeItem.data = (unsigned char *)&keyTypeStorage;
    keyTypeItem.len = sizeof (keyTypeStorage);
    rv = SECITEM_CopyItem(arena, &privKey->u.rsa.coefficient, &keyTypeItem);
    if (rv != SECSuccess) {
	crv = CKR_HOST_MEMORY;
	goto loser;
    }
    
    /* Private key version field set normally for compatibility */
    rv = DER_SetUInteger(privKey->arena, 
			&privKey->u.rsa.version, NSSLOWKEY_VERSION);
    if (rv != SECSuccess) { crv = CKR_HOST_MEMORY; goto loser; }

loser:
    if (crv != CKR_OK) {
	PORT_FreeArena(arena,PR_FALSE);
	privKey = 0;
    }

    return privKey;
}

/*
 * check the consistancy and initialize a Secret Key Object 
 */
static CK_RV
lg_createSecretKeyObject(SDB *sdb, CK_KEY_TYPE key_type,
     CK_OBJECT_HANDLE *handle, const CK_ATTRIBUTE *templ, CK_ULONG count)
{
    CK_RV crv;
    NSSLOWKEYPrivateKey *privKey   = NULL;
    NSSLOWKEYDBHandle   *keyHandle = NULL;
    SECItem pubKey;
    char *label = NULL;
    SECStatus rv = SECSuccess;

    pubKey.data = 0;

    /* If the object is a TOKEN object, store in the database */
    keyHandle = lg_getKeyDB(sdb);

    if (keyHandle == NULL) {
	return CKR_TOKEN_WRITE_PROTECTED;
    }

    label = lg_getString(CKA_LABEL,templ,count);

    crv = lg_Attribute2SecItem(NULL,CKA_ID,templ,count,&pubKey);
						/* Should this be ID? */
    if (crv != CKR_OK) goto loser;

    /* if we don't have an ID, generate one */
    if (pubKey.len == 0) {
	if (pubKey.data) { 
	    PORT_Free(pubKey.data);
	    pubKey.data = NULL;
	}
	crv = lg_GenerateSecretCKA_ID(keyHandle, &pubKey, label);
	if (crv != CKR_OK) goto loser;
    }

    privKey = lg_mkSecretKeyRep(templ, count, key_type, &pubKey, sdb);
    if (privKey == NULL) {
	crv = CKR_HOST_MEMORY;
	goto loser;
    }

    rv = nsslowkey_StoreKeyByPublicKey(keyHandle,
			privKey, &pubKey, label, sdb /*->password*/);
    if (rv != SECSuccess) {
	crv = CKR_DEVICE_ERROR;
	goto loser;
    }

    *handle = lg_mkHandle(sdb, &pubKey, LG_TOKEN_TYPE_KEY);

loser:
    if (label) PORT_Free(label);
    if (privKey) lg_nsslowkey_DestroyPrivateKey(privKey);
    if (pubKey.data) PORT_Free(pubKey.data);

    return crv;
}

/*
 * check the consistancy and initialize a Key Object 
 */
static CK_RV
lg_createKeyObject(SDB *sdb, CK_OBJECT_CLASS objclass, 
	CK_OBJECT_HANDLE *handle, const CK_ATTRIBUTE *templ, CK_ULONG count)
{
    CK_RV crv;
    CK_KEY_TYPE key_type;

    /* get the key type */
    crv = lg_GetULongAttribute(CKA_KEY_TYPE, templ, count, &key_type);
    if (crv != CKR_OK) {
	return crv;
    }

    switch (objclass) {
    case CKO_PUBLIC_KEY:
	return lg_createPublicKeyObject(sdb,key_type,handle,templ,count);
    case CKO_PRIVATE_KEY:
	return lg_createPrivateKeyObject(sdb,key_type,handle,templ,count);
    case CKO_SECRET_KEY:
	return lg_createSecretKeyObject(sdb,key_type,handle,templ,count);
    default:
	break;
    }
    return CKR_ATTRIBUTE_VALUE_INVALID;
}

/* 
 * Parse the template and create an object stored in the DB that reflects.
 * the object specified in the database.
 */
CK_RV
lg_CreateObject(SDB *sdb, CK_OBJECT_HANDLE *handle,
			const CK_ATTRIBUTE *templ, CK_ULONG count)
{
    CK_RV crv;
    CK_OBJECT_CLASS objclass;

    /* get the object class */
    crv = lg_GetULongAttribute(CKA_CLASS, templ, count, &objclass);
    if (crv != CKR_OK) {
	return crv;
    }

    /* Now handle the specific object class. 
     */
    switch (objclass) {
    case CKO_CERTIFICATE:
	crv = lg_createCertObject(sdb,handle,templ,count);
	break;
    case CKO_NSS_TRUST:
	crv = lg_createTrustObject(sdb,handle,templ,count);
	break;
    case CKO_NSS_CRL:
	crv = lg_createCrlObject(sdb,handle,templ,count);
	break;
    case CKO_NSS_SMIME:
	crv = lg_createSMimeObject(sdb,handle,templ,count);
	break;
    case CKO_PRIVATE_KEY:
    case CKO_PUBLIC_KEY:
    case CKO_SECRET_KEY:
	crv = lg_createKeyObject(sdb,objclass,handle,templ,count);
	break;
    default:
	crv = CKR_ATTRIBUTE_VALUE_INVALID;
	break;
    }

    return crv;
}

