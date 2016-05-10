/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNSSU2FToken.h"

#include "CryptoBuffer.h"
#include "nsNSSComponent.h"
#include "pk11pub.h"
#include "prerror.h"
#include "secerr.h"
#include "WebCryptoCommon.h"

using mozilla::dom::CreateECParamsForCurve;

NS_IMPL_ISUPPORTS(nsNSSU2FToken, nsINSSU2FToken)

// Not named "security.webauth.u2f_softtoken_counter" because setting that
// name causes the window.u2f object to disappear until preferences get
// reloaded, as its' pref is a substring!
#define PREF_U2F_NSSTOKEN_COUNTER "security.webauth.softtoken_counter"

const nsCString nsNSSU2FToken::mSecretNickname =
  NS_LITERAL_CSTRING("U2F_NSSTOKEN");
const nsString nsNSSU2FToken::mVersion =
  NS_LITERAL_STRING("U2F_V2");
NS_NAMED_LITERAL_CSTRING(kAttestCertSubjectName, "CN=Firefox U2F Soft Token");

// This U2F-compatible soft token uses FIDO U2F-compatible ECDSA keypairs
// on the SEC_OID_SECG_EC_SECP256R1 curve. When asked to Register, it will
// generate and return a new keypair KP, where the private component is wrapped
// using AES-KW with the 128-bit mWrappingKey to make an opaque "key handle".
// In other words, Register yields { KP_pub, AES-KW(KP_priv, key=mWrappingKey) }
//
// The value mWrappingKey is long-lived; it is persisted as part of the NSS DB
// for the current profile. The attestation certificates that are produced are
// ephemeral to counteract profiling. They have little use for a soft-token
// at any rate, but are required by the specification.

const uint32_t kParamLen = 32;
const uint32_t kPublicKeyLen = 65;
const uint32_t kWrappedKeyBufLen = 256;
const uint32_t kWrappingKeyByteLen = 128/8;
NS_NAMED_LITERAL_STRING(kEcAlgorithm, WEBCRYPTO_NAMED_CURVE_P256);

const PRTime kOneDay = PRTime(PR_USEC_PER_SEC)
                     * PRTime(60)  // sec
                     * PRTime(60)  // min
                     * PRTime(24); // hours
const PRTime kExpirationSlack = kOneDay; // Pre-date for clock skew
const PRTime kExpirationLife = kOneDay;

static mozilla::LazyLogModule gNSSTokenLog("webauth_u2f");

nsNSSU2FToken::nsNSSU2FToken()
  : mInitialized(false)
{}

nsNSSU2FToken::~nsNSSU2FToken()
{
  nsNSSShutDownPreventionLock locker;

  if (isAlreadyShutDown()) {
    return;
  }

  destructorSafeDestroyNSSReference();
  shutdown(calledFromObject);
}

void
nsNSSU2FToken::virtualDestroyNSSReference()
{
  destructorSafeDestroyNSSReference();
}

void
nsNSSU2FToken::destructorSafeDestroyNSSReference()
{
  mWrappingKey = nullptr;
}

static PK11SymKey*
GetSymKeyByNickname(const UniquePK11SlotInfo& aSlot,
                    nsCString aNickname,
                    const nsNSSShutDownPreventionLock&)
{
  MOZ_LOG(gNSSTokenLog, LogLevel::Debug,
          ("Searching for a symmetric key named %s", aNickname.get()));

  PK11SymKey* keyList;
  keyList = PK11_ListFixedKeysInSlot(aSlot.get(),
                                     const_cast<char*>(aNickname.get()),
                                     /* wincx */ nullptr);
  while (keyList) {
    ScopedPK11SymKey freeKey(keyList);

    UniquePORTString freeKeyName(PK11_GetSymKeyNickname(freeKey.get()));

    if (aNickname == freeKeyName.get()) {
      MOZ_LOG(gNSSTokenLog, LogLevel::Debug, ("Symmetric key found!"));
      return freeKey.forget();
    }

    keyList = PK11_GetNextSymKey(keyList);
  }

  MOZ_LOG(gNSSTokenLog, LogLevel::Debug, ("Symmetric key not found."));
  return nullptr;
}

