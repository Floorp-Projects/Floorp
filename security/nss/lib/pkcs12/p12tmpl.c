/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "plarena.h"
#include "secitem.h"
#include "secoid.h"
#include "seccomon.h"
#include "secport.h"
#include "cert.h"
#include "secpkcs7.h"
#include "secasn1.h"
#include "p12t.h"

SEC_ASN1_MKSUB(SEC_AnyTemplate)
SEC_ASN1_MKSUB(sgn_DigestInfoTemplate)

static const SEC_ASN1Template *
sec_pkcs12_choose_safe_bag_type(void *src_or_dest, PRBool encoding)
{
    const SEC_ASN1Template *theTemplate;
    sec_PKCS12SafeBag *safeBag;
    SECOidData *oiddata;

    if (src_or_dest == NULL) {
        return NULL;
    }

    safeBag = (sec_PKCS12SafeBag *)src_or_dest;

    oiddata = SECOID_FindOID(&safeBag->safeBagType);
    if (oiddata == NULL) {
        return SEC_ASN1_GET(SEC_AnyTemplate);
    }

    switch (oiddata->offset) {
        default:
            theTemplate = SEC_ASN1_GET(SEC_AnyTemplate);
            break;
        case SEC_OID_PKCS12_V1_KEY_BAG_ID:
            theTemplate = SEC_ASN1_GET(SECKEY_PointerToPrivateKeyInfoTemplate);
            break;
        case SEC_OID_PKCS12_V1_CERT_BAG_ID:
            theTemplate = sec_PKCS12PointerToCertBagTemplate;
            break;
        case SEC_OID_PKCS12_V1_CRL_BAG_ID:
            theTemplate = sec_PKCS12PointerToCRLBagTemplate;
            break;
        case SEC_OID_PKCS12_V1_SECRET_BAG_ID:
            theTemplate = sec_PKCS12PointerToSecretBagTemplate;
            break;
        case SEC_OID_PKCS12_V1_PKCS8_SHROUDED_KEY_BAG_ID:
            theTemplate =
                SEC_ASN1_GET(SECKEY_PointerToEncryptedPrivateKeyInfoTemplate);
            break;
        case SEC_OID_PKCS12_V1_SAFE_CONTENTS_BAG_ID:
            if (encoding) {
                theTemplate = sec_PKCS12PointerToSafeContentsTemplate;
            } else {
                theTemplate = SEC_ASN1_GET(SEC_PointerToAnyTemplate);
            }
            break;
    }
    return theTemplate;
}

static const SEC_ASN1Template *
sec_pkcs12_choose_crl_bag_type(void *src_or_dest, PRBool encoding)
{
    const SEC_ASN1Template *theTemplate;
    sec_PKCS12CRLBag *crlbag;
    SECOidData *oiddata;

    if (src_or_dest == NULL) {
        return NULL;
    }

    crlbag = (sec_PKCS12CRLBag *)src_or_dest;

    oiddata = SECOID_FindOID(&crlbag->bagID);
    if (oiddata == NULL) {
        return SEC_ASN1_GET(SEC_AnyTemplate);
    }

    switch (oiddata->offset) {
        default:
            theTemplate = SEC_ASN1_GET(SEC_AnyTemplate);
            break;
        case SEC_OID_PKCS9_X509_CRL:
            theTemplate = SEC_ASN1_GET(SEC_OctetStringTemplate);
            break;
    }
    return theTemplate;
}

static const SEC_ASN1Template *
sec_pkcs12_choose_cert_bag_type(void *src_or_dest, PRBool encoding)
{
    const SEC_ASN1Template *theTemplate;
    sec_PKCS12CertBag *certbag;
    SECOidData *oiddata;

    if (src_or_dest == NULL) {
        return NULL;
    }

    certbag = (sec_PKCS12CertBag *)src_or_dest;

    oiddata = SECOID_FindOID(&certbag->bagID);
    if (oiddata == NULL) {
        return SEC_ASN1_GET(SEC_AnyTemplate);
    }

    switch (oiddata->offset) {
        default:
            theTemplate = SEC_ASN1_GET(SEC_AnyTemplate);
            break;
        case SEC_OID_PKCS9_X509_CERT:
            theTemplate = SEC_ASN1_GET(SEC_OctetStringTemplate);
            break;
        case SEC_OID_PKCS9_SDSI_CERT:
            theTemplate = SEC_ASN1_GET(SEC_IA5StringTemplate);
            break;
    }
    return theTemplate;
}

