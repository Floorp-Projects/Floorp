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

/*
 * CMS recipientInfo methods.
 *
 * $Id: cmsrecinfo.c,v 1.3 2000/10/06 23:26:10 nelsonb%netscape.com Exp $
 */

#include "cmslocal.h"

#include "cert.h"
#include "key.h"
#include "secasn1.h"
#include "secitem.h"
#include "secoid.h"
#include "pk11func.h"
#include "secerr.h"

/*
 * NSS_CMSRecipientInfo_Create - create a recipientinfo
 *
 * we currently do not create KeyAgreement recipientinfos with multiple recipientEncryptedKeys
 * the certificate is supposed to have been verified by the caller
 */
NSSCMSRecipientInfo *
NSS_CMSRecipientInfo_Create(NSSCMSMessage *cmsg, CERTCertificate *cert)
{
    NSSCMSRecipientInfo *ri;
    void *mark;
    SECOidTag certalgtag;
    SECStatus rv;
    NSSCMSRecipientEncryptedKey *rek;
    NSSCMSOriginatorIdentifierOrKey *oiok;
    unsigned long version;
    SECItem *dummy;
    PLArenaPool *poolp;

    poolp = cmsg->poolp;

    mark = PORT_ArenaMark(poolp);

    ri = (NSSCMSRecipientInfo *)PORT_ArenaZAlloc(poolp, sizeof(NSSCMSRecipientInfo));
    if (ri == NULL)
	goto loser;

    ri->cmsg = cmsg;
    ri->cert = CERT_DupCertificate(cert);
    if (ri->cert == NULL)
	goto loser;

    certalgtag = SECOID_GetAlgorithmTag(&(cert->subjectPublicKeyInfo.algorithm));

    switch (certalgtag) {
    case SEC_OID_PKCS1_RSA_ENCRYPTION:
	ri->recipientInfoType = NSSCMSRecipientInfoID_KeyTrans;
	/* hardcoded issuerSN choice for now */
	ri->ri.keyTransRecipientInfo.recipientIdentifier.identifierType = NSSCMSRecipientID_IssuerSN;
	ri->ri.keyTransRecipientInfo.recipientIdentifier.id.issuerAndSN = CERT_GetCertIssuerAndSN(poolp, cert);
	if (ri->ri.keyTransRecipientInfo.recipientIdentifier.id.issuerAndSN == NULL) {
	    rv = SECFailure;
	    break;
	}
	break;
    case SEC_OID_MISSI_KEA_DSS_OLD:
    case SEC_OID_MISSI_KEA_DSS:
    case SEC_OID_MISSI_KEA:
	/* backward compatibility - this is not really a keytrans operation */
	ri->recipientInfoType = NSSCMSRecipientInfoID_KeyTrans;
	/* hardcoded issuerSN choice for now */
	ri->ri.keyTransRecipientInfo.recipientIdentifier.identifierType = NSSCMSRecipientID_IssuerSN;
	ri->ri.keyTransRecipientInfo.recipientIdentifier.id.issuerAndSN = CERT_GetCertIssuerAndSN(poolp, cert);
	if (ri->ri.keyTransRecipientInfo.recipientIdentifier.id.issuerAndSN == NULL) {
	    rv = SECFailure;
	    break;
	}
	break;
    case SEC_OID_X942_DIFFIE_HELMAN_KEY: /* dh-public-number */
	/* a key agreement op */
	ri->recipientInfoType = NSSCMSRecipientInfoID_KeyAgree;

	if (ri->ri.keyTransRecipientInfo.recipientIdentifier.id.issuerAndSN == NULL) {
	    rv = SECFailure;
	    break;
	}
	/* we do not support the case where multiple recipients 
	 * share the same KeyAgreeRecipientInfo and have multiple RecipientEncryptedKeys
	 * in this case, we would need to walk all the recipientInfos, take the
	 * ones that do KeyAgreement algorithms and join them, algorithm by algorithm
	 * Then, we'd generate ONE ukm and OriginatorIdentifierOrKey */

	/* only epheremal-static Diffie-Hellman is supported for now
	 * this is the only form of key agreement that provides potential anonymity
	 * of the sender, plus we do not have to include certs in the message */

	/* force single recipientEncryptedKey for now */
	if ((rek = NSS_CMSRecipientEncryptedKey_Create(poolp)) == NULL) {
	    rv = SECFailure;
	    break;
	}

	/* hardcoded IssuerSN choice for now */
	rek->recipientIdentifier.identifierType = NSSCMSKeyAgreeRecipientID_IssuerSN;
	if ((rek->recipientIdentifier.id.issuerAndSN = CERT_GetCertIssuerAndSN(poolp, cert)) == NULL) {
	    rv = SECFailure;
	    break;
	}

	oiok = &(ri->ri.keyAgreeRecipientInfo.originatorIdentifierOrKey);

	/* see RFC2630 12.3.1.1 */
	oiok->identifierType = NSSCMSOriginatorIDOrKey_OriginatorPublicKey;

	rv = NSS_CMSArray_Add(poolp, (void ***)&ri->ri.keyAgreeRecipientInfo.recipientEncryptedKeys,
				    (void *)rek);

	break;
    default:
	/* other algorithms not supported yet */
	/* NOTE that we do not support any KEK algorithm */
	PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
	rv = SECFailure;
	break;
    }

    if (rv == SECFailure)
	goto loser;

    /* set version */
    switch (ri->recipientInfoType) {
    case NSSCMSRecipientInfoID_KeyTrans:
	if (ri->ri.keyTransRecipientInfo.recipientIdentifier.identifierType == NSSCMSRecipientID_IssuerSN)
	    version = NSS_CMS_KEYTRANS_RECIPIENT_INFO_VERSION_ISSUERSN;
	else
	    version = NSS_CMS_KEYTRANS_RECIPIENT_INFO_VERSION_SUBJKEY;
	dummy = SEC_ASN1EncodeInteger(poolp, &(ri->ri.keyTransRecipientInfo.version), version);
	if (dummy == NULL)
	    goto loser;
	break;
    case NSSCMSRecipientInfoID_KeyAgree:
	dummy = SEC_ASN1EncodeInteger(poolp, &(ri->ri.keyAgreeRecipientInfo.version),
						NSS_CMS_KEYAGREE_RECIPIENT_INFO_VERSION);
	if (dummy == NULL)
	    goto loser;
	break;
    case NSSCMSRecipientInfoID_KEK:
	/* NOTE: this cannot happen as long as we do not support any KEK algorithm */
	dummy = SEC_ASN1EncodeInteger(poolp, &(ri->ri.kekRecipientInfo.version),
						NSS_CMS_KEK_RECIPIENT_INFO_VERSION);
	if (dummy == NULL)
	    goto loser;
	break;
    
    }

    PORT_ArenaUnmark (poolp, mark);
    return ri;

loser:
    PORT_ArenaRelease (poolp, mark);
    return NULL;
}

