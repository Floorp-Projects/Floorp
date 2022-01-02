/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * CMS envelopedData methods.
 */

#include "cmslocal.h"

#include "cert.h"
#include "keyhi.h"
#include "secasn1.h"
#include "secitem.h"
#include "secoid.h"
#include "pk11func.h"
#include "secerr.h"
#include "secpkcs5.h"

/*
 * NSS_CMSEnvelopedData_Create - create an enveloped data message
 */
NSSCMSEnvelopedData *
NSS_CMSEnvelopedData_Create(NSSCMSMessage *cmsg, SECOidTag algorithm, int keysize)
{
    void *mark;
    NSSCMSEnvelopedData *envd;
    PLArenaPool *poolp;
    SECStatus rv;

    poolp = cmsg->poolp;

    mark = PORT_ArenaMark(poolp);

    envd = (NSSCMSEnvelopedData *)PORT_ArenaZAlloc(poolp, sizeof(NSSCMSEnvelopedData));
    if (envd == NULL)
        goto loser;

    envd->cmsg = cmsg;

    /* version is set in NSS_CMSEnvelopedData_Encode_BeforeStart() */

    rv = NSS_CMSContentInfo_SetContentEncAlg(poolp, &(envd->contentInfo),
                                             algorithm, NULL, keysize);
    if (rv != SECSuccess)
        goto loser;

    PORT_ArenaUnmark(poolp, mark);
    return envd;

loser:
    PORT_ArenaRelease(poolp, mark);
    return NULL;
}

/*
 * NSS_CMSEnvelopedData_Destroy - destroy an enveloped data message
 */
void
NSS_CMSEnvelopedData_Destroy(NSSCMSEnvelopedData *edp)
{
    NSSCMSRecipientInfo **recipientinfos;
    NSSCMSRecipientInfo *ri;

    if (edp == NULL)
        return;

    recipientinfos = edp->recipientInfos;
    if (recipientinfos == NULL)
        return;

    while ((ri = *recipientinfos++) != NULL)
        NSS_CMSRecipientInfo_Destroy(ri);

    NSS_CMSContentInfo_Destroy(&(edp->contentInfo));
}

/*
 * NSS_CMSEnvelopedData_GetContentInfo - return pointer to this envelopedData's contentinfo
 */
NSSCMSContentInfo *
NSS_CMSEnvelopedData_GetContentInfo(NSSCMSEnvelopedData *envd)
{
    return &(envd->contentInfo);
}

/*
 * NSS_CMSEnvelopedData_AddRecipient - add a recipientinfo to the enveloped data msg
 *
 * rip must be created on the same pool as edp - this is not enforced, though.
 */
SECStatus
NSS_CMSEnvelopedData_AddRecipient(NSSCMSEnvelopedData *edp, NSSCMSRecipientInfo *rip)
{
    void *mark;
    SECStatus rv;

    /* XXX compare pools, if not same, copy rip into edp's pool */

    PR_ASSERT(edp != NULL);
    PR_ASSERT(rip != NULL);

    mark = PORT_ArenaMark(edp->cmsg->poolp);

    rv = NSS_CMSArray_Add(edp->cmsg->poolp, (void ***)&(edp->recipientInfos), (void *)rip);
    if (rv != SECSuccess) {
        PORT_ArenaRelease(edp->cmsg->poolp, mark);
        return SECFailure;
    }

    PORT_ArenaUnmark(edp->cmsg->poolp, mark);
    return SECSuccess;
}

/*
 * NSS_CMSEnvelopedData_Encode_BeforeStart - prepare this envelopedData for encoding
 *
 * at this point, we need
 * - recipientinfos set up with recipient's certificates
 * - a content encryption algorithm (if none, 3DES will be used)
 *
 * this function will generate a random content encryption key (aka bulk key),
 * initialize the recipientinfos with certificate identification and wrap the bulk key
 * using the proper algorithm for every certificiate.
 * it will finally set the bulk algorithm and key so that the encode step can find it.
 */
