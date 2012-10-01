/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * CMS decoding.
 *
 * $Id: cmsdecode.c,v 1.15 2012/04/25 14:50:08 gerv%gerv.net Exp $
 */

#include "cmslocal.h"

#include "cert.h"
#include "key.h"
#include "secasn1.h"
#include "secitem.h"
#include "secoid.h"
#include "prtime.h"
#include "secerr.h"

struct NSSCMSDecoderContextStr {
    SEC_ASN1DecoderContext *	dcx;		/* ASN.1 decoder context */
    NSSCMSMessage *		cmsg;		/* backpointer to the root message */
    SECOidTag			type;		/* type of message */
    NSSCMSContent		content;	/* pointer to message */
    NSSCMSDecoderContext *	childp7dcx;	/* inner CMS decoder context */
    PRBool			saw_contents;
    int				error;
    NSSCMSContentCallback	cb;
    void *			cb_arg;
    PRBool			first_decoded;
    PRBool			need_indefinite_finish;
};

struct NSSCMSDecoderDataStr {
    SECItem data; 	/* must be first */
    unsigned int totalBufferSize;
};

typedef struct NSSCMSDecoderDataStr NSSCMSDecoderData;

static void      nss_cms_decoder_update_filter (void *arg, const char *data, 
                 unsigned long len, int depth, SEC_ASN1EncodingPart data_kind);
static SECStatus nss_cms_before_data(NSSCMSDecoderContext *p7dcx);
static SECStatus nss_cms_after_data(NSSCMSDecoderContext *p7dcx);
static SECStatus nss_cms_after_end(NSSCMSDecoderContext *p7dcx);
static void      nss_cms_decoder_work_data(NSSCMSDecoderContext *p7dcx, 
		 const unsigned char *data, unsigned long len, PRBool final);
static NSSCMSDecoderData *nss_cms_create_decoder_data(PRArenaPool *poolp);

extern const SEC_ASN1Template NSSCMSMessageTemplate[];

static NSSCMSDecoderData *
nss_cms_create_decoder_data(PRArenaPool *poolp)
{
    NSSCMSDecoderData *decoderData = NULL;

    decoderData = (NSSCMSDecoderData *)
			PORT_ArenaAlloc(poolp,sizeof(NSSCMSDecoderData));
    if (!decoderData) {
	return NULL;
    }
    decoderData->data.data = NULL;
    decoderData->data.len = 0;
    decoderData->totalBufferSize = 0;
    return decoderData;
}

/* 
 * nss_cms_decoder_notify -
 *  this is the driver of the decoding process. It gets called by the ASN.1
 *  decoder before and after an object is decoded.
 *  at various points in the decoding process, we intercept to set up and do
 *  further processing.
 */
