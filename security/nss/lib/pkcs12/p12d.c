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


#include "nssrenam.h"
#include "p12t.h"
#include "p12.h"
#include "plarena.h"
#include "secitem.h"
#include "secoid.h"
#include "seccomon.h"
#include "secport.h"
#include "cert.h"
#include "secpkcs7.h"
#include "secasn1.h"
#include "secerr.h"
#include "pk11func.h"
#include "p12plcy.h"
#include "p12local.h"
#include "alghmac.h"
#include "secder.h"
#include "secport.h"

#include "certdb.h"

#include "prcpucfg.h"

typedef struct sec_PKCS12SafeContentsContextStr sec_PKCS12SafeContentsContext;

/* Opaque structure for decoding SafeContents.  These are used
 * for each authenticated safe as well as any nested safe contents.
 */
struct sec_PKCS12SafeContentsContextStr {
    /* the parent decoder context */
    SEC_PKCS12DecoderContext *p12dcx;

    /* memory arena to allocate space from */
    PRArenaPool *arena;

    /* decoder context and destination for decoding safe contents */
    SEC_ASN1DecoderContext *safeContentsDcx;
    sec_PKCS12SafeContents safeContents;

    /* information for decoding safe bags within the safe contents.
     * these variables are updated for each safe bag decoded.
     */
    SEC_ASN1DecoderContext *currentSafeBagDcx;
    sec_PKCS12SafeBag *currentSafeBag;
    PRBool skipCurrentSafeBag;

    /* if the safe contents is nested, the parent is pointed to here. */
    sec_PKCS12SafeContentsContext *nestedCtx;
};

/* opaque decoder context structure.  information for decoding a pkcs 12
 * PDU are stored here as well as decoding pointers for intermediary 
 * structures which are part of the PKCS 12 PDU.  Upon a successful
 * decode, the safe bags containing certificates and keys encountered.
 */  
struct SEC_PKCS12DecoderContextStr {
    PRArenaPool *arena;
    PK11SlotInfo *slot;
    void *wincx;
    PRBool error;
    int errorValue;

    /* password */
    SECItem *pwitem;

    /* used for decoding the PFX structure */
    SEC_ASN1DecoderContext *pfxDcx;
    sec_PKCS12PFXItem pfx;

    /* safe bags found during decoding */  
    sec_PKCS12SafeBag **safeBags;
    unsigned int safeBagCount;

    /* state variables for decoding authenticated safes. */
    SEC_PKCS7DecoderContext *currentASafeP7Dcx;
    SEC_PKCS5KeyAndPassword *currentASafeKeyPwd;
    SEC_ASN1DecoderContext *aSafeDcx;
    SEC_PKCS7DecoderContext *aSafeP7Dcx;
    sec_PKCS12AuthenticatedSafe authSafe;
    SEC_PKCS7ContentInfo *aSafeCinfo;
    sec_PKCS12SafeContents safeContents;

    /* safe contents info */
    unsigned int safeContentsCnt;
    sec_PKCS12SafeContentsContext **safeContentsList;

    /* HMAC info */
    sec_PKCS12MacData	macData;
    SEC_ASN1DecoderContext *hmacDcx;

    /* routines for reading back the data to be hmac'd */
    digestOpenFn dOpen;
    digestCloseFn dClose;
    digestIOFn dRead, dWrite;
    void *dArg;

    /* helper functions */
    SECKEYGetPasswordKey pwfn;
    void *pwfnarg;
    PRBool swapUnicodeBytes;

    /* import information */
    PRBool bagsVerified;

    /* buffer management for the default callbacks implementation */
    void        *buffer;      /* storage area */
    PRInt32     filesize;     /* actual data size */
    PRInt32     allocated;    /* total buffer size allocated */
    PRInt32     currentpos;   /* position counter */
};


/* make sure that the PFX version being decoded is a version
 * which we support.
 */
static PRBool
sec_pkcs12_proper_version(sec_PKCS12PFXItem *pfx)
{
    /* if no version, assume it is not supported */
    if(pfx->version.len == 0) {
	return PR_FALSE;
    }

    if(DER_GetInteger(&pfx->version) > SEC_PKCS12_VERSION) {
	return PR_FALSE;
    }

    return PR_TRUE;
}

/* retrieve the key for decrypting the safe contents */ 
static PK11SymKey *
sec_pkcs12_decoder_get_decrypt_key(void *arg, SECAlgorithmID *algid)
{
    SEC_PKCS5KeyAndPassword *keyPwd = 
        (SEC_PKCS5KeyAndPassword *)arg;

    if(!keyPwd) {
	return NULL;
    }

    /* if no slot specified, use the internal key slot */
    if(!keyPwd->slot) {
	keyPwd->slot = PK11_GetInternalKeySlot();
    }

    /* retrieve the key */
    if(!keyPwd->key) {
	keyPwd->key = PK11_PBEKeyGen(keyPwd->slot, algid, 
				     keyPwd->pwitem, PR_FALSE, keyPwd->wincx);
    }

    return (PK11SymKey *)keyPwd;
}

/* XXX this needs to be modified to handle enveloped data.  most
 * likely, it should mirror the routines for SMIME in that regard.
 */
static PRBool
sec_pkcs12_decoder_decryption_allowed(SECAlgorithmID *algid, 
				      PK11SymKey *bulkkey)
{
    PRBool decryptionAllowed = SEC_PKCS12DecryptionAllowed(algid);

    if(!decryptionAllowed) {
	return PR_FALSE;
    }

    return PR_TRUE;
}

/* when we encounter a new safe bag during the decoding, we need
 * to allocate space for the bag to be decoded to and set the 
 * state variables appropriately.  all of the safe bags are allocated
 * in a buffer in the outer SEC_PKCS12DecoderContext, however,
 * a pointer to the safeBag is also used in the sec_PKCS12SafeContentsContext
 * for the current bag.
 */
static SECStatus
sec_pkcs12_decoder_init_new_safe_bag(sec_PKCS12SafeContentsContext 
						*safeContentsCtx)
{
    void *mark = NULL;
    SEC_PKCS12DecoderContext *p12dcx;

    /* make sure that the structures are defined, and there has
     * not been an error in the decoding 
     */
    if(!safeContentsCtx || !safeContentsCtx->p12dcx 
		|| safeContentsCtx->p12dcx->error) {
	return SECFailure;
    }

    p12dcx = safeContentsCtx->p12dcx;
    mark = PORT_ArenaMark(p12dcx->arena);

    /* allocate a new safe bag, if bags already exist, grow the 
     * list of bags, otherwise allocate a new list.  the list is
     * NULL terminated.
     */
    if(p12dcx->safeBagCount) {
	p12dcx->safeBags = 
	    (sec_PKCS12SafeBag**)PORT_ArenaGrow(p12dcx->arena,p12dcx->safeBags,
			(p12dcx->safeBagCount + 1) * sizeof(sec_PKCS12SafeBag *),
			(p12dcx->safeBagCount + 2) * sizeof(sec_PKCS12SafeBag *));
    } else {
	p12dcx->safeBags = (sec_PKCS12SafeBag**)PORT_ArenaZAlloc(p12dcx->arena,
					    2 * sizeof(sec_PKCS12SafeBag *));
    }
    if(!p12dcx->safeBags) {
	p12dcx->errorValue = SEC_ERROR_NO_MEMORY;
	goto loser;
    }

    /* append the bag to the end of the list and update the reference
     * in the safeContentsCtx.
     */
    p12dcx->safeBags[p12dcx->safeBagCount] = 
        (sec_PKCS12SafeBag*)PORT_ArenaZAlloc(p12dcx->arena,
					     sizeof(sec_PKCS12SafeBag));
    safeContentsCtx->currentSafeBag = p12dcx->safeBags[p12dcx->safeBagCount];
    p12dcx->safeBags[++p12dcx->safeBagCount] = NULL;
    if(!safeContentsCtx->currentSafeBag) {
	p12dcx->errorValue = SEC_ERROR_NO_MEMORY;
	goto loser;
    }

    safeContentsCtx->currentSafeBag->slot = safeContentsCtx->p12dcx->slot;
    safeContentsCtx->currentSafeBag->pwitem = safeContentsCtx->p12dcx->pwitem;
    safeContentsCtx->currentSafeBag->swapUnicodeBytes = 
				safeContentsCtx->p12dcx->swapUnicodeBytes;
    safeContentsCtx->currentSafeBag->arena = safeContentsCtx->p12dcx->arena;

    PORT_ArenaUnmark(p12dcx->arena, mark);
    return SECSuccess;

loser:

    /* if an error occurred, release the memory and set the error flag
     * the only possible errors triggered by this function are memory 
     * related.
     */
    if(mark) {
	PORT_ArenaRelease(p12dcx->arena, mark);
    }

    p12dcx->error = PR_TRUE;
    return SECFailure;
}

/* A wrapper for updating the ASN1 context in which a safeBag is
 * being decoded.  This function is called as a callback from
 * secasn1d when decoding SafeContents structures.
 */
static void
sec_pkcs12_decoder_safe_bag_update(void *arg, const char *data, 
				   unsigned long len, int depth, 
				   SEC_ASN1EncodingPart data_kind)
{
    sec_PKCS12SafeContentsContext *safeContentsCtx = 
        (sec_PKCS12SafeContentsContext *)arg;
    SEC_PKCS12DecoderContext *p12dcx;
    SECStatus rv;

    /* make sure that we are not skipping the current safeBag,
     * and that there are no errors.  If so, just return rather
     * than continuing to process.
     */
    if(!safeContentsCtx || !safeContentsCtx->p12dcx 
		|| safeContentsCtx->p12dcx->error 
		|| safeContentsCtx->skipCurrentSafeBag) {
	return;
    }
    p12dcx = safeContentsCtx->p12dcx;

    rv = SEC_ASN1DecoderUpdate(safeContentsCtx->currentSafeBagDcx, data, len);
    if(rv != SECSuccess) {
	p12dcx->errorValue = SEC_ERROR_NO_MEMORY;
	goto loser;
    }

    return;

loser:
    /* set the error, and finish the decoder context.  because there 
     * is not a way of returning an error message, it may be worth
     * while to do a check higher up and finish any decoding contexts
     * that are still open.
     */
    p12dcx->error = PR_TRUE;
    SEC_ASN1DecoderFinish(safeContentsCtx->currentSafeBagDcx);
    safeContentsCtx->currentSafeBagDcx = NULL;
    return;
}

/* forward declarations of functions that are used when decoding
 * safeContents bags which are nested and when decoding the 
 * authenticatedSafes.
 */
static SECStatus
sec_pkcs12_decoder_begin_nested_safe_contents(sec_PKCS12SafeContentsContext 
							*safeContentsCtx);
static SECStatus
sec_pkcs12_decoder_finish_nested_safe_contents(sec_PKCS12SafeContentsContext
							*safeContentsCtx);
static void
sec_pkcs12_decoder_safe_bag_update(void *arg, const char *data, 
				   unsigned long len, int depth, 
				   SEC_ASN1EncodingPart data_kind);

/* notify function for decoding safeBags.  This function is
 * used to filter safeBag types which are not supported,
 * initiate the decoding of nested safe contents, and decode
 * safeBags in general.  this function is set when the decoder
 * context for the safeBag is first created.
 */
static void
sec_pkcs12_decoder_safe_bag_notify(void *arg, PRBool before,
				   void *dest, int real_depth)
{
    sec_PKCS12SafeContentsContext *safeContentsCtx = 
        (sec_PKCS12SafeContentsContext *)arg;
    SEC_PKCS12DecoderContext *p12dcx;
    sec_PKCS12SafeBag *bag;
    PRBool after;

    /* if an error is encountered, return */
    if(!safeContentsCtx || !safeContentsCtx->p12dcx || 
		safeContentsCtx->p12dcx->error) {
	return;
    }
    p12dcx = safeContentsCtx->p12dcx;

    /* to make things more readable */
    if(before)
	after = PR_FALSE;
    else 
	after = PR_TRUE;

    /* have we determined the safeBagType yet? */
    bag = safeContentsCtx->currentSafeBag;
    if(bag->bagTypeTag == NULL) {
	if(after && (dest == &(bag->safeBagType))) {
	    bag->bagTypeTag = SECOID_FindOID(&(bag->safeBagType));
	    if(bag->bagTypeTag == NULL) {
		p12dcx->error = PR_TRUE;
		p12dcx->errorValue = SEC_ERROR_PKCS12_CORRUPT_PFX_STRUCTURE;
	    }
	}
	return;
    }

    /* process the safeBag depending on it's type.  those
     * which we do not support, are ignored.  we start a decoding
     * context for a nested safeContents.
     */
    switch(bag->bagTypeTag->offset) {
	case SEC_OID_PKCS12_V1_KEY_BAG_ID:
	case SEC_OID_PKCS12_V1_CERT_BAG_ID:
	case SEC_OID_PKCS12_V1_PKCS8_SHROUDED_KEY_BAG_ID:
	    break;
	case SEC_OID_PKCS12_V1_SAFE_CONTENTS_BAG_ID:
	    /* if we are just starting to decode the safeContents, initialize
	     * a new safeContentsCtx to process it.
	     */
	    if(before && (dest == &(bag->safeBagContent))) {
		sec_pkcs12_decoder_begin_nested_safe_contents(safeContentsCtx);
	    } else if(after && (dest == &(bag->safeBagContent))) {
		/* clean up the nested decoding */
		sec_pkcs12_decoder_finish_nested_safe_contents(safeContentsCtx);
	    }
	    break;
	case SEC_OID_PKCS12_V1_CRL_BAG_ID:
	case SEC_OID_PKCS12_V1_SECRET_BAG_ID:
	default:
	    /* skip any safe bag types we don't understand or handle */
	    safeContentsCtx->skipCurrentSafeBag = PR_TRUE;
	    break;
    }

    return;
}