SECStatus
NSS_CMSEnvelopedData_Encode_BeforeStart(NSSCMSEnvelopedData *envd)
{
    int version;
    NSSCMSRecipientInfo **recipientinfos;
    NSSCMSContentInfo *cinfo;
    PK11SymKey *bulkkey = NULL;
    SECOidTag bulkalgtag;
    CK_MECHANISM_TYPE type;
    PK11SlotInfo *slot;
    SECStatus rv;
    SECItem *dummy;
    PLArenaPool *poolp;
    extern const SEC_ASN1Template NSSCMSRecipientInfoTemplate[];
    void *mark = NULL;
    int i;

    poolp = envd->cmsg->poolp;
    cinfo = &(envd->contentInfo);

    if (cinfo == NULL) {
        PORT_SetError(SEC_ERROR_BAD_DATA);
        goto loser;
    }

    recipientinfos = envd->recipientInfos;
    if (recipientinfos == NULL) {
        PORT_SetError(SEC_ERROR_BAD_DATA);
#if 0
    PORT_SetErrorString("Cannot find recipientinfos to encode.");
#endif
        goto loser;
    }

    version = NSS_CMS_ENVELOPED_DATA_VERSION_REG;
    if (envd->originatorInfo != NULL || envd->unprotectedAttr != NULL) {
        version = NSS_CMS_ENVELOPED_DATA_VERSION_ADV;
    } else {
        for (i = 0; recipientinfos[i] != NULL; i++) {
            if (NSS_CMSRecipientInfo_GetVersion(recipientinfos[i]) != 0) {
                version = NSS_CMS_ENVELOPED_DATA_VERSION_ADV;
                break;
            }
        }
    }
    dummy = SEC_ASN1EncodeInteger(poolp, &(envd->version), version);
    if (dummy == NULL)
        goto loser;

    /* now we need to have a proper content encryption algorithm
     * on the SMIME level, we would figure one out by looking at SMIME capabilities
     * we cannot do that on our level, so if none is set already, we'll just go
     * with one of the mandatory algorithms (3DES) */
    if ((bulkalgtag = NSS_CMSContentInfo_GetContentEncAlgTag(cinfo)) == SEC_OID_UNKNOWN) {
        rv = NSS_CMSContentInfo_SetContentEncAlg(poolp, cinfo, SEC_OID_DES_EDE3_CBC, NULL, 168);
        if (rv != SECSuccess)
            goto loser;
        bulkalgtag = SEC_OID_DES_EDE3_CBC;
    }

    /* generate a random bulk key suitable for content encryption alg */
    type = PK11_AlgtagToMechanism(bulkalgtag);
    slot = PK11_GetBestSlot(type, envd->cmsg->pwfn_arg);
    if (slot == NULL)
        goto loser; /* error has been set by PK11_GetBestSlot */

    /* this is expensive... */
    bulkkey = PK11_KeyGen(slot, type, NULL,
                          NSS_CMSContentInfo_GetBulkKeySize(cinfo) / 8,
                          envd->cmsg->pwfn_arg);
    PK11_FreeSlot(slot);
    if (bulkkey == NULL)
        goto loser; /* error has been set by PK11_KeyGen */

    mark = PORT_ArenaMark(poolp);

    /* Encrypt the bulk key with the public key of each recipient.  */
    for (i = 0; recipientinfos[i] != NULL; i++) {
        rv = NSS_CMSRecipientInfo_WrapBulkKey(recipientinfos[i], bulkkey, bulkalgtag);
        if (rv != SECSuccess)
            goto loser; /* error has been set by NSS_CMSRecipientInfo_EncryptBulkKey */
                        /* could be: alg not supported etc. */
    }

    /* the recipientinfos are all finished. now sort them by DER for SET OF encoding */
    rv = NSS_CMSArray_SortByDER((void **)envd->recipientInfos,
                                NSSCMSRecipientInfoTemplate, NULL);
    if (rv != SECSuccess)
        goto loser; /* error has been set by NSS_CMSArray_SortByDER */

    /* store the bulk key in the contentInfo so that the encoder can find it */
    NSS_CMSContentInfo_SetBulkKey(cinfo, bulkkey);

    PORT_ArenaUnmark(poolp, mark);

    PK11_FreeSymKey(bulkkey);

    return SECSuccess;

loser:
    if (mark != NULL)
        PORT_ArenaRelease(poolp, mark);
    if (bulkkey)
        PK11_FreeSymKey(bulkkey);

    return SECFailure;
}

/*
 * NSS_CMSEnvelopedData_Encode_BeforeData - set up encryption
 *
 * it is essential that this is called before the contentEncAlg is encoded, because
 * setting up the encryption may generate IVs and thus change it!
 */
SECStatus
NSS_CMSEnvelopedData_Encode_BeforeData(NSSCMSEnvelopedData *envd)
{
    NSSCMSContentInfo *cinfo;
    PK11SymKey *bulkkey;
    SECAlgorithmID *algid;
    SECStatus rv;

    cinfo = &(envd->contentInfo);

    /* find bulkkey and algorithm - must have been set by NSS_CMSEnvelopedData_Encode_BeforeStart */
    bulkkey = NSS_CMSContentInfo_GetBulkKey(cinfo);
    if (bulkkey == NULL)
        return SECFailure;
    algid = NSS_CMSContentInfo_GetContentEncAlg(cinfo);
    if (algid == NULL)
        return SECFailure;

    rv = NSS_CMSContentInfo_Private_Init(cinfo);
    if (rv != SECSuccess) {
        return SECFailure;
    }
    /* this may modify algid (with IVs generated in a token).
     * it is essential that algid is a pointer to the contentEncAlg data, not a
     * pointer to a copy! */
    cinfo->privateInfo->ciphcx = NSS_CMSCipherContext_StartEncrypt(envd->cmsg->poolp, bulkkey, algid);
    PK11_FreeSymKey(bulkkey);
    if (cinfo->privateInfo->ciphcx == NULL)
        return SECFailure;

    return SECSuccess;
}

