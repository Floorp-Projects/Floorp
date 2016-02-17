/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ckcapi.h"
#include "nssbase.h"

/*
 * ckcapi/cobject.c
 *
 * This file implements the NSSCKMDObject object for the
 * "nss to capi objects" cryptoki module.
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
const PRUint32 certAttrsCount = NSS_CKCAPI_ARRAY_SIZE(certAttrs);

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
const PRUint32 privKeyAttrsCount = NSS_CKCAPI_ARRAY_SIZE(privKeyAttrs);

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
const PRUint32 pubKeyAttrsCount = NSS_CKCAPI_ARRAY_SIZE(pubKeyAttrs);
static const CK_BBOOL ck_true = CK_TRUE;
static const CK_BBOOL ck_false = CK_FALSE;
static const CK_CERTIFICATE_TYPE ckc_x509 = CKC_X_509;
static const CK_KEY_TYPE ckk_rsa = CKK_RSA;
static const CK_OBJECT_CLASS cko_certificate = CKO_CERTIFICATE;
static const CK_OBJECT_CLASS cko_private_key = CKO_PRIVATE_KEY;
static const CK_OBJECT_CLASS cko_public_key = CKO_PUBLIC_KEY;
static const NSSItem ckcapi_trueItem = { 
  (void *)&ck_true, (PRUint32)sizeof(CK_BBOOL) };
static const NSSItem ckcapi_falseItem = { 
  (void *)&ck_false, (PRUint32)sizeof(CK_BBOOL) };
static const NSSItem ckcapi_x509Item = { 
  (void *)&ckc_x509, (PRUint32)sizeof(CK_CERTIFICATE_TYPE) };
static const NSSItem ckcapi_rsaItem = { 
  (void *)&ckk_rsa, (PRUint32)sizeof(CK_KEY_TYPE) };
static const NSSItem ckcapi_certClassItem = { 
  (void *)&cko_certificate, (PRUint32)sizeof(CK_OBJECT_CLASS) };
static const NSSItem ckcapi_privKeyClassItem = {
  (void *)&cko_private_key, (PRUint32)sizeof(CK_OBJECT_CLASS) };
static const NSSItem ckcapi_pubKeyClassItem = {
  (void *)&cko_public_key, (PRUint32)sizeof(CK_OBJECT_CLASS) };
static const NSSItem ckcapi_emptyItem = { 
  (void *)&ck_true, 0};

/*
 * these are utilities. The chould be moved to a new utilities file.
 */

/*
 * unwrap a single DER value
 */
unsigned char *
nss_ckcapi_DERUnwrap
(
  unsigned char *src, 
  unsigned int size, 
  unsigned int *outSize, 
  unsigned char **next
)
{
  unsigned char *start = src;
  unsigned char *end = src+size;
  unsigned int len = 0;

  /* initialize error condition return values */
  *outSize = 0;
  if (next) {
    *next = src;
  }

  if (size < 2) {
    return start;
  }
  src++; /* skip the tag -- should check it against an expected value! */
  len = (unsigned) *src++;
  if (len & 0x80) {
    unsigned int count = len & 0x7f;
    len = 0;

    if (count+2 > size) {
      return start;
    }
    while (count-- > 0) {
      len = (len << 8) | (unsigned) *src++;
    }
  }
  if (len + (src-start) > size) {
    return start;
  }
  if (next) {
    *next = src+len;
  }
  *outSize = len;

  return src;
}

/*
 * convert a PKCS #11 bytestrin into a CK_ULONG, the byte stream must be
 * less than sizeof (CK_ULONG).
 */
CK_ULONG  
nss_ckcapi_DataToInt
(
  NSSItem *data,
  CK_RV *pError
)
{
  CK_ULONG value = 0;
  unsigned long count = data->size;
  unsigned char *dataPtr = data->data;
  unsigned long size = 0;

  *pError = CKR_OK;

  while (count--) {
    value = value << 8;
    value = value + *dataPtr++;
    if (size || value) {
      size++;
    }
  }
  if (size > sizeof(CK_ULONG)) {
    *pError = CKR_ATTRIBUTE_VALUE_INVALID;
  }
  return value;
}

/*
 * convert a CK_ULONG to a bytestream. Data is stored in the buffer 'buf'
 * and must be at least CK_ULONG. Caller must provide buf.
 */
CK_ULONG  
nss_ckcapi_IntToData
(
  CK_ULONG value,
  NSSItem *data,
  unsigned char *dataPtr,
  CK_RV *pError
)
{
  unsigned long count = 0;
  unsigned long i;
#define SHIFT ((sizeof(CK_ULONG)-1)*8)
  PRBool first = 0;

  *pError = CKR_OK;

  data->data = dataPtr;
  for (i=0; i < sizeof(CK_ULONG); i++) {
    unsigned char digit = (unsigned char)((value >> SHIFT) & 0xff);

    value = value << 8;

    /* drop leading zero bytes */
    if (first && (0 == digit)) {
	continue;
    }
    *dataPtr++ = digit;
    count++;
  }
  data->size = count;
  return count;
}

/*
 * get an attribute from a template. Value is returned in NSS item.
 * data for the item is owned by the template.
 */