static void
nss_cms_decoder_notify(void *arg, PRBool before, void *dest, int depth)
{
    NSSCMSDecoderContext *p7dcx;
    NSSCMSContentInfo *rootcinfo, *cinfo;
    PRBool after = !before;

    p7dcx = (NSSCMSDecoderContext *)arg;
    rootcinfo = &(p7dcx->cmsg->contentInfo);

    /* XXX error handling: need to set p7dcx->error */

#ifdef CMSDEBUG 
    fprintf(stderr, "%6.6s, dest = 0x%08x, depth = %d\n", before ? "before" : "after", dest, depth);
#endif

    /* so what are we working on right now? */
    if (p7dcx->type == SEC_OID_UNKNOWN) {
	/*
	 * right now, we are still decoding the OUTER (root) cinfo
	 * As soon as we know the inner content type, set up the info,
	 * but NO inner decoder or filter. The root decoder handles the first
	 * level children by itself - only for encapsulated contents (which
	 * are encoded as DER inside of an OCTET STRING) we need to set up a
	 * child decoder...
	 */
	if (after && dest == &(rootcinfo->contentType)) {
	    p7dcx->type = NSS_CMSContentInfo_GetContentTypeTag(rootcinfo);
	    p7dcx->content = rootcinfo->content;	
	    /* is this ready already ? need to alloc? */
	    /* XXX yes we need to alloc -- continue here */
	}
    } else if (NSS_CMSType_IsData(p7dcx->type)) {
	/* this can only happen if the outermost cinfo has DATA in it */
	/* otherwise, we handle this type implicitely in the inner decoders */

	if (before && dest == &(rootcinfo->content)) {
	    /* cause the filter to put the data in the right place... 
	    ** We want the ASN.1 decoder to deliver the decoded bytes to us 
	    ** from now on 
	    */
	    SEC_ASN1DecoderSetFilterProc(p7dcx->dcx,
					  nss_cms_decoder_update_filter,
					  p7dcx,
					  (PRBool)(p7dcx->cb != NULL));
	} else if (after && dest == &(rootcinfo->content.data)) {
	    /* remove the filter */
	    SEC_ASN1DecoderClearFilterProc(p7dcx->dcx);
	}
    } else if (NSS_CMSType_IsWrapper(p7dcx->type)) {
	if (!before || dest != &(rootcinfo->content)) {

	    if (p7dcx->content.pointer == NULL)
		p7dcx->content = rootcinfo->content;

	    /* get this data type's inner contentInfo */
	    cinfo = NSS_CMSContent_GetContentInfo(p7dcx->content.pointer, 
	                                      p7dcx->type);

	    if (before && dest == &(cinfo->contentType)) {
	        /* at this point, set up the &%$&$ back pointer */
	        /* we cannot do it later, because the content itself 
		 * is optional! */
		switch (p7dcx->type) {
		case SEC_OID_PKCS7_SIGNED_DATA:
		    p7dcx->content.signedData->cmsg = p7dcx->cmsg;
		    break;
		case SEC_OID_PKCS7_DIGESTED_DATA:
		    p7dcx->content.digestedData->cmsg = p7dcx->cmsg;
		    break;
		case SEC_OID_PKCS7_ENVELOPED_DATA:
		    p7dcx->content.envelopedData->cmsg = p7dcx->cmsg;
		    break;
		case SEC_OID_PKCS7_ENCRYPTED_DATA:
		    p7dcx->content.encryptedData->cmsg = p7dcx->cmsg;
		    break;
		default:
		    p7dcx->content.genericData->cmsg = p7dcx->cmsg;
		    break;
		}
	    }

	    if (before && dest == &(cinfo->rawContent)) {
		/* we want the ASN.1 decoder to deliver the decoded bytes to us 
		 ** from now on 
		 */
		SEC_ASN1DecoderSetFilterProc(p7dcx->dcx, 
	                                 nss_cms_decoder_update_filter, 
					 p7dcx, (PRBool)(p7dcx->cb != NULL));


		/* we're right in front of the data */
		if (nss_cms_before_data(p7dcx) != SECSuccess) {
		    SEC_ASN1DecoderClearFilterProc(p7dcx->dcx);	
		    /* stop all processing */
		    p7dcx->error = PORT_GetError();
		}
	    }
	    if (after && dest == &(cinfo->rawContent)) {
		/* we're right after of the data */
		if (nss_cms_after_data(p7dcx) != SECSuccess)
		    p7dcx->error = PORT_GetError();

		/* we don't need to see the contents anymore */
		SEC_ASN1DecoderClearFilterProc(p7dcx->dcx);
	    }
	}
    } else {
	/* unsupported or unknown message type - fail  gracefully */
	p7dcx->error = SEC_ERROR_UNSUPPORTED_MESSAGE_TYPE;
    }
}

/*
 * nss_cms_before_data - set up the current encoder to receive data
 */
