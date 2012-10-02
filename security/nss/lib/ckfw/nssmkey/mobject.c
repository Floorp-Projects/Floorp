/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifdef DEBUG
static const char CVS_ID[] = "@(#) $RCSfile: mobject.c,v $ $Revision: 1.6 $ $Date: 2012/04/25 14:49:40 $";
#endif /* DEBUG */

#include "ckmk.h"
#include "nssbase.h"

#include "secdert.h" /* for DER_INTEGER */
#include "string.h"

/* asn1 encoder (to build pkcs#8 blobs) */
#include <seccomon.h>
#include <secitem.h>
#include <blapit.h>
#include <secoid.h>
#include <secasn1.h>

/* for importing the keys */
#include <CoreFoundation/CoreFoundation.h>
#include <security/SecImportExport.h>

/*
 * nssmkey/mobject.c
 *
 * This file implements the NSSCKMDObject object for the
 * "nssmkey" cryptoki module.
 */

const CK_ATTRIBUTE_TYPE certAttrs[] = {
    CKA_CLASS,
    CKA_TOKEN,
    CKA_PRIVATE,
    CKA_MODIFIABLE,
    CKA_LABEL,
    CKA_CERTIFICATE_TYPE,
    CKA_SUBJECT,
    CKA_ISSUER,
    CKA_SERIAL_NUMBER,
    CKA_VALUE
};
const PRUint32 certAttrsCount = NSS_CKMK_ARRAY_SIZE(certAttrs);

/* private keys, for now only support RSA */
const CK_ATTRIBUTE_TYPE privKeyAttrs[] = {
    CKA_CLASS,
    CKA_TOKEN,
    CKA_PRIVATE,
    CKA_MODIFIABLE,
    CKA_LABEL,
    CKA_KEY_TYPE,
    CKA_DERIVE,
    CKA_LOCAL,
    CKA_SUBJECT,
    CKA_SENSITIVE,
    CKA_DECRYPT,
    CKA_SIGN,
    CKA_SIGN_RECOVER,
    CKA_UNWRAP,
    CKA_EXTRACTABLE,
    CKA_ALWAYS_SENSITIVE,
    CKA_NEVER_EXTRACTABLE,
    CKA_MODULUS,
    CKA_PUBLIC_EXPONENT,
};
const PRUint32 privKeyAttrsCount = NSS_CKMK_ARRAY_SIZE(privKeyAttrs);

/* public keys, for now only support RSA */
const CK_ATTRIBUTE_TYPE pubKeyAttrs[] = {
    CKA_CLASS,
    CKA_TOKEN,
    CKA_PRIVATE,
    CKA_MODIFIABLE,
    CKA_LABEL,
    CKA_KEY_TYPE,
    CKA_DERIVE,
    CKA_LOCAL,
    CKA_SUBJECT,
    CKA_ENCRYPT,
    CKA_VERIFY,
    CKA_VERIFY_RECOVER,
    CKA_WRAP,
    CKA_MODULUS,
    CKA_PUBLIC_EXPONENT,
};
const PRUint32 pubKeyAttrsCount = NSS_CKMK_ARRAY_SIZE(pubKeyAttrs);
static const CK_BBOOL ck_true = CK_TRUE;
static const CK_BBOOL ck_false = CK_FALSE;
static const CK_CERTIFICATE_TYPE ckc_x509 = CKC_X_509;
static const CK_KEY_TYPE ckk_rsa = CKK_RSA;
static const CK_OBJECT_CLASS cko_certificate = CKO_CERTIFICATE;
static const CK_OBJECT_CLASS cko_private_key = CKO_PRIVATE_KEY;
static const CK_OBJECT_CLASS cko_public_key = CKO_PUBLIC_KEY;
static const NSSItem ckmk_trueItem = { 
  (void *)&ck_true, (PRUint32)sizeof(CK_BBOOL) };
static const NSSItem ckmk_falseItem = { 
  (void *)&ck_false, (PRUint32)sizeof(CK_BBOOL) };
static const NSSItem ckmk_x509Item = { 
  (void *)&ckc_x509, (PRUint32)sizeof(CK_CERTIFICATE_TYPE) };
static const NSSItem ckmk_rsaItem = { 
  (void *)&ckk_rsa, (PRUint32)sizeof(CK_KEY_TYPE) };
static const NSSItem ckmk_certClassItem = { 
  (void *)&cko_certificate, (PRUint32)sizeof(CK_OBJECT_CLASS) };
static const NSSItem ckmk_privKeyClassItem = {
  (void *)&cko_private_key, (PRUint32)sizeof(CK_OBJECT_CLASS) };
static const NSSItem ckmk_pubKeyClassItem = {
  (void *)&cko_public_key, (PRUint32)sizeof(CK_OBJECT_CLASS) };
static const NSSItem ckmk_emptyItem = { 
  (void *)&ck_true, 0};

/*
 * these are utilities. The chould be moved to a new utilities file.
 */
#ifdef DEBUG
static void
itemdump(char *str, void *data, int size, CK_RV error) 
{
  unsigned char *ptr = (unsigned char *)data;
  int i;
  fprintf(stderr,str);
  for (i=0; i < size; i++) {
    fprintf(stderr,"%02x ",(unsigned int) ptr[i]);
  }
  fprintf(stderr," (error = %d)\n", (int ) error);
}
#endif

/*
 * unwrap a single DER value
 * now that we have util linked in, we should probably use
 * the ANS1_Decoder for this work...
 */
unsigned char *
nss_ckmk_DERUnwrap
(
  unsigned char *src, 
  int size, 
  int *outSize, 
  unsigned char **next
)
{
  unsigned char *start = src;
  unsigned int len = 0;

  /* initialize error condition return values */
  *outSize = 0;
  if (next) {
    *next = src;
  }

  if (size < 2) {
    return start;
  }
  src ++ ; /* skip the tag -- should check it against an expected value! */
  len = (unsigned) *src++;
  if (len & 0x80) {
    int count = len & 0x7f;
    len =0;

    if (count+2 > size) {
      return start;
    }
    while (count-- > 0) {
      len = (len << 8) | (unsigned) *src++;
    }
  }
  if (len + (src-start) > (unsigned int)size) {
    return start;
  }
  if (next) {
    *next = src+len;
  }
  *outSize = len;

  return src;
}

/*
 * get an attribute from a template. Value is returned in NSS item.
 * data for the item is owned by the template.
 */
CK_RV
nss_ckmk_GetAttribute
(
  CK_ATTRIBUTE_TYPE type,
  CK_ATTRIBUTE *template,
  CK_ULONG templateSize, 
  NSSItem *item
)
{
  CK_ULONG i;

  for (i=0; i < templateSize; i++) {
    if (template[i].type == type) {
      item->data = template[i].pValue;
      item->size = template[i].ulValueLen;
      return CKR_OK;
    }
  }
  return CKR_TEMPLATE_INCOMPLETE;
}

/*
 * get an attribute which is type CK_ULONG.
 */
CK_ULONG
nss_ckmk_GetULongAttribute
(
  CK_ATTRIBUTE_TYPE type,
  CK_ATTRIBUTE *template,
  CK_ULONG templateSize, 
  CK_RV *pError
)
{
  NSSItem item;

  *pError = nss_ckmk_GetAttribute(type, template, templateSize, &item);
  if (CKR_OK != *pError) {
    return (CK_ULONG) 0;
  }
  if (item.size != sizeof(CK_ULONG)) {
    *pError = CKR_ATTRIBUTE_VALUE_INVALID;
    return (CK_ULONG) 0;
  }
  return *(CK_ULONG *)item.data;
}

/*
 * get an attribute which is type CK_BBOOL.
 */
CK_BBOOL
nss_ckmk_GetBoolAttribute
(
  CK_ATTRIBUTE_TYPE type,
  CK_ATTRIBUTE *template,
  CK_ULONG templateSize, 
  CK_BBOOL defaultBool
)
{
  NSSItem item;
  CK_RV error;

  error = nss_ckmk_GetAttribute(type, template, templateSize, &item);
  if (CKR_OK != error) {
    return defaultBool;
  }
  if (item.size != sizeof(CK_BBOOL)) {
    return defaultBool;
  }
  return *(CK_BBOOL *)item.data;
}

