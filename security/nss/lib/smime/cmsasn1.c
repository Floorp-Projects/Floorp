/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * CMS ASN.1 templates
 */

#include "cmslocal.h"

#include "cert.h"
#include "key.h"
#include "secasn1.h"
#include "secitem.h"
#include "secoid.h"
#include "prtime.h"
#include "secerr.h"


extern const SEC_ASN1Template nss_cms_set_of_attribute_template[];

SEC_ASN1_MKSUB(CERT_IssuerAndSNTemplate)
SEC_ASN1_MKSUB(CERT_SetOfSignedCrlTemplate)
SEC_ASN1_MKSUB(SECOID_AlgorithmIDTemplate)
SEC_ASN1_MKSUB(SEC_BitStringTemplate)
SEC_ASN1_MKSUB(SEC_OctetStringTemplate)
SEC_ASN1_MKSUB(SEC_PointerToOctetStringTemplate)
SEC_ASN1_MKSUB(SEC_SetOfAnyTemplate)

/* -----------------------------------------------------------------------------
 * MESSAGE
 * (uses NSSCMSContentInfo)
 */

/* forward declaration */
static const SEC_ASN1Template *
nss_cms_choose_content_template(void *src_or_dest, PRBool encoding);

static const SEC_ASN1TemplateChooserPtr nss_cms_chooser
	= nss_cms_choose_content_template;

const SEC_ASN1Template NSSCMSMessageTemplate[] = {
    { SEC_ASN1_SEQUENCE | SEC_ASN1_MAY_STREAM,
	  0, NULL, sizeof(NSSCMSMessage) },
    { SEC_ASN1_OBJECT_ID,
	  offsetof(NSSCMSMessage,contentInfo.contentType) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_DYNAMIC | SEC_ASN1_MAY_STREAM
     | SEC_ASN1_EXPLICIT | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 0,
	  offsetof(NSSCMSMessage,contentInfo.content),
	  &nss_cms_chooser },
    { 0 }
};

/* -----------------------------------------------------------------------------
 * ENCAPSULATED & ENCRYPTED CONTENTINFO
 * (both use a NSSCMSContentInfo)
 */
static const SEC_ASN1Template NSSCMSEncapsulatedContentInfoTemplate[] = {
    { SEC_ASN1_SEQUENCE | SEC_ASN1_MAY_STREAM,
	  0, NULL, sizeof(NSSCMSContentInfo) },
    { SEC_ASN1_OBJECT_ID,
	  offsetof(NSSCMSContentInfo,contentType) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_EXPLICIT | SEC_ASN1_MAY_STREAM |
	SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_XTRN | 0,
	  offsetof(NSSCMSContentInfo,rawContent),
	  SEC_ASN1_SUB(SEC_PointerToOctetStringTemplate) },
    { 0 }
};

static const SEC_ASN1Template NSSCMSEncryptedContentInfoTemplate[] = {
    { SEC_ASN1_SEQUENCE | SEC_ASN1_MAY_STREAM,
	  0, NULL, sizeof(NSSCMSContentInfo) },
    { SEC_ASN1_OBJECT_ID,
	  offsetof(NSSCMSContentInfo,contentType) },
    { SEC_ASN1_INLINE | SEC_ASN1_XTRN,
	  offsetof(NSSCMSContentInfo,contentEncAlg),
	  SEC_ASN1_SUB(SECOID_AlgorithmIDTemplate) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_POINTER | SEC_ASN1_MAY_STREAM | 
      SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_XTRN | 0,
	  offsetof(NSSCMSContentInfo,rawContent),
	  SEC_ASN1_SUB(SEC_OctetStringTemplate) },
    { 0 }
};

/* -----------------------------------------------------------------------------
 * SIGNED DATA
 */

const SEC_ASN1Template NSSCMSSignerInfoTemplate[];

