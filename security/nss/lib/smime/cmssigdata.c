/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * CMS signedData methods.
 */

#include "cmslocal.h"

#include "cert.h"
/*#include "cdbhdl.h"*/
#include "secasn1.h"
#include "secitem.h"
#include "secoid.h"
#include "pk11func.h"
#include "secerr.h"

NSSCMSSignedData *
NSS_CMSSignedData_Create(NSSCMSMessage *cmsg)
{
    void *mark;
    NSSCMSSignedData *sigd;
    PLArenaPool *poolp;

    if (!cmsg) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return NULL;
    }

    poolp = cmsg->poolp;

    mark = PORT_ArenaMark(poolp);

    sigd = (NSSCMSSignedData *)PORT_ArenaZAlloc(poolp, sizeof(NSSCMSSignedData));
    if (sigd == NULL)
        goto loser;

    sigd->cmsg = cmsg;

    /* signerInfos, certs, certlists, crls are all empty */
    /* version is set in NSS_CMSSignedData_Finalize() */

    PORT_ArenaUnmark(poolp, mark);
    return sigd;

loser:
    PORT_ArenaRelease(poolp, mark);
    return NULL;
}

void
NSS_CMSSignedData_Destroy(NSSCMSSignedData *sigd)
{
    CERTCertificate **certs, **tempCerts, *cert;
    CERTCertificateList **certlists, *certlist;
    NSSCMSSignerInfo **signerinfos, *si;

    if (sigd == NULL)
        return;

    certs = sigd->certs;
    tempCerts = sigd->tempCerts;
    certlists = sigd->certLists;
    signerinfos = sigd->signerInfos;

    if (certs != NULL) {
        while ((cert = *certs++) != NULL)
            CERT_DestroyCertificate(cert);
    }

    if (tempCerts != NULL) {
        while ((cert = *tempCerts++) != NULL)
            CERT_DestroyCertificate(cert);
    }

    if (certlists != NULL) {
        while ((certlist = *certlists++) != NULL)
            CERT_DestroyCertificateList(certlist);
    }

    if (signerinfos != NULL) {
        while ((si = *signerinfos++) != NULL)
            NSS_CMSSignerInfo_Destroy(si);
    }

    /* everything's in a pool, so don't worry about the storage */
    NSS_CMSContentInfo_Destroy(&(sigd->contentInfo));
}

/*
 * NSS_CMSSignedData_Encode_BeforeStart - do all the necessary things to a SignedData
 *     before start of encoding.
 *
 * In detail:
 *  - find out about the right value to put into sigd->version
 *  - come up with a list of digestAlgorithms (which should be the union of the algorithms
 *         in the signerinfos).
 *         If we happen to have a pre-set list of algorithms (and digest values!), we
 *         check if we have all the signerinfos' algorithms. If not, this is an error.
 */
SECStatus
NSS_CMSSignedData_Encode_BeforeStart(NSSCMSSignedData *sigd)
{
    NSSCMSSignerInfo *signerinfo;
    SECOidTag digestalgtag;
    SECItem *dummy;
    int version;
    SECStatus rv;
    PRBool haveDigests = PR_FALSE;
    int n, i;
    PLArenaPool *poolp;

    if (!sigd) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    poolp = sigd->cmsg->poolp;

    /* we assume that we have precomputed digests if there is a list of algorithms, and */
    /* a chunk of data for each of those algorithms */
    if (sigd->digestAlgorithms != NULL && sigd->digests != NULL) {
        for (i = 0; sigd->digestAlgorithms[i] != NULL; i++) {
            if (sigd->digests[i] == NULL)
                break;
        }
        if (sigd->digestAlgorithms[i] == NULL) /* reached the end of the array? */
            haveDigests = PR_TRUE;             /* yes: we must have all the digests */
    }

    version = NSS_CMS_SIGNED_DATA_VERSION_BASIC;

    /* RFC2630 5.1 "version is the syntax version number..." */
    if (NSS_CMSContentInfo_GetContentTypeTag(&(sigd->contentInfo)) != SEC_OID_PKCS7_DATA)
        version = NSS_CMS_SIGNED_DATA_VERSION_EXT;

    /* prepare all the SignerInfos (there may be none) */
    for (i = 0; i < NSS_CMSSignedData_SignerInfoCount(sigd); i++) {
        signerinfo = NSS_CMSSignedData_GetSignerInfo(sigd, i);

        /* RFC2630 5.1 "version is the syntax version number..." */
        if (NSS_CMSSignerInfo_GetVersion(signerinfo) != NSS_CMS_SIGNER_INFO_VERSION_ISSUERSN)
            version = NSS_CMS_SIGNED_DATA_VERSION_EXT;

        /* collect digestAlgorithms from SignerInfos */
        /* (we need to know which algorithms we have when the content comes in) */
        /* do not overwrite any existing digestAlgorithms (and digest) */
        digestalgtag = NSS_CMSSignerInfo_GetDigestAlgTag(signerinfo);
        n = NSS_CMSAlgArray_GetIndexByAlgTag(sigd->digestAlgorithms, digestalgtag);
        if (n < 0 && haveDigests) {
            /* oops, there is a digestalg we do not have a digest for */
            /* but we were supposed to have all the digests already... */
            goto loser;
        } else if (n < 0) {
            /* add the digestAlgorithm & a NULL digest */
            rv = NSS_CMSSignedData_AddDigest(poolp, sigd, digestalgtag, NULL);
            if (rv != SECSuccess)
                goto loser;
        } else {
            /* found it, nothing to do */
        }
    }

    dummy = SEC_ASN1EncodeInteger(poolp, &(sigd->version), (long)version);
    if (dummy == NULL)
        return SECFailure;

    /* this is a SET OF, so we need to sort them guys */
    rv = NSS_CMSArray_SortByDER((void **)sigd->digestAlgorithms,
                                SEC_ASN1_GET(SECOID_AlgorithmIDTemplate),
                                (void **)sigd->digests);
    if (rv != SECSuccess)
        return SECFailure;

    return SECSuccess;

loser:
    return SECFailure;
}