/*
 * NSS_CMSEnvelopedData_Encode_AfterData - finalize this envelopedData for encoding
 */
SECStatus
NSS_CMSEnvelopedData_Encode_AfterData(NSSCMSEnvelopedData *envd)
{
    if (envd->contentInfo.privateInfo && envd->contentInfo.privateInfo->ciphcx) {
        NSS_CMSCipherContext_Destroy(envd->contentInfo.privateInfo->ciphcx);
        envd->contentInfo.privateInfo->ciphcx = NULL;
    }

    /* nothing else to do after data */
    return SECSuccess;
}

/*
 * NSS_CMSEnvelopedData_Decode_BeforeData - find our recipientinfo,
 * derive bulk key & set up our contentinfo
 */
SECStatus
NSS_CMSEnvelopedData_Decode_BeforeData(NSSCMSEnvelopedData *envd)
{
    NSSCMSRecipientInfo *ri;
    PK11SymKey *bulkkey = NULL;
    SECOidTag bulkalgtag;
    SECAlgorithmID *bulkalg;
    SECStatus rv = SECFailure;
    NSSCMSContentInfo *cinfo;
    NSSCMSRecipient **recipient_list = NULL;
    NSSCMSRecipient *recipient;
    int rlIndex;

    if (NSS_CMSArray_Count((void **)envd->recipientInfos) == 0) {
        PORT_SetError(SEC_ERROR_BAD_DATA);
#if 0
    PORT_SetErrorString("No recipient data in envelope.");
#endif
        goto loser;
    }

    /* look if one of OUR cert's issuerSN is on the list of recipients, and if so,  */
    /* get the cert and private key for it right away */
    recipient_list = nss_cms_recipient_list_create(envd->recipientInfos);
    if (recipient_list == NULL)
        goto loser;

    /* what about multiple recipientInfos that match?
     * especially if, for some reason, we could not produce a bulk key with the first match?!
     * we could loop & feed partial recipient_list to PK11_FindCertAndKeyByRecipientList...
     * maybe later... */
    rlIndex = PK11_FindCertAndKeyByRecipientListNew(recipient_list, envd->cmsg->pwfn_arg);

    /* if that fails, then we're not an intended recipient and cannot decrypt */
    if (rlIndex < 0) {
        PORT_SetError(SEC_ERROR_NOT_A_RECIPIENT);
#if 0
    PORT_SetErrorString("Cannot decrypt data because proper key cannot be found.");
#endif
        goto loser;
    }

    recipient = recipient_list[rlIndex];
    if (!recipient->cert || !recipient->privkey) {
        /* XXX should set an error code ?!? */
        goto loser;
    }
    /* get a pointer to "our" recipientinfo */
    ri = envd->recipientInfos[recipient->riIndex];

    cinfo = &(envd->contentInfo);
    bulkalgtag = NSS_CMSContentInfo_GetContentEncAlgTag(cinfo);
    if (bulkalgtag == SEC_OID_UNKNOWN) {
        PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
    } else
        bulkkey =
            NSS_CMSRecipientInfo_UnwrapBulkKey(ri, recipient->subIndex,
                                               recipient->cert,
                                               recipient->privkey,
                                               bulkalgtag);
    if (bulkkey == NULL) {
        /* no success finding a bulk key */
        goto loser;
    }

    NSS_CMSContentInfo_SetBulkKey(cinfo, bulkkey);

    bulkalg = NSS_CMSContentInfo_GetContentEncAlg(cinfo);

    rv = NSS_CMSContentInfo_Private_Init(cinfo);
    if (rv != SECSuccess) {
        goto loser;
    }
    rv = SECFailure;
    cinfo->privateInfo->ciphcx = NSS_CMSCipherContext_StartDecrypt(bulkkey, bulkalg);
    if (cinfo->privateInfo->ciphcx == NULL)
        goto loser; /* error has been set by NSS_CMSCipherContext_StartDecrypt */

    rv = SECSuccess;

loser:
    if (bulkkey)
        PK11_FreeSymKey(bulkkey);
    if (recipient_list != NULL)
        nss_cms_recipient_list_destroy(recipient_list);
    return rv;
}

/*
 * NSS_CMSEnvelopedData_Decode_AfterData - finish decrypting this envelopedData's content
 */
SECStatus
NSS_CMSEnvelopedData_Decode_AfterData(NSSCMSEnvelopedData *envd)
{
    if (envd && envd->contentInfo.privateInfo && envd->contentInfo.privateInfo->ciphcx) {
        NSS_CMSCipherContext_Destroy(envd->contentInfo.privateInfo->ciphcx);
        envd->contentInfo.privateInfo->ciphcx = NULL;
    }

    return SECSuccess;
}

/*
 * NSS_CMSEnvelopedData_Decode_AfterEnd - finish decoding this envelopedData
 */
SECStatus
NSS_CMSEnvelopedData_Decode_AfterEnd(NSSCMSEnvelopedData *envd)
{
    /* apply final touches */
    return SECSuccess;
}