const SEC_ASN1Template NSSCMSSignedDataTemplate[] = {
    { SEC_ASN1_SEQUENCE | SEC_ASN1_MAY_STREAM,
	  0, NULL, sizeof(NSSCMSSignedData) },
    { SEC_ASN1_INTEGER,
	  offsetof(NSSCMSSignedData,version) },
    { SEC_ASN1_SET_OF | SEC_ASN1_XTRN,
	  offsetof(NSSCMSSignedData,digestAlgorithms),
	  SEC_ASN1_SUB(SECOID_AlgorithmIDTemplate) },
    { SEC_ASN1_INLINE,
	  offsetof(NSSCMSSignedData,contentInfo),
	  NSSCMSEncapsulatedContentInfoTemplate },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC |
      SEC_ASN1_XTRN | 0,
	  offsetof(NSSCMSSignedData,rawCerts),
	  SEC_ASN1_SUB(SEC_SetOfAnyTemplate) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC |
      SEC_ASN1_XTRN | 1,
	  offsetof(NSSCMSSignedData,crls),
	  SEC_ASN1_SUB(CERT_SetOfSignedCrlTemplate) },
    { SEC_ASN1_SET_OF,
	  offsetof(NSSCMSSignedData,signerInfos),
	  NSSCMSSignerInfoTemplate },
    { 0 }
};

const SEC_ASN1Template NSS_PointerToCMSSignedDataTemplate[] = {
    { SEC_ASN1_POINTER, 0, NSSCMSSignedDataTemplate }
};

/* -----------------------------------------------------------------------------
 * signeridentifier
 */

static const SEC_ASN1Template NSSCMSSignerIdentifierTemplate[] = {
    { SEC_ASN1_CHOICE,
	  offsetof(NSSCMSSignerIdentifier,identifierType), NULL,
	  sizeof(NSSCMSSignerIdentifier) },
    { SEC_ASN1_POINTER | SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_XTRN | 0,
	  offsetof(NSSCMSSignerIdentifier,id.subjectKeyID),
	  SEC_ASN1_SUB(SEC_OctetStringTemplate) ,
	  NSSCMSRecipientID_SubjectKeyID },
    { SEC_ASN1_POINTER | SEC_ASN1_XTRN,
	  offsetof(NSSCMSSignerIdentifier,id.issuerAndSN),
	  SEC_ASN1_SUB(CERT_IssuerAndSNTemplate),
	  NSSCMSRecipientID_IssuerSN },
    { 0 }
};

/* -----------------------------------------------------------------------------
 * signerinfo
 */

const SEC_ASN1Template NSSCMSSignerInfoTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(NSSCMSSignerInfo) },
    { SEC_ASN1_INTEGER,
	  offsetof(NSSCMSSignerInfo,version) },
    { SEC_ASN1_INLINE,
	  offsetof(NSSCMSSignerInfo,signerIdentifier),
	  NSSCMSSignerIdentifierTemplate },
    { SEC_ASN1_INLINE | SEC_ASN1_XTRN,
	  offsetof(NSSCMSSignerInfo,digestAlg),
	  SEC_ASN1_SUB(SECOID_AlgorithmIDTemplate) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 0,
	  offsetof(NSSCMSSignerInfo,authAttr),
	  nss_cms_set_of_attribute_template },
    { SEC_ASN1_INLINE | SEC_ASN1_XTRN,
	  offsetof(NSSCMSSignerInfo,digestEncAlg),
	  SEC_ASN1_SUB(SECOID_AlgorithmIDTemplate) },
    { SEC_ASN1_OCTET_STRING,
	  offsetof(NSSCMSSignerInfo,encDigest) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 1,
	  offsetof(NSSCMSSignerInfo,unAuthAttr),
	  nss_cms_set_of_attribute_template },
    { 0 }
};

/* -----------------------------------------------------------------------------
 * ENVELOPED DATA
 */