static SECStatus
nss_cms_before_data(NSSCMSDecoderContext *p7dcx)
{
    SECStatus rv;
    SECOidTag childtype;
    PLArenaPool *poolp;
    NSSCMSDecoderContext *childp7dcx;
    NSSCMSContentInfo *cinfo;
    const SEC_ASN1Template *template;
    void *mark = NULL;
    size_t size;
    
    poolp = p7dcx->cmsg->poolp;

    /* call _Decode_BeforeData handlers */
    switch (p7dcx->type) {
    case SEC_OID_PKCS7_SIGNED_DATA:
	/* we're decoding a signedData, so set up the digests */
	rv = NSS_CMSSignedData_Decode_BeforeData(p7dcx->content.signedData);
	break;
    case SEC_OID_PKCS7_DIGESTED_DATA:
	/* we're encoding a digestedData, so set up the digest */
	rv = NSS_CMSDigestedData_Decode_BeforeData(p7dcx->content.digestedData);
	break;
    case SEC_OID_PKCS7_ENVELOPED_DATA:
	rv = NSS_CMSEnvelopedData_Decode_BeforeData(
	                             p7dcx->content.envelopedData);
	break;
    case SEC_OID_PKCS7_ENCRYPTED_DATA:
	rv = NSS_CMSEncryptedData_Decode_BeforeData(
	                             p7dcx->content.encryptedData);
	break;
    default:
	rv = NSS_CMSGenericWrapperData_Decode_BeforeData(p7dcx->type,
				p7dcx->content.genericData);
    }
    if (rv != SECSuccess)
	return SECFailure;

    /* ok, now we have a pointer to cinfo */
    /* find out what kind of data is encapsulated */
    
    cinfo = NSS_CMSContent_GetContentInfo(p7dcx->content.pointer, p7dcx->type);
    childtype = NSS_CMSContentInfo_GetContentTypeTag(cinfo);

    if (NSS_CMSType_IsData(childtype)) {
	cinfo->content.pointer = (void *) nss_cms_create_decoder_data(poolp);
	if (cinfo->content.pointer == NULL)
	    /* set memory error */
	    return SECFailure;

	p7dcx->childp7dcx = NULL;
	return SECSuccess;
    }

    /* set up inner decoder */

    if ((template = NSS_CMSUtil_GetTemplateByTypeTag(childtype)) == NULL)
	return SECFailure;

    childp7dcx = PORT_ZNew(NSSCMSDecoderContext);
    if (childp7dcx == NULL)
	return SECFailure;

    mark = PORT_ArenaMark(poolp);

    /* allocate space for the stuff we're creating */
    size = NSS_CMSUtil_GetSizeByTypeTag(childtype);
    childp7dcx->content.pointer = (void *)PORT_ArenaZAlloc(poolp, size);
    if (childp7dcx->content.pointer == NULL)
	goto loser;

    /* give the parent a copy of the pointer so that it doesn't get lost */
    cinfo->content.pointer = childp7dcx->content.pointer;

    /* start the child decoder */
    childp7dcx->dcx = SEC_ASN1DecoderStart(poolp, childp7dcx->content.pointer, 
                                           template);
    if (childp7dcx->dcx == NULL)
	goto loser;

    /* the new decoder needs to notify, too */
    SEC_ASN1DecoderSetNotifyProc(childp7dcx->dcx, nss_cms_decoder_notify, 
                                 childp7dcx);

    /* tell the parent decoder that it needs to feed us the content data */
    p7dcx->childp7dcx = childp7dcx;

    childp7dcx->type = childtype;	/* our type */

    childp7dcx->cmsg = p7dcx->cmsg;	/* backpointer to root message */

    /* should the child decoder encounter real data, 
    ** it must give it to the caller 
    */
    childp7dcx->cb = p7dcx->cb;
    childp7dcx->cb_arg = p7dcx->cb_arg;
    childp7dcx->first_decoded = PR_FALSE;
    childp7dcx->need_indefinite_finish = PR_FALSE;
    if (childtype == SEC_OID_PKCS7_SIGNED_DATA) {
	childp7dcx->first_decoded = PR_TRUE;
    }

    /* now set up the parent to hand decoded data to the next level decoder */
    p7dcx->cb = (NSSCMSContentCallback)NSS_CMSDecoder_Update;
    p7dcx->cb_arg = childp7dcx;

    PORT_ArenaUnmark(poolp, mark);

    return SECSuccess;

loser:
    if (mark)
	PORT_ArenaRelease(poolp, mark);
    if (childp7dcx)
	PORT_Free(childp7dcx);
    p7dcx->childp7dcx = NULL;
    return SECFailure;
}