/*
 * get an attribute as a NULL terminated string. Caller is responsible to
 * free the string.
 */
char *
nss_ckmk_GetStringAttribute
(
  CK_ATTRIBUTE_TYPE type,
  CK_ATTRIBUTE *template,
  CK_ULONG templateSize, 
  CK_RV *pError
)
{
  NSSItem item;
  char *str;

  /* get the attribute */
  *pError = nss_ckmk_GetAttribute(type, template, templateSize, &item);
  if (CKR_OK != *pError) {
    return (char *)NULL;
  }
  /* make sure it is null terminated */
  str = nss_ZNEWARRAY(NULL, char, item.size+1);
  if ((char *)NULL == str) {
    *pError = CKR_HOST_MEMORY;
    return (char *)NULL;
  }

  nsslibc_memcpy(str, item.data, item.size);
  str[item.size] = 0;

  return str;
}

/*
 * Apple doesn't seem to have a public interface to the DER encoder,
 * wip out a quick one for integers only (anything more complicated,
 * we should use one of the 3 in lib/util). -- especially since we
 * now link with it.
 */
static CK_RV
ckmk_encodeInt(NSSItem *dest, void *src, int srcLen)
{                 
  int dataLen = srcLen;
  int lenLen = 1;
  int encLen;
  int isSigned = 0;
  int offset = 0;
  unsigned char *data = NULL;
  int i;

  if (*(unsigned char *)src & 0x80) {
    dataLen++;
    isSigned = 1;
  }
   
  /* calculate the length of the length specifier */
  /* (NOTE: destroys dataLen value) */ 
  if (dataLen > 0x7f) {
    do {
      lenLen++;
      dataLen >>= 8;
    } while (dataLen);
  }

  /* calculate our total length */
  dataLen = isSigned + srcLen;
  encLen = 1 + lenLen + dataLen;
  data  = nss_ZNEWARRAY(NULL, unsigned char, encLen);
  if ((unsigned char *)NULL == data) {
    return CKR_HOST_MEMORY;
  }
  data[0] = DER_INTEGER;
  if (1 == lenLen) {
    data[1] = dataLen;
  } else {
    data[1] = 0x80 + lenLen;
    for (i=0; i < lenLen; i++) {
      data[i+1] = ((dataLen >> ((lenLen-i-1)*8)) & 0xff);
    }
  }
  offset = lenLen+1;

  if (isSigned) {
    data[offset++] = 0;
  }
  nsslibc_memcpy(&data[offset], src, srcLen);
  dest->data = data;
  dest->size = encLen;
  return CKR_OK;
}


/*
 * Get a Keyring attribute. If content is set to true, then we get the
 * content, not the attribute.
 */
static CK_RV
ckmk_GetCommonAttribute
(
  ckmkInternalObject *io,
  SecItemAttr itemAttr,
  PRBool content,
  NSSItem *item,
  char *dbString
)
{
  SecKeychainAttributeList *attrList = NULL;
  SecKeychainAttributeInfo attrInfo;
  PRUint32 len = 0;
  PRUint32 dataLen = 0;
  PRUint32 attrFormat = 0;
  void *dataVal = 0;
  void *out = NULL;
  CK_RV error = CKR_OK;
  OSStatus macErr;

  attrInfo.count = 1;
  attrInfo.tag = &itemAttr;
  attrInfo.format = &attrFormat;

  macErr = SecKeychainItemCopyAttributesAndData(io->u.item.itemRef, 
                                &attrInfo, NULL, &attrList, &len, &out);
  if (noErr != macErr) {
    CKMK_MACERR(dbString, macErr);
    return CKR_ATTRIBUTE_TYPE_INVALID;
  }
  dataLen = content ? len : attrList->attr->length;
  dataVal = content ? out : attrList->attr->data;

  /* Apple's documentation says this value is DER Encoded, but it clearly isn't
   * der encode it before we ship it back off to NSS
   */
  if ( kSecSerialNumberItemAttr == itemAttr ) {
    error = ckmk_encodeInt(item, dataVal, dataLen);
    goto loser; /* logically 'done' if error == CKR_OK */
  }
  item->data = nss_ZNEWARRAY(NULL, char, dataLen);
  if (NULL == item->data) {
    error = CKR_HOST_MEMORY;
    goto loser;
  }
  nsslibc_memcpy(item->data, dataVal, dataLen);
  item->size = dataLen;

loser:
  SecKeychainItemFreeAttributesAndData(attrList, out);
  return error;
}

/*
 * change an attribute (does not operate on the content).
 */
static CK_RV
ckmk_updateAttribute
(
  SecKeychainItemRef itemRef,
  SecItemAttr itemAttr,
  void *data,
  PRUint32 len,
  char *dbString
)
{
  SecKeychainAttributeList attrList;
  SecKeychainAttribute     attrAttr;
  OSStatus macErr;
  CK_RV error = CKR_OK;

  attrList.count = 1;
  attrList.attr = &attrAttr;
  attrAttr.tag = itemAttr;
  attrAttr.data = data;
  attrAttr.length = len;
  macErr = SecKeychainItemModifyAttributesAndData(itemRef, &attrList, 0, NULL);
  if (noErr != macErr) {
    CKMK_MACERR(dbString, macErr);
    error = CKR_ATTRIBUTE_TYPE_INVALID;
  }
  return error;
}

/*
 * get an attribute (does not operate on the content)
 */
static CK_RV
ckmk_GetDataAttribute
(
  ckmkInternalObject *io,
  SecItemAttr itemAttr,
  NSSItem *item,
  char *dbString
)
{
  return ckmk_GetCommonAttribute(io, itemAttr, PR_FALSE, item, dbString);
}

/*
 * get an attribute we know is a BOOL.
 */
static CK_RV
ckmk_GetBoolAttribute
(
  ckmkInternalObject *io,
  SecItemAttr itemAttr,
  NSSItem *item,
  char *dbString
)
{
  SecKeychainAttribute attr;
  SecKeychainAttributeList attrList;
  CK_BBOOL *boolp = NULL;
  PRUint32 len = 0;;
  void *out = NULL;
  CK_RV error = CKR_OK;
  OSStatus macErr;

  attr.tag = itemAttr;
  attr.length = 0;
  attr.data = NULL;
  attrList.count = 1;
  attrList.attr = &attr;

  boolp = nss_ZNEW(NULL, CK_BBOOL);
  if ((CK_BBOOL *)NULL == boolp) {
    error = CKR_HOST_MEMORY;
    goto loser;
  }

  macErr = SecKeychainItemCopyContent(io->u.item.itemRef, NULL, 
                                      &attrList, &len, &out);
  if (noErr != macErr) {
    CKMK_MACERR(dbString, macErr);
    error = CKR_ATTRIBUTE_TYPE_INVALID;
    goto loser;
  }
  if (sizeof(PRUint32) != attr.length) {
    error = CKR_ATTRIBUTE_TYPE_INVALID;
    goto loser;
  }
  *boolp = *(PRUint32 *)attr.data ? 1 : 0;
  item->data =  boolp;
  boolp = NULL;
  item->size = sizeof(CK_BBOOL);

loser:
  nss_ZFreeIf(boolp);
  SecKeychainItemFreeContent(&attrList, out);
  return error;
}


/*
 * macros for fetching attributes into a cache and returning the
 * appropriate value. These operate inside switch statements
 */
#define CKMK_HANDLE_ITEM(func, io, type, loc, item, error, str) \
    if (0 == (item)->loc.size) { \
      error = func(io, type, &(item)->loc, str); \
    } \
    return (CKR_OK == (error)) ? &(item)->loc : NULL;

#define CKMK_HANDLE_OPT_ITEM(func, io, type, loc, item, error, str) \
    if (0 == (item)->loc.size) { \
      (void) func(io, type, &(item)->loc, str); \
    } \
    return &(item)->loc ;

#define CKMK_HANDLE_BOOL_ITEM(io, type, loc, item, error, str) \
    CKMK_HANDLE_ITEM(ckmk_GetBoolAttribute, io, type, loc, item, error, str)
#define CKMK_HANDLE_DATA_ITEM(io, type, loc, item, error, str) \
    CKMK_HANDLE_ITEM(ckmk_GetDataAttribute, io, type, loc, item, error, str)
