/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * PKCS7 implementation -- the exported parts that are used whether
 * creating or decoding.
 *
 * $Id: p7common.c,v 1.9 2012/04/25 14:50:06 gerv%gerv.net Exp $
 */

#include "p7local.h"

#include "cert.h"
#include "secitem.h"
#include "secoid.h"
#include "pk11func.h"

/*
 * Find out (saving pointer to lookup result for future reference)
 * and return the inner content type.
 */
SECOidTag
SEC_PKCS7ContentType (SEC_PKCS7ContentInfo *cinfo)
{
    if (cinfo->contentTypeTag == NULL)
	cinfo->contentTypeTag = SECOID_FindOID(&(cinfo->contentType));

    if (cinfo->contentTypeTag == NULL)
	return SEC_OID_UNKNOWN;

    return cinfo->contentTypeTag->offset;
}


/*
 * Destroy a PKCS7 contentInfo and all of its sub-pieces.
 */
void
SEC_PKCS7DestroyContentInfo(SEC_PKCS7ContentInfo *cinfo)
{
    SECOidTag kind;
    CERTCertificate **certs;
    CERTCertificateList **certlists;
    SEC_PKCS7SignerInfo **signerinfos;
    SEC_PKCS7RecipientInfo **recipientinfos;

    PORT_Assert (cinfo->refCount > 0);
    if (cinfo->refCount <= 0)
	return;

    cinfo->refCount--;
    if (cinfo->refCount > 0)
	return;

    certs = NULL;
    certlists = NULL;
    recipientinfos = NULL;
    signerinfos = NULL;

    kind = SEC_PKCS7ContentType (cinfo);
    switch (kind) {
      case SEC_OID_PKCS7_ENVELOPED_DATA:
	{
	    SEC_PKCS7EnvelopedData *edp;

	    edp = cinfo->content.envelopedData;
	    if (edp != NULL) {
		recipientinfos = edp->recipientInfos;
	    }
	}
	break;
      case SEC_OID_PKCS7_SIGNED_DATA:
	{
	    SEC_PKCS7SignedData *sdp;

	    sdp = cinfo->content.signedData;
	    if (sdp != NULL) {
		certs = sdp->certs;
		certlists = sdp->certLists;
		signerinfos = sdp->signerInfos;
	    }
	}
	break;
      case SEC_OID_PKCS7_SIGNED_ENVELOPED_DATA:
	{
	    SEC_PKCS7SignedAndEnvelopedData *saedp;

	    saedp = cinfo->content.signedAndEnvelopedData;
	    if (saedp != NULL) {
		certs = saedp->certs;
		certlists = saedp->certLists;
		recipientinfos = saedp->recipientInfos;
		signerinfos = saedp->signerInfos;
		if (saedp->sigKey != NULL)
		    PK11_FreeSymKey (saedp->sigKey);
	    }
	}
	break;
      default:
	/* XXX Anything else that needs to be "manually" freed/destroyed? */
	break;
    }

    if (certs != NULL) {
	CERTCertificate *cert;

	while ((cert = *certs++) != NULL) {
	    CERT_DestroyCertificate (cert);
	}
    }

    if (certlists != NULL) {
	CERTCertificateList *certlist;

	while ((certlist = *certlists++) != NULL) {
	    CERT_DestroyCertificateList (certlist);
	}
    }

    if (recipientinfos != NULL) {
	SEC_PKCS7RecipientInfo *ri;

	while ((ri = *recipientinfos++) != NULL) {
	    if (ri->cert != NULL)
		CERT_DestroyCertificate (ri->cert);
	}
    }

    if (signerinfos != NULL) {
	SEC_PKCS7SignerInfo *si;

	while ((si = *signerinfos++) != NULL) {
	    if (si->cert != NULL)
		CERT_DestroyCertificate (si->cert);
	    if (si->certList != NULL)
		CERT_DestroyCertificateList (si->certList);
	}
    }

    if (cinfo->poolp != NULL) {
	PORT_FreeArena (cinfo->poolp, PR_FALSE);	/* XXX clear it? */
    }
}


/*
 * Return a copy of the given contentInfo.  The copy may be virtual
 * or may be real -- either way, the result needs to be passed to
 * SEC_PKCS7DestroyContentInfo later (as does the original).
 */
