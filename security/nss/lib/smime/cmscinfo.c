/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * CMS contentInfo methods.
 */

#include "cmslocal.h"

#include "pk11func.h"
#include "secitem.h"
#include "secoid.h"
#include "secerr.h"

/*
 * NSS_CMSContentInfo_Create - create a content info
 *
 * version is set in the _Finalize procedures for each content type
 */
SECStatus
NSS_CMSContentInfo_Private_Init(NSSCMSContentInfo *cinfo)
{
    if (cinfo->privateInfo) {
        return SECSuccess;
    }
    cinfo->privateInfo = PORT_ZNew(NSSCMSContentInfoPrivate);
    return (cinfo->privateInfo) ? SECSuccess : SECFailure;
}

static void
nss_cmsContentInfo_private_destroy(NSSCMSContentInfoPrivate *privateInfo)
{
    if (privateInfo->digcx) {
        /* must destroy digest objects */
        NSS_CMSDigestContext_Cancel(privateInfo->digcx);
        privateInfo->digcx = NULL;
    }
    if (privateInfo->ciphcx) {
        NSS_CMSCipherContext_Destroy(privateInfo->ciphcx);
        privateInfo->ciphcx = NULL;
    }
    PORT_Free(privateInfo);
}

/*
 * NSS_CMSContentInfo_Destroy - destroy a CMS contentInfo and all of its sub-pieces.
 */
void
NSS_CMSContentInfo_Destroy(NSSCMSContentInfo *cinfo)
{
    SECOidTag kind;

    if (cinfo == NULL) {
        return;
    }

    kind = NSS_CMSContentInfo_GetContentTypeTag(cinfo);
    switch (kind) {
        case SEC_OID_PKCS7_ENVELOPED_DATA:
            NSS_CMSEnvelopedData_Destroy(cinfo->content.envelopedData);
            break;
        case SEC_OID_PKCS7_SIGNED_DATA:
            NSS_CMSSignedData_Destroy(cinfo->content.signedData);
            break;
        case SEC_OID_PKCS7_ENCRYPTED_DATA:
            NSS_CMSEncryptedData_Destroy(cinfo->content.encryptedData);
            break;
        case SEC_OID_PKCS7_DIGESTED_DATA:
            NSS_CMSDigestedData_Destroy(cinfo->content.digestedData);
            break;
        default:
            NSS_CMSGenericWrapperData_Destroy(kind, cinfo->content.genericData);
            /* XXX Anything else that needs to be "manually" freed/destroyed? */
            break;
    }
    if (cinfo->privateInfo) {
        nss_cmsContentInfo_private_destroy(cinfo->privateInfo);
        cinfo->privateInfo = NULL;
    }
    if (cinfo->bulkkey) {
        PK11_FreeSymKey(cinfo->bulkkey);
    }
}

/*
 * NSS_CMSContentInfo_GetChildContentInfo - get content's contentInfo (if it exists)
 */
NSSCMSContentInfo *
NSS_CMSContentInfo_GetChildContentInfo(NSSCMSContentInfo *cinfo)
{
    NSSCMSContentInfo *ccinfo = NULL;

    if (cinfo == NULL) {
        return NULL;
    }

    SECOidTag tag = NSS_CMSContentInfo_GetContentTypeTag(cinfo);
    switch (tag) {
        case SEC_OID_PKCS7_SIGNED_DATA:
            if (cinfo->content.signedData != NULL) {
                ccinfo = &(cinfo->content.signedData->contentInfo);
            }
            break;
        case SEC_OID_PKCS7_ENVELOPED_DATA:
            if (cinfo->content.envelopedData != NULL) {
                ccinfo = &(cinfo->content.envelopedData->contentInfo);
            }
            break;
        case SEC_OID_PKCS7_DIGESTED_DATA:
            if (cinfo->content.digestedData != NULL) {
                ccinfo = &(cinfo->content.digestedData->contentInfo);
            }
            break;
        case SEC_OID_PKCS7_ENCRYPTED_DATA:
            if (cinfo->content.encryptedData != NULL) {
                ccinfo = &(cinfo->content.encryptedData->contentInfo);
            }
            break;
        case SEC_OID_PKCS7_DATA:
        default:
            if (NSS_CMSType_IsWrapper(tag)) {
                if (cinfo->content.genericData != NULL) {
                    ccinfo = &(cinfo->content.genericData->contentInfo);
                }
            }
            break;
    }
    if (ccinfo && !ccinfo->privateInfo) {
        NSS_CMSContentInfo_Private_Init(ccinfo);
    }
    return ccinfo;
}

SECStatus
NSS_CMSContentInfo_SetDontStream(NSSCMSContentInfo *cinfo, PRBool dontStream)
{
    SECStatus rv;
    if (cinfo == NULL) {
        return SECFailure;
    }

    rv = NSS_CMSContentInfo_Private_Init(cinfo);
    if (rv != SECSuccess) {
        /* default is streaming, failure to get ccinfo will not effect this */
        return dontStream ? SECFailure : SECSuccess;
    }
    cinfo->privateInfo->dontStream = dontStream;
    return SECSuccess;
}