/* notify function for decoding safe contents.  each entry in the
 * safe contents is a safeBag which needs to be allocated and
 * the decoding context initialized at the beginning and then
 * the context needs to be closed and finished at the end.
 *
 * this function is set when the safeContents decode context is
 * initialized.
 */
static void
sec_pkcs12_decoder_safe_contents_notify(void *arg, PRBool before,
					void *dest, int real_depth)
{
    sec_PKCS12SafeContentsContext *safeContentsCtx = 
        (sec_PKCS12SafeContentsContext*)arg;
    SEC_PKCS12DecoderContext *p12dcx;
    SECStatus rv;

    /* if there is an error we don't want to continue processing,
     * just return and keep going.
     */
    if(!safeContentsCtx || !safeContentsCtx->p12dcx 
		|| safeContentsCtx->p12dcx->error) {
	return;
    }
    p12dcx = safeContentsCtx->p12dcx;

    /* if we are done with the current safeBag, then we need to
     * finish the context and set the state variables appropriately.
     */
    if(!before) {
	SEC_ASN1DecoderClearFilterProc(safeContentsCtx->safeContentsDcx);
	SEC_ASN1DecoderFinish(safeContentsCtx->currentSafeBagDcx);
	safeContentsCtx->currentSafeBagDcx = NULL;
	safeContentsCtx->skipCurrentSafeBag = PR_FALSE;
    } else {
	/* we are starting a new safe bag.  we need to allocate space
	 * for the bag and initialize the decoding context.
	 */
	rv = sec_pkcs12_decoder_init_new_safe_bag(safeContentsCtx);
	if(rv != SECSuccess) {
	    goto loser;
	}

	/* set up the decoder context */
	safeContentsCtx->currentSafeBagDcx = SEC_ASN1DecoderStart(p12dcx->arena,
						safeContentsCtx->currentSafeBag,
						sec_PKCS12SafeBagTemplate);
	if(!safeContentsCtx->currentSafeBagDcx) {
	    p12dcx->errorValue = SEC_ERROR_NO_MEMORY;
	    goto loser;
	}

	/* set the notify and filter procs so that the safe bag
	 * data gets sent to the proper location when decoding.
	 */
	SEC_ASN1DecoderSetNotifyProc(safeContentsCtx->currentSafeBagDcx, 
				 sec_pkcs12_decoder_safe_bag_notify, 
				 safeContentsCtx);
	SEC_ASN1DecoderSetFilterProc(safeContentsCtx->safeContentsDcx, 
				 sec_pkcs12_decoder_safe_bag_update, 
				 safeContentsCtx, PR_TRUE);
    }

    return;

loser:
    /* in the event of an error, we want to close the decoding
     * context and clear the filter and notify procedures.
     */
    p12dcx->error = PR_TRUE;

    if(safeContentsCtx->currentSafeBagDcx) {
	SEC_ASN1DecoderFinish(safeContentsCtx->currentSafeBagDcx);
	safeContentsCtx->currentSafeBagDcx = NULL;
    }

    SEC_ASN1DecoderClearNotifyProc(safeContentsCtx->safeContentsDcx);
    SEC_ASN1DecoderClearFilterProc(safeContentsCtx->safeContentsDcx);

    return;
}

/* initialize the safeContents for decoding.  this routine
 * is used for authenticatedSafes as well as nested safeContents.
 */
static sec_PKCS12SafeContentsContext *
sec_pkcs12_decoder_safe_contents_init_decode(SEC_PKCS12DecoderContext *p12dcx,
					     PRBool nestedSafe)
{
    sec_PKCS12SafeContentsContext *safeContentsCtx = NULL;
    const SEC_ASN1Template *theTemplate;

    if(!p12dcx || p12dcx->error) {
	return NULL;
    }

    /* allocate a new safeContents list or grow the existing list and
     * append the new safeContents onto the end.
     */
    if(!p12dcx->safeContentsCnt) {
	p12dcx->safeContentsList = 
	    (sec_PKCS12SafeContentsContext**)PORT_ArenaZAlloc(p12dcx->arena, 
	       			 sizeof(sec_PKCS12SafeContentsContext *));
    } else {
	p12dcx->safeContentsList = 
	   (sec_PKCS12SafeContentsContext **) PORT_ArenaGrow(p12dcx->arena,
			p12dcx->safeContentsList,
			(p12dcx->safeContentsCnt * 
				sizeof(sec_PKCS12SafeContentsContext *)),
			(1 + p12dcx->safeContentsCnt * 
				sizeof(sec_PKCS12SafeContentsContext *)));
    }
    if(!p12dcx->safeContentsList) {
	p12dcx->errorValue = SEC_ERROR_NO_MEMORY;
	goto loser;
    }

    p12dcx->safeContentsList[p12dcx->safeContentsCnt] = 
        (sec_PKCS12SafeContentsContext*)PORT_ArenaZAlloc(
					p12dcx->arena,
					sizeof(sec_PKCS12SafeContentsContext));
    p12dcx->safeContentsList[p12dcx->safeContentsCnt+1] = NULL;
    if(!p12dcx->safeContentsList[p12dcx->safeContentsCnt]) {
	p12dcx->errorValue = SEC_ERROR_NO_MEMORY;
	goto loser;
    }

    /* set up the state variables */
    safeContentsCtx = p12dcx->safeContentsList[p12dcx->safeContentsCnt];
    p12dcx->safeContentsCnt++;
    safeContentsCtx->p12dcx = p12dcx;
    safeContentsCtx->arena = p12dcx->arena;

    /* begin the decoding -- the template is based on whether we are
     * decoding a nested safeContents or not.
     */
    if(nestedSafe == PR_TRUE) {
	theTemplate = sec_PKCS12NestedSafeContentsDecodeTemplate;
    } else {
	theTemplate = sec_PKCS12SafeContentsDecodeTemplate;
    }

    /* start the decoder context */
    safeContentsCtx->safeContentsDcx = SEC_ASN1DecoderStart(p12dcx->arena, 
					&safeContentsCtx->safeContents,
					theTemplate);
	
    if(!safeContentsCtx->safeContentsDcx) {
	p12dcx->errorValue = SEC_ERROR_NO_MEMORY;
	goto loser;
    }

    /* set the safeContents notify procedure to look for
     * and start the decode of safeBags.
     */
    SEC_ASN1DecoderSetNotifyProc(safeContentsCtx->safeContentsDcx, 
				sec_pkcs12_decoder_safe_contents_notify,
				safeContentsCtx);

    return safeContentsCtx;

loser:
    /* in the case of an error, we want to finish the decoder
     * context and set the error flag.
     */
    if(safeContentsCtx && safeContentsCtx->safeContentsDcx) {
	SEC_ASN1DecoderFinish(safeContentsCtx->safeContentsDcx);
	safeContentsCtx->safeContentsDcx = NULL;
    }

    p12dcx->error = PR_TRUE;

    return NULL;
}

/* wrapper for updating safeContents.  this is set as the filter of
 * safeBag when there is a nested safeContents.
 */
static void
sec_pkcs12_decoder_nested_safe_contents_update(void *arg, const char *buf,
					  unsigned long len, int depth,
					  SEC_ASN1EncodingPart data_kind)
{
    sec_PKCS12SafeContentsContext *safeContentsCtx = 
        (sec_PKCS12SafeContentsContext *)arg;
    SEC_PKCS12DecoderContext *p12dcx;
    SECStatus rv;

    /* check for an error */
    if(!safeContentsCtx || !safeContentsCtx->p12dcx 
			|| safeContentsCtx->p12dcx->error) {
	return;
    }

    /* no need to update if no data sent in */
    if(!len || !buf) {
	return;
    }

    /* update the decoding context */
    p12dcx = safeContentsCtx->p12dcx;
    rv = SEC_ASN1DecoderUpdate(safeContentsCtx->safeContentsDcx, buf, len);
    if(rv != SECSuccess) {
	p12dcx->errorValue = SEC_ERROR_NO_MEMORY;
	goto loser;
    }

    return;

loser:
    /* handle any errors.  If a decoding context is open, close it. */
    p12dcx->error = PR_TRUE;
    if(safeContentsCtx->safeContentsDcx) {
	SEC_ASN1DecoderFinish(safeContentsCtx->safeContentsDcx);
	safeContentsCtx->safeContentsDcx = NULL;
    }
}

/* whenever a new safeContentsSafeBag is encountered, we need
 * to init a safeContentsContext.  
 */
static SECStatus  
sec_pkcs12_decoder_begin_nested_safe_contents(sec_PKCS12SafeContentsContext 
							*safeContentsCtx)
{
    /* check for an error */
    if(!safeContentsCtx || !safeContentsCtx->p12dcx || 
		safeContentsCtx->p12dcx->error) {
	return SECFailure;
    }

    safeContentsCtx->nestedCtx = sec_pkcs12_decoder_safe_contents_init_decode(
						safeContentsCtx->p12dcx,
						PR_TRUE);
    if(!safeContentsCtx->nestedCtx) {
	return SECFailure;
    }

    /* set up new filter proc */
    SEC_ASN1DecoderSetNotifyProc(safeContentsCtx->nestedCtx->safeContentsDcx,
				 sec_pkcs12_decoder_safe_contents_notify,
				 safeContentsCtx->nestedCtx);
    SEC_ASN1DecoderSetFilterProc(safeContentsCtx->currentSafeBagDcx,
				 sec_pkcs12_decoder_nested_safe_contents_update,
				 safeContentsCtx->nestedCtx, PR_TRUE);

    return SECSuccess;
}

/* when the safeContents is done decoding, we need to reset the
 * proper filter and notify procs and close the decoding context 
 */
static SECStatus
sec_pkcs12_decoder_finish_nested_safe_contents(sec_PKCS12SafeContentsContext
							*safeContentsCtx)
{
    /* check for error */
    if(!safeContentsCtx || !safeContentsCtx->p12dcx || 
		safeContentsCtx->p12dcx->error) {
	return SECFailure;
    }

    /* clean up */	
    SEC_ASN1DecoderClearFilterProc(safeContentsCtx->currentSafeBagDcx);
    SEC_ASN1DecoderClearNotifyProc(safeContentsCtx->nestedCtx->safeContentsDcx);
    SEC_ASN1DecoderFinish(safeContentsCtx->nestedCtx->safeContentsDcx);
    safeContentsCtx->nestedCtx->safeContentsDcx = NULL;
    safeContentsCtx->nestedCtx = NULL;

    return SECSuccess;
}

/* wrapper for updating safeContents.  This is used when decoding
 * the nested safeContents and any authenticatedSafes.
 */
static void
sec_pkcs12_decoder_safe_contents_callback(void *arg, const char *buf,
					  unsigned long len)
{
    SECStatus rv;
    sec_PKCS12SafeContentsContext *safeContentsCtx = 
        (sec_PKCS12SafeContentsContext *)arg;
    SEC_PKCS12DecoderContext *p12dcx;

    /* check for error */  
    if(!safeContentsCtx || !safeContentsCtx->p12dcx 
		|| safeContentsCtx->p12dcx->error) {
	return;
    }
    p12dcx = safeContentsCtx->p12dcx;

    /* update the decoder */
    rv = SEC_ASN1DecoderUpdate(safeContentsCtx->safeContentsDcx, buf, len);
    if(rv != SECSuccess) {
	/* if we fail while trying to decode a 'safe', it's probably because
	 * we didn't have the correct password. */
	PORT_SetError(SEC_ERROR_BAD_PASSWORD);
	p12dcx->errorValue = SEC_ERROR_PKCS12_CORRUPT_PFX_STRUCTURE;
	goto loser;
    }

    return;

loser:
    /* set the error and finish the context */
    p12dcx->error = PR_TRUE;
    if(safeContentsCtx->safeContentsDcx) {
	SEC_ASN1DecoderFinish(safeContentsCtx->safeContentsDcx);
	safeContentsCtx->safeContentsDcx = NULL;
    }

    return;
}

/* this is a wrapper for the ASN1 decoder to call SEC_PKCS7DecoderUpdate
 */
static void
sec_pkcs12_decoder_wrap_p7_update(void *arg, const char *data,
				  unsigned long len, int depth,
				  SEC_ASN1EncodingPart data_kind)
{
    SEC_PKCS7DecoderContext *p7dcx = (SEC_PKCS7DecoderContext *)arg;

    SEC_PKCS7DecoderUpdate(p7dcx, data, len);
}

/* notify function for decoding aSafes.  at the beginning,
 * of an authenticatedSafe, we start a decode of a safeContents.
 * at the end, we clean up the safeContents decoder context and
 * reset state variables 
 */