SEC_PKCS7ContentInfo *
SEC_PKCS7CopyContentInfo(SEC_PKCS7ContentInfo *cinfo)
{
    if (cinfo == NULL)
	return NULL;

    PORT_Assert (cinfo->refCount > 0);

    if (cinfo->created) {
	/*
	 * Want to do a real copy of these; otherwise subsequent
	 * changes made to either copy are likely to be a surprise.
	 * XXX I suspect that this will not actually be called for yet,
	 * which is why the assert, so to notice if it is...
	 */
	PORT_Assert (0);
	/*
	 * XXX Create a new pool here, and copy everything from
	 * within.  For cert stuff, need to call the appropriate
	 * copy functions, etc.
	 */
    }

    cinfo->refCount++;
    return cinfo;
}


/*
 * Return a pointer to the actual content.  In the case of those types
 * which are encrypted, this returns the *plain* content.
 * XXX Needs revisiting if/when we handle nested encrypted types.
 */
SECItem *
SEC_PKCS7GetContent(SEC_PKCS7ContentInfo *cinfo)
{
    SECOidTag kind;

    kind = SEC_PKCS7ContentType (cinfo);
    switch (kind) {
      case SEC_OID_PKCS7_DATA:
	return cinfo->content.data;
      case SEC_OID_PKCS7_DIGESTED_DATA:
	{
	    SEC_PKCS7DigestedData *digd;

	    digd = cinfo->content.digestedData;
	    if (digd == NULL)
		break;
	    return SEC_PKCS7GetContent (&(digd->contentInfo));
	}
      case SEC_OID_PKCS7_ENCRYPTED_DATA:
	{
	    SEC_PKCS7EncryptedData *encd;

	    encd = cinfo->content.encryptedData;
	    if (encd == NULL)
		break;
	    return &(encd->encContentInfo.plainContent);
	}
      case SEC_OID_PKCS7_ENVELOPED_DATA:
	{
	    SEC_PKCS7EnvelopedData *envd;

	    envd = cinfo->content.envelopedData;
	    if (envd == NULL)
		break;
	    return &(envd->encContentInfo.plainContent);
	}
      case SEC_OID_PKCS7_SIGNED_DATA:
	{
	    SEC_PKCS7SignedData *sigd;

	    sigd = cinfo->content.signedData;
	    if (sigd == NULL)
		break;
	    return SEC_PKCS7GetContent (&(sigd->contentInfo));
	}
      case SEC_OID_PKCS7_SIGNED_ENVELOPED_DATA:
	{
	    SEC_PKCS7SignedAndEnvelopedData *saed;

	    saed = cinfo->content.signedAndEnvelopedData;
	    if (saed == NULL)
		break;
	    return &(saed->encContentInfo.plainContent);
	}
      default:
	PORT_Assert(0);
	break;
    }

    return NULL;
}


/*
 * XXX Fix the placement and formatting of the
 * following routines (i.e. make them consistent with the rest of
 * the pkcs7 code -- I think some/many belong in other files and
 * they all need a formatting/style rehaul)
 */

/* retrieve the algorithm identifier for encrypted data.  
 * the identifier returned is a copy of the algorithm identifier
 * in the content info and needs to be freed after being used.
 *
 *   cinfo is the content info for which to retrieve the
 *     encryption algorithm.
 *
 * if the content info is not encrypted data or an error 
 * occurs NULL is returned.
 */
SECAlgorithmID *
SEC_PKCS7GetEncryptionAlgorithm(SEC_PKCS7ContentInfo *cinfo)
{
  SECAlgorithmID *alg = 0;
  switch (SEC_PKCS7ContentType(cinfo))
    {
    case SEC_OID_PKCS7_ENCRYPTED_DATA:
      alg = &cinfo->content.encryptedData->encContentInfo.contentEncAlg;
      break;
    case SEC_OID_PKCS7_ENVELOPED_DATA:
      alg = &cinfo->content.envelopedData->encContentInfo.contentEncAlg;
      break;
    case SEC_OID_PKCS7_SIGNED_ENVELOPED_DATA:
      alg = &cinfo->content.signedAndEnvelopedData
	->encContentInfo.contentEncAlg;
      break;
    default:
      alg = 0;
      break;
    }

    return alg;
}

/* set the content of the content info.  For data content infos,
 * the data is set.  For encrytped content infos, the plainContent
 * is set, and is expected to be encrypted later.
 *  
 * cinfo is the content info where the data will be set
 *
 * buf is a buffer of the data to set
 *
 * len is the length of the data being set.
 *
 * in the event of an error, SECFailure is returned.  SECSuccess 
 * indicates the content was successfully set.
 */
