/*
 * SSL3 Protocol
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 *   Dr Vipul Gupta <vipul.gupta@sun.com> and
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

/* ECC code moved here from ssl3con.c */
/* $Id: ssl3ecc.c,v 1.3 2005/10/14 16:48:58 wtchang%redhat.com Exp $ */

#ifdef NSS_ENABLE_ECC

#include "nssrenam.h"
#include "cert.h"
#include "ssl.h"
#include "cryptohi.h"	/* for DSAU_ stuff */
#include "keyhi.h"
#include "secder.h"
#include "secitem.h"

#include "sslimpl.h"
#include "sslproto.h"
#include "sslerr.h"
#include "prtime.h"
#include "prinrval.h"
#include "prerror.h"
#include "pratom.h"
#include "prthread.h"

#include "pk11func.h"
#include "secmod.h"
#include "nsslocks.h"
#include "ec.h"
#include "blapi.h"

#include <stdio.h>

/*
    line 297: implicit function declaration: ssl3_InitPendingCipherSpec
    line 305: implicit function declaration: ssl3_AppendHandshakeHeader
    line 311: implicit function declaration: ssl3_AppendHandshakeVariable
    line 356: implicit function declaration: ssl3_ConsumeHandshakeVariable
*/


#ifndef PK11_SETATTRS
#define PK11_SETATTRS(x,id,v,l) (x)->type = (id); \
		(x)->pValue=(v); (x)->ulValueLen = (l);
#endif

/* Types and names of elliptic curves used in TLS */
typedef enum { ec_type_explicitPrime = 1,
	       ec_type_explicitChar2Curve,
	       ec_type_named
} ECType;

typedef enum { ec_noName = 0,
	       ec_sect163k1, ec_sect163r1, ec_sect163r2,
	       ec_sect193r1, ec_sect193r2, ec_sect233k1,
	       ec_sect233r1, ec_sect239k1, ec_sect283k1,
	       ec_sect283r1, ec_sect409k1, ec_sect409r1,
	       ec_sect571k1, ec_sect571r1, ec_secp160k1,
	       ec_secp160r1, ec_secp160r2, ec_secp192k1,
	       ec_secp192r1, ec_secp224k1, ec_secp224r1,
	       ec_secp256k1, ec_secp256r1, ec_secp384r1,
	       ec_secp521r1,
	       ec_pastLastName
} ECName;

#define supportedCurve(x) (((x) > ec_noName) && ((x) < ec_pastLastName))

/* Table containing OID tags for elliptic curves named in the
 * ECC-TLS IETF draft.
 */
static const SECOidTag ecName2OIDTag[] = {
	0,  
	SEC_OID_SECG_EC_SECT163K1,  /*  1 */
	SEC_OID_SECG_EC_SECT163R1,  /*  2 */
	SEC_OID_SECG_EC_SECT163R2,  /*  3 */
	SEC_OID_SECG_EC_SECT193R1,  /*  4 */
	SEC_OID_SECG_EC_SECT193R2,  /*  5 */
	SEC_OID_SECG_EC_SECT233K1,  /*  6 */
	SEC_OID_SECG_EC_SECT233R1,  /*  7 */
	SEC_OID_SECG_EC_SECT239K1,  /*  8 */
	SEC_OID_SECG_EC_SECT283K1,  /*  9 */
	SEC_OID_SECG_EC_SECT283R1,  /* 10 */
	SEC_OID_SECG_EC_SECT409K1,  /* 11 */
	SEC_OID_SECG_EC_SECT409R1,  /* 12 */
	SEC_OID_SECG_EC_SECT571K1,  /* 13 */
	SEC_OID_SECG_EC_SECT571R1,  /* 14 */
	SEC_OID_SECG_EC_SECP160K1,  /* 15 */
	SEC_OID_SECG_EC_SECP160R1,  /* 16 */
	SEC_OID_SECG_EC_SECP160R2,  /* 17 */
	SEC_OID_SECG_EC_SECP192K1,  /* 18 */
	SEC_OID_SECG_EC_SECP192R1,  /* 19 */
	SEC_OID_SECG_EC_SECP224K1,  /* 20 */
	SEC_OID_SECG_EC_SECP224R1,  /* 21 */
	SEC_OID_SECG_EC_SECP256K1,  /* 22 */
	SEC_OID_SECG_EC_SECP256R1,  /* 23 */
	SEC_OID_SECG_EC_SECP384R1,  /* 24 */
	SEC_OID_SECG_EC_SECP521R1,  /* 25 */
};

