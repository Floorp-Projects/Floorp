/* -*- Mode: C; tab-width: 8 -*-*/
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

#include "crmf.h"
#include "crmfi.h"
#include "keyhi.h"
#include "secder.h"

/*
 * Macro that returns PR_TRUE if the pointer is not NULL.
 * If the pointer is NULL, then the macro will return PR_FALSE.
 */
#define IS_NOT_NULL(ptr) ((ptr) == NULL) ? PR_FALSE : PR_TRUE

const unsigned char hexTrue  = 0xff;
const unsigned char hexFalse = 0x00;


SECStatus
crmf_encode_integer(PRArenaPool *poolp, SECItem *dest, long value) 
{
    SECItem *dummy;

    dummy = SEC_ASN1EncodeInteger(poolp, dest, value);
    PORT_Assert (dummy == dest);
    if (dummy == NULL) {
        return SECFailure;
    }
    return SECSuccess;
}

static SECStatus
crmf_copy_secitem (PRArenaPool *poolp, SECItem *dest, SECItem *src)
{
    return  SECITEM_CopyItem (poolp, dest, src); 
}

PRBool
CRMF_DoesRequestHaveField (CRMFCertRequest       *inCertReq, 
			   CRMFCertTemplateField  inField)
{
  
    PORT_Assert(inCertReq != NULL);
    if (inCertReq == NULL) {
        return PR_FALSE;
    }
    switch (inField) {
    case crmfVersion:
        return inCertReq->certTemplate.version.data != NULL;
    case crmfSerialNumber:
        return inCertReq->certTemplate.serialNumber.data != NULL;
    case crmfSigningAlg:
        return inCertReq->certTemplate.signingAlg != NULL;
    case crmfIssuer:
        return inCertReq->certTemplate.issuer != NULL;
    case crmfValidity:
        return inCertReq->certTemplate.validity != NULL;
    case crmfSubject:
        return inCertReq->certTemplate.subject != NULL;
    case crmfPublicKey:
        return inCertReq->certTemplate.publicKey != NULL;
    case crmfIssuerUID:
        return inCertReq->certTemplate.issuerUID.data != NULL;
    case crmfSubjectUID:
        return inCertReq->certTemplate.subjectUID.data != NULL;
    case crmfExtension:
        return CRMF_CertRequestGetNumberOfExtensions(inCertReq) != 0;
    }
    return PR_FALSE;
}

CRMFCertRequest *
CRMF_CreateCertRequest (long inRequestID) {
    PRArenaPool     *poolp;
    CRMFCertRequest *certReq;
    SECStatus        rv;
    
    poolp = PORT_NewArena(CRMF_DEFAULT_ARENA_SIZE);
    if (poolp == NULL) {
        goto loser;
    }
    
    certReq=PORT_ArenaZNew(poolp,CRMFCertRequest);
    if (certReq == NULL) {
        goto loser;
    }

    certReq->poolp = poolp;
    certReq->requestID = inRequestID;
    
    rv = crmf_encode_integer(poolp, &(certReq->certReqId), inRequestID);
    if (rv != SECSuccess) {
        goto loser;
    }

    return certReq;
 loser:
    if (poolp) {
        PORT_FreeArena(poolp, PR_FALSE);
    }
    return NULL;
}

SECStatus
CRMF_DestroyCertRequest(CRMFCertRequest *inCertReq)
{
    PORT_Assert(inCertReq != NULL);
    if (inCertReq != NULL) {
        if (inCertReq->certTemplate.extensions) {
	    PORT_Free(inCertReq->certTemplate.extensions);
	}
	if (inCertReq->controls) {
	    /* Right now we don't support EnveloppedData option,
	     * so we won't go through and delete each occurrence of 
	     * an EnveloppedData in the control.
	     */
	    PORT_Free(inCertReq->controls);
	}
	if (inCertReq->poolp) {
	    PORT_FreeArena(inCertReq->poolp, PR_TRUE);
	}
    }
    return SECSuccess;
}

static SECStatus
crmf_template_add_version(PRArenaPool *poolp, SECItem *dest, long version)
{
    return (crmf_encode_integer(poolp, dest, version));
}

static SECStatus
crmf_template_add_serialnumber(PRArenaPool *poolp, SECItem *dest, long serial)
{
    return (crmf_encode_integer(poolp, dest, serial));
}

