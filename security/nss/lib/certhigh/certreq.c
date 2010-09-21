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

#include "cert.h"
#include "certt.h"
#include "secder.h"
#include "key.h"
#include "secitem.h"
#include "secasn1.h"
#include "secerr.h"

SEC_ASN1_MKSUB(SEC_AnyTemplate)

const SEC_ASN1Template CERT_AttributeTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	0, NULL, sizeof(CERTAttribute) },
    { SEC_ASN1_OBJECT_ID, offsetof(CERTAttribute, attrType) },
    { SEC_ASN1_SET_OF | SEC_ASN1_XTRN, offsetof(CERTAttribute, attrValue),
	SEC_ASN1_SUB(SEC_AnyTemplate) },
    { 0 }
};

const SEC_ASN1Template CERT_SetOfAttributeTemplate[] = {
    { SEC_ASN1_SET_OF, 0, CERT_AttributeTemplate },
};

const SEC_ASN1Template CERT_CertificateRequestTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(CERTCertificateRequest) },
    { SEC_ASN1_INTEGER,
	  offsetof(CERTCertificateRequest,version) },
    { SEC_ASN1_INLINE,
	  offsetof(CERTCertificateRequest,subject),
	  CERT_NameTemplate },
    { SEC_ASN1_INLINE,
	  offsetof(CERTCertificateRequest,subjectPublicKeyInfo),
	  CERT_SubjectPublicKeyInfoTemplate },
    { SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 0,
	  offsetof(CERTCertificateRequest,attributes), 
	  CERT_SetOfAttributeTemplate },
    { 0 }
};

SEC_ASN1_CHOOSER_IMPLEMENT(CERT_CertificateRequestTemplate)

CERTCertificate *
CERT_CreateCertificate(unsigned long serialNumber,
		      CERTName *issuer,
		      CERTValidity *validity,
		      CERTCertificateRequest *req)
{
    CERTCertificate *c;
    int rv;
    PRArenaPool *arena;
    
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    
    if ( !arena ) {
	return(0);
    }

    c = (CERTCertificate *)PORT_ArenaZAlloc(arena, sizeof(CERTCertificate));
    
    if (!c) {
	PORT_FreeArena(arena, PR_FALSE);
	return 0;
    }

    c->referenceCount = 1;
    c->arena = arena;

    /*
     * Default is a plain version 1.
     * If extensions are added, it will get changed as appropriate.
     */
    rv = DER_SetUInteger(arena, &c->version, SEC_CERTIFICATE_VERSION_1);
    if (rv) goto loser;

    rv = DER_SetUInteger(arena, &c->serialNumber, serialNumber);
    if (rv) goto loser;

    rv = CERT_CopyName(arena, &c->issuer, issuer);
    if (rv) goto loser;

    rv = CERT_CopyValidity(arena, &c->validity, validity);
    if (rv) goto loser;

    rv = CERT_CopyName(arena, &c->subject, &req->subject);
    if (rv) goto loser;
    rv = SECKEY_CopySubjectPublicKeyInfo(arena, &c->subjectPublicKeyInfo,
					 &req->subjectPublicKeyInfo);
    if (rv) goto loser;

    return c;

 loser:
    CERT_DestroyCertificate(c);
    return 0;
}

/************************************************************************/
/* It's clear from the comments that the original author of this 
 * function expected the template for certificate requests to treat
 * the attributes as a SET OF ANY.  This function expected to be 
 * passed an array of SECItems each of which contained an already encoded
 * Attribute.  But the cert request template does not treat the 
 * Attributes as a SET OF ANY, and AFAIK never has.  Instead the template
 * encodes attributes as a SET OF xxxxxxx.  That is, it expects to encode
 * each of the Attributes, not have them pre-encoded.  Consequently an 
 * array of SECItems containing encoded Attributes is of no value to this 
 * function.  But we cannot change the signature of this public function.
 * It must continue to take SECItems.
 *
 * I have recoded this function so that each SECItem contains an 
 * encoded cert extension.  The encoded cert extensions form the list for the
 * single attribute of the cert request. In this implementation there is at most
 * one attribute and it is always of type SEC_OID_PKCS9_EXTENSION_REQUEST.
 */

