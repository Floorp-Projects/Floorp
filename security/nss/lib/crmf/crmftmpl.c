/* -*- Mode: C; tab-width: 8 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "crmf.h"
#include "crmfi.h"
#include "secoid.h"
#include "secasn1.h"

SEC_ASN1_MKSUB(SECOID_AlgorithmIDTemplate)
SEC_ASN1_MKSUB(SEC_AnyTemplate)
SEC_ASN1_MKSUB(SEC_NullTemplate)
SEC_ASN1_MKSUB(SEC_BitStringTemplate)
SEC_ASN1_MKSUB(SEC_IntegerTemplate)
SEC_ASN1_MKSUB(SEC_OctetStringTemplate)
SEC_ASN1_MKSUB(CERT_TimeChoiceTemplate)
SEC_ASN1_MKSUB(CERT_SubjectPublicKeyInfoTemplate)
SEC_ASN1_MKSUB(CERT_NameTemplate)

/*
 * It's all implicit tagging.
 */

const SEC_ASN1Template CRMFControlTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(CRMFControl) },
    { SEC_ASN1_OBJECT_ID, offsetof(CRMFControl, derTag) },
    { SEC_ASN1_ANY, offsetof(CRMFControl, derValue) },
    { 0 }
};

static const SEC_ASN1Template CRMFCertExtensionTemplate[] = {
    { SEC_ASN1_SEQUENCE,
      0, NULL, sizeof(CRMFCertExtension) },
    { SEC_ASN1_OBJECT_ID,
      offsetof(CRMFCertExtension, id) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_BOOLEAN,
      offsetof(CRMFCertExtension, critical) },
    { SEC_ASN1_OCTET_STRING,
      offsetof(CRMFCertExtension, value) },
    { 0 }
};

static const SEC_ASN1Template CRMFSequenceOfCertExtensionTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF, 0, CRMFCertExtensionTemplate }
};

static const SEC_ASN1Template CRMFOptionalValidityTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(CRMFOptionalValidity) },
    { SEC_ASN1_EXPLICIT | SEC_ASN1_CONSTRUCTED | SEC_ASN1_NO_STREAM |
          SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_OPTIONAL | SEC_ASN1_XTRN | 0,
      offsetof(CRMFOptionalValidity, notBefore),
      SEC_ASN1_SUB(CERT_TimeChoiceTemplate) },
    { SEC_ASN1_EXPLICIT | SEC_ASN1_CONSTRUCTED | SEC_ASN1_NO_STREAM |
          SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_OPTIONAL | SEC_ASN1_XTRN | 1,
      offsetof(CRMFOptionalValidity, notAfter),
      SEC_ASN1_SUB(CERT_TimeChoiceTemplate) },
    { 0 }
};

static const SEC_ASN1Template crmfPointerToNameTemplate[] = {
    { SEC_ASN1_POINTER | SEC_ASN1_XTRN, 0, SEC_ASN1_SUB(CERT_NameTemplate) },
    { 0 }
};

static const SEC_ASN1Template CRMFCertTemplateTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(CRMFCertTemplate) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_XTRN | 0,
      offsetof(CRMFCertTemplate, version),
      SEC_ASN1_SUB(SEC_IntegerTemplate) },
    { SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_OPTIONAL | SEC_ASN1_XTRN | 1,
      offsetof(CRMFCertTemplate, serialNumber),
      SEC_ASN1_SUB(SEC_IntegerTemplate) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_POINTER |
          SEC_ASN1_XTRN | 2,
      offsetof(CRMFCertTemplate, signingAlg),
      SEC_ASN1_SUB(SECOID_AlgorithmIDTemplate) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONTEXT_SPECIFIC |
          SEC_ASN1_EXPLICIT | SEC_ASN1_CONSTRUCTED | 3,
      offsetof(CRMFCertTemplate, issuer), crmfPointerToNameTemplate },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_POINTER | 4,
      offsetof(CRMFCertTemplate, validity),
      CRMFOptionalValidityTemplate },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONTEXT_SPECIFIC |
          SEC_ASN1_EXPLICIT | SEC_ASN1_CONSTRUCTED | 5,
      offsetof(CRMFCertTemplate, subject), crmfPointerToNameTemplate },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_POINTER |
          SEC_ASN1_XTRN | 6,
      offsetof(CRMFCertTemplate, publicKey),
      SEC_ASN1_SUB(CERT_SubjectPublicKeyInfoTemplate) },
    { SEC_ASN1_NO_STREAM | SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_OPTIONAL |
          SEC_ASN1_XTRN | 7,
      offsetof(CRMFCertTemplate, issuerUID),
      SEC_ASN1_SUB(SEC_BitStringTemplate) },
    { SEC_ASN1_NO_STREAM | SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_OPTIONAL |
          SEC_ASN1_XTRN | 8,
      offsetof(CRMFCertTemplate, subjectUID),
      SEC_ASN1_SUB(SEC_BitStringTemplate) },
    { SEC_ASN1_CONSTRUCTED | SEC_ASN1_OPTIONAL |
          SEC_ASN1_CONTEXT_SPECIFIC | 9,
      offsetof(CRMFCertTemplate, extensions),
      CRMFSequenceOfCertExtensionTemplate },
    { 0 }
};

static const SEC_ASN1Template CRMFAttributeTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(CRMFAttribute) },
    { SEC_ASN1_OBJECT_ID, offsetof(CRMFAttribute, derTag) },
    { SEC_ASN1_ANY, offsetof(CRMFAttribute, derValue) },
    { 0 }
};

