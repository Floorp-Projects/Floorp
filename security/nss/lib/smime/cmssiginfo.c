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
 * CMS signerInfo methods.
 *
 * $Id: cmssiginfo.c,v 1.34 2011/02/07 18:32:19 nelson%bolyard.com Exp $
 */

#include "cmslocal.h"

#include "cert.h"
#include "key.h"
#include "secasn1.h"
#include "secitem.h"
#include "secoid.h"
#include "pk11func.h"
#include "prtime.h"
#include "secerr.h"
#include "secder.h"
#include "cryptohi.h"

#include "smime.h"

/* =============================================================================
 * SIGNERINFO
 */
NSSCMSSignerInfo *
nss_cmssignerinfo_create(NSSCMSMessage *cmsg, NSSCMSSignerIDSelector type, 
	CERTCertificate *cert, SECItem *subjKeyID, SECKEYPublicKey *pubKey, 
	SECKEYPrivateKey *signingKey, SECOidTag digestalgtag);

NSSCMSSignerInfo *
NSS_CMSSignerInfo_CreateWithSubjKeyID(NSSCMSMessage *cmsg, SECItem *subjKeyID, 
	SECKEYPublicKey *pubKey, SECKEYPrivateKey *signingKey, SECOidTag digestalgtag)
{
    return nss_cmssignerinfo_create(cmsg, NSSCMSSignerID_SubjectKeyID, NULL, subjKeyID, pubKey, signingKey, digestalgtag); 
}

NSSCMSSignerInfo *
NSS_CMSSignerInfo_Create(NSSCMSMessage *cmsg, CERTCertificate *cert, SECOidTag digestalgtag)
{
    return nss_cmssignerinfo_create(cmsg, NSSCMSSignerID_IssuerSN, cert, NULL, NULL, NULL, digestalgtag); 
}

NSSCMSSignerInfo *
nss_cmssignerinfo_create(NSSCMSMessage *cmsg, NSSCMSSignerIDSelector type, 
	CERTCertificate *cert, SECItem *subjKeyID, SECKEYPublicKey *pubKey, 
	SECKEYPrivateKey *signingKey, SECOidTag digestalgtag)
{
    void *mark;
    NSSCMSSignerInfo *signerinfo;
    int version;
    PLArenaPool *poolp;

    poolp = cmsg->poolp;

    mark = PORT_ArenaMark(poolp);

    signerinfo = (NSSCMSSignerInfo *)PORT_ArenaZAlloc(poolp, sizeof(NSSCMSSignerInfo));
    if (signerinfo == NULL) {
	PORT_ArenaRelease(poolp, mark);
	return NULL;
    }


    signerinfo->cmsg = cmsg;

    switch(type) {
    case NSSCMSSignerID_IssuerSN:
        signerinfo->signerIdentifier.identifierType = NSSCMSSignerID_IssuerSN;
        if ((signerinfo->cert = CERT_DupCertificate(cert)) == NULL)
	    goto loser;
        if ((signerinfo->signerIdentifier.id.issuerAndSN = CERT_GetCertIssuerAndSN(poolp, cert)) == NULL)
	    goto loser;
        break;
    case NSSCMSSignerID_SubjectKeyID:
        signerinfo->signerIdentifier.identifierType = NSSCMSSignerID_SubjectKeyID;
        PORT_Assert(subjKeyID);
        if (!subjKeyID)
            goto loser;

        signerinfo->signerIdentifier.id.subjectKeyID = PORT_ArenaNew(poolp, SECItem);
        SECITEM_CopyItem(poolp, signerinfo->signerIdentifier.id.subjectKeyID,
                         subjKeyID);
        signerinfo->signingKey = SECKEY_CopyPrivateKey(signingKey);
        if (!signerinfo->signingKey)
            goto loser;
        signerinfo->pubKey = SECKEY_CopyPublicKey(pubKey);
        if (!signerinfo->pubKey)
            goto loser;
        break;
    default:
        goto loser;
    }

    /* set version right now */
    version = NSS_CMS_SIGNER_INFO_VERSION_ISSUERSN;
    /* RFC2630 5.3 "version is the syntax version number. If the .... " */
    if (signerinfo->signerIdentifier.identifierType == NSSCMSSignerID_SubjectKeyID)
	version = NSS_CMS_SIGNER_INFO_VERSION_SUBJKEY;
    (void)SEC_ASN1EncodeInteger(poolp, &(signerinfo->version), (long)version);

    if (SECOID_SetAlgorithmID(poolp, &signerinfo->digestAlg, digestalgtag, NULL) != SECSuccess)
	goto loser;

    PORT_ArenaUnmark(poolp, mark);
    return signerinfo;

loser:
    PORT_ArenaRelease(poolp, mark);
    return NULL;
}

/*
 * NSS_CMSSignerInfo_Destroy - destroy a SignerInfo data structure
 */