static SECStatus 
ecName2params(PRArenaPool * arena, ECName curve, SECKEYECParams * params)
{
    SECOidData *oidData = NULL;

    if ((curve <= ec_noName) || (curve >= ec_pastLastName) ||
	((oidData = SECOID_FindOIDByTag(ecName2OIDTag[curve])) == NULL)) {
        PORT_SetError(SEC_ERROR_UNSUPPORTED_ELLIPTIC_CURVE);
	return SECFailure;
    }

    SECITEM_AllocItem(arena, params, (2 + oidData->oid.len));
    /* 
     * params->data needs to contain the ASN encoding of an object ID (OID)
     * representing the named curve. The actual OID is in 
     * oidData->oid.data so we simply prepend 0x06 and OID length
     */
    params->data[0] = SEC_ASN1_OBJECT_ID;
    params->data[1] = oidData->oid.len;
    memcpy(params->data + 2, oidData->oid.data, oidData->oid.len);

    return SECSuccess;
}

static ECName 
params2ecName(SECKEYECParams * params)
{
    SECItem oid = { siBuffer, NULL, 0};
    SECOidData *oidData = NULL;
    ECName i;

    /* 
     * params->data needs to contain the ASN encoding of an object ID (OID)
     * representing a named curve. Here, we strip away everything
     * before the actual OID and use the OID to look up a named curve.
     */
    if (params->data[0] != SEC_ASN1_OBJECT_ID) return ec_noName;
    oid.len = params->len - 2;
    oid.data = params->data + 2;
    if ((oidData = SECOID_FindOID(&oid)) == NULL) return ec_noName;
    for (i = ec_noName + 1; i < ec_pastLastName; i++) {
	if (ecName2OIDTag[i] == oidData->offset)
	    return i;
    }

    return ec_noName;
}

/* Caller must set hiLevel error code. */
static SECStatus
ssl3_ComputeECDHKeyHash(SECItem ec_params, SECItem server_ecpoint,
			     SSL3Random *client_rand, SSL3Random *server_rand,
			     SSL3Hashes *hashes, PRBool bypassPKCS11)
{
    PRUint8     * hashBuf;
    PRUint8     * pBuf;
    SECStatus     rv 		= SECSuccess;
    unsigned int  bufLen;
    /*
     * XXX For now, we only support named curves (the appropriate
     * checks are made before this method is called) so ec_params
     * takes up only two bytes. ECPoint needs to fit in 256 bytes
     * (because the spec says the length must fit in one byte)
     */
    PRUint8       buf[2*SSL3_RANDOM_LENGTH + 2 + 1 + 256];

    bufLen = 2*SSL3_RANDOM_LENGTH + ec_params.len + 1 + server_ecpoint.len;
    if (bufLen <= sizeof buf) {
    	hashBuf = buf;
    } else {
    	hashBuf = PORT_Alloc(bufLen);
	if (!hashBuf) {
	    return SECFailure;
	}
    }

    memcpy(hashBuf, client_rand, SSL3_RANDOM_LENGTH); 
    	pBuf = hashBuf + SSL3_RANDOM_LENGTH;
    memcpy(pBuf, server_rand, SSL3_RANDOM_LENGTH);
    	pBuf += SSL3_RANDOM_LENGTH;
    memcpy(pBuf, ec_params.data, ec_params.len);
    	pBuf += ec_params.len;
    pBuf[0] = (PRUint8)(server_ecpoint.len);
    pBuf += 1;
    memcpy(pBuf, server_ecpoint.data, server_ecpoint.len);
    	pBuf += server_ecpoint.len;
    PORT_Assert((unsigned int)(pBuf - hashBuf) == bufLen);

    rv = ssl3_ComputeCommonKeyHash(hashBuf, bufLen, hashes, bypassPKCS11);

    PRINT_BUF(95, (NULL, "ECDHkey hash: ", hashBuf, bufLen));
    PRINT_BUF(95, (NULL, "ECDHkey hash: MD5 result", hashes->md5, MD5_LENGTH));
    PRINT_BUF(95, (NULL, "ECDHkey hash: SHA1 result", hashes->sha, SHA1_LENGTH));

done:
    if (hashBuf != buf && hashBuf != NULL)
    	PORT_Free(hashBuf);
    return rv;
}