static void
sec_pkcs12_decoder_asafes_notify(void *arg, PRBool before, void *dest, 
			       int real_depth)
{
    SEC_PKCS12DecoderContext *p12dcx;
    sec_PKCS12SafeContentsContext *safeContentsCtx;

    /* make sure no error occurred. */
    p12dcx = (SEC_PKCS12DecoderContext *)arg;
    if(!p12dcx || p12dcx->error) {
	return;
    }

    if(before) {

	/* init a new safeContentsContext */
	safeContentsCtx = sec_pkcs12_decoder_safe_contents_init_decode(p12dcx, 
								PR_FALSE);
	if(!safeContentsCtx) {
	    goto loser;
	}

	/* set up password and encryption key information */
	p12dcx->currentASafeKeyPwd = 
	    (SEC_PKCS5KeyAndPassword*)PORT_ArenaZAlloc(p12dcx->arena, 
					      sizeof(SEC_PKCS5KeyAndPassword));
	if(!p12dcx->currentASafeKeyPwd) {
	    p12dcx->errorValue = SEC_ERROR_NO_MEMORY;
	    goto loser;
	}
	p12dcx->currentASafeKeyPwd->pwitem = p12dcx->pwitem;
	p12dcx->currentASafeKeyPwd->slot = p12dcx->slot;
	p12dcx->currentASafeKeyPwd->wincx = p12dcx->wincx;

	/* initiate the PKCS7ContentInfo decode */
	p12dcx->currentASafeP7Dcx = SEC_PKCS7DecoderStart(
				sec_pkcs12_decoder_safe_contents_callback,
				safeContentsCtx, 
				p12dcx->pwfn, p12dcx->pwfnarg,
				sec_pkcs12_decoder_get_decrypt_key, 
				p12dcx->currentASafeKeyPwd, 
				sec_pkcs12_decoder_decryption_allowed);
	if(!p12dcx->currentASafeP7Dcx) {
	    p12dcx->errorValue = PORT_GetError();
	    goto loser;
	}
	SEC_ASN1DecoderSetFilterProc(p12dcx->aSafeDcx, 
				     sec_pkcs12_decoder_wrap_p7_update,
				     p12dcx->currentASafeP7Dcx, PR_TRUE);
    }

    if(!before) {
	/* if one is being decoded, finish the decode */
	if(p12dcx->currentASafeP7Dcx != NULL) {
	    if(!SEC_PKCS7DecoderFinish(p12dcx->currentASafeP7Dcx)) {
		p12dcx->currentASafeP7Dcx = NULL;
		p12dcx->errorValue = PORT_GetError();
		goto loser;
	    }
	    p12dcx->currentASafeP7Dcx = NULL;
	}
	p12dcx->currentASafeP7Dcx = NULL;
	if(p12dcx->currentASafeKeyPwd->key != NULL) {
	    p12dcx->currentASafeKeyPwd->key = NULL;
	}
    }


    return;

loser:
    /* set the error flag */
    p12dcx->error = PR_TRUE;
    return;
}

/* wrapper for updating asafes decoding context.  this function
 * writes data being decoded to disk, so that a mac can be computed
 * later.  
 */
static void
sec_pkcs12_decoder_asafes_callback(void *arg, const char *buf, 
				  unsigned long len)
{
    SEC_PKCS12DecoderContext *p12dcx = (SEC_PKCS12DecoderContext *)arg;
    SECStatus rv;

    if(!p12dcx || p12dcx->error) {
	return;
    }

    /* update the context */
    rv = SEC_ASN1DecoderUpdate(p12dcx->aSafeDcx, buf, len);
    if(rv != SECSuccess) {
	p12dcx->error = (PRBool)SEC_ERROR_NO_MEMORY;
	goto loser;
    }

    /* if we are writing to a file, write out the new information */
    if(p12dcx->dWrite) {
	unsigned long writeLen = (*p12dcx->dWrite)(p12dcx->dArg, 	
						   (unsigned char *)buf, len);
	if(writeLen != len) {
	    p12dcx->errorValue = PORT_GetError();
	    goto loser;
	}
    }

    return;

loser:
    /* set the error flag */
    p12dcx->error = PR_TRUE;
    SEC_ASN1DecoderFinish(p12dcx->aSafeDcx);
    p12dcx->aSafeDcx = NULL;

    return;
}
   
/* start the decode of an authenticatedSafe contentInfo.
 */ 
static SECStatus
sec_pkcs12_decode_start_asafes_cinfo(SEC_PKCS12DecoderContext *p12dcx)
{
    if(!p12dcx || p12dcx->error) {
	return SECFailure;
    }

    /* start the decode context */
    p12dcx->aSafeDcx = SEC_ASN1DecoderStart(p12dcx->arena, 
    					&p12dcx->authSafe,
    					sec_PKCS12AuthenticatedSafeTemplate);
    if(!p12dcx->aSafeDcx) {
	p12dcx->errorValue = SEC_ERROR_NO_MEMORY;
   	goto loser;
    }

    /* set the notify function */
    SEC_ASN1DecoderSetNotifyProc(p12dcx->aSafeDcx,
    				 sec_pkcs12_decoder_asafes_notify, p12dcx);

    /* begin the authSafe decoder context */
    p12dcx->aSafeP7Dcx = SEC_PKCS7DecoderStart(
    				sec_pkcs12_decoder_asafes_callback, p12dcx,
    				p12dcx->pwfn, p12dcx->pwfnarg, NULL, NULL, NULL);
    if(!p12dcx->aSafeP7Dcx) {
	p12dcx->errorValue = SEC_ERROR_NO_MEMORY;
	goto loser;
    }
  
    /* open the temp file for writing, if the filter functions were set */ 
    if(p12dcx->dOpen && (*p12dcx->dOpen)(p12dcx->dArg, PR_FALSE) 
				!= SECSuccess) {
	p12dcx->errorValue = PORT_GetError();
	goto loser;
    }

    return SECSuccess;

loser:
    p12dcx->error = PR_TRUE;

    if(p12dcx->aSafeDcx) {
	SEC_ASN1DecoderFinish(p12dcx->aSafeDcx);
	p12dcx->aSafeDcx = NULL;
    } 

    if(p12dcx->aSafeP7Dcx) {
	SEC_PKCS7DecoderFinish(p12dcx->aSafeP7Dcx);
	p12dcx->aSafeP7Dcx = NULL;
    }

    return SECFailure;
}

/* wrapper for updating the safeContents.  this function is used as
 * a filter for the pfx when decoding the authenticated safes 
 */
static void 
sec_pkcs12_decode_asafes_cinfo_update(void *arg, const char *buf,
				      unsigned long len, int depth,
				      SEC_ASN1EncodingPart data_kind)
{
    SEC_PKCS12DecoderContext *p12dcx;
    SECStatus rv;

    p12dcx = (SEC_PKCS12DecoderContext*)arg;
    if(!p12dcx || p12dcx->error) {
	return;
    }

    /* update the safeContents decoder */
    rv = SEC_PKCS7DecoderUpdate(p12dcx->aSafeP7Dcx, buf, len);
    if(rv != SECSuccess) {
	p12dcx->errorValue = SEC_ERROR_PKCS12_CORRUPT_PFX_STRUCTURE;
	goto loser;
    }

    return;

loser:

    /* did we find an error?  if so, close the context and set the 
     * error flag.
     */
    SEC_PKCS7DecoderFinish(p12dcx->aSafeP7Dcx);
    p12dcx->aSafeP7Dcx = NULL;
    p12dcx->error = PR_TRUE;
}

/* notify procedure used while decoding the pfx.  When we encounter
 * the authSafes, we want to trigger the decoding of authSafes as well
 * as when we encounter the macData, trigger the decoding of it.  we do
 * this because we we are streaming the decoder and not decoding in place.
 * the pfx which is the destination, only has the version decoded into it.
 */
static void 
sec_pkcs12_decoder_pfx_notify_proc(void *arg, PRBool before, void *dest,
				   int real_depth)
{
    SECStatus rv;
    SEC_PKCS12DecoderContext *p12dcx = (SEC_PKCS12DecoderContext*)arg;

    /* if an error occurrs, clear the notifyProc and the filterProc 
     * and continue. 
     */
    if(p12dcx->error) {
	SEC_ASN1DecoderClearNotifyProc(p12dcx->pfxDcx);
	SEC_ASN1DecoderClearFilterProc(p12dcx->pfxDcx);
	return;
    }

    if(before && (dest == &p12dcx->pfx.encodedAuthSafe)) {

	/* we want to make sure this is a version we support */
	if(!sec_pkcs12_proper_version(&p12dcx->pfx)) {
	    p12dcx->errorValue = SEC_ERROR_PKCS12_UNSUPPORTED_VERSION;
	    goto loser;
	}

	/* start the decode of the aSafes cinfo... */
	rv = sec_pkcs12_decode_start_asafes_cinfo(p12dcx);
	if(rv != SECSuccess) {
	    goto loser;
	}

	/* set the filter proc to update the authenticated safes. */
	SEC_ASN1DecoderSetFilterProc(p12dcx->pfxDcx,
				     sec_pkcs12_decode_asafes_cinfo_update,
				     p12dcx, PR_TRUE);
    }

    if(!before && (dest == &p12dcx->pfx.encodedAuthSafe)) {

	/* we are done decoding the authenticatedSafes, so we need to 
	 * finish the decoderContext and clear the filter proc
	 * and close the hmac callback, if present
	 */
	p12dcx->aSafeCinfo = SEC_PKCS7DecoderFinish(p12dcx->aSafeP7Dcx);
	p12dcx->aSafeP7Dcx = NULL;
	if(!p12dcx->aSafeCinfo) {
	    p12dcx->errorValue = PORT_GetError();
	    goto loser;
	}
	SEC_ASN1DecoderClearFilterProc(p12dcx->pfxDcx);
	if(p12dcx->dClose && ((*p12dcx->dClose)(p12dcx->dArg, PR_FALSE) 
				!= SECSuccess)) {
	    p12dcx->errorValue = PORT_GetError();
	    goto loser;
	}

    }

    return;

loser:
    p12dcx->error = PR_TRUE;
}

/*  default implementations of the open/close/read/write functions for
    SEC_PKCS12DecoderStart 
*/

#define DEFAULT_TEMP_SIZE 4096

static SECStatus
p12u_DigestOpen(void *arg, PRBool readData)
{
    SEC_PKCS12DecoderContext* p12cxt = arg;

    p12cxt->currentpos = 0;

    if (PR_FALSE == readData) {
        /* allocate an initial buffer */
        p12cxt->filesize = 0;
        p12cxt->allocated = DEFAULT_TEMP_SIZE;
        p12cxt->buffer = PORT_Alloc(DEFAULT_TEMP_SIZE);
        PR_ASSERT(p12cxt->buffer);
    }
    else
    {
        PR_ASSERT(p12cxt->buffer);
        if (!p12cxt->buffer) {
            return SECFailure; /* no data to read */
        }
    }

    return SECSuccess;
}

static SECStatus
p12u_DigestClose(void *arg, PRBool removeFile)
{
    SEC_PKCS12DecoderContext* p12cxt = arg;

    PR_ASSERT(p12cxt);
    if (!p12cxt) {
        return SECFailure;
    }
    p12cxt->currentpos = 0;

    if (PR_TRUE == removeFile) {
        PR_ASSERT(p12cxt->buffer);
        if (!p12cxt->buffer) {
            return SECFailure;
        }
        if (p12cxt->buffer) {
            PORT_Free(p12cxt->buffer);
            p12cxt->buffer = NULL;
            p12cxt->allocated = 0;
            p12cxt->filesize = 0;
        }
    }

    return SECSuccess;
}

static int
p12u_DigestRead(void *arg, unsigned char *buf, unsigned long len)
{
    int toread = len;
    SEC_PKCS12DecoderContext* p12cxt = arg;

    if(!buf || len == 0) {
	return -1;
    }

    if (!p12cxt->buffer || ((p12cxt->filesize-p12cxt->currentpos)<len) ) {
        /* trying to read past the end of the buffer */
        toread = p12cxt->filesize-p12cxt->currentpos;
    }
    memcpy(buf, (void*)((char*)p12cxt->buffer+p12cxt->currentpos), toread);
    p12cxt->currentpos += toread;
    return toread;
}

static int
p12u_DigestWrite(void *arg, unsigned char *buf, unsigned long len)
{
    SEC_PKCS12DecoderContext* p12cxt = arg;

    if(!buf || len == 0) {
        return -1;
    }

    if (p12cxt->currentpos+len > p12cxt->filesize) {
        p12cxt->filesize = p12cxt->currentpos + len;
    }
    else {
        p12cxt->filesize += len;
    }
    if (p12cxt->filesize > p12cxt->allocated) {
        void* newbuffer;
        size_t newsize = p12cxt->filesize + DEFAULT_TEMP_SIZE;
        newbuffer  = PORT_Realloc(p12cxt->buffer, newsize);
        if (NULL == newbuffer) {
            return -1; /* can't extend the buffer */
        }
        p12cxt->buffer = newbuffer;
        p12cxt->allocated = newsize;
    }
    PR_ASSERT(p12cxt->buffer);
    memcpy((void*)((char*)p12cxt->buffer+p12cxt->currentpos), buf, len);
    p12cxt->currentpos += len;
    return len;
}

/* SEC_PKCS12DecoderStart
 *	Creates a decoder context for decoding a PKCS 12 PDU objct.
 *	This function sets up the initial decoding context for the
 *	PFX and sets the needed state variables.
 *
 *	pwitem - the password for the hMac and any encoded safes.
 *		 this should be changed to take a callback which retrieves
 *		 the password.  it may be possible for different safes to
 *		 have different passwords.  also, the password is already
 *		 in unicode.  it should probably be converted down below via
 *		 a unicode conversion callback.
 *	slot - the slot to import the dataa into should multiple slots 
 *		 be supported based on key type and cert type?
 *	dOpen, dClose, dRead, dWrite - digest routines for writing data
 *		 to a file so it could be read back and the hmack recomputed
 *		 and verified.  doesn't seem to be away for both encoding
 *		 and decoding to be single pass, thus the need for these
 *		 routines.
 *	dArg - the argument for dOpen, etc.
 *
 *      if NULL == dOpen == dClose == dRead == dWrite == dArg, then default
 *      implementations using a memory buffer are used
 *
 *	This function returns the decoder context, if it was successful.
 *	Otherwise, null is returned.
 */