CK_RV
nss_ckcapi_GetAttribute
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
nss_ckcapi_GetULongAttribute
(
  CK_ATTRIBUTE_TYPE type,
  CK_ATTRIBUTE *template,
  CK_ULONG templateSize, 
  CK_RV *pError
)
{
  NSSItem item;

  *pError = nss_ckcapi_GetAttribute(type, template, templateSize, &item);
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
nss_ckcapi_GetBoolAttribute
(
  CK_ATTRIBUTE_TYPE type,
  CK_ATTRIBUTE *template,
  CK_ULONG templateSize, 
  CK_RV *pError
)
{
  NSSItem item;

  *pError = nss_ckcapi_GetAttribute(type, template, templateSize, &item);
  if (CKR_OK != *pError) {
    return (CK_BBOOL) 0;
  }
  if (item.size != sizeof(CK_BBOOL)) {
    *pError = CKR_ATTRIBUTE_VALUE_INVALID;
    return (CK_BBOOL) 0;
  }
  return *(CK_BBOOL *)item.data;
}

/*
 * get an attribute which is type CK_BBOOL.
 */
char *
nss_ckcapi_GetStringAttribute
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
  *pError = nss_ckcapi_GetAttribute(type, template, templateSize, &item);
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
 * Return the size in bytes of a wide string, including the terminating null
 * character
 */
int
nss_ckcapi_WideSize
(
  LPCWSTR wide
)
{
  DWORD size;

  if ((LPWSTR)NULL == wide) {
    return 0;
  }
  size = wcslen(wide)+1;
  return size*sizeof(WCHAR);
}

/*
 * Covert a Unicode wide character string to a UTF8 string
 */
char *
nss_ckcapi_WideToUTF8
(
  LPCWSTR wide 
)
{
  DWORD size;
  char *buf;

  if ((LPWSTR)NULL == wide) {
    return (char *)NULL;
  }

  size = WideCharToMultiByte(CP_UTF8, 0, wide, -1, NULL, 0, NULL, 0);
  if (size == 0) {
    return (char *)NULL;
  }
  buf = nss_ZNEWARRAY(NULL, char, size);
  size = WideCharToMultiByte(CP_UTF8, 0, wide, -1, buf, size, NULL, 0);
  if (size == 0) {
    nss_ZFreeIf(buf);
    return (char *)NULL;
  }
  return buf;
}

/*
 * Return a Wide String duplicated with nss allocated memory.
 */
LPWSTR
nss_ckcapi_WideDup
(
  LPCWSTR wide
)
{
  DWORD len;
  LPWSTR buf;

  if ((LPWSTR)NULL == wide) {
    return (LPWSTR)NULL;
  }

  len = wcslen(wide)+1;

  buf = nss_ZNEWARRAY(NULL, WCHAR, len);
  if ((LPWSTR) NULL == buf) {
    return buf;
  }
  nsslibc_memcpy(buf, wide, len*sizeof(WCHAR));
  return buf;
}

/*
 * Covert a UTF8 string to Unicode wide character
 */
LPWSTR
nss_ckcapi_UTF8ToWide
(
  char *buf
)
{
  DWORD size;
  LPWSTR wide;

  if ((char *)NULL == buf) {
    return (LPWSTR) NULL;
  }
    
  size = MultiByteToWideChar(CP_UTF8, 0, buf, -1, NULL, 0);
  if (size == 0) {
    return (LPWSTR) NULL;
  }
  wide = nss_ZNEWARRAY(NULL, WCHAR, size);
  size = MultiByteToWideChar(CP_UTF8, 0, buf, -1, wide, size);
  if (size == 0) {
    nss_ZFreeIf(wide);
    return (LPWSTR) NULL;
  }
  return wide;
}


/*
 * keep all the knowlege of how the internalObject is laid out in this function
 *
 * nss_ckcapi_FetchKeyContainer
 *
 * fetches the Provider container and info as well as a key handle for a
 * private key. If something other than a private key is passed in,
 * this function fails with CKR_KEY_TYPE_INCONSISTENT
 */
NSS_EXTERN CK_RV
nss_ckcapi_FetchKeyContainer
(
  ckcapiInternalObject *iKey,
  HCRYPTPROV *hProv, 
  DWORD *keySpec,
  HCRYPTKEY *hKey
)
{
  ckcapiCertObject *co;
  ckcapiKeyObject *ko;
  BOOL rc, dummy;
  DWORD msError;


  switch (iKey->type) {
  default:
  case ckcapiRaw:
     /* can't have raw private keys */
     return CKR_KEY_TYPE_INCONSISTENT;
  case ckcapiCert:
    if (iKey->objClass != CKO_PRIVATE_KEY) {
      /* Only private keys have private key provider handles */
      return CKR_KEY_TYPE_INCONSISTENT;
    }
    co = &iKey->u.cert;

    /* OK, get the Provider */
    rc = CryptAcquireCertificatePrivateKey(co->certContext,
      CRYPT_ACQUIRE_CACHE_FLAG|CRYPT_ACQUIRE_COMPARE_KEY_FLAG, NULL, hProv, 
      keySpec, &dummy);
    if (!rc) {
      goto loser;
    }
    break;
  case ckcapiBareKey:
    if (iKey->objClass != CKO_PRIVATE_KEY) {
       /* Only private keys have private key provider handles */
       return CKR_KEY_TYPE_INCONSISTENT;
    }
    ko = &iKey->u.key;

    /* OK, get the Provider */
    if (0 == ko->hProv) {
      rc = CryptAcquireContext(hProv,
			       ko->containerName, 
			       ko->provName,
                               ko->provInfo.dwProvType , 0);
      if (!rc) {
        goto loser;
      }
    } else {
      *hProv = ko->hProv;
    }
    *keySpec = ko->provInfo.dwKeySpec;
    break;
  }

  /* and get the crypto handle */
  rc = CryptGetUserKey(*hProv, *keySpec, hKey);
  if (!rc) {
    goto loser;
  }
  return CKR_OK;
loser:
  /* map the microsoft error before leaving */
  msError = GetLastError();
  switch (msError) {
  case ERROR_INVALID_HANDLE:
  case ERROR_INVALID_PARAMETER:
  case NTE_BAD_KEY:
  case NTE_NO_KEY:
  case NTE_BAD_PUBLIC_KEY:
  case NTE_BAD_KEYSET:
  case NTE_KEYSET_NOT_DEF:
    return CKR_KEY_TYPE_INCONSISTENT;
  case NTE_BAD_UID:
  case NTE_KEYSET_ENTRY_BAD:
    return CKR_DEVICE_ERROR;
  }
  return CKR_GENERAL_ERROR;
}


/*
 * take a DER PUBLIC Key block and return the modulus and exponent
 */
static void
ckcapi_CertPopulateModulusExponent
(
  ckcapiInternalObject *io
)
{
  ckcapiKeyParams *kp = &io->u.cert.key;
  PCCERT_CONTEXT certContext = io->u.cert.certContext;
  unsigned char *pkData =
      certContext->pCertInfo->SubjectPublicKeyInfo.PublicKey.pbData;
  unsigned int size=
      certContext->pCertInfo->SubjectPublicKeyInfo.PublicKey.cbData;
  unsigned int newSize;
  unsigned char *ptr, *newptr;

  /* find the start of the modulus -- this will not give good results if
   * the key isn't an rsa key! */
  ptr = nss_ckcapi_DERUnwrap(pkData, size, &newSize, NULL);
  kp->modulus.data = nss_ckcapi_DERUnwrap(ptr, newSize, 
                                           &kp->modulus.size, &newptr);
  /* changed from signed to unsigned int */
  if (0 == *(char *)kp->modulus.data) {
    kp->modulus.data = ((char *)kp->modulus.data)+1;
    kp->modulus.size = kp->modulus.size - 1;
  }
  /* changed from signed to unsigned int */
  kp->exponent.data = nss_ckcapi_DERUnwrap(newptr, (newptr-ptr)+newSize, 
						&kp->exponent.size, NULL);
  if (0 == *(char *)kp->exponent.data) {
    kp->exponent.data = ((char *)kp->exponent.data)+1;
    kp->exponent.size = kp->exponent.size - 1;
  }
  return;
}

typedef struct _CAPI_RSA_KEY_BLOB {
  PUBLICKEYSTRUC header;
  RSAPUBKEY  rsa;
  char	     data[1];
} CAPI_RSA_KEY_BLOB;

#define CAPI_MODULUS_OFFSET(modSize)     0
#define CAPI_PRIME_1_OFFSET(modSize)     (modSize)
#define CAPI_PRIME_2_OFFSET(modSize)     ((modSize)+(modSize)/2)
#define CAPI_EXPONENT_1_OFFSET(modSize)  ((modSize)*2)
#define CAPI_EXPONENT_2_OFFSET(modSize)  ((modSize)*2+(modSize)/2)
#define CAPI_COEFFICIENT_OFFSET(modSize) ((modSize)*3)
#define CAPI_PRIVATE_EXP_OFFSET(modSize) ((modSize)*3+(modSize)/2)

void
ckcapi_FetchPublicKey
(
  ckcapiInternalObject *io
)
{
  ckcapiKeyParams *kp;
  HCRYPTPROV hProv;
  DWORD keySpec;
  HCRYPTKEY hKey = 0;
  CK_RV error;
  DWORD bufLen;
  BOOL rc;
  unsigned long modulus;
  char *buf = NULL;
  CAPI_RSA_KEY_BLOB *blob;

  error = nss_ckcapi_FetchKeyContainer(io, &hProv, &keySpec, &hKey);
  if (CKR_OK != error) {
    goto loser;
  }
  kp = (ckcapiCert == io->type) ?  &io->u.cert.key : &io->u.key.key;

  rc = CryptExportKey(hKey, 0, PUBLICKEYBLOB, 0, buf, &bufLen);
  if (!rc) {
    goto loser;
  }
  buf = nss_ZNEWARRAY(NULL, char, bufLen);
  rc = CryptExportKey(hKey, 0, PUBLICKEYBLOB, 0, buf, &bufLen);
  if (!rc) {
    goto loser;
  }
  /* validate the blob */
  blob = (CAPI_RSA_KEY_BLOB *)buf;
  if ((PUBLICKEYBLOB != blob->header.bType) ||
      (0x02 != blob->header.bVersion) ||
      (0x31415352 != blob->rsa.magic)) {
    goto loser;
  }
  modulus = blob->rsa.bitlen/8;
  kp->pubKey = buf;
  buf = NULL;

  kp->modulus.data = &blob->data[CAPI_MODULUS_OFFSET(modulus)];
  kp->modulus.size = modulus;
  ckcapi_ReverseData(&kp->modulus);
  nss_ckcapi_IntToData(blob->rsa.pubexp, &kp->exponent,
                     kp->publicExponentData, &error);

loser:
  nss_ZFreeIf(buf);
  if (0 != hKey) {
     CryptDestroyKey(hKey);
  }
  return;
}

void
ckcapi_FetchPrivateKey
(
  ckcapiInternalObject *io
)
{
  ckcapiKeyParams *kp;
  HCRYPTPROV hProv;
  DWORD keySpec;
  HCRYPTKEY hKey = 0;
  CK_RV error;
  DWORD bufLen;
  BOOL rc;
  unsigned long modulus;
  char *buf = NULL;
  CAPI_RSA_KEY_BLOB *blob;

  error = nss_ckcapi_FetchKeyContainer(io, &hProv, &keySpec, &hKey);
  if (CKR_OK != error) {
    goto loser;
  }
  kp = (ckcapiCert == io->type) ?  &io->u.cert.key : &io->u.key.key;

  rc = CryptExportKey(hKey, 0, PRIVATEKEYBLOB, 0, buf, &bufLen);
  if (!rc) {
    goto loser;
  }
  buf = nss_ZNEWARRAY(NULL, char, bufLen);
  rc = CryptExportKey(hKey, 0, PRIVATEKEYBLOB, 0, buf, &bufLen);
  if (!rc) {
    goto loser;
  }
  /* validate the blob */
  blob = (CAPI_RSA_KEY_BLOB *)buf;
  if ((PRIVATEKEYBLOB != blob->header.bType) ||
      (0x02 != blob->header.bVersion) ||
      (0x32415352 != blob->rsa.magic)) {
    goto loser;
  }
  modulus = blob->rsa.bitlen/8;
  kp->privateKey = buf;
  buf = NULL;

  kp->privateExponent.data = &blob->data[CAPI_PRIVATE_EXP_OFFSET(modulus)];
  kp->privateExponent.size = modulus;
  ckcapi_ReverseData(&kp->privateExponent);
  kp->prime1.data = &blob->data[CAPI_PRIME_1_OFFSET(modulus)];
  kp->prime1.size = modulus/2;
  ckcapi_ReverseData(&kp->prime1);
  kp->prime2.data = &blob->data[CAPI_PRIME_2_OFFSET(modulus)];
  kp->prime2.size = modulus/2;
  ckcapi_ReverseData(&kp->prime2);
  kp->exponent1.data = &blob->data[CAPI_EXPONENT_1_OFFSET(modulus)];
  kp->exponent1.size = modulus/2;
  ckcapi_ReverseData(&kp->exponent1);
  kp->exponent2.data = &blob->data[CAPI_EXPONENT_2_OFFSET(modulus)];
  kp->exponent2.size = modulus/2;
  ckcapi_ReverseData(&kp->exponent2);
  kp->coefficient.data = &blob->data[CAPI_COEFFICIENT_OFFSET(modulus)];
  kp->coefficient.size = modulus/2;
  ckcapi_ReverseData(&kp->coefficient);

loser:
  nss_ZFreeIf(buf);
  if (0 != hKey) {
     CryptDestroyKey(hKey);
  }
  return;
}


void
ckcapi_PopulateModulusExponent
(
  ckcapiInternalObject *io
)
{
  if (ckcapiCert == io->type) {
    ckcapi_CertPopulateModulusExponent(io);
  } else {
    ckcapi_FetchPublicKey(io);
  } 
  return;
}

/*
 * fetch the friendly name attribute.
 * can only be called with ckcapiCert type objects!
 */
void
ckcapi_FetchLabel
(
  ckcapiInternalObject *io
)
{
  ckcapiCertObject *co = &io->u.cert;
  char *label;
  PCCERT_CONTEXT certContext = io->u.cert.certContext;
  char labelDataUTF16[128];
  DWORD size = sizeof(labelDataUTF16);
  DWORD size8 = sizeof(co->labelData);
  BOOL rv;

  rv = CertGetCertificateContextProperty(certContext,
	CERT_FRIENDLY_NAME_PROP_ID, labelDataUTF16, &size);
  if (rv) {
    co->labelData = nss_ckcapi_WideToUTF8((LPCWSTR)labelDataUTF16);
    if ((CHAR *)NULL == co->labelData) {
      rv = 0;
    } else {
      size = strlen(co->labelData);
    }
  }
  label = co->labelData;
  /* we are presuming a user cert, make sure it has a nickname, even if
   * Microsoft never gave it one */
  if (!rv && co->hasID) {
    DWORD mserror = GetLastError();
#define DEFAULT_NICKNAME "no Microsoft nickname"
    label = DEFAULT_NICKNAME;
    size = sizeof(DEFAULT_NICKNAME);
    rv = 1;
  }
    
  if (rv) {
    co->label.data = label;
    co->label.size = size;
  }
  return;
}

void
ckcapi_FetchSerial
(
  ckcapiInternalObject *io
)
{
  ckcapiCertObject *co = &io->u.cert;
  PCCERT_CONTEXT certContext = io->u.cert.certContext;
  DWORD size = sizeof(co->derSerial);

  BOOL rc = CryptEncodeObject(X509_ASN_ENCODING,
                         X509_MULTI_BYTE_INTEGER,
                         &certContext->pCertInfo->SerialNumber,
			 co->derSerial,
                         &size);
  if (rc) {
    co->serial.data = co->derSerial;
    co->serial.size = size;
  }
  return;
}

/*
 * fetch the key ID.
 */
void
ckcapi_FetchID
(
  ckcapiInternalObject *io
)
{
  PCCERT_CONTEXT certContext = io->u.cert.certContext;
  DWORD size = 0;
  BOOL rc;

  rc = CertGetCertificateContextProperty(certContext,
	CERT_KEY_IDENTIFIER_PROP_ID, NULL, &size);
  if (!rc) {
    return;
  }
  io->idData = nss_ZNEWARRAY(NULL, char, size);
  if (io->idData == NULL) {
    return;
  }

  rc = CertGetCertificateContextProperty(certContext,
	CERT_KEY_IDENTIFIER_PROP_ID, io->idData, &size);
  if (!rc) {
    nss_ZFreeIf(io->idData);
    io->idData = NULL;
    return;
  }
  io->id.data = io->idData;
  io->id.size = size;
  return;
}

/*
 * fetch the hash key.
 */
void
ckcapi_CertFetchHashKey
(
  ckcapiInternalObject *io
)
{
  ckcapiCertObject *co = &io->u.cert;
  PCCERT_CONTEXT certContext = io->u.cert.certContext;
  DWORD size = certContext->cbCertEncoded;
  DWORD max = sizeof(io->hashKeyData)-1;
  DWORD offset = 0;

  /* make sure we don't over flow. NOTE: cutting the top of a cert is
   * not a big issue because the signature for will be unique for the cert */
  if (size > max) {
    offset = size - max;
    size = max;
  }

  nsslibc_memcpy(io->hashKeyData,certContext->pbCertEncoded+offset, size);
  io->hashKeyData[size] = (char)(io->objClass & 0xff);

  io->hashKey.data = io->hashKeyData;
  io->hashKey.size = size+1;
  return;
}

/*
 * fetch the hash key.
 */
void
ckcapi_KeyFetchHashKey
(
  ckcapiInternalObject *io
)
{
  ckcapiKeyObject *ko = &io->u.key;
  DWORD size;
  DWORD max = sizeof(io->hashKeyData)-2;
  DWORD offset = 0;
  DWORD provLen = strlen(ko->provName);
  DWORD containerLen = strlen(ko->containerName);

 
  size = provLen + containerLen; 

  /* make sure we don't overflow, try to keep things unique */
  if (size > max) {
    DWORD diff = ((size - max)+1)/2;
    provLen -= diff;
    containerLen -= diff;
    size = provLen+containerLen;
  }

  nsslibc_memcpy(io->hashKeyData, ko->provName, provLen);
  nsslibc_memcpy(&io->hashKeyData[provLen],
                 ko->containerName,
		 containerLen);
  io->hashKeyData[size] = (char)(io->objClass & 0xff);
  io->hashKeyData[size+1] = (char)(ko->provInfo.dwKeySpec & 0xff);

  io->hashKey.data = io->hashKeyData;
  io->hashKey.size = size+2;
  return;
}

/*
 * fetch the hash key.
 */
void
ckcapi_FetchHashKey
(
  ckcapiInternalObject *io
)
{
  if (ckcapiCert == io->type) {
    ckcapi_CertFetchHashKey(io);
  } else {
    ckcapi_KeyFetchHashKey(io);
  } 
  return;
}
 
const NSSItem *
ckcapi_FetchCertAttribute
(
  ckcapiInternalObject *io,
  CK_ATTRIBUTE_TYPE type
)
{
  PCCERT_CONTEXT certContext = io->u.cert.certContext;
  switch(type) {
  case CKA_CLASS:
    return &ckcapi_certClassItem;
  case CKA_TOKEN:
    return &ckcapi_trueItem;
  case CKA_MODIFIABLE:
  case CKA_PRIVATE:
    return &ckcapi_falseItem;
  case CKA_CERTIFICATE_TYPE:
    return &ckcapi_x509Item;
  case CKA_LABEL:
    if (0 == io->u.cert.label.size) {
      ckcapi_FetchLabel(io);
    }
    return &io->u.cert.label;
  case CKA_SUBJECT:
    if (0 == io->u.cert.subject.size) {
      io->u.cert.subject.data = certContext->pCertInfo->Subject.pbData;
      io->u.cert.subject.size = certContext->pCertInfo->Subject.cbData;
    }
    return &io->u.cert.subject;
  case CKA_ISSUER:
    if (0 == io->u.cert.issuer.size) {
      io->u.cert.issuer.data = certContext->pCertInfo->Issuer.pbData;
      io->u.cert.issuer.size = certContext->pCertInfo->Issuer.cbData;
    }
    return &io->u.cert.issuer;
  case CKA_SERIAL_NUMBER:
    if (0 == io->u.cert.serial.size) {
      /* not exactly right. This should be the encoded serial number, but
       * it's the decoded serial number! */
      ckcapi_FetchSerial(io);
    }
    return &io->u.cert.serial;
  case CKA_VALUE:
    if (0 == io->u.cert.derCert.size) {
      io->u.cert.derCert.data = io->u.cert.certContext->pbCertEncoded;
      io->u.cert.derCert.size = io->u.cert.certContext->cbCertEncoded;
    }
    return &io->u.cert.derCert;
  case CKA_ID:
    if (!io->u.cert.hasID) {
      return NULL;
    }
    if (0 == io->id.size) {
      ckcapi_FetchID(io);
    }
    return &io->id;
  default:
    break;
  }
  return NULL;
}

const NSSItem *
ckcapi_FetchPubKeyAttribute
(
  ckcapiInternalObject *io, 
  CK_ATTRIBUTE_TYPE type
)
{
  PRBool isCertType = (ckcapiCert == io->type);
  ckcapiKeyParams *kp = isCertType ? &io->u.cert.key : &io->u.key.key;
  
  switch(type) {
  case CKA_CLASS:
    return &ckcapi_pubKeyClassItem;
  case CKA_TOKEN:
  case CKA_LOCAL:
  case CKA_ENCRYPT:
  case CKA_VERIFY:
  case CKA_VERIFY_RECOVER:
    return &ckcapi_trueItem;
  case CKA_PRIVATE:
  case CKA_MODIFIABLE:
  case CKA_DERIVE:
  case CKA_WRAP:
    return &ckcapi_falseItem;
  case CKA_KEY_TYPE:
    return &ckcapi_rsaItem;
  case CKA_LABEL:
    if (!isCertType) {
      return &ckcapi_emptyItem;
    }
    if (0 == io->u.cert.label.size) {
      ckcapi_FetchLabel(io);
    }
    return &io->u.cert.label;
  case CKA_SUBJECT:
    if (!isCertType) {
      return &ckcapi_emptyItem;
    }
    if (0 == io->u.cert.subject.size) {
      PCCERT_CONTEXT certContext= io->u.cert.certContext;
      io->u.cert.subject.data = certContext->pCertInfo->Subject.pbData;
      io->u.cert.subject.size = certContext->pCertInfo->Subject.cbData;
    }
    return &io->u.cert.subject;
  case CKA_MODULUS:
    if (0 == kp->modulus.size) {
	ckcapi_PopulateModulusExponent(io);
    }
    return &kp->modulus;
  case CKA_PUBLIC_EXPONENT:
    if (0 == kp->modulus.size) {
	ckcapi_PopulateModulusExponent(io);
    }
    return &kp->exponent;
  case CKA_ID:
    if (0 == io->id.size) {
      ckcapi_FetchID(io);
    }
    return &io->id;
  default:
    break;
  }
  return NULL;
}

const NSSItem *
ckcapi_FetchPrivKeyAttribute
(
  ckcapiInternalObject *io, 
  CK_ATTRIBUTE_TYPE type
)
{
  PRBool isCertType = (ckcapiCert == io->type);
  ckcapiKeyParams *kp = isCertType ? &io->u.cert.key : &io->u.key.key;

  switch(type) {
  case CKA_CLASS:
    return &ckcapi_privKeyClassItem;
  case CKA_TOKEN:
  case CKA_LOCAL:
  case CKA_SIGN:
  case CKA_DECRYPT:
  case CKA_SIGN_RECOVER:
    return &ckcapi_trueItem;
  case CKA_SENSITIVE:
  case CKA_PRIVATE:    /* should move in the future */
  case CKA_MODIFIABLE:
  case CKA_DERIVE:
  case CKA_UNWRAP:
  case CKA_EXTRACTABLE: /* will probably move in the future */
  case CKA_ALWAYS_SENSITIVE:
  case CKA_NEVER_EXTRACTABLE:
    return &ckcapi_falseItem;
  case CKA_KEY_TYPE:
    return &ckcapi_rsaItem;
  case CKA_LABEL:
    if (!isCertType) {
      return &ckcapi_emptyItem;
    }
    if (0 == io->u.cert.label.size) {
      ckcapi_FetchLabel(io);
    }
    return &io->u.cert.label;
  case CKA_SUBJECT:
    if (!isCertType) {
      return &ckcapi_emptyItem;
    }
    if (0 == io->u.cert.subject.size) {
      PCCERT_CONTEXT certContext= io->u.cert.certContext;
      io->u.cert.subject.data = certContext->pCertInfo->Subject.pbData;
      io->u.cert.subject.size = certContext->pCertInfo->Subject.cbData;
    }
    return &io->u.cert.subject;
  case CKA_MODULUS:
    if (0 == kp->modulus.size) {
      ckcapi_PopulateModulusExponent(io);
    }
    return &kp->modulus;
  case CKA_PUBLIC_EXPONENT:
    if (0 == kp->modulus.size) {
      ckcapi_PopulateModulusExponent(io);
    }
    return &kp->exponent;
  case CKA_PRIVATE_EXPONENT:
    if (0 == kp->privateExponent.size) {
      ckcapi_FetchPrivateKey(io);
    }
    return &kp->privateExponent;
  case CKA_PRIME_1:
    if (0 == kp->privateExponent.size) {
      ckcapi_FetchPrivateKey(io);
    }
    return &kp->prime1;
  case CKA_PRIME_2:
    if (0 == kp->privateExponent.size) {
      ckcapi_FetchPrivateKey(io);
    }
    return &kp->prime2;
  case CKA_EXPONENT_1:
    if (0 == kp->privateExponent.size) {
      ckcapi_FetchPrivateKey(io);
    }
    return &kp->exponent1;
  case CKA_EXPONENT_2:
    if (0 == kp->privateExponent.size) {
      ckcapi_FetchPrivateKey(io);
    }
    return &kp->exponent2;
  case CKA_COEFFICIENT:
    if (0 == kp->privateExponent.size) {
      ckcapi_FetchPrivateKey(io);
    }
    return &kp->coefficient;
  case CKA_ID:
    if (0 == io->id.size) {
      ckcapi_FetchID(io);
    }
    return &io->id;
  default:
    return NULL;
  }
}

const NSSItem *
nss_ckcapi_FetchAttribute
(
  ckcapiInternalObject *io, 
  CK_ATTRIBUTE_TYPE type
)
{
  CK_ULONG i;

  if (io->type == ckcapiRaw) {
    for( i = 0; i < io->u.raw.n; i++ ) {
      if( type == io->u.raw.types[i] ) {
        return &io->u.raw.items[i];
      }
    }
    return NULL;
  }
  /* deal with the common attributes */
  switch (io->objClass) {
  case CKO_CERTIFICATE:
   return ckcapi_FetchCertAttribute(io, type); 
  case CKO_PRIVATE_KEY:
   return ckcapi_FetchPrivKeyAttribute(io, type); 
  case CKO_PUBLIC_KEY:
   return ckcapi_FetchPubKeyAttribute(io, type); 
  }
  return NULL;
}

/*
 * check to see if the certificate already exists
 */
static PRBool
ckcapi_cert_exists(
  NSSItem *value,
  ckcapiInternalObject **io
)
{
  int count,i;
  PRUint32 size = 0;
  ckcapiInternalObject **listp = NULL;
  CK_ATTRIBUTE myTemplate[2];
  CK_OBJECT_CLASS cert_class = CKO_CERTIFICATE;
  CK_ULONG templateCount = 2;
  CK_RV error;
  PRBool found = PR_FALSE;

  myTemplate[0].type = CKA_CLASS;
  myTemplate[0].pValue = &cert_class;
  myTemplate[0].ulValueLen = sizeof(cert_class);
  myTemplate[1].type = CKA_VALUE;
  myTemplate[1].pValue = value->data;
  myTemplate[1].ulValueLen = value->size;

  count = nss_ckcapi_collect_all_certs(myTemplate, templateCount, &listp, 
			&size, 0, &error);

  /* free them */
  if (count > 1) {
    *io = listp[0];
    found = PR_TRUE;
  }
    
  for (i=1; i < count; i++) {
    nss_ckcapi_DestroyInternalObject(listp[i]);
  }
  nss_ZFreeIf(listp);
  return found;
}

static PRBool
ckcapi_cert_hasEmail
(
  PCCERT_CONTEXT certContext
)
{
  int count;

  count = CertGetNameString(certContext, CERT_NAME_EMAIL_TYPE, 
                            0, NULL, NULL, 0);

  return count > 1 ? PR_TRUE : PR_FALSE;
}

static PRBool
ckcapi_cert_isRoot
(
  PCCERT_CONTEXT certContext
)
{
  return CertCompareCertificateName(certContext->dwCertEncodingType,
	&certContext->pCertInfo->Issuer, &certContext->pCertInfo->Subject);
}

static PRBool
ckcapi_cert_isCA
(
  PCCERT_CONTEXT certContext
)
{
  PCERT_EXTENSION extension;
  CERT_BASIC_CONSTRAINTS2_INFO basicInfo;
  DWORD size = sizeof(basicInfo);
  BOOL rc;

  extension = CertFindExtension (szOID_BASIC_CONSTRAINTS,
				certContext->pCertInfo->cExtension,
				certContext->pCertInfo->rgExtension);
  if ((PCERT_EXTENSION) NULL == extension ) {
    return PR_FALSE;
  }
  rc = CryptDecodeObject(X509_ASN_ENCODING, szOID_BASIC_CONSTRAINTS2,
          extension->Value.pbData, extension->Value.cbData, 
          0, &basicInfo, &size);
  if (!rc) {
    return PR_FALSE;
  }
  return (PRBool) basicInfo.fCA;
}

static CRYPT_KEY_PROV_INFO *
ckcapi_cert_getPrivateKeyInfo
(
  PCCERT_CONTEXT certContext,
  NSSItem *keyID
)
{
  BOOL rc;
  CRYPT_HASH_BLOB msKeyID;
  DWORD size = 0;
  CRYPT_KEY_PROV_INFO *prov = NULL;

  msKeyID.cbData = keyID->size;
  msKeyID.pbData = keyID->data;

  rc = CryptGetKeyIdentifierProperty(
	  &msKeyID,
       	  CERT_KEY_PROV_INFO_PROP_ID,
	  0, NULL, NULL, NULL, &size);
  if (!rc) {
    return (CRYPT_KEY_PROV_INFO *)NULL;
  }
  prov = (CRYPT_KEY_PROV_INFO *)nss_ZAlloc(NULL, size);
  if ((CRYPT_KEY_PROV_INFO *)prov == NULL) {
    return (CRYPT_KEY_PROV_INFO *) NULL;
  }
  rc = CryptGetKeyIdentifierProperty(
	  &msKeyID,
       	  CERT_KEY_PROV_INFO_PROP_ID,
	  0, NULL, NULL, prov, &size);
  if (!rc) {
    nss_ZFreeIf(prov);
    return (CRYPT_KEY_PROV_INFO *)NULL;
  }
  
  return prov;
}

static CRYPT_KEY_PROV_INFO *
ckcapi_cert_getProvInfo
(
  ckcapiInternalObject *io
)
{
  BOOL rc;
  DWORD size = 0;
  CRYPT_KEY_PROV_INFO *prov = NULL;

  rc = CertGetCertificateContextProperty(
	  io->u.cert.certContext,
       	  CERT_KEY_PROV_INFO_PROP_ID,
	  NULL, &size);
  if (!rc) {
    return (CRYPT_KEY_PROV_INFO *)NULL;
  }
  prov = (CRYPT_KEY_PROV_INFO *)nss_ZAlloc(NULL, size);
  if ((CRYPT_KEY_PROV_INFO *)prov == NULL) {
    return (CRYPT_KEY_PROV_INFO *) NULL;
  }
  rc = CertGetCertificateContextProperty(
	  io->u.cert.certContext,
       	  CERT_KEY_PROV_INFO_PROP_ID,
	  prov, &size);
  if (!rc) {
    nss_ZFreeIf(prov);
    return (CRYPT_KEY_PROV_INFO *)NULL;
  }

  return prov;
}
  
/* forward declaration */
static void
ckcapi_removeObjectFromHash
(
  ckcapiInternalObject *io
);

/*
 * Finalize - unneeded
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
ckcapi_mdObject_Destroy
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
  ckcapiInternalObject *io = (ckcapiInternalObject *)mdObject->etc;
  CK_OBJECT_CLASS objClass;
  BOOL rc;
  DWORD provType;
  DWORD msError;
  PRBool isCertType = (PRBool)(ckcapiCert == io->type);
  HCERTSTORE hStore = 0;

  if (ckcapiRaw == io->type) {
    /* there is not 'object write protected' error, use the next best thing */
    return CKR_TOKEN_WRITE_PROTECTED;
  }

  objClass = io->objClass;
  if (CKO_CERTIFICATE == objClass) {
    PCCERT_CONTEXT certContext;

    /* get the store */
    hStore = CertOpenSystemStore(0, io->u.cert.certStore);
    if (0 == hStore) {
      rc = 0;
      goto loser;
    }
    certContext = CertFindCertificateInStore(hStore, X509_ASN_ENCODING, 0, 
      CERT_FIND_EXISTING, io->u.cert.certContext, NULL); 
    if ((PCCERT_CONTEXT)NULL ==  certContext) {
      rc = 0;
      goto loser;
    }
    rc = CertDeleteCertificateFromStore(certContext);
  } else {
    char *provName = NULL;
    char *containerName = NULL;
    HCRYPTPROV hProv;
    CRYPT_HASH_BLOB msKeyID;

    if (0 == io->id.size) {
      ckcapi_FetchID(io);
    }

    if (isCertType) {
      CRYPT_KEY_PROV_INFO * provInfo = ckcapi_cert_getProvInfo(io);
      provName = nss_ckcapi_WideToUTF8(provInfo->pwszProvName);
      containerName = nss_ckcapi_WideToUTF8(provInfo->pwszContainerName);
      provType = provInfo->dwProvType;
      nss_ZFreeIf(provInfo);
    } else {
      provName = io->u.key.provName;
      containerName = io->u.key.containerName;
      provType = io->u.key.provInfo.dwProvType;
      io->u.key.provName = NULL;
      io->u.key.containerName = NULL;
    }
    /* first remove the key id pointer */
    msKeyID.cbData = io->id.size;
    msKeyID.pbData = io->id.data;
    rc = CryptSetKeyIdentifierProperty(&msKeyID, 
	CERT_KEY_PROV_INFO_PROP_ID, CRYPT_KEYID_DELETE_FLAG, NULL, NULL, NULL);
    if (rc) {
      rc = CryptAcquireContext(&hProv, containerName, provName, provType,
		CRYPT_DELETEKEYSET);
    }
    nss_ZFreeIf(provName);
    nss_ZFreeIf(containerName);
  }