void
NSS_CMSSignerInfo_Destroy(NSSCMSSignerInfo *si)
{
    if (si->cert != NULL)
	CERT_DestroyCertificate(si->cert);

    if (si->certList != NULL) 
	CERT_DestroyCertificateList(si->certList);

    /* XXX storage ??? */
}

/*
 * NSS_CMSSignerInfo_Sign - sign something
 *
 */
SECStatus
NSS_CMSSignerInfo_Sign(NSSCMSSignerInfo *signerinfo, SECItem *digest, 
                       SECItem *contentType)
{
    CERTCertificate *cert;
    SECKEYPrivateKey *privkey = NULL;
    SECOidTag digestalgtag;
    SECOidTag pubkAlgTag;
    SECItem signature = { 0 };
    SECStatus rv;
    PLArenaPool *poolp, *tmppoolp = NULL;
    SECAlgorithmID *algID, freeAlgID;
    CERTSubjectPublicKeyInfo *spki;

    PORT_Assert (digest != NULL);

    poolp = signerinfo->cmsg->poolp;

    switch (signerinfo->signerIdentifier.identifierType) {
    case NSSCMSSignerID_IssuerSN:
        cert = signerinfo->cert;

        privkey = PK11_FindKeyByAnyCert(cert, signerinfo->cmsg->pwfn_arg);
        if (privkey == NULL)
	    goto loser;
        algID = &cert->subjectPublicKeyInfo.algorithm;
        break;
    case NSSCMSSignerID_SubjectKeyID:
        privkey = signerinfo->signingKey;
        signerinfo->signingKey = NULL;
        spki = SECKEY_CreateSubjectPublicKeyInfo(signerinfo->pubKey);
        SECKEY_DestroyPublicKey(signerinfo->pubKey);
        signerinfo->pubKey = NULL;
        SECOID_CopyAlgorithmID(NULL, &freeAlgID, &spki->algorithm);
        SECKEY_DestroySubjectPublicKeyInfo(spki); 
        algID = &freeAlgID;
        break;
    default:
        goto loser;
    }
    digestalgtag = NSS_CMSSignerInfo_GetDigestAlgTag(signerinfo);
    /*
     * XXX I think there should be a cert-level interface for this,
     * so that I do not have to know about subjectPublicKeyInfo...
     */
    pubkAlgTag = SECOID_GetAlgorithmTag(algID);
    if (signerinfo->signerIdentifier.identifierType == NSSCMSSignerID_SubjectKeyID) {
      SECOID_DestroyAlgorithmID(&freeAlgID, PR_FALSE);
    }

    /* Fortezza MISSI have weird signature formats.  
     * Map them to standard DSA formats 
     */
    pubkAlgTag = PK11_FortezzaMapSig(pubkAlgTag);

    if (signerinfo->authAttr != NULL) {
	SECOidTag signAlgTag;
	SECItem encoded_attrs;

	/* find and fill in the message digest attribute. */
	rv = NSS_CMSAttributeArray_SetAttr(poolp, &(signerinfo->authAttr), 
	                       SEC_OID_PKCS9_MESSAGE_DIGEST, digest, PR_FALSE);
	if (rv != SECSuccess)
	    goto loser;

	if (contentType != NULL) {
	    /* if the caller wants us to, find and fill in the content type attribute. */
	    rv = NSS_CMSAttributeArray_SetAttr(poolp, &(signerinfo->authAttr), 
	                    SEC_OID_PKCS9_CONTENT_TYPE, contentType, PR_FALSE);
	    if (rv != SECSuccess)
		goto loser;
	}

	if ((tmppoolp = PORT_NewArena (1024)) == NULL) {
	    PORT_SetError(SEC_ERROR_NO_MEMORY);
	    goto loser;
	}

	/*
	 * Before encoding, reorder the attributes so that when they
	 * are encoded, they will be conforming DER, which is required
	 * to have a specific order and that is what must be used for
	 * the hash/signature.  We do this here, rather than building
	 * it into EncodeAttributes, because we do not want to do
	 * such reordering on incoming messages (which also uses
	 * EncodeAttributes) or our old signatures (and other "broken"
	 * implementations) will not verify.  So, we want to guarantee
	 * that we send out good DER encodings of attributes, but not
	 * to expect to receive them.
	 */
	if (NSS_CMSAttributeArray_Reorder(signerinfo->authAttr) != SECSuccess)
	    goto loser;

	encoded_attrs.data = NULL;
	encoded_attrs.len = 0;
	if (NSS_CMSAttributeArray_Encode(tmppoolp, &(signerinfo->authAttr), 
	                &encoded_attrs) == NULL)
	    goto loser;

	signAlgTag = SEC_GetSignatureAlgorithmOidTag(privkey->keyType, 
                                                     digestalgtag);
	if (signAlgTag == SEC_OID_UNKNOWN) {
	    PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
	    goto loser;
	}

	rv = SEC_SignData(&signature, encoded_attrs.data, encoded_attrs.len, 
	                  privkey, signAlgTag);
	PORT_FreeArena(tmppoolp, PR_FALSE); /* awkward memory management :-( */
	tmppoolp = 0;
    } else {
	rv = SGN_Digest(privkey, digestalgtag, &signature, digest);
    }
    SECKEY_DestroyPrivateKey(privkey);
    privkey = NULL;

    if (rv != SECSuccess)
	goto loser;

    if (SECITEM_CopyItem(poolp, &(signerinfo->encDigest), &signature) 
          != SECSuccess)
	goto loser;

    SECITEM_FreeItem(&signature, PR_FALSE);

    if (SECOID_SetAlgorithmID(poolp, &(signerinfo->digestEncAlg), pubkAlgTag, 
                              NULL) != SECSuccess)
	goto loser;

    return SECSuccess;

loser:
    if (signature.len != 0)
	SECITEM_FreeItem (&signature, PR_FALSE);
    if (privkey)
	SECKEY_DestroyPrivateKey(privkey);
    if (tmppoolp)
	PORT_FreeArena(tmppoolp, PR_FALSE);
    return SECFailure;
}