SEC_PKCS12DecoderContext *
SEC_PKCS12DecoderStart(SECItem *pwitem, PK11SlotInfo *slot, void *wincx,
		       digestOpenFn dOpen, digestCloseFn dClose, 
		       digestIOFn dRead, digestIOFn dWrite, void *dArg)
{
    SEC_PKCS12DecoderContext *p12dcx;
    PRArenaPool *arena;

    arena = PORT_NewArena(2048); /* different size? */
    if(!arena) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	return NULL;
    }

    /* allocate the decoder context and set the state variables */
    p12dcx = (SEC_PKCS12DecoderContext*)PORT_ArenaZAlloc(arena, sizeof(SEC_PKCS12DecoderContext));
    if(!p12dcx) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	goto loser;
    }

    if (!dOpen && !dClose && !dRead && !dWrite && !dArg) {
        /* use default implementations */
        dOpen = p12u_DigestOpen;
        dClose = p12u_DigestClose;
        dRead = p12u_DigestRead;
        dWrite = p12u_DigestWrite;
        dArg = (void*)p12dcx;
    }

    p12dcx->arena = arena;
    p12dcx->pwitem = pwitem;
    p12dcx->slot = (slot ? slot : PK11_GetInternalKeySlot());
    p12dcx->wincx = wincx;
#ifdef IS_LITTLE_ENDIAN
    p12dcx->swapUnicodeBytes = PR_TRUE;
#else
    p12dcx->swapUnicodeBytes = PR_FALSE;
#endif
    p12dcx->errorValue = 0;
    p12dcx->error = PR_FALSE;

    /* a slot is *required */
    if(!slot) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	goto loser;
    }

    /* start the decoding of the PFX and set the notify proc
     * for the PFX item.
     */
    p12dcx->pfxDcx = SEC_ASN1DecoderStart(p12dcx->arena, &p12dcx->pfx,
    					  sec_PKCS12PFXItemTemplate);
    if(!p12dcx->pfxDcx) {
	PORT_SetError(SEC_ERROR_NO_MEMORY); 
	goto loser;
    }

    SEC_ASN1DecoderSetNotifyProc(p12dcx->pfxDcx, 
				 sec_pkcs12_decoder_pfx_notify_proc,
    				 p12dcx); 
    
    /* set up digest functions */
    p12dcx->dOpen = dOpen;
    p12dcx->dWrite = dWrite;
    p12dcx->dClose = dClose;
    p12dcx->dRead = dRead;
    p12dcx->dArg = dArg;

    return p12dcx;

loser:
    PORT_FreeArena(arena, PR_TRUE);
    return NULL;
}

/* SEC_PKCS12DecoderUpdate 
 *	Streaming update sending more data to the decoder.  If 
 *	an error occurs, SECFailure is returned.
 *
 *	p12dcx - the decoder context 
 *	data, len - the data buffer and length of data to send to 
 *		the update functions.
 */
SECStatus
SEC_PKCS12DecoderUpdate(SEC_PKCS12DecoderContext *p12dcx,
			unsigned char *data, unsigned long len)
{
    SECStatus rv;

    if(!p12dcx || p12dcx->error) {
	return SECFailure;
    }

    /* update the PFX decoder context */
    rv = SEC_ASN1DecoderUpdate(p12dcx->pfxDcx, (const char *)data, len);
    if(rv != SECSuccess) {
	p12dcx->errorValue = SEC_ERROR_PKCS12_CORRUPT_PFX_STRUCTURE;
	goto loser;
    }

    return SECSuccess;

loser:

    p12dcx->error = PR_TRUE;
    return SECFailure;
}

/* IN_BUF_LEN should be larger than SHA1_LENGTH */
#define IN_BUF_LEN		80

/* verify the hmac by reading the data from the temporary file
 * using the routines specified when the decodingContext was 
 * created and return SECSuccess if the hmac matches.
 */
static SECStatus
sec_pkcs12_decoder_verify_mac(SEC_PKCS12DecoderContext *p12dcx)
{
    SECStatus rv = SECFailure;
    SECItem hmacRes;
    unsigned char buf[IN_BUF_LEN];
    unsigned int bufLen;
    int iteration;
    PK11Context *pk11cx = NULL;
    SECItem ignore = {0};
    PK11SymKey *symKey;
    SECItem *params;
    SECOidTag algtag;
    CK_MECHANISM_TYPE integrityMech;
    
    if(!p12dcx || p12dcx->error) {
	return SECFailure;
    }

    /* generate hmac key */
    if(p12dcx->macData.iter.data) {
	iteration = (int)DER_GetInteger(&p12dcx->macData.iter);
    } else {
	iteration = 1;
    }

    params = PK11_CreatePBEParams(&p12dcx->macData.macSalt, p12dcx->pwitem,
                                  iteration);

    algtag = SECOID_GetAlgorithmTag(&p12dcx->macData.safeMac.digestAlgorithm);
    switch (algtag) {
    case SEC_OID_SHA1:
	integrityMech = CKM_NETSCAPE_PBE_SHA1_HMAC_KEY_GEN; break;
    case SEC_OID_MD5:
	integrityMech = CKM_NETSCAPE_PBE_MD5_HMAC_KEY_GEN;  break;
    case SEC_OID_MD2:
	integrityMech = CKM_NETSCAPE_PBE_MD2_HMAC_KEY_GEN;  break;
    default:
	goto loser;
    }

    symKey = PK11_KeyGen(NULL, integrityMech, params, 20, NULL);
    PK11_DestroyPBEParams(params);
    if (!symKey) goto loser;
    /* init hmac */
    pk11cx = PK11_CreateContextBySymKey(sec_pkcs12_algtag_to_mech(algtag),
                                        CKA_SIGN, symKey, &ignore);
    if(!pk11cx) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	return SECFailure;
    }
    rv = PK11_DigestBegin(pk11cx);

    /* try to open the data for readback */
    if(p12dcx->dOpen && ((*p12dcx->dOpen)(p12dcx->dArg, PR_TRUE) 
			!= SECSuccess)) {
	goto loser;
    }

    /* read the data back IN_BUF_LEN bytes at a time and recompute
     * the hmac.  if fewer bytes are read than are requested, it is
     * assumed that the end of file has been reached. if bytesRead
     * is returned as -1, then an error occured reading from the 
     * file.
     */
    while(1) {
	int bytesRead = (*p12dcx->dRead)(p12dcx->dArg, buf, IN_BUF_LEN);
	if(bytesRead == -1) {
	    goto loser;
	}

	rv = PK11_DigestOp(pk11cx, buf, bytesRead);
	if(bytesRead < IN_BUF_LEN) {
	    break;
	}
    }

    /* finish the hmac context */
    rv = PK11_DigestFinal(pk11cx, buf, &bufLen, IN_BUF_LEN);

    hmacRes.data = buf;
    hmacRes.len = bufLen;

    /* is the hmac computed the same as the hmac which was decoded? */
    rv = SECSuccess;
    if(SECITEM_CompareItem(&hmacRes, &p12dcx->macData.safeMac.digest) 
			!= SECEqual) {
	PORT_SetError(SEC_ERROR_PKCS12_INVALID_MAC);
	rv = SECFailure;
    }

loser:
    /* close the file and remove it */
    if(p12dcx->dClose) {
	(*p12dcx->dClose)(p12dcx->dArg, PR_TRUE);
    }

    if(pk11cx) {
	PK11_DestroyContext(pk11cx, PR_TRUE);
    }

    return rv;
}

/* SEC_PKCS12DecoderVerify
 * 	Verify the macData or the signature of the decoded PKCS 12 PDU.
 *	If the signature or the macData do not match, SECFailure is
 *	returned.
 *
 * 	p12dcx - the decoder context 
 */
SECStatus
SEC_PKCS12DecoderVerify(SEC_PKCS12DecoderContext *p12dcx)
{
    SECStatus rv = SECSuccess;

    /* make sure that no errors have occured... */
    if(!p12dcx || p12dcx->error) {
	return SECFailure;
    }

    /* check the signature or the mac depending on the type of
     * integrity used.
     */
    if(p12dcx->pfx.encodedMacData.len) {
	rv = SEC_ASN1DecodeItem(p12dcx->arena, &p12dcx->macData,
				sec_PKCS12MacDataTemplate,
				&p12dcx->pfx.encodedMacData);
	if(rv == SECSuccess) {
	    return sec_pkcs12_decoder_verify_mac(p12dcx);
	} else {
	    PORT_SetError(SEC_ERROR_NO_MEMORY);
	}
    } else {
	if(SEC_PKCS7VerifySignature(p12dcx->aSafeCinfo, certUsageEmailSigner,
				    PR_FALSE)) {
	    return SECSuccess;
	} else {
	    PORT_SetError(SEC_ERROR_PKCS12_INVALID_MAC);
	}
    }

    return SECFailure;
}

/* SEC_PKCS12DecoderFinish
 *	Free any open ASN1 or PKCS7 decoder contexts and then
 *	free the arena pool which everything should be allocated
 *	from.  This function should be called upon completion of
 *	decoding and installing of a pfx pdu.  This should be
 *	called even if an error occurs.
 *
 *	p12dcx - the decoder context
 */
void
SEC_PKCS12DecoderFinish(SEC_PKCS12DecoderContext *p12dcx)
{
    if(!p12dcx) {
	return;
    }

    if(p12dcx->pfxDcx) {
	SEC_ASN1DecoderFinish(p12dcx->pfxDcx);
	p12dcx->pfxDcx = NULL;
    }

    if(p12dcx->aSafeDcx) {
	SEC_ASN1DecoderFinish(p12dcx->aSafeDcx);
	p12dcx->aSafeDcx = NULL;
    }

    if(p12dcx->currentASafeP7Dcx) {
	SEC_PKCS7DecoderFinish(p12dcx->currentASafeP7Dcx);
	p12dcx->currentASafeP7Dcx = NULL;
    }

    if(p12dcx->aSafeP7Dcx) {
	SEC_PKCS7DecoderFinish(p12dcx->aSafeP7Dcx);
    }

    if(p12dcx->hmacDcx) {
	SEC_ASN1DecoderFinish(p12dcx->hmacDcx);
	p12dcx->hmacDcx = NULL;
    }

    if(p12dcx->arena) {
	PORT_FreeArena(p12dcx->arena, PR_TRUE);
    }
}

static SECStatus
sec_pkcs12_decoder_set_attribute_value(sec_PKCS12SafeBag *bag,
			       SECOidTag attributeType,
			       SECItem *attrValue)
{
    int i = 0;
    SECOidData *oid;

    if(!bag || !attrValue) {
	return SECFailure;
    }

    oid = SECOID_FindOIDByTag(attributeType);
    if(!oid) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	return SECFailure;
    }

    if(!bag->attribs) {
	bag->attribs = (sec_PKCS12Attribute**)PORT_ArenaZAlloc(bag->arena, 
					sizeof(sec_PKCS12Attribute *) * 2);
    } else {
	while(bag->attribs[i]) i++;
	bag->attribs = (sec_PKCS12Attribute **)PORT_ArenaGrow(bag->arena, 
				      bag->attribs, 
				      (i + 1) * sizeof(sec_PKCS12Attribute *),
				      (i + 2) * sizeof(sec_PKCS12Attribute *));
    }

    if(!bag->attribs) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	return SECFailure;
    }

    bag->attribs[i] = (sec_PKCS12Attribute*)PORT_ArenaZAlloc(bag->arena, 
						  sizeof(sec_PKCS12Attribute));
    if(!bag->attribs) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	return SECFailure;
    }

    bag->attribs[i]->attrValue = (SECItem**)PORT_ArenaZAlloc(bag->arena, 
						  sizeof(SECItem *) * 2);
    if(!bag->attribs[i]->attrValue) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	return SECFailure;
    }

    bag->attribs[i+1] = NULL;
    bag->attribs[i]->attrValue[0] = attrValue;
    bag->attribs[i]->attrValue[1] = NULL;

    if(SECITEM_CopyItem(bag->arena, &bag->attribs[i]->attrType, &oid->oid)
			!= SECSuccess) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	return SECFailure;
    }

    return SECSuccess;
}

static SECItem *
sec_pkcs12_get_attribute_value(sec_PKCS12SafeBag *bag,
			       SECOidTag attributeType)
{
    int i = 0;

    if(!bag->attribs) {
	return NULL;
    }

    while(bag->attribs[i] != NULL) {
	if(SECOID_FindOIDTag(&bag->attribs[i]->attrType) 
			== attributeType) {
	    return bag->attribs[i]->attrValue[0];
	}
	i++;
    }

    return NULL;
}

/* For now, this function will merely remove any ":"
 * in the nickname which the PK11 functions may have
 * placed there.  This will keep dual certs from appearing
 * twice under "Your" certificates when imported onto smart
 * cards.  Once with the name "Slot:Cert" and another with
 * the nickname "Slot:Slot:Cert"
 */
static void
sec_pkcs12_sanitize_nickname(PK11SlotInfo *slot, SECItem *nick)
{
    char *nickname;
    char *delimit;
    int delimitlen;
 
    nickname = (char*)nick->data; /*Mac breaks without this type cast*/
    if ((delimit = PORT_Strchr(nickname, ':')) != NULL) {
        char *slotName;
	int slotNameLen;

	slotNameLen = delimit-nickname;
	slotName = PORT_NewArray(char, (slotNameLen+1));
	PORT_Assert(slotName);
	if (slotName == NULL) {
	  /* What else can we do?*/
	  return;
	}
	PORT_Memcpy(slotName, nickname, slotNameLen);
	slotName[slotNameLen] = '\0';
	if (PORT_Strcmp(PK11_GetTokenName(slot), slotName) == 0) {
	  delimitlen = PORT_Strlen(delimit+1);
	  PORT_Memmove(nickname, delimit+1, delimitlen+1);
	  nick->len = delimitlen;
	}
	PORT_Free(slotName);
    }
 
}