static nsresult
GenEcKeypair(const UniquePK11SlotInfo& aSlot, ScopedSECKEYPrivateKey& aPrivKey,
             ScopedSECKEYPublicKey& aPubKey, const nsNSSShutDownPreventionLock&)
{
  UniquePLArenaPool arena(PORT_NewArena(DER_DEFAULT_CHUNKSIZE));
  if (!arena) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // Set the curve parameters; keyParams belongs to the arena memory space
  SECItem* keyParams = CreateECParamsForCurve(kEcAlgorithm, arena.get());
  if (!keyParams) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // Generate a key pair
  CK_MECHANISM_TYPE mechanism = CKM_EC_KEY_PAIR_GEN;

  SECKEYPublicKey* pubKeyRaw;
  aPrivKey = PK11_GenerateKeyPair(aSlot.get(), mechanism, keyParams, &pubKeyRaw,
                                  /* ephemeral */ PR_FALSE, PR_FALSE,
                                  /* wincx */ nullptr);
  aPubKey = pubKeyRaw;
  if (!aPrivKey.get() || !aPubKey.get()) {
    return NS_ERROR_FAILURE;
  }

  // Check that the public key has the correct length
  if (aPubKey->u.ec.publicValue.len != kPublicKeyLen) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
nsNSSU2FToken::GetOrCreateWrappingKey(const UniquePK11SlotInfo& aSlot,
                                      const nsNSSShutDownPreventionLock& locker)
{
  // Search for an existing wrapping key. If we find it,
  // store it for later and mark ourselves initialized.
  mWrappingKey = GetSymKeyByNickname(aSlot, mSecretNickname, locker);
  if (mWrappingKey) {
    MOZ_LOG(gNSSTokenLog, LogLevel::Debug, ("U2F Soft Token Key found."));
    mInitialized = true;
    return NS_OK;
  }

  MOZ_LOG(gNSSTokenLog, LogLevel::Info,
          ("No keys found. Generating new U2F Soft Token wrapping key."));

  // We did not find an existing wrapping key, so we generate one in the
  // persistent database (e.g, Token).
  mWrappingKey = PK11_TokenKeyGenWithFlags(aSlot.get(), CKM_AES_KEY_GEN,
                                           /* default params */ nullptr,
                                           kWrappingKeyByteLen,
                                           /* empty keyid */ nullptr,
                                           /* flags */ CKF_WRAP | CKF_UNWRAP,
                                           /* attributes */ PK11_ATTR_TOKEN |
                                                            PK11_ATTR_PRIVATE,
                                           /* wincx */ nullptr);

  if (!mWrappingKey) {
      MOZ_LOG(gNSSTokenLog, LogLevel::Warning,
              ("Failed to store wrapping key, NSS error #%d", PORT_GetError()));
    return NS_ERROR_FAILURE;
  }

  SECStatus srv = PK11_SetSymKeyNickname(mWrappingKey, mSecretNickname.get());
  if (srv != SECSuccess) {
      MOZ_LOG(gNSSTokenLog, LogLevel::Warning,
              ("Failed to set nickname, NSS error #%d", PORT_GetError()));
    return NS_ERROR_FAILURE;
  }

  MOZ_LOG(gNSSTokenLog, LogLevel::Debug,
          ("Key stored, nickname set to %s.", mSecretNickname.get()));

  Preferences::SetUint(PREF_U2F_NSSTOKEN_COUNTER, 0);
  return NS_OK;
}

static nsresult
GetAttestationCertificate(const UniquePK11SlotInfo& aSlot,
                          ScopedSECKEYPrivateKey& aAttestPrivKey,
                          ScopedCERTCertificate& aAttestCert,
                          const nsNSSShutDownPreventionLock& locker)
{
  ScopedSECKEYPublicKey pubKey;

  // Construct an ephemeral keypair for this Attestation Certificate
  nsresult rv = GenEcKeypair(aSlot, aAttestPrivKey, pubKey, locker);
  if (NS_FAILED(rv) || !aAttestPrivKey || !pubKey) {
    MOZ_LOG(gNSSTokenLog, LogLevel::Warning,
            ("Failed to gen keypair, NSS error #%d", PORT_GetError()));
    return NS_ERROR_FAILURE;
  }

  // Construct the Attestation Certificate itself
  ScopedCERTName subjectName(CERT_AsciiToName(kAttestCertSubjectName.get()));
  if (!subjectName) {
    MOZ_LOG(gNSSTokenLog, LogLevel::Warning,
            ("Failed to set subject name, NSS error #%d", PORT_GetError()));
      return NS_ERROR_FAILURE;
  }

  ScopedCERTSubjectPublicKeyInfo spki(
      SECKEY_CreateSubjectPublicKeyInfo(pubKey));
  if (!spki) {
    MOZ_LOG(gNSSTokenLog, LogLevel::Warning,
            ("Failed to set SPKI, NSS error #%d", PORT_GetError()));
    return NS_ERROR_FAILURE;
  }

  ScopedCERTCertificateRequest certreq(
      CERT_CreateCertificateRequest(subjectName, spki, nullptr));
  if (!certreq) {
    MOZ_LOG(gNSSTokenLog, LogLevel::Warning,
            ("Failed to gen CSR, NSS error #%d", PORT_GetError()));
    return NS_ERROR_FAILURE;
  }

  PRTime now = PR_Now();
  PRTime notBefore = now - kExpirationSlack;
  PRTime notAfter = now + kExpirationLife;

  ScopedCERTValidity validity(CERT_CreateValidity(notBefore, notAfter));
  if (!validity) {
    MOZ_LOG(gNSSTokenLog, LogLevel::Warning,
            ("Failed to gen validity, NSS error #%d", PORT_GetError()));
    return NS_ERROR_FAILURE;
  }

  unsigned long serial;
  unsigned char* serialBytes = reinterpret_cast<unsigned char *>(&serial);
  SECStatus srv = PK11_GenerateRandomOnSlot(aSlot.get(), serialBytes,
                                            sizeof(serial));
  if (srv != SECSuccess) {
    MOZ_LOG(gNSSTokenLog, LogLevel::Warning,
            ("Failed to gen serial, NSS error #%d", PORT_GetError()));
    return NS_ERROR_FAILURE;
  }
  // Ensure that the most significant bit isn't set (which would
  // indicate a negative number, which isn't valid for serial
  // numbers).
  serialBytes[0] &= 0x7f;
  // Also ensure that the least significant bit on the most
  // significant byte is set (to prevent a leading zero byte,
  // which also wouldn't be valid).
  serialBytes[0] |= 0x01;

  aAttestCert = CERT_CreateCertificate(serial, subjectName, validity, certreq);
  if (!aAttestCert) {
    MOZ_LOG(gNSSTokenLog, LogLevel::Warning,
            ("Failed to gen certificate, NSS error #%d", PORT_GetError()));
    return NS_ERROR_FAILURE;
  }

  PLArenaPool *arena = aAttestCert->arena;

  srv = SECOID_SetAlgorithmID(arena, &aAttestCert->signature,
                              SEC_OID_ANSIX962_ECDSA_SHA256_SIGNATURE,
                              /* wincx */ nullptr);
  if (srv != SECSuccess) {
    return NS_ERROR_FAILURE;
  }

  // Set version to X509v3.
  *(aAttestCert->version.data) = SEC_CERTIFICATE_VERSION_3;
  aAttestCert->version.len = 1;

  SECItem innerDER = { siBuffer, nullptr, 0 };
  if (!SEC_ASN1EncodeItem(arena, &innerDER, aAttestCert,
                          SEC_ASN1_GET(CERT_CertificateTemplate))) {
    return NS_ERROR_FAILURE;
  }

  SECItem *signedCert = PORT_ArenaZNew(arena, SECItem);
  if (!signedCert) {
    return NS_ERROR_FAILURE;
  }

  srv = SEC_DerSignData(arena, signedCert, innerDER.data, innerDER.len,
                        aAttestPrivKey, SEC_OID_ANSIX962_ECDSA_SHA256_SIGNATURE);
  if (srv != SECSuccess) {
    return NS_ERROR_FAILURE;
  }
  aAttestCert->derCert = *signedCert;

  MOZ_LOG(gNSSTokenLog, LogLevel::Debug,
          ("U2F Soft Token attestation certificate generated."));
  return NS_OK;
}

// Set up the context for the soft U2F Token. This is called by NSS
// initialization.
NS_IMETHODIMP
nsNSSU2FToken::Init()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mInitialized);
  if (mInitialized) {
    return NS_ERROR_FAILURE;
  }

  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  UniquePK11SlotInfo slot(PK11_GetInternalKeySlot());
  MOZ_ASSERT(slot.get());

  // Search for an existing wrapping key, or create one.
  nsresult rv = GetOrCreateWrappingKey(slot, locker);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mInitialized = true;
  MOZ_LOG(gNSSTokenLog, LogLevel::Debug, ("U2F Soft Token initialized."));
  return NS_OK;
}