#define CKMK_HANDLE_OPT_DATA_ITEM(io, type, loc, item, error, str) \
    CKMK_HANDLE_OPT_ITEM(ckmk_GetDataAttribute, io, type, loc, item, error, str)

/*
 * fetch the unique identifier for each object type.
 */
static void
ckmk_FetchHashKey
(
  ckmkInternalObject *io
)
{
  NSSItem *key = &io->hashKey;

  if (io->objClass == CKO_CERTIFICATE) {
    ckmk_GetCommonAttribute(io, kSecCertEncodingItemAttr, 
                            PR_TRUE, key, "Fetching HashKey (cert)");
  } else {
    ckmk_GetCommonAttribute(io, kSecKeyLabel,
                            PR_FALSE, key, "Fetching HashKey (key)");
  }
}

/*
 * Apple mucks with the actual subject and issuer, so go fetch
 * the real ones ourselves.
 */
static void 
ckmk_fetchCert
(
  ckmkInternalObject *io
)
{
  CK_RV error;
  unsigned char * cert, *next;
  int certSize, thisEntrySize;

  error = ckmk_GetCommonAttribute(io, kSecCertEncodingItemAttr, PR_TRUE, 
                        &io->u.item.derCert, "Fetching Value (cert)");
  if (CKR_OK != error) {
    return;
  }
  /* unwrap the cert bundle */
  cert  = nss_ckmk_DERUnwrap((unsigned char *)io->u.item.derCert.data,
                            io->u.item.derCert.size,
                            &certSize, NULL);
  /* unwrap the cert itself */
  /* cert == certdata */
  cert = nss_ckmk_DERUnwrap(cert, certSize, &certSize, NULL);

  /* skip the optional version */
  if ((cert[0] & 0xa0) == 0xa0) {
    nss_ckmk_DERUnwrap(cert, certSize, &thisEntrySize, &next);
    certSize -= next - cert;
    cert = next;
  }
  /* skip the serial number */
  nss_ckmk_DERUnwrap(cert, certSize, &thisEntrySize, &next);
  certSize -= next - cert;
  cert = next;

  /* skip the OID */
  nss_ckmk_DERUnwrap(cert, certSize, &thisEntrySize, &next);
  certSize -= next - cert;
  cert = next;

  /* save the (wrapped) issuer */
  io->u.item.issuer.data = cert;
  nss_ckmk_DERUnwrap(cert, certSize, &thisEntrySize, &next);
  io->u.item.issuer.size = next - cert;
  certSize -= io->u.item.issuer.size;
  cert = next;

  /* skip the OID */
  nss_ckmk_DERUnwrap(cert, certSize, &thisEntrySize, &next);
  certSize -= next - cert;
  cert = next;

  /* save the (wrapped) subject */
  io->u.item.subject.data = cert;
  nss_ckmk_DERUnwrap(cert, certSize, &thisEntrySize, &next);
  io->u.item.subject.size = next - cert;
  certSize -= io->u.item.subject.size;
  cert = next;
}

static void 
ckmk_fetchModulus
(
  ckmkInternalObject *io
)
{
  NSSItem item;
  PRInt32 modLen;
  CK_RV error;

  /* we can't reliably get the modulus for private keys through CSSM (sigh).
   * For NSS this is OK because we really only use this to get the modulus
   * length (unless we are trying to get a public key from a private keys, 
   * something CSSM ALSO does not do!).
   */
  error = ckmk_GetDataAttribute(io, kSecKeyKeySizeInBits, &item, 
                        "Key Fetch Modulus");
  if (CKR_OK != error) {
    return;
  }

  modLen = *(PRInt32 *)item.data;
  modLen = modLen/8; /* convert from bits to bytes */

  nss_ZFreeIf(item.data);
  io->u.item.modulus.data = nss_ZNEWARRAY(NULL, char, modLen);
  if (NULL == io->u.item.modulus.data) {
    return;
  }
  *(char *)io->u.item.modulus.data = 0x80; /* fake NSS out or it will 
                                              * drop the first byte */
  io->u.item.modulus.size = modLen;
  return;
}

const NSSItem *
ckmk_FetchCertAttribute
(
  ckmkInternalObject *io,
  CK_ATTRIBUTE_TYPE type,
  CK_RV *pError
)
{
  ckmkItemObject *item = &io->u.item;
  *pError = CKR_OK;
  switch(type) {
  case CKA_CLASS:
    return &ckmk_certClassItem;
  case CKA_TOKEN:
  case CKA_MODIFIABLE:
    return &ckmk_trueItem;
  case CKA_PRIVATE:
    return &ckmk_falseItem;
  case CKA_CERTIFICATE_TYPE:
    return &ckmk_x509Item;
  case CKA_LABEL:
    CKMK_HANDLE_OPT_DATA_ITEM(io, kSecLabelItemAttr, label, item, *pError,
                              "Cert:Label attr")
  case CKA_SUBJECT:
    /* OK, well apple does provide an subject and issuer attribute, but they
     * decided to cannonicalize that value. Probably a good move for them,
     * but makes it useless for most users of PKCS #11.. Get the real subject
     * from the certificate */
    if (0 == item->derCert.size) {
      ckmk_fetchCert(io);
    }
    return &item->subject;
  case CKA_ISSUER:
    if (0 == item->derCert.size) {
      ckmk_fetchCert(io);
    }
    return &item->issuer;
  case CKA_SERIAL_NUMBER:
    CKMK_HANDLE_DATA_ITEM(io, kSecSerialNumberItemAttr, serial, item, *pError,
                          "Cert:Serial Number attr")
  case CKA_VALUE:
    if (0 == item->derCert.size) {
      ckmk_fetchCert(io);
    }
    return &item->derCert;
  case CKA_ID:
    CKMK_HANDLE_OPT_DATA_ITEM(io, kSecPublicKeyHashItemAttr, id, item, *pError,
                              "Cert:ID attr")
  default:
    *pError = CKR_ATTRIBUTE_TYPE_INVALID;
    break;
  }
  return NULL;
}

const NSSItem *
ckmk_FetchPubKeyAttribute
(
  ckmkInternalObject *io, 
  CK_ATTRIBUTE_TYPE type,
  CK_RV *pError
)
{
  ckmkItemObject *item = &io->u.item;
  *pError = CKR_OK;
  
  switch(type) {
  case CKA_CLASS:
    return &ckmk_pubKeyClassItem;
  case CKA_TOKEN:
  case CKA_LOCAL:
    return &ckmk_trueItem;
  case CKA_KEY_TYPE:
    return &ckmk_rsaItem;
  case CKA_LABEL:
    CKMK_HANDLE_OPT_DATA_ITEM(io, kSecKeyPrintName, label, item, *pError,
                              "PubKey:Label attr")
  case CKA_ENCRYPT:
    CKMK_HANDLE_BOOL_ITEM(io, kSecKeyEncrypt, encrypt, item, *pError,
                              "PubKey:Encrypt attr")
  case CKA_VERIFY:
    CKMK_HANDLE_BOOL_ITEM(io, kSecKeyVerify, verify, item, *pError,
                              "PubKey:Verify attr")
  case CKA_VERIFY_RECOVER:
    CKMK_HANDLE_BOOL_ITEM(io, kSecKeyVerifyRecover, verifyRecover, 
                          item, *pError, "PubKey:VerifyRecover attr")
  case CKA_PRIVATE:
    CKMK_HANDLE_BOOL_ITEM(io, kSecKeyPrivate, private, item, *pError,
                              "PubKey:Private attr")
  case CKA_MODIFIABLE:
    CKMK_HANDLE_BOOL_ITEM(io, kSecKeyModifiable, modify, item, *pError,
                              "PubKey:Modify attr")
  case CKA_DERIVE:
    CKMK_HANDLE_BOOL_ITEM(io, kSecKeyDerive, derive, item, *pError,
                              "PubKey:Derive attr")
  case CKA_WRAP:
    CKMK_HANDLE_BOOL_ITEM(io, kSecKeyWrap, wrap, item, *pError,
                              "PubKey:Wrap attr")
  case CKA_SUBJECT:
    CKMK_HANDLE_OPT_DATA_ITEM(io, kSecSubjectItemAttr, subject, item, *pError,
                              "PubKey:Subect attr")
  case CKA_MODULUS:
    return &ckmk_emptyItem;
  case CKA_PUBLIC_EXPONENT:
    return &ckmk_emptyItem;
  case CKA_ID:
    CKMK_HANDLE_OPT_DATA_ITEM(io, kSecKeyLabel, id, item, *pError,
                              "PubKey:ID attr")
  default:
    *pError = CKR_ATTRIBUTE_TYPE_INVALID;
    break;
  }
  return NULL;
}