void
NSS_CMSRecipientInfo_Destroy(NSSCMSRecipientInfo *ri)
{
    /* version was allocated on the pool, so no need to destroy it */
    /* issuerAndSN was allocated on the pool, so no need to destroy it */
    if (ri->cert != NULL)
	CERT_DestroyCertificate(ri->cert);
    /* recipientInfo structure itself was allocated on the pool, so no need to destroy it */
    /* we're done. */
}

int
NSS_CMSRecipientInfo_GetVersion(NSSCMSRecipientInfo *ri)
{
    unsigned long version;
    SECItem *versionitem;

    switch (ri->recipientInfoType) {
    case NSSCMSRecipientInfoID_KeyTrans:
	/* ignore subIndex */
	versionitem = &(ri->ri.keyTransRecipientInfo.version);
	break;
    case NSSCMSRecipientInfoID_KEK:
	/* ignore subIndex */
	versionitem = &(ri->ri.kekRecipientInfo.version);
	break;
    case NSSCMSRecipientInfoID_KeyAgree:
	versionitem = &(ri->ri.keyAgreeRecipientInfo.version);
	break;
    }
    /* always take apart the SECItem */
    if (SEC_ASN1DecodeInteger(versionitem, &version) != SECSuccess)
	return 0;
    else
	return (int)version;
}