SECStatus
NSS_CMSSignerInfo_VerifyCertificate(NSSCMSSignerInfo *signerinfo, CERTCertDBHandle *certdb,
			    SECCertUsage certusage)
{
    CERTCertificate *cert;
    int64 stime;

    if ((cert = NSS_CMSSignerInfo_GetSigningCertificate(signerinfo, certdb)) == NULL) {
	signerinfo->verificationStatus = NSSCMSVS_SigningCertNotFound;
	return SECFailure;
    }

    /*
     * Get and convert the signing time; if available, it will be used
     * both on the cert verification and for importing the sender
     * email profile.
     */
    if (NSS_CMSSignerInfo_GetSigningTime (signerinfo, &stime) != SECSuccess)
	stime = PR_Now(); /* not found or conversion failed, so check against now */
    
    /*
     * XXX  This uses the signing time, if available.  Additionally, we
     * might want to, if there is no signing time, get the message time
     * from the mail header itself, and use that.  That would require
     * a change to our interface though, and for S/MIME callers to pass
     * in a time (and for non-S/MIME callers to pass in nothing, or
     * maybe make them pass in the current time, always?).
     */
    if (CERT_VerifyCert(certdb, cert, PR_TRUE, certusage, stime, 
                        signerinfo->cmsg->pwfn_arg, NULL) != SECSuccess) {
	signerinfo->verificationStatus = NSSCMSVS_SigningCertNotTrusted;
	return SECFailure;
    }
    return SECSuccess;
}

/*
 * NSS_CMSSignerInfo_Verify - verify the signature of a single SignerInfo
 *
 * Just verifies the signature. The assumption is that verification of 
 * the certificate is done already.
 */