static const SEC_ASN1Template NSSCMSOriginatorInfoTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(NSSCMSOriginatorInfo) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC |
      SEC_ASN1_XTRN | 0,
	  offsetof(NSSCMSOriginatorInfo,rawCerts),
	  SEC_ASN1_SUB(SEC_SetOfAnyTemplate) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC |
      SEC_ASN1_XTRN | 1,
	  offsetof(NSSCMSOriginatorInfo,crls),
	  SEC_ASN1_SUB(CERT_SetOfSignedCrlTemplate) },
    { 0 }
};

const SEC_ASN1Template NSSCMSRecipientInfoTemplate[];

const SEC_ASN1Template NSSCMSEnvelopedDataTemplate[] = {
    { SEC_ASN1_SEQUENCE | SEC_ASN1_MAY_STREAM,
	  0, NULL, sizeof(NSSCMSEnvelopedData) },
    { SEC_ASN1_INTEGER,
	  offsetof(NSSCMSEnvelopedData,version) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_POINTER | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 0,
	  offsetof(NSSCMSEnvelopedData,originatorInfo),
	  NSSCMSOriginatorInfoTemplate },
    { SEC_ASN1_SET_OF,
	  offsetof(NSSCMSEnvelopedData,recipientInfos),
	  NSSCMSRecipientInfoTemplate },
    { SEC_ASN1_INLINE,
	  offsetof(NSSCMSEnvelopedData,contentInfo),
	  NSSCMSEncryptedContentInfoTemplate },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 1,
	  offsetof(NSSCMSEnvelopedData,unprotectedAttr),
	  nss_cms_set_of_attribute_template },
    { 0 }
};

const SEC_ASN1Template NSS_PointerToCMSEnvelopedDataTemplate[] = {
    { SEC_ASN1_POINTER, 0, NSSCMSEnvelopedDataTemplate }
};

/* here come the 15 gazillion templates for all the v3 varieties of RecipientInfo */

/* -----------------------------------------------------------------------------
 * key transport recipient info
 */

static const SEC_ASN1Template NSSCMSRecipientIdentifierTemplate[] = {
    { SEC_ASN1_CHOICE,
	  offsetof(NSSCMSRecipientIdentifier,identifierType), NULL,
	  sizeof(NSSCMSRecipientIdentifier) },
    { SEC_ASN1_POINTER | SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_XTRN | 0,
	  offsetof(NSSCMSRecipientIdentifier,id.subjectKeyID),
	  SEC_ASN1_SUB(SEC_OctetStringTemplate) ,
	  NSSCMSRecipientID_SubjectKeyID },
    { SEC_ASN1_POINTER | SEC_ASN1_XTRN,
	  offsetof(NSSCMSRecipientIdentifier,id.issuerAndSN),
	  SEC_ASN1_SUB(CERT_IssuerAndSNTemplate),
	  NSSCMSRecipientID_IssuerSN },
    { 0 }
};


static const SEC_ASN1Template NSSCMSKeyTransRecipientInfoTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(NSSCMSKeyTransRecipientInfo) },
    { SEC_ASN1_INTEGER,
	  offsetof(NSSCMSKeyTransRecipientInfo,version) },
    { SEC_ASN1_INLINE,
	  offsetof(NSSCMSKeyTransRecipientInfo,recipientIdentifier),
	  NSSCMSRecipientIdentifierTemplate },
    { SEC_ASN1_INLINE | SEC_ASN1_XTRN,
	  offsetof(NSSCMSKeyTransRecipientInfo,keyEncAlg),
	  SEC_ASN1_SUB(SECOID_AlgorithmIDTemplate) },
    { SEC_ASN1_OCTET_STRING,
	  offsetof(NSSCMSKeyTransRecipientInfo,encKey) },
    { 0 }
};

/* -----------------------------------------------------------------------------
 * key agreement recipient info
 */