/* Called from ssl3_SendClientKeyExchange(). */
SECStatus
ssl3_SendECDHClientKeyExchange(sslSocket * ss, SECKEYPublicKey * svrPubKey)
{
    PK11SymKey *	pms 		= NULL;
    SECStatus           rv    		= SECFailure;
    PRBool              isTLS;
    CK_MECHANISM_TYPE	target;
    SECKEYPublicKey	*pubKey = NULL;		/* Ephemeral ECDH key */
    SECKEYPrivateKey	*privKey = NULL;	/* Ephemeral ECDH key */
    CK_EC_KDF_TYPE	kdf;

    PORT_Assert( ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss) );
    PORT_Assert( ss->opt.noLocks || ssl_HaveXmitBufLock(ss));

    isTLS = (PRBool)(ss->ssl3.pwSpec->version > SSL_LIBRARY_VERSION_3_0);

    /* Generate ephemeral EC keypair */
    privKey = SECKEY_CreateECPrivateKey(&svrPubKey->u.ec.DEREncodedParams, 
	                                &pubKey, NULL);
    if (!privKey || !pubKey) {
	    ssl_MapLowLevelError(SEC_ERROR_KEYGEN_FAIL);
	    rv = SECFailure;
	    goto loser;
    }
    PRINT_BUF(50, (ss, "ECDH public value:",
					pubKey->u.ec.publicValue.data,
					pubKey->u.ec.publicValue.len));

    if (isTLS) target = CKM_TLS_MASTER_KEY_DERIVE_DH;
    else target = CKM_SSL3_MASTER_KEY_DERIVE_DH;

    /* If field size is not more than 24 octets, then use SHA-1 hash of result;
     * otherwise, use result (see section 4.8 of draft-ietf-tls-ecc-03.txt).
     */
    if ((pubKey->u.ec.publicValue.len - 1) / 2 <= 24) {
	kdf = CKD_SHA1_KDF;
    } else {
	kdf = CKD_NULL;
    }

    /*  Determine the PMS */
    pms = PK11_PubDeriveWithKDF(privKey, svrPubKey, PR_FALSE, NULL, NULL,
			    CKM_ECDH1_DERIVE, target, CKA_DERIVE, 0,
			    kdf, NULL, NULL);

    if (pms == NULL) {
	ssl_MapLowLevelError(SSL_ERROR_CLIENT_KEY_EXCHANGE_FAILURE);
	goto loser;
    }

    SECKEY_DestroyPrivateKey(privKey);
    privKey = NULL;

    rv = ssl3_InitPendingCipherSpec(ss,  pms);
    PK11_FreeSymKey(pms); pms = NULL;

    if (rv != SECSuccess) {
	ssl_MapLowLevelError(SSL_ERROR_CLIENT_KEY_EXCHANGE_FAILURE);
	goto loser;
    }

    rv = ssl3_AppendHandshakeHeader(ss, client_key_exchange, 
					pubKey->u.ec.publicValue.len + 1);
    if (rv != SECSuccess) {
        goto loser;	/* err set by ssl3_AppendHandshake* */
    }

    rv = ssl3_AppendHandshakeVariable(ss, 
					pubKey->u.ec.publicValue.data,
					pubKey->u.ec.publicValue.len, 1);
    SECKEY_DestroyPublicKey(pubKey);
    pubKey = NULL;

    if (rv != SECSuccess) {
        goto loser;	/* err set by ssl3_AppendHandshake* */
    }

    rv = SECSuccess;

loser:
    if(pms) PK11_FreeSymKey(pms);
    if(privKey) SECKEY_DestroyPrivateKey(privKey);
    if(pubKey) SECKEY_DestroyPublicKey(pubKey);
    return rv;
}


