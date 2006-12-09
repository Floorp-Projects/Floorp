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
 *   Sun Microsystems
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
 * pkix_pl_httpcertstore.c
 *
 * HTTPCertStore Function Definitions
 *
 */

/* We can't decode the length of a message without at least this many bytes */

#include "pkix_pl_httpcertstore.h"
extern PKIX_PL_HashTable *httpSocketCache;
static const SEC_ASN1Template sec_PKCS7ContentInfoTemplate[4];
SEC_ASN1_MKSUB(CERT_IssuerAndSNTemplate)
SEC_ASN1_MKSUB(SECOID_AlgorithmIDTemplate)
SEC_ASN1_MKSUB(SEC_SetOfAnyTemplate)
SEC_ASN1_MKSUB(CERT_SetOfSignedCrlTemplate)

SEC_ASN1_CHOOSER_DECLARE(CERT_IssuerAndSNTemplate)
SEC_ASN1_CHOOSER_DECLARE(SECOID_AlgorithmIDTemplate)
/* SEC_ASN1_CHOOSER_DECLARE(SEC_SetOfAnyTemplate)
SEC_ASN1_CHOOSER_DECLARE(CERT_SetOfSignedCrlTemplate)

const SEC_ASN1Template CERT_IssuerAndSNTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(CERTIssuerAndSN) },
    { SEC_ASN1_SAVE,
	  offsetof(CERTIssuerAndSN,derIssuer) },
    { SEC_ASN1_INLINE,
	  offsetof(CERTIssuerAndSN,issuer),
	  CERT_NameTemplate },
    { SEC_ASN1_INTEGER,
	  offsetof(CERTIssuerAndSN,serialNumber) },
    { 0 }
};

const SEC_ASN1Template SECOID_AlgorithmIDTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(SECAlgorithmID) },
    { SEC_ASN1_OBJECT_ID,
	  offsetof(SECAlgorithmID,algorithm), },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_ANY,
	  offsetof(SECAlgorithmID,parameters), },
    { 0, }
}; */

/* --Private-HttpCertStoreContext-Object Functions----------------------- */

/*
 * FUNCTION: pkix_pl_HttpCertStoreContext_Destroy
 * (see comments for PKIX_PL_DestructorCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_HttpCertStoreContext_Destroy(
        PKIX_PL_Object *object,
        void *plContext)
{
        const SEC_HttpClientFcnV1 *hcv1 = NULL;
        PKIX_PL_HttpCertStoreContext *context = NULL;

        PKIX_ENTER
                (HTTPCERTSTORECONTEXT, "pkix_pl_HttpCertStoreContext_Destroy");
        PKIX_NULLCHECK_ONE(object);

        PKIX_CHECK(pkix_CheckType
                    (object, PKIX_HTTPCERTSTORECONTEXT_TYPE, plContext),
                    PKIX_OBJECTNOTANHTTPCERTSTORECONTEXT);

        context = (PKIX_PL_HttpCertStoreContext *)object;

        hcv1 = (const SEC_HttpClientFcnV1 *)(context->client);

        if (context->requestSession != NULL) {
                PKIX_PL_NSSCALL(HTTPCERTSTORECONTEXT, hcv1->freeFcn,
                        (context->requestSession));
                context->requestSession = NULL;
        }

        if (context->serverSession != NULL) {
                PKIX_PL_NSSCALL(HTTPCERTSTORECONTEXT, hcv1->freeSessionFcn,
                        (context->serverSession));
                context->serverSession = NULL;
        }

        if (context->path != NULL) {
                PKIX_PL_NSSCALL
                        (HTTPCERTSTORECONTEXT, PORT_Free, (context->path));
                context->path = NULL;
        }

cleanup:

        PKIX_RETURN(HTTPCERTSTORECONTEXT);
}

/*
 * FUNCTION: pkix_pl_HttpCertStoreContext_RegisterSelf
 *
 * DESCRIPTION:
 *  Registers PKIX_PL_HTTPCERTSTORECONTEXT_TYPE and its related
 *  functions with systemClasses[]
 *
 * THREAD SAFETY:
 *  Not Thread Safe - for performance and complexity reasons
 *
 *  Since this function is only called by PKIX_PL_Initialize, which should
 *  only be called once, it is acceptable that this function is not
 *  thread-safe.
 */
PKIX_Error *
pkix_pl_HttpCertStoreContext_RegisterSelf(void *plContext)
{
        extern pkix_ClassTable_Entry systemClasses[PKIX_NUMTYPES];
        pkix_ClassTable_Entry entry;

        PKIX_ENTER
                (HTTPCERTSTORECONTEXT,
                "pkix_pl_HttpCertStoreContext_RegisterSelf");

        entry.description = "HttpCertStoreContext";
        entry.destructor = pkix_pl_HttpCertStoreContext_Destroy;
        entry.equalsFunction = NULL;
        entry.hashcodeFunction = NULL;
        entry.toStringFunction = NULL;
        entry.comparator = NULL;
        entry.duplicateFunction = NULL;

        systemClasses[PKIX_HTTPCERTSTORECONTEXT_TYPE] = entry;

        PKIX_RETURN(HTTPCERTSTORECONTEXT);
}

/* --Private-Http-CertStore-Database-Functions----------------------- */
extern void SEC_ASN1DecoderSetNotifyProc(
        SEC_ASN1DecoderContext *dcx,
        SEC_ASN1NotifyProc sec_pkcs7_decoder_notify,
        void *p7dcx);


typedef struct callbackContextStruct  {
        PKIX_List *pkixCertList;
        void *plContext;
} callbackContext;

/*
 * FUNCTION: certCallback
 * DESCRIPTION:
 *
 *  This function processes the null-terminated array of SECItems produced by
 *  extracting the contents of a signedData message received in response to an
 *  HTTP cert query. Its address is supplied as a callback function to
 *  CERT_DecodeCertPackage; it is not expected to be called directly.
 *
 *  Note that it does not conform to the libpkix API standard of returning
 *  a PKIX_Error*. It returns a SECStatus.
 *
 * PARAMETERS:
 *  "arg"
 *      The address of the callbackContext provided as a void* argument to
 *      CERT_DecodeCertPackage. Must be non-NULL.
 *  "secitemCerts"
 *      The address of the null-terminated array of SECItems. Must be non-NULL.
 *  "numcerts"
 *      The number of SECItems found in the signedData. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns SECSuccess if the function succeeds.
 *  Returns SECFailure if the function fails.
 */
static SECStatus
certCallback(void *arg, SECItem **secitemCerts, int numcerts)
{
        callbackContext *cbContext;
        SECItem **certs = NULL;
        SECItem *cert = NULL;
        PKIX_List *pkixCertList = NULL;
        PKIX_Error *error = NULL;
        void *plContext = NULL;
        int certsFound = 0;

        if ((arg == NULL) || (secitemCerts == NULL)) {
                return (SECFailure);
        }

        cbContext = (callbackContext *)arg;
        plContext = cbContext->plContext;
        pkixCertList = cbContext->pkixCertList;
        certs = secitemCerts;

        while ( *certs ) {
                cert = *certs;
                error = pkix_pl_Cert_CreateToList
                        (cert, pkixCertList, plContext);
                certsFound++;
                certs++;
                if (error != NULL) {
                        PKIX_PL_Object_DecRef
                                ((PKIX_PL_Object *)error, plContext);
                }
        }

        return ((certsFound == numcerts)?SECSuccess:SECFailure);
}

static SECOidTag
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

static void
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

