/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCryptoHash.h"

#include <algorithm>

#include "mozilla/ArrayUtils.h"
#include "mozilla/Base64.h"
#include "mozilla/Casting.h"
#include "nsDependentString.h"
#include "nsIInputStream.h"
#include "nsIKeyModule.h"
#include "nsString.h"
#include "pk11pub.h"
#include "sechash.h"

using namespace mozilla;

namespace {

static const uint64_t STREAM_BUFFER_SIZE = 4096;

}  // namespace

//---------------------------------------------
// Implementing nsICryptoHash
//---------------------------------------------

nsCryptoHash::nsCryptoHash() : mHashContext(nullptr), mInitialized(false) {}

NS_IMPL_ISUPPORTS(nsCryptoHash, nsICryptoHash)

NS_IMETHODIMP
nsCryptoHash::Init(uint32_t algorithm) {
  HASH_HashType hashType;
  switch (algorithm) {
    case nsICryptoHash::MD5:
      hashType = HASH_AlgMD5;
      break;
    case nsICryptoHash::SHA1:
      hashType = HASH_AlgSHA1;
      break;
    case nsICryptoHash::SHA256:
      hashType = HASH_AlgSHA256;
      break;
    case nsICryptoHash::SHA384:
      hashType = HASH_AlgSHA384;
      break;
    case nsICryptoHash::SHA512:
      hashType = HASH_AlgSHA512;
      break;
    default:
      return NS_ERROR_INVALID_ARG;
  }

  if (mHashContext) {
    if (!mInitialized && HASH_GetType(mHashContext.get()) == hashType) {
      mInitialized = true;
      HASH_Begin(mHashContext.get());
      return NS_OK;
    }

    // Destroy current hash context if the type was different
    // or Finish method wasn't called.
    mHashContext = nullptr;
    mInitialized = false;
  }

  mHashContext.reset(HASH_Create(hashType));
  if (!mHashContext) {
    return NS_ERROR_INVALID_ARG;
  }

  HASH_Begin(mHashContext.get());
  mInitialized = true;
  return NS_OK;
}