const NSSItem *
ckmk_FetchPrivKeyAttribute
(
  ckmkInternalObject *io, 
  CK_ATTRIBUTE_TYPE type,
  CK_RV *pError
)
{
  ckmkItemObject *item = &io->u.item;
  *pError = CKR_OK;

  switch(type) {
  case CKA_CLASS:
    return &ckmk_privKeyClassItem;
  case CKA_TOKEN:
  case CKA_LOCAL:
    return &ckmk_trueItem;
  case CKA_SENSITIVE:
  case CKA_EXTRACTABLE: /* will probably move in the future */
  case CKA_ALWAYS_SENSITIVE:
  case CKA_NEVER_EXTRACTABLE:
    return &ckmk_falseItem;
  case CKA_KEY_TYPE:
    return &ckmk_rsaItem;
  case CKA_LABEL:
    CKMK_HANDLE_OPT_DATA_ITEM(io, kSecKeyPrintName, label, item, *pError,
                              "PrivateKey:Label attr")
  case CKA_DECRYPT:
    CKMK_HANDLE_BOOL_ITEM(io, kSecKeyDecrypt, decrypt, item, *pError,
                              "PrivateKey:Decrypt attr") 
  case CKA_SIGN:
    CKMK_HANDLE_BOOL_ITEM(io, kSecKeySign, sign, item, *pError,
                              "PrivateKey:Sign attr") 
  case CKA_SIGN_RECOVER:
    CKMK_HANDLE_BOOL_ITEM(io, kSecKeySignRecover, signRecover, item, *pError,
                              "PrivateKey:Sign Recover attr") 
  case CKA_PRIVATE:
    CKMK_HANDLE_BOOL_ITEM(io, kSecKeyPrivate, private, item, *pError,
                              "PrivateKey:Private attr") 
  case CKA_MODIFIABLE:
    CKMK_HANDLE_BOOL_ITEM(io, kSecKeyModifiable, modify, item, *pError,
                              "PrivateKey:Modify attr") 
  case CKA_DERIVE:
    CKMK_HANDLE_BOOL_ITEM(io, kSecKeyDerive, derive, item, *pError,
                              "PrivateKey:Derive attr")
  case CKA_UNWRAP:
    CKMK_HANDLE_BOOL_ITEM(io, kSecKeyUnwrap, unwrap, item, *pError,
                              "PrivateKey:Unwrap attr") 
  case CKA_SUBJECT:
    CKMK_HANDLE_OPT_DATA_ITEM(io, kSecSubjectItemAttr, subject, item, *pError,
                              "PrivateKey:Subject attr")
  case CKA_MODULUS:
    if (0 == item->modulus.size) {
      ckmk_fetchModulus(io);
    }
    return &item->modulus;
  case CKA_PUBLIC_EXPONENT:
    return &ckmk_emptyItem;
#ifdef notdef
  /* the following are sensitive attributes. We could implement them for 
   * sensitive keys using the key export function, but it's better to
   * just support wrap through this token. That will more reliably allow us
   * to export any private key that is truly exportable.
   */
  case CKA_PRIVATE_EXPONENT:
    CKMK_HANDLE_DATA_ITEM(io, kSecPrivateExponentItemAttr, privateExponent, 
                          item, *pError)
  case CKA_PRIME_1:
    CKMK_HANDLE_DATA_ITEM(io, kSecPrime1ItemAttr, prime1, item, *pError)
  case CKA_PRIME_2:
    CKMK_HANDLE_DATA_ITEM(io, kSecPrime2ItemAttr, prime2, item, *pError)
  case CKA_EXPONENT_1:
    CKMK_HANDLE_DATA_ITEM(io, kSecExponent1ItemAttr, exponent1, item, *pError)
  case CKA_EXPONENT_2:
    CKMK_HANDLE_DATA_ITEM(io, kSecExponent2ItemAttr, exponent2, item, *pError)
  case CKA_COEFFICIENT:
    CKMK_HANDLE_DATA_ITEM(io, kSecCoefficientItemAttr, coefficient, 
                          item, *pError)
#endif
  case CKA_ID:
    CKMK_HANDLE_OPT_DATA_ITEM(io, kSecKeyLabel, id, item, *pError,
                              "PrivateKey:ID attr")
  default:
    *pError = CKR_ATTRIBUTE_TYPE_INVALID;
    return NULL;
  }
}

const NSSItem *
nss_ckmk_FetchAttribute
(
  ckmkInternalObject *io, 
  CK_ATTRIBUTE_TYPE type,
  CK_RV *pError
)
{
  CK_ULONG i;
  const NSSItem * value = NULL;

  if (io->type == ckmkRaw) {
    for( i = 0; i < io->u.raw.n; i++ ) {
      if( type == io->u.raw.types[i] ) {
        return &io->u.raw.items[i];
      }
    }
    *pError = CKR_ATTRIBUTE_TYPE_INVALID;
    return NULL;
  }
  /* deal with the common attributes */
  switch (io->objClass) {
  case CKO_CERTIFICATE:
   value = ckmk_FetchCertAttribute(io, type, pError); 
   break;
  case CKO_PRIVATE_KEY:
   value = ckmk_FetchPrivKeyAttribute(io, type, pError); 
   break;
  case CKO_PUBLIC_KEY:
   value = ckmk_FetchPubKeyAttribute(io, type, pError); 
   break;
  default:
    *pError = CKR_OBJECT_HANDLE_INVALID;
    return NULL;
 }

#ifdef DEBUG
  if (CKA_ID == type) {
    itemdump("id: ", value->data, value->size, *pError);
  }
#endif
  return value;
}

static void 
ckmk_removeObjectFromHash
(
  ckmkInternalObject *io
);

/*
 *
 * These are the MSObject functions we need to implement
 *
 * Finalize - unneeded (actually we should clean up the hashtables)
 * Destroy 
 * IsTokenObject - CK_TRUE
 * GetAttributeCount
 * GetAttributeTypes
 * GetAttributeSize
 * GetAttribute
 * SetAttribute
 * GetObjectSize
 */

static CK_RV
ckmk_mdObject_Destroy
(
  NSSCKMDObject *mdObject,
  NSSCKFWObject *fwObject,
  NSSCKMDSession *mdSession,
  NSSCKFWSession *fwSession,
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance
)
{
  ckmkInternalObject *io = (ckmkInternalObject *)mdObject->etc;
  OSStatus macErr;

  if (ckmkRaw == io->type) {
    /* there is not 'object write protected' error, use the next best thing */
    return CKR_TOKEN_WRITE_PROTECTED;
  }

  /* This API is done well. The following 4 lines are the complete apple
   * specific part of this implementation */
  macErr = SecKeychainItemDelete(io->u.item.itemRef);
  if (noErr != macErr) {
    CKMK_MACERR("Delete object", macErr);
  }

  /* remove it from the hash */
  ckmk_removeObjectFromHash(io);

  /* free the puppy.. */
  nss_ckmk_DestroyInternalObject(io);

  return CKR_OK;
}

static CK_BBOOL
ckmk_mdObject_IsTokenObject
(
  NSSCKMDObject *mdObject,
  NSSCKFWObject *fwObject,
  NSSCKMDSession *mdSession,
  NSSCKFWSession *fwSession,
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance
)
{
  return CK_TRUE;
}

static CK_ULONG
ckmk_mdObject_GetAttributeCount
(
  NSSCKMDObject *mdObject,
  NSSCKFWObject *fwObject,
  NSSCKMDSession *mdSession,
  NSSCKFWSession *fwSession,
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  CK_RV *pError
)
{
  ckmkInternalObject *io = (ckmkInternalObject *)mdObject->etc;

  if (ckmkRaw == io->type) {
     return io->u.raw.n;
  }
  switch (io->objClass) {
  case CKO_CERTIFICATE:
    return certAttrsCount;
  case CKO_PUBLIC_KEY:
    return pubKeyAttrsCount;
  case CKO_PRIVATE_KEY:
    return privKeyAttrsCount;
  default:
    break;
  }
  return 0;
}