// Convert a Private Key object into an opaque key handle, using AES Key Wrap
// and aWrappingKey to convert aPrivKey.
static SECItem*
KeyHandleFromPrivateKey(const UniquePK11SlotInfo& aSlot,
                        PK11SymKey* aWrappingKey,
                        SECKEYPrivateKey* aPrivKey,
                        const nsNSSShutDownPreventionLock&)
{
  ScopedSECItem wrappedKey(SECITEM_AllocItem(/* default arena */ nullptr,
                                             /* no buffer */ nullptr,
                                             kWrappedKeyBufLen));
  if (!wrappedKey) {
      MOZ_LOG(gNSSTokenLog, LogLevel::Warning,
              ("Failed to allocate memory, NSS error #%d", PORT_GetError()));
    return nullptr;
  }

  ScopedSECItem param(PK11_ParamFromIV(CKM_NSS_AES_KEY_WRAP_PAD,
                                       /* default IV */ nullptr ));

  SECStatus srv = PK11_WrapPrivKey(aSlot.get(), aWrappingKey, aPrivKey,
                                   CKM_NSS_AES_KEY_WRAP_PAD, param,
                                   wrappedKey.get(), /* wincx */ nullptr);
  if (srv != SECSuccess) {
      MOZ_LOG(gNSSTokenLog, LogLevel::Warning,
              ("Failed to wrap U2F key, NSS error #%d", PORT_GetError()));
    return nullptr;
  }

  return wrappedKey.forget();
}

