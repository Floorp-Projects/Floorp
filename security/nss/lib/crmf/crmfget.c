/* -*- Mode: C; tab-width: 8 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "crmf.h"
#include "crmfi.h"
#include "keyhi.h"
#include "secder.h"


CRMFPOPChoice
CRMF_CertReqMsgGetPOPType(CRMFCertReqMsg *inCertReqMsg)
{
    PORT_Assert(inCertReqMsg != NULL);
    if (inCertReqMsg != NULL && inCertReqMsg->pop != NULL) {
        return inCertReqMsg->pop->popUsed;
    }
    return crmfNoPOPChoice;
}

static SECStatus
crmf_destroy_validity(CRMFOptionalValidity *inValidity, PRBool freeit)
{
    if (inValidity != NULL){
        if (inValidity->notBefore.data != NULL) {
	    PORT_Free(inValidity->notBefore.data);
	}
	if (inValidity->notAfter.data != NULL) {
	    PORT_Free(inValidity->notAfter.data);
	}
	if (freeit) {
	    PORT_Free(inValidity);
	}
    }
    return SECSuccess;
}

static SECStatus 
crmf_copy_cert_request_validity(PLArenaPool           *poolp,
				CRMFOptionalValidity **destValidity,
				CRMFOptionalValidity  *srcValidity)
{
    CRMFOptionalValidity *myValidity = NULL;
    SECStatus             rv;

    *destValidity = myValidity = (poolp == NULL) ?
                                  PORT_ZNew(CRMFOptionalValidity) :
                                  PORT_ArenaZNew(poolp, CRMFOptionalValidity);
    if (myValidity == NULL) {
        goto loser;
    }
    if (srcValidity->notBefore.data != NULL) {
        rv = SECITEM_CopyItem(poolp, &myValidity->notBefore, 
			      &srcValidity->notBefore);
	if (rv != SECSuccess) {
	    goto loser;
	}
    }
    if (srcValidity->notAfter.data != NULL) {
        rv = SECITEM_CopyItem(poolp, &myValidity->notAfter, 
			      &srcValidity->notAfter);
	if (rv != SECSuccess) {
	    goto loser;
	}
    }
    return SECSuccess;
 loser:
    if (myValidity != NULL && poolp == NULL) {
        crmf_destroy_validity(myValidity, PR_TRUE);
    }
    return SECFailure;
}

static SECStatus
crmf_copy_extensions(PLArenaPool        *poolp,
		     CRMFCertTemplate   *destTemplate,
		     CRMFCertExtension **srcExt)
{
    int       numExt = 0, i;
    CRMFCertExtension **myExtArray = NULL;

    while (srcExt[numExt] != NULL) {
        numExt++;
    }
    if (numExt == 0) {
        /*No extensions to copy.*/
        destTemplate->extensions = NULL;
	destTemplate->numExtensions = 0;
        return SECSuccess;
    }
    destTemplate->extensions = myExtArray = 
                           PORT_NewArray(CRMFCertExtension*, numExt+1);
    if (myExtArray == NULL) {
        goto loser;
    }
     
    for (i=0; i<numExt; i++) {
        myExtArray[i] = crmf_copy_cert_extension(poolp, srcExt[i]);
	if (myExtArray[i] == NULL) {
	    goto loser;
	}
    }
    destTemplate->numExtensions = numExt;
    myExtArray[numExt] = NULL;
    return SECSuccess;
 loser:
    if (myExtArray != NULL) {
        if (poolp == NULL) {
	    for (i=0; myExtArray[i] != NULL; i++) {
	        CRMF_DestroyCertExtension(myExtArray[i]);
	    }
	}
	PORT_Free(myExtArray);
    }
    destTemplate->extensions = NULL;
    destTemplate->numExtensions = 0;
    return SECFailure;
}