/* XXX unused? */
static SECStatus
sec_pkcs7_decoder_start_digests (SEC_PKCS7DecoderContext *p7dcx, int depth,
				 SECAlgorithmID **digestalgs)
{
	return (SECFailure);
}

/* XXX unused? */
static SECStatus
sec_pkcs7_decoder_finish_digests (SEC_PKCS7DecoderContext *p7dcx,
				  PRArenaPool *poolp,
				  SECItem ***digestsp)
{
	return (SECFailure);
}

static void
sec_pkcs7_decoder_notify (void *arg, PRBool before, void *dest, int depth)
{
    SEC_PKCS7DecoderContext *p7dcx;
    SEC_PKCS7ContentInfo *cinfo;
    SEC_PKCS7SignedData *sigd;
    PRBool after;
    SECStatus rv;

    /*
     * Just to make the code easier to read, create an "after" variable
     * that is equivalent to "not before".
     * (This used to be just the statement "after = !before", but that
     * causes a warning on the mac; to avoid that, we do it the long way.)
     */
    if (before)
	after = PR_FALSE;
    else
	after = PR_TRUE;

    p7dcx = (SEC_PKCS7DecoderContext*)arg;
    cinfo = p7dcx->cinfo;

    if (cinfo->contentTypeTag == NULL) {
	if (after && dest == &(cinfo->contentType))
	    cinfo->contentTypeTag = SECOID_FindOID(&(cinfo->contentType));
	return;
    }

    switch (cinfo->contentTypeTag->offset) {
      case SEC_OID_PKCS7_SIGNED_DATA:
	sigd = cinfo->content.signedData;
	if (sigd == NULL)
	    break;

	if (sigd->contentInfo.contentTypeTag == NULL) {
	    if (after && dest == &(sigd->contentInfo.contentType))
		sigd->contentInfo.contentTypeTag =
			SECOID_FindOID(&(sigd->contentInfo.contentType));
	    break;
	}

	/*
	 * We only set up a filtering digest if the content is
	 * plain DATA; anything else needs more work because a
	 * second pass is required to produce a DER encoding from
	 * an input that can be BER encoded.  (This is a requirement
	 * of PKCS7 that is unfortunate, but there you have it.)
	 *
	 * XXX Also, since we stop here if this is not DATA, the
	 * inner content is not getting processed at all.  Someday
	 * we may want to fix that.
	 */
	if (sigd->contentInfo.contentTypeTag->offset != SEC_OID_PKCS7_DATA) {
	    /* XXX Set an error in p7dcx->error */
	    SEC_ASN1DecoderClearNotifyProc (p7dcx->dcx);
	    break;
	}

	/*
	 * Just before the content, we want to set up a digest context
	 * for each digest algorithm listed, and start a filter which
	 * will run all of the contents bytes through that digest.
	 */
	if (before && dest == &(sigd->contentInfo.content)) {
	    rv = sec_pkcs7_decoder_start_digests (p7dcx, depth,
						  sigd->digestAlgorithms);
	    if (rv != SECSuccess)
		SEC_ASN1DecoderClearNotifyProc (p7dcx->dcx);

	    break;
	}

	/*
	 * XXX To handle nested types, here is where we would want
	 * to check for inner boundaries that need handling.
	 */

	/*
	 * Are we done?
	 */
	if (after && dest == &(sigd->contentInfo.content)) {
	    /*
	     * Close out the digest contexts.  We ignore any error
	     * because we are stopping anyway; the error status left
	     * behind in p7dcx will be seen by outer functions.
	     */
	    (void) sec_pkcs7_decoder_finish_digests (p7dcx, cinfo->poolp,
						     &(sigd->digests));

	    /*
	     * XXX To handle nested contents, we would need to remove
	     * the worker from the chain (and free it).
	     */

	    /*
	     * Stop notify.
	     */
	    SEC_ASN1DecoderClearNotifyProc (p7dcx->dcx);
	}
	break;

      default:
	SEC_ASN1DecoderClearNotifyProc (p7dcx->dcx);
	break;
    }
}


/* XXX Only called with all args NULL */

static SEC_PKCS7DecoderContext *
SEC_PKCS7DecoderStart(SEC_PKCS7DecoderContentCallback cb, void *cb_arg,
		      SECKEYGetPasswordKey pwfn, void *pwfn_arg,
		      SEC_PKCS7GetDecryptKeyCallback decrypt_key_cb, 
		      void *decrypt_key_cb_arg,
		      SEC_PKCS7DecryptionAllowedCallback decrypt_allowed_cb)
{
    SEC_PKCS7DecoderContext *p7dcx;
    SEC_ASN1DecoderContext *dcx;
    SEC_PKCS7ContentInfo *cinfo;
    PRArenaPool *poolp;

    poolp = PORT_NewArena (1024);		/* XXX what is right value? */
    if (poolp == NULL)
	return NULL;

    cinfo = (SEC_PKCS7ContentInfo*)PORT_ArenaZAlloc (poolp, sizeof(*cinfo));
    if (cinfo == NULL) {
	PORT_FreeArena (poolp, PR_FALSE);
	return NULL;
    }

    cinfo->poolp = poolp;
    cinfo->pwfn = pwfn;
    cinfo->pwfn_arg = pwfn_arg;
    cinfo->created = PR_FALSE;
    cinfo->refCount = 1;

    p7dcx = 
      (SEC_PKCS7DecoderContext*)PORT_ZAlloc (sizeof(SEC_PKCS7DecoderContext));
    if (p7dcx == NULL) {
	PORT_FreeArena (poolp, PR_FALSE);
	return NULL;
    }

    p7dcx->tmp_poolp = PORT_NewArena (1024);	/* XXX what is right value? */
    if (p7dcx->tmp_poolp == NULL) {
	PORT_Free (p7dcx);
	PORT_FreeArena (poolp, PR_FALSE);
	return NULL;
    }

    dcx = SEC_ASN1DecoderStart (poolp, cinfo, sec_PKCS7ContentInfoTemplate);
    if (dcx == NULL) {
	PORT_FreeArena (p7dcx->tmp_poolp, PR_FALSE);
	PORT_Free (p7dcx);
	PORT_FreeArena (poolp, PR_FALSE);
	return NULL;
    }

    SEC_ASN1DecoderSetNotifyProc (dcx, sec_pkcs7_decoder_notify, p7dcx);

    p7dcx->dcx = dcx;
    p7dcx->cinfo = cinfo;
    p7dcx->cb = cb;
    p7dcx->cb_arg = cb_arg;
    p7dcx->pwfn = pwfn;
    p7dcx->pwfn_arg = pwfn_arg;
    p7dcx->dkcb = decrypt_key_cb;
    p7dcx->dkcb_arg = decrypt_key_cb_arg;
    p7dcx->decrypt_allowed_cb = decrypt_allowed_cb;

    return p7dcx;
}


/*
 * Do the next chunk of PKCS7 decoding.  If there is a problem, set
 * an error and return a failure status.  Note that in the case of
 * an error, this routine is still prepared to be called again and
 * again in case that is the easiest route for our caller to take.
 * We simply detect it and do not do anything except keep setting
 * that error in case our caller has not noticed it yet...
 */
