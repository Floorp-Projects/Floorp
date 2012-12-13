/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIIdentityCryptoService.h"
#include "mozilla/ModuleUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsNSSShutDown.h"
#include "nsIThread.h"
#include "nsThreadUtils.h"
#include "nsCOMPtr.h"
#include "nsStringGlue.h"
#include "mozilla/Base64.h"
#include "ScopedNSSTypes.h"

#include "nss.h"
#include "pk11pub.h"
#include "secmod.h"
#include "secerr.h"
#include "keyhi.h"
#include "cryptohi.h"

using namespace mozilla;

namespace {

void
HexEncode(const SECItem * it, nsACString & result)
{
  const char * digits = "0123456789ABCDEF";
  result.SetCapacity((it->len * 2) + 1);
  result.SetLength(it->len * 2);
  char * p = result.BeginWriting();
  for (unsigned int i = 0; i < it->len; ++i) {
    *p++ = digits[it->data[i] >> 4];
    *p++ = digits[it->data[i] & 0x0f];
  }
}

nsresult
Base64UrlEncodeImpl(const nsACString & utf8Input, nsACString & result)
{
  nsresult rv = Base64Encode(utf8Input, result);

  NS_ENSURE_SUCCESS(rv, rv);

  nsACString::char_type * out = result.BeginWriting();
  nsACString::size_type length = result.Length();
  // base64url encoding is defined in RFC 4648. It replaces the last two
  // alphabet characters of base64 encoding with '-' and '_' respectively.
  for (unsigned int i = 0; i < length; ++i) {
    if (out[i] == '+') {
      out[i] = '-';
    } else if (out[i] == '/') {
      out[i] = '_';
    }
  }

  return NS_OK;
}

#define DSA_KEY_TYPE_STRING (NS_LITERAL_CSTRING("DS160"))
#define RSA_KEY_TYPE_STRING (NS_LITERAL_CSTRING("RS256"))

class KeyPair : public nsIIdentityKeyPair, public nsNSSShutDownObject
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIIDENTITYKEYPAIR

  KeyPair(SECKEYPrivateKey* aPrivateKey, SECKEYPublicKey* aPublicKey);

private:
  ~KeyPair()
  {
    destructorSafeDestroyNSSReference();
    shutdown(calledFromObject);
  }

  void virtualDestroyNSSReference() MOZ_OVERRIDE
  {
    destructorSafeDestroyNSSReference();
  }

  void destructorSafeDestroyNSSReference()
  {
    nsNSSShutDownPreventionLock locker;
    if (isAlreadyShutDown())
      return;

    SECKEY_DestroyPrivateKey(mPrivateKey);
    mPrivateKey = NULL;
    SECKEY_DestroyPublicKey(mPublicKey);
    mPublicKey = NULL;
  }

  SECKEYPrivateKey * mPrivateKey;
  SECKEYPublicKey * mPublicKey;

  KeyPair(const KeyPair &) MOZ_DELETE;
  void operator=(const KeyPair &) MOZ_DELETE;
};

NS_IMPL_THREADSAFE_ISUPPORTS1(KeyPair, nsIIdentityKeyPair)

class KeyGenRunnable : public nsRunnable, public nsNSSShutDownObject
{
public:
  NS_DECL_NSIRUNNABLE

  KeyGenRunnable(KeyType keyType, nsIIdentityKeyGenCallback * aCallback);

private:
  ~KeyGenRunnable()
  {
    destructorSafeDestroyNSSReference();
    shutdown(calledFromObject);
  }

  virtual void virtualDestroyNSSReference() MOZ_OVERRIDE
  {
    destructorSafeDestroyNSSReference();
  }

  void destructorSafeDestroyNSSReference()
  {
    nsNSSShutDownPreventionLock locker;
    if (isAlreadyShutDown())
      return;

     mKeyPair = NULL;
  }

  const KeyType mKeyType; // in
  nsCOMPtr<nsIIdentityKeyGenCallback> mCallback; // in
  nsresult mRv; // out
  nsCOMPtr<KeyPair> mKeyPair; // out

  KeyGenRunnable(const KeyGenRunnable &) MOZ_DELETE;
  void operator=(const KeyGenRunnable &) MOZ_DELETE;
};

class SignRunnable : public nsRunnable, public nsNSSShutDownObject
{
public:
  NS_DECL_NSIRUNNABLE