static SECStatus
nss_cms_after_data(NSSCMSDecoderContext *p7dcx)
{
    NSSCMSDecoderContext *childp7dcx;
    SECStatus rv = SECFailure;

    /* Handle last block. This is necessary to flush out the last bytes
     * of a possibly incomplete block */
    nss_cms_decoder_work_data(p7dcx, NULL, 0, PR_TRUE);

    /* finish any "inner" decoders - there's no more data coming... */
    if (p7dcx->childp7dcx != NULL) {
	childp7dcx = p7dcx->childp7dcx;
	if (childp7dcx->dcx != NULL) {
	    /* we started and indefinite sequence somewhere, not complete it */
	    if (childp7dcx->need_indefinite_finish) {
		static const char lbuf[2] = { 0, 0 };
		NSS_CMSDecoder_Update(childp7dcx, lbuf, sizeof(lbuf));
		childp7dcx->need_indefinite_finish = PR_FALSE;
	    }

	    if (SEC_ASN1DecoderFinish(childp7dcx->dcx) != SECSuccess) {
		/* do what? free content? */
		rv = SECFailure;
	    } else {
		rv = nss_cms_after_end(childp7dcx);
	    }
	    if (rv != SECSuccess)
		goto done;
	}
	PORT_Free(p7dcx->childp7dcx);
	p7dcx->childp7dcx = NULL;
    }

    switch (p7dcx->type) {
    case SEC_OID_PKCS7_SIGNED_DATA:
	/* this will finish the digests and verify */
	rv = NSS_CMSSignedData_Decode_AfterData(p7dcx->content.signedData);
	break;
    case SEC_OID_PKCS7_ENVELOPED_DATA:
	rv = NSS_CMSEnvelopedData_Decode_AfterData(
	                            p7dcx->content.envelopedData);
	break;
    case SEC_OID_PKCS7_DIGESTED_DATA:
	rv = NSS_CMSDigestedData_Decode_AfterData(
	                           p7dcx->content.digestedData);
	break;
    case SEC_OID_PKCS7_ENCRYPTED_DATA:
	rv = NSS_CMSEncryptedData_Decode_AfterData(
	                            p7dcx->content.encryptedData);
	break;
    case SEC_OID_PKCS7_DATA:
	/* do nothing */
	break;
    default:
	rv = NSS_CMSGenericWrapperData_Decode_AfterData(p7dcx->type,
	                            p7dcx->content.genericData);
	break;
    }
done:
    return rv;
}

static SECStatus
nss_cms_after_end(NSSCMSDecoderContext *p7dcx)
{
    SECStatus rv = SECSuccess;

    switch (p7dcx->type) {
    case SEC_OID_PKCS7_SIGNED_DATA:
	if (p7dcx->content.signedData)
	    rv = NSS_CMSSignedData_Decode_AfterEnd(p7dcx->content.signedData);
	break;
    case SEC_OID_PKCS7_ENVELOPED_DATA:
	if (p7dcx->content.envelopedData)
	    rv = NSS_CMSEnvelopedData_Decode_AfterEnd(
	                               p7dcx->content.envelopedData);
	break;
    case SEC_OID_PKCS7_DIGESTED_DATA:
	if (p7dcx->content.digestedData)
	    rv = NSS_CMSDigestedData_Decode_AfterEnd(
	                              p7dcx->content.digestedData);
	break;
    case SEC_OID_PKCS7_ENCRYPTED_DATA:
	if (p7dcx->content.encryptedData)
	    rv = NSS_CMSEncryptedData_Decode_AfterEnd(
	                               p7dcx->content.encryptedData);
	break;
    case SEC_OID_PKCS7_DATA:
	break;
    default:
	rv = NSS_CMSGenericWrapperData_Decode_AfterEnd(p7dcx->type,
	                               p7dcx->content.genericData);
	break;
    }
    return rv;
}

/*
 * nss_cms_decoder_work_data - handle decoded data bytes.
 *
 * This function either decrypts the data if needed, and/or calculates digests
 * on it, then either stores it or passes it on to the next level decoder.
 */