// Convert an opaque key handle aKeyHandle back into a Private Key object, using
// aWrappingKey and the AES Key Wrap algorithm.
static SECKEYPrivateKey*
PrivateKeyFromKeyHandle(const UniquePK11SlotInfo& aSlot,
                        PK11SymKey* aWrappingKey,
                        uint8_t* aKeyHandle, uint32_t aKeyHandleLen,
                        const nsNSSShutDownPreventionLock&)
{
  ScopedAutoSECItem pubKey(kPublicKeyLen);

  ScopedAutoSECItem keyHandleItem(aKeyHandleLen);
  memcpy(keyHandleItem.data, aKeyHandle, keyHandleItem.len);

  ScopedSECItem param(PK11_ParamFromIV(CKM_NSS_AES_KEY_WRAP_PAD,
                                       /* default IV */ nullptr ));

  CK_ATTRIBUTE_TYPE usages[] = { CKA_SIGN };
  int usageCount = 1;

  SECKEYPrivateKey* unwrappedKey;
  unwrappedKey = PK11_UnwrapPrivKey(aSlot.get(), aWrappingKey,
                                    CKM_NSS_AES_KEY_WRAP_PAD,
                                    param, &keyHandleItem,
                                    /* no nickname */ nullptr,
                                    /* discard pubkey */ &pubKey,
                                    /* not permanent */ PR_FALSE,
                                    /* non-exportable */ PR_TRUE,
                                    CKK_EC, usages, usageCount,
                                    /* wincx */ nullptr);
  if (!unwrappedKey) {
    // Not our key.
    MOZ_LOG(gNSSTokenLog, LogLevel::Debug,
            ("Could not unwrap key handle, NSS Error #%d", PORT_GetError()));
    return nullptr;
  }

  return unwrappedKey;
}