SECStatus
NSS_CMSSignerInfo_Verify(NSSCMSSignerInfo *signerinfo, 
                         SECItem *digest,               /* may be NULL */
                         SECItem *contentType)          /* may be NULL */
{
    SECKEYPublicKey *publickey = NULL;
    NSSCMSAttribute *attr;
    SECItem encoded_attrs;
    CERTCertificate *cert;
    NSSCMSVerificationStatus vs = NSSCMSVS_Unverified;
    PLArenaPool *poolp;
    SECOidTag    digestalgtag;
    SECOidTag    pubkAlgTag;

    if (signerinfo == NULL)
	return SECFailure;

    /* NSS_CMSSignerInfo_GetSigningCertificate will fail if 2nd parm is NULL 
    ** and cert has not been verified 
    */
    cert = NSS_CMSSignerInfo_GetSigningCertificate(signerinfo, NULL);
    if (cert == NULL) {
	vs = NSSCMSVS_SigningCertNotFound;
	goto loser;
    }

    if ((publickey = CERT_ExtractPublicKey(cert)) == NULL) {
	vs = NSSCMSVS_ProcessingError;
	goto loser;
    }

    digestalgtag = NSS_CMSSignerInfo_GetDigestAlgTag(signerinfo);
    pubkAlgTag = SECOID_GetAlgorithmTag(&(signerinfo->digestEncAlg));
    if ((pubkAlgTag == SEC_OID_UNKNOWN) || (digestalgtag == SEC_OID_UNKNOWN)) {
	vs = NSSCMSVS_SignatureAlgorithmUnknown;
	goto loser;
    }

#ifndef NSS_ECC_MORE_THAN_SUITE_B
    if (pubkAlgTag == SEC_OID_ANSIX962_EC_PUBLIC_KEY) {
	vs = NSSCMSVS_SignatureAlgorithmUnknown;
	goto loser;
    }
#endif

    if (!NSS_CMSArray_IsEmpty((void **)signerinfo->authAttr)) {
	if (contentType) {
	    /*
	     * Check content type
	     *
	     * RFC2630 sez that if there are any authenticated attributes,
	     * then there must be one for content type which matches the
	     * content type of the content being signed, and there must
	     * be one for message digest which matches our message digest.
	     * So check these things first.
	     */
	    attr = NSS_CMSAttributeArray_FindAttrByOidTag(signerinfo->authAttr,
					SEC_OID_PKCS9_CONTENT_TYPE, PR_TRUE);
	    if (attr == NULL) {
		vs = NSSCMSVS_MalformedSignature;
		goto loser;
	    }
		
	    if (NSS_CMSAttribute_CompareValue(attr, contentType) == PR_FALSE) {
		vs = NSSCMSVS_MalformedSignature;
		goto loser;
	    }
	}

	/*
	 * Check digest
	 */
	attr = NSS_CMSAttributeArray_FindAttrByOidTag(signerinfo->authAttr, 
	                              SEC_OID_PKCS9_MESSAGE_DIGEST, PR_TRUE);
	if (attr == NULL) {
	    vs = NSSCMSVS_MalformedSignature;
	    goto loser;
	}
	if (!digest || 
	    NSS_CMSAttribute_CompareValue(attr, digest) == PR_FALSE) {
	    vs = NSSCMSVS_DigestMismatch;
	    goto loser;
	}

	if ((poolp = PORT_NewArena (1024)) == NULL) {
	    vs = NSSCMSVS_ProcessingError;
	    goto loser;
	}

	/*
	 * Check signature
	 *
	 * The signature is based on a digest of the DER-encoded authenticated
	 * attributes.  So, first we encode and then we digest/verify.
	 * we trust the decoder to have the attributes in the right (sorted) 
	 * order
	 */
	encoded_attrs.data = NULL;
	encoded_attrs.len = 0;

	if (NSS_CMSAttributeArray_Encode(poolp, &(signerinfo->authAttr), 
	                                 &encoded_attrs) == NULL ||
		encoded_attrs.data == NULL || encoded_attrs.len == 0) {
	    vs = NSSCMSVS_ProcessingError;
	    goto loser;
	}

	vs = (VFY_VerifyDataDirect(encoded_attrs.data, encoded_attrs.len,
		publickey, &(signerinfo->encDigest), pubkAlgTag,
		digestalgtag, NULL, signerinfo->cmsg->pwfn_arg) != SECSuccess) 
		? NSSCMSVS_BadSignature : NSSCMSVS_GoodSignature;

	PORT_FreeArena(poolp, PR_FALSE);  /* awkward memory management :-( */

    } else {
	SECItem *sig;

	/* No authenticated attributes. 
	** The signature is based on the plain message digest. 
	*/
	sig = &(signerinfo->encDigest);
	if (sig->len == 0)
	    goto loser;

	vs = (!digest || 
	      VFY_VerifyDigestDirect(digest, publickey, sig, pubkAlgTag,
		digestalgtag, signerinfo->cmsg->pwfn_arg) != SECSuccess) 
		? NSSCMSVS_BadSignature : NSSCMSVS_GoodSignature;
    }

    if (vs == NSSCMSVS_BadSignature) {
	int error = PORT_GetError();
	/*
	 * XXX Change the generic error into our specific one, because
	 * in that case we get a better explanation out of the Security
	 * Advisor.  This is really a bug in the PSM error strings (the
	 * "generic" error has a lousy/wrong message associated with it
	 * which assumes the signature verification was done for the
	 * purposes of checking the issuer signature on a certificate)
	 * but this is at least an easy workaround and/or in the
	 * Security Advisor, which specifically checks for the error
	 * SEC_ERROR_PKCS7_BAD_SIGNATURE and gives more explanation
	 * in that case but does not similarly check for
	 * SEC_ERROR_BAD_SIGNATURE.  It probably should, but then would
	 * probably say the wrong thing in the case that it *was* the
	 * certificate signature check that failed during the cert
	 * verification done above.  Our error handling is really a mess.
	 */
	if (error == SEC_ERROR_BAD_SIGNATURE)
	    PORT_SetError(SEC_ERROR_PKCS7_BAD_SIGNATURE);
	/*
	 * map algorithm failures to NSSCMSVS values 
	 */
	if ((error == SEC_ERROR_PKCS7_KEYALG_MISMATCH) ||
	    (error == SEC_ERROR_INVALID_ALGORITHM)) {
	    /* keep the same error code as 3.11 and before */
	    PORT_SetError(SEC_ERROR_PKCS7_BAD_SIGNATURE);
	    vs = NSSCMSVS_SignatureAlgorithmUnsupported;
	}
    }

    if (publickey != NULL)
	SECKEY_DestroyPublicKey (publickey);

    signerinfo->verificationStatus = vs;

    return (vs == NSSCMSVS_GoodSignature) ? SECSuccess : SECFailure;

loser:
    if (publickey != NULL)
	SECKEY_DestroyPublicKey (publickey);

    signerinfo->verificationStatus = vs;

    PORT_SetError (SEC_ERROR_PKCS7_BAD_SIGNATURE);
    return SECFailure;
}