static void
nss_cms_decoder_work_data(NSSCMSDecoderContext *p7dcx, 
			     const unsigned char *data, unsigned long len,
			     PRBool final)
{
    NSSCMSContentInfo *cinfo;
    unsigned char *buf = NULL;
    unsigned char *dest;
    unsigned int offset;
    SECStatus rv;

    /*
     * We should really have data to process, or we should be trying
     * to finish/flush the last block.  (This is an overly paranoid
     * check since all callers are in this file and simple inspection
     * proves they do it right.  But it could find a bug in future
     * modifications/development, that is why it is here.)
     */
    PORT_Assert ((data != NULL && len) || final);

    cinfo = NSS_CMSContent_GetContentInfo(p7dcx->content.pointer, p7dcx->type);
    if (!cinfo) {
	/* The original programmer didn't expect this to happen */
	p7dcx->error = SEC_ERROR_LIBRARY_FAILURE;
	goto loser;
    }

    if (cinfo->privateInfo && cinfo->privateInfo->ciphcx != NULL) {
	/*
	 * we are decrypting.
	 * 
	 * XXX If we get an error, we do not want to do the digest or callback,
	 * but we want to keep decoding.  Or maybe we want to stop decoding
	 * altogether if there is a callback, because obviously we are not
	 * sending the data back and they want to know that.
	 */

	unsigned int outlen = 0;	/* length of decrypted data */
	unsigned int buflen;		/* length available for decrypted data */

	/* find out about the length of decrypted data */
	buflen = NSS_CMSCipherContext_DecryptLength(cinfo->privateInfo->ciphcx, len, final);

	/*
	 * it might happen that we did not provide enough data for a full
	 * block (decryption unit), and that there is no output available
	 */

	/* no output available, AND no input? */
	if (buflen == 0 && len == 0)
	    goto loser;	/* bail out */

	/*
	 * have inner decoder: pass the data on (means inner content type is NOT data)
	 * no inner decoder: we have DATA in here: either call callback or store
	 */
	if (buflen != 0) {
	    /* there will be some output - need to make room for it */
	    /* allocate buffer from the heap */
	    buf = (unsigned char *)PORT_Alloc(buflen);
	    if (buf == NULL) {
		p7dcx->error = SEC_ERROR_NO_MEMORY;
		goto loser;
	    }
	}

	/*
	 * decrypt incoming data
	 * buf can still be NULL here (and buflen == 0) here if we don't expect
	 * any output (see above), but we still need to call NSS_CMSCipherContext_Decrypt to
	 * keep track of incoming data
	 */
	rv = NSS_CMSCipherContext_Decrypt(cinfo->privateInfo->ciphcx, buf, &outlen, buflen,
			       data, len, final);
	if (rv != SECSuccess) {
	    p7dcx->error = PORT_GetError();
	    goto loser;
	}

	PORT_Assert (final || outlen == buflen);
	
	/* swap decrypted data in */
	data = buf;
	len = outlen;
    }

    if (len == 0)
	goto done;		/* nothing more to do */

    /*
     * Update the running digests with plaintext bytes (if we need to).
     */
    if (cinfo->privateInfo && cinfo->privateInfo->digcx)
	NSS_CMSDigestContext_Update(cinfo->privateInfo->digcx, data, len);

    /* at this point, we have the plain decoded & decrypted data 
    ** which is either more encoded DER (which we need to hand to the child 
    ** decoder) or data we need to hand back to our caller 
    */

    /* pass the content back to our caller or */
    /* feed our freshly decrypted and decoded data into child decoder */
    if (p7dcx->cb != NULL) {
	(*p7dcx->cb)(p7dcx->cb_arg, (const char *)data, len);
    }
#if 1
    else
#endif
    if (NSS_CMSContentInfo_GetContentTypeTag(cinfo) == SEC_OID_PKCS7_DATA) {
	/* store it in "inner" data item as well */
	/* find the DATA item in the encapsulated cinfo and store it there */
	NSSCMSDecoderData *decoderData = 
				(NSSCMSDecoderData *)cinfo->content.pointer;
	SECItem *dataItem = &decoderData->data;

	offset = dataItem->len;
	if (dataItem->len+len > decoderData->totalBufferSize) {
	    int needLen = (dataItem->len+len) * 2;
	    dest = (unsigned char *)
				PORT_ArenaAlloc(p7dcx->cmsg->poolp, needLen);
	    if (dest == NULL) {
		p7dcx->error = SEC_ERROR_NO_MEMORY;
		goto loser;
	    }

	    if (dataItem->len) {
		PORT_Memcpy(dest, dataItem->data, dataItem->len);
	    }
	    decoderData->totalBufferSize = needLen;
	    dataItem->data = dest;
	}

	/* copy it in */
	PORT_Memcpy(dataItem->data + offset, data, len);
	dataItem->len += len;
    }

done:
loser:
    if (buf)
	PORT_Free (buf);
}