static SECStatus
SEC_PKCS7DecoderUpdate(SEC_PKCS7DecoderContext *p7dcx,
		       const char *buf, unsigned long len)
{
    if (p7dcx->cinfo != NULL && p7dcx->dcx != NULL) { 
	PORT_Assert (p7dcx->error == 0);
	if (p7dcx->error == 0) {
	    if (SEC_ASN1DecoderUpdate (p7dcx->dcx, buf, len) != SECSuccess) {
		p7dcx->error = PORT_GetError();
		PORT_Assert (p7dcx->error);
		if (p7dcx->error == 0)
		    p7dcx->error = -1;
	    }
	}
    }

    if (p7dcx->error) {
	if (p7dcx->dcx != NULL) {
	    (void) SEC_ASN1DecoderFinish (p7dcx->dcx);
	    p7dcx->dcx = NULL;
	}
	if (p7dcx->cinfo != NULL) {
	    SEC_PKCS7DestroyContentInfo (p7dcx->cinfo);
	    p7dcx->cinfo = NULL;
	}
	PORT_SetError (p7dcx->error);
	return SECFailure;
    }

    return SECSuccess;
}

static void
sec_pkcs7_destroy_cipher (sec_PKCS7CipherObject *obj)
{
    (* obj->destroy) (obj->cx, PR_TRUE);
    PORT_Free (obj);
}

static void
sec_PKCS7DestroyDecryptObject (sec_PKCS7CipherObject *obj)
{
    PORT_Assert (obj != NULL);
    if (obj == NULL)
	return;
    PORT_Assert (! obj->encrypt);
    sec_pkcs7_destroy_cipher (obj);
}

static SEC_PKCS7ContentInfo *
SEC_PKCS7DecoderFinish(SEC_PKCS7DecoderContext *p7dcx)
{
    SEC_PKCS7ContentInfo *cinfo;

    cinfo = p7dcx->cinfo;
    if (p7dcx->dcx != NULL) {
	if (SEC_ASN1DecoderFinish (p7dcx->dcx) != SECSuccess) {
	    SEC_PKCS7DestroyContentInfo (cinfo);
	    cinfo = NULL;
	}
    }
    /* free any NSS data structures */
    if (p7dcx->worker.decryptobj) {
        sec_PKCS7DestroyDecryptObject (p7dcx->worker.decryptobj);
    }
    PORT_FreeArena (p7dcx->tmp_poolp, PR_FALSE);
    PORT_Free (p7dcx);
    return cinfo;
}

static SECStatus
SEC_ReadPKCS7Certs(SECItem *pkcs7Item, CERTImportCertificateFunc f, void *arg)
{
    SEC_PKCS7ContentInfo *contentInfo = NULL;
    SECStatus rv;
    SECItem **certs;
    int count;

    SEC_PKCS7DecoderContext *p7dcx;

    p7dcx = SEC_PKCS7DecoderStart(NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    (void) SEC_PKCS7DecoderUpdate
	    (p7dcx, (char *) pkcs7Item->data, pkcs7Item->len);
    contentInfo = SEC_PKCS7DecoderFinish(p7dcx);

    if ( contentInfo == NULL ) {
	goto loser;
    }

    if ( SEC_PKCS7ContentType (contentInfo) != SEC_OID_PKCS7_SIGNED_DATA ) {
	goto loser;
    }

    certs = contentInfo->content.signedData->rawCerts;
    if ( certs ) {
	count = 0;
	
	while ( *certs ) {
	    count++;
	    certs++;
	}
	rv = (* f)(arg, contentInfo->content.signedData->rawCerts, count);
    }
    
    rv = SECSuccess;
    
    goto done;
loser:
    rv = SECFailure;
    
done:
    if ( contentInfo ) {
	SEC_PKCS7DestroyContentInfo(contentInfo);
    }

    return(rv);
}

/*
 * This function is based on CERT_DecodeCertPackage from lib/pkcs7/certread.c
 * read an old style ascii or binary certificate chain
 */
PKIX_Error *
pkix_pl_HttpCertStore_DecodeCertPackage
        (char *certbuf,
        int certlen,
        CERTImportCertificateFunc f,
        void *arg,
        void *plContext)
{
        unsigned char *cp;
        unsigned char *bincert = NULL;
        char *ascCert = NULL;
        SECItem certitem;
        SECItem *pcertitem = &certitem;
        int seqLen, seqLenLen;
        SECStatus rv = SECFailure;
    
        PKIX_ENTER
                (HTTPCERTSTORECONTEXT,
                "pkix_pl_HttpCertStore_DecodeCertPackage");
        PKIX_NULLCHECK_TWO(certbuf, f);
    
        cp = (unsigned char *)certbuf;

        /* is this a DER encoded certificate of some type? */
        if ( ( *cp  & 0x1f ) == SEC_ASN1_SEQUENCE ) {

                cp++;
        
                if (*cp & 0x80) {
                        /* Multibyte length */
                        seqLenLen = cp[0] & 0x7f;
            
                        switch (seqLenLen) {
                          case 4:
                            seqLen = ((unsigned long)cp[1] << 24) |
                                    ((unsigned long)cp[2] << 16) |
                                    (cp[3]<<8) | cp[4];
                            break;
                          case 3:
                            seqLen = ((unsigned long)cp[1]<<16) |
                                    (cp[2]<<8) | cp[3];
                            break;
                          case 2:
                            seqLen = (cp[1]<<8) | cp[2];
                            break;
                          case 1:
                            seqLen = cp[1];
                            break;
                          default:
                            /* indefinite length */
                            seqLen = 0;
                        }
                        cp += ( seqLenLen + 1 );

                } else {
                        seqLenLen = 0;
                        seqLen = *cp;
                        cp++;
                }

                /* check entire length if definite length */
                if ( seqLen || seqLenLen ) {
                        if ( certlen != ( seqLen + seqLenLen + 2 ) ) {
                            if (certlen > ( seqLen + seqLenLen + 2 )) {
                                PKIX_ERROR(PKIX_TOOMUCHDATAINDERSEQUENCE);
                            } else {
                                PKIX_ERROR(PKIX_TOOLITTLEDATAINDERSEQUENCE);
                            }
                        }
                } else {
                        PKIX_ERROR(PKIX_NOTDERPACKAGE);
		}

                if ( cp[0] == SEC_ASN1_OBJECT_ID ) {
                        SECOidData *oiddata;
            		SECItem oiditem;
            		/* XXX - assume DER encoding of OID len!! */
            		oiditem.len = cp[1];
            		oiditem.data = (unsigned char *)&cp[2];
			PKIX_PL_NSSCALLRV
				(HTTPCERTSTORECONTEXT, oiddata, SECOID_FindOID,
				(&oiditem));
            		if ( oiddata == NULL ) {
				PKIX_ERROR(PKIX_UNKNOWNOBJECTOID);
            		}

            		certitem.data = (unsigned char*)certbuf;
            		certitem.len = certlen;
            
            		switch ( oiddata->offset ) {
              			case SEC_OID_PKCS7_SIGNED_DATA:
					PKIX_PL_NSSCALLRV
						(HTTPCERTSTORECONTEXT,
						rv,
                				SEC_ReadPKCS7Certs,
						(&certitem, f, arg));
		                        if (rv != SECSuccess) {
                	                    PKIX_ERROR
						(PKIX_SECREADPKCS7CERTSFAILED);
					}
                		break;
              		default:
				PKIX_ERROR(PKIX_UNKNOWNOBJECTOID);
                      		break;
            		}
            
        	} else {
            		/* it had better be a certificate by now!! */
            		certitem.data = (unsigned char*)certbuf;
            		certitem.len = certlen;
            
	        	rv = (* f)(arg, &pcertitem, 1);
                        if (rv != SECSuccess) {
                                PKIX_ERROR
				    (PKIX_CERTIMPORTCERTIFICATEFUNCTIONFAILED);
			}
                }
        }

cleanup:

        if ( bincert ) {
                PORT_Free(bincert);
        }

        if ( ascCert ) {
                PORT_Free(ascCert);
        }

        PKIX_RETURN(HTTPCERTSTORECONTEXT);
}


/*
 * FUNCTION: pkix_pl_HttpCertStore_ProcessCertResponse
 * DESCRIPTION:
 *
 *  This function verifies that the response code pointed to by "responseCode"
 *  and the content type pointed to by "responseContentType" are as expected,
 *  and then decodes the data pointed to by "responseData", of length
 *  "responseDataLen", into a List of Certs, possibly empty, which is returned
 *  at "pCertList".
 *
 * PARAMETERS:
 *  "responseCode"
 *      The value of the HTTP response code.
 *  "responseContentType"
 *      The address of the Content-type string. Must be non-NULL.
 *  "responseData"
 *      The address of the message data. Must be non-NULL.
 *  "responseDataLen"
 *      The length of the message data.
 *  "pCertList"
 *      The address of the List that is created. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a HttpCertStore Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_HttpCertStore_ProcessCertResponse(
        PRUint16 responseCode,
        const char *responseContentType,
        const char *responseData,
        PRUint32 responseDataLen,
        PKIX_List **pCertList,
        void *plContext)
{
        PRArenaPool *arena = NULL;
        char *encodedResponse = NULL;
        PRIntn compareVal = 0;
        PKIX_List *certs = NULL;
        callbackContext cbContext;

        PKIX_ENTER
                (HTTPCERTSTORECONTEXT,
                "pkix_pl_HttpCertStore_ProcessCertResponse");
        PKIX_NULLCHECK_ONE(pCertList);

        if (responseCode != 200) {
                PKIX_ERROR(PKIX_BADHTTPRESPONSE);
        }

        /* check that response type is application/pkcs7-mime */
        if (responseContentType == NULL) {
                PKIX_ERROR(PKIX_NOCONTENTTYPEINHTTPRESPONSE);
        }

        PKIX_PL_NSSCALLRV(HTTPCERTSTORECONTEXT, compareVal, PORT_Strcasecmp,
                (responseContentType, "application/pkcs7-mime"));

        if (compareVal != 0) {
                PKIX_ERROR(PKIX_CONTENTTYPENOTPKCS7MIME);
        }

        PKIX_PL_NSSCALLRV(HTTPCERTSTORECONTEXT, arena, PORT_NewArena,
                (DER_DEFAULT_CHUNKSIZE));

        if (arena == NULL) {
                PKIX_ERROR(PKIX_OUTOFMEMORY);
        }

        if (responseData == NULL) {
                PKIX_ERROR(PKIX_NORESPONSEDATAINHTTPRESPONSE);
        }

        PKIX_CHECK(PKIX_List_Create(&certs, plContext),
                PKIX_LISTCREATEFAILED);

        cbContext.pkixCertList = certs;
        cbContext.plContext = plContext;
        encodedResponse = (char *)responseData;

	PKIX_CHECK(pkix_pl_HttpCertStore_DecodeCertPackage
		(encodedResponse,
                responseDataLen,
                certCallback,
                &cbContext,
                plContext),
		PKIX_HTTPCERTSTOREDECODECERTPACKAGEFAILED);

        *pCertList = cbContext.pkixCertList;

cleanup:
        if (PKIX_ERROR_RECEIVED) {
                PKIX_DECREF(cbContext.pkixCertList);
        }

        if (arena != NULL) {
                PKIX_PL_NSSCALL(CERTSTORE, PORT_FreeArena, (arena, PR_FALSE));
        }

        PKIX_RETURN(HTTPCERTSTORECONTEXT);
}