NS_IMETHODIMP
nsCryptoHash::InitWithString(const nsACString& aAlgorithm) {
  if (aAlgorithm.LowerCaseEqualsLiteral("md5")) return Init(nsICryptoHash::MD5);

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
nsCryptoHash::Update(const uint8_t* data, uint32_t len) {
  if (!mInitialized) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  HASH_Update(mHashContext.get(), data, len);
  return NS_OK;
}

NS_IMETHODIMP
nsCryptoHash::UpdateFromStream(nsIInputStream* data, uint32_t aLen) {
  if (!mInitialized) return NS_ERROR_NOT_INITIALIZED;

  if (!data) return NS_ERROR_INVALID_ARG;

  uint64_t n;
  nsresult rv = data->Available(&n);
  if (NS_FAILED(rv)) return rv;

  // if the user has passed UINT32_MAX, then read
  // everything in the stream

  uint64_t len = aLen;
  if (aLen == UINT32_MAX) len = n;

  // So, if the stream has NO data available for the hash,
  // or if the data available is less then what the caller
  // requested, we can not fulfill the hash update.  In this
  // case, just return NS_ERROR_NOT_AVAILABLE indicating
  // that there is not enough data in the stream to satisify
  // the request.

  if (n == 0 || n < len) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  char buffer[STREAM_BUFFER_SIZE];
  while (len > 0) {
    uint64_t readLimit = std::min<uint64_t>(STREAM_BUFFER_SIZE, len);
    uint32_t read;
    rv = data->Read(buffer, AssertedCast<uint32_t>(readLimit), &read);
    if (NS_FAILED(rv)) {
      return rv;
    }

    rv = Update(BitwiseCast<uint8_t*>(buffer), read);
    if (NS_FAILED(rv)) {
      return rv;
    }

    len -= read;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsCryptoHash::Finish(bool ascii, nsACString& _retval) {
  if (!mInitialized) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  uint32_t hashLen = 0;
  unsigned char buffer[HASH_LENGTH_MAX];
  HASH_End(mHashContext.get(), buffer, &hashLen, HASH_LENGTH_MAX);

  mInitialized = false;

  if (ascii) {
    nsDependentCSubstring dataStr(BitwiseCast<char*>(buffer), hashLen);
    return Base64Encode(dataStr, _retval);
  }

  _retval.Assign(BitwiseCast<char*>(buffer), hashLen);
  return NS_OK;
}

//---------------------------------------------
// Implementing nsICryptoHMAC
//---------------------------------------------

NS_IMPL_ISUPPORTS(nsCryptoHMAC, nsICryptoHMAC)

nsCryptoHMAC::nsCryptoHMAC() : mHMACContext(nullptr) {}

NS_IMETHODIMP
nsCryptoHMAC::Init(uint32_t aAlgorithm, nsIKeyObject* aKeyObject) {
  if (mHMACContext) {
    mHMACContext = nullptr;
  }

  CK_MECHANISM_TYPE mechType;
  switch (aAlgorithm) {
    case nsICryptoHMAC::MD5:
      mechType = CKM_MD5_HMAC;
      break;
    case nsICryptoHMAC::SHA1:
      mechType = CKM_SHA_1_HMAC;
      break;
    case nsICryptoHMAC::SHA256:
      mechType = CKM_SHA256_HMAC;
      break;
    case nsICryptoHMAC::SHA384:
      mechType = CKM_SHA384_HMAC;
      break;
    case nsICryptoHMAC::SHA512:
      mechType = CKM_SHA512_HMAC;
      break;
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
  rv = aKeyObject->GetKeyObj(&key);
  NS_ENSURE_SUCCESS(rv, rv);

  SECItem rawData;
  rawData.data = 0;
  rawData.len = 0;
  mHMACContext.reset(
      PK11_CreateContextBySymKey(mechType, CKA_SIGN, key, &rawData));
  NS_ENSURE_TRUE(mHMACContext, NS_ERROR_FAILURE);

  if (PK11_DigestBegin(mHMACContext.get()) != SECSuccess) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsCryptoHMAC::Update(const uint8_t* aData, uint32_t aLen) {
  if (!mHMACContext) return NS_ERROR_NOT_INITIALIZED;

  if (!aData) return NS_ERROR_INVALID_ARG;

  if (PK11_DigestOp(mHMACContext.get(), aData, aLen) != SECSuccess) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsCryptoHMAC::UpdateFromStream(nsIInputStream* aStream, uint32_t aLen) {
  if (!mHMACContext) return NS_ERROR_NOT_INITIALIZED;

  if (!aStream) return NS_ERROR_INVALID_ARG;

  uint64_t n;
  nsresult rv = aStream->Available(&n);
  if (NS_FAILED(rv)) return rv;

  // if the user has passed UINT32_MAX, then read
  // everything in the stream

  uint64_t len = aLen;
  if (aLen == UINT32_MAX) len = n;

  // So, if the stream has NO data available for the hash,
  // or if the data available is less then what the caller
  // requested, we can not fulfill the HMAC update.  In this
  // case, just return NS_ERROR_NOT_AVAILABLE indicating
  // that there is not enough data in the stream to satisify
  // the request.

  if (n == 0 || n < len) return NS_ERROR_NOT_AVAILABLE;

  char buffer[STREAM_BUFFER_SIZE];
  while (len > 0) {
    uint64_t readLimit = std::min<uint64_t>(STREAM_BUFFER_SIZE, len);
    uint32_t read;
    rv = aStream->Read(buffer, AssertedCast<uint32_t>(readLimit), &read);
    if (NS_FAILED(rv)) {
      return rv;
    }

    if (read == 0) {
      return NS_BASE_STREAM_CLOSED;
    }

    rv = Update(BitwiseCast<uint8_t*>(buffer), read);
    if (NS_FAILED(rv)) {
      return rv;
    }

    len -= read;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsCryptoHMAC::Finish(bool aASCII, nsACString& _retval) {
  if (!mHMACContext) return NS_ERROR_NOT_INITIALIZED;

  uint32_t hashLen = 0;
  unsigned char buffer[HASH_LENGTH_MAX];
  SECStatus srv =
      PK11_DigestFinal(mHMACContext.get(), buffer, &hashLen, HASH_LENGTH_MAX);
  if (srv != SECSuccess) {
    return NS_ERROR_FAILURE;
  }

  if (aASCII) {
    nsDependentCSubstring dataStr(BitwiseCast<char*>(buffer), hashLen);
    return Base64Encode(dataStr, _retval);
  }

  _retval.Assign(BitwiseCast<char*>(buffer), hashLen);
  return NS_OK;
}

NS_IMETHODIMP
nsCryptoHMAC::Reset() {
  if (PK11_DigestBegin(mHMACContext.get()) != SECSuccess) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}
