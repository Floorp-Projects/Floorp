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
 * Portions created by Red Hat, Inc, are Copyright (C) 2005
 *
 * Contributor(s):
 *   Bob Relyea (rrelyea@redhat.com)
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
#ifdef DEBUG
static const char CVS_ID[] = "@(#) $RCSfile: cobject.c,v $ $Revision: 1.2 $ $Date: 2005/11/04 23:44:19 $";
#endif /* DEBUG */

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
  (void *)&ckc_x509, (PRUint32)sizeof(CKC_X_509) };
static const NSSItem ckcapi_rsaItem = { 
  (void *)&ckk_rsa, (PRUint32)sizeof(CK_KEY_TYPE) };
static const NSSItem ckcapi_certClassItem = { 
  (void *)&cko_certificate, (PRUint32)sizeof(CK_OBJECT_CLASS) };
static const NSSItem ckcapi_privKeyClassItem = {
  (void *)&cko_private_key, (PRUint32)sizeof(CK_OBJECT_CLASS) };
static const NSSItem ckcapi_pubKeyClassItem = {
  (void *)&cko_public_key, (PRUint32)sizeof(CK_OBJECT_CLASS) };

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
  BOOL rc, dummy;
  DWORD msError;
#ifdef DEBUG
  char string[512];
  DWORD stringLen;
  PCERT_PUBLIC_KEY_INFO pInfo;
#endif

  if (ckcapiRaw == iKey->type) {
     /* can't have raw private keys */
     return CKR_KEY_TYPE_INCONSISTENT;
  }
  co=&iKey->u.cert;
  if (co->objClass != CKO_PRIVATE_KEY) {
     /* Only private keys have private key provider handles */
     return CKR_KEY_TYPE_INCONSISTENT;
  }

  /* OK, get the Provider */
  rc = CryptAcquireCertificatePrivateKey(co->certContext,
    CRYPT_ACQUIRE_CACHE_FLAG|CRYPT_ACQUIRE_COMPARE_KEY_FLAG, NULL, hProv, 
    keySpec, &dummy);
  if (!rc) {
    goto loser;
  }

#ifdef DEBUG
  /* get information about the provider for debugging purposes */
  stringLen = sizeof(string);
  rc = CryptGetProvParam(*hProv, PP_CONTAINER, string, &stringLen, 0);
  if (!rc) {
    msError = GetLastError();
  }

  stringLen = sizeof(string);
  rc = CryptGetProvParam(*hProv, PP_NAME, string, &stringLen, 0);
  if (!rc) {
    msError = GetLastError();
  }

  pInfo = (PCERT_PUBLIC_KEY_INFO) string;
  stringLen = sizeof(string);
  rc = CryptExportPublicKeyInfo(*hProv, *keySpec, X509_ASN_ENCODING,
  pInfo, &stringLen);
  if (!rc) {
    msError = GetLastError();
  }
#endif

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
 * unwrap a single DER value
 */