static const SEC_ASN1Template NSSCMSOriginatorPublicKeyTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(NSSCMSOriginatorPublicKey) },
    { SEC_ASN1_INLINE | SEC_ASN1_XTRN,
	  offsetof(NSSCMSOriginatorPublicKey,algorithmIdentifier),
	  SEC_ASN1_SUB(SECOID_AlgorithmIDTemplate) },
    { SEC_ASN1_INLINE | SEC_ASN1_XTRN,
	  offsetof(NSSCMSOriginatorPublicKey,publicKey),
	  SEC_ASN1_SUB(SEC_BitStringTemplate) },
    { 0 }
};


static const SEC_ASN1Template NSSCMSOriginatorIdentifierOrKeyTemplate[] = {
    { SEC_ASN1_CHOICE,
	  offsetof(NSSCMSOriginatorIdentifierOrKey,identifierType), NULL,
	  sizeof(NSSCMSOriginatorIdentifierOrKey) },
    { SEC_ASN1_POINTER | SEC_ASN1_XTRN,
	  offsetof(NSSCMSOriginatorIdentifierOrKey,id.issuerAndSN),
	  SEC_ASN1_SUB(CERT_IssuerAndSNTemplate),
	  NSSCMSOriginatorIDOrKey_IssuerSN },
    { SEC_ASN1_EXPLICIT | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC |
      SEC_ASN1_XTRN | 1,
	  offsetof(NSSCMSOriginatorIdentifierOrKey,id.subjectKeyID),
	  SEC_ASN1_SUB(SEC_PointerToOctetStringTemplate) ,
	  NSSCMSOriginatorIDOrKey_SubjectKeyID },
    { SEC_ASN1_EXPLICIT | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 2,
	  offsetof(NSSCMSOriginatorIdentifierOrKey,id.originatorPublicKey),
	  NSSCMSOriginatorPublicKeyTemplate,
	  NSSCMSOriginatorIDOrKey_OriginatorPublicKey },
    { 0 }
};

const SEC_ASN1Template NSSCMSRecipientKeyIdentifierTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(NSSCMSRecipientKeyIdentifier) },
    { SEC_ASN1_OCTET_STRING,
	  offsetof(NSSCMSRecipientKeyIdentifier,subjectKeyIdentifier) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_OCTET_STRING,
	  offsetof(NSSCMSRecipientKeyIdentifier,date) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_OCTET_STRING,
	  offsetof(NSSCMSRecipientKeyIdentifier,other) },
    { 0 }
};


static const SEC_ASN1Template NSSCMSKeyAgreeRecipientIdentifierTemplate[] = {
    { SEC_ASN1_CHOICE,
	  offsetof(NSSCMSKeyAgreeRecipientIdentifier,identifierType), NULL,
	  sizeof(NSSCMSKeyAgreeRecipientIdentifier) },
    { SEC_ASN1_POINTER | SEC_ASN1_XTRN,
	  offsetof(NSSCMSKeyAgreeRecipientIdentifier,id.issuerAndSN),
	  SEC_ASN1_SUB(CERT_IssuerAndSNTemplate),
	  NSSCMSKeyAgreeRecipientID_IssuerSN },
    { SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 0,
	  offsetof(NSSCMSKeyAgreeRecipientIdentifier,id.recipientKeyIdentifier),
	  NSSCMSRecipientKeyIdentifierTemplate,
	  NSSCMSKeyAgreeRecipientID_RKeyID },
    { 0 }
};

static const SEC_ASN1Template NSSCMSRecipientEncryptedKeyTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(NSSCMSRecipientEncryptedKey) },
    { SEC_ASN1_INLINE,
	  offsetof(NSSCMSRecipientEncryptedKey,recipientIdentifier),
	  NSSCMSKeyAgreeRecipientIdentifierTemplate },
    { SEC_ASN1_INLINE | SEC_ASN1_XTRN,
	  offsetof(NSSCMSRecipientEncryptedKey,encKey),
	  SEC_ASN1_SUB(SEC_BitStringTemplate) },
    { 0 }
};