/*
 * NSS_CMSContentInfo_SetContent - set content type & content
 */
SECStatus
NSS_CMSContentInfo_SetContent(NSSCMSMessage *cmsg, NSSCMSContentInfo *cinfo,
                              SECOidTag type, void *ptr)
{
    SECStatus rv;
    if (cinfo == NULL || cmsg == NULL) {
        return SECFailure;
    }

    cinfo->contentTypeTag = SECOID_FindOIDByTag(type);
    if (cinfo->contentTypeTag == NULL) {
        return SECFailure;
    }

    /* do not copy the oid, just create a reference */
    rv = SECITEM_CopyItem(cmsg->poolp, &(cinfo->contentType), &(cinfo->contentTypeTag->oid));
    if (rv != SECSuccess) {
        return SECFailure;
    }

    cinfo->content.pointer = ptr;

    if (NSS_CMSType_IsData(type) && ptr) {
        cinfo->rawContent = ptr;
    } else {
        /* as we always have some inner data,
     * we need to set it to something, just to fool the encoder enough to work on it
     * and get us into nss_cms_encoder_notify at that point */
        cinfo->rawContent = SECITEM_AllocItem(cmsg->poolp, NULL, 1);
        if (cinfo->rawContent == NULL) {
            PORT_SetError(SEC_ERROR_NO_MEMORY);
            return SECFailure;
        }
    }

    return SECSuccess;
}

/*
 * NSS_CMSContentInfo_SetContent_XXXX - typesafe wrappers for NSS_CMSContentInfo_SetContent
 */

/*
 * data == NULL -> pass in data via NSS_CMSEncoder_Update
 * data != NULL -> take this data
 */
SECStatus
NSS_CMSContentInfo_SetContent_Data(NSSCMSMessage *cmsg, NSSCMSContentInfo *cinfo,
                                   SECItem *data, PRBool detached)
{
    if (NSS_CMSContentInfo_SetContent(cmsg, cinfo, SEC_OID_PKCS7_DATA, (void *)data) != SECSuccess) {
        return SECFailure;
    }
    if (detached) {
        cinfo->rawContent = NULL;
    }

    return SECSuccess;
}

SECStatus
NSS_CMSContentInfo_SetContent_SignedData(NSSCMSMessage *cmsg, NSSCMSContentInfo *cinfo,
                                         NSSCMSSignedData *sigd)
{
    return NSS_CMSContentInfo_SetContent(cmsg, cinfo, SEC_OID_PKCS7_SIGNED_DATA, (void *)sigd);
}

SECStatus
NSS_CMSContentInfo_SetContent_EnvelopedData(NSSCMSMessage *cmsg, NSSCMSContentInfo *cinfo,
                                            NSSCMSEnvelopedData *envd)
{
    return NSS_CMSContentInfo_SetContent(cmsg, cinfo, SEC_OID_PKCS7_ENVELOPED_DATA, (void *)envd);
}

SECStatus
NSS_CMSContentInfo_SetContent_DigestedData(NSSCMSMessage *cmsg, NSSCMSContentInfo *cinfo,
                                           NSSCMSDigestedData *digd)
{
    return NSS_CMSContentInfo_SetContent(cmsg, cinfo, SEC_OID_PKCS7_DIGESTED_DATA, (void *)digd);
}

SECStatus
NSS_CMSContentInfo_SetContent_EncryptedData(NSSCMSMessage *cmsg, NSSCMSContentInfo *cinfo,
                                            NSSCMSEncryptedData *encd)
{
    return NSS_CMSContentInfo_SetContent(cmsg, cinfo, SEC_OID_PKCS7_ENCRYPTED_DATA, (void *)encd);
}

/*
 * NSS_CMSContentInfo_GetContent - get pointer to inner content
 *
 * needs to be casted...
 */
void *
NSS_CMSContentInfo_GetContent(NSSCMSContentInfo *cinfo)
{
    if (cinfo == NULL) {
        return NULL;
    }

    SECOidTag tag = cinfo->contentTypeTag
                        ? cinfo->contentTypeTag->offset
                        : SEC_OID_UNKNOWN;
    switch (tag) {
        case SEC_OID_PKCS7_DATA:
        case SEC_OID_PKCS7_SIGNED_DATA:
        case SEC_OID_PKCS7_ENVELOPED_DATA:
        case SEC_OID_PKCS7_DIGESTED_DATA:
        case SEC_OID_PKCS7_ENCRYPTED_DATA:
            return cinfo->content.pointer;
        default:
            return NSS_CMSType_IsWrapper(tag) ? cinfo->content.pointer
                                              : (NSS_CMSType_IsData(tag) ? cinfo->rawContent
                                                                         : NULL);
    }
}

/*
 * NSS_CMSContentInfo_GetInnerContent - get pointer to innermost content
 *
 * this is typically only called by NSS_CMSMessage_GetContent()
 */