char *
nss_ckcapi_DERUnwrap
(
  char *src, 
  int size, 
  int *outSize, 
  char **next
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
 * take a DER PUBLIC Key block and return the modulus and exponent
 */
void
ckcapi_PopulateModulusExponent(ckcapiInternalObject *io)
{
  ckcapiCertObject *co = &io->u.cert;
  PCCERT_CONTEXT certContext = io->u.cert.certContext;
  char *pkData = certContext->pCertInfo->SubjectPublicKeyInfo.PublicKey.pbData;
  CK_ULONG size= certContext->pCertInfo->SubjectPublicKeyInfo.PublicKey.cbData;
  CK_ULONG newSize;
  char *ptr, *newptr;

  /* find the start of the modulus -- this will not give good results if
   * the key isn't an rsa key! */
  ptr = nss_ckcapi_DERUnwrap(pkData, size, &newSize, NULL);
  co->modulus.data = nss_ckcapi_DERUnwrap(ptr, newSize, 
                                           &co->modulus.size, &newptr);
  /* changed from signed to unsigned int */
  if (0 == *(char *)co->modulus.data) {
    co->modulus.data = ((char *)co->modulus.data)+1;
    co->modulus.size = co->modulus.size - 1;
  }
  /* changed from signed to unsigned int */
  co->exponent.data = nss_ckcapi_DERUnwrap(newptr, (newptr-ptr)+newSize, 
						&co->exponent.size, NULL);
  if (0 == *(char *)co->exponent.data) {
    co->exponent.data = ((char *)co->exponent.data)+1;
    co->exponent.size = co->exponent.size - 1;
  }

  return;
}

/*
 * fetch the friendly name attribute.
 */
void
ckcapi_FetchLabel(ckcapiInternalObject *io)
{
  ckcapiCertObject *co = &io->u.cert;
  PCCERT_CONTEXT certContext = io->u.cert.certContext;
  char labelDataUTF16[128];
  DWORD size = sizeof(labelDataUTF16);
  DWORD size8 = sizeof(co->labelData);
  BOOL rv;

  rv = CertGetCertificateContextProperty(certContext,
	CERT_FRIENDLY_NAME_PROP_ID, labelDataUTF16, &size);
  if (rv) {
    size = WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR)labelDataUTF16, size,
		co->labelData, size8, NULL, 0);
    if (size == 0) {
      rv = 0;
    } else {
      size = strlen(co->labelData); /* sigh WideCharToMultiByte returns a 
                                     * broken length, get the real one. */
    }
  }
  /* we are presuming a user cert, make sure it has a nickname, even if
   * Microsoft never gave it one */
  if (!rv && co->hasID) {
    DWORD mserror = GetLastError();
#define DEFAULT_NICKNAME "no Microsoft nickname"
    nsslibc_memcpy(co->labelData, DEFAULT_NICKNAME, sizeof(DEFAULT_NICKNAME));
    size = sizeof(DEFAULT_NICKNAME);
    rv = 1;
  }
    
  if (rv) {
    co->label.data = co->labelData;
    co->label.size = size;
  }
  return;
}

void
ckcapi_FetchSerial(ckcapiInternalObject *io)
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
ckcapi_FetchPrivateKey(ckcapiInternalObject *io)
{
  ckcapiCertObject *co = &io->u.cert;
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
  co->privateKey = buf;
  buf = NULL;

  co->privateExponent.data = &blob->data[CAPI_PRIVATE_EXP_OFFSET(modulus)];
  co->privateExponent.size = modulus;
  ckcapi_ReverseData(&co->privateExponent);
  co->prime1.data = &blob->data[CAPI_PRIME_1_OFFSET(modulus)];
  co->prime1.size = modulus/2;
  ckcapi_ReverseData(&co->prime1);
  co->prime2.data = &blob->data[CAPI_PRIME_2_OFFSET(modulus)];
  co->prime2.size = modulus/2;
  ckcapi_ReverseData(&co->prime2);
  co->exponent1.data = &blob->data[CAPI_EXPONENT_1_OFFSET(modulus)];
  co->exponent1.size = modulus/2;
  ckcapi_ReverseData(&co->exponent1);
  co->exponent2.data = &blob->data[CAPI_EXPONENT_2_OFFSET(modulus)];
  co->exponent2.size = modulus/2;
  ckcapi_ReverseData(&co->exponent2);
  co->coefficient.data = &blob->data[CAPI_COEFFICIENT_OFFSET(modulus)];
  co->coefficient.size = modulus/2;
  ckcapi_ReverseData(&co->coefficient);

loser:
  nss_ZFreeIf(buf);
  if (0 != hKey) {
     CryptDestroyKey(hKey);
  }
  return;
}

/*
 * fetch the key ID.
 */
void
ckcapi_FetchID(ckcapiInternalObject *io)
{
  ckcapiCertObject *co = &io->u.cert;
  PCCERT_CONTEXT certContext = io->u.cert.certContext;
  DWORD size = sizeof(co->idData);
  BOOL rv;

  rv = CertGetCertificateContextProperty(certContext,
	CERT_KEY_IDENTIFIER_PROP_ID, co->idData, &size);
  if (rv) {
    co->id.data = co->idData;
    co->id.size = size;
  }
  return;
}

/*
 * fetch the hash key.
 */
void
ckcapi_FetchHashKey(ckcapiInternalObject *io)
{
  ckcapiCertObject *co = &io->u.cert;
  PCCERT_CONTEXT certContext = io->u.cert.certContext;
  DWORD size = certContext->cbCertEncoded;
  DWORD max = sizeof(co->hashKeyData);
  DWORD offset = 0;

  if (size+1 > max) {
    offset = size+1 - max;
    size = max-1;
  }

  nsslibc_memcpy(co->hashKeyData,certContext->pbCertEncoded+offset, size);
  co->hashKeyData[size] = (char)(co->objClass & 0xff);

  co->hashKey.data = co->hashKeyData;
  co->hashKey.size = size+1;
  return;
}

