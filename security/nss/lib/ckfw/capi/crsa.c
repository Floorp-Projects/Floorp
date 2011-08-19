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
 * Red Hat, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
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
static const char CVS_ID[] = "@(#) $RCSfile: crsa.c,v $ $Revision: 1.5 $ $Date: 2011/02/02 17:13:40 $";
#endif /* DEBUG */

#include "ckcapi.h"
#include "secdert.h"

#define SSL3_SHAMD5_HASH_SIZE  36 /* LEN_MD5 (16) + LEN_SHA1 (20) */

/*
 * ckcapi/crsa.c
 *
 * This file implements the NSSCKMDMechnaism and NSSCKMDCryptoOperation objects
 * for the RSA operation on the CAPI cryptoki module.
 */

/*
 * write a Decimal value to a string
 */

static char *
putDecimalString(char *cstr, unsigned long value)
{
  unsigned long tenpower;
  int first = 1;

  for (tenpower=10000000; tenpower; tenpower /= 10) {
    unsigned char digit = (unsigned char )(value/tenpower);
    value = value % tenpower;

    /* drop leading zeros */
    if (first && (0 == digit)) {
      continue;
    }
    first = 0;
    *cstr++ = digit + '0';
  }

  /* if value was zero, put one of them out */
  if (first) {
    *cstr++ = '0';
  }
  return cstr;
}


/*
 * Create a Capi OID string value from a DER OID
 */
static char *
nss_ckcapi_GetOidString
(
  unsigned char *oidTag,
  unsigned int oidTagSize,
  CK_RV *pError
)
{
  unsigned char *oid;
  char *oidStr;
  char *cstr;
  unsigned long value;
  unsigned int oidSize;

  if (DER_OBJECT_ID != *oidTag) {
    /* wasn't an oid */
    *pError = CKR_DATA_INVALID;
    return NULL;
  }
  oid = nss_ckcapi_DERUnwrap(oidTag, oidTagSize, &oidSize, NULL);

  if (oidSize < 2) {
    *pError = CKR_DATA_INVALID;
    return NULL;
  }

  oidStr = nss_ZNEWARRAY( NULL, char, oidSize*4 );
  if ((char *)NULL == oidStr) {
    *pError = CKR_HOST_MEMORY;
    return NULL;
  }
  cstr = oidStr;
  cstr = putDecimalString(cstr, (*oid) / 40);
  *cstr++ = '.';
  cstr = putDecimalString(cstr, (*oid) % 40);
  oidSize--;

  value = 0;
  while (oidSize--) {
    oid++;
    value = (value << 7) + (*oid & 0x7f);
    if (0 == (*oid & 0x80)) {
      *cstr++ = '.';
      cstr = putDecimalString(cstr, value);
      value = 0;
    }
  }

  *cstr = 0; /* NULL terminate */

  if (value != 0) {
    nss_ZFreeIf(oidStr);
    *pError = CKR_DATA_INVALID;
    return NULL;
  }
  return oidStr;
}


/*
 * PKCS #11 sign for RSA expects to take a fully DER-encoded hash value, 
 * which includes the hash OID. CAPI expects to take a Hash Context. While 
 * CAPI does have the capability of setting a raw hash value, it does not 
 * have the ability to sign an arbitrary value. This function tries to
 * reduce the passed in data into something that CAPI could actually sign.
 */
