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
 * CMS contentInfo methods.
 *
 * $Id: cmscinfo.c,v 1.5 2003/12/04 00:39:24 nelsonb%netscape.com Exp $
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

/*
 * NSS_CMSContentInfo_Destroy - destroy a CMS contentInfo and all of its sub-pieces.
 */
void
NSS_CMSContentInfo_Destroy(NSSCMSContentInfo *cinfo)
{
    SECOidTag kind;

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
	/* XXX Anything else that needs to be "manually" freed/destroyed? */
	break;
    }
    if (cinfo->digcx) {
	/* must destroy digest objects */
	NSS_CMSDigestContext_Cancel(cinfo->digcx);
	cinfo->digcx = NULL;
    }
    if (cinfo->bulkkey)
	PK11_FreeSymKey(cinfo->bulkkey);

    if (cinfo->ciphcx) {
	NSS_CMSCipherContext_Destroy(cinfo->ciphcx);
	cinfo->ciphcx = NULL;
    }
    
    /* we live in a pool, so no need to worry about storage */
}

/*
 * NSS_CMSContentInfo_GetChildContentInfo - get content's contentInfo (if it exists)
 */
NSSCMSContentInfo *
NSS_CMSContentInfo_GetChildContentInfo(NSSCMSContentInfo *cinfo)
{
    void * ptr                  = NULL;
    NSSCMSContentInfo * ccinfo  = NULL;
    SECOidTag tag = NSS_CMSContentInfo_GetContentTypeTag(cinfo);
    switch (tag) {
    case SEC_OID_PKCS7_SIGNED_DATA:
	ptr    = (void *)cinfo->content.signedData;
	ccinfo = &(cinfo->content.signedData->contentInfo);
	break;
    case SEC_OID_PKCS7_ENVELOPED_DATA:
	ptr    = (void *)cinfo->content.envelopedData;
	ccinfo = &(cinfo->content.envelopedData->contentInfo);
	break;
    case SEC_OID_PKCS7_DIGESTED_DATA:
	ptr    = (void *)cinfo->content.digestedData;
	ccinfo = &(cinfo->content.digestedData->contentInfo);
	break;
    case SEC_OID_PKCS7_ENCRYPTED_DATA:
	ptr    = (void *)cinfo->content.encryptedData;
	ccinfo = &(cinfo->content.encryptedData->contentInfo);
	break;
    case SEC_OID_PKCS7_DATA:
    default:
	break;
    }
    return (ptr ? ccinfo : NULL);
}

/*
 * NSS_CMSContentInfo_SetContent - set content type & content
 */