/*
 * FUNCTION: pkix_pl_HttpCertStore_ProcessCrlResponse
 * DESCRIPTION:
 *
 *  This function verifies that the response code pointed to by "responseCode"
 *  and the content type pointed to by "responseContentType" are as expected,
 *  and then decodes the data pointed to by "responseData", of length
 *  "responseDataLen", into a List of Crls, possibly empty, which is returned
 *  at "pCrlList".
 *
 * PARAMETERS:
 *  "responseCode"
 *      The value of the HTTP response code.
 *  "responseContentType"
 *      The address of the Content-type string. Must be non-NULL.
 *  "responseData"
 *      The address of the message data. Must be non-NULL.
 *  "responseDataLen"
 *      The length of the message data.
 *  "pCrlList"
 *      The address of the List that is created. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a HttpCertStore Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_HttpCertStore_ProcessCrlResponse(
        PRUint16 responseCode,
        const char *responseContentType,
        const char *responseData,
        PRUint32 responseDataLen,
        PKIX_List **pCrlList,
        void *plContext)
{
        PRArenaPool *arena = NULL;
        SECItem *encodedResponse = NULL;
        PRInt16 compareVal = 0;
        PKIX_List *crls = NULL;

        PKIX_ENTER
                (HTTPCERTSTORECONTEXT,
                "pkix_pl_HttpCertStore_ProcessCrlResponse");
        PKIX_NULLCHECK_ONE(pCrlList);

        if (responseCode != 200) {
                PKIX_ERROR(PKIX_BADHTTPRESPONSE);
        }

        /* check that response type is application/pkix-crl */
        if (responseContentType == NULL) {
                PKIX_ERROR(PKIX_NOCONTENTTYPEINHTTPRESPONSE);
        }

        PKIX_PL_NSSCALLRV
                (HTTPCERTSTORECONTEXT, compareVal, PORT_Strcasecmp,
                (responseContentType, "application/pkix-crl"));

        if (compareVal != 0) {
                PKIX_ERROR(PKIX_CONTENTTYPENOTPKIXCRL);
        }

        /* Make a SECItem of the response data */
        PKIX_PL_NSSCALLRV(HTTPCERTSTORECONTEXT, arena, PORT_NewArena,
                (DER_DEFAULT_CHUNKSIZE));

        if (arena == NULL) {
                PKIX_ERROR(PKIX_OUTOFMEMORY);
        }

        if (responseData == NULL) {
                PKIX_ERROR(PKIX_NORESPONSEDATAINHTTPRESPONSE);
        }

        PKIX_PL_NSSCALLRV
                (HTTPCERTSTORECONTEXT, encodedResponse, SECITEM_AllocItem,
                (arena, NULL, responseDataLen));

        if (encodedResponse == NULL) {
                PKIX_ERROR(PKIX_OUTOFMEMORY);
        }

        PKIX_PL_NSSCALL(HTTPCERTSTORECONTEXT, PORT_Memcpy,
                (encodedResponse->data, responseData, responseDataLen));

        PKIX_CHECK(PKIX_List_Create(&crls, plContext),
                PKIX_LISTCREATEFAILED);

        PKIX_CHECK(pkix_pl_CRL_CreateToList
                (encodedResponse, crls, plContext),
                PKIX_CRLCREATETOLISTFAILED);

        *pCrlList = crls;

cleanup:
        if (PKIX_ERROR_RECEIVED) {
                PKIX_DECREF(crls);
        }

        if (arena != NULL) {
                PKIX_PL_NSSCALL(CERTSTORE, PORT_FreeArena, (arena, PR_FALSE));
        }


        PKIX_RETURN(HTTPCERTSTORECONTEXT);
}