static CK_RV
ckcapi_GetRawHash
(
  const NSSItem *input,
  NSSItem *hash, 
  ALG_ID *hashAlg
)
{
   unsigned char *current;
   unsigned char *algid;
   unsigned char *oid;
   unsigned char *hashData;
   char *oidStr;
   CK_RV error;
   unsigned int oidSize;
   unsigned int size;
   /*
    * there are 2 types of hashes NSS typically tries to sign, regular
    * RSA signature format (with encoded DER_OIDS), and SSL3 Signed hashes.
    * CAPI knows not to add any oids to SSL3_Signed hashes, so if we have any
    * random hash that is exactly the same size as an SSL3 hash, then we can
    * just pass the data through. CAPI has know way of knowing if the value
    * is really a combined hash or some other arbitrary data, so it's safe to
    * handle this case first.
    */
  if (SSL3_SHAMD5_HASH_SIZE == input->size) {
    hash->data = input->data;
    hash->size = input->size;
    *hashAlg = CALG_SSL3_SHAMD5;
    return CKR_OK;
  }

  current = (unsigned char *)input->data;

  /* make sure we have a sequence tag */
  if ((DER_SEQUENCE|DER_CONSTRUCTED) != *current) {
    return CKR_DATA_INVALID;
  }

  /* parse the input block to get 1) the hash oid, and 2) the raw hash value.
   * unfortunatly CAPI doesn't have a builtin function to do this work, so
   * we go ahead and do it by hand here.
   *
   * format is:
   *  SEQUENCE {
   *     SECQUENCE { // algid
   *       OID {}    // oid
   *       ANY {}    // optional params 
   *     }
   *     OCTECT {}   // hash
   */

  /* unwrap */
  algid = nss_ckcapi_DERUnwrap(current,input->size, &size, NULL);
  
  if (algid+size != current+input->size) {
    /* make sure there is not extra data at the end */
    return CKR_DATA_INVALID;
  }

  if ((DER_SEQUENCE|DER_CONSTRUCTED) != *algid) {
    /* wasn't an algid */
    return CKR_DATA_INVALID;
  }
  oid = nss_ckcapi_DERUnwrap(algid, size, &oidSize, &hashData);

  if (DER_OCTET_STRING != *hashData) {
    /* wasn't a hash */
    return CKR_DATA_INVALID;
  }

  /* get the real hash */
  current = hashData;
  size = size - (hashData-algid);
  hash->data = nss_ckcapi_DERUnwrap(current, size, &hash->size, NULL);

  /* get the real oid as a string. Again, Microsoft does not
   * export anything that does this for us */
  oidStr = nss_ckcapi_GetOidString(oid, oidSize, &error);
  if ((char *)NULL == oidStr ) {
    return error;
  }

  /* look up the hash alg from the oid (fortunately CAPI does to this) */ 
  *hashAlg = CertOIDToAlgId(oidStr);
  nss_ZFreeIf(oidStr);
  if (0 == *hashAlg) {
    return CKR_HOST_MEMORY;
  }

  /* hash looks reasonably consistent, we should be able to sign it now */
  return CKR_OK;
}

/*
 * So everyone else in the worlds stores their bignum data MSB first, but not
 * Microsoft, we need to byte swap everything coming into and out of CAPI.
 */
void
ckcapi_ReverseData(NSSItem *item)
{
  int end = (item->size)-1;
  int middle = (item->size)/2;
  unsigned char *buf = item->data;
  int i;

  for (i=0; i < middle; i++) {
    unsigned char  tmp = buf[i];
    buf[i] = buf[end-i];
    buf[end-i] = tmp;
  }
  return;
}

typedef struct ckcapiInternalCryptoOperationRSAPrivStr 
               ckcapiInternalCryptoOperationRSAPriv;
struct ckcapiInternalCryptoOperationRSAPrivStr
{
  NSSCKMDCryptoOperation mdOperation;
  NSSCKMDMechanism     *mdMechanism;
  ckcapiInternalObject *iKey;
  HCRYPTPROV           hProv;
  DWORD                keySpec;
  HCRYPTKEY            hKey;
  NSSItem	       *buffer;
};

/*
 * ckcapi_mdCryptoOperationRSAPriv_Create
 */