SECStatus 
SEC_PKCS7SetContent(SEC_PKCS7ContentInfo *cinfo,
		    const char *buf, 
		    unsigned long len)
{
    SECOidTag cinfo_type;
    SECStatus rv;
    SECItem content;
    SECOidData *contentTypeTag = NULL;

    content.type = siBuffer;
    content.data = (unsigned char *)buf;
    content.len = len;

    cinfo_type = SEC_PKCS7ContentType(cinfo);

    /* set inner content */
    switch(cinfo_type)
    {
	case SEC_OID_PKCS7_SIGNED_DATA:
	    if(content.len > 0) {
		/* we "leak" the old content here, but as it's all in the pool */
		/* it does not really matter */

		/* create content item if necessary */
		if (cinfo->content.signedData->contentInfo.content.data == NULL)
		    cinfo->content.signedData->contentInfo.content.data = SECITEM_AllocItem(cinfo->poolp, NULL, 0);
		rv = SECITEM_CopyItem(cinfo->poolp, 
			cinfo->content.signedData->contentInfo.content.data,
			&content);
	    } else {
		cinfo->content.signedData->contentInfo.content.data->data = NULL;
		cinfo->content.signedData->contentInfo.content.data->len = 0;
		rv = SECSuccess;
	    }
	    if(rv == SECFailure)
		goto loser;
	    
	    break;
	case SEC_OID_PKCS7_ENCRYPTED_DATA:
	    /* XXX this forces the inner content type to be "data" */
	    /* do we really want to override without asking or reason? */
	    contentTypeTag = SECOID_FindOIDByTag(SEC_OID_PKCS7_DATA);
	    if(contentTypeTag == NULL)
		goto loser;
	    rv = SECITEM_CopyItem(cinfo->poolp, 
		&(cinfo->content.encryptedData->encContentInfo.contentType),
		&(contentTypeTag->oid));
	    if(rv == SECFailure)
		goto loser;
	    if(content.len > 0) {
		rv = SECITEM_CopyItem(cinfo->poolp, 
			&(cinfo->content.encryptedData->encContentInfo.plainContent),
			&content);
	    } else {
		cinfo->content.encryptedData->encContentInfo.plainContent.data = NULL;
		cinfo->content.encryptedData->encContentInfo.encContent.data = NULL;
		cinfo->content.encryptedData->encContentInfo.plainContent.len = 0;
		cinfo->content.encryptedData->encContentInfo.encContent.len = 0;
		rv = SECSuccess;
	    }
	    if(rv == SECFailure)
		goto loser;
	    break;
	case SEC_OID_PKCS7_DATA:
	    cinfo->content.data = (SECItem *)PORT_ArenaZAlloc(cinfo->poolp,
		sizeof(SECItem));
	    if(cinfo->content.data == NULL)
		goto loser;
	    if(content.len > 0) {
		rv = SECITEM_CopyItem(cinfo->poolp,
			cinfo->content.data, &content);
	    } else {
	    	/* handle case with NULL content */
		rv = SECSuccess;
	    }
	    if(rv == SECFailure)
		goto loser;
	    break;
	default:
	    goto loser;
    }

    return SECSuccess;

loser:
	
    return SECFailure;
}

/* the content of an encrypted data content info is encrypted.
 * it is assumed that for encrypted data, that the data has already
 * been set and is in the "plainContent" field of the content info.
 *
 * cinfo is the content info to encrypt
 *
 * key is the key with which to perform the encryption.  if the
 *     algorithm is a password based encryption algorithm, the
 *     key is actually a password which will be processed per
 *     PKCS #5.
 * 
 * in the event of an error, SECFailure is returned.  SECSuccess
 * indicates a success.
 */