static SECItem *
sec_pkcs12_get_nickname(sec_PKCS12SafeBag *bag)
{
    SECItem *src, *dest;

    if(!bag) {
	bag->problem = PR_TRUE;
	bag->error = SEC_ERROR_NO_MEMORY;
	return NULL;
    }

    src = sec_pkcs12_get_attribute_value(bag, SEC_OID_PKCS9_FRIENDLY_NAME);
    if(!src) {
	return NULL;
    }

    dest = (SECItem*)PORT_ZAlloc(sizeof(SECItem));
    if(!dest) { 
	goto loser;
    }
    if(!sec_pkcs12_convert_item_to_unicode(NULL, dest, src, PR_FALSE, 
				PR_FALSE, PR_FALSE)) {
	goto loser;
    }

    sec_pkcs12_sanitize_nickname(bag->slot, dest);

    return dest;

loser:
    if(dest) {
	SECITEM_ZfreeItem(dest, PR_TRUE);
    }

    bag->problem = PR_TRUE;
    bag->error = PORT_GetError();
    return NULL;
}

static SECStatus
sec_pkcs12_set_nickname(sec_PKCS12SafeBag *bag, SECItem *name)
{
    int i = 0;
    sec_PKCS12Attribute *attr = NULL;
    SECOidData *oid = SECOID_FindOIDByTag(SEC_OID_PKCS9_FRIENDLY_NAME);

    if(!bag || !bag->arena || !name) {
	return SECFailure;
    }
	
    if(!bag->attribs) {
	if(!oid) {
	    goto loser;
	}

	bag->attribs = (sec_PKCS12Attribute**)PORT_ArenaZAlloc(bag->arena, 
					     sizeof(sec_PKCS12Attribute *)*2);
	if(!bag->attribs) {
	    goto loser;
	}
	bag->attribs[0] = (sec_PKCS12Attribute*)PORT_ArenaZAlloc(bag->arena, 
						  sizeof(sec_PKCS12Attribute));
	if(!bag->attribs[0]) {
	    goto loser;
	}
	bag->attribs[1] = NULL;

	attr = bag->attribs[0];
	if(SECITEM_CopyItem(bag->arena, &attr->attrType, &oid->oid) 
			!= SECSuccess) {
	    goto loser;
	}
    } else {
	while(bag->attribs[i]) {
	    if(SECOID_FindOIDTag(&bag->attribs[i]->attrType)
			== SEC_OID_PKCS9_FRIENDLY_NAME) {
		attr = bag->attribs[i];
		goto have_attrib;
		
	    }
	    i++;
	}
	if(!attr) {
	    bag->attribs = (sec_PKCS12Attribute **)PORT_ArenaGrow(bag->arena, 
								  bag->attribs,
					(i+1) * sizeof(sec_PKCS12Attribute *),
					(i+2) * sizeof(sec_PKCS12Attribute *));
	    if(!bag->attribs) {
		goto loser;
	    }
	    bag->attribs[i] = 
	        (sec_PKCS12Attribute *)PORT_ArenaZAlloc(bag->arena, 
						  sizeof(sec_PKCS12Attribute));
	    if(!bag->attribs[i]) {
		goto loser;
	    }
	    bag->attribs[i+1] = NULL;
	    attr = bag->attribs[i];
	    if(SECITEM_CopyItem(bag->arena, &attr->attrType, &oid->oid) 
				!= SECSuccess) {
		goto loser;
	    }
	}
    }
have_attrib:
    PORT_Assert(attr);
    if(!attr->attrValue) {
	attr->attrValue = (SECItem **)PORT_ArenaZAlloc(bag->arena, 
						       sizeof(SECItem *) * 2);
	if(!attr->attrValue) {
	    goto loser;
	}
	attr->attrValue[0] = (SECItem*)PORT_ArenaZAlloc(bag->arena, 
							sizeof(SECItem));
	if(!attr->attrValue[0]) {
	    goto loser;
	}
	attr->attrValue[1] = NULL;
    }

    name->len = PORT_Strlen((char *)name->data);
    if(!sec_pkcs12_convert_item_to_unicode(bag->arena, attr->attrValue[0],
				name, PR_FALSE, PR_FALSE, PR_TRUE)) {
	goto loser;
    }

    return SECSuccess;

loser:
    bag->problem = PR_TRUE;
    bag->error = SEC_ERROR_NO_MEMORY;
    return SECFailure;
}
	
static SECStatus
sec_pkcs12_get_key_info(sec_PKCS12SafeBag *key) 
{
    int i = 0;
    SECKEYPrivateKeyInfo *pki = NULL;

    if(!key) {
	return SECFailure;
    }

    /* if the bag does *not* contain an unencrypted PrivateKeyInfo 
     * then we cannot convert the attributes.  We are propagating
     * attributes within the PrivateKeyInfo to the SafeBag level.
     */
    if(SECOID_FindOIDTag(&(key->safeBagType)) != 
			SEC_OID_PKCS12_V1_KEY_BAG_ID) {
	return SECSuccess;
    }

    pki = key->safeBagContent.pkcs8KeyBag;

    if(!pki || !pki->attributes) {
	return SECSuccess;
    }

    while(pki->attributes[i]) {
	SECItem *attrValue = NULL;

	if(SECOID_FindOIDTag(&pki->attributes[i]->attrType) == 
			SEC_OID_PKCS9_LOCAL_KEY_ID) {
	    attrValue = sec_pkcs12_get_attribute_value(key, 
						 SEC_OID_PKCS9_LOCAL_KEY_ID);
	    if(!attrValue) {
		if(sec_pkcs12_decoder_set_attribute_value(key, 
					SEC_OID_PKCS9_LOCAL_KEY_ID,
					pki->attributes[i]->attrValue[0])
					!= SECSuccess) {
		    key->problem = PR_TRUE;
		    key->error = SEC_ERROR_NO_MEMORY;
		    return SECFailure;
		}
	    }
	}

	if(SECOID_FindOIDTag(&pki->attributes[i]->attrType) == 
			SEC_OID_PKCS9_FRIENDLY_NAME) {
	    attrValue = sec_pkcs12_get_attribute_value(key, 
						SEC_OID_PKCS9_FRIENDLY_NAME);
	    if(!attrValue) {
		if(sec_pkcs12_decoder_set_attribute_value(key, 
					SEC_OID_PKCS9_FRIENDLY_NAME,
					pki->attributes[i]->attrValue[0])
					!= SECSuccess) {
		    key->problem = PR_TRUE;
		    key->error = SEC_ERROR_NO_MEMORY;
		    return SECFailure;
		}
	    }
	}

	i++;
    }

    return SECSuccess;
}

/* retrieve the nickname for the certificate bag.  first look
 * in the cert bag, otherwise get it from the key.
 */
static SECItem *
sec_pkcs12_get_nickname_for_cert(sec_PKCS12SafeBag *cert,
				 sec_PKCS12SafeBag *key, 
				 void *wincx)
{
    SECItem *nickname;

    if(!cert) {
	return NULL;
    }

    nickname = sec_pkcs12_get_nickname(cert);
    if(nickname) {
	return nickname;
    }

    if(key) {
	nickname = sec_pkcs12_get_nickname(key);
	
        if(nickname && sec_pkcs12_set_nickname(cert, nickname)
			!= SECSuccess) {
	    cert->error = SEC_ERROR_NO_MEMORY;
	    cert->problem = PR_TRUE;
	    if(nickname) { 
		SECITEM_ZfreeItem(nickname, PR_TRUE);
	    }
	    return NULL;
	}
    }

    return nickname;
}

/* set the nickname for the certificate */
static SECStatus
sec_pkcs12_set_nickname_for_cert(sec_PKCS12SafeBag *cert, 
				 sec_PKCS12SafeBag *key, 
				 SECItem *nickname, 
				 void *wincx)
{
    if(!nickname || !cert) {
	return SECFailure;
    }

    if(sec_pkcs12_set_nickname(cert, nickname) != SECSuccess) {
	cert->error = SEC_ERROR_NO_MEMORY;
	cert->problem = PR_TRUE;
	return SECFailure;
    }

    if(key) {
	if(sec_pkcs12_set_nickname(key, nickname) != SECSuccess) {
	    cert->error = SEC_ERROR_NO_MEMORY;
	    cert->problem = PR_TRUE;
	    return SECFailure;
	}
    }

    return SECSuccess;
}

/* retrieve the DER cert from the cert bag */
static SECItem *
sec_pkcs12_get_der_cert(sec_PKCS12SafeBag *cert) 
{
    if(!cert) {
	return NULL;
    }

    if(SECOID_FindOIDTag(&cert->safeBagType) != SEC_OID_PKCS12_V1_CERT_BAG_ID) {
	return NULL;
    }

    /* only support X509 certs not SDSI */
    if(SECOID_FindOIDTag(&cert->safeBagContent.certBag->bagID) 
			!= SEC_OID_PKCS9_X509_CERT) {
	return NULL;
    }
			
    return SECITEM_DupItem(&(cert->safeBagContent.certBag->value.x509Cert));
}

struct certNickInfo {
    PRArenaPool *arena;
    unsigned int nNicks;
    SECItem **nickList;
    unsigned int error;
};

/* callback for traversing certificates to gather the nicknames 
 * used in a particular traversal.  for instance, when using
 * CERT_TraversePermCertsForSubject, gather the nicknames and
 * store them in the certNickInfo for a particular DN.
 * 
 * this handles the case where multiple nicknames are allowed
 * for the same dn, which is not currently allowed, but may be
 * in the future.
 */ 
static SECStatus
gatherNicknames(CERTCertificate *cert, void *arg)
{
    struct certNickInfo *nickArg = (struct certNickInfo *)arg;
    SECItem tempNick;
    unsigned int i;

    if(!cert || !nickArg || nickArg->error) {
	return SECFailure;
    }

    if(!cert->nickname) {
	return SECSuccess;
    }

    tempNick.data = (unsigned char *)cert->nickname;
    tempNick.len = PORT_Strlen(cert->nickname) + 1;

    /* do we already have the nickname in the list? */
    if(nickArg->nNicks > 0) {

	/* nicknames have been encountered, but there is no list -- bad */
	if(!nickArg->nickList) {
	    nickArg->error = SEC_ERROR_NO_MEMORY;
	    return SECFailure;
	}

	for(i = 0; i < nickArg->nNicks; i++) {
	    if(SECITEM_CompareItem(nickArg->nickList[i], &tempNick) 
				== SECEqual) {
		return SECSuccess;
	    }
	}
    }

    /* add the nickname to the list */
    if(nickArg->nNicks == 0) {
	nickArg->nickList = (SECItem **)PORT_ArenaZAlloc(nickArg->arena, 
					     2 * sizeof(SECItem *));
    } else {
	nickArg->nickList = (SECItem **)PORT_ArenaGrow(nickArg->arena,
				nickArg->nickList, 
				(nickArg->nNicks + 1) * sizeof(SECItem *),
				(nickArg->nNicks + 2) * sizeof(SECItem *));
    }
    if(!nickArg->nickList) {
	nickArg->error = SEC_ERROR_NO_MEMORY;
	return SECFailure;
    }

    nickArg->nickList[nickArg->nNicks] = 
        (SECItem *)PORT_ArenaZAlloc(nickArg->arena, sizeof(SECItem));
    if(!nickArg->nickList[nickArg->nNicks]) {
	nickArg->error = SEC_ERROR_NO_MEMORY;
	return SECFailure;
    }
    

    if(SECITEM_CopyItem(nickArg->arena, nickArg->nickList[nickArg->nNicks],
			&tempNick) != SECSuccess) {
	nickArg->error = SEC_ERROR_NO_MEMORY;
	return SECFailure;
    }

    nickArg->nNicks++;

    return SECSuccess;
}

/* traverses the certs in the data base or in the token for the
 * DN to see if any certs currently have a nickname set.
 * If so, return it. 
 */
static SECItem *
sec_pkcs12_get_existing_nick_for_dn(sec_PKCS12SafeBag *cert, void *wincx)
{
    struct certNickInfo *nickArg = NULL;
    SECItem *derCert, *returnDn = NULL;
    PRArenaPool *arena = NULL;
    CERTCertificate *tempCert;

    if(!cert) {
	return NULL;
    }

    derCert = sec_pkcs12_get_der_cert(cert);
    if(!derCert) {
	return NULL;
    }

    tempCert = CERT_DecodeDERCertificate(derCert, PR_FALSE, NULL);
    if(!tempCert) {
	returnDn = NULL;
	goto loser;
    }

    arena = PORT_NewArena(1024);
    if(!arena) {
	returnDn = NULL;
	goto loser;
    }
    nickArg = (struct certNickInfo *)PORT_ArenaZAlloc(arena, 
						 sizeof(struct certNickInfo));
    if(!nickArg) {
	returnDn = NULL;
	goto loser;
    }
    nickArg->error = 0;
    nickArg->nNicks = 0;
    nickArg->nickList = NULL;
    nickArg->arena = arena;

    /* if the token is local, first traverse the cert database 
     * then traverse the token.
     */
    if(PK11_TraverseCertsForSubjectInSlot(tempCert, cert->slot, gatherNicknames,
			(void *)nickArg) != SECSuccess) {
	returnDn = NULL;
	goto loser;
    }

    if(nickArg->error) {
	/* XXX do we want to set the error? */
	returnDn = NULL;
	goto loser;
    }
		
    if(nickArg->nNicks == 0) {
	returnDn = NULL;
	goto loser;
    }

    /* set it to the first name, for now.  handle multiple names? */
    returnDn = SECITEM_DupItem(nickArg->nickList[0]);

loser:
    if(arena) {
	PORT_FreeArena(arena, PR_TRUE);
    }

    if(tempCert) {
	CERT_DestroyCertificate(tempCert);
    }

    if(derCert) {
	SECITEM_FreeItem(derCert, PR_TRUE);
    }

    return (returnDn);
}

/* counts certificates found for a given traversal function */
static SECStatus
countCertificate(CERTCertificate *cert, void *arg)
{
    unsigned int *nCerts = (unsigned int *)arg;

    if(!cert || !arg) {
	return SECFailure;
    }

    (*nCerts)++;
    return SECSuccess;
}