/*
** Called from ssl3_HandleClientKeyExchange()
*/
SECStatus
ssl3_HandleECDHClientKeyExchange(sslSocket *ss, SSL3Opaque *b,
				     PRUint32 length,
                                     SECKEYPublicKey *srvrPubKey,
                                     SECKEYPrivateKey *srvrPrivKey)
{
    PK11SymKey *      pms;
    SECStatus         rv;
    SECKEYPublicKey   clntPubKey;
    CK_MECHANISM_TYPE	target;
    PRBool isTLS;
    CK_EC_KDF_TYPE	kdf;

    PORT_Assert( ss->opt.noLocks || ssl_HaveRecvBufLock(ss) );
    PORT_Assert( ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss) );

    clntPubKey.keyType = ecKey;
    clntPubKey.u.ec.DEREncodedParams.len = 
	srvrPubKey->u.ec.DEREncodedParams.len;
    clntPubKey.u.ec.DEREncodedParams.data = 
	srvrPubKey->u.ec.DEREncodedParams.data;

    rv = ssl3_ConsumeHandshakeVariable(ss, &clntPubKey.u.ec.publicValue, 
	                               1, &b, &length);
    if (rv != SECSuccess) {
	SEND_ALERT
	return SECFailure;	/* XXX Who sets the error code?? */
    }

    isTLS = (PRBool)(ss->ssl3.prSpec->version > SSL_LIBRARY_VERSION_3_0);

    if (isTLS) target = CKM_TLS_MASTER_KEY_DERIVE_DH;
    else target = CKM_SSL3_MASTER_KEY_DERIVE_DH;

    /* If field size is not more than 24 octets, then use SHA-1 hash of result;
     * otherwise, use result (see section 4.8 of draft-ietf-tls-ecc-03.txt).
     */
    if (srvrPubKey->u.ec.size <= 24 * 8) {
	kdf = CKD_SHA1_KDF;
    } else {
	kdf = CKD_NULL;
    }

    /*  Determine the PMS */
    pms = PK11_PubDeriveWithKDF(srvrPrivKey, &clntPubKey, PR_FALSE, NULL, NULL,
			    CKM_ECDH1_DERIVE, target, CKA_DERIVE, 0,
			    kdf, NULL, NULL);

    if (pms == NULL) {
	/* last gasp.  */
	ssl_MapLowLevelError(SSL_ERROR_CLIENT_KEY_EXCHANGE_FAILURE);
	return SECFailure;
    }

    rv = ssl3_InitPendingCipherSpec(ss,  pms);
    PK11_FreeSymKey(pms);
    if (rv != SECSuccess) {
	SEND_ALERT
	return SECFailure; /* error code set by ssl3_InitPendingCipherSpec */
    }
    return SECSuccess;
}


/*
 * Creates the ephemeral public and private ECDH keys used by
 * server in ECDHE_RSA and ECDHE_ECDSA handshakes.
 * XXX For now, the elliptic curve is hardcoded to NIST P-224.
 * We need an API to specify the curve. This won't be a real
 * issue until we further develop server-side support for ECC
 * cipher suites.
 */
SECStatus
ssl3_CreateECDHEphemeralKeys(sslSocket *ss)
{
    SECStatus             rv  	 = SECSuccess;
    SECKEYPrivateKey *    privKey;
    SECKEYPublicKey *     pubKey;
    SECKEYECParams	  ecParams = { siBuffer, NULL, 0 };

    if (ss->ephemeralECDHKeyPair)
	ssl3_FreeKeyPair(ss->ephemeralECDHKeyPair);
    ss->ephemeralECDHKeyPair = NULL;

    if (ecName2params(NULL, ec_secp224r1, &ecParams) == SECFailure)
	return SECFailure;
    privKey = SECKEY_CreateECPrivateKey(&ecParams, &pubKey, NULL);    
    if (!privKey || !pubKey ||
	!(ss->ephemeralECDHKeyPair = ssl3_NewKeyPair(privKey, pubKey))) {
	    ssl_MapLowLevelError(SEC_ERROR_KEYGEN_FAIL);
	    rv = SECFailure;
    }

    PORT_Free(ecParams.data);
    return rv;
}