loser:

  if (hStore) {
    CertCloseStore(hStore, 0);
  }
  if (!rc) {
    msError = GetLastError();
    return CKR_GENERAL_ERROR;
  }

  /* remove it from the hash */
  ckcapi_removeObjectFromHash(io);

  /* free the puppy.. */
  nss_ckcapi_DestroyInternalObject(io);
  return CKR_OK;
}

static CK_BBOOL
ckcapi_mdObject_IsTokenObject
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
ckcapi_mdObject_GetAttributeCount
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
  ckcapiInternalObject *io = (ckcapiInternalObject *)mdObject->etc;

  if (ckcapiRaw == io->type) {
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
ckcapi_mdObject_GetAttributeTypes
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
  ckcapiInternalObject *io = (ckcapiInternalObject *)mdObject->etc;
  CK_ULONG i;
  CK_RV error = CKR_OK;
  const CK_ATTRIBUTE_TYPE *attrs = NULL;
  CK_ULONG size = ckcapi_mdObject_GetAttributeCount(
			mdObject, fwObject, mdSession, fwSession, 
			mdToken, fwToken, mdInstance, fwInstance, &error);

  if( size != ulCount ) {
    return CKR_BUFFER_TOO_SMALL;
  }
  if (io->type == ckcapiRaw) {
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
ckcapi_mdObject_GetAttributeSize
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
  ckcapiInternalObject *io = (ckcapiInternalObject *)mdObject->etc;

  const NSSItem *b;

  b = nss_ckcapi_FetchAttribute(io, attribute);

  if ((const NSSItem *)NULL == b) {
    *pError = CKR_ATTRIBUTE_TYPE_INVALID;
    return 0;
  }
  return b->size;
}

static CK_RV
ckcapi_mdObject_SetAttribute
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
  return CKR_OK;
}