static PRBool
sec_pkcs12_certs_for_nickname_exist(SECItem *nickname, PK11SlotInfo *slot)
{
    unsigned int nCerts = 0;

    if(!nickname || !slot) {
	return PR_TRUE;
    }

    /* we want to check the local database first if we are importing to it */
    PK11_TraverseCertsForNicknameInSlot(nickname, slot, countCertificate, 
					(void *)&nCerts);
    if(nCerts) return PR_TRUE;

    return PR_FALSE;
}

/* validate cert nickname such that there is a one-to-one relation
 * between nicknames and dn's.  we want to enforce the case that the
 * nickname is non-NULL and that there is only one nickname per DN.
 * 
 * if there is a problem with a nickname or the nickname is not present, 
 * the user will be prompted for it.
 */
static void
sec_pkcs12_validate_cert_nickname(sec_PKCS12SafeBag *cert,
				sec_PKCS12SafeBag *key,
				SEC_PKCS12NicknameCollisionCallback nicknameCb,
				void *wincx)
{
    SECItem *certNickname, *existingDNNick;
    PRBool setNickname = PR_FALSE, cancel = PR_FALSE;
    SECItem *newNickname = NULL;

    if(!cert || !cert->hasKey) {
	return;
    }

    if(!nicknameCb) {
	cert->problem = PR_TRUE;
	cert->error = SEC_ERROR_NO_MEMORY;
	return;
    }

    if(cert->hasKey && !key) {
	cert->problem = PR_TRUE;
	cert->error = SEC_ERROR_NO_MEMORY;
	return;
    }

    certNickname = sec_pkcs12_get_nickname_for_cert(cert, key, wincx);
    existingDNNick = sec_pkcs12_get_existing_nick_for_dn(cert, wincx);

    /* nickname is already used w/ this dn, so it is safe to return */
    if(certNickname && existingDNNick &&
		SECITEM_CompareItem(certNickname, existingDNNick) == SECEqual) {
	goto loser;
    }

    /* nickname not set in pkcs 12 bags, but a nick is already used for
     * this dn.  set the nicks in the p12 bags and finish.
     */
    if(existingDNNick) {
	if(sec_pkcs12_set_nickname_for_cert(cert, key, existingDNNick, wincx)
			!= SECSuccess) {
	    cert->problem = PR_TRUE;
	    cert->error = SEC_ERROR_NO_MEMORY;
	}
	goto loser;
    }

    /* at this point, we have a certificate for which the DN is not located
     * on the token.  the nickname specified may or may not be NULL.  if it
     * is not null, we need to make sure that there are no other certificates
     * with this nickname in the token for it to be valid.  this imposes a 
     * one to one relationship between DN and nickname.  
     *
     * if the nickname is null, we need the user to enter a nickname for
     * the certificate.
     *
     * once we have a nickname, we make sure that the nickname is unique
     * for the DN.  if it is not, the user is reprompted to enter a new 
     * nickname.  
     *
     * in order to exit this loop, the nickname entered is either unique
     * or the user hits cancel and the certificate is not imported.
     */
    setNickname = PR_FALSE;
    while(1) {
	if(certNickname && certNickname->data) {
	    /* we will use the nickname so long as no other certs have the
	     * same nickname.  and the nickname is not NULL.
	     */		
	    if(!sec_pkcs12_certs_for_nickname_exist(certNickname, cert->slot)) {
		if(setNickname) {
		    if(sec_pkcs12_set_nickname_for_cert(cert, key, certNickname,
					wincx) != SECSuccess) {
			cert->problem = PR_TRUE;
			cert->error = SEC_ERROR_NO_MEMORY;
		    }
		}
		goto loser;
	    }
	}

	setNickname = PR_FALSE;
	newNickname = (*nicknameCb)(certNickname, &cancel, wincx);
	if(cancel) {
	    cert->problem = PR_TRUE;
	    cert->error = SEC_ERROR_USER_CANCELLED;
	    goto loser;
	}

	if(!newNickname) {
	    cert->problem = PR_TRUE;
	    cert->error = SEC_ERROR_NO_MEMORY;
	    goto loser;
	}

	/* at this point we have a new nickname, if we have an existing
	 * certNickname, we need to free it and assign the new nickname
	 * to it to avoid a memory leak.  happy?
	 */
	if(certNickname) {
	    SECITEM_ZfreeItem(certNickname, PR_TRUE);
	    certNickname = NULL;
	}

	certNickname = newNickname;
	setNickname = PR_TRUE;
	/* go back and recheck the new nickname */
    }

loser:
    if(certNickname) {
	SECITEM_ZfreeItem(certNickname, PR_TRUE);
    }

    if(existingDNNick) {
	SECITEM_ZfreeItem(existingDNNick, PR_TRUE);
    }
}

static void 
sec_pkcs12_validate_cert(sec_PKCS12SafeBag *cert,
			 sec_PKCS12SafeBag *key,
			 SEC_PKCS12NicknameCollisionCallback nicknameCb,
			 void *wincx)
{
    CERTCertificate *leafCert, *testCert;

    if(!cert) {
	return;
    }
    
    cert->validated = PR_TRUE;

    if(!nicknameCb) {
	cert->problem = PR_TRUE;
	cert->error = SEC_ERROR_NO_MEMORY;
	cert->noInstall = PR_TRUE;
	return;
    }

    if(!cert->safeBagContent.certBag) {
	cert->noInstall = PR_TRUE;
	cert->problem = PR_TRUE;
	cert->error = SEC_ERROR_PKCS12_CORRUPT_PFX_STRUCTURE;
	return;
    }

    cert->noInstall = PR_FALSE;
    cert->removeExisting = PR_FALSE;
    cert->problem = PR_FALSE;
    cert->error = 0;

    leafCert = CERT_DecodeDERCertificate(
	 &cert->safeBagContent.certBag->value.x509Cert, PR_FALSE, NULL);
    if(!leafCert) {
	cert->noInstall = PR_TRUE;
	cert->problem = PR_TRUE;
	cert->error = SEC_ERROR_NO_MEMORY;
	return;
    }

    testCert = PK11_FindCertFromDERCert(cert->slot, leafCert, wincx);
    CERT_DestroyCertificate(leafCert);
    /* if we can't find the certificate through the PKCS11 interface,
     * we should check the cert database directly, if we are
     * importing to an internal slot.
     */
    if(!testCert && PK11_IsInternal(cert->slot)) {
	testCert = CERT_FindCertByDERCert(CERT_GetDefaultCertDB(),
				 &cert->safeBagContent.certBag->value.x509Cert);
    }

    if(testCert) {
	if(!testCert->nickname) {
	    cert->removeExisting = PR_TRUE;
	}
	CERT_DestroyCertificate(testCert);
	if(cert->noInstall && !cert->removeExisting) {
	    return;
	}
    }

    sec_pkcs12_validate_cert_nickname(cert, key, nicknameCb, wincx);
}

static void
sec_pkcs12_validate_key_by_cert(sec_PKCS12SafeBag *cert, sec_PKCS12SafeBag *key,
				void *wincx)
{
    CERTCertificate *leafCert;
    SECKEYPrivateKey *privk;

    if(!key) {
	return;
    }

    key->validated = PR_TRUE;

    if(!cert) {
	key->problem = PR_TRUE;
	key->noInstall = PR_TRUE;
	key->error = SEC_ERROR_PKCS12_UNABLE_TO_IMPORT_KEY;
	return;
    }

    leafCert = CERT_DecodeDERCertificate(
	&(cert->safeBagContent.certBag->value.x509Cert), PR_FALSE, NULL);
    if(!leafCert) {
	key->problem = PR_TRUE;
	key->noInstall = PR_TRUE;
	key->error = SEC_ERROR_NO_MEMORY;
	return;
    }

    privk = PK11_FindPrivateKeyFromCert(key->slot, leafCert, wincx);
    if(!privk) {
	privk = PK11_FindKeyByDERCert(key->slot, leafCert, wincx);
    }
 
    if(privk) {
	SECKEY_DestroyPrivateKey(privk);
	key->noInstall = PR_TRUE;
    } 

    CERT_DestroyCertificate(leafCert);
}

static SECStatus
sec_pkcs12_remove_existing_cert(sec_PKCS12SafeBag *cert, 
				void *wincx)
{
    SECItem *derCert = NULL;
    CERTCertificate *tempCert = NULL;
    CK_OBJECT_HANDLE certObj;
    PRBool removed = PR_FALSE;

    if(!cert) {
	return SECFailure;
    }

    PORT_Assert(cert->removeExisting);

    cert->removeExisting = PR_FALSE;
    derCert = &cert->safeBagContent.certBag->value.x509Cert;
    tempCert = CERT_DecodeDERCertificate(derCert, PR_FALSE, NULL);
    if(!tempCert) {
	return SECFailure;
    }

    certObj = PK11_FindCertInSlot(cert->slot, tempCert, wincx);
    CERT_DestroyCertificate(tempCert);
    tempCert = NULL;

    if(certObj != CK_INVALID_HANDLE) {
	PK11_DestroyObject(cert->slot, certObj);
	removed = PR_TRUE;
    } else if(PK11_IsInternal(cert->slot)) {
	tempCert = CERT_FindCertByDERCert(CERT_GetDefaultCertDB(), derCert);
	if(tempCert) {
	    if(SEC_DeletePermCertificate(tempCert) == SECSuccess) {
		removed = PR_TRUE;
	    } 
	    CERT_DestroyCertificate(tempCert);
	    tempCert = NULL;
	}
    }

    if(!removed) {
	cert->problem = PR_TRUE;
	cert->error = SEC_ERROR_NO_MEMORY;
	cert->noInstall = PR_TRUE;
    }
	
    if(tempCert) {
	CERT_DestroyCertificate(tempCert);
    }

    return ((removed) ? SECSuccess : SECFailure);
}

static SECStatus
sec_pkcs12_add_cert(sec_PKCS12SafeBag *cert, PRBool keyExists, void *wincx)
{
    SECItem *derCert, *nickName;
    char *nickData = NULL;
    SECStatus rv;

    if(!cert) {
	return SECFailure;
    }

    if(cert->problem || cert->noInstall || cert->installed) {
	return SECSuccess;
    }

    derCert = &cert->safeBagContent.certBag->value.x509Cert;
    if(cert->removeExisting) {
	if(sec_pkcs12_remove_existing_cert(cert, wincx) 
			!= SECSuccess) {
	    return SECFailure;
	}
	cert->removeExisting = PR_FALSE;
    }

    PORT_Assert(!cert->problem && !cert->removeExisting && !cert->noInstall);

    nickName = sec_pkcs12_get_nickname(cert);
    if(nickName) {
	nickData = (char *)nickName->data;
    }

    if(keyExists) {
	CERTCertificate *newCert;

	newCert = CERT_DecodeDERCertificate( derCert, PR_FALSE, NULL);
	if(!newCert) {
	     if(nickName) SECITEM_ZfreeItem(nickName, PR_TRUE);
	     cert->error = SEC_ERROR_NO_MEMORY;
	     cert->problem = PR_TRUE;
	     return SECFailure;
	}

	rv = PK11_ImportCertForKeyToSlot(cert->slot, newCert, nickData, 
					 PR_TRUE, wincx);
	CERT_DestroyCertificate(newCert);
    } else {
	SECItem *certList[2];
	certList[0] = derCert;
	certList[1] = NULL;
	rv = CERT_ImportCerts(CERT_GetDefaultCertDB(), certUsageUserCertImport,
			      1, certList, NULL, PR_TRUE, PR_FALSE, nickData);
    }

    cert->installed = PR_TRUE;
    if(nickName) SECITEM_ZfreeItem(nickName, PR_TRUE);
    return rv;
}

static SECStatus
sec_pkcs12_add_key(sec_PKCS12SafeBag *key, SECItem *publicValue, 
			KeyType keyType, unsigned int keyUsage, void *wincx)
{
    SECStatus rv;
    SECItem *nickName;

    if(!key) {
	return SECFailure;
    }

    if(key->removeExisting) {
	key->problem = PR_TRUE;
	key->error = SEC_ERROR_PKCS12_UNABLE_TO_IMPORT_KEY;
	return SECFailure;
    }

    if(key->problem || key->noInstall) {
	return SECSuccess;
    }

    nickName = sec_pkcs12_get_nickname(key);

    switch(SECOID_FindOIDTag(&key->safeBagType))
    {
	case SEC_OID_PKCS12_V1_KEY_BAG_ID:
	    rv = PK11_ImportPrivateKeyInfo(key->slot, 
			  key->safeBagContent.pkcs8KeyBag, 
			  nickName, publicValue, PR_TRUE, PR_TRUE,
			  keyUsage,  wincx);
	    break;
	case SEC_OID_PKCS12_V1_PKCS8_SHROUDED_KEY_BAG_ID:
	    rv = PK11_ImportEncryptedPrivateKeyInfo(key->slot,
					key->safeBagContent.pkcs8ShroudedKeyBag,
					key->pwitem, nickName, publicValue, 
					PR_TRUE, PR_TRUE, keyType, keyUsage,
					wincx);
	    break;
	default:
	    key->error = SEC_ERROR_PKCS12_UNSUPPORTED_VERSION;
	    key->problem = PR_TRUE;
	    if(nickName) {
		SECITEM_ZfreeItem(nickName, PR_TRUE);
	    }
	    return SECFailure;
    }

    key->installed = PR_TRUE;

    if(nickName) {
	SECITEM_ZfreeItem(nickName, PR_TRUE);
    }
					
    if(rv != SECSuccess) {
	key->error = SEC_ERROR_PKCS12_UNABLE_TO_IMPORT_KEY;
	key->problem = PR_TRUE;
    } else {
	key->installed = PR_TRUE;
    }

    return rv;
}