SECStatus
ssl3_HandleECDHServerKeyExchange(sslSocket *ss, SSL3Opaque *b, PRUint32 length)
{
    PRArenaPool *    arena     = NULL;
    SECKEYPublicKey *peerKey   = NULL;
    PRBool           isTLS;
    SECStatus        rv;
    int              errCode   = SSL_ERROR_RX_MALFORMED_SERVER_KEY_EXCH;
    SSL3AlertDescription desc  = illegal_parameter;
    SSL3Hashes       hashes;
    SECItem          signature = {siBuffer, NULL, 0};

    SECItem          ec_params = {siBuffer, NULL, 0};
    SECItem          ec_point  = {siBuffer, NULL, 0};
    unsigned char    paramBuf[2];

    isTLS = (PRBool)(ss->ssl3.prSpec->version > SSL_LIBRARY_VERSION_3_0);

    /* XXX This works only for named curves, revisit this when
     * we support generic curves.
     */
    ec_params.len = 2;
    ec_params.data = paramBuf;
    rv = ssl3_ConsumeHandshake(ss, ec_params.data, ec_params.len, 
			       &b, &length);
    if (rv != SECSuccess) {
	goto loser;		/* malformed. */
    }

    /* Fail if the curve is not a named curve */
    if ((ec_params.data[0] != ec_type_named) || 
	!supportedCurve(ec_params.data[1])) {
	    errCode = SEC_ERROR_UNSUPPORTED_ELLIPTIC_CURVE;
	    desc = handshake_failure;
	    goto alert_loser;
    }

    rv = ssl3_ConsumeHandshakeVariable(ss, &ec_point, 1, &b, &length);
    if (rv != SECSuccess) {
	goto loser;		/* malformed. */
    }
    /* Fail if the ec point uses compressed representation */
    if (ec_point.data[0] != EC_POINT_FORM_UNCOMPRESSED) {
	    errCode = SEC_ERROR_UNSUPPORTED_EC_POINT_FORM;
	    desc = handshake_failure;
	    goto alert_loser;
    }

    rv = ssl3_ConsumeHandshakeVariable(ss, &signature, 2, &b, &length);
    if (rv != SECSuccess) {
	goto loser;		/* malformed. */
    }

    if (length != 0) {
	if (isTLS)
	    desc = decode_error;
	goto alert_loser;		/* malformed. */
    }

    PRINT_BUF(60, (NULL, "Server EC params", ec_params.data, 
	ec_params.len));
    PRINT_BUF(60, (NULL, "Server EC point", ec_point.data, ec_point.len));

    /* failures after this point are not malformed handshakes. */
    /* TLS: send decrypt_error if signature failed. */
    desc = isTLS ? decrypt_error : handshake_failure;

    /*
     *  check to make sure the hash is signed by right guy
     */
    rv = ssl3_ComputeECDHKeyHash(ec_params, ec_point,
				      &ss->ssl3.hs.client_random,
				      &ss->ssl3.hs.server_random, 
				      &hashes, ss->opt.bypassPKCS11);

    if (rv != SECSuccess) {
	errCode =
	    ssl_MapLowLevelError(SSL_ERROR_SERVER_KEY_EXCHANGE_FAILURE);
	goto alert_loser;
    }
    rv = ssl3_VerifySignedHashes(&hashes, ss->sec.peerCert, &signature,
				isTLS, ss->pkcs11PinArg);
    if (rv != SECSuccess)  {
	errCode =
	    ssl_MapLowLevelError(SSL_ERROR_SERVER_KEY_EXCHANGE_FAILURE);
	goto alert_loser;
    }

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL) {
	goto no_memory;
    }

    ss->sec.peerKey = peerKey = PORT_ArenaZNew(arena, SECKEYPublicKey);
    if (peerKey == NULL) {
	goto no_memory;
    }

    peerKey->arena                 = arena;
    peerKey->keyType               = ecKey;

    /* set up EC parameters in peerKey */
    if (ecName2params(arena, ec_params.data[1], 
	    &peerKey->u.ec.DEREncodedParams) != SECSuccess) {
	/* we should never get here since we already 
	 * checked that we are dealing with a supported curve
	 */
	errCode = SEC_ERROR_UNSUPPORTED_ELLIPTIC_CURVE;
	goto alert_loser;
    }

    /* copy publicValue in peerKey */
    if (SECITEM_CopyItem(arena, &peerKey->u.ec.publicValue,  &ec_point))
    {
	PORT_FreeArena(arena, PR_FALSE);
	goto no_memory;
    }
    peerKey->pkcs11Slot         = NULL;
    peerKey->pkcs11ID           = CK_INVALID_HANDLE;

    ss->sec.peerKey = peerKey;
    ss->ssl3.hs.ws = wait_cert_request;

    return SECSuccess;

alert_loser:
    (void)SSL3_SendAlert(ss, alert_fatal, desc);
loser:
    PORT_SetError( errCode );
    return SECFailure;

no_memory:	/* no-memory error has already been set. */
    ssl_MapLowLevelError(SSL_ERROR_SERVER_KEY_EXCHANGE_FAILURE);
    return SECFailure;
}