const SEC_ASN1Template CRMFCertRequestTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(CRMFCertRequest) },
    { SEC_ASN1_INTEGER, offsetof(CRMFCertRequest, certReqId) },
    { SEC_ASN1_INLINE, offsetof(CRMFCertRequest, certTemplate),
      CRMFCertTemplateTemplate },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_SEQUENCE_OF,
      offsetof(CRMFCertRequest, controls),
      CRMFControlTemplate }, /* SEQUENCE SIZE (1...MAX)*/
    { 0 }
};

const SEC_ASN1Template CRMFCertReqMsgTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(CRMFCertReqMsg) },
    { SEC_ASN1_POINTER, offsetof(CRMFCertReqMsg, certReq),
      CRMFCertRequestTemplate },
    { SEC_ASN1_ANY | SEC_ASN1_OPTIONAL,
      offsetof(CRMFCertReqMsg, derPOP) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_SEQUENCE_OF,
      offsetof(CRMFCertReqMsg, regInfo),
      CRMFAttributeTemplate }, /* SEQUENCE SIZE (1...MAX)*/
    { 0 }
};

const SEC_ASN1Template CRMFCertReqMessagesTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF, offsetof(CRMFCertReqMessages, messages),
      CRMFCertReqMsgTemplate, sizeof(CRMFCertReqMessages) }
};

const SEC_ASN1Template CRMFRAVerifiedTemplate[] = {
    { SEC_ASN1_CONTEXT_SPECIFIC | 0 | SEC_ASN1_XTRN,
      0,
      SEC_ASN1_SUB(SEC_NullTemplate) },
    { 0 }
};

/* This template will need to add POPOSigningKeyInput eventually, maybe*/
static const SEC_ASN1Template crmfPOPOSigningKeyTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(CRMFPOPOSigningKey) },
    { SEC_ASN1_NO_STREAM | SEC_ASN1_OPTIONAL | SEC_ASN1_CONTEXT_SPECIFIC |
          SEC_ASN1_XTRN | 0,
      offsetof(CRMFPOPOSigningKey, derInput),
      SEC_ASN1_SUB(SEC_AnyTemplate) },
    { SEC_ASN1_POINTER | SEC_ASN1_XTRN,
      offsetof(CRMFPOPOSigningKey, algorithmIdentifier),
      SEC_ASN1_SUB(SECOID_AlgorithmIDTemplate) },
    { SEC_ASN1_BIT_STRING | SEC_ASN1_XTRN,
      offsetof(CRMFPOPOSigningKey, signature),
      SEC_ASN1_SUB(SEC_BitStringTemplate) },
    { 0 }
};

const SEC_ASN1Template CRMFPOPOSigningKeyTemplate[] = {
    { SEC_ASN1_CONTEXT_SPECIFIC | 1,
      0,
      crmfPOPOSigningKeyTemplate },
    { 0 }
};

const SEC_ASN1Template CRMFThisMessageTemplate[] = {
    { SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_XTRN | 0,
      0,
      SEC_ASN1_SUB(SEC_BitStringTemplate) },
    { 0 }
};

const SEC_ASN1Template CRMFSubsequentMessageTemplate[] = {
    { SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_XTRN | 1,
      0,
      SEC_ASN1_SUB(SEC_IntegerTemplate) },
    { 0 }
};

const SEC_ASN1Template CRMFDHMACTemplate[] = {
    { SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_XTRN | 2,
      0,
      SEC_ASN1_SUB(SEC_BitStringTemplate) },
    { 0 }
};

const SEC_ASN1Template CRMFPOPOKeyEnciphermentTemplate[] = {
    { SEC_ASN1_EXPLICIT | SEC_ASN1_CONSTRUCTED |
          SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_XTRN | 2,
      0,
      SEC_ASN1_SUB(SEC_AnyTemplate) },
    { 0 }
};

const SEC_ASN1Template CRMFPOPOKeyAgreementTemplate[] = {
    { SEC_ASN1_EXPLICIT | SEC_ASN1_CONSTRUCTED |
          SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_XTRN | 3,
      0,
      SEC_ASN1_SUB(SEC_AnyTemplate) },
    { 0 }
};

const SEC_ASN1Template CRMFEncryptedValueTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(CRMFEncryptedValue) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_POINTER |
          SEC_ASN1_XTRN | 0,
      offsetof(CRMFEncryptedValue, intendedAlg),
      SEC_ASN1_SUB(SECOID_AlgorithmIDTemplate) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_POINTER |
          SEC_ASN1_XTRN | 1,
      offsetof(CRMFEncryptedValue, symmAlg),
      SEC_ASN1_SUB(SECOID_AlgorithmIDTemplate) },
    { SEC_ASN1_NO_STREAM | SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_OPTIONAL |
          SEC_ASN1_XTRN | 2,
      offsetof(CRMFEncryptedValue, encSymmKey),
      SEC_ASN1_SUB(SEC_BitStringTemplate) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_POINTER |
          SEC_ASN1_XTRN | 3,
      offsetof(CRMFEncryptedValue, keyAlg),
      SEC_ASN1_SUB(SECOID_AlgorithmIDTemplate) },
    { SEC_ASN1_NO_STREAM | SEC_ASN1_OPTIONAL | SEC_ASN1_CONTEXT_SPECIFIC |
          SEC_ASN1_XTRN | 4,
      offsetof(CRMFEncryptedValue, valueHint),
      SEC_ASN1_SUB(SEC_OctetStringTemplate) },
    { SEC_ASN1_BIT_STRING, offsetof(CRMFEncryptedValue, encValue) },
    { 0 }
};

const SEC_ASN1Template CRMFEncryptedKeyWithEncryptedValueTemplate[] = {
    { SEC_ASN1_EXPLICIT | SEC_ASN1_CONSTRUCTED |
          SEC_ASN1_CONTEXT_SPECIFIC | 0,
      0,
      CRMFEncryptedValueTemplate },
    { 0 }
};