static SECStatus
sec_pkcs12_add_item_to_bag_list(sec_PKCS12SafeBag ***bagList, 
				sec_PKCS12SafeBag *bag)
{
    int i = 0;

    if(!bagList || !bag) {
	return SECFailure;
    }

    if(!(*bagList)) {
	(*bagList) = (sec_PKCS12SafeBag **)PORT_ArenaZAlloc(bag->arena, 
				      sizeof(sec_PKCS12SafeBag *) * 2);
    } else {
	while((*bagList)[i]) i++;
	(*bagList) = (sec_PKCS12SafeBag **)PORT_ArenaGrow(bag->arena, *bagList,
				sizeof(sec_PKCS12SafeBag *) * (i + 1),
				sizeof(sec_PKCS12SafeBag *) * (i + 2));
    }

    if(!(*bagList)) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	return SECFailure;
    }

    (*bagList)[i] = bag;
    (*bagList)[i+1] = NULL;

    return SECSuccess;
}

static sec_PKCS12SafeBag **
sec_pkcs12_find_certs_for_key(sec_PKCS12SafeBag **safeBags, sec_PKCS12SafeBag *key ) 
{
    sec_PKCS12SafeBag **certList = NULL;
    SECItem *keyId;
    int i;

    if(!safeBags || !safeBags[0]) {
	return NULL;
    }

    keyId = sec_pkcs12_get_attribute_value(key, SEC_OID_PKCS9_LOCAL_KEY_ID);
    if(!keyId) {
	return NULL;
    }

    i = 0;
    certList = NULL;
    while(safeBags[i]) {
	if(SECOID_FindOIDTag(&(safeBags[i]->safeBagType)) 
				== SEC_OID_PKCS12_V1_CERT_BAG_ID) {
	    SECItem *certKeyId = sec_pkcs12_get_attribute_value(safeBags[i],
						SEC_OID_PKCS9_LOCAL_KEY_ID);

	    if(certKeyId && (SECITEM_CompareItem(certKeyId, keyId) 
				== SECEqual)) {
		if(sec_pkcs12_add_item_to_bag_list(&certList, safeBags[i])
				!= SECSuccess) {
		    return NULL;
		}
	    }
	}
	i++;
    }

    return certList;
}

CERTCertList *
SEC_PKCS12DecoderGetCerts(SEC_PKCS12DecoderContext *p12dcx)
{
    CERTCertList *certList = NULL;
    sec_PKCS12SafeBag **safeBags = p12dcx->safeBags;
    int i;

    if (!p12dcx || !p12dcx->safeBags || !p12dcx->safeBags[0]) {
	return NULL;
    }

    safeBags = p12dcx->safeBags;
    i = 0;
    certList = CERT_NewCertList();

    if (certList == NULL) {
	 return NULL;
    }

    while(safeBags[i]) {
	if (SECOID_FindOIDTag(&(safeBags[i]->safeBagType)) 
				== SEC_OID_PKCS12_V1_CERT_BAG_ID) {
		SECItem *derCert = sec_pkcs12_get_der_cert(safeBags[i]) ;
		CERTCertificate *tempCert = NULL;

		if (derCert == NULL) continue;
    		tempCert=CERT_DecodeDERCertificate(derCert, PR_TRUE, NULL);

		if (tempCert) {
		    CERT_AddCertToListTail(certList,tempCert);
		}
		SECITEM_FreeItem(derCert,PR_TRUE);
	}
	i++;
    }

    return certList;
}
static sec_PKCS12SafeBag **
sec_pkcs12_get_key_bags(sec_PKCS12SafeBag **safeBags)
{
    int i;
    sec_PKCS12SafeBag **keyList = NULL;
    SECOidTag bagType;

    if(!safeBags || !safeBags[0]) {
	return NULL;
    }

    i = 0;
    while(safeBags[i]) {
	bagType = SECOID_FindOIDTag(&(safeBags[i]->safeBagType));
	switch(bagType) {
	    case SEC_OID_PKCS12_V1_KEY_BAG_ID:
	    case SEC_OID_PKCS12_V1_PKCS8_SHROUDED_KEY_BAG_ID:
		if(sec_pkcs12_add_item_to_bag_list(&keyList, safeBags[i])
				!= SECSuccess) {
		    return NULL;
		}
		break;
	    default:
		break;
	}
	i++;
    }

    return keyList;
}

static SECStatus 
sec_pkcs12_validate_bags(sec_PKCS12SafeBag **safeBags, 
			 SEC_PKCS12NicknameCollisionCallback nicknameCb,
			 void *wincx)
{
    sec_PKCS12SafeBag **keyList;
    int i;

    if(!safeBags || !nicknameCb) {
	return SECFailure;
    }

    if(!safeBags[0]) {
	return SECSuccess;
    }

    keyList = sec_pkcs12_get_key_bags(safeBags);
    if(keyList) {
	i = 0;

	while(keyList[i]) {
	    sec_PKCS12SafeBag **certList = sec_pkcs12_find_certs_for_key(
							safeBags, keyList[i]);
	    if(certList) {
		int j = 0;

		if(SECOID_FindOIDTag(&(keyList[i]->safeBagType)) == 
					SEC_OID_PKCS12_V1_KEY_BAG_ID) {
		    /* if it is an unencrypted private key then make sure
		     * the attributes are propageted to the appropriate 
		     * level 
		     */
		    if(sec_pkcs12_get_key_info(keyList[i]) != SECSuccess) {
			keyList[i]->problem = PR_TRUE;
			keyList[i]->error = SEC_ERROR_NO_MEMORY;
			return SECFailure;
		     }
		}
	
		sec_pkcs12_validate_key_by_cert(certList[0], keyList[i], wincx);
		while(certList[j]) {
		    certList[j]->hasKey = PR_TRUE;
		    if(keyList[i]->problem) {
			certList[j]->problem = PR_TRUE;
			certList[j]->error = keyList[i]->error;
		    } else {
			sec_pkcs12_validate_cert(certList[j], keyList[i],
						 nicknameCb, wincx);
			if(certList[j]->problem) {
			    keyList[i]->problem = certList[j]->problem;
			    keyList[i]->error = certList[j]->error;
			}
		    }
		    j++;
		}
	    }

	    i++;
	}
    }

    i = 0;
    while(safeBags[i]) {
	if(!safeBags[i]->validated) {
	    SECOidTag bagType = SECOID_FindOIDTag(&safeBags[i]->safeBagType);

	    switch(bagType) {
		case SEC_OID_PKCS12_V1_CERT_BAG_ID:
		    sec_pkcs12_validate_cert(safeBags[i], NULL, nicknameCb, 
					     wincx);
		    break;
		case SEC_OID_PKCS12_V1_KEY_BAG_ID:
		case SEC_OID_PKCS12_V1_PKCS8_SHROUDED_KEY_BAG_ID:
		    safeBags[i]->noInstall = PR_TRUE;
		    safeBags[i]->problem = PR_TRUE;	
		    safeBags[i]->error = SEC_ERROR_PKCS12_UNABLE_TO_IMPORT_KEY;
		    break;
		default:
		    safeBags[i]->noInstall = PR_TRUE;
	    }
	}
	i++;
    }

    return SECSuccess;
}

SECStatus
SEC_PKCS12DecoderValidateBags(SEC_PKCS12DecoderContext *p12dcx,
			      SEC_PKCS12NicknameCollisionCallback nicknameCb)
{
    SECStatus rv;
    int i, noInstallCnt, probCnt, bagCnt, errorVal = 0;
    if(!p12dcx || p12dcx->error) {
	return SECFailure;
    }

    rv = sec_pkcs12_validate_bags(p12dcx->safeBags, nicknameCb, p12dcx->wincx);
    if(rv == SECSuccess) {
	p12dcx->bagsVerified = PR_TRUE;
    }

    noInstallCnt = probCnt = bagCnt = 0;
    i = 0;
    while(p12dcx->safeBags[i]) {
	bagCnt++;
	if(p12dcx->safeBags[i]->noInstall) noInstallCnt++;
	if(p12dcx->safeBags[i]->problem) {
	    probCnt++;
	    errorVal = p12dcx->safeBags[i]->error;
	}
	i++;
    }

    if(bagCnt == noInstallCnt) {
	PORT_SetError(SEC_ERROR_PKCS12_DUPLICATE_DATA);
	return SECFailure;
    }

    if(probCnt) {
	PORT_SetError(errorVal);
	return SECFailure;
    }

    return rv;
}

static SECItem *
sec_pkcs12_get_public_value_and_type(sec_PKCS12SafeBag *certBag,
					KeyType *type, unsigned int *usage)
{
    SECKEYPublicKey *pubKey = NULL;
    CERTCertificate *cert = NULL;
    SECItem *pubValue;

    *type = nullKey;
    *usage = 0;

    if(!certBag) {
	return NULL;
    }

    cert = CERT_DecodeDERCertificate(
	 &certBag->safeBagContent.certBag->value.x509Cert, PR_FALSE, NULL);
    if(!cert) {
	return NULL;
    }

    *usage = cert->keyUsage;
    pubKey = CERT_ExtractPublicKey(cert);
    CERT_DestroyCertificate(cert);
    if(!pubKey) {
	return NULL;
    }

    *type = pubKey->keyType;
    switch(pubKey->keyType) {
	case dsaKey:
	    pubValue = SECITEM_DupItem(&pubKey->u.dsa.publicValue);
	    break;
	case dhKey:
	    pubValue = SECITEM_DupItem(&pubKey->u.dh.publicValue);
	    break;
	case rsaKey:
	    pubValue = SECITEM_DupItem(&pubKey->u.rsa.modulus);
	    break;
	default:
	    pubValue = NULL;
    }

    SECKEY_DestroyPublicKey(pubKey);

    return pubValue;
}

static SECStatus 
sec_pkcs12_install_bags(sec_PKCS12SafeBag **safeBags, 
			void *wincx)
{
    sec_PKCS12SafeBag **keyList, **certList;
    int i;

    if(!safeBags) {
	return SECFailure;
    }

    if(!safeBags[0]) {
	return SECSuccess;
    }

    keyList = sec_pkcs12_get_key_bags(safeBags);
    if(keyList) {
	i = 0;

	while(keyList[i]) {
	    SECStatus rv;
	    SECItem *publicValue = NULL;
	    KeyType keyType;
	    unsigned int keyUsage;

	    if(keyList[i]->problem) {
		goto next_key_bag;
	    }

	    certList = sec_pkcs12_find_certs_for_key(safeBags,
						     keyList[i]);
	    if(certList) {
		publicValue = sec_pkcs12_get_public_value_and_type(certList[0],
							&keyType, &keyUsage);
	    }
	    rv = sec_pkcs12_add_key(keyList[i], publicValue, keyType, keyUsage,
									wincx);
	    if(publicValue) {
		SECITEM_FreeItem(publicValue, PR_TRUE);
	    }
	    if(rv != SECSuccess) {
		PORT_SetError(keyList[i]->error);
		return SECFailure;
	    }

	    if(certList) {
		int j = 0;

		while(certList[j]) {
		    SECStatus certRv;

		    if(rv != SECSuccess) {
			certList[j]->problem = keyList[i]->problem;
			certList[j]->error = keyList[i]->error;	
			certList[j]->noInstall = PR_TRUE;
			goto next_cert_bag;
		    }

		    certRv = sec_pkcs12_add_cert(certList[j], 
						 certList[j]->hasKey, wincx);
		    if(certRv != SECSuccess) {
			keyList[i]->problem = certList[j]->problem;
			keyList[i]->error = certList[j]->error;
			PORT_SetError(certList[j]->error);
			return SECFailure;
		    }
next_cert_bag:
		    j++;
		}
	    }

next_key_bag:
	    i++;
	}
    }

    i = 0;
    while(safeBags[i]) {
	if(!safeBags[i]->installed) {
	    SECStatus rv;
	    SECOidTag bagType = SECOID_FindOIDTag(&(safeBags[i]->safeBagType));

	    switch(bagType) {
		case SEC_OID_PKCS12_V1_CERT_BAG_ID:
		    rv = sec_pkcs12_add_cert(safeBags[i], safeBags[i]->hasKey,
					     wincx);
		    if(rv != SECSuccess) {
			PORT_SetError(safeBags[i]->error);
			return SECFailure;
		    }
		    break;
		case SEC_OID_PKCS12_V1_KEY_BAG_ID:
		case SEC_OID_PKCS12_V1_PKCS8_SHROUDED_KEY_BAG_ID:
		default:
		    break;
	    }
	}
	i++;
    }

    return SECSuccess;
}

SECStatus
SEC_PKCS12DecoderImportBags(SEC_PKCS12DecoderContext *p12dcx)
{
    if(!p12dcx || p12dcx->error) {
	return SECFailure;
    }

    if(!p12dcx->bagsVerified) {
	return SECFailure;
    }

    return sec_pkcs12_install_bags(p12dcx->safeBags, p12dcx->wincx);
}

static SECStatus
sec_pkcs12_decoder_append_bag_to_context(SEC_PKCS12DecoderContext *p12dcx,
					 sec_PKCS12SafeBag *bag)
{
    if(!p12dcx || p12dcx->error) {
	return SECFailure;
    }

    if(!p12dcx->safeBagCount) {
	p12dcx->safeBags = (sec_PKCS12SafeBag **)PORT_ArenaZAlloc(p12dcx->arena, 
					    sizeof(sec_PKCS12SafeBag *) * 2);
    } else {
	p12dcx->safeBags = 
	  (sec_PKCS12SafeBag **)PORT_ArenaGrow(p12dcx->arena, p12dcx->safeBags,
		     (p12dcx->safeBagCount + 1) * sizeof(sec_PKCS12SafeBag *),
		     (p12dcx->safeBagCount + 2) * sizeof(sec_PKCS12SafeBag *));
    }

    if(!p12dcx->safeBags) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	return SECFailure;
    }

    p12dcx->safeBags[p12dcx->safeBagCount] = bag;
    p12dcx->safeBags[p12dcx->safeBagCount+1] = NULL;
    p12dcx->safeBagCount++;

    return SECSuccess;
}