static SECStatus
crmf_copy_cert_request_template(PLArenaPool      *poolp,
				CRMFCertTemplate *destTemplate,
				CRMFCertTemplate *srcTemplate)
{
    SECStatus rv;

    if (srcTemplate->version.data != NULL) {
        rv = SECITEM_CopyItem(poolp, &destTemplate->version, 
			      &srcTemplate->version);
	if (rv != SECSuccess) {
	    goto loser;
	}
    }
    if (srcTemplate->serialNumber.data != NULL) {
        rv = SECITEM_CopyItem(poolp, &destTemplate->serialNumber,
			      &srcTemplate->serialNumber);
	if (rv != SECSuccess) {
	    goto loser;
	}
    }
    if (srcTemplate->signingAlg != NULL) {
        rv = crmf_template_copy_secalg(poolp, &destTemplate->signingAlg,
				       srcTemplate->signingAlg);
	if (rv != SECSuccess) {
	    goto loser;
	}
    }
    if (srcTemplate->issuer != NULL) {
        rv = crmf_copy_cert_name(poolp, &destTemplate->issuer,
				 srcTemplate->issuer);
	if (rv != SECSuccess) {
	    goto loser;
	}
    }
    if (srcTemplate->validity != NULL) {
        rv = crmf_copy_cert_request_validity(poolp, &destTemplate->validity,
					     srcTemplate->validity);
	if (rv != SECSuccess) {
	    goto loser;
	}
    }
    if (srcTemplate->subject != NULL) {
        rv = crmf_copy_cert_name(poolp, &destTemplate->subject, 
				 srcTemplate->subject);
	if (rv != SECSuccess) {
	    goto loser;
	}
    }
    if (srcTemplate->publicKey != NULL) {
        rv = crmf_template_add_public_key(poolp, &destTemplate->publicKey,
					  srcTemplate->publicKey);
	if (rv != SECSuccess) {
	    goto loser;
	}
    }
    if (srcTemplate->issuerUID.data != NULL) {
        rv = crmf_make_bitstring_copy(poolp, &destTemplate->issuerUID,
				      &srcTemplate->issuerUID);
	if (rv != SECSuccess) {
	    goto loser;
	}
    }
    if (srcTemplate->subjectUID.data != NULL) {
        rv = crmf_make_bitstring_copy(poolp, &destTemplate->subjectUID,
				      &srcTemplate->subjectUID);
	if (rv != SECSuccess) {
	    goto loser;
	}
    }
    if (srcTemplate->extensions != NULL) {
        rv = crmf_copy_extensions(poolp, destTemplate,
				  srcTemplate->extensions);
	if (rv != SECSuccess) {
	    goto loser;
	}
    }
    return SECSuccess;
 loser:
    return SECFailure;
}

static CRMFControl*
crmf_copy_control(PLArenaPool *poolp, CRMFControl *srcControl)
{
    CRMFControl *newControl;
    SECStatus    rv;

    newControl = (poolp == NULL) ? PORT_ZNew(CRMFControl) :
                                   PORT_ArenaZNew(poolp, CRMFControl);
    if (newControl == NULL) {
        goto loser;
    }
    newControl->tag = srcControl->tag;
    rv = SECITEM_CopyItem(poolp, &newControl->derTag, &srcControl->derTag);
    if (rv != SECSuccess) {
        goto loser;
    }
    rv = SECITEM_CopyItem(poolp, &newControl->derValue, &srcControl->derValue);
    if (rv != SECSuccess) {
        goto loser;
    }
    /* We only handle PKIArchiveOptions Control right now.  But if in
     * the future, more controls that are part of the union are added,
     * then they need to be handled here as well.
     */
    switch (newControl->tag) {
    case SEC_OID_PKIX_REGCTRL_PKI_ARCH_OPTIONS:
        rv = crmf_copy_pkiarchiveoptions(poolp, 
					 &newControl->value.archiveOptions,
					 &srcControl->value.archiveOptions);
      break;
    default:
        rv = SECSuccess;
    }
    if (rv != SECSuccess) {
        goto loser;
    }
    return newControl;

 loser:
    if (poolp == NULL && newControl != NULL) {
        CRMF_DestroyControl(newControl);
    }
    return NULL;
}

static SECStatus
crmf_copy_cert_request_controls(PLArenaPool     *poolp,
				CRMFCertRequest *destReq, 
				CRMFCertRequest *srcReq)
{
    int           numControls, i;
    CRMFControl **myControls = NULL;

    numControls = CRMF_CertRequestGetNumControls(srcReq);
    if (numControls == 0) {
        /* No Controls To Copy*/
        return SECSuccess;
    }
    myControls = destReq->controls = PORT_NewArray(CRMFControl*, 
						   numControls+1);
    if (myControls == NULL) {
        goto loser;
    }
    for (i=0; i<numControls; i++) {
        myControls[i] = crmf_copy_control(poolp, srcReq->controls[i]);
	if (myControls[i] == NULL) {
	    goto loser;
	}
    }
    myControls[numControls] = NULL;
    return SECSuccess;
 loser:
    if (myControls != NULL) {
        if (poolp == NULL) {
	    for (i=0; myControls[i] != NULL; i++) {
	        CRMF_DestroyControl(myControls[i]);
	    }
	}
	PORT_Free(myControls);
    }
    return SECFailure;
}