static CK_RV
ckmk_mdObject_GetAttributeTypes
(
  NSSCKMDObject *mdObject,
  NSSCKFWObject *fwObject,
  NSSCKMDSession *mdSession,
  NSSCKFWSession *fwSession,
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  CK_ATTRIBUTE_TYPE_PTR typeArray,
  CK_ULONG ulCount
)
{
  ckmkInternalObject *io = (ckmkInternalObject *)mdObject->etc;
  CK_ULONG i;
  CK_RV error = CKR_OK;
  const CK_ATTRIBUTE_TYPE *attrs = NULL;
  CK_ULONG size = ckmk_mdObject_GetAttributeCount(
                        mdObject, fwObject, mdSession, fwSession, 
                        mdToken, fwToken, mdInstance, fwInstance, &error);

  if( size != ulCount ) {
    return CKR_BUFFER_TOO_SMALL;
  }
  if (io->type == ckmkRaw) {
    attrs = io->u.raw.types;
  } else switch(io->objClass) {
    case CKO_CERTIFICATE:
      attrs = certAttrs;
      break;
    case CKO_PUBLIC_KEY:
      attrs = pubKeyAttrs;
      break;
    case CKO_PRIVATE_KEY:
      attrs = privKeyAttrs;
      break;
    default:
      return CKR_OK;
  }
  
  for( i = 0; i < size; i++) {
    typeArray[i] = attrs[i];
  }

  return CKR_OK;
}

static CK_ULONG
ckmk_mdObject_GetAttributeSize
(
  NSSCKMDObject *mdObject,
  NSSCKFWObject *fwObject,
  NSSCKMDSession *mdSession,
  NSSCKFWSession *fwSession,
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  CK_ATTRIBUTE_TYPE attribute,
  CK_RV *pError
)
{
  ckmkInternalObject *io = (ckmkInternalObject *)mdObject->etc;

  const NSSItem *b;

  b = nss_ckmk_FetchAttribute(io, attribute, pError);

  if ((const NSSItem *)NULL == b) {
    return 0;
  }
  return b->size;
}

static CK_RV
ckmk_mdObject_SetAttribute
(
  NSSCKMDObject *mdObject,
  NSSCKFWObject *fwObject,
  NSSCKMDSession *mdSession,
  NSSCKFWSession *fwSession,
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  CK_ATTRIBUTE_TYPE attribute,
  NSSItem           *value
)
{
  ckmkInternalObject *io = (ckmkInternalObject *)mdObject->etc;
  SecKeychainItemRef itemRef;

  if (io->type == ckmkRaw) {
    return CKR_TOKEN_WRITE_PROTECTED;
  }
  itemRef = io->u.item.itemRef;

  switch (io->objClass) {
  case CKO_PRIVATE_KEY:
  case CKO_PUBLIC_KEY:
    switch (attribute) {
    case CKA_ID:
      ckmk_updateAttribute(itemRef, kSecKeyLabel, 
                              value->data, value->size, "Set Attr Key ID");
#ifdef DEBUG
      itemdump("key id: ", value->data, value->size, CKR_OK);
#endif
      break;
    case CKA_LABEL:
      ckmk_updateAttribute(itemRef, kSecKeyPrintName, value->data, 
                              value->size, "Set Attr Key Label");
      break;
    default:
      break;
    }
    break;

  case CKO_CERTIFICATE:
    switch (attribute) {
    case CKA_ID:
      ckmk_updateAttribute(itemRef, kSecPublicKeyHashItemAttr, 
                              value->data, value->size, "Set Attr Cert ID");
      break;
    case CKA_LABEL:
      ckmk_updateAttribute(itemRef, kSecLabelItemAttr, value->data, 
                              value->size, "Set Attr Cert Label");
      break;
    default:
      break;
    }
    break;

   default:
    break;
  } 
  return CKR_OK;
}

static NSSCKFWItem
ckmk_mdObject_GetAttribute
(
  NSSCKMDObject *mdObject,
  NSSCKFWObject *fwObject,
  NSSCKMDSession *mdSession,
  NSSCKFWSession *fwSession,
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  CK_ATTRIBUTE_TYPE attribute,
  CK_RV *pError
)
{
  NSSCKFWItem mdItem;
  ckmkInternalObject *io = (ckmkInternalObject *)mdObject->etc;

  mdItem.needsFreeing = PR_FALSE;
  mdItem.item = (NSSItem*)nss_ckmk_FetchAttribute(io, attribute, pError);


  return mdItem;
}

static CK_ULONG
ckmk_mdObject_GetObjectSize
(
  NSSCKMDObject *mdObject,
  NSSCKFWObject *fwObject,
  NSSCKMDSession *mdSession,
  NSSCKFWSession *fwSession,
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  CK_RV *pError
)
{
  CK_ULONG rv = 1;

  /* size is irrelevant to this token */
  return rv;
}

static const NSSCKMDObject
ckmk_prototype_mdObject = {
  (void *)NULL, /* etc */
  NULL, /* Finalize */
  ckmk_mdObject_Destroy,
  ckmk_mdObject_IsTokenObject,
  ckmk_mdObject_GetAttributeCount,
  ckmk_mdObject_GetAttributeTypes,
  ckmk_mdObject_GetAttributeSize,
  ckmk_mdObject_GetAttribute,
  NULL, /* FreeAttribute */
  ckmk_mdObject_SetAttribute,
  ckmk_mdObject_GetObjectSize,
  (void *)NULL /* null terminator */
};

static nssHash *ckmkInternalObjectHash = NULL;

NSS_IMPLEMENT NSSCKMDObject *
nss_ckmk_CreateMDObject
(
  NSSArena *arena,
  ckmkInternalObject *io,
  CK_RV *pError
)
{
  if ((nssHash *)NULL == ckmkInternalObjectHash) {
    ckmkInternalObjectHash = nssHash_CreateItem(NULL, 10);
  }
  if (ckmkItem == io->type) {
    /* the hash key, not a cryptographic key */
    NSSItem *key = &io->hashKey;
    ckmkInternalObject *old_o = NULL;

    if (key->size == 0) {
      ckmk_FetchHashKey(io);
    }
    old_o = (ckmkInternalObject *) 
              nssHash_Lookup(ckmkInternalObjectHash, key);
    if (!old_o) {
      nssHash_Add(ckmkInternalObjectHash, key, io);
    } else if (old_o != io) {
      nss_ckmk_DestroyInternalObject(io);
      io = old_o;
    }
  }
    
  if ( (void*)NULL == io->mdObject.etc) {
    (void) nsslibc_memcpy(&io->mdObject,&ckmk_prototype_mdObject,
                                        sizeof(ckmk_prototype_mdObject));
    io->mdObject.etc = (void *)io;
  }
  return &io->mdObject;
}

static void
ckmk_removeObjectFromHash
(
  ckmkInternalObject *io
)
{
  NSSItem *key = &io->hashKey;

  if ((nssHash *)NULL == ckmkInternalObjectHash) {
    return;
  }
  if (key->size == 0) {
    ckmk_FetchHashKey(io);
  }
  nssHash_Remove(ckmkInternalObjectHash, key);
  return;
}


void
nss_ckmk_DestroyInternalObject
(
  ckmkInternalObject *io
)
{
  switch (io->type) {
  case ckmkRaw:
    return;
  case ckmkItem:
    nss_ZFreeIf(io->u.item.modify.data);
    nss_ZFreeIf(io->u.item.private.data);
    nss_ZFreeIf(io->u.item.encrypt.data);
    nss_ZFreeIf(io->u.item.decrypt.data);
    nss_ZFreeIf(io->u.item.derive.data);
    nss_ZFreeIf(io->u.item.sign.data);
    nss_ZFreeIf(io->u.item.signRecover.data);
    nss_ZFreeIf(io->u.item.verify.data);
    nss_ZFreeIf(io->u.item.verifyRecover.data);
    nss_ZFreeIf(io->u.item.wrap.data);
    nss_ZFreeIf(io->u.item.unwrap.data);
    nss_ZFreeIf(io->u.item.label.data);
    /*nss_ZFreeIf(io->u.item.subject.data); */
    /*nss_ZFreeIf(io->u.item.issuer.data); */
    nss_ZFreeIf(io->u.item.serial.data);
    nss_ZFreeIf(io->u.item.modulus.data);
    nss_ZFreeIf(io->u.item.exponent.data);
    nss_ZFreeIf(io->u.item.privateExponent.data);
    nss_ZFreeIf(io->u.item.prime1.data);
    nss_ZFreeIf(io->u.item.prime2.data);
    nss_ZFreeIf(io->u.item.exponent1.data);
    nss_ZFreeIf(io->u.item.exponent2.data);
    nss_ZFreeIf(io->u.item.coefficient.data);
    break;
  }
  nss_ZFreeIf(io);
  return;
}