static NSSCKMDCryptoOperation *
ckcapi_mdCryptoOperationRSAPriv_Create
(
  const NSSCKMDCryptoOperation *proto,
  NSSCKMDMechanism *mdMechanism,
  NSSCKMDObject *mdKey,
  CK_RV *pError
)
{
  ckcapiInternalObject *iKey = (ckcapiInternalObject *)mdKey->etc;
  const NSSItem *classItem = nss_ckcapi_FetchAttribute(iKey, CKA_CLASS);
  const NSSItem *keyType = nss_ckcapi_FetchAttribute(iKey, CKA_KEY_TYPE);
  ckcapiInternalCryptoOperationRSAPriv *iOperation;
  CK_RV   error;
  HCRYPTPROV  hProv;
  DWORD       keySpec;
  HCRYPTKEY   hKey;

  /* make sure we have the right objects */
  if (((const NSSItem *)NULL == classItem) ||
      (sizeof(CK_OBJECT_CLASS) != classItem->size) ||
      (CKO_PRIVATE_KEY != *(CK_OBJECT_CLASS *)classItem->data) ||
      ((const NSSItem *)NULL == keyType) ||
      (sizeof(CK_KEY_TYPE) != keyType->size) ||
      (CKK_RSA != *(CK_KEY_TYPE *)keyType->data)) {
    *pError =  CKR_KEY_TYPE_INCONSISTENT;
    return (NSSCKMDCryptoOperation *)NULL;
  }

  error = nss_ckcapi_FetchKeyContainer(iKey, &hProv, &keySpec, &hKey);
  if (error != CKR_OK) {
    *pError = error;
    return (NSSCKMDCryptoOperation *)NULL;
  }

  iOperation = nss_ZNEW(NULL, ckcapiInternalCryptoOperationRSAPriv);
  if ((ckcapiInternalCryptoOperationRSAPriv *)NULL == iOperation) {
    *pError = CKR_HOST_MEMORY;
    return (NSSCKMDCryptoOperation *)NULL;
  }
  iOperation->mdMechanism = mdMechanism;
  iOperation->iKey = iKey;
  iOperation->hProv = hProv;
  iOperation->keySpec = keySpec;
  iOperation->hKey = hKey;

  nsslibc_memcpy(&iOperation->mdOperation, 
                 proto, sizeof(NSSCKMDCryptoOperation));
  iOperation->mdOperation.etc = iOperation;

  return &iOperation->mdOperation;
}

static CK_RV
ckcapi_mdCryptoOperationRSAPriv_Destroy
(
  NSSCKMDCryptoOperation *mdOperation,
  NSSCKFWCryptoOperation *fwOperation,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance
)
{
  ckcapiInternalCryptoOperationRSAPriv *iOperation =
       (ckcapiInternalCryptoOperationRSAPriv *)mdOperation->etc;

  if (iOperation->hKey) {
    CryptDestroyKey(iOperation->hKey);
  }
  if (iOperation->buffer) {
    nssItem_Destroy(iOperation->buffer);
  }
  nss_ZFreeIf(iOperation);
  return CKR_OK;
}

static CK_ULONG
ckcapi_mdCryptoOperationRSA_GetFinalLength
(
  NSSCKMDCryptoOperation *mdOperation,
  NSSCKFWCryptoOperation *fwOperation,
  NSSCKMDSession *mdSession,
  NSSCKFWSession *fwSession,
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  CK_RV *pError
)
{
  ckcapiInternalCryptoOperationRSAPriv *iOperation =
       (ckcapiInternalCryptoOperationRSAPriv *)mdOperation->etc;
  const NSSItem *modulus = 
       nss_ckcapi_FetchAttribute(iOperation->iKey, CKA_MODULUS);

  return modulus->size;
}


/*
 * ckcapi_mdCryptoOperationRSADecrypt_GetOperationLength
 * we won't know the length until we actually decrypt the
 * input block. Since we go to all the work to decrypt the
 * the block, we'll save if for when the block is asked for
 */