/*
 * FUNCTION: pkix_pl_HttpCertStore_CreateRequestSession
 * DESCRIPTION:
 *
 *  This function takes elements from the HttpCertStoreContext pointed to by
 *  "context" (path, client, and serverSession) and creates a RequestSession.
 *  See the HTTPClient API described in ocspt.h for further details.
 *
 * PARAMETERS:
 *  "context"
 *      The address of the HttpCertStoreContext. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a HttpCertStore Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_HttpCertStore_CreateRequestSession(
        PKIX_PL_HttpCertStoreContext *context,
        void *plContext)
{
        const SEC_HttpClientFcnV1 *hcv1 = NULL;
        SECStatus rv = SECFailure;
        char *pathString = NULL;

        PKIX_ENTER
                (HTTPCERTSTORECONTEXT,
                "pkix_pl_HttpCertStore_CreateRequestSession");
        PKIX_NULLCHECK_TWO(context, context->serverSession);

        pathString = PR_smprintf("%s", context->path);

        if (context->client->version == 1) {
                hcv1 = &(context->client->fcnTable.ftable1);

                if (context->requestSession != NULL) {
                        PKIX_PL_NSSCALL(HTTPCERTSTORECONTEXT, hcv1->freeFcn,
                                (context->requestSession));
                        context->requestSession = 0;
                }

                PKIX_PL_NSSCALLRV
                        (HTTPCERTSTORECONTEXT, rv, hcv1->createFcn,
                        (context->serverSession,
                        "http",
                        pathString,
                        "GET",
                        PR_TicksPerSecond() * 60,
                        &(context->requestSession)));

                if (rv != SECSuccess) {
                        if (pathString != NULL) {
                                PORT_Free(pathString);
                        }
                        PKIX_ERROR(PKIX_HTTPSERVERERROR);
                }
        } else {
                PKIX_ERROR(PKIX_UNSUPPORTEDVERSIONOFHTTPCLIENT);
        }

cleanup:

        PKIX_RETURN(HTTPCERTSTORECONTEXT);

}

/*
 * FUNCTION: pkix_pl_HttpCertStore_GetCert
 *  (see description of PKIX_CertStore_CertCallback in pkix_certstore.h)
 */
PKIX_Error *
pkix_pl_HttpCertStore_GetCert(
        PKIX_CertStore *store,
        PKIX_CertSelector *selector,
        void **pNBIOContext,
        PKIX_List **pCertList,
        void *plContext)
{
        const SEC_HttpClientFcnV1 *hcv1 = NULL;
        PKIX_PL_HttpCertStoreContext *context = NULL;
        void *nbioContext = NULL;
        SECStatus rv = SECFailure;
        PRUint16 responseCode = 0;
        const char *responseContentType = NULL;
        const char *responseData = NULL;
        PRUint32 responseDataLen = 0;
        PKIX_List *certList = NULL;

        PKIX_ENTER(HTTPCERTSTORECONTEXT, "pkix_pl_HttpCertStore_GetCert");
        PKIX_NULLCHECK_THREE(store, selector, pCertList);

        nbioContext = *pNBIOContext;
        *pNBIOContext = NULL;

        PKIX_CHECK(PKIX_CertStore_GetCertStoreContext
                (store, (PKIX_PL_Object **)&context, plContext),
                PKIX_CERTSTOREGETCERTSTORECONTEXTFAILED);

        if (context->client->version == 1) {
                hcv1 = &(context->client->fcnTable.ftable1);

                PKIX_CHECK(pkix_pl_HttpCertStore_CreateRequestSession
                        (context, plContext),
                        PKIX_HTTPCERTSTORECREATEREQUESTSESSIONFAILED);

                PKIX_PL_NSSCALLRV
                        (HTTPCERTSTORECONTEXT, rv, hcv1->trySendAndReceiveFcn,
                        (context->requestSession,
                        (PRPollDesc **)&nbioContext,
                        &responseCode,
                        (const char **)&responseContentType,
                        NULL, /* &responseHeaders */
                        (const char **)&responseData,
                        &responseDataLen));

                if (rv != SECSuccess) {
                        PKIX_ERROR(PKIX_HTTPSERVERERROR);
                }

                if (nbioContext != 0) {
                        *pNBIOContext = nbioContext;
                        goto cleanup;
                }

        } else {
                PKIX_ERROR(PKIX_UNSUPPORTEDVERSIONOFHTTPCLIENT);
        }

        PKIX_CHECK(pkix_pl_HttpCertStore_ProcessCertResponse
                (responseCode,
                responseContentType,
                responseData,
                responseDataLen,
                &certList,
                plContext),
                PKIX_HTTPCERTSTOREPROCESSCERTRESPONSEFAILED);

        *pCertList = certList;

cleanup:

        PKIX_RETURN(CERTSTORE);
}

/*
 * FUNCTION: pkix_pl_HttpCertStore_GetCertContinue
 *  (see description of PKIX_CertStore_CertCallback in pkix_certstore.h)
 */
PKIX_Error *
pkix_pl_HttpCertStore_GetCertContinue(
        PKIX_CertStore *store,
        PKIX_CertSelector *selector,
        void **pNBIOContext,
        PKIX_List **pCertList,
        void *plContext)
{
        const SEC_HttpClientFcnV1 *hcv1 = NULL;
        PKIX_PL_HttpCertStoreContext *context = NULL;
        void *nbioContext = NULL;
        SECStatus rv = SECFailure;
        PRUint16 responseCode = 0;
        const char *responseContentType = NULL;
        const char *responseData = NULL;
        PRUint32 responseDataLen = 0;
        PKIX_List *certList = NULL;

        PKIX_ENTER(CERTSTORE, "pkix_pl_HttpCertStore_GetCertContinue");
        PKIX_NULLCHECK_THREE(store, selector, pCertList);

        nbioContext = *pNBIOContext;
        *pNBIOContext = NULL;

        PKIX_CHECK(PKIX_CertStore_GetCertStoreContext
                (store, (PKIX_PL_Object **)&context, plContext),
                PKIX_CERTSTOREGETCERTSTORECONTEXTFAILED);

        if (context->client->version == 1) {
                hcv1 = &(context->client->fcnTable.ftable1);

                PKIX_NULLCHECK_ONE(context->requestSession);

                PKIX_PL_NSSCALLRV
                        (HTTPCERTSTORECONTEXT, rv, hcv1->trySendAndReceiveFcn,
                        (context->requestSession,
                        (PRPollDesc **)&nbioContext,
                        &responseCode,
                        (const char **)&responseContentType,
                        NULL, /* &responseHeaders */
                        (const char **)&responseData,
                        &responseDataLen));

                if (rv != SECSuccess) {
                        PKIX_ERROR(PKIX_HTTPSERVERERROR);
                }

                if (nbioContext != 0) {
                        *pNBIOContext = nbioContext;
                        goto cleanup;
                }

        } else {
                PKIX_ERROR(PKIX_UNSUPPORTEDVERSIONOFHTTPCLIENT);
        }

        PKIX_CHECK(pkix_pl_HttpCertStore_ProcessCertResponse
                (responseCode,
                responseContentType,
                responseData,
                responseDataLen,
                &certList,
                plContext),
                PKIX_HTTPCERTSTOREPROCESSCERTRESPONSEFAILED);

        *pCertList = certList;

cleanup:

        PKIX_RETURN(CERTSTORE);
}

/*
 * FUNCTION: pkix_pl_HttpCertStore_GetCRL
 *  (see description of PKIX_CertStore_CRLCallback in pkix_certstore.h)
 */