SECStatus
NSS_CMSSignedData_Encode_BeforeData(NSSCMSSignedData *sigd)
{
    SECStatus rv;
    if (!sigd) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }
    rv = NSS_CMSContentInfo_Private_Init(&sigd->contentInfo);
    if (rv != SECSuccess) {
        return SECFailure;
    }
    /* set up the digests */
    if (sigd->digests && sigd->digests[0]) {
        sigd->contentInfo.privateInfo->digcx = NULL; /* don't attempt to make new ones. */
    } else if (sigd->digestAlgorithms != NULL) {
        sigd->contentInfo.privateInfo->digcx =
            NSS_CMSDigestContext_StartMultiple(sigd->digestAlgorithms);
        if (sigd->contentInfo.privateInfo->digcx == NULL)
            return SECFailure;
    }
    return SECSuccess;
}

/*
 * NSS_CMSSignedData_Encode_AfterData - do all the necessary things to a SignedData
 *     after all the encapsulated data was passed through the encoder.
 *
 * In detail:
 *  - create the signatures in all the SignerInfos
 *
 * Please note that nothing is done to the Certificates and CRLs in the message - this
 * is entirely the responsibility of our callers.
 */
SECStatus
NSS_CMSSignedData_Encode_AfterData(NSSCMSSignedData *sigd)
{
    NSSCMSSignerInfo **signerinfos, *signerinfo;
    NSSCMSContentInfo *cinfo;
    SECOidTag digestalgtag;
    SECStatus ret = SECFailure;
    SECStatus rv;
    SECItem *contentType;
    int certcount;
    int i, ci, cli, n, rci, si;
    PLArenaPool *poolp;
    CERTCertificateList *certlist;
    extern const SEC_ASN1Template NSSCMSSignerInfoTemplate[];

    if (!sigd) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    poolp = sigd->cmsg->poolp;
    cinfo = &(sigd->contentInfo);

    /* did we have digest calculation going on? */
    if (cinfo->privateInfo && cinfo->privateInfo->digcx) {
        rv = NSS_CMSDigestContext_FinishMultiple(cinfo->privateInfo->digcx, poolp,
                                                 &(sigd->digests));
        /* error has been set by NSS_CMSDigestContext_FinishMultiple */
        cinfo->privateInfo->digcx = NULL;
        if (rv != SECSuccess)
            goto loser;
    }

    signerinfos = sigd->signerInfos;
    certcount = 0;

    /* prepare all the SignerInfos (there may be none) */
    for (i = 0; i < NSS_CMSSignedData_SignerInfoCount(sigd); i++) {
        signerinfo = NSS_CMSSignedData_GetSignerInfo(sigd, i);

        /* find correct digest for this signerinfo */
        digestalgtag = NSS_CMSSignerInfo_GetDigestAlgTag(signerinfo);
        n = NSS_CMSAlgArray_GetIndexByAlgTag(sigd->digestAlgorithms, digestalgtag);
        if (n < 0 || sigd->digests == NULL || sigd->digests[n] == NULL) {
            /* oops - digest not found */
            PORT_SetError(SEC_ERROR_DIGEST_NOT_FOUND);
            goto loser;
        }

        /* XXX if our content is anything else but data, we need to force the
         * presence of signed attributes (RFC2630 5.3 "signedAttributes is a
         * collection...") */

        /* pass contentType here as we want a contentType attribute */
        if ((contentType = NSS_CMSContentInfo_GetContentTypeOID(cinfo)) == NULL)
            goto loser;

        /* sign the thing */
        rv = NSS_CMSSignerInfo_Sign(signerinfo, sigd->digests[n], contentType);
        if (rv != SECSuccess)
            goto loser;

        /* while we're at it, count number of certs in certLists */
        certlist = NSS_CMSSignerInfo_GetCertList(signerinfo);
        if (certlist)
            certcount += certlist->len;
    }

    /* this is a SET OF, so we need to sort them guys */
    rv = NSS_CMSArray_SortByDER((void **)signerinfos, NSSCMSSignerInfoTemplate, NULL);
    if (rv != SECSuccess)
        goto loser;

    /*
     * now prepare certs & crls
     */

    /* count the rest of the certs */
    if (sigd->certs != NULL) {
        for (ci = 0; sigd->certs[ci] != NULL; ci++)
            certcount++;
    }

    if (sigd->certLists != NULL) {
        for (cli = 0; sigd->certLists[cli] != NULL; cli++)
            certcount += sigd->certLists[cli]->len;
    }

    if (certcount == 0) {
        sigd->rawCerts = NULL;
    } else {
        /*
         * Combine all of the certs and cert chains into rawcerts.
         * Note: certcount is an upper bound; we may not need that many slots
         * but we will allocate anyway to avoid having to do another pass.
         * (The temporary space saving is not worth it.)
         *
         * XXX ARGH - this NEEDS to be fixed. need to come up with a decent
         *  SetOfDERcertficates implementation
         */
        sigd->rawCerts = (SECItem **)PORT_ArenaAlloc(poolp, (certcount + 1) * sizeof(SECItem *));
        if (sigd->rawCerts == NULL)
            return SECFailure;

        /*
         * XXX Want to check for duplicates and not add *any* cert that is
         * already in the set.  This will be more important when we start
         * dealing with larger sets of certs, dual-key certs (signing and
         * encryption), etc.  For the time being we can slide by...
         *
         * XXX ARGH - this NEEDS to be fixed. need to come up with a decent
         *  SetOfDERcertficates implementation
         */
        rci = 0;
        if (signerinfos != NULL) {
            for (si = 0; signerinfos[si] != NULL; si++) {
                signerinfo = signerinfos[si];
                for (ci = 0; ci < signerinfo->certList->len; ci++)
                    sigd->rawCerts[rci++] = &(signerinfo->certList->certs[ci]);
            }
        }

        if (sigd->certs != NULL) {
            for (ci = 0; sigd->certs[ci] != NULL; ci++)
                sigd->rawCerts[rci++] = &(sigd->certs[ci]->derCert);
        }

        if (sigd->certLists != NULL) {
            for (cli = 0; sigd->certLists[cli] != NULL; cli++) {
                for (ci = 0; ci < sigd->certLists[cli]->len; ci++)
                    sigd->rawCerts[rci++] = &(sigd->certLists[cli]->certs[ci]);
            }
        }

        sigd->rawCerts[rci] = NULL;

        /* this is a SET OF, so we need to sort them guys - we have the DER already, though */
        NSS_CMSArray_Sort((void **)sigd->rawCerts, NSS_CMSUtil_DERCompare, NULL, NULL);
    }

    ret = SECSuccess;

loser:
    return ret;
}