NSSCMSVerificationStatus
NSS_CMSSignerInfo_GetVerificationStatus(NSSCMSSignerInfo *signerinfo)
{
    return signerinfo->verificationStatus;
}

SECOidData *
NSS_CMSSignerInfo_GetDigestAlg(NSSCMSSignerInfo *signerinfo)
{
    return SECOID_FindOID (&(signerinfo->digestAlg.algorithm));
}

SECOidTag
NSS_CMSSignerInfo_GetDigestAlgTag(NSSCMSSignerInfo *signerinfo)
{
    SECOidData *algdata;

    if (!signerinfo) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SEC_OID_UNKNOWN;
    }

    algdata = SECOID_FindOID (&(signerinfo->digestAlg.algorithm));
    if (algdata != NULL)
	return algdata->offset;
    else
	return SEC_OID_UNKNOWN;
}

CERTCertificateList *
NSS_CMSSignerInfo_GetCertList(NSSCMSSignerInfo *signerinfo)
{
    return signerinfo->certList;
}

int
NSS_CMSSignerInfo_GetVersion(NSSCMSSignerInfo *signerinfo)
{
    unsigned long version;

    /* always take apart the SECItem */
    if (SEC_ASN1DecodeInteger(&(signerinfo->version), &version) != SECSuccess)
	return 0;
    else
	return (int)version;
}

/*
 * NSS_CMSSignerInfo_GetSigningTime - return the signing time,
 *				      in UTCTime or GeneralizedTime format,
 *                                    of a CMS signerInfo.
 *
 * sinfo - signerInfo data for this signer
 *
 * Returns a pointer to XXXX (what?)
 * A return value of NULL is an error.
 */
SECStatus
NSS_CMSSignerInfo_GetSigningTime(NSSCMSSignerInfo *sinfo, PRTime *stime)
{
    NSSCMSAttribute *attr;
    SECItem *value;

    if (sinfo == NULL)
	return SECFailure;

    if (sinfo->signingTime != 0) {
	*stime = sinfo->signingTime;	/* cached copy */
	return SECSuccess;
    }

    attr = NSS_CMSAttributeArray_FindAttrByOidTag(sinfo->authAttr, SEC_OID_PKCS9_SIGNING_TIME, PR_TRUE);
    /* XXXX multi-valued attributes NIH */
    if (attr == NULL || (value = NSS_CMSAttribute_GetValue(attr)) == NULL)
	return SECFailure;
    if (DER_DecodeTimeChoice(stime, value) != SECSuccess)
	return SECFailure;
    sinfo->signingTime = *stime;	/* make cached copy */
    return SECSuccess;
}

/*
 * Return the signing cert of a CMS signerInfo.
 *
 * the certs in the enclosing SignedData must have been imported already
 */
CERTCertificate *
NSS_CMSSignerInfo_GetSigningCertificate(NSSCMSSignerInfo *signerinfo, CERTCertDBHandle *certdb)
{
    CERTCertificate *cert;
    NSSCMSSignerIdentifier *sid;

    if (signerinfo->cert != NULL)
	return signerinfo->cert;

    /* no certdb, and cert hasn't been set yet? */
    if (certdb == NULL)
	return NULL;

    /*
     * This cert will also need to be freed, but since we save it
     * in signerinfo for later, we do not want to destroy it when
     * we leave this function -- we let the clean-up of the entire
     * cinfo structure later do the destroy of this cert.
     */
    sid = &signerinfo->signerIdentifier;
    switch (sid->identifierType) {
    case NSSCMSSignerID_IssuerSN:
	cert = CERT_FindCertByIssuerAndSN(certdb, sid->id.issuerAndSN);
	break;
    case NSSCMSSignerID_SubjectKeyID:
	cert = CERT_FindCertBySubjectKeyID(certdb, sid->id.subjectKeyID);
	break;
    default:
	cert = NULL;
	break;
    }

    /* cert can be NULL at that point */
    signerinfo->cert = cert;	/* earmark it */

    return cert;
}

/*
 * NSS_CMSSignerInfo_GetSignerCommonName - return the common name of the signer
 *
 * sinfo - signerInfo data for this signer
 *
 * Returns a pointer to allocated memory, which must be freed with PORT_Free.
 * A return value of NULL is an error.
 */
char *
NSS_CMSSignerInfo_GetSignerCommonName(NSSCMSSignerInfo *sinfo)
{
    CERTCertificate *signercert;

    /* will fail if cert is not verified */
    if ((signercert = NSS_CMSSignerInfo_GetSigningCertificate(sinfo, NULL)) == NULL)
	return NULL;

    return (CERT_GetCommonName(&signercert->subject));
}

