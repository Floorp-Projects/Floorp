/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OSKeyStore.h"

#include "mozilla/Base64.h"
#include "mozilla/dom/Promise.h"
#include "nsThreadUtils.h"
#include "nsXPCOM.h"
#include "pk11pub.h"

#if defined(XP_MACOSX)
#  include "KeychainSecret.h"
#elif defined(XP_WIN)
#  include "CredentialManagerSecret.h"
#elif defined(MOZ_WIDGET_GTK)
#  include "LibSecret.h"
#  include "NSSKeyStore.h"
#else
#  include "NSSKeyStore.h"
#endif

NS_IMPL_ISUPPORTS(OSKeyStore, nsIOSKeyStore)

using namespace mozilla;
using dom::Promise;

OSKeyStore::OSKeyStore() : mKs(nullptr), mKsIsNSSKeyStore(false) {
  MOZ_ASSERT(NS_IsMainThread());
  if (NS_WARN_IF(!NS_IsMainThread())) {
    return;
  }

#if defined(XP_MACOSX)
  mKs.reset(new KeychainSecret());
#elif defined(XP_WIN)
  mKs.reset(new CredentialManagerSecret());
#elif defined(MOZ_WIDGET_GTK)
  if (NS_SUCCEEDED(MaybeLoadLibSecret())) {
    mKs.reset(new LibSecret());
  } else {
    mKs.reset(new NSSKeyStore());
    mKsIsNSSKeyStore = true;
  }
#else
  mKs.reset(new NSSKeyStore());
  mKsIsNSSKeyStore = true;
#endif
}