PKIX_Error *
pkix_pl_HttpCertStore_GetCRL(
        PKIX_CertStore *store,
        PKIX_CRLSelector *selector,
        void **pNBIOContext,
        PKIX_List **pCrlList,
        void *plContext)
{

        const SEC_HttpClientFcnV1 *hcv1 = NULL;
        PKIX_PL_HttpCertStoreContext *context = NULL;
        void *nbioContext = NULL;
        SECStatus rv = SECFailure;
        PRUint16 responseCode = 0;
        const char *responseContentType = NULL;
        const char *responseData = NULL;
        PRUint32 responseDataLen = 0;
        PKIX_List *crlList = NULL;

        PKIX_ENTER(CERTSTORE, "pkix_pl_HttpCertStore_GetCRL");
        PKIX_NULLCHECK_THREE(store, selector, pCrlList);

        nbioContext = *pNBIOContext;
        *pNBIOContext = NULL;

        PKIX_CHECK(PKIX_CertStore_GetCertStoreContext
                (store, (PKIX_PL_Object **)&context, plContext),
                PKIX_CERTSTOREGETCERTSTORECONTEXTFAILED);

        if (context->client->version == 1) {
                hcv1 = &(context->client->fcnTable.ftable1);

                PKIX_CHECK(pkix_pl_HttpCertStore_CreateRequestSession
                        (context, plContext),
                        PKIX_HTTPCERTSTORECREATEREQUESTSESSIONFAILED);

                PKIX_PL_NSSCALLRV
                        (HTTPCERTSTORECONTEXT, rv, hcv1->trySendAndReceiveFcn,
                        (context->requestSession,
                        (PRPollDesc **)&nbioContext,
                        &responseCode,
                        (const char **)&responseContentType,
                        NULL, /* &responseHeaders */
                        (const char **)&responseData,
                        &responseDataLen));

                if (rv != SECSuccess) {
                        PKIX_ERROR(PKIX_HTTPSERVERERROR);
                }

                if (nbioContext != 0) {
                        *pNBIOContext = nbioContext;
                        goto cleanup;
                }

        } else {
                PKIX_ERROR(PKIX_UNSUPPORTEDVERSIONOFHTTPCLIENT);
        }

        PKIX_CHECK(pkix_pl_HttpCertStore_ProcessCrlResponse
                (responseCode,
                responseContentType,
                responseData,
                responseDataLen,
                &crlList,
                plContext),
                PKIX_HTTPCERTSTOREPROCESSCRLRESPONSEFAILED);

        *pCrlList = crlList;

cleanup:

        PKIX_RETURN(CERTSTORE);
}

/*
 * FUNCTION: pkix_pl_HttpCertStore_GetCRLContinue
 *  (see description of PKIX_CertStore_CRLCallback in pkix_certstore.h)
 */
PKIX_Error *
pkix_pl_HttpCertStore_GetCRLContinue(
        PKIX_CertStore *store,
        PKIX_CRLSelector *selector,
        void **pNBIOContext,
        PKIX_List **pCrlList,
        void *plContext)
{
        const SEC_HttpClientFcnV1 *hcv1 = NULL;
        PKIX_PL_HttpCertStoreContext *context = NULL;
        void *nbioContext = NULL;
        SECStatus rv = SECFailure;
        PRUint16 responseCode = 0;
        const char *responseContentType = NULL;
        const char *responseData = NULL;
        PRUint32 responseDataLen = 0;
        PKIX_List *crlList = NULL;

        PKIX_ENTER(CERTSTORE, "pkix_pl_HttpCertStore_GetCRLContinue");
        PKIX_NULLCHECK_FOUR(store, selector, pNBIOContext, pCrlList);

        nbioContext = *pNBIOContext;
        *pNBIOContext = NULL;

        PKIX_CHECK(PKIX_CertStore_GetCertStoreContext
                (store, (PKIX_PL_Object **)&context, plContext),
                PKIX_CERTSTOREGETCERTSTORECONTEXTFAILED);

        if (context->client->version == 1) {
                hcv1 = &(context->client->fcnTable.ftable1);

                PKIX_CHECK(pkix_pl_HttpCertStore_CreateRequestSession
                        (context, plContext),
                        PKIX_HTTPCERTSTORECREATEREQUESTSESSIONFAILED);

                PKIX_PL_NSSCALLRV
                        (HTTPCERTSTORECONTEXT, rv, hcv1->trySendAndReceiveFcn,
                        (context->requestSession,
                        (PRPollDesc **)&nbioContext,
                        &responseCode,
                        (const char **)&responseContentType,
                        NULL, /* &responseHeaders */
                        (const char **)&responseData,
                        &responseDataLen));

                if (rv != SECSuccess) {
                        PKIX_ERROR(PKIX_HTTPSERVERERROR);
                }

                if (nbioContext != 0) {
                        *pNBIOContext = nbioContext;
                        goto cleanup;
                }

        } else {
                PKIX_ERROR(PKIX_UNSUPPORTEDVERSIONOFHTTPCLIENT);
        }

        PKIX_CHECK(pkix_pl_HttpCertStore_ProcessCrlResponse
                (responseCode,
                responseContentType,
                responseData,
                responseDataLen,
                &crlList,
                plContext),
                PKIX_HTTPCERTSTOREPROCESSCRLRESPONSEFAILED);

        *pCrlList = crlList;

cleanup:

        PKIX_RETURN(CERTSTORE);
}

/* --Public-HttpCertStore-Functions----------------------------------- */

/*
 * FUNCTION: pkix_pl_HttpCertStore_CreateWithAsciiName
 * DESCRIPTION:
 *
 *  This function uses the HttpClient pointed to by "client" and the string
 *  (hostname:portnum/path, with portnum optional) pointed to by "locationAscii"
 *  to create an HttpCertStore connected to the desired location, storing the
 *  created CertStore at "pCertStore".
 *
 * PARAMETERS:
 *  "client"
 *      The address of the HttpClient. Must be non-NULL.
 *  "locationAscii"
 *      The address of the character string indicating the hostname, port, and
 *      path to be queried for Certs or Crls. Must be non-NULL.
 *  "pCertStore"
 *      The address in which the object is stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a HttpCertStore Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_HttpCertStore_CreateWithAsciiName(
        PKIX_PL_HttpClient *client,
        char *locationAscii,
        PKIX_CertStore **pCertStore,
        void *plContext)
{
        const SEC_HttpClientFcn *clientFcn = NULL;
        const SEC_HttpClientFcnV1 *hcv1 = NULL;
        PKIX_PL_HttpCertStoreContext *httpCertStore = NULL;
        PKIX_CertStore *certStore = NULL;
        char *hostname = NULL;
        char *path = NULL;
        PRUint16 port = 0;
        SECStatus rv = SECFailure;

        PKIX_ENTER(CERTSTORE, "pkix_pl_HttpCertStore_CreateWithAsciiName");
        PKIX_NULLCHECK_TWO(locationAscii, pCertStore);

        if (client == NULL) {
                clientFcn = GetRegisteredHttpClient();
                if (clientFcn == NULL) {
                        PKIX_ERROR(PKIX_NOREGISTEREDHTTPCLIENT);
                }
        } else {
                clientFcn = (const SEC_HttpClientFcn *)client;
        }

        /* create a PKIX_PL_HttpCertStore object */
        PKIX_CHECK(PKIX_PL_Object_Alloc
                  (PKIX_HTTPCERTSTORECONTEXT_TYPE,
                  sizeof (PKIX_PL_HttpCertStoreContext),
                  (PKIX_PL_Object **)&httpCertStore,
                  plContext),
                  PKIX_COULDNOTCREATEOBJECT);

        /* Initialize fields */
        httpCertStore->client = clientFcn; /* not a PKIX object! */

        /* parse location -> hostname, port, path */
        PKIX_PL_NSSCALLRV(CERTSTORE, rv, CERT_ParseURL,
                (locationAscii, &hostname, &port, &path));

        if ((hostname == NULL) || (path == NULL)) {
                PKIX_ERROR(PKIX_URLPARSINGFAILED);
        }

        httpCertStore->path = path;

        if (clientFcn->version == 1) {

                hcv1 = &(clientFcn->fcnTable.ftable1);

                PKIX_PL_NSSCALLRV
                        (HTTPCERTSTORECONTEXT,
                        rv,
                        hcv1->createSessionFcn,
                        (hostname, port, &(httpCertStore->serverSession)));

                if (rv != SECSuccess) {
                        PKIX_ERROR(PKIX_HTTPCLIENTCREATESESSIONFAILED);
                }
        } else {
                PKIX_ERROR(PKIX_UNSUPPORTEDVERSIONOFHTTPCLIENT);
        }

        httpCertStore->requestSession = NULL;

        PKIX_CHECK(PKIX_CertStore_Create
                (pkix_pl_HttpCertStore_GetCert,
                pkix_pl_HttpCertStore_GetCRL,
                pkix_pl_HttpCertStore_GetCertContinue,
                pkix_pl_HttpCertStore_GetCRLContinue,
                NULL,       /* don't support trust */
                (PKIX_PL_Object *)httpCertStore,
                PKIX_TRUE,  /* cache flag */
                PKIX_FALSE, /* not local */
                &certStore,
                plContext),
                PKIX_CERTSTORECREATEFAILED);

        *pCertStore = certStore;

