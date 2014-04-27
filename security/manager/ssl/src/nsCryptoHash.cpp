/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZ_LOGGING
#define FORCE_PR_LOG 1
#endif

#include <algorithm>

#include "nsCryptoHash.h"

#include "nsIInputStream.h"
#include "nsIKeyModule.h"

#include "nsString.h"

#include "sechash.h"
#include "pk11pub.h"
#include "base64.h"

#define NS_CRYPTO_HASH_BUFFER_SIZE 4096

//---------------------------------------------
// Implementing nsICryptoHash
//---------------------------------------------

nsCryptoHash::nsCryptoHash()
  : mHashContext(nullptr)
  , mInitialized(false)
{
}

nsCryptoHash::~nsCryptoHash()
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return;
  }
  destructorSafeDestroyNSSReference();
  shutdown(calledFromObject);
}

void nsCryptoHash::virtualDestroyNSSReference()
{
  destructorSafeDestroyNSSReference();
}

void nsCryptoHash::destructorSafeDestroyNSSReference()
{
  if (mHashContext)
    HASH_Destroy(mHashContext);
  mHashContext = nullptr;
}

NS_IMPL_ISUPPORTS(nsCryptoHash, nsICryptoHash)

NS_IMETHODIMP 
nsCryptoHash::Init(uint32_t algorithm)
{
  nsNSSShutDownPreventionLock locker;

  HASH_HashType hashType = (HASH_HashType)algorithm;
  if (mHashContext)
  {
    if ((!mInitialized) && (HASH_GetType(mHashContext) == hashType))
    {
      mInitialized = true;
      HASH_Begin(mHashContext);
      return NS_OK;
    }

    // Destroy current hash context if the type was different
    // or Finish method wasn't called.
    HASH_Destroy(mHashContext);
    mInitialized = false;
  }

  mHashContext = HASH_Create(hashType);
  if (!mHashContext)
    return NS_ERROR_INVALID_ARG;

  HASH_Begin(mHashContext);
  mInitialized = true;
  return NS_OK; 
}

NS_IMETHODIMP
nsCryptoHash::InitWithString(const nsACString & aAlgorithm)
{
  if (aAlgorithm.LowerCaseEqualsLiteral("md2"))
    return Init(nsICryptoHash::MD2);

  if (aAlgorithm.LowerCaseEqualsLiteral("md5"))
    return Init(nsICryptoHash::MD5);

  if (aAlgorithm.LowerCaseEqualsLiteral("sha1"))
    return Init(nsICryptoHash::SHA1);

  if (aAlgorithm.LowerCaseEqualsLiteral("sha256"))
    return Init(nsICryptoHash::SHA256);

  if (aAlgorithm.LowerCaseEqualsLiteral("sha384"))
    return Init(nsICryptoHash::SHA384);

  if (aAlgorithm.LowerCaseEqualsLiteral("sha512"))
    return Init(nsICryptoHash::SHA512);

  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP
nsCryptoHash::Update(const uint8_t *data, uint32_t len)
{
  nsNSSShutDownPreventionLock locker;
  
  if (!mInitialized)
    return NS_ERROR_NOT_INITIALIZED;

  HASH_Update(mHashContext, data, len);
  return NS_OK; 
}

NS_IMETHODIMP
nsCryptoHash::UpdateFromStream(nsIInputStream *data, uint32_t aLen)
{
  if (!mInitialized)
    return NS_ERROR_NOT_INITIALIZED;

  if (!data)
    return NS_ERROR_INVALID_ARG;

  uint64_t n;
  nsresult rv = data->Available(&n);
  if (NS_FAILED(rv))
    return rv;

  // if the user has passed UINT32_MAX, then read
  // everything in the stream

  uint64_t len = aLen;
  if (aLen == UINT32_MAX)
    len = n;

  // So, if the stream has NO data available for the hash,
  // or if the data available is less then what the caller
  // requested, we can not fulfill the hash update.  In this
  // case, just return NS_ERROR_NOT_AVAILABLE indicating
  // that there is not enough data in the stream to satisify
  // the request.

  if (n == 0 || n < len)
    return NS_ERROR_NOT_AVAILABLE;
  
  char buffer[NS_CRYPTO_HASH_BUFFER_SIZE];
  uint32_t read, readLimit;
  
  while(NS_SUCCEEDED(rv) && len>0)
  {
    readLimit = (uint32_t)std::min<uint64_t>(NS_CRYPTO_HASH_BUFFER_SIZE, len);
    
    rv = data->Read(buffer, readLimit, &read);
    
    if (NS_SUCCEEDED(rv))
      rv = Update((const uint8_t*)buffer, read);
    
    len -= read;
  }
  
  return rv;
}

NS_IMETHODIMP
nsCryptoHash::Finish(bool ascii, nsACString & _retval)
{
  nsNSSShutDownPreventionLock locker;
  
  if (!mInitialized)
    return NS_ERROR_NOT_INITIALIZED;
  
  uint32_t hashLen = 0;
  unsigned char buffer[HASH_LENGTH_MAX];
  unsigned char* pbuffer = buffer;

  HASH_End(mHashContext, pbuffer, &hashLen, HASH_LENGTH_MAX);

  mInitialized = false;

  if (ascii)
  {
    char *asciiData = BTOA_DataToAscii(buffer, hashLen);
    NS_ENSURE_TRUE(asciiData, NS_ERROR_OUT_OF_MEMORY);

    _retval.Assign(asciiData);
    PORT_Free(asciiData);
  }
  else
  {
    _retval.Assign((const char*)buffer, hashLen);
  }

  return NS_OK;
}

//---------------------------------------------
// Implementing nsICryptoHMAC
//---------------------------------------------

NS_IMPL_ISUPPORTS(nsCryptoHMAC, nsICryptoHMAC)

nsCryptoHMAC::nsCryptoHMAC()
{
  mHMACContext = nullptr;
}

nsCryptoHMAC::~nsCryptoHMAC()
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return;
  }
  destructorSafeDestroyNSSReference();
  shutdown(calledFromObject);
}