static ckmkInternalObject *
nss_ckmk_NewInternalObject
(
  CK_OBJECT_CLASS objClass,
  SecKeychainItemRef itemRef,
  SecItemClass itemClass,
  CK_RV *pError
)
{
  ckmkInternalObject *io = nss_ZNEW(NULL, ckmkInternalObject);

  if ((ckmkInternalObject *)NULL == io) {
    *pError = CKR_HOST_MEMORY;
    return io;
  }
  io->type = ckmkItem;
  io->objClass = objClass;
  io->u.item.itemRef = itemRef;
  io->u.item.itemClass = itemClass;
  return io;
}

/*
 * Apple doesn't alway have a default keyChain set by the OS, use the 
 * SearchList to try to find one.
 */
static CK_RV
ckmk_GetSafeDefaultKeychain
(
  SecKeychainRef *keychainRef
)
{
  OSStatus macErr;
  CFArrayRef searchList = 0;
  CK_RV error = CKR_OK;

  macErr = SecKeychainCopyDefault(keychainRef);
  if (noErr != macErr) {
    int searchCount = 0;
    if (errSecNoDefaultKeychain != macErr) {
      CKMK_MACERR("Getting default key chain", macErr);
      error = CKR_GENERAL_ERROR;
      goto loser;
    }
    /* ok, we don't have a default key chain, find one */
    macErr = SecKeychainCopySearchList(&searchList);
    if (noErr != macErr) {
      CKMK_MACERR("failed to find a keyring searchList", macErr);
      error = CKR_DEVICE_REMOVED;
      goto loser;
    }
    searchCount = CFArrayGetCount(searchList);
    if (searchCount < 1) {
      error = CKR_DEVICE_REMOVED;
      goto loser;
    }
    *keychainRef = 
        (SecKeychainRef)CFRetain(CFArrayGetValueAtIndex(searchList, 0));
    if (0 == *keychainRef) {
      error = CKR_DEVICE_REMOVED;
      goto loser;
    }
    /* should we set it as default? */
  }
loser:
  if (0 != searchList) {
    CFRelease(searchList);
  }
  return error;
}
static ckmkInternalObject *
nss_ckmk_CreateCertificate
(
  NSSCKFWSession *fwSession,
  CK_ATTRIBUTE_PTR pTemplate,
  CK_ULONG ulAttributeCount,
  CK_RV *pError
)
{
  NSSItem value;
  ckmkInternalObject *io = NULL; 
  OSStatus macErr;
  SecCertificateRef certRef;
  SecKeychainItemRef itemRef;
  SecKeychainRef keychainRef;
  CSSM_DATA certData;

  *pError = nss_ckmk_GetAttribute(CKA_VALUE, pTemplate, 
                                  ulAttributeCount, &value);
  if (CKR_OK != *pError) {
    goto loser;
  }

  certData.Data = value.data;
  certData.Length = value.size;
  macErr = SecCertificateCreateFromData(&certData, CSSM_CERT_X_509v3, 
                              CSSM_CERT_ENCODING_BER, &certRef);
  if (noErr != macErr) {
    CKMK_MACERR("Create cert from data Failed", macErr);
    *pError = CKR_GENERAL_ERROR; /* need to map macErr */
    goto loser;
  }

  *pError = ckmk_GetSafeDefaultKeychain(&keychainRef);
  if (CKR_OK != *pError) {
    goto loser;
  }

  macErr = SecCertificateAddToKeychain( certRef, keychainRef);
  itemRef = (SecKeychainItemRef) certRef;
  if (errSecDuplicateItem != macErr) {
    NSSItem keyID = { NULL, 0 };
    char *nickname = NULL;
    CK_RV dummy;

    if (noErr != macErr) {
      CKMK_MACERR("Add cert to keychain Failed", macErr);
      *pError = CKR_GENERAL_ERROR; /* need to map macErr */
      goto loser;
    }
    /* these two are optional */
    nickname = nss_ckmk_GetStringAttribute(CKA_LABEL, pTemplate, 
                                  ulAttributeCount, &dummy);
    /* we've added a new one, update the attributes in the key ring */
    if (nickname) {
      ckmk_updateAttribute(itemRef, kSecLabelItemAttr, nickname, 
                              strlen(nickname)+1, "Modify Cert Label");
      nss_ZFreeIf(nickname);
    }
    dummy = nss_ckmk_GetAttribute(CKA_ID, pTemplate, 
                                  ulAttributeCount, &keyID);
    if (CKR_OK == dummy) {
      dummy = ckmk_updateAttribute(itemRef, kSecPublicKeyHashItemAttr, 
                              keyID.data, keyID.size, "Modify Cert ID");
    }
  }

  io = nss_ckmk_NewInternalObject(CKO_CERTIFICATE, itemRef, 
                                  kSecCertificateItemClass, pError);
  if ((ckmkInternalObject *)NULL != io) {
    itemRef = 0;
  }

loser:
  if (0 != itemRef) {
    CFRelease(itemRef);
  }
  if (0 != keychainRef) {
    CFRelease(keychainRef);
  }

  return io;
}

/*
 * PKCS #8 attributes
 */
struct ckmk_AttributeStr {
    SECItem attrType;
    SECItem *attrValue;
};
typedef struct ckmk_AttributeStr ckmk_Attribute;

/*
** A PKCS#8 private key info object
*/
struct PrivateKeyInfoStr {
    PLArenaPool *arena;
    SECItem version;
    SECAlgorithmID algorithm;
    SECItem privateKey;
    ckmk_Attribute **attributes;
};
typedef struct PrivateKeyInfoStr PrivateKeyInfo;

const SEC_ASN1Template ckmk_RSAPrivateKeyTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(RSAPrivateKey) },
    { SEC_ASN1_INTEGER, offsetof(RSAPrivateKey,version) },
    { SEC_ASN1_INTEGER, offsetof(RSAPrivateKey,modulus) },
    { SEC_ASN1_INTEGER, offsetof(RSAPrivateKey,publicExponent) },
    { SEC_ASN1_INTEGER, offsetof(RSAPrivateKey,privateExponent) },
    { SEC_ASN1_INTEGER, offsetof(RSAPrivateKey,prime1) },
    { SEC_ASN1_INTEGER, offsetof(RSAPrivateKey,prime2) },
    { SEC_ASN1_INTEGER, offsetof(RSAPrivateKey,exponent1) },
    { SEC_ASN1_INTEGER, offsetof(RSAPrivateKey,exponent2) },
    { SEC_ASN1_INTEGER, offsetof(RSAPrivateKey,coefficient) },
    { 0 }                                                                     
}; 

const SEC_ASN1Template ckmk_AttributeTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(ckmk_Attribute) },
    { SEC_ASN1_OBJECT_ID, offsetof(ckmk_Attribute, attrType) },
    { SEC_ASN1_SET_OF, offsetof(ckmk_Attribute, attrValue), 
        SEC_AnyTemplate },
    { 0 }
};

const SEC_ASN1Template ckmk_SetOfAttributeTemplate[] = {
    { SEC_ASN1_SET_OF, 0, ckmk_AttributeTemplate },
};

SEC_ASN1_MKSUB(SECOID_AlgorithmIDTemplate)