SECStatus
crmf_template_copy_secalg (PRArenaPool *poolp, SECAlgorithmID **dest, 
			   SECAlgorithmID* src)
{
    SECStatus         rv;
    void             *mark = NULL;
    SECAlgorithmID   *mySecAlg;

    if (poolp != NULL) {
        mark = PORT_ArenaMark(poolp);
    }
    *dest = mySecAlg = PORT_ArenaZNew(poolp, SECAlgorithmID);
    if (mySecAlg == NULL) {
        goto loser;
    }
    rv = SECOID_CopyAlgorithmID(poolp, mySecAlg, src);
    if (rv != SECSuccess) {
        goto loser;
    }
    if (mark) {
        PORT_ArenaUnmark(poolp, mark);
    }
    return SECSuccess;

 loser:
    *dest = NULL;
    if (mark) {
        PORT_ArenaRelease(poolp, mark);
    }
    return SECFailure;
}

SECStatus
crmf_copy_cert_name(PRArenaPool *poolp, CERTName **dest, 
		    CERTName *src)
{
    CERTName *newName;
    SECStatus rv;
    void     *mark;

    mark = PORT_ArenaMark(poolp);
    *dest = newName = PORT_ArenaZNew(poolp, CERTName);
    if (newName == NULL) {
        goto loser;
    }

    rv = CERT_CopyName(poolp, newName, src);
    if (rv != SECSuccess) {
      goto loser;
    }
    PORT_ArenaUnmark(poolp, mark);
    return SECSuccess;
 loser:
    PORT_ArenaRelease(poolp, mark);
    *dest = NULL;
    return SECFailure;
}

static SECStatus
crmf_template_add_issuer (PRArenaPool *poolp, CERTName **dest, 
			  CERTName* issuerName)
{
    return crmf_copy_cert_name(poolp, dest, issuerName);
}


static SECStatus
crmf_encode_utctime(PRArenaPool *poolp, SECItem *destTime, PRTime time)
{
    SECItem   tmpItem;
    SECStatus rv;


    rv = DER_TimeToUTCTime (&tmpItem, time);
    if (rv != SECSuccess) {
        return rv;
    }
    rv = SECITEM_CopyItem(poolp, destTime, &tmpItem);
    PORT_Free(tmpItem.data);
    return rv;
}

static SECStatus
crmf_template_add_validity (PRArenaPool *poolp, CRMFOptionalValidity **dest,
			    CRMFValidityCreationInfo *info)
{
    SECStatus             rv;
    void                 *mark; 
    CRMFOptionalValidity *myValidity;

    /*First off, let's make sure at least one of the two fields is present*/
    if (!info  || (!info->notBefore && !info->notAfter)) {
        return SECFailure;
    }
    mark = PORT_ArenaMark (poolp);
    *dest = myValidity = PORT_ArenaZNew(poolp, CRMFOptionalValidity);
    if (myValidity == NULL) {
        goto loser;
    }

    if (info->notBefore) {
        rv = crmf_encode_utctime (poolp, &myValidity->notBefore, 
				  *info->notBefore);
	if (rv != SECSuccess) {
	    goto loser;
	}
    }
    if (info->notAfter) {
        rv = crmf_encode_utctime (poolp, &myValidity->notAfter,
				  *info->notAfter);
	if (rv != SECSuccess) {
	    goto loser;
	}
    }
    PORT_ArenaUnmark(poolp, mark);
    return SECSuccess;
 loser:
    PORT_ArenaRelease(poolp, mark);
    *dest = NULL;
    return SECFailure;
}

static SECStatus
crmf_template_add_subject (PRArenaPool *poolp, CERTName **dest,
			   CERTName *subject)
{
    return crmf_copy_cert_name(poolp, dest, subject);
}

SECStatus
crmf_template_add_public_key(PRArenaPool *poolp, 
			     CERTSubjectPublicKeyInfo **dest,
			     CERTSubjectPublicKeyInfo  *pubKey)
{
    CERTSubjectPublicKeyInfo *spki;
    SECStatus rv;

    *dest = spki = (poolp == NULL) ?
                              PORT_ZNew(CERTSubjectPublicKeyInfo) :
                              PORT_ArenaZNew (poolp, CERTSubjectPublicKeyInfo);
    if (spki == NULL) {
        goto loser;
    }
    rv = SECKEY_CopySubjectPublicKeyInfo (poolp, spki, pubKey);
    if (rv != SECSuccess) {
        goto loser;
    }
    return SECSuccess;
 loser:
    if (poolp == NULL && spki != NULL) {
        SECKEY_DestroySubjectPublicKeyInfo(spki);
    }
    *dest = NULL;
    return SECFailure;
}