CRMFCertRequest*
crmf_copy_cert_request(PLArenaPool *poolp, CRMFCertRequest *srcReq)
{
    CRMFCertRequest *newReq = NULL;
    SECStatus        rv;

    if (srcReq == NULL) {
        return NULL;
    }
    newReq = (poolp == NULL) ? PORT_ZNew(CRMFCertRequest) :
                               PORT_ArenaZNew(poolp, CRMFCertRequest);
    if (newReq == NULL) {
        goto loser;
    }
    rv = SECITEM_CopyItem(poolp, &newReq->certReqId, &srcReq->certReqId);
    if (rv != SECSuccess) {
        goto loser;
    }
    rv = crmf_copy_cert_request_template(poolp, &newReq->certTemplate, 
					 &srcReq->certTemplate);
    if (rv != SECSuccess) {
        goto loser;
    }
    rv = crmf_copy_cert_request_controls(poolp, newReq, srcReq);
    if (rv != SECSuccess) {
        goto loser;
    }
    return newReq;
 loser:
    if (newReq != NULL && poolp == NULL) {
        CRMF_DestroyCertRequest(newReq);
        PORT_Free(newReq);
    }
    return NULL;
}

SECStatus 
CRMF_DestroyGetValidity(CRMFGetValidity *inValidity)
{
    PORT_Assert(inValidity != NULL);
    if (inValidity != NULL) {
        if (inValidity->notAfter) {
	    PORT_Free(inValidity->notAfter);
	    inValidity->notAfter = NULL;
	}
	if (inValidity->notBefore) {
	    PORT_Free(inValidity->notBefore);
	    inValidity->notBefore = NULL;
	}
    }
    return SECSuccess;
}

SECStatus
crmf_make_bitstring_copy(PLArenaPool *arena, SECItem *dest, SECItem *src)
{
    int origLenBits;
    int bytesToCopy;
    SECStatus rv;

    origLenBits = src->len;
    bytesToCopy = CRMF_BITS_TO_BYTES(origLenBits);
    src->len = bytesToCopy;         
    rv = SECITEM_CopyItem(arena, dest, src);
    src->len = origLenBits;
    if (rv != SECSuccess) {
        return rv;
    }
    dest->len = origLenBits;
    return SECSuccess;
}

int
CRMF_CertRequestGetNumberOfExtensions(CRMFCertRequest *inCertReq)
{
    CRMFCertTemplate *certTemplate;
    int count = 0;
    
    certTemplate = &inCertReq->certTemplate;
    if (certTemplate->extensions) {
        while (certTemplate->extensions[count] != NULL)
	    count++;
    }
    return count;
}

SECOidTag
CRMF_CertExtensionGetOidTag(CRMFCertExtension *inExtension)
{
    PORT_Assert(inExtension != NULL);
    if (inExtension == NULL) {
        return SEC_OID_UNKNOWN;
    }
    return SECOID_FindOIDTag(&inExtension->id);
}

PRBool
CRMF_CertExtensionGetIsCritical(CRMFCertExtension *inExt)
{
    PORT_Assert(inExt != NULL);
    if (inExt == NULL) {
        return PR_FALSE;
    }
    return inExt->critical.data != NULL;
}

SECItem*
CRMF_CertExtensionGetValue(CRMFCertExtension *inExtension)
{
    PORT_Assert(inExtension != NULL);
    if (inExtension == NULL) {
        return NULL;
    }
    
    return SECITEM_DupItem(&inExtension->value);
}
			  

SECStatus
CRMF_DestroyPOPOSigningKey(CRMFPOPOSigningKey *inKey)
{
    PORT_Assert(inKey != NULL);
    if (inKey != NULL) {
        if (inKey->derInput.data != NULL) {
	    SECITEM_FreeItem(&inKey->derInput, PR_FALSE);
	}
	if (inKey->algorithmIdentifier != NULL) {
	    SECOID_DestroyAlgorithmID(inKey->algorithmIdentifier, PR_TRUE);
	}
	if (inKey->signature.data != NULL) {
	    SECITEM_FreeItem(&inKey->signature, PR_FALSE);
	}
	PORT_Free(inKey);
    }
    return SECSuccess;
}

SECStatus
CRMF_DestroyPOPOPrivKey(CRMFPOPOPrivKey *inPrivKey)
{
    PORT_Assert(inPrivKey != NULL);
    if (inPrivKey != NULL) {
        SECITEM_FreeItem(&inPrivKey->message.thisMessage, PR_FALSE);
	PORT_Free(inPrivKey);
    }
    return SECSuccess;
}

int
CRMF_CertRequestGetNumControls(CRMFCertRequest *inCertReq)
{
    int              count = 0;

    PORT_Assert(inCertReq != NULL);
    if (inCertReq == NULL) {
        return 0;
    }
    if (inCertReq->controls) {
        while (inCertReq->controls[count] != NULL)
	    count++;
    }
    return count;
}