void nsCryptoHMAC::virtualDestroyNSSReference()
{
  destructorSafeDestroyNSSReference();
}

void nsCryptoHMAC::destructorSafeDestroyNSSReference()
{
  if (mHMACContext)
    PK11_DestroyContext(mHMACContext, true);
  mHMACContext = nullptr;
}

/* void init (in unsigned long aAlgorithm, in nsIKeyObject aKeyObject); */
NS_IMETHODIMP nsCryptoHMAC::Init(uint32_t aAlgorithm, nsIKeyObject *aKeyObject)
{
  nsNSSShutDownPreventionLock locker;

  if (mHMACContext)
  {
    PK11_DestroyContext(mHMACContext, true);
    mHMACContext = nullptr;
  }

  CK_MECHANISM_TYPE HMACMechType;
  switch (aAlgorithm)
  {
  case nsCryptoHMAC::MD2:
    HMACMechType = CKM_MD2_HMAC; break;
  case nsCryptoHMAC::MD5:
    HMACMechType = CKM_MD5_HMAC; break;
  case nsCryptoHMAC::SHA1:
    HMACMechType = CKM_SHA_1_HMAC; break;
  case nsCryptoHMAC::SHA256:
    HMACMechType = CKM_SHA256_HMAC; break;
  case nsCryptoHMAC::SHA384:
    HMACMechType = CKM_SHA384_HMAC; break;
  case nsCryptoHMAC::SHA512:
    HMACMechType = CKM_SHA512_HMAC; break;
  default:
    return NS_ERROR_INVALID_ARG;
  }

  NS_ENSURE_ARG_POINTER(aKeyObject);

  nsresult rv;

  int16_t keyType;
  rv = aKeyObject->GetType(&keyType);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ENSURE_TRUE(keyType == nsIKeyObject::SYM_KEY, NS_ERROR_INVALID_ARG);

  PK11SymKey* key;
  // GetKeyObj doesn't addref the key
  rv = aKeyObject->GetKeyObj((void**)&key);
  NS_ENSURE_SUCCESS(rv, rv);

  SECItem rawData;
  rawData.data = 0;
  rawData.len = 0;
  mHMACContext = PK11_CreateContextBySymKey(
      HMACMechType, CKA_SIGN, key, &rawData);
  NS_ENSURE_TRUE(mHMACContext, NS_ERROR_FAILURE);

  SECStatus ss = PK11_DigestBegin(mHMACContext);
  NS_ENSURE_TRUE(ss == SECSuccess, NS_ERROR_FAILURE);

  return NS_OK;
}