/* ASN1 Templates for new decoder/encoder */
const SEC_ASN1Template ckmk_PrivateKeyInfoTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(PrivateKeyInfo) },
    { SEC_ASN1_INTEGER, offsetof(PrivateKeyInfo,version) },
    { SEC_ASN1_INLINE | SEC_ASN1_XTRN, offsetof(PrivateKeyInfo,algorithm),
        SEC_ASN1_SUB(SECOID_AlgorithmIDTemplate) },
    { SEC_ASN1_OCTET_STRING, offsetof(PrivateKeyInfo,privateKey) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 0,
        offsetof(PrivateKeyInfo, attributes), ckmk_SetOfAttributeTemplate },
    { 0 }
};

#define CKMK_PRIVATE_KEY_INFO_VERSION 0
static CK_RV
ckmk_CreateRSAKeyBlob
(
  RSAPrivateKey *lk,
  NSSItem *keyBlob
)
{
  PrivateKeyInfo *pki = NULL;
  PLArenaPool *arena = NULL;
  SECOidTag algorithm = SEC_OID_UNKNOWN;
  void *dummy;
  SECStatus rv;
  SECItem *encodedKey = NULL;
  CK_RV error = CKR_OK;

  arena = PORT_NewArena(2048);         /* XXX different size? */
  if(!arena) {
    error = CKR_HOST_MEMORY;
    goto loser;
  }

  pki = (PrivateKeyInfo*)PORT_ArenaZAlloc(arena, sizeof(PrivateKeyInfo));
  if(!pki) {
    error = CKR_HOST_MEMORY;
    goto loser;
  }
  pki->arena = arena;

  dummy = SEC_ASN1EncodeItem(arena, &pki->privateKey, lk,
                             ckmk_RSAPrivateKeyTemplate);
  algorithm = SEC_OID_PKCS1_RSA_ENCRYPTION;
 
  if (!dummy) {
    error = CKR_DEVICE_ERROR; /* should map NSS SECError */
    goto loser;
  }
    
  rv = SECOID_SetAlgorithmID(arena, &pki->algorithm, algorithm, 
                             (SECItem*)NULL);
  if (rv != SECSuccess) {
    error = CKR_DEVICE_ERROR; /* should map NSS SECError */
    goto loser;
  }

  dummy = SEC_ASN1EncodeInteger(arena, &pki->version, 
                                CKMK_PRIVATE_KEY_INFO_VERSION);
  if (!dummy) {
    error = CKR_DEVICE_ERROR; /* should map NSS SECError */
    goto loser;
  }

  encodedKey = SEC_ASN1EncodeItem(NULL, NULL, pki, 
                                  ckmk_PrivateKeyInfoTemplate);
  if (!encodedKey) {
    error = CKR_DEVICE_ERROR;
    goto loser;
  }

  keyBlob->data = nss_ZNEWARRAY(NULL, char, encodedKey->len);
  if (NULL == keyBlob->data) {
    error = CKR_HOST_MEMORY;
    goto loser;
  }
  nsslibc_memcpy(keyBlob->data, encodedKey->data, encodedKey->len);
  keyBlob->size = encodedKey->len;

loser:
  if(arena) {
    PORT_FreeArena(arena, PR_TRUE);
  }
  if (encodedKey) {
    SECITEM_FreeItem(encodedKey, PR_TRUE);
  }
 
  return error;
}
/*
 * There MUST be a better way to do this. For now, find the key based on the
 * default name Apple gives it once we import.
 */
#define IMPORTED_NAME "Imported Private Key"
static CK_RV
ckmk_FindImportedKey
(
  SecKeychainRef keychainRef,
  SecItemClass itemClass,
  SecKeychainItemRef *outItemRef
)
{
  OSStatus macErr;
  SecKeychainSearchRef searchRef = 0;
  SecKeychainItemRef newItemRef;

  macErr = SecKeychainSearchCreateFromAttributes(keychainRef, itemClass,
           NULL, &searchRef);
  if (noErr != macErr) {
    CKMK_MACERR("Can't search for Key", macErr);
    return CKR_GENERAL_ERROR;
  }
  while (noErr == SecKeychainSearchCopyNext(searchRef, &newItemRef)) {
    SecKeychainAttributeList *attrList = NULL;
    SecKeychainAttributeInfo attrInfo;
    SecItemAttr itemAttr = kSecKeyPrintName;
    PRUint32 attrFormat = 0;
    OSStatus macErr;

    attrInfo.count = 1;
    attrInfo.tag = &itemAttr;
    attrInfo.format = &attrFormat;

    macErr = SecKeychainItemCopyAttributesAndData(newItemRef,
                                &attrInfo, NULL, &attrList, NULL, NULL);
    if (noErr == macErr) {
      if (nsslibc_memcmp(attrList->attr->data, IMPORTED_NAME, 
                         attrList->attr->length, NULL) == 0) {
        *outItemRef = newItemRef;
        CFRelease (searchRef);
        SecKeychainItemFreeAttributesAndData(attrList, NULL);
        return CKR_OK;
      }
      SecKeychainItemFreeAttributesAndData(attrList, NULL);
    }
    CFRelease(newItemRef);
  }
  CFRelease (searchRef);
  return CKR_GENERAL_ERROR; /* we can come up with something better! */
}

