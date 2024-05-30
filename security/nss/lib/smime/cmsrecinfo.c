/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * CMS recipientInfo methods.
 */

#include "cmslocal.h"

#include "cert.h"
#include "keyhi.h"
#include "secasn1.h"
#include "secitem.h"
#include "secoid.h"
#include "pk11func.h"
#include "secerr.h"
#include "smime.h"

PRBool
nss_cmsrecipientinfo_usessubjectkeyid(NSSCMSRecipientInfo *ri)
{
    if (ri->recipientInfoType == NSSCMSRecipientInfoID_KeyTrans) {
        NSSCMSRecipientIdentifier *rid;
        rid = &ri->ri.keyTransRecipientInfo.recipientIdentifier;
        if (rid->identifierType == NSSCMSRecipientID_SubjectKeyID) {
            return PR_TRUE;
        }
    }
    return PR_FALSE;
}

/*
 * NOTE: fakeContent marks CMSMessage structure which is only used as a carrier
 * of pwfn_arg and arena pools. In an ideal world, NSSCMSMessage would not have
 * been exported, and we would have added an ordinary enum to handle this
 * check. Unfortunatly wo don't have that luxury so we are overloading the
 * contentTypeTag field. NO code should every try to interpret this content tag
 * as a real OID tag, or use any fields other than pwfn_arg or poolp of this
 * CMSMessage for that matter */