static NSSCKFWItem
ckcapi_mdObject_GetAttribute
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
  ckcapiInternalObject *io = (ckcapiInternalObject *)mdObject->etc;

  mdItem.needsFreeing = PR_FALSE;
  mdItem.item = (NSSItem*)nss_ckcapi_FetchAttribute(io, attribute);

  if ((NSSItem *)NULL == mdItem.item) {
    *pError = CKR_ATTRIBUTE_TYPE_INVALID;
  }

  return mdItem;
}

static CK_ULONG
ckcapi_mdObject_GetObjectSize
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
  ckcapiInternalObject *io = (ckcapiInternalObject *)mdObject->etc;
  CK_ULONG rv = 1;

  /* size is irrelevant to this token */
  return rv;
}

static const NSSCKMDObject
ckcapi_prototype_mdObject = {
  (void *)NULL, /* etc */
  NULL, /* Finalize */
  ckcapi_mdObject_Destroy,
  ckcapi_mdObject_IsTokenObject,
  ckcapi_mdObject_GetAttributeCount,
  ckcapi_mdObject_GetAttributeTypes,
  ckcapi_mdObject_GetAttributeSize,
  ckcapi_mdObject_GetAttribute,
  NULL, /* FreeAttribute */
  ckcapi_mdObject_SetAttribute,
  ckcapi_mdObject_GetObjectSize,
  (void *)NULL /* null terminator */
};