static sec_PKCS12SafeBag *
sec_pkcs12_decoder_convert_old_key(SEC_PKCS12DecoderContext *p12dcx,
				   void *key, PRBool isEspvk)
{
    sec_PKCS12SafeBag *keyBag;
    SECOidData *oid;
    SECOidTag keyTag;
    SECItem *keyID, *nickName, *newNickName;

    if(!p12dcx || p12dcx->error || !key) {
	return NULL;
    }

    newNickName =(SECItem *)PORT_ArenaZAlloc(p12dcx->arena, sizeof(SECItem));
    keyBag = (sec_PKCS12SafeBag *)PORT_ArenaZAlloc(p12dcx->arena, 
						   sizeof(sec_PKCS12SafeBag));
    if(!keyBag || !newNickName) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	return NULL;
    }

    keyBag->swapUnicodeBytes = p12dcx->swapUnicodeBytes;
    keyBag->slot = p12dcx->slot;
    keyBag->arena = p12dcx->arena;
    keyBag->pwitem = p12dcx->pwitem;
    keyBag->oldBagType = PR_TRUE;

    keyTag = (isEspvk) ? SEC_OID_PKCS12_V1_PKCS8_SHROUDED_KEY_BAG_ID :
			 SEC_OID_PKCS12_V1_KEY_BAG_ID;
    oid = SECOID_FindOIDByTag(keyTag);
    if(!oid) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	return NULL;
    }

    if(SECITEM_CopyItem(p12dcx->arena, &keyBag->safeBagType, &oid->oid) 
			!= SECSuccess) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	return NULL;
    }

    if(isEspvk) {
	SEC_PKCS12ESPVKItem *espvk = (SEC_PKCS12ESPVKItem *)key;
	keyBag->safeBagContent.pkcs8ShroudedKeyBag  = 
					espvk->espvkCipherText.pkcs8KeyShroud;
	nickName = &(espvk->espvkData.uniNickName); 
	if(!espvk->espvkData.assocCerts || !espvk->espvkData.assocCerts[0]) {
	    PORT_SetError(SEC_ERROR_PKCS12_CORRUPT_PFX_STRUCTURE);
	    return NULL;
	}
	keyID = &espvk->espvkData.assocCerts[0]->digest;
    } else {
	SEC_PKCS12PrivateKey *pk = (SEC_PKCS12PrivateKey *)key;
	keyBag->safeBagContent.pkcs8KeyBag = &pk->pkcs8data;
	nickName= &(pk->pvkData.uniNickName);
	if(!pk->pvkData.assocCerts || !pk->pvkData.assocCerts[0]) {
	    PORT_SetError(SEC_ERROR_PKCS12_CORRUPT_PFX_STRUCTURE);
	    return NULL;
	}
	keyID = &pk->pvkData.assocCerts[0]->digest;
    }

    if(nickName->len) {
	if(nickName->len >= 2) {
	    if(nickName->data[0] && nickName->data[1]) {
		if(!sec_pkcs12_convert_item_to_unicode(p12dcx->arena, newNickName, 
					nickName, PR_FALSE, PR_FALSE, PR_TRUE)) {
		    PORT_SetError(SEC_ERROR_NO_MEMORY);
		    return NULL;
		}
		nickName = newNickName;
	    } else if(nickName->data[0] && !nickName->data[1]) {
		unsigned int j = 0;
		unsigned char t;
		for(j = 0; j < nickName->len; j+=2) {
		    t = nickName->data[j+1];
		    nickName->data[j+1] = nickName->data[j];
		    nickName->data[j] = t;
		}
	    }
	} else {
	    if(!sec_pkcs12_convert_item_to_unicode(p12dcx->arena, newNickName, 
					nickName, PR_FALSE, PR_FALSE, PR_TRUE)) {
		PORT_SetError(SEC_ERROR_NO_MEMORY);
		return NULL;
	    }
	    nickName = newNickName;
	}
    }

    if(sec_pkcs12_decoder_set_attribute_value(keyBag,
					      SEC_OID_PKCS9_FRIENDLY_NAME,
					      nickName) != SECSuccess) {
	return NULL;
    }
	
    if(sec_pkcs12_decoder_set_attribute_value(keyBag,SEC_OID_PKCS9_LOCAL_KEY_ID,
					keyID) != SECSuccess) {
	return NULL;
    }

    return keyBag;
}

static sec_PKCS12SafeBag *
sec_pkcs12_decoder_create_cert(SEC_PKCS12DecoderContext *p12dcx,
			       SECItem *derCert)
{
    sec_PKCS12SafeBag *certBag;
    SECOidData *oid;
    SGNDigestInfo *digest;
    SECItem *keyId;
    SECStatus rv;

    if(!p12dcx || p12dcx->error || !derCert) {
	return NULL;
    }

    keyId = (SECItem *)PORT_ArenaZAlloc(p12dcx->arena, sizeof(SECItem));
    if(!keyId) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	return NULL;
    }

    digest = sec_pkcs12_compute_thumbprint(derCert);
    if(!digest) {
	return NULL;
    }

    rv = SECITEM_CopyItem(p12dcx->arena, keyId, &digest->digest);
    SGN_DestroyDigestInfo(digest);
    if(rv != SECSuccess) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	return NULL;
    }

    oid = SECOID_FindOIDByTag(SEC_OID_PKCS12_V1_CERT_BAG_ID);
    certBag = (sec_PKCS12SafeBag *)PORT_ArenaZAlloc(p12dcx->arena, 
						    sizeof(sec_PKCS12SafeBag));
    if(!certBag || !oid || (SECITEM_CopyItem(p12dcx->arena, 
			&certBag->safeBagType, &oid->oid) != SECSuccess)) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	return NULL;
    }

    certBag->slot = p12dcx->slot;
    certBag->pwitem = p12dcx->pwitem;
    certBag->swapUnicodeBytes = p12dcx->swapUnicodeBytes;
    certBag->arena = p12dcx->arena;

    oid = SECOID_FindOIDByTag(SEC_OID_PKCS9_X509_CERT);
    certBag->safeBagContent.certBag = 
        (sec_PKCS12CertBag *)PORT_ArenaZAlloc(p12dcx->arena, 
					      sizeof(sec_PKCS12CertBag));
    if(!certBag->safeBagContent.certBag || !oid ||
			(SECITEM_CopyItem(p12dcx->arena, 
				 &certBag->safeBagContent.certBag->bagID,
				 &oid->oid) != SECSuccess)) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	return NULL;
    }
      
    if(SECITEM_CopyItem(p12dcx->arena, 
			 &(certBag->safeBagContent.certBag->value.x509Cert),
			 derCert) != SECSuccess) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	return NULL;
    }

    if(sec_pkcs12_decoder_set_attribute_value(certBag, SEC_OID_PKCS9_LOCAL_KEY_ID,
					keyId) != SECSuccess) {
	return NULL;
    }

    return certBag;
}

static sec_PKCS12SafeBag **
sec_pkcs12_decoder_convert_old_cert(SEC_PKCS12DecoderContext *p12dcx,
				    SEC_PKCS12CertAndCRL *oldCert)
{
    sec_PKCS12SafeBag **certList;
    SECItem **derCertList;
    int i, j;

    if(!p12dcx || p12dcx->error || !oldCert) {
	return NULL;
    }

    derCertList = SEC_PKCS7GetCertificateList(&oldCert->value.x509->certOrCRL);
    if(!derCertList) {
	return NULL;
    }

    i = 0;
    while(derCertList[i]) i++;

    certList = (sec_PKCS12SafeBag **)PORT_ArenaZAlloc(p12dcx->arena, 
				(i + 1) * sizeof(sec_PKCS12SafeBag *));
    if(!certList) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	return NULL;
    }

    for(j = 0; j < i; j++) {
	certList[j] = sec_pkcs12_decoder_create_cert(p12dcx, derCertList[j]);
	if(!certList[j]) {
	    return NULL;
	}
    }

    return certList;
}   

static SECStatus
sec_pkcs12_decoder_convert_old_key_and_certs(SEC_PKCS12DecoderContext *p12dcx,
					     void *oldKey, PRBool isEspvk,
					     SEC_PKCS12SafeContents *safe,
					     SEC_PKCS12Baggage *baggage)
{
    sec_PKCS12SafeBag *key, **certList;
    SEC_PKCS12CertAndCRL *oldCert;
    SEC_PKCS12PVKSupportingData *pvkData;
    int i;
    SECItem *keyName;

    if(!p12dcx || !oldKey) {
	return SECFailure;
    }

    if(isEspvk) {
	pvkData = &((SEC_PKCS12ESPVKItem *)(oldKey))->espvkData;
    } else {
	pvkData = &((SEC_PKCS12PrivateKey *)(oldKey))->pvkData;
    }

    if(!pvkData->assocCerts || !pvkData->assocCerts[0]) {
	PORT_SetError(SEC_ERROR_PKCS12_CORRUPT_PFX_STRUCTURE);
	return SECFailure;
    }

    oldCert = (SEC_PKCS12CertAndCRL *)sec_pkcs12_find_object(safe, baggage, 
				     SEC_OID_PKCS12_CERT_AND_CRL_BAG_ID, NULL,
				     pvkData->assocCerts[0]);
    if(!oldCert) {
	PORT_SetError(SEC_ERROR_PKCS12_CORRUPT_PFX_STRUCTURE);
	return SECFailure;
    }
	
    key = sec_pkcs12_decoder_convert_old_key(p12dcx,oldKey, isEspvk);
    certList = sec_pkcs12_decoder_convert_old_cert(p12dcx, oldCert);
    if(!key || !certList) {
	return SECFailure;
    }

    if(sec_pkcs12_decoder_append_bag_to_context(p12dcx, key) != SECSuccess) {
	return SECFailure;
    }

    keyName = sec_pkcs12_get_nickname(key);
    if(!keyName) {
	return SECFailure;
    }

    i = 0;
    while(certList[i]) {
	if(sec_pkcs12_decoder_append_bag_to_context(p12dcx, certList[i]) 
				!= SECSuccess) {
	    return SECFailure;
	}
	i++;
    }

    certList = sec_pkcs12_find_certs_for_key(p12dcx->safeBags, key);
    if(!certList) {
	return SECFailure;
    }

    i = 0;
    while(certList[i] != 0) {
	if(sec_pkcs12_set_nickname(certList[i], keyName) != SECSuccess) {
	    return SECFailure;
	}
	i++;
    }
				
    return SECSuccess;
}
	
static SECStatus
sec_pkcs12_decoder_convert_old_safe_to_bags(SEC_PKCS12DecoderContext *p12dcx,
					    SEC_PKCS12SafeContents *safe,
					    SEC_PKCS12Baggage *baggage)
{
    SECStatus rv;

    if(!p12dcx || p12dcx->error) {
	return SECFailure;
    }

    if(safe && safe->contents) {
	int i = 0;
	while(safe->contents[i] != NULL) {
	    if(SECOID_FindOIDTag(&safe->contents[i]->safeBagType) 
				== SEC_OID_PKCS12_KEY_BAG_ID) {
		int j = 0;
		SEC_PKCS12PrivateKeyBag *privBag = 
					safe->contents[i]->safeContent.keyBag;
		
		while(privBag->privateKeys[j] != NULL) {
		    SEC_PKCS12PrivateKey *pk = privBag->privateKeys[j];
		    rv = sec_pkcs12_decoder_convert_old_key_and_certs(p12dcx,pk,
						PR_FALSE, safe, baggage);
		    if(rv != SECSuccess) {
			goto loser;
		    }
		    j++;
		}
	    }
	    i++;
	}
    }

    if(baggage && baggage->bags) {
	int i = 0;
	while(baggage->bags[i] != NULL) {
	    SEC_PKCS12BaggageItem *bag = baggage->bags[i];
	    int j = 0;

	    if(!bag->espvks) {
		i++;
		continue;
	    }

	    while(bag->espvks[j] != NULL) {
		SEC_PKCS12ESPVKItem *espvk = bag->espvks[j];
		rv = sec_pkcs12_decoder_convert_old_key_and_certs(p12dcx, espvk,
							PR_TRUE, safe, baggage);
		if(rv != SECSuccess) {
		    goto loser;
		}
		j++;
	    }
	    i++;
	}
    }

    return SECSuccess;

loser:
    return SECFailure;
}

SEC_PKCS12DecoderContext *
sec_PKCS12ConvertOldSafeToNew(PRArenaPool *arena, PK11SlotInfo *slot, 
			      PRBool swapUnicode, SECItem *pwitem,
			      void *wincx, SEC_PKCS12SafeContents *safe,
			      SEC_PKCS12Baggage *baggage)
{
    SEC_PKCS12DecoderContext *p12dcx;

    if(!arena || !slot || !pwitem) {
	return NULL;
    }

    if(!safe && !baggage) {
	return NULL;
    }

    p12dcx = (SEC_PKCS12DecoderContext *)PORT_ArenaZAlloc(arena, 
					    sizeof(SEC_PKCS12DecoderContext));
    if(!p12dcx) {
	return NULL;
    }

    p12dcx->arena = arena;
    p12dcx->slot = slot;
    p12dcx->wincx = wincx;
    p12dcx->error = PR_FALSE;
    p12dcx->swapUnicodeBytes = swapUnicode; 
    p12dcx->pwitem = pwitem;
    
    if(sec_pkcs12_decoder_convert_old_safe_to_bags(p12dcx, safe, baggage) 
				!= SECSuccess) {
	p12dcx->error = PR_TRUE;
	return NULL;
    }

    return p12dcx;
}