static const SECOidData fakeContent;
NSSCMSRecipientInfo *
nss_cmsrecipientinfo_create(NSSCMSMessage *cmsg,
                            NSSCMSRecipientIDSelector type,
                            CERTCertificate *cert,
                            SECKEYPublicKey *pubKey,
                            SECItem *subjKeyID,
                            void *pwfn_arg,
                            SECItem *DERinput)
{
    NSSCMSRecipientInfo *ri;
    void *mark;
    SECOidTag certalgtag;
    SECStatus rv = SECSuccess;
    NSSCMSRecipientEncryptedKey *rek;
    NSSCMSOriginatorIdentifierOrKey *oiok;
    unsigned long version;
    SECItem *dummy;
    PLArenaPool *poolp;
    CERTSubjectPublicKeyInfo *spki, *freeSpki = NULL;
    NSSCMSRecipientIdentifier *rid;
    extern const SEC_ASN1Template NSSCMSRecipientInfoTemplate[];

    if (!cmsg) {
        /* a CMSMessage wasn't supplied, create a fake one to hold the pwfunc
         * and a private arena pool */
        cmsg = NSS_CMSMessage_Create(NULL);
        cmsg->pwfn_arg = pwfn_arg;
        /* mark it as a special cms message */
        cmsg->contentInfo.contentTypeTag = (SECOidData *)&fakeContent;
    }

    poolp = cmsg->poolp;

    mark = PORT_ArenaMark(poolp);

    ri = (NSSCMSRecipientInfo *)PORT_ArenaZAlloc(poolp, sizeof(NSSCMSRecipientInfo));
    if (ri == NULL)
        goto loser;

    ri->cmsg = cmsg;

    if (DERinput) {
        /* decode everything from DER */
        SECItem newinput;
        rv = SECITEM_CopyItem(poolp, &newinput, DERinput);
        if (SECSuccess != rv)
            goto loser;
        rv = SEC_QuickDERDecodeItem(poolp, ri, NSSCMSRecipientInfoTemplate, &newinput);
        if (SECSuccess != rv)
            goto loser;
    }

    switch (type) {
        case NSSCMSRecipientID_IssuerSN: {
            ri->cert = CERT_DupCertificate(cert);
            if (NULL == ri->cert)
                goto loser;
            spki = &(cert->subjectPublicKeyInfo);
            break;
        }

        case NSSCMSRecipientID_SubjectKeyID: {
            PORT_Assert(pubKey);
            spki = freeSpki = SECKEY_CreateSubjectPublicKeyInfo(pubKey);
            break;
        }

        case NSSCMSRecipientID_BrandNew:
            goto done;
            break;

        default:
            /* unkown type */
            goto loser;
            break;
    }

    certalgtag = SECOID_GetAlgorithmTag(&(spki->algorithm));

    rid = &ri->ri.keyTransRecipientInfo.recipientIdentifier;

    // This switch must match the switch in NSS_CMSRecipient_IsSupported.
    switch (certalgtag) {
        case SEC_OID_PKCS1_RSA_ENCRYPTION:
            ri->recipientInfoType = NSSCMSRecipientInfoID_KeyTrans;
            rid->identifierType = type;
            if (type == NSSCMSRecipientID_IssuerSN) {
                rid->id.issuerAndSN = CERT_GetCertIssuerAndSN(poolp, cert);
                if (rid->id.issuerAndSN == NULL) {
                    break;
                }
            } else if (type == NSSCMSRecipientID_SubjectKeyID) {
                NSSCMSKeyTransRecipientInfoEx *riExtra;

                rid->id.subjectKeyID = PORT_ArenaNew(poolp, SECItem);
                if (rid->id.subjectKeyID == NULL) {
                    rv = SECFailure;
                    PORT_SetError(SEC_ERROR_NO_MEMORY);
                    break;
                }
                rv = SECITEM_CopyItem(poolp, rid->id.subjectKeyID, subjKeyID);
                if (rv != SECSuccess || rid->id.subjectKeyID->data == NULL) {
                    rv = SECFailure;
                    PORT_SetError(SEC_ERROR_NO_MEMORY);
                    break;
                }
                riExtra = &ri->ri.keyTransRecipientInfoEx;
                riExtra->version = 0;
                riExtra->pubKey = SECKEY_CopyPublicKey(pubKey);
                if (riExtra->pubKey == NULL) {
                    rv = SECFailure;
                    PORT_SetError(SEC_ERROR_NO_MEMORY);
                    break;
                }
            } else {
                PORT_SetError(SEC_ERROR_INVALID_ARGS);
                rv = SECFailure;
            }
            break;
        case SEC_OID_ANSIX962_EC_PUBLIC_KEY:
            PORT_Assert(type == NSSCMSRecipientID_IssuerSN);
            if (type != NSSCMSRecipientID_IssuerSN) {
                rv = SECFailure;
                break;
            }
            /* a key agreement op */
            ri->recipientInfoType = NSSCMSRecipientInfoID_KeyAgree;

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

done:
    PORT_ArenaUnmark(poolp, mark);
    if (freeSpki)
        SECKEY_DestroySubjectPublicKeyInfo(freeSpki);
    return ri;

loser:
    if (ri && ri->cert) {
        CERT_DestroyCertificate(ri->cert);
    }
    if (freeSpki) {
        SECKEY_DestroySubjectPublicKeyInfo(freeSpki);
    }
    PORT_ArenaRelease(poolp, mark);
    if (cmsg->contentInfo.contentTypeTag == &fakeContent) {
        NSS_CMSMessage_Destroy(cmsg);
    }
    return NULL;
}

/*
 * NSS_CMSRecipient_IsSupported - checks for a support certificate
 *
 * Use this function to confirm that the given certificate will be
 * accepted by NSS_CMSRecipientInfo_Create, which means that the
 * certificate can be used with a supported encryption algorithm.
 */
PRBool
NSS_CMSRecipient_IsSupported(CERTCertificate *cert)
{
    CERTSubjectPublicKeyInfo *spki = &(cert->subjectPublicKeyInfo);
    SECOidTag certalgtag = SECOID_GetAlgorithmTag(&(spki->algorithm));

    switch (certalgtag) {
        case SEC_OID_PKCS1_RSA_ENCRYPTION:
        case SEC_OID_ANSIX962_EC_PUBLIC_KEY:
            return PR_TRUE;
        default:
            return PR_FALSE;
    }
}

/*
 * NSS_CMSRecipientInfo_Create - create a recipientinfo
 *
 * we currently do not create KeyAgreement recipientinfos with multiple
 * recipientEncryptedKeys the certificate is supposed to have been
 * verified by the caller
 */
NSSCMSRecipientInfo *
NSS_CMSRecipientInfo_Create(NSSCMSMessage *cmsg, CERTCertificate *cert)
{
    return nss_cmsrecipientinfo_create(cmsg, NSSCMSRecipientID_IssuerSN, cert,
                                       NULL, NULL, NULL, NULL);
}

NSSCMSRecipientInfo *
NSS_CMSRecipientInfo_CreateNew(void *pwfn_arg)
{
    return nss_cmsrecipientinfo_create(NULL, NSSCMSRecipientID_BrandNew, NULL,
                                       NULL, NULL, pwfn_arg, NULL);
}

NSSCMSRecipientInfo *
NSS_CMSRecipientInfo_CreateFromDER(SECItem *input, void *pwfn_arg)
{
    return nss_cmsrecipientinfo_create(NULL, NSSCMSRecipientID_BrandNew, NULL,
                                       NULL, NULL, pwfn_arg, input);
}

NSSCMSRecipientInfo *
NSS_CMSRecipientInfo_CreateWithSubjKeyID(NSSCMSMessage *cmsg,
                                         SECItem *subjKeyID,
                                         SECKEYPublicKey *pubKey)
{
    return nss_cmsrecipientinfo_create(cmsg, NSSCMSRecipientID_SubjectKeyID,
                                       NULL, pubKey, subjKeyID, NULL, NULL);
}

NSSCMSRecipientInfo *
NSS_CMSRecipientInfo_CreateWithSubjKeyIDFromCert(NSSCMSMessage *cmsg,
                                                 CERTCertificate *cert)
{
    SECKEYPublicKey *pubKey = NULL;
    SECItem subjKeyID = { siBuffer, NULL, 0 };
    NSSCMSRecipientInfo *retVal = NULL;

    if (!cmsg || !cert) {
        return NULL;
    }
    pubKey = CERT_ExtractPublicKey(cert);
    if (!pubKey) {
        goto done;
    }
    if (CERT_FindSubjectKeyIDExtension(cert, &subjKeyID) != SECSuccess ||
        subjKeyID.data == NULL) {
        goto done;
    }
    retVal = NSS_CMSRecipientInfo_CreateWithSubjKeyID(cmsg, &subjKeyID, pubKey);
done:
    if (pubKey)
        SECKEY_DestroyPublicKey(pubKey);

    if (subjKeyID.data)
        SECITEM_FreeItem(&subjKeyID, PR_FALSE);

    return retVal;
}

void
NSS_CMSRecipientInfo_Destroy(NSSCMSRecipientInfo *ri)
{
    if (!ri) {
        return;
    }
    /* version was allocated on the pool, so no need to destroy it */
    /* issuerAndSN was allocated on the pool, so no need to destroy it */
    if (ri->cert != NULL)
        CERT_DestroyCertificate(ri->cert);

    if (nss_cmsrecipientinfo_usessubjectkeyid(ri)) {
        NSSCMSKeyTransRecipientInfoEx *extra;
        extra = &ri->ri.keyTransRecipientInfoEx;
        if (extra->pubKey)
            SECKEY_DestroyPublicKey(extra->pubKey);
    }
    if (ri->cmsg && ri->cmsg->contentInfo.contentTypeTag == &fakeContent) {
        NSS_CMSMessage_Destroy(ri->cmsg);
    }

    /* we're done. */
}

int
NSS_CMSRecipientInfo_GetVersion(NSSCMSRecipientInfo *ri)
{
    unsigned long version;
    SECItem *versionitem = NULL;

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

    PORT_Assert(versionitem);
    if (versionitem == NULL)
        return 0;

    /* always take apart the SECItem */
    if (SEC_ASN1DecodeInteger(versionitem, &version) != SECSuccess)
        return 0;
    else
        return (int)version;
}

SECItem *
NSS_CMSRecipientInfo_GetEncryptedKey(NSSCMSRecipientInfo *ri, int subIndex)
{
    SECItem *enckey = NULL;

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
    SECOidTag encalgtag = SEC_OID_UNKNOWN; /* an invalid encryption alg */

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
NSS_CMSRecipientInfo_WrapBulkKey(NSSCMSRecipientInfo *ri, PK11SymKey *bulkkey,
                                 SECOidTag bulkalgtag)
{
    CERTCertificate *cert;
    SECOidTag certalgtag;
    SECStatus rv = SECSuccess;
    NSSCMSRecipientEncryptedKey *rek;
    NSSCMSOriginatorIdentifierOrKey *oiok;
    CERTSubjectPublicKeyInfo *spki, *freeSpki = NULL;
    PLArenaPool *poolp;
    NSSCMSKeyTransRecipientInfoEx *extra = NULL;
    PRBool usesSubjKeyID;
    void *wincx = NULL;

    poolp = ri->cmsg->poolp;
    cert = ri->cert;
    usesSubjKeyID = nss_cmsrecipientinfo_usessubjectkeyid(ri);
    if (cert) {
        spki = &cert->subjectPublicKeyInfo;
    } else if (usesSubjKeyID) {
        extra = &ri->ri.keyTransRecipientInfoEx;
        /* sanity check */
        PORT_Assert(extra->pubKey);
        if (!extra->pubKey) {
            PORT_SetError(SEC_ERROR_INVALID_ARGS);
            return SECFailure;
        }
        spki = freeSpki = SECKEY_CreateSubjectPublicKeyInfo(extra->pubKey);
    } else {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    /* XXX set ri->recipientInfoType to the proper value here */
    /* or should we look if it's been set already ? */

    certalgtag = SECOID_GetAlgorithmTag(&spki->algorithm);
    if (!NSS_SMIMEUtil_KeyEncodingAllowed(&spki->algorithm, cert, extra ? extra->pubKey : NULL)) {
        PORT_SetError(SEC_ERROR_BAD_EXPORT_ALGORITHM);
        rv = SECFailure;
        goto loser;
    }
    switch (certalgtag) {
        case SEC_OID_PKCS1_RSA_ENCRYPTION:
            /* wrap the symkey */
            if (cert) {
                rv = NSS_CMSUtil_EncryptSymKey_RSA(poolp, cert, bulkkey,
                                                   &ri->ri.keyTransRecipientInfo.encKey);
                if (rv != SECSuccess)
                    break;
            } else if (usesSubjKeyID) {
                PORT_Assert(extra != NULL);
                rv = NSS_CMSUtil_EncryptSymKey_RSAPubKey(poolp, extra->pubKey,
                                                         bulkkey, &ri->ri.keyTransRecipientInfo.encKey);
                if (rv != SECSuccess)
                    break;
            }

            rv = SECOID_SetAlgorithmID(poolp, &(ri->ri.keyTransRecipientInfo.keyEncAlg), certalgtag, NULL);
            break;
        case SEC_OID_ANSIX962_EC_PUBLIC_KEY:
            rek = ri->ri.keyAgreeRecipientInfo.recipientEncryptedKeys[0];
            if (rek == NULL) {
                rv = SECFailure;
                break;
            }

            oiok = &(ri->ri.keyAgreeRecipientInfo.originatorIdentifierOrKey);
            PORT_Assert(oiok->identifierType == NSSCMSOriginatorIDOrKey_OriginatorPublicKey);

            /* see RFC2630 12.3.1.1 */
            if (SECOID_SetAlgorithmID(poolp, &oiok->id.originatorPublicKey.algorithmIdentifier,
                                      certalgtag, NULL) != SECSuccess) {
                rv = SECFailure;
                break;
            }

            /* this will generate a key pair, compute the shared secret, */
            /* derive a key and ukm for the keyEncAlg out of it, encrypt the bulk key with */
            /* the keyEncAlg, set encKey, keyEncAlg, publicKey etc. */
            switch (certalgtag) {
                case SEC_OID_ANSIX962_EC_PUBLIC_KEY:
                    if (ri->cmsg) {
                        wincx = ri->cmsg->pwfn_arg;
                    } else {
                        wincx = PK11_GetWindow(bulkkey);
                    }
                    rv = NSS_CMSUtil_EncryptSymKey_ESECDH(poolp, cert, bulkkey,
                                                          &rek->encKey,
                                                          PR_TRUE,
                                                          &ri->ri.keyAgreeRecipientInfo.ukm,
                                                          &ri->ri.keyAgreeRecipientInfo.keyEncAlg,
                                                          &oiok->id.originatorPublicKey.publicKey,
                                                          wincx);
                    break;

                default:
                    /* Not reached. Added to silence enum warnings. */
                    PORT_Assert(0);
                    break;
            }
            break;
        default:
            /* other algorithms not supported yet */
            /* NOTE that we do not support any KEK algorithm */
            PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
            rv = SECFailure;
    }
loser:
    if (freeSpki)
        SECKEY_DestroySubjectPublicKeyInfo(freeSpki);

    return rv;
}

PK11SymKey *
NSS_CMSRecipientInfo_UnwrapBulkKey(NSSCMSRecipientInfo *ri, int subIndex,
                                   CERTCertificate *cert, SECKEYPrivateKey *privkey, SECOidTag bulkalgtag)
{
    PK11SymKey *bulkkey = NULL;
    SECAlgorithmID *algid;
    SECOidTag encalgtag;
    SECItem *enckey = NULL, *ukm = NULL, *parameters = NULL;
    NSSCMSOriginatorIdentifierOrKey *oiok = NULL;
    int error;
    void *wincx = NULL;

    ri->cert = CERT_DupCertificate(cert);
    /* mark the recipientInfo so we can find it later */

    switch (ri->recipientInfoType) {
        case NSSCMSRecipientInfoID_KeyTrans:
            algid = &(ri->ri.keyTransRecipientInfo.keyEncAlg);
            parameters = &(ri->ri.keyTransRecipientInfo.keyEncAlg.parameters);
            enckey = &(ri->ri.keyTransRecipientInfo.encKey); /* ignore subIndex */
            break;
        case NSSCMSRecipientInfoID_KeyAgree:
            algid = &(ri->ri.keyAgreeRecipientInfo.keyEncAlg);
            parameters = &(ri->ri.keyAgreeRecipientInfo.keyEncAlg.parameters);
            enckey = &(ri->ri.keyAgreeRecipientInfo.recipientEncryptedKeys[subIndex]->encKey);
            oiok = &(ri->ri.keyAgreeRecipientInfo.originatorIdentifierOrKey);
            ukm = &(ri->ri.keyAgreeRecipientInfo.ukm);
            break;
        case NSSCMSRecipientInfoID_KEK:
            algid = &(ri->ri.kekRecipientInfo.keyEncAlg);
            parameters = &(ri->ri.kekRecipientInfo.keyEncAlg.parameters);
            enckey = &(ri->ri.kekRecipientInfo.encKey);
            /* not supported yet */
        default:
            error = SEC_ERROR_UNSUPPORTED_KEYALG;
            goto loser;
            break;
    }
    if (!NSS_SMIMEUtil_KeyDecodingAllowed(algid, privkey)) {
        error = SEC_ERROR_BAD_EXPORT_ALGORITHM;
        goto loser;
    }
    encalgtag = SECOID_GetAlgorithmTag(algid);
    switch (encalgtag) {
        case SEC_OID_PKCS1_RSA_ENCRYPTION:
            /* RSA encryption algorithm: */
            if (ri->recipientInfoType != NSSCMSRecipientInfoID_KeyTrans) {
                error = SEC_ERROR_UNSUPPORTED_KEYALG;
                goto loser;
            }
            /* get the symmetric (bulk) key by unwrapping it using our private key */
            bulkkey = NSS_CMSUtil_DecryptSymKey_RSA(privkey, enckey, bulkalgtag);
            break;
        case SEC_OID_PKCS1_RSA_OAEP_ENCRYPTION:
            /* RSA OAEP encryption algorithm: */
            if (ri->recipientInfoType != NSSCMSRecipientInfoID_KeyTrans) {
                error = SEC_ERROR_UNSUPPORTED_KEYALG;
                goto loser;
            }
            /* get the symmetric (bulk) key by unwrapping it using our private key */
            bulkkey = NSS_CMSUtil_DecryptSymKey_RSA_OAEP(privkey, parameters, enckey,
                                                         bulkalgtag);
            break;
        case SEC_OID_DHSINGLEPASS_STDDH_SHA1KDF_SCHEME:
        case SEC_OID_DHSINGLEPASS_STDDH_SHA224KDF_SCHEME:
        case SEC_OID_DHSINGLEPASS_STDDH_SHA256KDF_SCHEME:
        case SEC_OID_DHSINGLEPASS_STDDH_SHA384KDF_SCHEME:
        case SEC_OID_DHSINGLEPASS_STDDH_SHA512KDF_SCHEME:
        case SEC_OID_DHSINGLEPASS_COFACTORDH_SHA1KDF_SCHEME:
        case SEC_OID_DHSINGLEPASS_COFACTORDH_SHA224KDF_SCHEME:
        case SEC_OID_DHSINGLEPASS_COFACTORDH_SHA256KDF_SCHEME:
        case SEC_OID_DHSINGLEPASS_COFACTORDH_SHA384KDF_SCHEME:
        case SEC_OID_DHSINGLEPASS_COFACTORDH_SHA512KDF_SCHEME:
            if (ri->recipientInfoType != NSSCMSRecipientInfoID_KeyAgree) {
                error = SEC_ERROR_UNSUPPORTED_KEYALG;
                goto loser;
            }
            if (ri->cmsg) {
                wincx = ri->cmsg->pwfn_arg;
            }
            bulkkey = NSS_CMSUtil_DecryptSymKey_ECDH(privkey, enckey, algid,
                                                     bulkalgtag, ukm, oiok, wincx);
            break;
        default:
            error = SEC_ERROR_UNSUPPORTED_KEYALG;
            goto loser;
    }
    /* XXXX continue here */
    return bulkkey;

loser:
    PORT_SetError(error);
    return NULL;
}

SECStatus
NSS_CMSRecipientInfo_GetCertAndKey(NSSCMSRecipientInfo *ri,
                                   CERTCertificate **retcert,
                                   SECKEYPrivateKey **retkey)
{
    CERTCertificate *cert = NULL;
    NSSCMSRecipient **recipients = NULL;
    NSSCMSRecipientInfo *recipientInfos[2];
    SECStatus rv = SECSuccess;
    SECKEYPrivateKey *key = NULL;

    if (!ri)
        return SECFailure;

    if (!retcert && !retkey) {
        /* nothing requested, nothing found, success */
        return SECSuccess;
    }

    if (retcert) {
        *retcert = NULL;
    }
    if (retkey) {
        *retkey = NULL;
    }

    if (ri->cert) {
        cert = CERT_DupCertificate(ri->cert);
        if (!cert) {
            rv = SECFailure;
        }
    }
    if (SECSuccess == rv && !cert) {
        /* we don't have the cert, we have to look for it */
        /* first build an NSS_CMSRecipient */
        recipientInfos[0] = ri;
        recipientInfos[1] = NULL;

        recipients = nss_cms_recipient_list_create(recipientInfos);
        if (recipients) {
            /* now look for the cert and key */
            if (0 == PK11_FindCertAndKeyByRecipientListNew(recipients,
                                                           ri->cmsg->pwfn_arg)) {
                cert = CERT_DupCertificate(recipients[0]->cert);
                key = SECKEY_CopyPrivateKey(recipients[0]->privkey);
            } else {
                rv = SECFailure;
            }

            nss_cms_recipient_list_destroy(recipients);
        } else {
            rv = SECFailure;
        }
    } else if (SECSuccess == rv && cert && retkey) {
        /* we have the cert, we just need the key now */
        key = PK11_FindPrivateKeyFromCert(cert->slot, cert, ri->cmsg->pwfn_arg);
    }
    if (retcert) {
        *retcert = cert;
    } else {
        if (cert) {
            CERT_DestroyCertificate(cert);
        }
    }
    if (retkey) {
        *retkey = key;
    } else {
        if (key) {
            SECKEY_DestroyPrivateKey(key);
        }
    }

    return rv;
}

SECStatus
NSS_CMSRecipientInfo_Encode(PLArenaPool *poolp,
                            const NSSCMSRecipientInfo *src,
                            SECItem *returned)
{
    extern const SEC_ASN1Template NSSCMSRecipientInfoTemplate[];
    SECStatus rv = SECFailure;
    if (!src || !returned) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
    } else if (SEC_ASN1EncodeItem(poolp, returned, src,
                                  NSSCMSRecipientInfoTemplate)) {
        rv = SECSuccess;
    }
    return rv;
}