static SECStatus
crmf_copy_bitstring (PRArenaPool *poolp, SECItem *dest, SECItem *src)
{
    SECStatus rv;
    int origLenBits, numBytesToCopy;
    
    origLenBits = src->len;
    numBytesToCopy = CRMF_BITS_TO_BYTES(origLenBits);
    rv = crmf_copy_secitem(poolp, dest, src);
    src->len = origLenBits;
    dest->len = origLenBits;
    return rv;
}

static SECStatus
crmf_template_add_issuer_uid(PRArenaPool *poolp, SECItem *dest,
			     SECItem *issuerUID)
{
    return crmf_copy_bitstring (poolp, dest, issuerUID);
}

static SECStatus
crmf_template_add_subject_uid(PRArenaPool *poolp, SECItem *dest, 
			      SECItem *subjectUID)
{
    return crmf_copy_bitstring (poolp, dest, subjectUID);
}

static void
crmf_zeroize_new_extensions (CRMFCertExtension **extensions,
			     int numToZeroize) 
{
    PORT_Memset((void*)extensions, 0, sizeof(CERTCertExtension*)*numToZeroize);
}

/*
 * The strategy for adding templates will differ from all the other
 * attributes in the template.  First, we want to allow the client
 * of this API to set extensions more than just once.  So we will
 * need the ability grow the array of extensions.  Since arenas don't
 * give us the realloc function, we'll use the generic PORT_* functions
 * to allocate the array of pointers *ONLY*.  Then we will allocate each
 * individual extension from the arena that comes along with the certReq
 * structure that owns this template.
 */
static SECStatus
crmf_template_add_extensions(PRArenaPool *poolp, CRMFCertTemplate *inTemplate,
			     CRMFCertExtCreationInfo *extensions)
{
    void               *mark;
    int                 newSize, oldSize, i;
    SECStatus           rv;
    CRMFCertExtension **extArray;
    CRMFCertExtension  *newExt, *currExt;

    mark = PORT_ArenaMark(poolp);
    if (inTemplate->extensions == NULL) {
        newSize = extensions->numExtensions;
        extArray = PORT_ZNewArray(CRMFCertExtension*,newSize+1);
    } else {
        newSize = inTemplate->numExtensions + extensions->numExtensions;
        extArray = PORT_Realloc(inTemplate->extensions, 
				sizeof(CRMFCertExtension*)*(newSize+1));
    }
    if (extArray == NULL) {
        goto loser;
    }
    oldSize                   = inTemplate->numExtensions;
    inTemplate->extensions    = extArray;
    inTemplate->numExtensions = newSize;
    for (i=oldSize; i < newSize; i++) {
        newExt = PORT_ArenaZNew(poolp, CRMFCertExtension);
	if (newExt == NULL) {
	    goto loser2;
	}
	currExt = extensions->extensions[i-oldSize];
	rv = crmf_copy_secitem(poolp, &(newExt->id), &(currExt->id));
	if (rv != SECSuccess) {
	    goto loser2;
	}
	rv = crmf_copy_secitem(poolp, &(newExt->critical),
			       &(currExt->critical));
	if (rv != SECSuccess) {
	    goto loser2;
	}
	rv = crmf_copy_secitem(poolp, &(newExt->value), &(currExt->value));
	if (rv != SECSuccess) {
	    goto loser2;
	}
	extArray[i] = newExt;
    }
    extArray[newSize] = NULL;
    PORT_ArenaUnmark(poolp, mark);
    return SECSuccess;
 loser2:
    crmf_zeroize_new_extensions (&(inTemplate->extensions[oldSize]),
				 extensions->numExtensions);
    inTemplate->numExtensions = oldSize;
 loser:
    PORT_ArenaRelease(poolp, mark);
    return SECFailure;
}