// Return whether the provided version is supported by this token.
NS_IMETHODIMP
nsNSSU2FToken::IsCompatibleVersion(const nsAString& aVersion, bool* aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  MOZ_ASSERT(mInitialized);
  *aResult = (mVersion == aVersion);
  return NS_OK;
}

// IsRegistered determines if the provided key handle is usable by this token.
NS_IMETHODIMP
nsNSSU2FToken::IsRegistered(uint8_t* aKeyHandle, uint32_t aKeyHandleLen,
                            bool* aResult)
{
  NS_ENSURE_ARG_POINTER(aKeyHandle);
  NS_ENSURE_ARG_POINTER(aResult);

  if (!NS_IsMainThread()) {
    NS_ERROR("nsNSSU2FToken::IsRegistered called off the main thread");
    return NS_ERROR_NOT_SAME_THREAD;
  }

  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return NS_ERROR_FAILURE;
  }

  MOZ_ASSERT(mInitialized);
  if (!mInitialized) {
    return NS_ERROR_FAILURE;
  }

  UniquePK11SlotInfo slot(PK11_GetInternalSlot());
  MOZ_ASSERT(slot.get());

  // Decode the key handle
  UniqueSECKEYPrivateKey privKey(PrivateKeyFromKeyHandle(slot,
                                                         mWrappingKey.get(),
                                                         aKeyHandle,
                                                         aKeyHandleLen,
                                                         locker));
  *aResult = (privKey.get() != nullptr);
  return NS_OK;
}