SECStatus
NSS_CMSContentInfo_SetContent(NSSCMSMessage *cmsg, NSSCMSContentInfo *cinfo, SECOidTag type, void *ptr)
{
    SECStatus rv;

    cinfo->contentTypeTag = SECOID_FindOIDByTag(type);
    if (cinfo->contentTypeTag == NULL)
	return SECFailure;
    
    /* do not copy the oid, just create a reference */
    rv = SECITEM_CopyItem (cmsg->poolp, &(cinfo->contentType), &(cinfo->contentTypeTag->oid));
    if (rv != SECSuccess)
	return SECFailure;

    cinfo->content.pointer = ptr;

    if (type != SEC_OID_PKCS7_DATA) {
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
NSS_CMSContentInfo_SetContent_Data(NSSCMSMessage *cmsg, NSSCMSContentInfo *cinfo, SECItem *data, PRBool detached)
{
    if (NSS_CMSContentInfo_SetContent(cmsg, cinfo, SEC_OID_PKCS7_DATA, (void *)data) != SECSuccess)
	return SECFailure;
    cinfo->rawContent = (detached) ? 
			    NULL : (data) ? 
				data : SECITEM_AllocItem(cmsg->poolp, NULL, 1);
    return SECSuccess;
}

SECStatus
NSS_CMSContentInfo_SetContent_SignedData(NSSCMSMessage *cmsg, NSSCMSContentInfo *cinfo, NSSCMSSignedData *sigd)
{
    return NSS_CMSContentInfo_SetContent(cmsg, cinfo, SEC_OID_PKCS7_SIGNED_DATA, (void *)sigd);
}

SECStatus
NSS_CMSContentInfo_SetContent_EnvelopedData(NSSCMSMessage *cmsg, NSSCMSContentInfo *cinfo, NSSCMSEnvelopedData *envd)
{
    return NSS_CMSContentInfo_SetContent(cmsg, cinfo, SEC_OID_PKCS7_ENVELOPED_DATA, (void *)envd);
}

SECStatus
NSS_CMSContentInfo_SetContent_DigestedData(NSSCMSMessage *cmsg, NSSCMSContentInfo *cinfo, NSSCMSDigestedData *digd)
{
    return NSS_CMSContentInfo_SetContent(cmsg, cinfo, SEC_OID_PKCS7_DIGESTED_DATA, (void *)digd);
}

SECStatus
NSS_CMSContentInfo_SetContent_EncryptedData(NSSCMSMessage *cmsg, NSSCMSContentInfo *cinfo, NSSCMSEncryptedData *encd)
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
    switch (cinfo->contentTypeTag->offset) {
    case SEC_OID_PKCS7_DATA:
    case SEC_OID_PKCS7_SIGNED_DATA:
    case SEC_OID_PKCS7_ENVELOPED_DATA:
    case SEC_OID_PKCS7_DIGESTED_DATA:
    case SEC_OID_PKCS7_ENCRYPTED_DATA:
	return cinfo->content.pointer;
    default:
	return NULL;
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

    switch (NSS_CMSContentInfo_GetContentTypeTag(cinfo)) {
    case SEC_OID_PKCS7_DATA:
	return cinfo->content.data;	/* end of recursion - every message has to have a data cinfo */
    case SEC_OID_PKCS7_DIGESTED_DATA:
    case SEC_OID_PKCS7_ENCRYPTED_DATA:
    case SEC_OID_PKCS7_ENVELOPED_DATA:
    case SEC_OID_PKCS7_SIGNED_DATA:
	if ((ccinfo = NSS_CMSContentInfo_GetChildContentInfo(cinfo)) == NULL)
	    break;
	return NSS_CMSContentInfo_GetContent(ccinfo);
    default:
	PORT_Assert(0);
	break;
    }
    return NULL;
}

/*
 * NSS_CMSContentInfo_GetContentType{Tag,OID} - find out (saving pointer to lookup result
 * for future reference) and return the inner content type.
 */
SECOidTag
NSS_CMSContentInfo_GetContentTypeTag(NSSCMSContentInfo *cinfo)
{
    if (cinfo->contentTypeTag == NULL)
	cinfo->contentTypeTag = SECOID_FindOID(&(cinfo->contentType));

    if (cinfo->contentTypeTag == NULL)
	return SEC_OID_UNKNOWN;

    return cinfo->contentTypeTag->offset;
}

SECItem *
NSS_CMSContentInfo_GetContentTypeOID(NSSCMSContentInfo *cinfo)
{
    if (cinfo->contentTypeTag == NULL)
	cinfo->contentTypeTag = SECOID_FindOID(&(cinfo->contentType));

    if (cinfo->contentTypeTag == NULL)
	return NULL;

    return &(cinfo->contentTypeTag->oid);
}

/*
 * NSS_CMSContentInfo_GetContentEncAlgTag - find out (saving pointer to lookup result
 * for future reference) and return the content encryption algorithm tag.
 */
SECOidTag
NSS_CMSContentInfo_GetContentEncAlgTag(NSSCMSContentInfo *cinfo)
{
    if (cinfo->contentEncAlgTag == SEC_OID_UNKNOWN)
	cinfo->contentEncAlgTag = SECOID_GetAlgorithmTag(&(cinfo->contentEncAlg));

    return cinfo->contentEncAlgTag;
}

/*
 * NSS_CMSContentInfo_GetContentEncAlg - find out and return the content encryption algorithm tag.
 */
SECAlgorithmID *
NSS_CMSContentInfo_GetContentEncAlg(NSSCMSContentInfo *cinfo)
{
    return &(cinfo->contentEncAlg);
}

SECStatus
NSS_CMSContentInfo_SetContentEncAlg(PLArenaPool *poolp, NSSCMSContentInfo *cinfo,
				    SECOidTag bulkalgtag, SECItem *parameters, int keysize)
{
    SECStatus rv;

    rv = SECOID_SetAlgorithmID(poolp, &(cinfo->contentEncAlg), bulkalgtag, parameters);
    if (rv != SECSuccess)
	return SECFailure;
    cinfo->keysize = keysize;
    return SECSuccess;
}

SECStatus
NSS_CMSContentInfo_SetContentEncAlgID(PLArenaPool *poolp, NSSCMSContentInfo *cinfo,
				    SECAlgorithmID *algid, int keysize)
{
    SECStatus rv;

    rv = SECOID_CopyAlgorithmID(poolp, &(cinfo->contentEncAlg), algid);
    if (rv != SECSuccess)
	return SECFailure;
    if (keysize >= 0)
	cinfo->keysize = keysize;
    return SECSuccess;
}

void
NSS_CMSContentInfo_SetBulkKey(NSSCMSContentInfo *cinfo, PK11SymKey *bulkkey)
{
    cinfo->bulkkey = PK11_ReferenceSymKey(bulkkey);
    cinfo->keysize = PK11_GetKeyStrength(cinfo->bulkkey, &(cinfo->contentEncAlg));
}

PK11SymKey *
NSS_CMSContentInfo_GetBulkKey(NSSCMSContentInfo *cinfo)
{
    if (cinfo->bulkkey == NULL)
	return NULL;

    return PK11_ReferenceSymKey(cinfo->bulkkey);
}

int
NSS_CMSContentInfo_GetBulkKeySize(NSSCMSContentInfo *cinfo)
{
    return cinfo->keysize;
}