cleanup:

        if (PKIX_ERROR_RECEIVED) {
                PKIX_DECREF(httpCertStore);
        }

        PKIX_RETURN(CERTSTORE);
}

/*
 * FUNCTION: PKIX_PL_HttpCertStore_Create
 * (see comments in pkix_samples_modules.h)
 */
PKIX_Error *
PKIX_PL_HttpCertStore_Create(
        PKIX_PL_HttpClient *client,
        PKIX_PL_GeneralName *location,
        PKIX_CertStore **pCertStore,
        void *plContext)
{
        PKIX_PL_String *locationString = NULL;
        char *locationAscii = NULL;
        PKIX_UInt32 len = 0;

        PKIX_ENTER(CERTSTORE, "PKIX_PL_HttpCertStore_Create");
        PKIX_NULLCHECK_TWO(location, pCertStore);

        PKIX_TOSTRING(location, &locationString, plContext,
                PKIX_GENERALNAMETOSTRINGFAILED);

        PKIX_CHECK(PKIX_PL_String_GetEncoded
                (locationString,
                PKIX_ESCASCII,
                (void **)&locationAscii,
                &len,
                plContext),
                PKIX_STRINGGETENCODEDFAILED);

        PKIX_CHECK(pkix_pl_HttpCertStore_CreateWithAsciiName
                (client, locationAscii, pCertStore, plContext),
                PKIX_HTTPCERTSTORECREATEWITHASCIINAMEFAILED);

cleanup:

        PKIX_DECREF(locationString);

        PKIX_RETURN(CERTSTORE);
}

/*
 * FUNCTION: pkix_HttpCertStore_FindSocketConnection
 * DESCRIPTION:
 *
        PRIntervalTime timeout,
        char *hostname,
        PRUint16 portnum,
        PRErrorCode *pStatus,
        PKIX_PL_Socket **pSocket,

 *  This function checks for an existing socket, creating a new one if unable
 *  to find an existing one, for the host pointed to by "hostname" and the port
 *  pointed to by "portnum". If a new socket is created the PRIntervalTime in
 *  "timeout" will be used for the timeout value and a creation status is
 *  returned at "pStatus". The address of the socket is stored at "pSocket".
 *
 * PARAMETERS:
 *  "timeout"
 *      The PRIntervalTime of the timeout value.
 *  "hostname"
 *      The address of the string containing the hostname. Must be non-NULL.
 *  "portnum"
 *      The port number for the desired socket.
 *  "pStatus"
 *      The address at which the status is stored. Must be non-NULL.
 *  "pSocket"
 *      The address at which the socket is stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a HttpCertStore Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_HttpCertStore_FindSocketConnection(
        PRIntervalTime timeout,
        char *hostname,
        PRUint16 portnum,
        PRErrorCode *pStatus,
        PKIX_PL_Socket **pSocket,
        void *plContext)
{
        PKIX_PL_String *formatString = NULL;
        PKIX_PL_String *hostString = NULL;
        PKIX_PL_String *domainString = NULL;
        PKIX_PL_Socket *socket = NULL;

        PKIX_ENTER(CERTSTORE, "pkix_HttpCertStore_FindSocketConnection");
        PKIX_NULLCHECK_THREE(hostname, pStatus, pSocket);

        *pStatus = 0;

        /* create PKIX_PL_String from hostname and port */
        PKIX_CHECK(PKIX_PL_String_Create
                (PKIX_ESCASCII, "%s:%d", 0, &formatString, plContext),
                PKIX_STRINGCREATEFAILED);

#if 0
hostname = "variation.red.iplanet.com";
portnum = 2001;
#endif

        PKIX_CHECK(PKIX_PL_String_Create
                (PKIX_ESCASCII, hostname, 0, &hostString, plContext),
                PKIX_STRINGCREATEFAILED);

        PKIX_CHECK(PKIX_PL_Sprintf
                (&domainString, plContext, formatString, hostString, portnum),
                PKIX_STRINGCREATEFAILED);

        /* Is this domainName already in cache? */
        PKIX_CHECK(PKIX_PL_HashTable_Lookup
                (httpSocketCache,
                (PKIX_PL_Object *)domainString,
                (PKIX_PL_Object **)&socket,
                plContext),
                PKIX_HASHTABLELOOKUPFAILED);

        if (socket == NULL) {

                /* No, create a connection (and cache it) */
                PKIX_CHECK(pkix_pl_Socket_CreateByHostAndPort
                        (PKIX_FALSE,       /* create a client, not a server */
                        timeout,
                        hostname,
                        portnum,
                        pStatus,
                        &socket,
                        plContext),
                        PKIX_SOCKETCREATEBYHOSTANDPORTFAILED);

                PKIX_CHECK(PKIX_PL_HashTable_Add
                        (httpSocketCache,
                        (PKIX_PL_Object *)domainString,
                        (PKIX_PL_Object *)socket,
                        plContext),
                        PKIX_HASHTABLEADDFAILED);

        }

        *pSocket = socket;

cleanup:

        PKIX_DECREF(formatString);
        PKIX_DECREF(hostString);
        PKIX_DECREF(domainString);

        PKIX_RETURN(CERTSTORE);
}