SECStatus
NSS_CMSSignedData_Decode_BeforeData(NSSCMSSignedData *sigd)
{
    SECStatus rv;
    if (!sigd) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }
    rv = NSS_CMSContentInfo_Private_Init(&sigd->contentInfo);
    if (rv != SECSuccess) {
        return SECFailure;
    }
    /* handle issue with Windows 2003 servers and kerberos */
    if (sigd->digestAlgorithms != NULL) {
        int i;
        for (i = 0; sigd->digestAlgorithms[i] != NULL; i++) {
            SECAlgorithmID *algid = sigd->digestAlgorithms[i];
            SECOidTag senttag = SECOID_FindOIDTag(&algid->algorithm);
            SECOidTag maptag = NSS_CMSUtil_MapSignAlgs(senttag);

            if (maptag != senttag) {
                SECOidData *hashoid = SECOID_FindOIDByTag(maptag);
                rv = SECITEM_CopyItem(sigd->cmsg->poolp, &algid->algorithm, &hashoid->oid);
                if (rv != SECSuccess) {
                    return rv;
                }
            }
        }
    }

    /* set up the digests */
    if (sigd->digestAlgorithms != NULL && sigd->digests == NULL) {
        /* if digests are already there, do nothing */
        sigd->contentInfo.privateInfo->digcx = NSS_CMSDigestContext_StartMultiple(sigd->digestAlgorithms);
        if (sigd->contentInfo.privateInfo->digcx == NULL)
            return SECFailure;
    }
    return SECSuccess;
}