SECStatus
CRMF_CertRequestSetTemplateField(CRMFCertRequest       *inCertReq, 
				 CRMFCertTemplateField  inTemplateField,
				 void                  *data)
{
    CRMFCertTemplate *certTemplate;
    PRArenaPool      *poolp;
    SECStatus         rv = SECFailure;
    void             *mark;
    

    if (inCertReq == NULL) {
        return SECFailure;
    }

    certTemplate = &(inCertReq->certTemplate);

    poolp = inCertReq->poolp;
    mark = PORT_ArenaMark(poolp);
    switch (inTemplateField) {
    case crmfVersion:
      rv = crmf_template_add_version(poolp,&(certTemplate->version), 
				     *(long*)data);
      break;
    case crmfSerialNumber:
      rv = crmf_template_add_serialnumber(poolp, 
					  &(certTemplate->serialNumber),
					  *(long*)data);
      break;
    case crmfSigningAlg:
      rv = crmf_template_copy_secalg (poolp, &(certTemplate->signingAlg),
				      (SECAlgorithmID*)data);
      break;
    case crmfIssuer:
      rv = crmf_template_add_issuer (poolp, &(certTemplate->issuer), 
				     (CERTName*)data);
      break;
    case crmfValidity:
      rv = crmf_template_add_validity (poolp, &(certTemplate->validity),
				       (CRMFValidityCreationInfo*)data);
      break;
    case crmfSubject:
      rv = crmf_template_add_subject (poolp, &(certTemplate->subject),
				      (CERTName*)data);
      break;
    case crmfPublicKey:
      rv = crmf_template_add_public_key(poolp, &(certTemplate->publicKey),
					(CERTSubjectPublicKeyInfo*)data);
      break;
    case crmfIssuerUID:
      rv = crmf_template_add_issuer_uid(poolp, &(certTemplate->issuerUID),
					(SECItem*)data);
      break;
    case crmfSubjectUID:
      rv = crmf_template_add_subject_uid(poolp, &(certTemplate->subjectUID),
					 (SECItem*)data);
      break;
    case crmfExtension:
      rv = crmf_template_add_extensions(poolp, certTemplate, 
					(CRMFCertExtCreationInfo*)data);
      break;
    }
    if (rv != SECSuccess) {
        PORT_ArenaRelease(poolp, mark);
    } else {
        PORT_ArenaUnmark(poolp, mark);
    }
    return rv;
}

SECStatus
CRMF_CertReqMsgSetCertRequest (CRMFCertReqMsg  *inCertReqMsg, 
			       CRMFCertRequest *inCertReq)
{
    PORT_Assert (inCertReqMsg != NULL && inCertReq != NULL);
    if (inCertReqMsg == NULL || inCertReq == NULL) {
        return SECFailure;
    }
    inCertReqMsg->certReq = crmf_copy_cert_request(inCertReqMsg->poolp,
						   inCertReq);
    return (inCertReqMsg->certReq == NULL) ? SECFailure : SECSuccess;
}

CRMFCertReqMsg*
CRMF_CreateCertReqMsg(void)
{
    PRArenaPool    *poolp;
    CRMFCertReqMsg *reqMsg;

    poolp = PORT_NewArena(CRMF_DEFAULT_ARENA_SIZE);
    if (poolp == NULL) {
        goto loser;
    }
    reqMsg = PORT_ArenaZNew(poolp, CRMFCertReqMsg);
    if (reqMsg == NULL) {
        goto loser;
    }
    reqMsg->poolp = poolp;
    return reqMsg;
    
 loser:
    if (poolp) {
        PORT_FreeArena(poolp, PR_FALSE);
    }
    return NULL;
}

SECStatus 
CRMF_DestroyCertReqMsg(CRMFCertReqMsg *inCertReqMsg)
{
    PORT_Assert(inCertReqMsg != NULL && inCertReqMsg->poolp != NULL);
    if (!inCertReqMsg->isDecoded) {
        if (inCertReqMsg->certReq->certTemplate.extensions != NULL) {
	    PORT_Free(inCertReqMsg->certReq->certTemplate.extensions);
	}
	if (inCertReqMsg->certReq->controls != NULL) {
	    PORT_Free(inCertReqMsg->certReq->controls);
	}
    }
    PORT_FreeArena(inCertReqMsg->poolp, PR_TRUE);
    return SECSuccess;
}