static CK_ULONG
ckcapi_mdCryptoOperationRSADecrypt_GetOperationLength
(
  NSSCKMDCryptoOperation *mdOperation,
  NSSCKFWCryptoOperation *fwOperation,
  NSSCKMDSession *mdSession,
  NSSCKFWSession *fwSession,
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  const NSSItem *input,
  CK_RV *pError
)
{
  ckcapiInternalCryptoOperationRSAPriv *iOperation =
       (ckcapiInternalCryptoOperationRSAPriv *)mdOperation->etc;
  BOOL rc;

  /* Microsoft's Decrypt operation works in place. Since we don't want
   * to trash our input buffer, we make a copy of it */
  iOperation->buffer = nssItem_Duplicate((NSSItem *)input, NULL, NULL);
  if ((NSSItem *) NULL == iOperation->buffer) {
    *pError = CKR_HOST_MEMORY;
    return 0;
  }
  /* Sigh, reverse it */
  ckcapi_ReverseData(iOperation->buffer);
  
  rc = CryptDecrypt(iOperation->hKey, 0, TRUE, 0, 
		    iOperation->buffer->data, &iOperation->buffer->size);
  if (!rc) {
    DWORD msError = GetLastError();
    switch (msError) {
    case NTE_BAD_DATA:
      *pError = CKR_ENCRYPTED_DATA_INVALID;
      break;
    case NTE_FAIL:
    case NTE_BAD_UID:
      *pError = CKR_DEVICE_ERROR;
      break;
    default:
      *pError = CKR_GENERAL_ERROR; 
    }
    return 0;
  }

  return iOperation->buffer->size;
}

/*
 * ckcapi_mdCryptoOperationRSADecrypt_UpdateFinal
 *
 * NOTE: ckcapi_mdCryptoOperationRSADecrypt_GetOperationLength is presumed to 
 * have been called previously.
 */
static CK_RV
ckcapi_mdCryptoOperationRSADecrypt_UpdateFinal
(
  NSSCKMDCryptoOperation *mdOperation,
  NSSCKFWCryptoOperation *fwOperation,
  NSSCKMDSession *mdSession,
  NSSCKFWSession *fwSession,
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  const NSSItem *input,
  NSSItem *output
)
{
  ckcapiInternalCryptoOperationRSAPriv *iOperation =
       (ckcapiInternalCryptoOperationRSAPriv *)mdOperation->etc;
  NSSItem *buffer = iOperation->buffer;

  if ((NSSItem *)NULL == buffer) {
    return CKR_GENERAL_ERROR;
  }
  nsslibc_memcpy(output->data, buffer->data, buffer->size);
  output->size = buffer->size;
  return CKR_OK;
}

/*
 * ckcapi_mdCryptoOperationRSASign_UpdateFinal
 *
 */