/*
 * NSS_CMSSignerInfo_GetSignerEmailAddress - return the common name of the signer
 *
 * sinfo - signerInfo data for this signer
 *
 * Returns a pointer to allocated memory, which must be freed.
 * A return value of NULL is an error.
 */
char *
NSS_CMSSignerInfo_GetSignerEmailAddress(NSSCMSSignerInfo *sinfo)
{
    CERTCertificate *signercert;

    if ((signercert = NSS_CMSSignerInfo_GetSigningCertificate(sinfo, NULL)) == NULL)
	return NULL;

    if (!signercert->emailAddr || !signercert->emailAddr[0])
	return NULL;

    return (PORT_Strdup(signercert->emailAddr));
}

/*
 * NSS_CMSSignerInfo_AddAuthAttr - add an attribute to the
 * authenticated (i.e. signed) attributes of "signerinfo". 
 */
SECStatus
NSS_CMSSignerInfo_AddAuthAttr(NSSCMSSignerInfo *signerinfo, NSSCMSAttribute *attr)
{
    return NSS_CMSAttributeArray_AddAttr(signerinfo->cmsg->poolp, &(signerinfo->authAttr), attr);
}

/*
 * NSS_CMSSignerInfo_AddUnauthAttr - add an attribute to the
 * unauthenticated attributes of "signerinfo". 
 */
SECStatus
NSS_CMSSignerInfo_AddUnauthAttr(NSSCMSSignerInfo *signerinfo, NSSCMSAttribute *attr)
{
    return NSS_CMSAttributeArray_AddAttr(signerinfo->cmsg->poolp, &(signerinfo->unAuthAttr), attr);
}

/* 
 * NSS_CMSSignerInfo_AddSigningTime - add the signing time to the
 * authenticated (i.e. signed) attributes of "signerinfo". 
 *
 * This is expected to be included in outgoing signed
 * messages for email (S/MIME) but is likely useful in other situations.
 *
 * This should only be added once; a second call will do nothing.
 *
 * XXX This will probably just shove the current time into "signerinfo"
 * but it will not actually get signed until the entire item is
 * processed for encoding.  Is this (expected to be small) delay okay?
 */
SECStatus
NSS_CMSSignerInfo_AddSigningTime(NSSCMSSignerInfo *signerinfo, PRTime t)
{
    NSSCMSAttribute *attr;
    SECItem stime;
    void *mark;
    PLArenaPool *poolp;

    poolp = signerinfo->cmsg->poolp;

    mark = PORT_ArenaMark(poolp);

    /* create new signing time attribute */
    if (DER_EncodeTimeChoice(NULL, &stime, t) != SECSuccess)
	goto loser;

    if ((attr = NSS_CMSAttribute_Create(poolp, SEC_OID_PKCS9_SIGNING_TIME, &stime, PR_FALSE)) == NULL) {
	SECITEM_FreeItem (&stime, PR_FALSE);
	goto loser;
    }

    SECITEM_FreeItem (&stime, PR_FALSE);

    if (NSS_CMSSignerInfo_AddAuthAttr(signerinfo, attr) != SECSuccess)
	goto loser;

    PORT_ArenaUnmark (poolp, mark);

    return SECSuccess;

loser:
    PORT_ArenaRelease (poolp, mark);
    return SECFailure;
}

/* 
 * NSS_CMSSignerInfo_AddSMIMECaps - add a SMIMECapabilities attribute to the
 * authenticated (i.e. signed) attributes of "signerinfo". 
 *
 * This is expected to be included in outgoing signed
 * messages for email (S/MIME).
 */
SECStatus
NSS_CMSSignerInfo_AddSMIMECaps(NSSCMSSignerInfo *signerinfo)
{
    NSSCMSAttribute *attr;
    SECItem *smimecaps = NULL;
    void *mark;
    PLArenaPool *poolp;

    poolp = signerinfo->cmsg->poolp;

    mark = PORT_ArenaMark(poolp);

    smimecaps = SECITEM_AllocItem(poolp, NULL, 0);
    if (smimecaps == NULL)
	goto loser;

    /* create new signing time attribute */
    if (NSS_SMIMEUtil_CreateSMIMECapabilities(poolp, smimecaps,
			    PK11_FortezzaHasKEA(signerinfo->cert)) != SECSuccess)
	goto loser;

    if ((attr = NSS_CMSAttribute_Create(poolp, SEC_OID_PKCS9_SMIME_CAPABILITIES, smimecaps, PR_TRUE)) == NULL)
	goto loser;

    if (NSS_CMSSignerInfo_AddAuthAttr(signerinfo, attr) != SECSuccess)
	goto loser;

    PORT_ArenaUnmark (poolp, mark);
    return SECSuccess;

loser:
    PORT_ArenaRelease (poolp, mark);
    return SECFailure;
}