CRMFCertExtension*
crmf_create_cert_extension(PRArenaPool *poolp, 
			   SECOidTag    id,
			   PRBool       isCritical,
			   SECItem     *data)
{
    CRMFCertExtension *newExt;
    SECOidData        *oidData;
    SECStatus          rv;

    newExt = (poolp == NULL) ? PORT_ZNew(CRMFCertExtension) :
                               PORT_ArenaZNew(poolp, CRMFCertExtension);
    if (newExt == NULL) {
        goto loser;
    }
    oidData = SECOID_FindOIDByTag(id);
    if (oidData == NULL || 
	oidData->supportedExtension != SUPPORTED_CERT_EXTENSION) {
       goto loser;
    }

    rv = SECITEM_CopyItem(poolp, &(newExt->id), &(oidData->oid));
    if (rv != SECSuccess) {
        goto loser;
    }

    rv = SECITEM_CopyItem(poolp, &(newExt->value), data);
    if (rv != SECSuccess) {
        goto loser;
    }

    if (isCritical) {
        newExt->critical.data = (poolp == NULL) ? 
	                                PORT_New(unsigned char) :
	                                PORT_ArenaNew(poolp, unsigned char);
	if (newExt->critical.data == NULL) {
	    goto loser;
	}
	newExt->critical.data[0] = hexTrue;
	newExt->critical.len = 1;
    }
    return newExt;
 loser:
    if (newExt != NULL && poolp == NULL) {
        CRMF_DestroyCertExtension(newExt);
    }
    return NULL;
}

CRMFCertExtension *
CRMF_CreateCertExtension(SECOidTag id,
			 PRBool    isCritical,
			 SECItem  *data) 
{
    return crmf_create_cert_extension(NULL, id, isCritical, data);
}

SECStatus
crmf_destroy_cert_extension(CRMFCertExtension *inExtension, PRBool freeit)
{
    if (inExtension != NULL) {
        SECITEM_FreeItem (&(inExtension->id), PR_FALSE);
	SECITEM_FreeItem (&(inExtension->value), PR_FALSE);
	SECITEM_FreeItem (&(inExtension->critical), PR_FALSE);
	if (freeit) {
	    PORT_Free(inExtension);
	}
    }
    return SECSuccess;
}

SECStatus
CRMF_DestroyCertExtension(CRMFCertExtension *inExtension)
{
    return crmf_destroy_cert_extension(inExtension, PR_TRUE);
}

SECStatus
CRMF_DestroyCertReqMessages(CRMFCertReqMessages *inCertReqMsgs) 
{
    PORT_Assert (inCertReqMsgs != NULL);
    if (inCertReqMsgs != NULL) {
        PORT_FreeArena(inCertReqMsgs->poolp, PR_TRUE);
    }
    return SECSuccess;
}

static PRBool
crmf_item_has_data(SECItem *item)
{
    if (item != NULL && item->data != NULL) {
        return PR_TRUE;
    }
    return PR_FALSE;
}

PRBool
CRMF_CertRequestIsFieldPresent(CRMFCertRequest       *inCertReq,
			       CRMFCertTemplateField  inTemplateField)
{
    PRBool             retVal;
    CRMFCertTemplate *certTemplate;

    PORT_Assert(inCertReq != NULL);
    if (inCertReq == NULL) {
        /* This is probably some kind of error, but this is 
	 * the safest return value for this function.
	 */
        return PR_FALSE;
    }
    certTemplate = &inCertReq->certTemplate;
    switch (inTemplateField) {
    case crmfVersion:
      retVal = crmf_item_has_data(&certTemplate->version);
      break;
    case crmfSerialNumber:
      retVal = crmf_item_has_data(&certTemplate->serialNumber);
      break;
    case crmfSigningAlg:
      retVal = IS_NOT_NULL(certTemplate->signingAlg);
      break;
    case crmfIssuer:
      retVal = IS_NOT_NULL(certTemplate->issuer);
      break;
    case crmfValidity:
      retVal = IS_NOT_NULL(certTemplate->validity);
      break;
    case crmfSubject:
      retVal = IS_NOT_NULL(certTemplate->subject);
      break;
    case crmfPublicKey:
      retVal = IS_NOT_NULL(certTemplate->publicKey);
      break;
    case crmfIssuerUID:
      retVal = crmf_item_has_data(&certTemplate->issuerUID);
      break;
    case crmfSubjectUID:
      retVal = crmf_item_has_data(&certTemplate->subjectUID);
      break;
    case crmfExtension:
      retVal = IS_NOT_NULL(certTemplate->extensions);
      break;
    default:
      retVal = PR_FALSE;
    }
    return retVal;
}
