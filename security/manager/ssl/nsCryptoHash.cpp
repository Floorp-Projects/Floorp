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

already_AddRefed<nsICryptoHash> NS_NewCryptoHash() {
  return MakeAndAddRef<nsCryptoHash>();
}

nsresult NS_NewCryptoHash(uint32_t aHashType, nsICryptoHash** aOutHasher) {
  MOZ_ASSERT(aOutHasher);

  nsCOMPtr<nsICryptoHash> hasher = new nsCryptoHash();
  nsresult rv = hasher->Init(aHashType);

  if (NS_SUCCEEDED(rv)) {
    hasher.forget(aOutHasher);
  }

  return rv;
}

nsresult NS_NewCryptoHash(const nsACString& aHashType,
                          nsICryptoHash** aOutHasher) {
  MOZ_ASSERT(aOutHasher);

  nsCOMPtr<nsICryptoHash> hasher = new nsCryptoHash();
  nsresult rv = hasher->InitWithString(aHashType);

  if (NS_SUCCEEDED(rv)) {
    hasher.forget(aOutHasher);
  }

  return rv;
}