static const SEC_ASN1Template *
sec_pkcs12_choose_attr_type(void *src_or_dest, PRBool encoding)
{
    const SEC_ASN1Template *theTemplate;
    sec_PKCS12Attribute *attr;
    SECOidData *oiddata;

    if (src_or_dest == NULL) {
        return NULL;
    }

    attr = (sec_PKCS12Attribute *)src_or_dest;

    oiddata = SECOID_FindOID(&attr->attrType);
    if (oiddata == NULL) {
        return SEC_ASN1_GET(SEC_AnyTemplate);
    }

    switch (oiddata->offset) {
        default:
            theTemplate = SEC_ASN1_GET(SEC_AnyTemplate);
            break;
        case SEC_OID_PKCS9_FRIENDLY_NAME:
            theTemplate = SEC_ASN1_GET(SEC_BMPStringTemplate);
            break;
        case SEC_OID_PKCS9_LOCAL_KEY_ID:
            theTemplate = SEC_ASN1_GET(SEC_OctetStringTemplate);
            break;
        case SEC_OID_PKCS12_KEY_USAGE:
            theTemplate = SEC_ASN1_GET(SEC_BitStringTemplate);
            break;
    }

    return theTemplate;
}

const SEC_ASN1Template sec_PKCS12PointerToContentInfoTemplate[] = {
    { SEC_ASN1_POINTER | SEC_ASN1_MAY_STREAM, 0, sec_PKCS7ContentInfoTemplate }
};

static const SEC_ASN1TemplateChooserPtr sec_pkcs12_crl_bag_chooser =
    sec_pkcs12_choose_crl_bag_type;

static const SEC_ASN1TemplateChooserPtr sec_pkcs12_cert_bag_chooser =
    sec_pkcs12_choose_cert_bag_type;

static const SEC_ASN1TemplateChooserPtr sec_pkcs12_safe_bag_chooser =
    sec_pkcs12_choose_safe_bag_type;

static const SEC_ASN1TemplateChooserPtr sec_pkcs12_attr_chooser =
    sec_pkcs12_choose_attr_type;

const SEC_ASN1Template sec_PKCS12PointerToCertBagTemplate[] = {
    { SEC_ASN1_POINTER, 0, sec_PKCS12CertBagTemplate }
};

const SEC_ASN1Template sec_PKCS12PointerToCRLBagTemplate[] = {
    { SEC_ASN1_POINTER, 0, sec_PKCS12CRLBagTemplate }
};

const SEC_ASN1Template sec_PKCS12PointerToSecretBagTemplate[] = {
    { SEC_ASN1_POINTER, 0, sec_PKCS12SecretBagTemplate }
};

const SEC_ASN1Template sec_PKCS12PointerToSafeContentsTemplate[] = {
    { SEC_ASN1_POINTER, 0, sec_PKCS12SafeContentsTemplate }
};

const SEC_ASN1Template sec_PKCS12PFXItemTemplate[] = {
    { SEC_ASN1_SEQUENCE | SEC_ASN1_MAY_STREAM, 0, NULL,
      sizeof(sec_PKCS12PFXItem) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_INTEGER,
      offsetof(sec_PKCS12PFXItem, version) },
    { SEC_ASN1_ANY | SEC_ASN1_MAY_STREAM,
      offsetof(sec_PKCS12PFXItem, encodedAuthSafe) },
    { SEC_ASN1_ANY | SEC_ASN1_MAY_STREAM,
      offsetof(sec_PKCS12PFXItem, encodedMacData) },
    { 0 }
};

const SEC_ASN1Template sec_PKCS12MacDataTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(sec_PKCS12MacData) },
    { SEC_ASN1_INLINE | SEC_ASN1_XTRN, offsetof(sec_PKCS12MacData, safeMac),
      SEC_ASN1_SUB(sgn_DigestInfoTemplate) },
    { SEC_ASN1_OCTET_STRING, offsetof(sec_PKCS12MacData, macSalt) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_INTEGER, offsetof(sec_PKCS12MacData, iter) },
    { 0 }
};