/*
 * NSS_CMSSignedData_Decode_AfterData - do all the necessary things to a
 *   SignedData after all the encapsulated data was passed through the decoder.
 */
SECStatus
NSS_CMSSignedData_Decode_AfterData(NSSCMSSignedData *sigd)
{
    SECStatus rv = SECSuccess;

    if (!sigd) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    /* did we have digest calculation going on? */
    if (sigd->contentInfo.privateInfo && sigd->contentInfo.privateInfo->digcx) {
        rv = NSS_CMSDigestContext_FinishMultiple(sigd->contentInfo.privateInfo->digcx,
                                                 sigd->cmsg->poolp, &(sigd->digests));
        /* error set by NSS_CMSDigestContext_FinishMultiple */
        sigd->contentInfo.privateInfo->digcx = NULL;
    }
    return rv;
}

/*
 * NSS_CMSSignedData_Decode_AfterEnd - do all the necessary things to a SignedData
 *     after all decoding is finished.
 */
SECStatus
NSS_CMSSignedData_Decode_AfterEnd(NSSCMSSignedData *sigd)
{
    NSSCMSSignerInfo **signerinfos = NULL;
    int i;

    if (!sigd) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    /* set cmsg for all the signerinfos */
    signerinfos = sigd->signerInfos;

    /* set cmsg for all the signerinfos */
    if (signerinfos) {
        for (i = 0; signerinfos[i] != NULL; i++)
            signerinfos[i]->cmsg = sigd->cmsg;
    }

    return SECSuccess;
}

/*
 * NSS_CMSSignedData_GetSignerInfos - retrieve the SignedData's signer list
 */
NSSCMSSignerInfo **
NSS_CMSSignedData_GetSignerInfos(NSSCMSSignedData *sigd)
{
    if (!sigd) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return NULL;
    }
    return sigd->signerInfos;
}

int
NSS_CMSSignedData_SignerInfoCount(NSSCMSSignedData *sigd)
{
    if (!sigd) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return 0;
    }
    return NSS_CMSArray_Count((void **)sigd->signerInfos);
}

NSSCMSSignerInfo *
NSS_CMSSignedData_GetSignerInfo(NSSCMSSignedData *sigd, int i)
{
    if (!sigd) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return NULL;
    }
    return sigd->signerInfos[i];
}

/*
 * NSS_CMSSignedData_GetDigestAlgs - retrieve the SignedData's digest algorithm list
 */
SECAlgorithmID **
NSS_CMSSignedData_GetDigestAlgs(NSSCMSSignedData *sigd)
{
    if (!sigd) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return NULL;
    }
    return sigd->digestAlgorithms;
}

/*
 * NSS_CMSSignedData_GetContentInfo - return pointer to this signedData's contentinfo
 */
NSSCMSContentInfo *
NSS_CMSSignedData_GetContentInfo(NSSCMSSignedData *sigd)
{
    if (!sigd) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return NULL;
    }
    return &(sigd->contentInfo);
}

/*
 * NSS_CMSSignedData_GetCertificateList - retrieve the SignedData's certificate list
 */
SECItem **
NSS_CMSSignedData_GetCertificateList(NSSCMSSignedData *sigd)
{
    if (!sigd) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return NULL;
    }
    return sigd->rawCerts;
}