/* 
 * NSS_CMSSignerInfo_AddSMIMEEncKeyPrefs - add a SMIMEEncryptionKeyPreferences attribute to the
 * authenticated (i.e. signed) attributes of "signerinfo". 
 *
 * This is expected to be included in outgoing signed messages for email (S/MIME).
 */
SECStatus
NSS_CMSSignerInfo_AddSMIMEEncKeyPrefs(NSSCMSSignerInfo *signerinfo, CERTCertificate *cert, CERTCertDBHandle *certdb)
{
    NSSCMSAttribute *attr;
    SECItem *smimeekp = NULL;
    void *mark;
    PLArenaPool *poolp;

    /* verify this cert for encryption */
    if (CERT_VerifyCert(certdb, cert, PR_TRUE, certUsageEmailRecipient, PR_Now(), signerinfo->cmsg->pwfn_arg, NULL) != SECSuccess) {
	return SECFailure;
    }

    poolp = signerinfo->cmsg->poolp;
    mark = PORT_ArenaMark(poolp);

    smimeekp = SECITEM_AllocItem(poolp, NULL, 0);
    if (smimeekp == NULL)
	goto loser;

    /* create new signing time attribute */
    if (NSS_SMIMEUtil_CreateSMIMEEncKeyPrefs(poolp, smimeekp, cert) != SECSuccess)
	goto loser;

    if ((attr = NSS_CMSAttribute_Create(poolp, SEC_OID_SMIME_ENCRYPTION_KEY_PREFERENCE, smimeekp, PR_TRUE)) == NULL)
	goto loser;

    if (NSS_CMSSignerInfo_AddAuthAttr(signerinfo, attr) != SECSuccess)
	goto loser;

    PORT_ArenaUnmark (poolp, mark);
    return SECSuccess;

loser:
    PORT_ArenaRelease (poolp, mark);
    return SECFailure;
}

/* 
 * NSS_CMSSignerInfo_AddMSSMIMEEncKeyPrefs - add a SMIMEEncryptionKeyPreferences attribute to the
 * authenticated (i.e. signed) attributes of "signerinfo", using the OID preferred by Microsoft.
 *
 * This is expected to be included in outgoing signed messages for email (S/MIME),
 * if compatibility with Microsoft mail clients is wanted.
 */
SECStatus
NSS_CMSSignerInfo_AddMSSMIMEEncKeyPrefs(NSSCMSSignerInfo *signerinfo, CERTCertificate *cert, CERTCertDBHandle *certdb)
{
    NSSCMSAttribute *attr;
    SECItem *smimeekp = NULL;
    void *mark;
    PLArenaPool *poolp;

    /* verify this cert for encryption */
    if (CERT_VerifyCert(certdb, cert, PR_TRUE, certUsageEmailRecipient, PR_Now(), signerinfo->cmsg->pwfn_arg, NULL) != SECSuccess) {
	return SECFailure;
    }

    poolp = signerinfo->cmsg->poolp;
    mark = PORT_ArenaMark(poolp);

    smimeekp = SECITEM_AllocItem(poolp, NULL, 0);
    if (smimeekp == NULL)
	goto loser;

    /* create new signing time attribute */
    if (NSS_SMIMEUtil_CreateMSSMIMEEncKeyPrefs(poolp, smimeekp, cert) != SECSuccess)
	goto loser;

    if ((attr = NSS_CMSAttribute_Create(poolp, SEC_OID_MS_SMIME_ENCRYPTION_KEY_PREFERENCE, smimeekp, PR_TRUE)) == NULL)
	goto loser;

    if (NSS_CMSSignerInfo_AddAuthAttr(signerinfo, attr) != SECSuccess)
	goto loser;

    PORT_ArenaUnmark (poolp, mark);
    return SECSuccess;

loser:
    PORT_ArenaRelease (poolp, mark);
    return SECFailure;
}

/* 
 * NSS_CMSSignerInfo_AddCounterSignature - countersign a signerinfo
 *
 * 1. digest the DER-encoded signature value of the original signerinfo
 * 2. create new signerinfo with correct version, sid, digestAlg
 * 3. add message-digest authAttr, but NO content-type
 * 4. sign the authAttrs
 * 5. DER-encode the new signerInfo
 * 6. add the whole thing to original signerInfo's unAuthAttrs
 *    as a SEC_OID_PKCS9_COUNTER_SIGNATURE attribute
 *
 * XXXX give back the new signerinfo?
 */
SECStatus
NSS_CMSSignerInfo_AddCounterSignature(NSSCMSSignerInfo *signerinfo,
				    SECOidTag digestalg, CERTCertificate signingcert)
{
    /* XXXX TBD XXXX */
    return SECFailure;
}

/*
 * XXXX the following needs to be done in the S/MIME layer code
 * after signature of a signerinfo is verified
 */