static const SEC_ASN1Template NSSCMSKeyAgreeRecipientInfoTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(NSSCMSKeyAgreeRecipientInfo) },
    { SEC_ASN1_INTEGER,
	  offsetof(NSSCMSKeyAgreeRecipientInfo,version) },
    { SEC_ASN1_EXPLICIT | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 0,
	  offsetof(NSSCMSKeyAgreeRecipientInfo,originatorIdentifierOrKey),
	  NSSCMSOriginatorIdentifierOrKeyTemplate },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | SEC_ASN1_EXPLICIT |
      SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_XTRN | 1,
	  offsetof(NSSCMSKeyAgreeRecipientInfo,ukm),
	  SEC_ASN1_SUB(SEC_OctetStringTemplate) },
    { SEC_ASN1_INLINE | SEC_ASN1_XTRN,
	  offsetof(NSSCMSKeyAgreeRecipientInfo,keyEncAlg),
	  SEC_ASN1_SUB(SECOID_AlgorithmIDTemplate) },
    { SEC_ASN1_SEQUENCE_OF,
	  offsetof(NSSCMSKeyAgreeRecipientInfo,recipientEncryptedKeys),
	  NSSCMSRecipientEncryptedKeyTemplate },
    { 0 }
};

/* -----------------------------------------------------------------------------
 * KEK recipient info
 */

static const SEC_ASN1Template NSSCMSKEKIdentifierTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(NSSCMSKEKIdentifier) },
    { SEC_ASN1_OCTET_STRING,
	  offsetof(NSSCMSKEKIdentifier,keyIdentifier) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_OCTET_STRING,
	  offsetof(NSSCMSKEKIdentifier,date) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_OCTET_STRING,
	  offsetof(NSSCMSKEKIdentifier,other) },
    { 0 }
};

static const SEC_ASN1Template NSSCMSKEKRecipientInfoTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(NSSCMSKEKRecipientInfo) },
    { SEC_ASN1_INTEGER,
	  offsetof(NSSCMSKEKRecipientInfo,version) },
    { SEC_ASN1_INLINE,
	  offsetof(NSSCMSKEKRecipientInfo,kekIdentifier),
	  NSSCMSKEKIdentifierTemplate },
    { SEC_ASN1_INLINE | SEC_ASN1_XTRN,
	  offsetof(NSSCMSKEKRecipientInfo,keyEncAlg),
	  SEC_ASN1_SUB(SECOID_AlgorithmIDTemplate) },
    { SEC_ASN1_OCTET_STRING,
	  offsetof(NSSCMSKEKRecipientInfo,encKey) },
    { 0 }
};

/* -----------------------------------------------------------------------------
 * recipient info
 */
const SEC_ASN1Template NSSCMSRecipientInfoTemplate[] = {
    { SEC_ASN1_CHOICE,
	  offsetof(NSSCMSRecipientInfo,recipientInfoType), NULL,
	  sizeof(NSSCMSRecipientInfo) },
    { SEC_ASN1_EXPLICIT | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 1,
	  offsetof(NSSCMSRecipientInfo,ri.keyAgreeRecipientInfo),
	  NSSCMSKeyAgreeRecipientInfoTemplate,
	  NSSCMSRecipientInfoID_KeyAgree },
    { SEC_ASN1_EXPLICIT | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 2,
	  offsetof(NSSCMSRecipientInfo,ri.kekRecipientInfo),
	  NSSCMSKEKRecipientInfoTemplate,
	  NSSCMSRecipientInfoID_KEK },
    { SEC_ASN1_INLINE,
	  offsetof(NSSCMSRecipientInfo,ri.keyTransRecipientInfo),
	  NSSCMSKeyTransRecipientInfoTemplate,
	  NSSCMSRecipientInfoID_KeyTrans },
    { 0 }
};

/* -----------------------------------------------------------------------------
 *
 */