// A U2F Register operation causes a new key pair to be generated by the token.
// The token then returns the public key of the key pair, and a handle to the
// private key, which is a fancy way of saying "key wrapped private key", as
// well as the generated attestation certificate and a signature using that
// certificate's private key.
//
// The KeyHandleFromPrivateKey and PrivateKeyFromKeyHandle methods perform
// the actual key wrap/unwrap operations.
//
// The format of the return registration data is as follows:
//
// Bytes  Value
// 1      0x05
// 65     public key
// 1      key handle length
// *      key handle
// ASN.1  attestation certificate
// *      attestation signature
//
NS_IMETHODIMP
nsNSSU2FToken::Register(uint8_t* aApplication,
                        uint32_t aApplicationLen,
                        uint8_t* aChallenge,
                        uint32_t aChallengeLen,
                        uint8_t** aRegistration,
                        uint32_t* aRegistrationLen)
{
  NS_ENSURE_ARG_POINTER(aApplication);
  NS_ENSURE_ARG_POINTER(aChallenge);
  NS_ENSURE_ARG_POINTER(aRegistration);
  NS_ENSURE_ARG_POINTER(aRegistrationLen);

  if (!NS_IsMainThread()) {
    NS_ERROR("nsNSSU2FToken::Register called off the main thread");
    return NS_ERROR_NOT_SAME_THREAD;
  }

  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  MOZ_ASSERT(mInitialized);
  if (!mInitialized) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // We should already have a wrapping key
  MOZ_ASSERT(mWrappingKey);

  UniquePK11SlotInfo slot(PK11_GetInternalSlot());
  MOZ_ASSERT(slot.get());

  // Construct a one-time-use Attestation Certificate
  ScopedSECKEYPrivateKey attestPrivKey;
  ScopedCERTCertificate attestCert;
  nsresult rv = GetAttestationCertificate(slot, attestPrivKey, attestCert,
                                          locker);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_FAILURE;
  }
  MOZ_ASSERT(attestCert);
  MOZ_ASSERT(attestPrivKey);

  // Generate a new keypair; the private will be wrapped into a Key Handle
  ScopedSECKEYPrivateKey privKey;
  ScopedSECKEYPublicKey pubKey;
  rv = GenEcKeypair(slot, privKey, pubKey, locker);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_FAILURE;
  }

  // The key handle will be the result of keywrap(privKey, key=mWrappingKey)
  UniqueSECItem keyHandleItem(KeyHandleFromPrivateKey(slot, mWrappingKey.get(),
                                                      privKey.get(),
                                                      locker));
  if (!keyHandleItem.get()) {
    return NS_ERROR_FAILURE;
  }

  // Sign the challenge using the Attestation privkey (from attestCert)
  mozilla::dom::CryptoBuffer signedDataBuf;
  if (!signedDataBuf.SetCapacity(1 + aApplicationLen + aChallengeLen +
                                 keyHandleItem->len + kPublicKeyLen,
                                 mozilla::fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // It's OK to ignore the return values here because we're writing into
  // pre-allocated space
  signedDataBuf.AppendElement(0x00, mozilla::fallible);
  signedDataBuf.AppendElements(aApplication, aApplicationLen, mozilla::fallible);
  signedDataBuf.AppendElements(aChallenge, aChallengeLen, mozilla::fallible);
  signedDataBuf.AppendSECItem(keyHandleItem.get());
  signedDataBuf.AppendSECItem(pubKey->u.ec.publicValue);

  ScopedSECItem signatureItem(::SECITEM_AllocItem(/* default arena */ nullptr,
                                                  /* no buffer */ nullptr, 0));
  if (!signatureItem) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  SECStatus srv = SEC_SignData(signatureItem.get(), signedDataBuf.Elements(),
                               signedDataBuf.Length(), attestPrivKey.get(),
                               SEC_OID_ANSIX962_ECDSA_SHA256_SIGNATURE);
  if (srv != SECSuccess) {
    MOZ_LOG(gNSSTokenLog, LogLevel::Warning,
            ("Signature failure: %d", PORT_GetError()));
    return NS_ERROR_FAILURE;
  }

  // Serialize the registration data
  mozilla::dom::CryptoBuffer registrationBuf;
  if (!registrationBuf.SetCapacity(1 + kPublicKeyLen + 1 + keyHandleItem->len +
                                   attestCert.get()->derCert.len +
                                   signatureItem->len, mozilla::fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  registrationBuf.AppendElement(0x05, mozilla::fallible);
  registrationBuf.AppendSECItem(pubKey->u.ec.publicValue);
  registrationBuf.AppendElement(keyHandleItem->len, mozilla::fallible);
  registrationBuf.AppendSECItem(keyHandleItem.get());
  registrationBuf.AppendSECItem(attestCert.get()->derCert);
  registrationBuf.AppendSECItem(signatureItem.get());
  if (!registrationBuf.ToNewUnsignedBuffer(aRegistration, aRegistrationLen)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

// A U2F Sign operation creates a signature over the "param" arguments (plus
// some other stuff) using the private key indicated in the key handle argument.
//
// The format of the signed data is as follows:
//
//  32    Application parameter
//  1     User presence (0x01)
//  4     Counter
//  32    Challenge parameter
//
// The format of the signature data is as follows:
//
//  1     User presence
//  4     Counter
//  *     Signature
//
NS_IMETHODIMP
nsNSSU2FToken::Sign(uint8_t* aApplication, uint32_t aApplicationLen,
                    uint8_t* aChallenge, uint32_t aChallengeLen,
                    uint8_t* aKeyHandle, uint32_t aKeyHandleLen,
                    uint8_t** aSignature, uint32_t* aSignatureLen)
{
  NS_ENSURE_ARG_POINTER(aApplication);
  NS_ENSURE_ARG_POINTER(aChallenge);
  NS_ENSURE_ARG_POINTER(aKeyHandle);
  NS_ENSURE_ARG_POINTER(aKeyHandleLen);
  NS_ENSURE_ARG_POINTER(aSignature);
  NS_ENSURE_ARG_POINTER(aSignatureLen);

  if (!NS_IsMainThread()) {
    NS_ERROR("nsNSSU2FToken::Sign called off the main thread");
    return NS_ERROR_NOT_SAME_THREAD;
  }

  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  MOZ_ASSERT(mInitialized);
  if (!mInitialized) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  MOZ_ASSERT(mWrappingKey);

  UniquePK11SlotInfo slot(PK11_GetInternalSlot());
  MOZ_ASSERT(slot.get());

  if ((aChallengeLen != kParamLen) || (aApplicationLen != kParamLen)) {
    MOZ_LOG(gNSSTokenLog, LogLevel::Warning,
            ("Parameter lengths are wrong! challenge=%d app=%d expected=%d",
            aChallengeLen, aApplicationLen, kParamLen));

    return NS_ERROR_ILLEGAL_VALUE;
  }

  // Decode the key handle
  UniqueSECKEYPrivateKey privKey(PrivateKeyFromKeyHandle(slot,
                                                         mWrappingKey.get(),
                                                         aKeyHandle,
                                                         aKeyHandleLen,
                                                         locker));
  if (!privKey.get()) {
    MOZ_LOG(gNSSTokenLog, LogLevel::Warning, ("Couldn't get the priv key!"));
    return NS_ERROR_FAILURE;
  }

  // Increment the counter and turn it into a SECItem
  uint32_t counter = Preferences::GetUint(PREF_U2F_NSSTOKEN_COUNTER) + 1;
  Preferences::SetUint(PREF_U2F_NSSTOKEN_COUNTER, counter);
  ScopedAutoSECItem counterItem(4);
  counterItem.data[0] = (counter >> 24) & 0xFF;
  counterItem.data[1] = (counter >> 16) & 0xFF;
  counterItem.data[2] = (counter >>  8) & 0xFF;
  counterItem.data[3] = (counter >>  0) & 0xFF;

  // Compute the signature
  mozilla::dom::CryptoBuffer signedDataBuf;
  if (!signedDataBuf.SetCapacity(1 + 4 + (2 * kParamLen), mozilla::fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // It's OK to ignore the return values here because we're writing into
  // pre-allocated space
  signedDataBuf.AppendElements(aApplication, aApplicationLen, mozilla::fallible);
  signedDataBuf.AppendElement(0x01, mozilla::fallible);
  signedDataBuf.AppendSECItem(counterItem);
  signedDataBuf.AppendElements(aChallenge, aChallengeLen, mozilla::fallible);

  ScopedSECItem signatureItem(::SECITEM_AllocItem(/* default arena */ nullptr,
                                                  /* no buffer */ nullptr, 0));
  if (!signatureItem) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  SECStatus srv = SEC_SignData(signatureItem.get(), signedDataBuf.Elements(),
                               signedDataBuf.Length(), privKey.get(),
                               SEC_OID_ANSIX962_ECDSA_SHA256_SIGNATURE);
  if (srv != SECSuccess) {
    MOZ_LOG(gNSSTokenLog, LogLevel::Warning,
            ("Signature failure: %d", PORT_GetError()));
    return NS_ERROR_FAILURE;
  }

  // Assmeble the signature data into a buffer for return
  mozilla::dom::CryptoBuffer signatureBuf;
  if (!signatureBuf.SetCapacity(1 + counterItem.len + signatureItem->len,
                                mozilla::fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // It's OK to ignore the return values here because we're writing into
  // pre-allocated space
  signatureBuf.AppendElement(0x01, mozilla::fallible);
  signatureBuf.AppendSECItem(counterItem);
  signatureBuf.AppendSECItem(signatureItem);

  if (!signatureBuf.ToNewUnsignedBuffer(aSignature, aSignatureLen)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}