SECStatus
NSS_SMIMESignerInfo_SaveSMIMEProfile(NSSCMSSignerInfo *signerinfo)
{
    CERTCertificate *cert = NULL;
    SECItem *profile = NULL;
    NSSCMSAttribute *attr;
    SECItem *stime = NULL;
    SECItem *ekp;
    CERTCertDBHandle *certdb;
    int save_error;
    SECStatus rv;
    PRBool must_free_cert = PR_FALSE;

    certdb = CERT_GetDefaultCertDB();

    /* sanity check - see if verification status is ok (unverified does not count...) */
    if (signerinfo->verificationStatus != NSSCMSVS_GoodSignature)
	return SECFailure;

    /* find preferred encryption cert */
    if (!NSS_CMSArray_IsEmpty((void **)signerinfo->authAttr) &&
	(attr = NSS_CMSAttributeArray_FindAttrByOidTag(signerinfo->authAttr,
			       SEC_OID_SMIME_ENCRYPTION_KEY_PREFERENCE, PR_TRUE)) != NULL)
    { /* we have a SMIME_ENCRYPTION_KEY_PREFERENCE attribute! */
	ekp = NSS_CMSAttribute_GetValue(attr);
	if (ekp == NULL)
	    return SECFailure;

	/* we assume that all certs coming with the message have been imported to the */
	/* temporary database */
	cert = NSS_SMIMEUtil_GetCertFromEncryptionKeyPreference(certdb, ekp);
	if (cert == NULL)
	    return SECFailure;
	must_free_cert = PR_TRUE;
    }

    if (cert == NULL) {
	/* no preferred cert found?
	 * find the cert the signerinfo is signed with instead */
	cert = NSS_CMSSignerInfo_GetSigningCertificate(signerinfo, certdb);
	if (cert == NULL || cert->emailAddr == NULL || !cert->emailAddr[0])
	    return SECFailure;
    }

    /* verify this cert for encryption (has been verified for signing so far) */
    /* don't verify this cert for encryption. It may just be a signing cert.
     * that's OK, we can still save the S/MIME profile. The encryption cert
     * should have already been saved */
#ifdef notdef
    if (CERT_VerifyCert(certdb, cert, PR_TRUE, certUsageEmailRecipient, PR_Now(), signerinfo->cmsg->pwfn_arg, NULL) != SECSuccess) {
	if (must_free_cert)
	    CERT_DestroyCertificate(cert);
	return SECFailure;
    }
#endif

    /* XXX store encryption cert permanently? */

    /*
     * Remember the current error set because we do not care about
     * anything set by the functions we are about to call.
     */
    save_error = PORT_GetError();

    if (!NSS_CMSArray_IsEmpty((void **)signerinfo->authAttr)) {
	attr = NSS_CMSAttributeArray_FindAttrByOidTag(signerinfo->authAttr,
				       SEC_OID_PKCS9_SMIME_CAPABILITIES,
				       PR_TRUE);
	profile = NSS_CMSAttribute_GetValue(attr);
	attr = NSS_CMSAttributeArray_FindAttrByOidTag(signerinfo->authAttr,
				       SEC_OID_PKCS9_SIGNING_TIME,
				       PR_TRUE);
	stime = NSS_CMSAttribute_GetValue(attr);
    }

    rv = CERT_SaveSMimeProfile (cert, profile, stime);
    if (must_free_cert)
	CERT_DestroyCertificate(cert);

    /*
     * Restore the saved error in case the calls above set a new
     * one that we do not actually care about.
     */
    PORT_SetError (save_error);

    return rv;
}

/*
 * NSS_CMSSignerInfo_IncludeCerts - set cert chain inclusion mode for this signer
 */
SECStatus
NSS_CMSSignerInfo_IncludeCerts(NSSCMSSignerInfo *signerinfo, NSSCMSCertChainMode cm, SECCertUsage usage)
{
    if (signerinfo->cert == NULL)
	return SECFailure;

    /* don't leak if we get called twice */
    if (signerinfo->certList != NULL) {
	CERT_DestroyCertificateList(signerinfo->certList);
	signerinfo->certList = NULL;
    }

    switch (cm) {
    case NSSCMSCM_None:
	signerinfo->certList = NULL;
	break;
    case NSSCMSCM_CertOnly:
	signerinfo->certList = CERT_CertListFromCert(signerinfo->cert);
	break;
    case NSSCMSCM_CertChain:
	signerinfo->certList = CERT_CertChainFromCert(signerinfo->cert, usage, PR_FALSE);
	break;
    case NSSCMSCM_CertChainWithRoot:
	signerinfo->certList = CERT_CertChainFromCert(signerinfo->cert, usage, PR_TRUE);
	break;
    }

    if (cm != NSSCMSCM_None && signerinfo->certList == NULL)
	return SECFailure;
    
    return SECSuccess;
}