/*
 * nss_cms_decoder_update_filter - process ASN.1 data
 *
 * once we have set up a filter in nss_cms_decoder_notify(),
 * all data processed by the ASN.1 decoder is also passed through here.
 * we pass the content bytes (as opposed to length and tag bytes) on to
 * nss_cms_decoder_work_data().
 */
static void
nss_cms_decoder_update_filter (void *arg, const char *data, unsigned long len,
			  int depth, SEC_ASN1EncodingPart data_kind)
{
    NSSCMSDecoderContext *p7dcx;

    PORT_Assert (len);	/* paranoia */
    if (len == 0)
	return;

    p7dcx = (NSSCMSDecoderContext*)arg;

    p7dcx->saw_contents = PR_TRUE;

    /* pass on the content bytes only */
    if (data_kind == SEC_ASN1_Contents)
	nss_cms_decoder_work_data(p7dcx, (const unsigned char *) data, len, 
	                          PR_FALSE);
}

/*
 * NSS_CMSDecoder_Start - set up decoding of a DER-encoded CMS message
 *
 * "poolp" - pointer to arena for message, or NULL if new pool should be created
 * "cb", "cb_arg" - callback function and argument for delivery of inner content
 * "pwfn", pwfn_arg" - callback function for getting token password
 * "decrypt_key_cb", "decrypt_key_cb_arg" - callback function for getting bulk key for encryptedData
 */
NSSCMSDecoderContext *
NSS_CMSDecoder_Start(PRArenaPool *poolp,
		      NSSCMSContentCallback cb, void *cb_arg,
		      PK11PasswordFunc pwfn, void *pwfn_arg,
		      NSSCMSGetDecryptKeyCallback decrypt_key_cb, 
		      void *decrypt_key_cb_arg)
{
    NSSCMSDecoderContext *p7dcx;
    NSSCMSMessage *cmsg;

    cmsg = NSS_CMSMessage_Create(poolp);
    if (cmsg == NULL)
	return NULL;

    NSS_CMSMessage_SetEncodingParams(cmsg, pwfn, pwfn_arg, decrypt_key_cb, 
                                     decrypt_key_cb_arg, NULL, NULL);

    p7dcx = PORT_ZNew(NSSCMSDecoderContext);
    if (p7dcx == NULL) {
	NSS_CMSMessage_Destroy(cmsg);
	return NULL;
    }

    p7dcx->dcx = SEC_ASN1DecoderStart(cmsg->poolp, cmsg, NSSCMSMessageTemplate);
    if (p7dcx->dcx == NULL) {
	PORT_Free (p7dcx);
	NSS_CMSMessage_Destroy(cmsg);
	return NULL;
    }

    SEC_ASN1DecoderSetNotifyProc (p7dcx->dcx, nss_cms_decoder_notify, p7dcx);

    p7dcx->cmsg = cmsg;
    p7dcx->type = SEC_OID_UNKNOWN;

    p7dcx->cb = cb;
    p7dcx->cb_arg = cb_arg;
    p7dcx->first_decoded = PR_FALSE;
    p7dcx->need_indefinite_finish = PR_FALSE;
    return p7dcx;
}

/*
 * NSS_CMSDecoder_Update - feed DER-encoded data to decoder
 */