SECItem *
NSS_CMSRecipientInfo_GetEncryptedKey(NSSCMSRecipientInfo *ri, int subIndex)
{
    SECItem *enckey;

    switch (ri->recipientInfoType) {
    case NSSCMSRecipientInfoID_KeyTrans:
	/* ignore subIndex */
	enckey = &(ri->ri.keyTransRecipientInfo.encKey);
	break;
    case NSSCMSRecipientInfoID_KEK:
	/* ignore subIndex */
	enckey = &(ri->ri.kekRecipientInfo.encKey);
	break;
    case NSSCMSRecipientInfoID_KeyAgree:
	enckey = &(ri->ri.keyAgreeRecipientInfo.recipientEncryptedKeys[subIndex]->encKey);
	break;
    }
    return enckey;
}


SECOidTag
NSS_CMSRecipientInfo_GetKeyEncryptionAlgorithmTag(NSSCMSRecipientInfo *ri)
{
    SECOidTag encalgtag;

    switch (ri->recipientInfoType) {
    case NSSCMSRecipientInfoID_KeyTrans:
	encalgtag = SECOID_GetAlgorithmTag(&(ri->ri.keyTransRecipientInfo.keyEncAlg));
	break;
    case NSSCMSRecipientInfoID_KeyAgree:
	encalgtag = SECOID_GetAlgorithmTag(&(ri->ri.keyAgreeRecipientInfo.keyEncAlg));
	break;
    case NSSCMSRecipientInfoID_KEK:
	encalgtag = SECOID_GetAlgorithmTag(&(ri->ri.kekRecipientInfo.keyEncAlg));
	break;
    }
    return encalgtag;
}

SECStatus
NSS_CMSRecipientInfo_WrapBulkKey(NSSCMSRecipientInfo *ri, PK11SymKey *bulkkey, SECOidTag bulkalgtag)
{
    CERTCertificate *cert;
    SECOidTag certalgtag;
    SECStatus rv;
    SECItem *params = NULL;
    NSSCMSRecipientEncryptedKey *rek;
    NSSCMSOriginatorIdentifierOrKey *oiok;
    PLArenaPool *poolp;

    poolp = ri->cmsg->poolp;
    cert = ri->cert;
    PORT_Assert (cert != NULL);
    if (cert == NULL)
	return SECFailure;

    /* XXX set ri->recipientInfoType to the proper value here */
    /* or should we look if it's been set already ? */

    certalgtag = SECOID_GetAlgorithmTag(&(cert->subjectPublicKeyInfo.algorithm));
    switch (certalgtag) {
    case SEC_OID_PKCS1_RSA_ENCRYPTION:
	/* wrap the symkey */
	if (NSS_CMSUtil_EncryptSymKey_RSA(poolp, cert, bulkkey, &ri->ri.keyTransRecipientInfo.encKey) != SECSuccess) {
	    rv = SECFailure;
	    break;
	}

	rv = SECOID_SetAlgorithmID(poolp, &(ri->ri.keyTransRecipientInfo.keyEncAlg), certalgtag, NULL);
	break;
    case SEC_OID_MISSI_KEA_DSS_OLD:
    case SEC_OID_MISSI_KEA_DSS:
    case SEC_OID_MISSI_KEA:
	rv = NSS_CMSUtil_EncryptSymKey_MISSI(poolp, cert, bulkkey,
					bulkalgtag,
					&ri->ri.keyTransRecipientInfo.encKey,
					&params, ri->cmsg->pwfn_arg);
	if (rv != SECSuccess)
	    break;

	/* here, we DO need to pass the params to the wrap function because, with
	 * RSA, there is no funny stuff going on with generation of IV vectors or so */
	rv = SECOID_SetAlgorithmID(poolp, &(ri->ri.keyTransRecipientInfo.keyEncAlg), certalgtag, params);
	break;
    case SEC_OID_X942_DIFFIE_HELMAN_KEY: /* dh-public-number */
	rek = ri->ri.keyAgreeRecipientInfo.recipientEncryptedKeys[0];
	if (rek == NULL) {
	    rv = SECFailure;
	    break;
	}

	oiok = &(ri->ri.keyAgreeRecipientInfo.originatorIdentifierOrKey);
	PORT_Assert(oiok->identifierType == NSSCMSOriginatorIDOrKey_OriginatorPublicKey);

	/* see RFC2630 12.3.1.1 */
	if (SECOID_SetAlgorithmID(poolp, &oiok->id.originatorPublicKey.algorithmIdentifier,
				    SEC_OID_X942_DIFFIE_HELMAN_KEY, NULL) != SECSuccess) {
	    rv = SECFailure;
	    break;
	}

	/* this will generate a key pair, compute the shared secret, */
	/* derive a key and ukm for the keyEncAlg out of it, encrypt the bulk key with */
	/* the keyEncAlg, set encKey, keyEncAlg, publicKey etc. */
	rv = NSS_CMSUtil_EncryptSymKey_ESDH(poolp, cert, bulkkey,
					&rek->encKey,
					&ri->ri.keyAgreeRecipientInfo.ukm,
					&ri->ri.keyAgreeRecipientInfo.keyEncAlg,
					&oiok->id.originatorPublicKey.publicKey);

	break;
    default:
	/* other algorithms not supported yet */
	/* NOTE that we do not support any KEK algorithm */
	PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
	rv = SECFailure;
	break;
    }
    return rv;
}