static nssHash *ckcapiInternalObjectHash = NULL;

NSS_IMPLEMENT NSSCKMDObject *
nss_ckcapi_CreateMDObject
(
  NSSArena *arena,
  ckcapiInternalObject *io,
  CK_RV *pError
)
{
  if ((nssHash *)NULL == ckcapiInternalObjectHash) {
    ckcapiInternalObjectHash = nssHash_CreateItem(NULL, 10);
  }
  if (ckcapiCert == io->type) {
    /* the hash key, not a cryptographic key */
    NSSItem *key = &io->hashKey;
    ckcapiInternalObject *old_o = NULL;

    if (key->size == 0) {
      ckcapi_FetchHashKey(io);
    }
    old_o = (ckcapiInternalObject *) 
              nssHash_Lookup(ckcapiInternalObjectHash, key);
    if (!old_o) {
      nssHash_Add(ckcapiInternalObjectHash, key, io);
    } else if (old_o != io) {
      nss_ckcapi_DestroyInternalObject(io);
      io = old_o;
    }
  }
    
  if ( (void*)NULL == io->mdObject.etc) {
    (void) nsslibc_memcpy(&io->mdObject,&ckcapi_prototype_mdObject,
					sizeof(ckcapi_prototype_mdObject));
    io->mdObject.etc = (void *)io;
  }
  return &io->mdObject;
}