const SEC_ASN1Template sec_PKCS12AuthenticatedSafeTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF | SEC_ASN1_MAY_STREAM | SEC_ASN1_XTRN,
      offsetof(sec_PKCS12AuthenticatedSafe, encodedSafes),
      SEC_ASN1_SUB(SEC_AnyTemplate) }
};

const SEC_ASN1Template sec_PKCS12SafeBagTemplate[] = {
    { SEC_ASN1_SEQUENCE | SEC_ASN1_MAY_STREAM, 0, NULL,
      sizeof(sec_PKCS12SafeBag) },
    { SEC_ASN1_OBJECT_ID, offsetof(sec_PKCS12SafeBag, safeBagType) },
    { SEC_ASN1_EXPLICIT | SEC_ASN1_DYNAMIC | SEC_ASN1_CONSTRUCTED |
          SEC_ASN1_MAY_STREAM | SEC_ASN1_CONTEXT_SPECIFIC | 0,
      offsetof(sec_PKCS12SafeBag, safeBagContent),
      &sec_pkcs12_safe_bag_chooser },
    { SEC_ASN1_SET_OF | SEC_ASN1_OPTIONAL, offsetof(sec_PKCS12SafeBag, attribs),
      sec_PKCS12AttributeTemplate },
    { 0 }
};

const SEC_ASN1Template sec_PKCS12SafeContentsTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF | SEC_ASN1_MAY_STREAM,
      offsetof(sec_PKCS12SafeContents, safeBags),
      sec_PKCS12SafeBagTemplate }
};

const SEC_ASN1Template sec_PKCS12SequenceOfAnyTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF | SEC_ASN1_MAY_STREAM | SEC_ASN1_XTRN, 0,
      SEC_ASN1_SUB(SEC_AnyTemplate) }
};

const SEC_ASN1Template sec_PKCS12NestedSafeContentsDecodeTemplate[] = {
    { SEC_ASN1_EXPLICIT | SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_CONSTRUCTED | 0,
      offsetof(sec_PKCS12SafeContents, encodedSafeBags),
      sec_PKCS12SequenceOfAnyTemplate }
};

const SEC_ASN1Template sec_PKCS12SafeContentsDecodeTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF | SEC_ASN1_MAY_STREAM | SEC_ASN1_XTRN,
      offsetof(sec_PKCS12SafeContents, encodedSafeBags),
      SEC_ASN1_SUB(SEC_AnyTemplate) }
};

const SEC_ASN1Template sec_PKCS12CRLBagTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(sec_PKCS12CRLBag) },
    { SEC_ASN1_OBJECT_ID, offsetof(sec_PKCS12CRLBag, bagID) },
    { SEC_ASN1_DYNAMIC | SEC_ASN1_POINTER,
      offsetof(sec_PKCS12CRLBag, value), &sec_pkcs12_crl_bag_chooser },
    { 0 }
};

const SEC_ASN1Template sec_PKCS12CertBagTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(sec_PKCS12CertBag) },
    { SEC_ASN1_OBJECT_ID, offsetof(sec_PKCS12CertBag, bagID) },
    { SEC_ASN1_DYNAMIC | SEC_ASN1_EXPLICIT | SEC_ASN1_CONSTRUCTED |
          SEC_ASN1_CONTEXT_SPECIFIC | 0,
      offsetof(sec_PKCS12CertBag, value), &sec_pkcs12_cert_bag_chooser },
    { 0 }
};

const SEC_ASN1Template sec_PKCS12SecretBagTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(sec_PKCS12SecretBag) },
    { SEC_ASN1_OBJECT_ID, offsetof(sec_PKCS12SecretBag, secretType) },
    { SEC_ASN1_ANY, offsetof(sec_PKCS12SecretBag, secretContent) },
    { 0 }
};

const SEC_ASN1Template sec_PKCS12AttributeTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(sec_PKCS12Attribute) },
    { SEC_ASN1_OBJECT_ID, offsetof(sec_PKCS12Attribute, attrType) },
    { SEC_ASN1_SET_OF | SEC_ASN1_DYNAMIC,
      offsetof(sec_PKCS12Attribute, attrValue),
      &sec_pkcs12_attr_chooser },
    { 0 }
};