const SEC_ASN1Template NSSCMSDigestedDataTemplate[] = {
    { SEC_ASN1_SEQUENCE | SEC_ASN1_MAY_STREAM,
	  0, NULL, sizeof(NSSCMSDigestedData) },
    { SEC_ASN1_INTEGER,
	  offsetof(NSSCMSDigestedData,version) },
    { SEC_ASN1_INLINE | SEC_ASN1_XTRN,
	  offsetof(NSSCMSDigestedData,digestAlg),
	  SEC_ASN1_SUB(SECOID_AlgorithmIDTemplate) },
    { SEC_ASN1_INLINE,
	  offsetof(NSSCMSDigestedData,contentInfo),
	  NSSCMSEncapsulatedContentInfoTemplate },
    { SEC_ASN1_OCTET_STRING,
	  offsetof(NSSCMSDigestedData,digest) },
    { 0 }
};

const SEC_ASN1Template NSS_PointerToCMSDigestedDataTemplate[] = {
    { SEC_ASN1_POINTER, 0, NSSCMSDigestedDataTemplate }
};

const SEC_ASN1Template NSSCMSEncryptedDataTemplate[] = {
    { SEC_ASN1_SEQUENCE | SEC_ASN1_MAY_STREAM,
	  0, NULL, sizeof(NSSCMSEncryptedData) },
    { SEC_ASN1_INTEGER,
	  offsetof(NSSCMSEncryptedData,version) },
    { SEC_ASN1_INLINE,
	  offsetof(NSSCMSEncryptedData,contentInfo),
	  NSSCMSEncryptedContentInfoTemplate },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 1,
	  offsetof(NSSCMSEncryptedData,unprotectedAttr),
	  nss_cms_set_of_attribute_template },
    { 0 }
};

const SEC_ASN1Template NSS_PointerToCMSEncryptedDataTemplate[] = {
    { SEC_ASN1_POINTER, 0, NSSCMSEncryptedDataTemplate }
};

const SEC_ASN1Template NSSCMSGenericWrapperDataTemplate[] = {
    { SEC_ASN1_INLINE,
	  offsetof(NSSCMSGenericWrapperData,contentInfo),
	  NSSCMSEncapsulatedContentInfoTemplate },
};

SEC_ASN1_CHOOSER_IMPLEMENT(NSSCMSGenericWrapperDataTemplate)

const SEC_ASN1Template NSS_PointerToCMSGenericWrapperDataTemplate[] = {
    { SEC_ASN1_POINTER, 0, NSSCMSGenericWrapperDataTemplate }
};

SEC_ASN1_CHOOSER_IMPLEMENT(NSS_PointerToCMSGenericWrapperDataTemplate)

/* -----------------------------------------------------------------------------
 *
 */
static const SEC_ASN1Template *
nss_cms_choose_content_template(void *src_or_dest, PRBool encoding)
{
    const SEC_ASN1Template *theTemplate;
    NSSCMSContentInfo *cinfo;
    SECOidTag type;

    PORT_Assert (src_or_dest != NULL);
    if (src_or_dest == NULL)
	return NULL;

    cinfo = (NSSCMSContentInfo *)src_or_dest;
    type = NSS_CMSContentInfo_GetContentTypeTag(cinfo);
    switch (type) {
    default:
	theTemplate = NSS_CMSType_GetTemplate(type);
	break;
    case SEC_OID_PKCS7_DATA:
	theTemplate = SEC_ASN1_GET(SEC_PointerToOctetStringTemplate);
	break;
    case SEC_OID_PKCS7_SIGNED_DATA:
	theTemplate = NSS_PointerToCMSSignedDataTemplate;
	break;
    case SEC_OID_PKCS7_ENVELOPED_DATA:
	theTemplate = NSS_PointerToCMSEnvelopedDataTemplate;
	break;
    case SEC_OID_PKCS7_DIGESTED_DATA:
	theTemplate = NSS_PointerToCMSDigestedDataTemplate;
	break;
    case SEC_OID_PKCS7_ENCRYPTED_DATA:
	theTemplate = NSS_PointerToCMSEncryptedDataTemplate;
	break;
    }
    return theTemplate;
}