SECStatus
NSS_CMSSignedData_ImportCerts(NSSCMSSignedData *sigd, CERTCertDBHandle *certdb,
                              SECCertUsage certusage, PRBool keepcerts)
{
    int certcount;
    CERTCertificate **certArray = NULL;
    CERTCertList *certList = NULL;
    CERTCertListNode *node;
    SECStatus rv;
    SECItem **rawArray;
    int i;
    PRTime now;

    if (!sigd) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    certcount = NSS_CMSArray_Count((void **)sigd->rawCerts);

    /* get the certs in the temp DB */
    rv = CERT_ImportCerts(certdb, certusage, certcount, sigd->rawCerts,
                          &certArray, PR_FALSE, PR_FALSE, NULL);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* save the certs so they don't get destroyed */
    for (i = 0; i < certcount; i++) {
        CERTCertificate *cert = certArray[i];
        if (cert)
            NSS_CMSSignedData_AddTempCertificate(sigd, cert);
    }

    if (!keepcerts) {
        goto done;
    }

    /* build a CertList for filtering */
    certList = CERT_NewCertList();
    if (certList == NULL) {
        rv = SECFailure;
        goto loser;
    }
    for (i = 0; i < certcount; i++) {
        CERTCertificate *cert = certArray[i];
        if (cert)
            cert = CERT_DupCertificate(cert);
        if (cert)
            CERT_AddCertToListTail(certList, cert);
    }

    /* filter out the certs we don't want */
    rv = CERT_FilterCertListByUsage(certList, certusage, PR_FALSE);
    if (rv != SECSuccess) {
        goto loser;
    }

    /* go down the remaining list of certs and verify that they have
     * valid chains, then import them.
     */
    now = PR_Now();
    for (node = CERT_LIST_HEAD(certList); !CERT_LIST_END(node, certList);
         node = CERT_LIST_NEXT(node)) {
        CERTCertificateList *certChain;

        if (CERT_VerifyCert(certdb, node->cert,
                            PR_TRUE, certusage, now, NULL, NULL) != SECSuccess) {
            continue;
        }

        certChain = CERT_CertChainFromCert(node->cert, certusage, PR_FALSE);
        if (!certChain) {
            continue;
        }

        /*
         * CertChain returns an array of SECItems, import expects an array of
         * SECItem pointers. Create the SECItem Pointers from the array of
         * SECItems.
         */
        rawArray = (SECItem **)PORT_Alloc(certChain->len * sizeof(SECItem *));
        if (!rawArray) {
            CERT_DestroyCertificateList(certChain);
            continue;
        }
        for (i = 0; i < certChain->len; i++) {
            rawArray[i] = &certChain->certs[i];
        }
        (void)CERT_ImportCerts(certdb, certusage, certChain->len,
                               rawArray, NULL, keepcerts, PR_FALSE, NULL);
        PORT_Free(rawArray);
        CERT_DestroyCertificateList(certChain);
    }

    rv = SECSuccess;

/* XXX CRL handling */

done:
    if (sigd->signerInfos != NULL) {
        /* fill in all signerinfo's certs */
        for (i = 0; sigd->signerInfos[i] != NULL; i++)
            (void)NSS_CMSSignerInfo_GetSigningCertificate(
                sigd->signerInfos[i], certdb);
    }

loser:
    /* now free everything */
    if (certArray) {
        CERT_DestroyCertArray(certArray, certcount);
    }
    if (certList) {
        CERT_DestroyCertList(certList);
    }

    return rv;
}

/*
 * XXX the digests need to be passed in BETWEEN the decoding and the verification in case
 *     of external signatures!
 */

/*
 * NSS_CMSSignedData_VerifySignerInfo - check the signatures.
 *
 * The digests were either calculated during decoding (and are stored in the
 * signedData itself) or set after decoding using NSS_CMSSignedData_SetDigests.
 *
 * The verification checks if the signing cert is valid and has a trusted chain
 * for the purpose specified by "certusage".
 */
SECStatus
NSS_CMSSignedData_VerifySignerInfo(NSSCMSSignedData *sigd, int i,
                                   CERTCertDBHandle *certdb, SECCertUsage certusage)
{
    NSSCMSSignerInfo *signerinfo;
    NSSCMSContentInfo *cinfo;
    SECOidData *algiddata;
    SECItem *contentType, *digest;
    SECOidTag oidTag;
    SECStatus rv;

    if (!sigd) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    cinfo = &(sigd->contentInfo);

    signerinfo = sigd->signerInfos[i];

    /* verify certificate */
    rv = NSS_CMSSignerInfo_VerifyCertificate(signerinfo, certdb, certusage);
    if (rv != SECSuccess)
        return rv; /* error is set */

    /* find digest and contentType for signerinfo */
    algiddata = NSS_CMSSignerInfo_GetDigestAlg(signerinfo);
    oidTag = algiddata ? algiddata->offset : SEC_OID_UNKNOWN;
    digest = NSS_CMSSignedData_GetDigestValue(sigd, oidTag);
    /* NULL digest is acceptable. */
    contentType = NSS_CMSContentInfo_GetContentTypeOID(cinfo);
    /* NULL contentType is acceptable. */

    /* now verify signature */
    rv = NSS_CMSSignerInfo_Verify(signerinfo, digest, contentType);
    return rv;
}