static void
ckcapi_removeObjectFromHash
(
  ckcapiInternalObject *io
)
{
  NSSItem *key = &io->hashKey;

  if ((nssHash *)NULL == ckcapiInternalObjectHash) {
    return;
  }
  if (key->size == 0) {
    ckcapi_FetchHashKey(io);
  }
  nssHash_Remove(ckcapiInternalObjectHash, key);
  return;
}

void
nss_ckcapi_DestroyInternalObject
(
  ckcapiInternalObject *io
)
{
  switch (io->type) {
  case ckcapiRaw:
    return;
  case ckcapiCert:
    CertFreeCertificateContext(io->u.cert.certContext);
    nss_ZFreeIf(io->u.cert.labelData);
    nss_ZFreeIf(io->u.cert.key.privateKey);
    nss_ZFreeIf(io->u.cert.key.pubKey);
    nss_ZFreeIf(io->idData);
    break;
  case ckcapiBareKey:
    nss_ZFreeIf(io->u.key.provInfo.pwszContainerName);
    nss_ZFreeIf(io->u.key.provInfo.pwszProvName);
    nss_ZFreeIf(io->u.key.provName);
    nss_ZFreeIf(io->u.key.containerName);
    nss_ZFreeIf(io->u.key.key.privateKey);
    nss_ZFreeIf(io->u.key.key.pubKey);
    if (0 != io->u.key.hProv) {
      CryptReleaseContext(io->u.key.hProv, 0);
    }
    nss_ZFreeIf(io->idData);
    break;
  }
  nss_ZFreeIf(io);
  return;
}

static ckcapiInternalObject *
nss_ckcapi_CreateCertificate
(
  NSSCKFWSession *fwSession,
  CK_ATTRIBUTE_PTR pTemplate,
  CK_ULONG ulAttributeCount,
  CK_RV *pError
)
{
  NSSItem value;
  NSSItem keyID;
  char *storeStr;
  ckcapiInternalObject *io = NULL;
  PCCERT_CONTEXT certContext = NULL;
  PCCERT_CONTEXT storedCertContext = NULL;
  CRYPT_KEY_PROV_INFO *prov_info = NULL;
  char *nickname = NULL;
  HCERTSTORE hStore = 0;
  DWORD msError = 0;
  PRBool hasID;
  CK_RV dummy;
  BOOL rc;

  *pError = nss_ckcapi_GetAttribute(CKA_VALUE, pTemplate, 
				  ulAttributeCount, &value);

  if (CKR_OK != *pError) {
    return (ckcapiInternalObject *)NULL;
  }

  *pError = nss_ckcapi_GetAttribute(CKA_ID, pTemplate, 
				  ulAttributeCount, &keyID);

  if (CKR_OK != *pError) {
    return (ckcapiInternalObject *)NULL;
  }

  if (ckcapi_cert_exists(&value, &io)) {
    return io;
  }

  /* OK, we are creating a new one, figure out what store it belongs to.. 
   * first get a certContext handle.. */
  certContext = CertCreateCertificateContext(X509_ASN_ENCODING, 
                                             value.data, value.size);
  if ((PCCERT_CONTEXT) NULL == certContext) {
    msError = GetLastError();
    *pError = CKR_ATTRIBUTE_VALUE_INVALID;
    goto loser;
  }

  /* do we have a private key laying around... */
  prov_info = ckcapi_cert_getPrivateKeyInfo(certContext, &keyID);
  if (prov_info) {
    CRYPT_DATA_BLOB msKeyID;
    storeStr = "My";
    hasID = PR_TRUE;
    rc = CertSetCertificateContextProperty(certContext,
       	        CERT_KEY_PROV_INFO_PROP_ID,
                0, prov_info);
    nss_ZFreeIf(prov_info);
    if (!rc) {
      msError = GetLastError();
      *pError = CKR_DEVICE_ERROR;
      goto loser;
    }
    msKeyID.cbData = keyID.size;
    msKeyID.pbData = keyID.data;
    rc = CertSetCertificateContextProperty(certContext,
       	        CERT_KEY_IDENTIFIER_PROP_ID,
                0, &msKeyID);
    if (!rc) {
      msError = GetLastError();
      *pError = CKR_DEVICE_ERROR;
      goto loser;
    }
    
  /* does it look like a CA */
  } else if (ckcapi_cert_isCA(certContext)) {
    storeStr = ckcapi_cert_isRoot(certContext) ? "CA" : "Root";
  /* does it look like an S/MIME cert */
  } else if (ckcapi_cert_hasEmail(certContext)) {
    storeStr = "AddressBook";
  } else {
  /* just pick a store */
    storeStr = "CA";
  }

  /* get the nickname, not an error if we can't find it */
  nickname = nss_ckcapi_GetStringAttribute(CKA_LABEL, pTemplate, 
				  ulAttributeCount, &dummy);
  if (nickname) {
    LPWSTR nicknameUTF16 = NULL;
    CRYPT_DATA_BLOB nicknameBlob;

    nicknameUTF16 = nss_ckcapi_UTF8ToWide(nickname);
    nss_ZFreeIf(nickname);
    nickname = NULL;
    if ((LPWSTR)NULL == nicknameUTF16) {
      *pError = CKR_HOST_MEMORY;
      goto loser;
    }
    nicknameBlob.cbData = nss_ckcapi_WideSize(nicknameUTF16);
    nicknameBlob.pbData = (BYTE *)nicknameUTF16;
    rc = CertSetCertificateContextProperty(certContext,
	CERT_FRIENDLY_NAME_PROP_ID, 0, &nicknameBlob);
    nss_ZFreeIf(nicknameUTF16);
    if (!rc) {
      msError = GetLastError();
      *pError = CKR_DEVICE_ERROR;
      goto loser;
    }
  }

  hStore = CertOpenSystemStore((HCRYPTPROV) NULL, storeStr);
  if (0 == hStore) {
    msError = GetLastError();
    *pError = CKR_DEVICE_ERROR;
    goto loser;
  }

  rc = CertAddCertificateContextToStore(hStore, certContext, 
       CERT_STORE_ADD_REPLACE_EXISTING_INHERIT_PROPERTIES, &storedCertContext);
  CertFreeCertificateContext(certContext);
  certContext = NULL;
  CertCloseStore(hStore, 0);
  hStore = 0;
  if (!rc) {
    msError = GetLastError();
    *pError = CKR_DEVICE_ERROR;
    goto loser;
  }

  io = nss_ZNEW(NULL, ckcapiInternalObject);
  if ((ckcapiInternalObject *)NULL == io) {
    *pError = CKR_HOST_MEMORY;
    goto loser;
  }
  io->type = ckcapiCert;
  io->objClass = CKO_CERTIFICATE;
  io->u.cert.certContext = storedCertContext;
  io->u.cert.hasID = hasID;
  return io;

loser:
  if (certContext) {
    CertFreeCertificateContext(certContext);
    certContext = NULL;
  }
  if (storedCertContext) {
    CertFreeCertificateContext(storedCertContext);
    storedCertContext = NULL;
  }
  if (0 != hStore) {
    CertCloseStore(hStore, 0);
  }
  return (ckcapiInternalObject *)NULL;

}