CERTCertificateRequest *
CERT_CreateCertificateRequest(CERTName *subject,
			     CERTSubjectPublicKeyInfo *spki,
			     SECItem **attributes)
{
    CERTCertificateRequest *certreq;
    PRArenaPool *arena;
    CERTAttribute * attribute;
    SECOidData * oidData;
    SECStatus rv;
    int i = 0;

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if ( arena == NULL ) {
	return NULL;
    }
    
    certreq = PORT_ArenaZNew(arena, CERTCertificateRequest);
    if (!certreq) {
	PORT_FreeArena(arena, PR_FALSE);
	return NULL;
    }
    /* below here it is safe to goto loser */

    certreq->arena = arena;
    
    rv = DER_SetUInteger(arena, &certreq->version,
			 SEC_CERTIFICATE_REQUEST_VERSION);
    if (rv != SECSuccess)
	goto loser;

    rv = CERT_CopyName(arena, &certreq->subject, subject);
    if (rv != SECSuccess)
	goto loser;

    rv = SECKEY_CopySubjectPublicKeyInfo(arena,
				      &certreq->subjectPublicKeyInfo,
				      spki);
    if (rv != SECSuccess)
	goto loser;

    certreq->attributes = PORT_ArenaZNewArray(arena, CERTAttribute*, 2);
    if(!certreq->attributes) 
	goto loser;

    /* Copy over attribute information */
    if (!attributes || !attributes[0]) {
	/*
	 ** Invent empty attribute information. According to the
	 ** pkcs#10 spec, attributes has this ASN.1 type:
	 **
	 ** attributes [0] IMPLICIT Attributes
	 ** 
	 ** Which means, we should create a NULL terminated list
	 ** with the first entry being NULL;
	 */
	certreq->attributes[0] = NULL;
	return certreq;
    }	

    /* allocate space for attributes */
    attribute = PORT_ArenaZNew(arena, CERTAttribute);
    if (!attribute) 
	goto loser;

    oidData = SECOID_FindOIDByTag( SEC_OID_PKCS9_EXTENSION_REQUEST );
    PORT_Assert(oidData);
    if (!oidData)
	goto loser;
    rv = SECITEM_CopyItem(arena, &attribute->attrType, &oidData->oid);
    if (rv != SECSuccess)
	goto loser;

    for (i = 0; attributes[i] != NULL ; i++) 
	;
    attribute->attrValue = PORT_ArenaZNewArray(arena, SECItem *, i+1);
    if (!attribute->attrValue) 
	goto loser;

    /* copy attributes */
    for (i = 0; attributes[i]; i++) {
	/*
	** Attributes are a SetOf Attribute which implies
	** lexigraphical ordering.  It is assumes that the
	** attributes are passed in sorted.  If we need to
	** add functionality to sort them, there is an
	** example in the PKCS 7 code.
	*/
	attribute->attrValue[i] = SECITEM_ArenaDupItem(arena, attributes[i]);
	if(!attribute->attrValue[i]) 
	    goto loser;
    }

    certreq->attributes[0] = attribute;

    return certreq;

loser:
    CERT_DestroyCertificateRequest(certreq);
    return NULL;
}

void
CERT_DestroyCertificateRequest(CERTCertificateRequest *req)
{
    if (req && req->arena) {
	PORT_FreeArena(req->arena, PR_FALSE);
    }
    return;
}

static void
setCRExt(void *o, CERTCertExtension **exts)
{
    ((CERTCertificateRequest *)o)->attributes = (struct CERTAttributeStr **)exts;
}

/*
** Set up to start gathering cert extensions for a cert request.
** The list is created as CertExtensions and converted to an
** attribute list by CERT_FinishCRAttributes().
 */
extern void *cert_StartExtensions(void *owner, PRArenaPool *ownerArena,
                       void (*setExts)(void *object, CERTCertExtension **exts));
void *
CERT_StartCertificateRequestAttributes(CERTCertificateRequest *req)
{
    return (cert_StartExtensions ((void *)req, req->arena, setCRExt));
}

/*
** At entry req->attributes actually contains an list of cert extensions--
** req-attributes is overloaded until the list is DER encoded (the first
** ...EncodeItem() below).
** We turn this into an attribute list by encapsulating it
** in a PKCS 10 Attribute structure
 */
SECStatus
CERT_FinishCertificateRequestAttributes(CERTCertificateRequest *req)
{   SECItem *extlist;
    SECOidData *oidrec;
    CERTAttribute *attribute;
   
    if (!req || !req->arena) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }
    if (req->attributes == NULL || req->attributes[0] == NULL)
        return SECSuccess;

    extlist = SEC_ASN1EncodeItem(req->arena, NULL, &req->attributes,
                            SEC_ASN1_GET(CERT_SequenceOfCertExtensionTemplate));
    if (extlist == NULL)
        return(SECFailure);

    oidrec = SECOID_FindOIDByTag(SEC_OID_PKCS9_EXTENSION_REQUEST);
    if (oidrec == NULL)
	return SECFailure;

    /* now change the list of cert extensions into a list of attributes
     */
    req->attributes = PORT_ArenaZNewArray(req->arena, CERTAttribute*, 2);

    attribute = PORT_ArenaZNew(req->arena, CERTAttribute);
    
    if (req->attributes == NULL || attribute == NULL ||
        SECITEM_CopyItem(req->arena, &attribute->attrType, &oidrec->oid) != 0) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
	return SECFailure;
    }
    attribute->attrValue = PORT_ArenaZNewArray(req->arena, SECItem*, 2);

    if (attribute->attrValue == NULL)
        return SECFailure;

    attribute->attrValue[0] = extlist;
    attribute->attrValue[1] = NULL;
    req->attributes[0] = attribute;
    req->attributes[1] = NULL;

    return SECSuccess;
}

SECStatus
CERT_GetCertificateRequestExtensions(CERTCertificateRequest *req,
                                        CERTCertExtension ***exts)
{
    if (req == NULL || exts == NULL) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }
    
    if (req->attributes == NULL || *req->attributes == NULL)
        return SECSuccess;
    
    if ((*req->attributes)->attrValue == NULL) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    return(SEC_ASN1DecodeItem(req->arena, exts, 
            SEC_ASN1_GET(CERT_SequenceOfCertExtensionTemplate),
            (*req->attributes)->attrValue[0]));
}