static CK_RV
ckcapi_mdCryptoOperationRSASign_UpdateFinal
(
  NSSCKMDCryptoOperation *mdOperation,
  NSSCKFWCryptoOperation *fwOperation,
  NSSCKMDSession *mdSession,
  NSSCKFWSession *fwSession,
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  const NSSItem *input,
  NSSItem *output
)
{
  ckcapiInternalCryptoOperationRSAPriv *iOperation =
       (ckcapiInternalCryptoOperationRSAPriv *)mdOperation->etc;
  CK_RV error = CKR_OK;
  DWORD msError;
  NSSItem hash;
  HCRYPTHASH hHash = 0;
  ALG_ID  hashAlg;
  DWORD  hashSize;
  DWORD  len; /* temp length value we throw away */
  BOOL   rc;

  /*
   * PKCS #11 sign for RSA expects to take a fully DER-encoded hash value, 
   * which includes the hash OID. CAPI expects to take a Hash Context. While 
   * CAPI does have the capability of setting a raw hash value, it does not 
   * have the ability to sign an arbitrary value. This function tries to
   * reduce the passed in data into something that CAPI could actually sign.
   */
  error = ckcapi_GetRawHash(input, &hash, &hashAlg);
  if (CKR_OK != error) {
    goto loser;
  }

  rc = CryptCreateHash(iOperation->hProv, hashAlg, 0, 0, &hHash);
  if (!rc) {
    goto loser;
  }

  /* make sure the hash lens match before we set it */
  len = sizeof(DWORD);
  rc = CryptGetHashParam(hHash, HP_HASHSIZE, (BYTE *)&hashSize, &len, 0);
  if (!rc) {
    goto loser;
  }

  if (hash.size != hashSize) {
    /* The input must have been bad for this to happen */
    error = CKR_DATA_INVALID;
    goto loser;
  }

  /* we have an explicit hash, set it, note that the length is
   * implicit by the hashAlg used in create */
  rc = CryptSetHashParam(hHash, HP_HASHVAL, hash.data, 0);
  if (!rc) {
    goto loser;
  }

  /* OK, we have the data in a hash structure, sign it! */
  rc = CryptSignHash(hHash, iOperation->keySpec, NULL, 0,
                     output->data, &output->size);
  if (!rc) {
    goto loser;
  }

  /* Don't return a signature that might have been broken because of a cosmic
   * ray, or a broken processor, verify that it is valid... */
  rc = CryptVerifySignature(hHash, output->data, output->size, 
                            iOperation->hKey, NULL, 0);
  if (!rc) {
    goto loser;
  }

  /* OK, Microsoft likes to do things completely differently than anyone
   * else. We need to reverse the data we received here */
  ckcapi_ReverseData(output);
  CryptDestroyHash(hHash);
  return CKR_OK;

loser:
  /* map the microsoft error */
  if (CKR_OK == error) {
    msError = GetLastError();
    switch (msError) {
    case ERROR_NOT_ENOUGH_MEMORY:
      error = CKR_HOST_MEMORY;
      break;
    case NTE_NO_MEMORY:
      error = CKR_DEVICE_MEMORY;
      break;
    case ERROR_MORE_DATA:
      return CKR_BUFFER_TOO_SMALL;
    case ERROR_INVALID_PARAMETER: /* these params were derived from the */
    case ERROR_INVALID_HANDLE:    /* inputs, so if they are bad, the input */ 
    case NTE_BAD_ALGID:           /* data is bad */
    case NTE_BAD_HASH:
      error = CKR_DATA_INVALID;
      break;
    case ERROR_BUSY:
    case NTE_FAIL:
    case NTE_BAD_UID:
      error = CKR_DEVICE_ERROR;
      break;
    default:
      error = CKR_GENERAL_ERROR;
      break;
    }
  }
  if (hHash) {
    CryptDestroyHash(hHash);
  }
  return error;
}
  

NSS_IMPLEMENT_DATA const NSSCKMDCryptoOperation
ckcapi_mdCryptoOperationRSADecrypt_proto = {
  NULL, /* etc */
  ckcapi_mdCryptoOperationRSAPriv_Destroy,
  NULL, /* GetFinalLengh - not needed for one shot Decrypt/Encrypt */
  ckcapi_mdCryptoOperationRSADecrypt_GetOperationLength,
  NULL, /* Final - not needed for one shot operation */
  NULL, /* Update - not needed for one shot operation */
  NULL, /* DigetUpdate - not needed for one shot operation */
  ckcapi_mdCryptoOperationRSADecrypt_UpdateFinal,
  NULL, /* UpdateCombo - not needed for one shot operation */
  NULL, /* DigetKey - not needed for one shot operation */
  (void *)NULL /* null terminator */
};

NSS_IMPLEMENT_DATA const NSSCKMDCryptoOperation
ckcapi_mdCryptoOperationRSASign_proto = {
  NULL, /* etc */
  ckcapi_mdCryptoOperationRSAPriv_Destroy,
  ckcapi_mdCryptoOperationRSA_GetFinalLength,
  NULL, /* GetOperationLengh - not needed for one shot Sign/Verify */
  NULL, /* Final - not needed for one shot operation */
  NULL, /* Update - not needed for one shot operation */
  NULL, /* DigetUpdate - not needed for one shot operation */
  ckcapi_mdCryptoOperationRSASign_UpdateFinal,
  NULL, /* UpdateCombo - not needed for one shot operation */
  NULL, /* DigetKey - not needed for one shot operation */
  (void *)NULL /* null terminator */
};