SECStatus 
SEC_PKCS7EncryptContents(PRArenaPool *poolp,
			 SEC_PKCS7ContentInfo *cinfo,
			 SECItem *key,
			 void *wincx)
{
    SECAlgorithmID *algid 	= NULL;
    SECItem *       result 	= NULL;
    SECItem *       src;
    SECItem *       dest;
    SECItem *       blocked_data = NULL;
    void *          mark;
    void *          cx;
    PK11SymKey *    eKey 	= NULL;
    PK11SlotInfo *  slot 	= NULL;

    CK_MECHANISM_TYPE cryptoMechType;
    int             bs;
    SECStatus       rv 		= SECFailure;
    SECItem         *c_param = NULL;

    if((cinfo == NULL) || (key == NULL))
	return SECFailure;

    if(SEC_PKCS7ContentType(cinfo) != SEC_OID_PKCS7_ENCRYPTED_DATA)
	return SECFailure;

    algid = SEC_PKCS7GetEncryptionAlgorithm(cinfo);	
    if(algid == NULL)
	return SECFailure;

    if(poolp == NULL)
	poolp = cinfo->poolp;

    mark = PORT_ArenaMark(poolp);
    
    src = &cinfo->content.encryptedData->encContentInfo.plainContent;
    dest = &cinfo->content.encryptedData->encContentInfo.encContent;
    dest->data = (unsigned char*)PORT_ArenaZAlloc(poolp, (src->len + 64));
    dest->len = (src->len + 64);
    if(dest->data == NULL) {
	rv = SECFailure;
	goto loser;
    }

    slot = PK11_GetInternalKeySlot();
    if(slot == NULL) {
	rv = SECFailure;
	goto loser;
    }

    eKey = PK11_PBEKeyGen(slot, algid, key, PR_FALSE, wincx);
    if(eKey == NULL) {
	rv = SECFailure;
	goto loser;
    }
    
    cryptoMechType = PK11_GetPBECryptoMechanism(algid, &c_param, key);
    if (cryptoMechType == CKM_INVALID_MECHANISM) {
	rv = SECFailure;
	goto loser;
    }

    /* block according to PKCS 8 */
    bs = PK11_GetBlockSize(cryptoMechType, c_param);
    rv = SECSuccess;
    if(bs) {
	char pad_char;
	pad_char = (char)(bs - (src->len % bs));
	if(src->len % bs) {
	    rv = SECSuccess;
	    blocked_data = PK11_BlockData(src, bs);
	    if(blocked_data) {
		PORT_Memset((blocked_data->data + blocked_data->len 
			    - (int)pad_char), 
			    pad_char, (int)pad_char);
	    } else {
		rv = SECFailure;
		goto loser;
	    }
	} else {
	    blocked_data = SECITEM_DupItem(src);
	    if(blocked_data) {
		blocked_data->data = (unsigned char*)PORT_Realloc(
						  blocked_data->data,
						  blocked_data->len + bs);
		if(blocked_data->data) {
		    blocked_data->len += bs;
		    PORT_Memset((blocked_data->data + src->len), (char)bs, bs);
		} else {
		    rv = SECFailure;
		    goto loser;
		}
	    } else {
		rv = SECFailure;
		goto loser;
	    }
	 }
    } else {
	blocked_data = SECITEM_DupItem(src);
	if(!blocked_data) {
	    rv = SECFailure;
	    goto loser;
	}
    }

    cx = PK11_CreateContextBySymKey(cryptoMechType, CKA_ENCRYPT,
		    		    eKey, c_param);
    if(cx == NULL) {
	rv = SECFailure;
	goto loser;
    }

    rv = PK11_CipherOp((PK11Context*)cx, dest->data, (int *)(&dest->len), 
		       (int)(src->len + 64), blocked_data->data, 
		       (int)blocked_data->len);
    PK11_DestroyContext((PK11Context*)cx, PR_TRUE);

loser:
    /* let success fall through */
    if(blocked_data != NULL)
	SECITEM_ZfreeItem(blocked_data, PR_TRUE);

    if(result != NULL)
	SECITEM_ZfreeItem(result, PR_TRUE);

    if(rv == SECFailure)
	PORT_ArenaRelease(poolp, mark);
    else 
	PORT_ArenaUnmark(poolp, mark);

    if(eKey != NULL)
	PK11_FreeSymKey(eKey);

    if(slot != NULL)
	PK11_FreeSlot(slot);

    if(c_param != NULL) 
	SECITEM_ZfreeItem(c_param, PR_TRUE);
	
    return rv;
}

/* the content of an encrypted data content info is decrypted.
 * it is assumed that for encrypted data, that the data has already
 * been set and is in the "encContent" field of the content info.
 *
 * cinfo is the content info to decrypt
 *
 * key is the key with which to perform the decryption.  if the
 *     algorithm is a password based encryption algorithm, the
 *     key is actually a password which will be processed per
 *     PKCS #5.
 * 
 * in the event of an error, SECFailure is returned.  SECSuccess
 * indicates a success.
 */