/* void update ([array, size_is (aLen), const] in octet aData, in unsigned long aLen); */
NS_IMETHODIMP nsCryptoHMAC::Update(const uint8_t *aData, uint32_t aLen)
{
  nsNSSShutDownPreventionLock locker;

  if (!mHMACContext)
    return NS_ERROR_NOT_INITIALIZED;

  if (!aData)
    return NS_ERROR_INVALID_ARG;

  SECStatus ss = PK11_DigestOp(mHMACContext, aData, aLen);
  NS_ENSURE_TRUE(ss == SECSuccess, NS_ERROR_FAILURE);
  
  return NS_OK;
}

/* void updateFromStream (in nsIInputStream aStream, in unsigned long aLen); */
NS_IMETHODIMP nsCryptoHMAC::UpdateFromStream(nsIInputStream *aStream, uint32_t aLen)
{
  if (!mHMACContext)
    return NS_ERROR_NOT_INITIALIZED;

  if (!aStream)
    return NS_ERROR_INVALID_ARG;

  uint64_t n;
  nsresult rv = aStream->Available(&n);
  if (NS_FAILED(rv))
    return rv;

  // if the user has passed UINT32_MAX, then read
  // everything in the stream

  uint64_t len = aLen;
  if (aLen == UINT32_MAX)
    len = n;

  // So, if the stream has NO data available for the hash,
  // or if the data available is less then what the caller
  // requested, we can not fulfill the HMAC update.  In this
  // case, just return NS_ERROR_NOT_AVAILABLE indicating
  // that there is not enough data in the stream to satisify
  // the request.

  if (n == 0 || n < len)
    return NS_ERROR_NOT_AVAILABLE;
  
  char buffer[NS_CRYPTO_HASH_BUFFER_SIZE];
  uint32_t read, readLimit;
  
  while(NS_SUCCEEDED(rv) && len > 0)
  {
    readLimit = (uint32_t)std::min<uint64_t>(NS_CRYPTO_HASH_BUFFER_SIZE, len);
    
    rv = aStream->Read(buffer, readLimit, &read);
    if (read == 0)
      return NS_BASE_STREAM_CLOSED;
    
    if (NS_SUCCEEDED(rv))
      rv = Update((const uint8_t*)buffer, read);
    
    len -= read;
  }
  
  return rv;
}

/* ACString finish (in bool aASCII); */
NS_IMETHODIMP nsCryptoHMAC::Finish(bool aASCII, nsACString & _retval)
{
  nsNSSShutDownPreventionLock locker;

  if (!mHMACContext)
    return NS_ERROR_NOT_INITIALIZED;
  
  uint32_t hashLen = 0;
  unsigned char buffer[HASH_LENGTH_MAX];
  unsigned char* pbuffer = buffer;

  PK11_DigestFinal(mHMACContext, pbuffer, &hashLen, HASH_LENGTH_MAX);
  if (aASCII)
  {
    char *asciiData = BTOA_DataToAscii(buffer, hashLen);
    NS_ENSURE_TRUE(asciiData, NS_ERROR_OUT_OF_MEMORY);

    _retval.Assign(asciiData);
    PORT_Free(asciiData);
  }
  else
  {
    _retval.Assign((const char*)buffer, hashLen);
  }

  return NS_OK;
}

/* void reset (); */
NS_IMETHODIMP nsCryptoHMAC::Reset()
{
  nsNSSShutDownPreventionLock locker;

  SECStatus ss = PK11_DigestBegin(mHMACContext);
  NS_ENSURE_TRUE(ss == SECSuccess, NS_ERROR_FAILURE);

  return NS_OK;
}