/********** NSSCKMDMechansim functions ***********************/
/*
 * ckcapi_mdMechanismRSA_Destroy
 */
static void
ckcapi_mdMechanismRSA_Destroy
(
  NSSCKMDMechanism *mdMechanism,
  NSSCKFWMechanism *fwMechanism,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance
)
{
  nss_ZFreeIf(fwMechanism);
}

/*
 * ckcapi_mdMechanismRSA_GetMinKeySize
 */
static CK_ULONG
ckcapi_mdMechanismRSA_GetMinKeySize
(
  NSSCKMDMechanism *mdMechanism,
  NSSCKFWMechanism *fwMechanism,
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  CK_RV *pError
)
{
  return 384;
}

/*
 * ckcapi_mdMechanismRSA_GetMaxKeySize
 */
static CK_ULONG
ckcapi_mdMechanismRSA_GetMaxKeySize
(
  NSSCKMDMechanism *mdMechanism,
  NSSCKFWMechanism *fwMechanism,
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  CK_RV *pError
)
{
  return 16384;
}

/*
 * ckcapi_mdMechanismRSA_DecryptInit
 */
static NSSCKMDCryptoOperation * 
ckcapi_mdMechanismRSA_DecryptInit
(
  NSSCKMDMechanism *mdMechanism,
  NSSCKFWMechanism *fwMechanism,
  CK_MECHANISM     *pMechanism,
  NSSCKMDSession *mdSession,
  NSSCKFWSession *fwSession,
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  NSSCKMDObject *mdKey,
  NSSCKFWObject *fwKey,
  CK_RV *pError
)
{
  return ckcapi_mdCryptoOperationRSAPriv_Create(
		&ckcapi_mdCryptoOperationRSADecrypt_proto,
		mdMechanism, mdKey, pError);
}

/*
 * ckcapi_mdMechanismRSA_SignInit
 */
static NSSCKMDCryptoOperation * 
ckcapi_mdMechanismRSA_SignInit
(
  NSSCKMDMechanism *mdMechanism,
  NSSCKFWMechanism *fwMechanism,
  CK_MECHANISM     *pMechanism,
  NSSCKMDSession *mdSession,
  NSSCKFWSession *fwSession,
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  NSSCKMDObject *mdKey,
  NSSCKFWObject *fwKey,
  CK_RV *pError
)
{
  return ckcapi_mdCryptoOperationRSAPriv_Create(
		&ckcapi_mdCryptoOperationRSASign_proto,
		mdMechanism, mdKey, pError);
}


NSS_IMPLEMENT_DATA const NSSCKMDMechanism
nss_ckcapi_mdMechanismRSA = {
  (void *)NULL, /* etc */
  ckcapi_mdMechanismRSA_Destroy,
  ckcapi_mdMechanismRSA_GetMinKeySize,
  ckcapi_mdMechanismRSA_GetMaxKeySize,
  NULL, /* GetInHardware - default false */
  NULL, /* EncryptInit - default errs */
  ckcapi_mdMechanismRSA_DecryptInit,
  NULL, /* DigestInit - default errs*/
  ckcapi_mdMechanismRSA_SignInit,
  NULL, /* VerifyInit - default errs */
  ckcapi_mdMechanismRSA_SignInit,  /* SignRecoverInit */
  NULL, /* VerifyRecoverInit - default errs */
  NULL, /* GenerateKey - default errs */
  NULL, /* GenerateKeyPair - default errs */
  NULL, /* GetWrapKeyLength - default errs */
  NULL, /* WrapKey - default errs */
  NULL, /* UnwrapKey - default errs */
  NULL, /* DeriveKey - default errs */
  (void *)NULL /* null terminator */
};