SECStatus 
SEC_PKCS7DecryptContents(PRArenaPool *poolp,
			 SEC_PKCS7ContentInfo *cinfo,
			 SECItem *key,
			 void *wincx)
{
    SECAlgorithmID *algid = NULL;
    SECStatus rv = SECFailure;
    SECItem *result = NULL, *dest, *src;
    void *mark;

    PK11SymKey *eKey = NULL;
    PK11SlotInfo *slot = NULL;
    CK_MECHANISM_TYPE cryptoMechType;
    void *cx;
    SECItem *c_param = NULL;
    int bs;

    if((cinfo == NULL) || (key == NULL))
	return SECFailure;

    if(SEC_PKCS7ContentType(cinfo) != SEC_OID_PKCS7_ENCRYPTED_DATA)
	return SECFailure;

    algid = SEC_PKCS7GetEncryptionAlgorithm(cinfo);	
    if(algid == NULL)
	return SECFailure;

    if(poolp == NULL)
	poolp = cinfo->poolp;

    mark = PORT_ArenaMark(poolp);
    
    src = &cinfo->content.encryptedData->encContentInfo.encContent;
    dest = &cinfo->content.encryptedData->encContentInfo.plainContent;
    dest->data = (unsigned char*)PORT_ArenaZAlloc(poolp, (src->len + 64));
    dest->len = (src->len + 64);
    if(dest->data == NULL) {
	rv = SECFailure;
	goto loser;
    }

    slot = PK11_GetInternalKeySlot();
    if(slot == NULL) {
	rv = SECFailure;
	goto loser;
    }

    eKey = PK11_PBEKeyGen(slot, algid, key, PR_FALSE, wincx);
    if(eKey == NULL) {
	rv = SECFailure;
	goto loser;
    }
    
    cryptoMechType = PK11_GetPBECryptoMechanism(algid, &c_param, key);
    if (cryptoMechType == CKM_INVALID_MECHANISM) {
	rv = SECFailure;
	goto loser;
    }

    cx = PK11_CreateContextBySymKey(cryptoMechType, CKA_DECRYPT,
		    		    eKey, c_param);
    if(cx == NULL) {
	rv = SECFailure;
	goto loser;
    }

    rv = PK11_CipherOp((PK11Context*)cx, dest->data, (int *)(&dest->len), 
		       (int)(src->len + 64), src->data, (int)src->len);
    PK11_DestroyContext((PK11Context *)cx, PR_TRUE);

    bs = PK11_GetBlockSize(cryptoMechType, c_param);
    if(bs) {
	/* check for proper badding in block algorithms.  this assumes
	 * RC2 cbc or a DES cbc variant.  and the padding is thus defined
	 */
	if(((int)dest->data[dest->len-1] <= bs) && 
	   ((int)dest->data[dest->len-1] > 0)) {
	    dest->len -= (int)dest->data[dest->len-1];
	} else {
	    rv = SECFailure;
	    /* set an error ? */
	}
    } 

loser:
    /* let success fall through */
    if(result != NULL)
	SECITEM_ZfreeItem(result, PR_TRUE);

    if(rv == SECFailure)
	PORT_ArenaRelease(poolp, mark);
    else
	PORT_ArenaUnmark(poolp, mark);

    if(eKey != NULL)
	PK11_FreeSymKey(eKey);

    if(slot != NULL)
	PK11_FreeSlot(slot);

    if(c_param != NULL) 
	SECITEM_ZfreeItem(c_param, PR_TRUE);
	
    return rv;
}

SECItem **
SEC_PKCS7GetCertificateList(SEC_PKCS7ContentInfo *cinfo)
{
    switch(SEC_PKCS7ContentType(cinfo))
    {
	case SEC_OID_PKCS7_SIGNED_DATA:
	    return cinfo->content.signedData->rawCerts;
	    break;
	default:
	    return NULL;
	    break;
    }
}


int
SEC_PKCS7GetKeyLength(SEC_PKCS7ContentInfo *cinfo)
{
  if (cinfo->contentTypeTag->offset == SEC_OID_PKCS7_ENVELOPED_DATA)
    return cinfo->content.envelopedData->encContentInfo.keysize;
  else
    return 0;
}