static ckmkInternalObject *
nss_ckmk_CreatePrivateKey
(
  NSSCKFWSession *fwSession,
  CK_ATTRIBUTE_PTR pTemplate,
  CK_ULONG ulAttributeCount,
  CK_RV *pError
)
{
  NSSItem attribute;
  RSAPrivateKey lk;
  NSSItem keyID;
  char *nickname = NULL;
  ckmkInternalObject *io = NULL;
  CK_KEY_TYPE keyType;
  OSStatus macErr;
  SecKeychainItemRef itemRef = 0;
  NSSItem keyBlob = { NULL, 0 };
  CFDataRef dataRef = 0;
  SecExternalFormat inputFormat = kSecFormatBSAFE; 
  /*SecExternalFormat inputFormat = kSecFormatOpenSSL;  */
  SecExternalItemType itemType = kSecItemTypePrivateKey;
  SecKeyImportExportParameters keyParams ;
  SecKeychainRef targetKeychain = 0;
  unsigned char zero = 0;
  CK_RV error;
  
  keyParams.version = SEC_KEY_IMPORT_EXPORT_PARAMS_VERSION;
  keyParams.flags = 0;
  keyParams.passphrase = 0;
  keyParams.alertTitle = 0;
  keyParams.alertPrompt = 0;
  keyParams.accessRef = 0; /* default */
  keyParams.keyUsage = 0; /* will get filled in */
  keyParams.keyAttributes = CSSM_KEYATTR_PERMANENT; /* will get filled in */
  keyType = nss_ckmk_GetULongAttribute
                  (CKA_KEY_TYPE, pTemplate, ulAttributeCount, pError);
  if (CKR_OK != *pError) {
    return (ckmkInternalObject *)NULL;
  }
  if (CKK_RSA != keyType) {
    *pError = CKR_ATTRIBUTE_VALUE_INVALID;
    return (ckmkInternalObject *)NULL;
  }
  if (nss_ckmk_GetBoolAttribute(CKA_DECRYPT, 
                                pTemplate, ulAttributeCount, CK_TRUE)) {
    keyParams.keyUsage |= CSSM_KEYUSE_DECRYPT;
  }
  if (nss_ckmk_GetBoolAttribute(CKA_UNWRAP, 
                                pTemplate, ulAttributeCount, CK_TRUE)) {
    keyParams.keyUsage |=  CSSM_KEYUSE_UNWRAP;
  }
  if (nss_ckmk_GetBoolAttribute(CKA_SIGN, 
                                pTemplate, ulAttributeCount, CK_TRUE)) {
    keyParams.keyUsage |= CSSM_KEYUSE_SIGN;
  }
  if (nss_ckmk_GetBoolAttribute(CKA_DERIVE, 
                                pTemplate, ulAttributeCount, CK_FALSE)) {
    keyParams.keyUsage |= CSSM_KEYUSE_DERIVE;
  }
  if (nss_ckmk_GetBoolAttribute(CKA_SENSITIVE, 
                                pTemplate, ulAttributeCount, CK_TRUE)) {
    keyParams.keyAttributes |= CSSM_KEYATTR_SENSITIVE; 
  }
  if (nss_ckmk_GetBoolAttribute(CKA_EXTRACTABLE, 
                                pTemplate, ulAttributeCount, CK_TRUE)) {
    keyParams.keyAttributes |= CSSM_KEYATTR_EXTRACTABLE;
  }

  lk.version.type = siUnsignedInteger;
  lk.version.data = &zero;
  lk.version.len = 1;

  *pError = nss_ckmk_GetAttribute(CKA_MODULUS, pTemplate, 
                                  ulAttributeCount, &attribute);
  if (CKR_OK != *pError) {
    return (ckmkInternalObject *)NULL;
  }
  lk.modulus.type = siUnsignedInteger;
  lk.modulus.data = attribute.data;
  lk.modulus.len = attribute.size;

  *pError = nss_ckmk_GetAttribute(CKA_PUBLIC_EXPONENT, pTemplate, 
                                  ulAttributeCount, &attribute);
  if (CKR_OK != *pError) {
    return (ckmkInternalObject *)NULL;
  }
  lk.publicExponent.type = siUnsignedInteger;
  lk.publicExponent.data = attribute.data;
  lk.publicExponent.len = attribute.size;

  *pError = nss_ckmk_GetAttribute(CKA_PRIVATE_EXPONENT, pTemplate, 
                                  ulAttributeCount, &attribute);
  if (CKR_OK != *pError) {
    return (ckmkInternalObject *)NULL;
  }
  lk.privateExponent.type = siUnsignedInteger;
  lk.privateExponent.data = attribute.data;
  lk.privateExponent.len = attribute.size;

  *pError = nss_ckmk_GetAttribute(CKA_PRIME_1, pTemplate, 
                                  ulAttributeCount, &attribute);
  if (CKR_OK != *pError) {
    return (ckmkInternalObject *)NULL;
  }
  lk.prime1.type = siUnsignedInteger;
  lk.prime1.data = attribute.data;
  lk.prime1.len = attribute.size;

  *pError = nss_ckmk_GetAttribute(CKA_PRIME_2, pTemplate, 
                                  ulAttributeCount, &attribute);
  if (CKR_OK != *pError) {
    return (ckmkInternalObject *)NULL;
  }
  lk.prime2.type = siUnsignedInteger;
  lk.prime2.data = attribute.data;
  lk.prime2.len = attribute.size;

  *pError = nss_ckmk_GetAttribute(CKA_EXPONENT_1, pTemplate, 
                                  ulAttributeCount, &attribute);
  if (CKR_OK != *pError) {
    return (ckmkInternalObject *)NULL;
  }
  lk.exponent1.type = siUnsignedInteger;
  lk.exponent1.data = attribute.data;
  lk.exponent1.len = attribute.size;

  *pError = nss_ckmk_GetAttribute(CKA_EXPONENT_2, pTemplate, 
                                  ulAttributeCount, &attribute);
  if (CKR_OK != *pError) {
    return (ckmkInternalObject *)NULL;
  }
  lk.exponent2.type = siUnsignedInteger;
  lk.exponent2.data = attribute.data;
  lk.exponent2.len = attribute.size;

  *pError = nss_ckmk_GetAttribute(CKA_COEFFICIENT, pTemplate, 
                                  ulAttributeCount, &attribute);
  if (CKR_OK != *pError) {
    return (ckmkInternalObject *)NULL;
  }
  lk.coefficient.type = siUnsignedInteger;
  lk.coefficient.data = attribute.data;
  lk.coefficient.len = attribute.size;

  /* ASN1 Encode the pkcs8 structure... look at softoken to see how this
   * is done... */
  error = ckmk_CreateRSAKeyBlob(&lk, &keyBlob);
  if (CKR_OK != error) {
    goto loser;
  }

  dataRef = CFDataCreate(NULL, (UInt8 *)keyBlob.data, keyBlob.size);
  if (0 == dataRef) {
    *pError = CKR_HOST_MEMORY;
    goto loser;
  }

  *pError == ckmk_GetSafeDefaultKeychain(&targetKeychain);
  if (CKR_OK != *pError) {
    goto loser;
  }


  /* the itemArray that is returned is useless. the item does not
   * is 'not on the key chain' so none of the modify calls work on it.
   * It also has a key that isn't the same key as the one in the actual
   * key chain. In short it isn't the item we want, and it gives us zero
   * information about the item we want, so don't even bother with it...
   */
  macErr = SecKeychainItemImport(dataRef, NULL, &inputFormat, &itemType, 0,
        &keyParams, targetKeychain, NULL);
  if (noErr != macErr) {
    CKMK_MACERR("Import Private Key", macErr);
    *pError = CKR_GENERAL_ERROR;
    goto loser;
  }

  *pError = ckmk_FindImportedKey(targetKeychain, 
                                 CSSM_DL_DB_RECORD_PRIVATE_KEY,
                                 &itemRef);
  if (CKR_OK != *pError) {
#ifdef DEBUG
    fprintf(stderr,"couldn't find key in keychain \n");
#endif
    goto loser;
  }


  /* set the CKA_ID and  the CKA_LABEL */
  error = nss_ckmk_GetAttribute(CKA_ID, pTemplate, 
                                  ulAttributeCount, &keyID);
  if (CKR_OK == error) {
    error = ckmk_updateAttribute(itemRef, kSecKeyLabel, 
                              keyID.data, keyID.size, "Modify Key ID");
#ifdef DEBUG
    itemdump("key id: ", keyID.data, keyID.size, error);
#endif
  }
  nickname = nss_ckmk_GetStringAttribute(CKA_LABEL, pTemplate, 
                                  ulAttributeCount, &error);
  if (nickname) {
    ckmk_updateAttribute(itemRef, kSecKeyPrintName, nickname, 
                              strlen(nickname)+1, "Modify Key Label");
  } else {
#define DEFAULT_NICKNAME "NSS Imported Key"
    ckmk_updateAttribute(itemRef, kSecKeyPrintName, DEFAULT_NICKNAME, 
                              sizeof(DEFAULT_NICKNAME), "Modify Key Label");
  }

  io = nss_ckmk_NewInternalObject(CKO_PRIVATE_KEY, itemRef, 
                                  CSSM_DL_DB_RECORD_PRIVATE_KEY, pError);
  if ((ckmkInternalObject *)NULL == io) {
    CFRelease(itemRef);
  }

  return io;

loser:
  /* free the key blob */
  if (keyBlob.data) {
    nss_ZFreeIf(keyBlob.data);
  }
  if (0 != targetKeychain) {
    CFRelease(targetKeychain);
  }
  if (0 != dataRef) {
    CFRelease(dataRef);
  }
  return io;
}


NSS_EXTERN NSSCKMDObject *
nss_ckmk_CreateObject
(
  NSSCKFWSession *fwSession,
  CK_ATTRIBUTE_PTR pTemplate,
  CK_ULONG ulAttributeCount,
  CK_RV *pError
)
{
  CK_OBJECT_CLASS objClass;
  ckmkInternalObject *io;
  CK_BBOOL isToken;

  /*
   * only create token objects
   */
  isToken = nss_ckmk_GetBoolAttribute(CKA_TOKEN, pTemplate, 
                                      ulAttributeCount, CK_FALSE);
  if (!isToken) {
    *pError = CKR_ATTRIBUTE_VALUE_INVALID;
    return (NSSCKMDObject *) NULL;
  }

  /*
   * only create keys and certs.
   */
  objClass = nss_ckmk_GetULongAttribute(CKA_CLASS, pTemplate, 
                                        ulAttributeCount, pError);
  if (CKR_OK != *pError) {
    return (NSSCKMDObject *) NULL;
  }
#ifdef notdef
  if (objClass == CKO_PUBLIC_KEY) {
    return CKR_OK; /* fake public key creation, happens as a side effect of
                    * private key creation */
  }
#endif
  if (objClass == CKO_CERTIFICATE) {
    io = nss_ckmk_CreateCertificate(fwSession, pTemplate, 
                                    ulAttributeCount, pError);
  } else if (objClass == CKO_PRIVATE_KEY) {
    io = nss_ckmk_CreatePrivateKey(fwSession, pTemplate, 
                                   ulAttributeCount, pError);
  } else {
    *pError = CKR_ATTRIBUTE_VALUE_INVALID;
  }

  if ((ckmkInternalObject *)NULL == io) {
    return (NSSCKMDObject *) NULL;
  }
  return nss_ckmk_CreateMDObject(NULL, io, pError);
}