/*
 * NSS_CMSSignedData_VerifyCertsOnly - verify the certs in a certs-only message
 */
SECStatus
NSS_CMSSignedData_VerifyCertsOnly(NSSCMSSignedData *sigd,
                                  CERTCertDBHandle *certdb,
                                  SECCertUsage usage)
{
    CERTCertificate *cert;
    SECStatus rv = SECSuccess;
    int i;
    int count;
    PRTime now;

    if (!sigd || !certdb || !sigd->rawCerts) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    count = NSS_CMSArray_Count((void **)sigd->rawCerts);
    now = PR_Now();
    for (i = 0; i < count; i++) {
        if (sigd->certs && sigd->certs[i]) {
            cert = CERT_DupCertificate(sigd->certs[i]);
        } else {
            cert = CERT_FindCertByDERCert(certdb, sigd->rawCerts[i]);
            if (!cert) {
                rv = SECFailure;
                break;
            }
        }
        rv |= CERT_VerifyCert(certdb, cert, PR_TRUE, usage, now,
                              NULL, NULL);
        CERT_DestroyCertificate(cert);
    }

    return rv;
}

/*
 * NSS_CMSSignedData_HasDigests - see if we have digests in place
 */
PRBool
NSS_CMSSignedData_HasDigests(NSSCMSSignedData *sigd)
{
    if (!sigd) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return PR_FALSE;
    }
    return (sigd->digests != NULL);
}

SECStatus
NSS_CMSSignedData_AddCertList(NSSCMSSignedData *sigd, CERTCertificateList *certlist)
{
    SECStatus rv;

    if (!sigd || !certlist) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    /* XXX memory?? a certlist has an arena of its own and is not refcounted!?!? */
    rv = NSS_CMSArray_Add(sigd->cmsg->poolp, (void ***)&(sigd->certLists), (void *)certlist);

    return rv;
}

/*
 * NSS_CMSSignedData_AddCertChain - add cert and its entire chain to the set of certs
 */
SECStatus
NSS_CMSSignedData_AddCertChain(NSSCMSSignedData *sigd, CERTCertificate *cert)
{
    CERTCertificateList *certlist;
    SECCertUsage usage;
    SECStatus rv;

    usage = certUsageEmailSigner;

    if (!sigd || !cert) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    /* do not include root */
    certlist = CERT_CertChainFromCert(cert, usage, PR_FALSE);
    if (certlist == NULL)
        return SECFailure;

    rv = NSS_CMSSignedData_AddCertList(sigd, certlist);

    return rv;
}

extern SECStatus
NSS_CMSSignedData_AddTempCertificate(NSSCMSSignedData *sigd, CERTCertificate *cert)
{
    CERTCertificate *c;
    SECStatus rv;

    if (!sigd || !cert) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    c = CERT_DupCertificate(cert);
    rv = NSS_CMSArray_Add(sigd->cmsg->poolp, (void ***)&(sigd->tempCerts), (void *)c);
    return rv;
}

SECStatus
NSS_CMSSignedData_AddCertificate(NSSCMSSignedData *sigd, CERTCertificate *cert)
{
    CERTCertificate *c;
    SECStatus rv;

    if (!sigd || !cert) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    c = CERT_DupCertificate(cert);
    rv = NSS_CMSArray_Add(sigd->cmsg->poolp, (void ***)&(sigd->certs), (void *)c);
    return rv;
}

PRBool
NSS_CMSSignedData_ContainsCertsOrCrls(NSSCMSSignedData *sigd)
{
    if (!sigd) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return PR_FALSE;
    }
    if (sigd->rawCerts != NULL && sigd->rawCerts[0] != NULL)
        return PR_TRUE;
    else if (sigd->crls != NULL && sigd->crls[0] != NULL)
        return PR_TRUE;
    else
        return PR_FALSE;
}