SECStatus
NSS_CMSDecoder_Update(NSSCMSDecoderContext *p7dcx, const char *buf, 
                      unsigned long len)
{
    SECStatus rv = SECSuccess;
    if (p7dcx->dcx != NULL && p7dcx->error == 0) {	
    	/* if error is set already, don't bother */
	if ((p7dcx->type == SEC_OID_PKCS7_SIGNED_DATA) 
		&& (p7dcx->first_decoded==PR_TRUE)
		&& (buf[0] == SEC_ASN1_INTEGER)) {
	    /* Microsoft Windows 2008 left out the Sequence wrapping in some
	     * of their kerberos replies. If we are here, we most likely are
	     * dealing with one of those replies. Supply the Sequence wrap
	     * as indefinite encoding (since we don't know the total length
	     * yet) */
	     static const char lbuf[2] = 
		{ SEC_ASN1_SEQUENCE|SEC_ASN1_CONSTRUCTED, 0x80 };
	     rv = SEC_ASN1DecoderUpdate(p7dcx->dcx, lbuf, sizeof(lbuf));
	     if (rv != SECSuccess) {
		goto loser;
	    }
	    /* ok, we're going to need the indefinite finish when we are done */
	    p7dcx->need_indefinite_finish = PR_TRUE;
	}
	
	rv = SEC_ASN1DecoderUpdate(p7dcx->dcx, buf, len);
    }

loser:
    p7dcx->first_decoded = PR_FALSE;
    if (rv != SECSuccess) {
	p7dcx->error = PORT_GetError();
	PORT_Assert (p7dcx->error);
	if (p7dcx->error == 0)
	    p7dcx->error = -1;
    }

    if (p7dcx->error == 0)
	return SECSuccess;

    /* there has been a problem, let's finish the decoder */
    if (p7dcx->dcx != NULL) {
	(void) SEC_ASN1DecoderFinish (p7dcx->dcx);
	p7dcx->dcx = NULL;
    }
    PORT_SetError (p7dcx->error);

    return SECFailure;
}

/*
 * NSS_CMSDecoder_Cancel - stop decoding in case of error
 */
void
NSS_CMSDecoder_Cancel(NSSCMSDecoderContext *p7dcx)
{
    if (p7dcx->dcx != NULL)
	(void)SEC_ASN1DecoderFinish(p7dcx->dcx);
    NSS_CMSMessage_Destroy(p7dcx->cmsg);
    PORT_Free(p7dcx);
}

/*
 * NSS_CMSDecoder_Finish - mark the end of inner content and finish decoding
 */
NSSCMSMessage *
NSS_CMSDecoder_Finish(NSSCMSDecoderContext *p7dcx)
{
    NSSCMSMessage *cmsg;

    cmsg = p7dcx->cmsg;

    if (p7dcx->dcx == NULL || 
        SEC_ASN1DecoderFinish(p7dcx->dcx) != SECSuccess ||
	nss_cms_after_end(p7dcx) != SECSuccess)
    {
	NSS_CMSMessage_Destroy(cmsg);	/* get rid of pool if it's ours */
	cmsg = NULL;
    }

    PORT_Free(p7dcx);
    return cmsg;
}

NSSCMSMessage *
NSS_CMSMessage_CreateFromDER(SECItem *DERmessage,
		    NSSCMSContentCallback cb, void *cb_arg,
		    PK11PasswordFunc pwfn, void *pwfn_arg,
		    NSSCMSGetDecryptKeyCallback decrypt_key_cb, 
		    void *decrypt_key_cb_arg)
{
    NSSCMSDecoderContext *p7dcx;

    /* first arg(poolp) == NULL => create our own pool */
    p7dcx = NSS_CMSDecoder_Start(NULL, cb, cb_arg, pwfn, pwfn_arg, 
                                 decrypt_key_cb, decrypt_key_cb_arg);
    if (p7dcx == NULL)
	return NULL;
    NSS_CMSDecoder_Update(p7dcx, (char *)DERmessage->data, DERmessage->len);
    return NSS_CMSDecoder_Finish(p7dcx);
}