static nsresult GenerateRandom(std::vector<uint8_t>& r) {
  if (r.empty()) {
    return NS_ERROR_INVALID_ARG;
  }
  UniquePK11SlotInfo slot(PK11_GetInternalSlot());
  if (!slot) {
    return NS_ERROR_FAILURE;
  }

  SECStatus srv = PK11_GenerateRandomOnSlot(slot.get(), r.data(), r.size());
  if (srv != SECSuccess) {
    r.clear();
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult OSKeyStore::SecretAvailable(const nsACString& aLabel,
                                     /* out */ bool* aAvailable) {
  NS_ENSURE_STATE(mKs);
  nsAutoCString label = mLabelPrefix + aLabel;
  *aAvailable = mKs->SecretAvailable(label);
  return NS_OK;
}

nsresult OSKeyStore::GenerateSecret(const nsACString& aLabel,
                                    /* out */ nsACString& aRecoveryPhrase) {
  NS_ENSURE_STATE(mKs);
  size_t keyByteLength = mKs->GetKeyByteLength();
  std::vector<uint8_t> secret(keyByteLength);
  nsresult rv = GenerateRandom(secret);
  if (NS_FAILED(rv) || secret.size() != keyByteLength) {
    return NS_ERROR_FAILURE;
  }
  nsAutoCString secretString;
  secretString.Assign(BitwiseCast<char*, uint8_t*>(secret.data()),
                      secret.size());

  nsAutoCString base64;
  rv = Base64Encode(secretString, base64);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsAutoCString label = mLabelPrefix + aLabel;
  rv = mKs->StoreSecret(secretString, label);
  if (NS_FAILED(rv)) {
    return rv;
  }

  aRecoveryPhrase = base64;
  return NS_OK;
}

nsresult OSKeyStore::RecoverSecret(const nsACString& aLabel,
                                   const nsACString& aRecoveryPhrase) {
  NS_ENSURE_STATE(mKs);
  nsAutoCString secret;
  nsresult rv = Base64Decode(aRecoveryPhrase, secret);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (secret.Length() != mKs->GetKeyByteLength()) {
    return NS_ERROR_INVALID_ARG;
  }
  nsAutoCString label = mLabelPrefix + aLabel;
  rv = mKs->StoreSecret(secret, label);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return NS_OK;
}

nsresult OSKeyStore::DeleteSecret(const nsACString& aLabel) {
  NS_ENSURE_STATE(mKs);
  nsAutoCString label = mLabelPrefix + aLabel;
  return mKs->DeleteSecret(label);
}

enum Cipher { Encrypt = true, Decrypt = false };

nsresult OSKeyStore::EncryptBytes(const nsACString& aLabel,
                                  const std::vector<uint8_t>& aInBytes,
                                  /*out*/ nsACString& aEncryptedBase64Text) {
  NS_ENSURE_STATE(mKs);

  nsAutoCString label = mLabelPrefix + aLabel;
  aEncryptedBase64Text.Truncate();
  std::vector<uint8_t> outBytes;
  nsresult rv = mKs->EncryptDecrypt(label, aInBytes, outBytes, Cipher::Encrypt);
  if (NS_FAILED(rv)) {
    return rv;
  }
  nsAutoCString ciphertext;
  ciphertext.Assign(BitwiseCast<char*, uint8_t*>(outBytes.data()),
                    outBytes.size());

  nsAutoCString base64ciphertext;
  rv = Base64Encode(ciphertext, base64ciphertext);
  if (NS_FAILED(rv)) {
    return rv;
  }
  aEncryptedBase64Text.Assign(base64ciphertext);
  return NS_OK;
}

nsresult OSKeyStore::DecryptBytes(const nsACString& aLabel,
                                  const nsACString& aEncryptedBase64Text,
                                  /*out*/ uint32_t* outLen,
                                  /*out*/ uint8_t** outBytes) {
  NS_ENSURE_STATE(mKs);
  NS_ENSURE_ARG_POINTER(outLen);
  NS_ENSURE_ARG_POINTER(outBytes);
  *outLen = 0;
  *outBytes = nullptr;

  nsAutoCString ciphertext;
  nsresult rv = Base64Decode(aEncryptedBase64Text, ciphertext);
  if (NS_FAILED(rv)) {
    return rv;
  }
  nsAutoCString label = mLabelPrefix + aLabel;
  uint8_t* tmp = BitwiseCast<uint8_t*, const char*>(ciphertext.BeginReading());
  const std::vector<uint8_t> ciphertextBytes(tmp, tmp + ciphertext.Length());
  std::vector<uint8_t> plaintextBytes;
  rv = mKs->EncryptDecrypt(label, ciphertextBytes, plaintextBytes,
                           Cipher::Decrypt);
  if (NS_FAILED(rv)) {
    return rv;
  }

  *outBytes = (uint8_t*)moz_xmalloc(plaintextBytes.size());
  memcpy(*outBytes, plaintextBytes.data(), plaintextBytes.size());
  *outLen = plaintextBytes.size();
  return NS_OK;
}

nsresult OSKeyStore::Lock() {
  NS_ENSURE_STATE(mKs);
  return mKs->Lock();
}

nsresult OSKeyStore::Unlock() {
  NS_ENSURE_STATE(mKs);
  return mKs->Unlock();
}

NS_IMETHODIMP
OSKeyStore::GetIsNSSKeyStore(bool* aNSSKeyStore) {
  NS_ENSURE_ARG_POINTER(aNSSKeyStore);
  *aNSSKeyStore = mKsIsNSSKeyStore;
  return NS_OK;
}

// Async interfaces that return promises because the key store implementation
// might block, e.g. asking for a password.

nsresult GetPromise(JSContext* aCx, /* out */ RefPtr<Promise>& aPromise) {
  nsIGlobalObject* globalObject = xpc::CurrentNativeGlobal(aCx);
  if (NS_WARN_IF(!globalObject)) {
    return NS_ERROR_UNEXPECTED;
  }
  ErrorResult result;
  aPromise = Promise::Create(globalObject, result);
  if (NS_WARN_IF(result.Failed())) {
    return result.StealNSResult();
  }
  return NS_OK;
}

void BackgroundUnlock(RefPtr<Promise>& aPromise, RefPtr<OSKeyStore> self) {
  nsAutoCString recovery;
  nsresult rv = self->Unlock();
  nsCOMPtr<nsIRunnable> runnable(NS_NewRunnableFunction(
      "BackgroundUnlockOSKSResolve", [rv, aPromise = std::move(aPromise)]() {
        if (NS_FAILED(rv)) {
          aPromise->MaybeReject(rv);
        } else {
          aPromise->MaybeResolveWithUndefined();
        }
      }));
  NS_DispatchToMainThread(runnable.forget());
}

NS_IMETHODIMP
OSKeyStore::AsyncUnlock(JSContext* aCx, Promise** promiseOut) {
  MOZ_ASSERT(NS_IsMainThread());
  if (!NS_IsMainThread()) {
    return NS_ERROR_NOT_SAME_THREAD;
  }

  NS_ENSURE_ARG_POINTER(aCx);

  RefPtr<Promise> promiseHandle;
  nsresult rv = GetPromise(aCx, promiseHandle);
  if (NS_FAILED(rv)) {
    return rv;
  }

  RefPtr<OSKeyStore> self = this;
  nsCOMPtr<nsIRunnable> runnable(NS_NewRunnableFunction(
      "BackgroundUnlock", [self, promiseHandle]() mutable {
        BackgroundUnlock(promiseHandle, self);
      }));

  promiseHandle.forget(promiseOut);
  return NS_DispatchBackgroundTask(runnable.forget(),
                                   NS_DISPATCH_EVENT_MAY_BLOCK);
}

void BackgroundLock(RefPtr<Promise>& aPromise, RefPtr<OSKeyStore> self) {
  nsresult rv = self->Lock();
  nsCOMPtr<nsIRunnable> runnable(NS_NewRunnableFunction(
      "BackgroundLockOSKSResolve", [rv, aPromise = std::move(aPromise)]() {
        if (NS_FAILED(rv)) {
          aPromise->MaybeReject(rv);
        } else {
          aPromise->MaybeResolveWithUndefined();
        }
      }));
  NS_DispatchToMainThread(runnable.forget());
}

NS_IMETHODIMP
OSKeyStore::AsyncLock(JSContext* aCx, Promise** promiseOut) {
  MOZ_ASSERT(NS_IsMainThread());
  if (!NS_IsMainThread()) {
    return NS_ERROR_NOT_SAME_THREAD;
  }

  NS_ENSURE_ARG_POINTER(aCx);

  RefPtr<Promise> promiseHandle;
  nsresult rv = GetPromise(aCx, promiseHandle);
  if (NS_FAILED(rv)) {
    return rv;
  }

  RefPtr<OSKeyStore> self = this;
  nsCOMPtr<nsIRunnable> runnable(
      NS_NewRunnableFunction("BackgroundLock", [self, promiseHandle]() mutable {
        BackgroundLock(promiseHandle, self);
      }));

  promiseHandle.forget(promiseOut);
  return NS_DispatchBackgroundTask(runnable.forget(),
                                   NS_DISPATCH_EVENT_MAY_BLOCK);
}

void BackgroundGenerateSecret(const nsACString& aLabel,
                              RefPtr<Promise>& aPromise,
                              RefPtr<OSKeyStore> self) {
  nsAutoCString recovery;
  nsresult rv = self->GenerateSecret(aLabel, recovery);
  nsAutoString recoveryString;
  if (NS_SUCCEEDED(rv)) {
    CopyUTF8toUTF16(recovery, recoveryString);
  }
  nsCOMPtr<nsIRunnable> runnable(NS_NewRunnableFunction(
      "BackgroundGenerateSecreteOSKSResolve",
      [rv, aPromise = std::move(aPromise), recoveryString]() {
        if (NS_FAILED(rv)) {
          aPromise->MaybeReject(rv);
        } else {
          aPromise->MaybeResolve(recoveryString);
        }
      }));
  NS_DispatchToMainThread(runnable.forget());
}

NS_IMETHODIMP
OSKeyStore::AsyncGenerateSecret(const nsACString& aLabel, JSContext* aCx,
                                Promise** promiseOut) {
  MOZ_ASSERT(NS_IsMainThread());
  if (!NS_IsMainThread()) {
    return NS_ERROR_NOT_SAME_THREAD;
  }

  NS_ENSURE_ARG_POINTER(aCx);

  RefPtr<Promise> promiseHandle;
  nsresult rv = GetPromise(aCx, promiseHandle);
  if (NS_FAILED(rv)) {
    return rv;
  }

  RefPtr<OSKeyStore> self = this;
  nsCOMPtr<nsIRunnable> runnable(NS_NewRunnableFunction(
      "BackgroundGenerateSecret",
      [self, promiseHandle, aLabel = nsAutoCString(aLabel)]() mutable {
        BackgroundGenerateSecret(aLabel, promiseHandle, self);
      }));

  promiseHandle.forget(promiseOut);
  return NS_DispatchBackgroundTask(runnable.forget(),
                                   NS_DISPATCH_EVENT_MAY_BLOCK);
}

void BackgroundSecretAvailable(const nsACString& aLabel,
                               RefPtr<Promise>& aPromise,
                               RefPtr<OSKeyStore> self) {
  bool available = false;
  nsresult rv = self->SecretAvailable(aLabel, &available);
  nsCOMPtr<nsIRunnable> runnable(NS_NewRunnableFunction(
      "BackgroundSecreteAvailableOSKSResolve",
      [rv, aPromise = std::move(aPromise), available = available]() {
        if (NS_FAILED(rv)) {
          aPromise->MaybeReject(rv);
        } else {
          aPromise->MaybeResolve(available);
        }
      }));
  NS_DispatchToMainThread(runnable.forget());
}

NS_IMETHODIMP
OSKeyStore::AsyncSecretAvailable(const nsACString& aLabel, JSContext* aCx,
                                 Promise** promiseOut) {
  MOZ_ASSERT(NS_IsMainThread());
  if (!NS_IsMainThread()) {
    return NS_ERROR_NOT_SAME_THREAD;
  }

  NS_ENSURE_ARG_POINTER(aCx);

  RefPtr<Promise> promiseHandle;
  nsresult rv = GetPromise(aCx, promiseHandle);
  if (NS_FAILED(rv)) {
    return rv;
  }

  RefPtr<OSKeyStore> self = this;
  nsCOMPtr<nsIRunnable> runnable(NS_NewRunnableFunction(
      "BackgroundSecretAvailable",
      [self, promiseHandle, aLabel = nsAutoCString(aLabel)]() mutable {
        BackgroundSecretAvailable(aLabel, promiseHandle, self);
      }));

  promiseHandle.forget(promiseOut);
  return NS_DispatchBackgroundTask(runnable.forget(),
                                   NS_DISPATCH_EVENT_MAY_BLOCK);
}

void BackgroundRecoverSecret(const nsACString& aLabel,
                             const nsACString& aRecoveryPhrase,
                             RefPtr<Promise>& aPromise,
                             RefPtr<OSKeyStore> self) {
  nsresult rv = self->RecoverSecret(aLabel, aRecoveryPhrase);
  nsCOMPtr<nsIRunnable> runnable(
      NS_NewRunnableFunction("BackgroundRecoverSecreteOSKSResolve",
                             [rv, aPromise = std::move(aPromise)]() {
                               if (NS_FAILED(rv)) {
                                 aPromise->MaybeReject(rv);
                               } else {
                                 aPromise->MaybeResolveWithUndefined();
                               }
                             }));
  NS_DispatchToMainThread(runnable.forget());
}

NS_IMETHODIMP
OSKeyStore::AsyncRecoverSecret(const nsACString& aLabel,
                               const nsACString& aRecoveryPhrase,
                               JSContext* aCx, Promise** promiseOut) {
  MOZ_ASSERT(NS_IsMainThread());
  if (!NS_IsMainThread()) {
    return NS_ERROR_NOT_SAME_THREAD;
  }

  NS_ENSURE_ARG_POINTER(aCx);

  RefPtr<Promise> promiseHandle;
  nsresult rv = GetPromise(aCx, promiseHandle);
  if (NS_FAILED(rv)) {
    return rv;
  }

  RefPtr<OSKeyStore> self = this;
  nsCOMPtr<nsIRunnable> runnable(NS_NewRunnableFunction(
      "BackgroundRecoverSecret",
      [self, promiseHandle, aLabel = nsAutoCString(aLabel),
       aRecoveryPhrase = nsAutoCString(aRecoveryPhrase)]() mutable {
        BackgroundRecoverSecret(aLabel, aRecoveryPhrase, promiseHandle, self);
      }));

  promiseHandle.forget(promiseOut);
  return NS_DispatchBackgroundTask(runnable.forget(),
                                   NS_DISPATCH_EVENT_MAY_BLOCK);
}

void BackgroundDeleteSecret(const nsACString& aLabel, RefPtr<Promise>& aPromise,
                            RefPtr<OSKeyStore> self) {
  nsresult rv = self->DeleteSecret(aLabel);
  nsCOMPtr<nsIRunnable> runnable(
      NS_NewRunnableFunction("BackgroundDeleteSecreteOSKSResolve",
                             [rv, aPromise = std::move(aPromise)]() {
                               if (NS_FAILED(rv)) {
                                 aPromise->MaybeReject(rv);
                               } else {
                                 aPromise->MaybeResolveWithUndefined();
                               }
                             }));
  NS_DispatchToMainThread(runnable.forget());
}

NS_IMETHODIMP
OSKeyStore::AsyncDeleteSecret(const nsACString& aLabel, JSContext* aCx,
                              Promise** promiseOut) {
  MOZ_ASSERT(NS_IsMainThread());
  if (!NS_IsMainThread()) {
    return NS_ERROR_NOT_SAME_THREAD;
  }

  NS_ENSURE_ARG_POINTER(aCx);

  RefPtr<Promise> promiseHandle;
  nsresult rv = GetPromise(aCx, promiseHandle);
  if (NS_FAILED(rv)) {
    return rv;
  }

  RefPtr<OSKeyStore> self = this;
  nsCOMPtr<nsIRunnable> runnable(NS_NewRunnableFunction(
      "BackgroundDeleteSecret",
      [self, promiseHandle, aLabel = nsAutoCString(aLabel)]() mutable {
        BackgroundDeleteSecret(aLabel, promiseHandle, self);
      }));

  promiseHandle.forget(promiseOut);
  return NS_DispatchBackgroundTask(runnable.forget(),
                                   NS_DISPATCH_EVENT_MAY_BLOCK);
}

static void BackgroundEncryptBytes(const nsACString& aLabel,
                                   const std::vector<uint8_t>& aInBytes,
                                   RefPtr<Promise>& aPromise,
                                   RefPtr<OSKeyStore> self) {
  nsAutoCString ciphertext;
  nsresult rv = self->EncryptBytes(aLabel, aInBytes, ciphertext);
  nsAutoString ctext;
  CopyUTF8toUTF16(ciphertext, ctext);

  nsCOMPtr<nsIRunnable> runnable(
      NS_NewRunnableFunction("BackgroundEncryptOSKSResolve",
                             [rv, aPromise = std::move(aPromise), ctext]() {
                               if (NS_FAILED(rv)) {
                                 aPromise->MaybeReject(rv);
                               } else {
                                 aPromise->MaybeResolve(ctext);
                               }
                             }));
  NS_DispatchToMainThread(runnable.forget());
}

NS_IMETHODIMP
OSKeyStore::AsyncEncryptBytes(const nsACString& aLabel,
                              const nsTArray<uint8_t>& inBytes, JSContext* aCx,
                              Promise** promiseOut) {
  MOZ_ASSERT(NS_IsMainThread());
  if (!NS_IsMainThread()) {
    return NS_ERROR_NOT_SAME_THREAD;
  }

  NS_ENSURE_ARG_POINTER(aCx);

  RefPtr<Promise> promiseHandle;
  nsresult rv = GetPromise(aCx, promiseHandle);
  if (NS_FAILED(rv)) {
    return rv;
  }

  RefPtr<OSKeyStore> self = this;
  nsCOMPtr<nsIRunnable> runnable(NS_NewRunnableFunction(
      "BackgroundEncryptBytes",
      [promiseHandle,
       inBytes = std::vector<uint8_t>(inBytes.Elements(),
                                      inBytes.Elements() + inBytes.Length()),
       aLabel = nsAutoCString(aLabel), self]() mutable {
        BackgroundEncryptBytes(aLabel, inBytes, promiseHandle, self);
      }));

  promiseHandle.forget(promiseOut);
  return NS_DispatchBackgroundTask(runnable.forget(),
                                   NS_DISPATCH_EVENT_MAY_BLOCK);
}

void BackgroundDecryptBytes(const nsACString& aLabel,
                            const nsACString& aEncryptedBase64Text,
                            RefPtr<Promise>& aPromise,
                            RefPtr<OSKeyStore> self) {
  uint8_t* plaintext = nullptr;
  uint32_t plaintextLen = 0;
  nsresult rv = self->DecryptBytes(aLabel, aEncryptedBase64Text, &plaintextLen,
                                   &plaintext);
  nsTArray<uint8_t> plain;
  if (plaintext) {
    MOZ_ASSERT(plaintextLen > 0);
    plain.AppendElements(plaintext, plaintextLen);
    free(plaintext);
  }

  nsCOMPtr<nsIRunnable> runnable(NS_NewRunnableFunction(
      "BackgroundDecryptOSKSResolve",
      [rv, aPromise = std::move(aPromise), plain = std::move(plain)]() {
        if (NS_FAILED(rv)) {
          aPromise->MaybeReject(rv);
        } else {
          aPromise->MaybeResolve(plain);
        }
      }));
  NS_DispatchToMainThread(runnable.forget());
}

NS_IMETHODIMP
OSKeyStore::AsyncDecryptBytes(const nsACString& aLabel,
                              const nsACString& aEncryptedBase64Text,
                              JSContext* aCx, Promise** promiseOut) {
  MOZ_ASSERT(NS_IsMainThread());
  if (!NS_IsMainThread()) {
    return NS_ERROR_NOT_SAME_THREAD;
  }

  NS_ENSURE_ARG_POINTER(aCx);

  RefPtr<Promise> promiseHandle;
  nsresult rv = GetPromise(aCx, promiseHandle);
  if (NS_FAILED(rv)) {
    return rv;
  }

  RefPtr<OSKeyStore> self = this;
  nsCOMPtr<nsIRunnable> runnable(NS_NewRunnableFunction(
      "BackgroundDecryptBytes",
      [promiseHandle, self,
       aEncryptedBase64Text = nsAutoCString(aEncryptedBase64Text),
       aLabel = nsAutoCString(aLabel)]() mutable {
        BackgroundDecryptBytes(aLabel, aEncryptedBase64Text, promiseHandle,
                               self);
      }));

  promiseHandle.forget(promiseOut);
  return NS_DispatchBackgroundTask(runnable.forget(),
                                   NS_DISPATCH_EVENT_MAY_BLOCK);
}

// Generic AES-GCM cipher wrapper for NSS functions.

nsresult AbstractOSKeyStore::BuildAesGcmKey(std::vector<uint8_t> aKeyBytes,
                                            /* out */ UniquePK11SymKey& aKey) {
  if (aKeyBytes.size() != mKeyByteLength) {
    return NS_ERROR_INVALID_ARG;
  }

  UniquePK11SlotInfo slot(PK11_GetInternalSlot());
  if (!slot) {
    return NS_ERROR_FAILURE;
  }

  UniqueSECItem key =
      UniqueSECItem(SECITEM_AllocItem(nullptr, nullptr, mKeyByteLength));
  if (!key) {
    return NS_ERROR_FAILURE;
  }
  key->type = siBuffer;
  memcpy(key->data, aKeyBytes.data(), mKeyByteLength);
  key->len = mKeyByteLength;

  UniquePK11SymKey symKey(
      PK11_ImportSymKey(slot.get(), CKM_AES_GCM, PK11_OriginUnwrap,
                        CKA_DECRYPT | CKA_ENCRYPT, key.get(), nullptr));

  if (!symKey) {
    return NS_ERROR_FAILURE;
  }
  aKey.swap(symKey);

  return NS_OK;
}

nsresult AbstractOSKeyStore::DoCipher(const UniquePK11SymKey& aSymKey,
                                      const std::vector<uint8_t>& inBytes,
                                      std::vector<uint8_t>& outBytes,
                                      bool encrypt) {
  NS_ENSURE_ARG_POINTER(aSymKey);
  outBytes.clear();

  // Build params.
  // We need to get the IV from inBytes if we decrypt.
  if (!encrypt && (inBytes.size() < mIVLength || inBytes.size() == 0)) {
    return NS_ERROR_INVALID_ARG;
  }

  const uint8_t* ivp = nullptr;
  std::vector<uint8_t> ivBuf;
  if (encrypt) {
    // Generate a new IV.
    ivBuf.resize(mIVLength);
    nsresult rv = GenerateRandom(ivBuf);
    if (NS_FAILED(rv) || ivBuf.size() != mIVLength) {
      return NS_ERROR_FAILURE;
    }
    ivp = ivBuf.data();
  } else {
    // An IV was passed in. Use the first mIVLength bytes from inBytes as IV.
    ivp = inBytes.data();
  }

  CK_GCM_PARAMS gcm_params;
  gcm_params.pIv = const_cast<unsigned char*>(ivp);
  gcm_params.ulIvLen = mIVLength;
  gcm_params.ulIvBits = gcm_params.ulIvLen * 8;
  gcm_params.ulTagBits = 128;
  gcm_params.pAAD = nullptr;
  gcm_params.ulAADLen = 0;

  SECItem paramsItem = {siBuffer, reinterpret_cast<unsigned char*>(&gcm_params),
                        sizeof(CK_GCM_PARAMS)};

  size_t blockLength = 16;
  outBytes.resize(inBytes.size() + blockLength);
  unsigned int outLen = 0;
  SECStatus srv = SECFailure;
  if (encrypt) {
    srv = PK11_Encrypt(aSymKey.get(), CKM_AES_GCM, &paramsItem, outBytes.data(),
                       &outLen, inBytes.size() + blockLength, inBytes.data(),
                       inBytes.size());
    // Prepend the used IV to the ciphertext.
    Unused << outBytes.insert(outBytes.begin(), ivp, ivp + mIVLength);
    outLen += mIVLength;
  } else {
    // Remove the IV from the input.
    std::vector<uint8_t> input(inBytes);
    input.erase(input.begin(), input.begin() + mIVLength);
    srv = PK11_Decrypt(aSymKey.get(), CKM_AES_GCM, &paramsItem, outBytes.data(),
                       &outLen, input.size() + blockLength, input.data(),
                       input.size());
  }
  if (srv != SECSuccess || outLen > outBytes.size()) {
    outBytes.clear();
    return NS_ERROR_FAILURE;
  }
  if (outLen < outBytes.size()) {
    outBytes.resize(outLen);
  }

  return NS_OK;
}

bool AbstractOSKeyStore::SecretAvailable(const nsACString& aLabel) {
  nsAutoCString secret;
  nsresult rv = RetrieveSecret(aLabel, secret);
  if (NS_FAILED(rv) || secret.Length() == 0) {
    return false;
  }
  return true;
}

nsresult AbstractOSKeyStore::EncryptDecrypt(const nsACString& aLabel,
                                            const std::vector<uint8_t>& inBytes,
                                            std::vector<uint8_t>& outBytes,
                                            bool encrypt) {
  nsAutoCString secret;
  nsresult rv = RetrieveSecret(aLabel, secret);
  if (NS_FAILED(rv) || secret.Length() == 0) {
    return NS_ERROR_FAILURE;
  }

  uint8_t* p = BitwiseCast<uint8_t*, const char*>(secret.BeginReading());
  std::vector<uint8_t> buf(p, p + secret.Length());
  UniquePK11SymKey symKey;
  rv = BuildAesGcmKey(buf, symKey);
  if (NS_FAILED(rv)) {
    return NS_ERROR_FAILURE;
  }
  return DoCipher(symKey, inBytes, outBytes, encrypt);
}