const NSSItem *
ckcapi_FetchCertAttribute(ckcapiInternalObject *io,
                              CK_ATTRIBUTE_TYPE type)
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
    if (0 == io->u.cert.id.size) {
      ckcapi_FetchID(io);
    }
    return &io->u.cert.id;
  default:
    break;
  }
  return NULL;
}
  

const NSSItem *
ckcapi_FetchPubKeyAttribute(ckcapiInternalObject *io, 
                                CK_ATTRIBUTE_TYPE type)
{
  PCCERT_CONTEXT certContext = io->u.cert.certContext;
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
  case CKA_MODULUS:
    if (0 == io->u.cert.modulus.size) {
	ckcapi_PopulateModulusExponent(io);
    }
    return &io->u.cert.modulus;
  case CKA_PUBLIC_EXPONENT:
    if (0 == io->u.cert.modulus.size) {
	ckcapi_PopulateModulusExponent(io);
    }
    return &io->u.cert.exponent;
  case CKA_ID:
    if (0 == io->u.cert.id.size) {
      ckcapi_FetchID(io);
    }
    return &io->u.cert.id;
  default:
    break;
  }
  return NULL;
}
const NSSItem *
ckcapi_FetchPrivKeyAttribute(ckcapiInternalObject *io, 
                                CK_ATTRIBUTE_TYPE type)
{
  PCCERT_CONTEXT certContext = io->u.cert.certContext;
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
  case CKA_MODULUS:
    if (0 == io->u.cert.modulus.size) {
      ckcapi_PopulateModulusExponent(io);
    }
    return &io->u.cert.modulus;
  case CKA_PUBLIC_EXPONENT:
    if (0 == io->u.cert.modulus.size) {
      ckcapi_PopulateModulusExponent(io);
    }
    return &io->u.cert.exponent;
  case CKA_PRIVATE_EXPONENT:
    if (0 == io->u.cert.privateExponent.size) {
      ckcapi_FetchPrivateKey(io);
    }
    return &io->u.cert.privateExponent;
  case CKA_PRIME_1:
    if (0 == io->u.cert.privateExponent.size) {
      ckcapi_FetchPrivateKey(io);
    }
    return &io->u.cert.prime1;
  case CKA_PRIME_2:
    if (0 == io->u.cert.privateExponent.size) {
      ckcapi_FetchPrivateKey(io);
    }
    return &io->u.cert.prime2;
  case CKA_EXPONENT_1:
    if (0 == io->u.cert.privateExponent.size) {
      ckcapi_FetchPrivateKey(io);
    }
    return &io->u.cert.exponent1;
  case CKA_EXPONENT_2:
    if (0 == io->u.cert.privateExponent.size) {
      ckcapi_FetchPrivateKey(io);
    }
    return &io->u.cert.exponent2;
  case CKA_COEFFICIENT:
    if (0 == io->u.cert.privateExponent.size) {
      ckcapi_FetchPrivateKey(io);
    }
    return &io->u.cert.coefficient;
  case CKA_ID:
    if (0 == io->u.cert.id.size) {
      ckcapi_FetchID(io);
    }
    return &io->u.cert.id;
  default:
    return NULL;
  }
}

const NSSItem *
nss_ckcapi_FetchAttribute(
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
  switch (io->u.cert.objClass) {
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
 * Finalize - unneeded
 * Destroy - CKR_SESSION_READ_ONLY
 * IsTokenObject - CK_TRUE
 * GetAttributeCount
 * GetAttributeTypes
 * GetAttributeSize
 * GetAttribute
 * SetAttribute - unneeded
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
  return CKR_SESSION_READ_ONLY;
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
  switch (io->u.cert.objClass) {
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
  } else switch(io->u.cert.objClass) {
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
  NULL, /* SetAttribute */
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
    NSSItem *key = &io->u.cert.hashKey;
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

void
nss_ckcapi_DestroyInternalObject(ckcapiInternalObject *io)
{
  if (ckcapiRaw == io->type) {
    return;
  }
  CertFreeCertificateContext(io->u.cert.certContext);
  nss_ZFreeIf(io->u.cert.privateKey);
  nss_ZFreeIf(io);
  return;
}