SECStatus
NSS_CMSSignedData_AddSignerInfo(NSSCMSSignedData *sigd,
                                NSSCMSSignerInfo *signerinfo)
{
    void *mark;
    SECStatus rv;
    SECOidTag digestalgtag;
    PLArenaPool *poolp;

    if (!sigd || !signerinfo) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    poolp = sigd->cmsg->poolp;

    mark = PORT_ArenaMark(poolp);

    /* add signerinfo */
    rv = NSS_CMSArray_Add(poolp, (void ***)&(sigd->signerInfos), (void *)signerinfo);
    if (rv != SECSuccess)
        goto loser;

    /*
     * add empty digest
     * Empty because we don't have it yet. Either it gets created during encoding
     * (if the data is present) or has to be set externally.
     * XXX maybe pass it in optionally?
     */
    digestalgtag = NSS_CMSSignerInfo_GetDigestAlgTag(signerinfo);
    rv = NSS_CMSSignedData_SetDigestValue(sigd, digestalgtag, NULL);
    if (rv != SECSuccess)
        goto loser;

    /*
     * The last thing to get consistency would be adding the digest.
     */

    PORT_ArenaUnmark(poolp, mark);
    return SECSuccess;

loser:
    PORT_ArenaRelease(poolp, mark);
    return SECFailure;
}

/*
 * NSS_CMSSignedData_SetDigests - set a signedData's digests member
 *
 * "digestalgs" - array of digest algorithm IDs
 * "digests"    - array of digests corresponding to the digest algorithms
 */
SECStatus
NSS_CMSSignedData_SetDigests(NSSCMSSignedData *sigd,
                             SECAlgorithmID **digestalgs,
                             SECItem **digests)
{
    int cnt, i, idx;

    if (!sigd || !digestalgs || !digests) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    if (sigd->digestAlgorithms == NULL) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    /* we assume that the digests array is just not there yet */
    PORT_Assert(sigd->digests == NULL);
    if (sigd->digests != NULL) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        return SECFailure;
    }

    /* now allocate one (same size as digestAlgorithms) */
    cnt = NSS_CMSArray_Count((void **)sigd->digestAlgorithms);
    sigd->digests = PORT_ArenaZAlloc(sigd->cmsg->poolp, (cnt + 1) * sizeof(SECItem *));
    if (sigd->digests == NULL) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return SECFailure;
    }

    for (i = 0; sigd->digestAlgorithms[i] != NULL; i++) {
        /* try to find the sigd's i'th digest algorithm in the array we passed in */
        idx = NSS_CMSAlgArray_GetIndexByAlgID(digestalgs, sigd->digestAlgorithms[i]);
        if (idx < 0) {
            PORT_SetError(SEC_ERROR_DIGEST_NOT_FOUND);
            return SECFailure;
        }
        if (!digests[idx]) {
            /* We have no digest for this algorithm, probably because it is
            ** unrecognized or unsupported.  We'll ignore this here.  If this
            ** digest is needed later, an error will be be generated then.
            */
            continue;
        }

        /* found it - now set it */
        if ((sigd->digests[i] = SECITEM_AllocItem(sigd->cmsg->poolp, NULL, 0)) == NULL ||
            SECITEM_CopyItem(sigd->cmsg->poolp, sigd->digests[i], digests[idx]) != SECSuccess) {
            PORT_SetError(SEC_ERROR_NO_MEMORY);
            return SECFailure;
        }
    }
    return SECSuccess;
}

SECStatus
NSS_CMSSignedData_SetDigestValue(NSSCMSSignedData *sigd,
                                 SECOidTag digestalgtag,
                                 SECItem *digestdata)
{
    SECItem *digest = NULL;
    PLArenaPool *poolp;
    void *mark;
    int n, cnt;

    if (!sigd) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    poolp = sigd->cmsg->poolp;

    mark = PORT_ArenaMark(poolp);

    if (digestdata) {
        digest = (SECItem *)PORT_ArenaZAlloc(poolp, sizeof(SECItem));

        /* copy digestdata item to arena (in case we have it and are not only making room) */
        if (SECITEM_CopyItem(poolp, digest, digestdata) != SECSuccess)
            goto loser;
    }

    /* now allocate one (same size as digestAlgorithms) */
    if (sigd->digests == NULL) {
        cnt = NSS_CMSArray_Count((void **)sigd->digestAlgorithms);
        sigd->digests = PORT_ArenaZAlloc(sigd->cmsg->poolp, (cnt + 1) * sizeof(SECItem *));
        if (sigd->digests == NULL) {
            PORT_SetError(SEC_ERROR_NO_MEMORY);
            return SECFailure;
        }
    }

    n = -1;
    if (sigd->digestAlgorithms != NULL)
        n = NSS_CMSAlgArray_GetIndexByAlgTag(sigd->digestAlgorithms, digestalgtag);

    /* if not found, add a digest */
    if (n < 0) {
        if (NSS_CMSSignedData_AddDigest(poolp, sigd, digestalgtag, digest) != SECSuccess)
            goto loser;
    } else {
        /* replace NULL pointer with digest item (and leak previous value) */
        sigd->digests[n] = digest;
    }

    PORT_ArenaUnmark(poolp, mark);
    return SECSuccess;

loser:
    PORT_ArenaRelease(poolp, mark);
    return SECFailure;
}