SECStatus
ssl3_SendECDHServerKeyExchange(sslSocket *ss)
{
const ssl3KEADef *     kea_def     = ss->ssl3.hs.kea_def;
    SECStatus          rv          = SECFailure;
    int                length;
    PRBool             isTLS;
    SECItem            signed_hash = {siBuffer, NULL, 0};
    SSL3Hashes         hashes;

    SECKEYPublicKey *  ecdhePub;
    SECItem            ec_params = {siBuffer, NULL, 0};
    ECName             curve;
    SSL3KEAType        certIndex;


    /* Generate ephemeral ECDH key pair and send the public key */
    rv = ssl3_CreateECDHEphemeralKeys(ss);
    if (rv != SECSuccess) {
	goto loser; 	/* err set by AppendHandshake. */
    }	    
    ecdhePub = ss->ephemeralECDHKeyPair->pubKey;
    PORT_Assert(ecdhePub != NULL);
    if (!ecdhePub) {
	PORT_SetError(SSL_ERROR_SERVER_KEY_EXCHANGE_FAILURE);
	return SECFailure;
    }	
    
    ec_params.len = 2;
    ec_params.data = (unsigned char*)PORT_Alloc(ec_params.len);
    curve = params2ecName(&ecdhePub->u.ec.DEREncodedParams);
    if (curve != ec_noName) {
	ec_params.data[0] = ec_type_named;
	ec_params.data[1] = curve;
    } else {
	PORT_SetError(SEC_ERROR_UNSUPPORTED_ELLIPTIC_CURVE);
	goto loser;
    }		

    rv = ssl3_ComputeECDHKeyHash(ec_params, ecdhePub->u.ec.publicValue,
				      &ss->ssl3.hs.client_random,
				      &ss->ssl3.hs.server_random,
				      &hashes, ss->opt.bypassPKCS11);
    if (rv != SECSuccess) {
	ssl_MapLowLevelError(SSL_ERROR_SERVER_KEY_EXCHANGE_FAILURE);
	goto loser;
    }

    isTLS = (PRBool)(ss->ssl3.pwSpec->version > SSL_LIBRARY_VERSION_3_0);

    /* XXX SSLKEAType isn't really a good choice for 
     * indexing certificates but that's all we have
     * for now.
     */
    if (kea_def->kea == kea_ecdhe_rsa)
	certIndex = kt_rsa;
    else /* kea_def->kea == kea_ecdhe_ecdsa */
	certIndex = kt_ecdh;

    rv = ssl3_SignHashes(&hashes, ss->serverCerts[certIndex].SERVERKEY, 
			 &signed_hash, isTLS);
    if (rv != SECSuccess) {
	goto loser;		/* ssl3_SignHashes has set err. */
    }
    if (signed_hash.data == NULL) {
	/* how can this happen and rv == SECSuccess ?? */
	PORT_SetError(SSL_ERROR_SERVER_KEY_EXCHANGE_FAILURE);
	goto loser;
    }

    length = ec_params.len + 
	     1 + ecdhePub->u.ec.publicValue.len + 
	     2 + signed_hash.len;

    rv = ssl3_AppendHandshakeHeader(ss, server_key_exchange, length);
    if (rv != SECSuccess) {
	goto loser; 	/* err set by AppendHandshake. */
    }

    rv = ssl3_AppendHandshake(ss, ec_params.data, ec_params.len);
    if (rv != SECSuccess) {
	goto loser; 	/* err set by AppendHandshake. */
    }

    rv = ssl3_AppendHandshakeVariable(ss, ecdhePub->u.ec.publicValue.data,
				      ecdhePub->u.ec.publicValue.len, 1);
    if (rv != SECSuccess) {
	goto loser; 	/* err set by AppendHandshake. */
    }

    rv = ssl3_AppendHandshakeVariable(ss, signed_hash.data,
				      signed_hash.len, 2);
    if (rv != SECSuccess) {
	goto loser; 	/* err set by AppendHandshake. */
    }

    PORT_Free(ec_params.data);
    PORT_Free(signed_hash.data);
    return SECSuccess;

loser:
    if (ec_params.data != NULL) 
	PORT_Free(ec_params.data);
    if (signed_hash.data != NULL) 
    	PORT_Free(signed_hash.data);
    return SECFailure;
}


#endif /* NSS_ENABLE_ECC */