PK11SymKey *
NSS_CMSRecipientInfo_UnwrapBulkKey(NSSCMSRecipientInfo *ri, int subIndex, 
	CERTCertificate *cert, SECKEYPrivateKey *privkey, SECOidTag bulkalgtag)
{
    PK11SymKey *bulkkey = NULL;
    SECAlgorithmID *encalg;
    SECOidTag encalgtag;
    SECItem *enckey;
    int error;

    ri->cert = cert;	/* mark the recipientInfo so we can find it later */

    switch (ri->recipientInfoType) {
    case NSSCMSRecipientInfoID_KeyTrans:
	encalg = &(ri->ri.keyTransRecipientInfo.keyEncAlg);
	encalgtag = SECOID_GetAlgorithmTag(&(ri->ri.keyTransRecipientInfo.keyEncAlg));
	enckey = &(ri->ri.keyTransRecipientInfo.encKey); /* ignore subIndex */
	switch (encalgtag) {
	case SEC_OID_PKCS1_RSA_ENCRYPTION:
	    /* RSA encryption algorithm: */
	    /* get the symmetric (bulk) key by unwrapping it using our private key */
	    bulkkey = NSS_CMSUtil_DecryptSymKey_RSA(privkey, enckey, bulkalgtag);
	    break;
	case SEC_OID_NETSCAPE_SMIME_KEA:
	    /* FORTEZZA key exchange algorithm */
	    /* the supplemental data is in the parameters of encalg */
	    bulkkey = NSS_CMSUtil_DecryptSymKey_MISSI(privkey, enckey, encalg, bulkalgtag, ri->cmsg->pwfn_arg);
	    break;
	default:
	    error = SEC_ERROR_UNSUPPORTED_KEYALG;
	    goto loser;
	}
	break;
    case NSSCMSRecipientInfoID_KeyAgree:
	encalg = &(ri->ri.keyAgreeRecipientInfo.keyEncAlg);
	encalgtag = SECOID_GetAlgorithmTag(&(ri->ri.keyAgreeRecipientInfo.keyEncAlg));
	enckey = &(ri->ri.keyAgreeRecipientInfo.recipientEncryptedKeys[subIndex]->encKey);
	switch (encalgtag) {
	case SEC_OID_X942_DIFFIE_HELMAN_KEY:
	    /* Diffie-Helman key exchange */
	    /* XXX not yet implemented */
	    /* XXX problem: SEC_OID_X942_DIFFIE_HELMAN_KEY points to a PKCS3 mechanism! */
	    /* we support ephemeral-static DH only, so if the recipientinfo */
	    /* has originator stuff in it, we punt (or do we? shouldn't be that hard...) */
	    /* first, we derive the KEK (a symkey!) using a Derive operation, then we get the */
	    /* content encryption key using a Unwrap op */
	    /* the derive operation has to generate the key using the algorithm in RFC2631 */
	    error = SEC_ERROR_UNSUPPORTED_KEYALG;
	    break;
	default:
	    error = SEC_ERROR_UNSUPPORTED_KEYALG;
	    goto loser;
	}
	break;
    case NSSCMSRecipientInfoID_KEK:
	encalg = &(ri->ri.kekRecipientInfo.keyEncAlg);
	encalgtag = SECOID_GetAlgorithmTag(&(ri->ri.kekRecipientInfo.keyEncAlg));
	enckey = &(ri->ri.kekRecipientInfo.encKey);
	/* not supported yet */
	error = SEC_ERROR_UNSUPPORTED_KEYALG;
	goto loser;
	break;
    }
    /* XXXX continue here */
    return bulkkey;

loser:
    return NULL;
}