  SignRunnable(const nsACString & textToSign, SECKEYPrivateKey * privateKey,
               nsIIdentitySignCallback * aCallback);

private:
  ~SignRunnable()
  {
    destructorSafeDestroyNSSReference();
    shutdown(calledFromObject);
  }

  void virtualDestroyNSSReference() MOZ_OVERRIDE
  {
    destructorSafeDestroyNSSReference();
  }

  void destructorSafeDestroyNSSReference()
  {
    nsNSSShutDownPreventionLock locker;
    if (isAlreadyShutDown())
      return;

    SECKEY_DestroyPrivateKey(mPrivateKey);
    mPrivateKey = NULL;
  }

  const nsCString mTextToSign; // in
  SECKEYPrivateKey* mPrivateKey; // in
  const nsCOMPtr<nsIIdentitySignCallback> mCallback; // in
  nsresult mRv; // out
  nsCString mSignature; // out

private:
  SignRunnable(const SignRunnable &) MOZ_DELETE;
  void operator=(const SignRunnable &) MOZ_DELETE;
};

class IdentityCryptoService MOZ_FINAL : public nsIIdentityCryptoService
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIIDENTITYCRYPTOSERVICE

  IdentityCryptoService() { }
  nsresult Init()
  {
    nsresult rv;
    nsCOMPtr<nsISupports> dummyUsedToEnsureNSSIsInitialized
      = do_GetService("@mozilla.org/psm;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

private:
  IdentityCryptoService(const KeyPair &) MOZ_DELETE;
  void operator=(const IdentityCryptoService &) MOZ_DELETE;
};

NS_IMPL_THREADSAFE_ISUPPORTS1(IdentityCryptoService, nsIIdentityCryptoService)

NS_IMETHODIMP
IdentityCryptoService::GenerateKeyPair(
  const nsACString & keyTypeString, nsIIdentityKeyGenCallback * callback)
{
  KeyType keyType;
  if (keyTypeString.Equals(RSA_KEY_TYPE_STRING)) {
    keyType = rsaKey;
  } else if (keyTypeString.Equals(DSA_KEY_TYPE_STRING)) {
    keyType = dsaKey;
  } else {
    return NS_ERROR_UNEXPECTED;
  }

  nsCOMPtr<nsIRunnable> r = new KeyGenRunnable(keyType, callback);
  nsCOMPtr<nsIThread> thread;
  nsresult rv = NS_NewThread(getter_AddRefs(thread), r);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
IdentityCryptoService::Base64UrlEncode(const nsACString & utf8Input,
                                       nsACString & result)
{
  return Base64UrlEncodeImpl(utf8Input, result);
}

KeyPair::KeyPair(SECKEYPrivateKey * privateKey, SECKEYPublicKey * publicKey)
  : mPrivateKey(privateKey)
  , mPublicKey(publicKey)
{
  MOZ_ASSERT(!NS_IsMainThread());
}

NS_IMETHODIMP
KeyPair::GetHexRSAPublicKeyExponent(nsACString & result)
{
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_TRUE(mPublicKey, NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_TRUE(mPublicKey->keyType == rsaKey, NS_ERROR_NOT_AVAILABLE);
  HexEncode(&mPublicKey->u.rsa.publicExponent, result);
  return NS_OK;
}

NS_IMETHODIMP
KeyPair::GetHexRSAPublicKeyModulus(nsACString & result)
{
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_TRUE(mPublicKey, NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_TRUE(mPublicKey->keyType == rsaKey, NS_ERROR_NOT_AVAILABLE);
  HexEncode(&mPublicKey->u.rsa.modulus, result);
  return NS_OK;
}

NS_IMETHODIMP
KeyPair::GetHexDSAPrime(nsACString & result)
{
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_TRUE(mPublicKey, NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_TRUE(mPublicKey->keyType == dsaKey, NS_ERROR_NOT_AVAILABLE);
  HexEncode(&mPublicKey->u.dsa.params.prime, result);
  return NS_OK;
}

NS_IMETHODIMP
KeyPair::GetHexDSASubPrime(nsACString & result)
{
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_TRUE(mPublicKey, NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_TRUE(mPublicKey->keyType == dsaKey, NS_ERROR_NOT_AVAILABLE);
  HexEncode(&mPublicKey->u.dsa.params.subPrime, result);
  return NS_OK;
}

NS_IMETHODIMP
KeyPair::GetHexDSAGenerator(nsACString & result)
{
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_TRUE(mPublicKey, NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_TRUE(mPublicKey->keyType == dsaKey, NS_ERROR_NOT_AVAILABLE);
  HexEncode(&mPublicKey->u.dsa.params.base, result);
  return NS_OK;
}

NS_IMETHODIMP
KeyPair::GetHexDSAPublicValue(nsACString & result)
{
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_TRUE(mPublicKey, NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_TRUE(mPublicKey->keyType == dsaKey, NS_ERROR_NOT_AVAILABLE);
  HexEncode(&mPublicKey->u.dsa.publicValue, result);
  return NS_OK;
}

NS_IMETHODIMP
KeyPair::GetKeyType(nsACString & result)
{
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_TRUE(mPublicKey, NS_ERROR_NOT_AVAILABLE);

  switch (mPublicKey->keyType) {
    case rsaKey: result = RSA_KEY_TYPE_STRING; return NS_OK;
    case dsaKey: result = DSA_KEY_TYPE_STRING; return NS_OK;
    default: return NS_ERROR_UNEXPECTED;
  }
}

NS_IMETHODIMP
KeyPair::Sign(const nsACString & textToSign,
              nsIIdentitySignCallback* callback)
{
  MOZ_ASSERT(NS_IsMainThread());
  nsCOMPtr<nsIRunnable> r = new SignRunnable(textToSign, mPrivateKey,
                                             callback);

  nsCOMPtr<nsIThread> thread;
  nsresult rv = NS_NewThread(getter_AddRefs(thread), r);
  return rv;
}

KeyGenRunnable::KeyGenRunnable(KeyType keyType,
                               nsIIdentityKeyGenCallback * callback)
  : mKeyType(keyType)
  , mCallback(callback)
  , mRv(NS_ERROR_NOT_INITIALIZED)
{
}

MOZ_WARN_UNUSED_RESULT nsresult
GenerateKeyPair(PK11SlotInfo * slot,
                SECKEYPrivateKey ** privateKey,
                SECKEYPublicKey ** publicKey,
                CK_MECHANISM_TYPE mechanism,
                void * params)
{
  *publicKey = NULL;
  *privateKey = PK11_GenerateKeyPair(slot, mechanism, params, publicKey,
                                     PR_FALSE /*isPerm*/,
                                     PR_TRUE /*isSensitive*/,
                                     NULL /*&pwdata*/);
  if (!*privateKey) {
    MOZ_ASSERT(!*publicKey);
    return PRErrorCode_to_nsresult(PR_GetError());
  }
  if (!*publicKey) {
	SECKEY_DestroyPrivateKey(*privateKey);
	*privateKey = NULL;
    MOZ_NOT_REACHED("PK11_GnerateKeyPair returned private key without public "
                    "key");
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}


MOZ_WARN_UNUSED_RESULT nsresult
GenerateRSAKeyPair(PK11SlotInfo * slot,
                   SECKEYPrivateKey ** privateKey,
                   SECKEYPublicKey ** publicKey)
{
  MOZ_ASSERT(!NS_IsMainThread());

  PK11RSAGenParams rsaParams;
  rsaParams.keySizeInBits = 2048;
  rsaParams.pe = 0x10001;
  return GenerateKeyPair(slot, privateKey, publicKey, CKM_RSA_PKCS_KEY_PAIR_GEN,
                         &rsaParams);
}

MOZ_WARN_UNUSED_RESULT nsresult
GenerateDSAKeyPair(PK11SlotInfo * slot,
                   SECKEYPrivateKey ** privateKey,
                   SECKEYPublicKey ** publicKey)
{
  MOZ_ASSERT(!NS_IsMainThread());

  // XXX: These could probably be static const arrays, but this way we avoid
  // compiler warnings and also we avoid having to worry much about whether the
  // functions that take these inputs will (unexpectedly) modify them.

  // Using NIST parameters. Some other BrowserID components require that these
  // exact parameters are used.
  uint8_t P[] = {
    0xFF,0x60,0x04,0x83,0xDB,0x6A,0xBF,0xC5,0xB4,0x5E,0xAB,0x78,
    0x59,0x4B,0x35,0x33,0xD5,0x50,0xD9,0xF1,0xBF,0x2A,0x99,0x2A,
    0x7A,0x8D,0xAA,0x6D,0xC3,0x4F,0x80,0x45,0xAD,0x4E,0x6E,0x0C,
    0x42,0x9D,0x33,0x4E,0xEE,0xAA,0xEF,0xD7,0xE2,0x3D,0x48,0x10,
    0xBE,0x00,0xE4,0xCC,0x14,0x92,0xCB,0xA3,0x25,0xBA,0x81,0xFF,
    0x2D,0x5A,0x5B,0x30,0x5A,0x8D,0x17,0xEB,0x3B,0xF4,0xA0,0x6A,
    0x34,0x9D,0x39,0x2E,0x00,0xD3,0x29,0x74,0x4A,0x51,0x79,0x38,
    0x03,0x44,0xE8,0x2A,0x18,0xC4,0x79,0x33,0x43,0x8F,0x89,0x1E,
    0x22,0xAE,0xEF,0x81,0x2D,0x69,0xC8,0xF7,0x5E,0x32,0x6C,0xB7,
    0x0E,0xA0,0x00,0xC3,0xF7,0x76,0xDF,0xDB,0xD6,0x04,0x63,0x8C,
    0x2E,0xF7,0x17,0xFC,0x26,0xD0,0x2E,0x17
  };

  uint8_t Q[] = {
    0xE2,0x1E,0x04,0xF9,0x11,0xD1,0xED,0x79,0x91,0x00,0x8E,0xCA,
    0xAB,0x3B,0xF7,0x75,0x98,0x43,0x09,0xC3
  };

  uint8_t G[] = {
    0xC5,0x2A,0x4A,0x0F,0xF3,0xB7,0xE6,0x1F,0xDF,0x18,0x67,0xCE,
    0x84,0x13,0x83,0x69,0xA6,0x15,0x4F,0x4A,0xFA,0x92,0x96,0x6E,
    0x3C,0x82,0x7E,0x25,0xCF,0xA6,0xCF,0x50,0x8B,0x90,0xE5,0xDE,
    0x41,0x9E,0x13,0x37,0xE0,0x7A,0x2E,0x9E,0x2A,0x3C,0xD5,0xDE,
    0xA7,0x04,0xD1,0x75,0xF8,0xEB,0xF6,0xAF,0x39,0x7D,0x69,0xE1,
    0x10,0xB9,0x6A,0xFB,0x17,0xC7,0xA0,0x32,0x59,0x32,0x9E,0x48,
    0x29,0xB0,0xD0,0x3B,0xBC,0x78,0x96,0xB1,0x5B,0x4A,0xDE,0x53,
    0xE1,0x30,0x85,0x8C,0xC3,0x4D,0x96,0x26,0x9A,0xA8,0x90,0x41,
    0xF4,0x09,0x13,0x6C,0x72,0x42,0xA3,0x88,0x95,0xC9,0xD5,0xBC,
    0xCA,0xD4,0xF3,0x89,0xAF,0x1D,0x7A,0x4B,0xD1,0x39,0x8B,0xD0,
    0x72,0xDF,0xFA,0x89,0x62,0x33,0x39,0x7A
  };

  MOZ_STATIC_ASSERT(PR_ARRAY_SIZE(P) == 1024 / PR_BITS_PER_BYTE, "bad DSA P");
  MOZ_STATIC_ASSERT(PR_ARRAY_SIZE(Q) ==  160 / PR_BITS_PER_BYTE, "bad DSA Q");
  MOZ_STATIC_ASSERT(PR_ARRAY_SIZE(G) == 1024 / PR_BITS_PER_BYTE, "bad DSA G");

  PQGParams pqgParams  = {
    NULL /*arena*/,
    { siBuffer, P, PR_ARRAY_SIZE(P) },
    { siBuffer, Q, PR_ARRAY_SIZE(Q) },
    { siBuffer, G, PR_ARRAY_SIZE(G) }
  };

  return GenerateKeyPair(slot, privateKey, publicKey, CKM_DSA_KEY_PAIR_GEN,
                         &pqgParams);
}

NS_IMETHODIMP
KeyGenRunnable::Run()
{
  if (!NS_IsMainThread()) {
    nsNSSShutDownPreventionLock locker;
    if (isAlreadyShutDown()) {
      mRv = NS_ERROR_NOT_AVAILABLE;
    } else {
      // We always want to use the internal slot for BrowserID; in particular,
      // we want to avoid smartcard slots.
      PK11SlotInfo *slot = PK11_GetInternalSlot();
      if (!slot) {
        mRv = NS_ERROR_UNEXPECTED;
      } else {
        SECKEYPrivateKey *privk = NULL;
        SECKEYPublicKey *pubk = NULL;

        switch (mKeyType) {
        case rsaKey:
          mRv = GenerateRSAKeyPair(slot, &privk, &pubk);
          break;
        case dsaKey:
          mRv = GenerateDSAKeyPair(slot, &privk, &pubk);
          break;
        default:
          MOZ_NOT_REACHED("unknown key type");
          mRv = NS_ERROR_UNEXPECTED;
        }

        PK11_FreeSlot(slot);

        if (NS_SUCCEEDED(mRv)) {
          MOZ_ASSERT(privk);
          MOZ_ASSERT(pubk);
		  // mKeyPair will take over ownership of privk and pubk
          mKeyPair = new KeyPair(privk, pubk);
        }
      }
    }

    NS_DispatchToMainThread(this);
  } else {
    // Back on Main Thread
    (void) mCallback->GenerateKeyPairFinished(mRv, mKeyPair);
  }
  return NS_OK;
}

SignRunnable::SignRunnable(const nsACString & aText,
                           SECKEYPrivateKey * privateKey,
                           nsIIdentitySignCallback * aCallback)
  : mTextToSign(aText)
  , mPrivateKey(SECKEY_CopyPrivateKey(privateKey))
  , mCallback(aCallback)
  , mRv(NS_ERROR_NOT_INITIALIZED)
{
}

NS_IMETHODIMP
SignRunnable::Run()
{
  if (!NS_IsMainThread()) {
    nsNSSShutDownPreventionLock locker;
    if (isAlreadyShutDown()) {
      mRv = NS_ERROR_NOT_AVAILABLE;
    } else {
      // We need the output in PKCS#11 format, not DER encoding, so we must use
      // PK11_HashBuf and PK11_Sign instead of SEC_SignData.

      SECItem sig = { siBuffer, NULL, 0 };
      int sigLength = PK11_SignatureLen(mPrivateKey);
      if (sigLength <= 0) {
        mRv = PRErrorCode_to_nsresult(PR_GetError());
      } else if (!SECITEM_AllocItem(NULL, &sig, sigLength)) {
        mRv = PRErrorCode_to_nsresult(PR_GetError());
      } else {
        uint8_t hash[32]; // big enough for SHA-1 or SHA-256
        SECOidTag hashAlg = mPrivateKey->keyType == dsaKey ? SEC_OID_SHA1
                                                           : SEC_OID_SHA256;
        SECItem hashItem = { siBuffer, hash,
                             hashAlg == SEC_OID_SHA1 ? 20u : 32u };

        mRv = MapSECStatus(PK11_HashBuf(hashAlg, hash,
                    const_cast<uint8_t*>(reinterpret_cast<const uint8_t *>(
                                            mTextToSign.get())),
                                      mTextToSign.Length()));
        if (NS_SUCCEEDED(mRv)) {
          mRv = MapSECStatus(PK11_Sign(mPrivateKey, &sig, &hashItem));
        }
        if (NS_SUCCEEDED(mRv)) {
          nsDependentCSubstring sigString(
            reinterpret_cast<const char*>(sig.data), sig.len);
          mRv = Base64UrlEncodeImpl(sigString, mSignature);
        }
        SECITEM_FreeItem(&sig, false);
      }
    }

    NS_DispatchToMainThread(this);
  } else {
    // Back on Main Thread
    (void) mCallback->SignFinished(mRv, mSignature);
  }

  return NS_OK;
}

// XPCOM module registration

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(IdentityCryptoService, Init)

#define NS_IDENTITYCRYPTOSERVICE_CID \
  {0xbea13a3a, 0x44e8, 0x4d7f, {0xa0, 0xa2, 0x2c, 0x67, 0xf8, 0x4e, 0x3a, 0x97}}

NS_DEFINE_NAMED_CID(NS_IDENTITYCRYPTOSERVICE_CID);

const mozilla::Module::CIDEntry kCIDs[] = {
  { &kNS_IDENTITYCRYPTOSERVICE_CID, false, NULL, IdentityCryptoServiceConstructor },
  { NULL }
};

const mozilla::Module::ContractIDEntry kContracts[] = {
  { "@mozilla.org/identity/crypto-service;1", &kNS_IDENTITYCRYPTOSERVICE_CID },
  { NULL }
};

const mozilla::Module kModule = {
  mozilla::Module::kVersion,
  kCIDs,
  kContracts
};

} // unnamed namespace

NSMODULE_DEFN(identity) = &kModule;