static const SEC_ASN1Template *
sec_attr_choose_attr_value_template(void *src_or_dest, PRBool encoding)
{
    const SEC_ASN1Template *theTemplate;

    SEC_PKCS7Attribute *attribute;
    SECOidData *oiddata;
    PRBool encoded;

    PORT_Assert (src_or_dest != NULL);
    if (src_or_dest == NULL)
	return NULL;

    attribute = (SEC_PKCS7Attribute*)src_or_dest;

    if (encoding && attribute->encoded)
	return SEC_ASN1_GET(SEC_AnyTemplate);

    oiddata = attribute->typeTag;
    if (oiddata == NULL) {
	oiddata = SECOID_FindOID(&attribute->type);
	attribute->typeTag = oiddata;
    }

    if (oiddata == NULL) {
	encoded = PR_TRUE;
	theTemplate = SEC_ASN1_GET(SEC_AnyTemplate);
    } else {
	switch (oiddata->offset) {
	  default:
	    encoded = PR_TRUE;
	    theTemplate = SEC_ASN1_GET(SEC_AnyTemplate);
	    break;
	  case SEC_OID_PKCS9_EMAIL_ADDRESS:
	  case SEC_OID_RFC1274_MAIL:
	  case SEC_OID_PKCS9_UNSTRUCTURED_NAME:
	    encoded = PR_FALSE;
	    theTemplate = SEC_ASN1_GET(SEC_IA5StringTemplate);
	    break;
	  case SEC_OID_PKCS9_CONTENT_TYPE:
	    encoded = PR_FALSE;
	    theTemplate = SEC_ASN1_GET(SEC_ObjectIDTemplate);
	    break;
	  case SEC_OID_PKCS9_MESSAGE_DIGEST:
	    encoded = PR_FALSE;
	    theTemplate = SEC_ASN1_GET(SEC_OctetStringTemplate);
	    break;
	  case SEC_OID_PKCS9_SIGNING_TIME:
	    encoded = PR_FALSE;
            theTemplate = SEC_ASN1_GET(CERT_TimeChoiceTemplate);
	    break;
	  /* XXX Want other types here, too */
	}
    }

    if (encoding) {
	/*
	 * If we are encoding and we think we have an already-encoded value,
	 * then the code which initialized this attribute should have set
	 * the "encoded" property to true (and we would have returned early,
	 * up above).  No devastating error, but that code should be fixed.
	 * (It could indicate that the resulting encoded bytes are wrong.)
	 */
	PORT_Assert (!encoded);
    } else {
	/*
	 * We are decoding; record whether the resulting value is
	 * still encoded or not.
	 */
	attribute->encoded = encoded;
    }
    return theTemplate;
}

static const SEC_ASN1Template *
sec_pkcs7_choose_content_template(void *src_or_dest, PRBool encoding);

static const SEC_ASN1TemplateChooserPtr sec_pkcs7_chooser
	= sec_pkcs7_choose_content_template;

static const SEC_ASN1TemplateChooserPtr sec_attr_chooser
	= sec_attr_choose_attr_value_template;

static const SEC_ASN1Template sec_pkcs7_attribute_template[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(SEC_PKCS7Attribute) },
    { SEC_ASN1_OBJECT_ID,
	  offsetof(SEC_PKCS7Attribute,type) },
    { SEC_ASN1_DYNAMIC | SEC_ASN1_SET_OF,
	  offsetof(SEC_PKCS7Attribute,values),
	  &sec_attr_chooser },
    { 0 }
};

static const SEC_ASN1Template sec_pkcs7_set_of_attribute_template[] = {
    { SEC_ASN1_SET_OF, 0, sec_pkcs7_attribute_template },
};

static const SEC_ASN1Template sec_PKCS7ContentInfoTemplate[] = {
    { SEC_ASN1_SEQUENCE | SEC_ASN1_MAY_STREAM,
	  0, NULL, sizeof(SEC_PKCS7ContentInfo) },
    { SEC_ASN1_OBJECT_ID,
	  offsetof(SEC_PKCS7ContentInfo,contentType) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_DYNAMIC | SEC_ASN1_MAY_STREAM
     | SEC_ASN1_EXPLICIT | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 0,
	  offsetof(SEC_PKCS7ContentInfo,content),
	  &sec_pkcs7_chooser },
    { 0 }
};

static const SEC_ASN1Template SEC_PKCS7SignerInfoTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(SEC_PKCS7SignerInfo) },
    { SEC_ASN1_INTEGER,
	  offsetof(SEC_PKCS7SignerInfo,version) },
    { SEC_ASN1_POINTER | SEC_ASN1_XTRN,
	  offsetof(SEC_PKCS7SignerInfo,issuerAndSN),
	  SEC_ASN1_SUB(CERT_IssuerAndSNTemplate) },
    { SEC_ASN1_INLINE | SEC_ASN1_XTRN,
	  offsetof(SEC_PKCS7SignerInfo,digestAlg),
	  SEC_ASN1_SUB(SECOID_AlgorithmIDTemplate) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 0,
	  offsetof(SEC_PKCS7SignerInfo,authAttr),
	  sec_pkcs7_set_of_attribute_template },
    { SEC_ASN1_INLINE | SEC_ASN1_XTRN,
	  offsetof(SEC_PKCS7SignerInfo,digestEncAlg),
	  SEC_ASN1_SUB(SECOID_AlgorithmIDTemplate) },
    { SEC_ASN1_OCTET_STRING,
	  offsetof(SEC_PKCS7SignerInfo,encDigest) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 1,
	  offsetof(SEC_PKCS7SignerInfo,unAuthAttr),
	  sec_pkcs7_set_of_attribute_template },
    { 0 }
};

static const SEC_ASN1Template SEC_PKCS7SignedDataTemplate[] = {
    { SEC_ASN1_SEQUENCE | SEC_ASN1_MAY_STREAM,
	  0, NULL, sizeof(SEC_PKCS7SignedData) },
    { SEC_ASN1_INTEGER,
	  offsetof(SEC_PKCS7SignedData,version) },
    { SEC_ASN1_SET_OF | SEC_ASN1_XTRN,
	  offsetof(SEC_PKCS7SignedData,digestAlgorithms),
	  SEC_ASN1_SUB(SECOID_AlgorithmIDTemplate) },
    { SEC_ASN1_INLINE,
	  offsetof(SEC_PKCS7SignedData,contentInfo),
	  sec_PKCS7ContentInfoTemplate },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC  |
      SEC_ASN1_XTRN | 0,
	  offsetof(SEC_PKCS7SignedData,rawCerts),
	  SEC_ASN1_SUB(SEC_SetOfAnyTemplate) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC  |
      SEC_ASN1_XTRN | 1,
	  offsetof(SEC_PKCS7SignedData,crls),
	  SEC_ASN1_SUB(CERT_SetOfSignedCrlTemplate) },
    { SEC_ASN1_SET_OF,
	  offsetof(SEC_PKCS7SignedData,signerInfos),
	  SEC_PKCS7SignerInfoTemplate },
    { 0 }
};

static const SEC_ASN1Template SEC_PointerToPKCS7SignedDataTemplate[] = {
    { SEC_ASN1_POINTER, 0, SEC_PKCS7SignedDataTemplate }
};

static const SEC_ASN1Template *
sec_pkcs7_choose_content_template(void *src_or_dest, PRBool encoding)
{
    const SEC_ASN1Template *theTemplate;
    SEC_PKCS7ContentInfo *cinfo;
    SECOidTag kind;

    PORT_Assert (src_or_dest != NULL);
    if (src_or_dest == NULL)
	return NULL;

    cinfo = (SEC_PKCS7ContentInfo*)src_or_dest;
    kind = SEC_PKCS7ContentType (cinfo);
    switch (kind) {
      default:
	theTemplate = SEC_ASN1_GET(SEC_PointerToAnyTemplate);
	break;
      case SEC_OID_PKCS7_SIGNED_DATA:
	theTemplate = SEC_PointerToPKCS7SignedDataTemplate;
	break;
    }
    return theTemplate;
}

/*
 * End of templates.  Do not add stuff after this; put new code
 * up above the start of the template definitions.
 */