static char *
ckcapi_getDefaultProvider
(
  CK_RV *pError
)
{
  char *name = NULL;
  BOOL rc;
  DWORD nameLength = 0;

  rc = CryptGetDefaultProvider(PROV_RSA_FULL, NULL, CRYPT_USER_DEFAULT, NULL,
		&nameLength);
  if (!rc) {
    return (char *)NULL;
  }

  name = nss_ZNEWARRAY(NULL, char, nameLength);
  if ((char *)NULL == name ) {
    return (char *)NULL;
  }
  rc = CryptGetDefaultProvider(PROV_RSA_FULL, NULL, CRYPT_USER_DEFAULT, name,
		&nameLength);
  if (!rc) {
    nss_ZFreeIf(name);
    return (char *)NULL;
  }

  return name;
}

static char *
ckcapi_getContainer
(
  CK_RV *pError,
  NSSItem *id
)
{
  RPC_STATUS rstat;
  UUID uuid;
  char *uuidStr;
  char *container;

  rstat = UuidCreate(&uuid);
  rstat = UuidToString(&uuid, &uuidStr);

  /* convert it from rcp memory to our own */
  container = nssUTF8_Duplicate(uuidStr, NULL);
  RpcStringFree(&uuidStr);
  
  return container;
}

static CK_RV
ckcapi_buildPrivateKeyBlob
(
  NSSItem  *keyBlob, 
  NSSItem  *modulus,
  NSSItem  *publicExponent,
  NSSItem  *privateExponent,
  NSSItem  *prime1,
  NSSItem  *prime2,
  NSSItem  *exponent1,
  NSSItem  *exponent2,
  NSSItem  *coefficient, 
  PRBool   isKeyExchange
)
{
  CAPI_RSA_KEY_BLOB *keyBlobData = NULL;
  unsigned char *target;
  unsigned long modSize = modulus->size;
  unsigned long dataSize;
  CK_RV error = CKR_OK;

  /* validate extras */
  if (privateExponent->size != modSize) {
    error = CKR_ATTRIBUTE_VALUE_INVALID;
    goto loser;
  }
  if (prime1->size != modSize/2) {
    error = CKR_ATTRIBUTE_VALUE_INVALID;
    goto loser;
  }
  if (prime2->size != modSize/2) {
    error = CKR_ATTRIBUTE_VALUE_INVALID;
    goto loser;
  }
  if (exponent1->size != modSize/2) {
    error = CKR_ATTRIBUTE_VALUE_INVALID;
    goto loser;
  }
  if (exponent2->size != modSize/2) {
    error = CKR_ATTRIBUTE_VALUE_INVALID;
    goto loser;
  }
  if (coefficient->size != modSize/2) {
    error = CKR_ATTRIBUTE_VALUE_INVALID;
    goto loser;
  }
  dataSize = (modSize*4)+(modSize/2) + sizeof(CAPI_RSA_KEY_BLOB);
  keyBlobData = (CAPI_RSA_KEY_BLOB *)nss_ZAlloc(NULL, dataSize);
  if ((CAPI_RSA_KEY_BLOB *)NULL == keyBlobData) {
    error = CKR_HOST_MEMORY;
    goto loser;
  }

  keyBlobData->header.bType = PRIVATEKEYBLOB;
  keyBlobData->header.bVersion = 0x02;
  keyBlobData->header.reserved = 0x00;
  keyBlobData->header.aiKeyAlg = isKeyExchange ? CALG_RSA_KEYX:CALG_RSA_SIGN;
  keyBlobData->rsa.magic = 0x32415352;
  keyBlobData->rsa.bitlen = modSize * 8;
  keyBlobData->rsa.pubexp = nss_ckcapi_DataToInt(publicExponent,&error);
  if (CKR_OK != error) {
    goto loser;
  }

  target = &keyBlobData->data[CAPI_MODULUS_OFFSET(modSize)];
  nsslibc_memcpy(target, modulus->data, modulus->size);
  modulus->data = target;
  ckcapi_ReverseData(modulus);

  target = &keyBlobData->data[CAPI_PRIVATE_EXP_OFFSET(modSize)];
  nsslibc_memcpy(target, privateExponent->data, privateExponent->size);
  privateExponent->data = target;
  ckcapi_ReverseData(privateExponent);

  target = &keyBlobData->data[CAPI_PRIME_1_OFFSET(modSize)];
  nsslibc_memcpy(target, prime1->data, prime1->size);
  prime1->data = target;
  ckcapi_ReverseData(prime1);

  target = &keyBlobData->data[CAPI_PRIME_2_OFFSET(modSize)];
  nsslibc_memcpy(target, prime2->data, prime2->size);
  prime2->data = target;
  ckcapi_ReverseData(prime2);

  target = &keyBlobData->data[CAPI_EXPONENT_1_OFFSET(modSize)];
  nsslibc_memcpy(target, exponent1->data, exponent1->size);
  exponent1->data = target;
  ckcapi_ReverseData(exponent1);

  target = &keyBlobData->data[CAPI_EXPONENT_2_OFFSET(modSize)];
  nsslibc_memcpy(target, exponent2->data, exponent2->size);
  exponent2->data = target;
  ckcapi_ReverseData(exponent2);

  target = &keyBlobData->data[CAPI_COEFFICIENT_OFFSET(modSize)];
  nsslibc_memcpy(target, coefficient->data, coefficient->size);
  coefficient->data = target;
  ckcapi_ReverseData(coefficient);

  keyBlob->data = keyBlobData;
  keyBlob->size = dataSize;

  return CKR_OK;

loser:
  nss_ZFreeIf(keyBlobData);
  return error;
}