SECItem *
NSS_CMSContentInfo_GetInnerContent(NSSCMSContentInfo *cinfo)
{
    NSSCMSContentInfo *ccinfo;
    SECOidTag tag;
    SECItem *pItem = NULL;

    if (cinfo == NULL) {
        return NULL;
    }

    tag = NSS_CMSContentInfo_GetContentTypeTag(cinfo);
    if (NSS_CMSType_IsData(tag)) {
        pItem = cinfo->content.data;
    } else if (NSS_CMSType_IsWrapper(tag)) {
        ccinfo = NSS_CMSContentInfo_GetChildContentInfo(cinfo);
        if (ccinfo != NULL) {
            pItem = NSS_CMSContentInfo_GetContent(ccinfo);
        }
    } else {
        PORT_Assert(0);
    }

    return pItem;
}

/*
 * NSS_CMSContentInfo_GetContentType{Tag,OID} - find out (saving pointer to lookup result
 * for future reference) and return the inner content type.
 */
SECOidTag
NSS_CMSContentInfo_GetContentTypeTag(NSSCMSContentInfo *cinfo)
{
    if (cinfo == NULL) {
        return SEC_OID_UNKNOWN;
    }

    if (cinfo->contentTypeTag == NULL)
        cinfo->contentTypeTag = SECOID_FindOID(&(cinfo->contentType));

    if (cinfo->contentTypeTag == NULL)
        return SEC_OID_UNKNOWN;

    return cinfo->contentTypeTag->offset;
}

SECItem *
NSS_CMSContentInfo_GetContentTypeOID(NSSCMSContentInfo *cinfo)
{
    if (cinfo == NULL) {
        return NULL;
    }

    if (cinfo->contentTypeTag == NULL) {
        cinfo->contentTypeTag = SECOID_FindOID(&(cinfo->contentType));
    }

    if (cinfo->contentTypeTag == NULL) {
        return NULL;
    }

    return &(cinfo->contentTypeTag->oid);
}

/*
 * NSS_CMSContentInfo_GetContentEncAlgTag - find out (saving pointer to lookup result
 * for future reference) and return the content encryption algorithm tag.
 */
SECOidTag
NSS_CMSContentInfo_GetContentEncAlgTag(NSSCMSContentInfo *cinfo)
{
    if (cinfo == NULL) {
        return SEC_OID_UNKNOWN;
    }

    if (cinfo->contentEncAlgTag == SEC_OID_UNKNOWN) {
        cinfo->contentEncAlgTag = SECOID_GetAlgorithmTag(&(cinfo->contentEncAlg));
    }

    return cinfo->contentEncAlgTag;
}

/*
 * NSS_CMSContentInfo_GetContentEncAlg - find out and return the content encryption algorithm tag.
 */
SECAlgorithmID *
NSS_CMSContentInfo_GetContentEncAlg(NSSCMSContentInfo *cinfo)
{
    if (cinfo == NULL) {
        return NULL;
    }

    return &(cinfo->contentEncAlg);
}

SECStatus
NSS_CMSContentInfo_SetContentEncAlg(PLArenaPool *poolp, NSSCMSContentInfo *cinfo,
                                    SECOidTag bulkalgtag, SECItem *parameters, int keysize)
{
    SECStatus rv;
    if (cinfo == NULL) {
        return SECFailure;
    }

    rv = SECOID_SetAlgorithmID(poolp, &(cinfo->contentEncAlg), bulkalgtag, parameters);
    if (rv != SECSuccess) {
        return SECFailure;
    }
    cinfo->keysize = keysize;
    return SECSuccess;
}

SECStatus
NSS_CMSContentInfo_SetContentEncAlgID(PLArenaPool *poolp, NSSCMSContentInfo *cinfo,
                                      SECAlgorithmID *algid, int keysize)
{
    SECStatus rv;
    if (cinfo == NULL) {
        return SECFailure;
    }

    rv = SECOID_CopyAlgorithmID(poolp, &(cinfo->contentEncAlg), algid);
    if (rv != SECSuccess) {
        return SECFailure;
    }
    if (keysize >= 0) {
        cinfo->keysize = keysize;
    }
    return SECSuccess;
}

void
NSS_CMSContentInfo_SetBulkKey(NSSCMSContentInfo *cinfo, PK11SymKey *bulkkey)
{
    if (cinfo == NULL) {
        return;
    }

    if (bulkkey == NULL) {
        cinfo->bulkkey = NULL;
        cinfo->keysize = 0;
    } else {
        cinfo->bulkkey = PK11_ReferenceSymKey(bulkkey);
        cinfo->keysize = PK11_GetKeyStrength(cinfo->bulkkey, &(cinfo->contentEncAlg));
    }
}

PK11SymKey *
NSS_CMSContentInfo_GetBulkKey(NSSCMSContentInfo *cinfo)
{
    if (cinfo == NULL || cinfo->bulkkey == NULL) {
        return NULL;
    }

    return PK11_ReferenceSymKey(cinfo->bulkkey);
}

int
NSS_CMSContentInfo_GetBulkKeySize(NSSCMSContentInfo *cinfo)
{
    if (cinfo == NULL) {
        return 0;
    }

    return cinfo->keysize;
}