SECStatus
NSS_CMSSignedData_AddDigest(PLArenaPool *poolp,
                            NSSCMSSignedData *sigd,
                            SECOidTag digestalgtag,
                            SECItem *digest)
{
    SECAlgorithmID *digestalg;
    void *mark;

    if (!sigd || !poolp) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    mark = PORT_ArenaMark(poolp);

    digestalg = PORT_ArenaZAlloc(poolp, sizeof(SECAlgorithmID));
    if (digestalg == NULL)
        goto loser;

    if (SECOID_SetAlgorithmID(poolp, digestalg, digestalgtag, NULL) != SECSuccess) /* no params */
        goto loser;

    if (NSS_CMSArray_Add(poolp, (void ***)&(sigd->digestAlgorithms),
                         (void *)digestalg) != SECSuccess ||
        /* even if digest is NULL, add dummy to have same-size array */
        NSS_CMSArray_Add(poolp, (void ***)&(sigd->digests),
                         (void *)digest) != SECSuccess) {
        goto loser;
    }

    PORT_ArenaUnmark(poolp, mark);
    return SECSuccess;

loser:
    PORT_ArenaRelease(poolp, mark);
    return SECFailure;
}

/* XXX This function doesn't set the error code on failure. */
SECItem *
NSS_CMSSignedData_GetDigestValue(NSSCMSSignedData *sigd, SECOidTag digestalgtag)
{
    int n;

    if (!sigd) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return NULL;
    }

    if (sigd->digestAlgorithms == NULL || sigd->digests == NULL) {
        PORT_SetError(SEC_ERROR_DIGEST_NOT_FOUND);
        return NULL;
    }

    n = NSS_CMSAlgArray_GetIndexByAlgTag(sigd->digestAlgorithms, digestalgtag);

    return (n < 0) ? NULL : sigd->digests[n];
}

/* =============================================================================
 * Misc. utility functions
 */

/*
 * NSS_CMSSignedData_CreateCertsOnly - create a certs-only SignedData.
 *
 * cert          - base certificates that will be included
 * include_chain - if true, include the complete cert chain for cert
 *
 * More certs and chains can be added via AddCertificate and AddCertChain.
 *
 * An error results in a return value of NULL and an error set.
 *
 * XXXX CRLs
 */
NSSCMSSignedData *
NSS_CMSSignedData_CreateCertsOnly(NSSCMSMessage *cmsg, CERTCertificate *cert, PRBool include_chain)
{
    NSSCMSSignedData *sigd;
    void *mark;
    PLArenaPool *poolp;
    SECStatus rv;

    if (!cmsg || !cert) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return NULL;
    }

    poolp = cmsg->poolp;
    mark = PORT_ArenaMark(poolp);

    sigd = NSS_CMSSignedData_Create(cmsg);
    if (sigd == NULL)
        goto loser;

    /* no signerinfos, thus no digestAlgorithms */

    /* but certs */
    if (include_chain) {
        rv = NSS_CMSSignedData_AddCertChain(sigd, cert);
    } else {
        rv = NSS_CMSSignedData_AddCertificate(sigd, cert);
    }
    if (rv != SECSuccess)
        goto loser;

    /* RFC2630 5.2 sez:
     * In the degenerate case where there are no signers, the
     * EncapsulatedContentInfo value being "signed" is irrelevant.  In this
     * case, the content type within the EncapsulatedContentInfo value being
     * "signed" should be id-data (as defined in section 4), and the content
     * field of the EncapsulatedContentInfo value should be omitted.
     */
    rv = NSS_CMSContentInfo_SetContent_Data(cmsg, &(sigd->contentInfo), NULL, PR_TRUE);
    if (rv != SECSuccess)
        goto loser;

    PORT_ArenaUnmark(poolp, mark);
    return sigd;

loser:
    if (sigd)
        NSS_CMSSignedData_Destroy(sigd);
    PORT_ArenaRelease(poolp, mark);
    return NULL;
}

/* TODO:
 * NSS_CMSSignerInfo_GetReceiptRequest()
 * NSS_CMSSignedData_HasReceiptRequest()
 * easy way to iterate over signers
 */