static ckcapiInternalObject *
nss_ckcapi_CreatePrivateKey
(
  NSSCKFWSession *fwSession,
  CK_ATTRIBUTE_PTR pTemplate,
  CK_ULONG ulAttributeCount,
  CK_RV *pError
)
{
  NSSItem modulus;
  NSSItem publicExponent;
  NSSItem privateExponent;
  NSSItem exponent1;
  NSSItem exponent2;
  NSSItem prime1;
  NSSItem prime2;
  NSSItem coefficient;
  NSSItem keyID;
  NSSItem keyBlob;
  ckcapiInternalObject *io = NULL;
  char *providerName = NULL;
  char *containerName = NULL;
  char *idData = NULL;
  CRYPT_KEY_PROV_INFO provInfo;
  CRYPT_HASH_BLOB msKeyID;
  CK_KEY_TYPE keyType;
  HCRYPTPROV hProv = 0;
  HCRYPTKEY hKey = 0;
  PRBool decrypt;
  DWORD keySpec;
  DWORD msError;
  BOOL rc;

  keyType = nss_ckcapi_GetULongAttribute
                  (CKA_KEY_TYPE, pTemplate, ulAttributeCount, pError);
  if (CKR_OK != *pError) {
    return (ckcapiInternalObject *)NULL;
  }
  if (CKK_RSA != keyType) {
    *pError = CKR_ATTRIBUTE_VALUE_INVALID;
    return (ckcapiInternalObject *)NULL;
  }

  decrypt = nss_ckcapi_GetBoolAttribute(CKA_DECRYPT, 
				pTemplate, ulAttributeCount, pError);
  if (CKR_TEMPLATE_INCOMPLETE == *pError) {
    decrypt = PR_TRUE; /* default to true */
  }
  decrypt = decrypt || nss_ckcapi_GetBoolAttribute(CKA_UNWRAP, 
				pTemplate, ulAttributeCount, pError);
  if (CKR_TEMPLATE_INCOMPLETE == *pError) {
    decrypt = PR_TRUE; /* default to true */
  }
  keySpec = decrypt ? AT_KEYEXCHANGE : AT_SIGNATURE;

  *pError = nss_ckcapi_GetAttribute(CKA_MODULUS, pTemplate, 
				  ulAttributeCount, &modulus);
  if (CKR_OK != *pError) {
    return (ckcapiInternalObject *)NULL;
  }
  *pError = nss_ckcapi_GetAttribute(CKA_PUBLIC_EXPONENT, pTemplate, 
				  ulAttributeCount, &publicExponent);
  if (CKR_OK != *pError) {
    return (ckcapiInternalObject *)NULL;
  }
  *pError = nss_ckcapi_GetAttribute(CKA_PRIVATE_EXPONENT, pTemplate, 
				  ulAttributeCount, &privateExponent);
  if (CKR_OK != *pError) {
    return (ckcapiInternalObject *)NULL;
  }
  *pError = nss_ckcapi_GetAttribute(CKA_PRIME_1, pTemplate, 
				  ulAttributeCount, &prime1);
  if (CKR_OK != *pError) {
    return (ckcapiInternalObject *)NULL;
  }
  *pError = nss_ckcapi_GetAttribute(CKA_PRIME_2, pTemplate, 
				  ulAttributeCount, &prime2);
  if (CKR_OK != *pError) {
    return (ckcapiInternalObject *)NULL;
  }
  *pError = nss_ckcapi_GetAttribute(CKA_EXPONENT_1, pTemplate, 
				  ulAttributeCount, &exponent1);
  if (CKR_OK != *pError) {
    return (ckcapiInternalObject *)NULL;
  }
  *pError = nss_ckcapi_GetAttribute(CKA_EXPONENT_2, pTemplate, 
				  ulAttributeCount, &exponent2);
  if (CKR_OK != *pError) {
    return (ckcapiInternalObject *)NULL;
  }
  *pError = nss_ckcapi_GetAttribute(CKA_COEFFICIENT, pTemplate, 
				  ulAttributeCount, &coefficient);
  if (CKR_OK != *pError) {
    return (ckcapiInternalObject *)NULL;
  }
  *pError = nss_ckcapi_GetAttribute(CKA_ID, pTemplate, 
				  ulAttributeCount, &keyID);
  if (CKR_OK != *pError) {
    return (ckcapiInternalObject *)NULL;
  }
  providerName = ckcapi_getDefaultProvider(pError);
  if ((char *)NULL == providerName ) {
    return (ckcapiInternalObject *)NULL;
  }
  containerName = ckcapi_getContainer(pError, &keyID);
  if ((char *)NULL == containerName) {
    goto loser;
  }
  rc = CryptAcquireContext(&hProv, containerName, providerName, 
                           PROV_RSA_FULL, CRYPT_NEWKEYSET);
  if (!rc) {
    msError = GetLastError();
    *pError = CKR_DEVICE_ERROR;
    goto loser;
  }

  *pError = ckcapi_buildPrivateKeyBlob(
		&keyBlob, 
		&modulus, 
		&publicExponent, 
		&privateExponent,
		&prime1,
		&prime2, 
		&exponent1, 
		&exponent2, 
		&coefficient, 
		decrypt);
  if (CKR_OK != *pError) {
    goto loser;
  }

  rc = CryptImportKey(hProv, keyBlob.data, keyBlob.size, 
		      0, CRYPT_EXPORTABLE, &hKey);
  if (!rc) {
    msError = GetLastError();
    *pError = CKR_DEVICE_ERROR;
    goto loser;
  }

  idData = nss_ZNEWARRAY(NULL, char, keyID.size);
  if ((void *)NULL == idData) {
    *pError = CKR_HOST_MEMORY;
    goto loser;
  }
  nsslibc_memcpy(idData, keyID.data, keyID.size);

  provInfo.pwszContainerName = nss_ckcapi_UTF8ToWide(containerName);
  provInfo.pwszProvName = nss_ckcapi_UTF8ToWide(providerName);
  provInfo.dwProvType = PROV_RSA_FULL;
  provInfo.dwFlags = 0;
  provInfo.cProvParam = 0;
  provInfo.rgProvParam = NULL;
  provInfo.dwKeySpec = keySpec;

  msKeyID.cbData = keyID.size;
  msKeyID.pbData = keyID.data;

  rc = CryptSetKeyIdentifierProperty(&msKeyID, CERT_KEY_PROV_INFO_PROP_ID,
                                     0, NULL, NULL, &provInfo);
  if (!rc) {
    goto loser;
  }

  /* handle error here */
  io = nss_ZNEW(NULL, ckcapiInternalObject);
  if ((ckcapiInternalObject *)NULL == io) {
    *pError = CKR_HOST_MEMORY;
    goto loser;
  }
  io->type = ckcapiBareKey;
  io->objClass = CKO_PRIVATE_KEY;
  io->u.key.provInfo = provInfo;
  io->u.key.provName = providerName;
  io->u.key.containerName = containerName;
  io->u.key.hProv = hProv; /* save the handle */
  io->idData = idData;
  io->id.data = idData;
  io->id.size = keyID.size;
  /* done with the key handle */
  CryptDestroyKey(hKey);
  return io;

loser:
  nss_ZFreeIf(containerName);
  nss_ZFreeIf(providerName);
  nss_ZFreeIf(idData);
  if (0 != hProv) {
    CryptReleaseContext(hProv, 0);
  }
  if (0 != hKey) {
    CryptDestroyKey(hKey);
  }
  return (ckcapiInternalObject *)NULL;
}


NSS_EXTERN NSSCKMDObject *
nss_ckcapi_CreateObject
(
  NSSCKFWSession *fwSession,
  CK_ATTRIBUTE_PTR pTemplate,
  CK_ULONG ulAttributeCount,
  CK_RV *pError
)
{
  CK_OBJECT_CLASS objClass;
  ckcapiInternalObject *io = NULL;
  CK_BBOOL isToken;

  /*
   * only create token objects
   */
  isToken = nss_ckcapi_GetBoolAttribute(CKA_TOKEN, pTemplate, 
					ulAttributeCount, pError);
  if (CKR_OK != *pError) {
    return (NSSCKMDObject *) NULL;
  }
  if (!isToken) {
    *pError = CKR_ATTRIBUTE_VALUE_INVALID;
    return (NSSCKMDObject *) NULL;
  }

  /*
   * only create keys and certs.
   */
  objClass = nss_ckcapi_GetULongAttribute(CKA_CLASS, pTemplate, 
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
    io = nss_ckcapi_CreateCertificate(fwSession, pTemplate, 
				      ulAttributeCount, pError);
  } else if (objClass == CKO_PRIVATE_KEY) {
    io = nss_ckcapi_CreatePrivateKey(fwSession, pTemplate, 
                                     ulAttributeCount, pError);
  } else {
    *pError = CKR_ATTRIBUTE_VALUE_INVALID;
  }

  if ((ckcapiInternalObject *)NULL == io) {
    return (NSSCKMDObject *) NULL;
  }
  return nss_ckcapi_CreateMDObject(NULL, io, pError);
}
