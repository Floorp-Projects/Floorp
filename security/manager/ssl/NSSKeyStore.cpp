/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NSSKeyStore.h"

#include "mozilla/Base64.h"
#include "mozilla/SyncRunnable.h"
#include "nsNSSComponent.h"
#include "nsPK11TokenDB.h"
#include "nsXULAppAPI.h"

/* Implementing OSKeyStore when there is no platform specific one.
 * This key store instead puts the keys into the NSS DB.
 */

using namespace mozilla;
using mozilla::SyncRunnable;

LazyLogModule gNSSKeyStoreLog("nsskeystore");

NSSKeyStore::NSSKeyStore() {
  MOZ_ASSERT(XRE_IsParentProcess());
  if (!XRE_IsParentProcess()) {
    // This shouldn't happen as this is only initialised when creating the
    // OSKeyStore, which is ParentProcessOnly.
    return;
  }
  Unused << EnsureNSSInitializedChromeOrContent();
  Unused << InitToken();
}
NSSKeyStore::~NSSKeyStore() {}

nsresult NSSKeyStore::InitToken() {
  if (!mSlot) {
    mSlot = UniquePK11SlotInfo(PK11_GetInternalKeySlot());
    if (!mSlot) {
      MOZ_LOG(gNSSKeyStoreLog, LogLevel::Debug,
              ("Error getting internal key slot"));
      return NS_ERROR_NOT_AVAILABLE;
    }
  }
  return NS_OK;
}

nsresult NSSKeyStoreMainThreadLock(PK11SlotInfo* aSlot) {
  nsCOMPtr<nsIPK11Token> token = new nsPK11Token(aSlot);
  return token->LogoutSimple();
}

nsresult NSSKeyStore::Lock() {
  NS_ENSURE_STATE(mSlot);

  if (!NS_IsMainThread()) {
    nsCOMPtr<nsIThread> mainThread;
    nsresult rv = NS_GetMainThread(getter_AddRefs(mainThread));
    if (NS_FAILED(rv)) {
      return NS_ERROR_FAILURE;
    }

    // Forward to the main thread synchronously.
    SyncRunnable::DispatchToThread(
        mainThread, new SyncRunnable(NS_NewRunnableFunction(
                        "NSSKeyStoreMainThreadLock", [slot = mSlot.get()]() {
                          NSSKeyStoreMainThreadLock(slot);
                        })));

    return NS_OK;
  }

  return NSSKeyStoreMainThreadLock(mSlot.get());
}

nsresult NSSKeyStoreMainThreadUnlock(PK11SlotInfo* aSlot) {
  nsCOMPtr<nsIPK11Token> token = new nsPK11Token(aSlot);
  return NS_FAILED(token->Login(false /* force */)) ? NS_ERROR_FAILURE : NS_OK;
}

nsresult NSSKeyStore::Unlock() {
  NS_ENSURE_STATE(mSlot);

  if (!NS_IsMainThread()) {
    nsCOMPtr<nsIThread> mainThread;
    nsresult rv = NS_GetMainThread(getter_AddRefs(mainThread));
    if (NS_FAILED(rv)) {
      return NS_ERROR_FAILURE;
    }

    // Forward to the main thread synchronously.
    nsresult result = NS_ERROR_FAILURE;
    SyncRunnable::DispatchToThread(
        mainThread, new SyncRunnable(NS_NewRunnableFunction(
                        "NSSKeyStoreMainThreadUnlock",
                        [slot = mSlot.get(), result = &result]() {
                          *result = NSSKeyStoreMainThreadUnlock(slot);
                        })));

    return result;
  }

  return NSSKeyStoreMainThreadUnlock(mSlot.get());
}

nsresult NSSKeyStore::StoreSecret(const nsACString& aSecret,
                                  const nsACString& aLabel) {
  NS_ENSURE_STATE(mSlot);
  if (NS_FAILED(Unlock())) {
    MOZ_LOG(gNSSKeyStoreLog, LogLevel::Debug, ("Error unlocking NSS key db"));
    return NS_ERROR_FAILURE;
  }

  // It is possible for multiple keys to have the same nickname in NSS. To
  // prevent the problem of not knowing which key to use in the future, simply
  // delete all keys with this nickname before storing a new one (if something
  // else is using our prefix ("org.mozilla.nss.keystore") with the given label,
  // it may result in breakage).
  nsresult rv = DeleteSecret(aLabel);
  if (NS_FAILED(rv)) {
    MOZ_LOG(gNSSKeyStoreLog, LogLevel::Debug,
            ("DeleteSecret before StoreSecret failed"));
    return rv;
  }

  uint8_t* p = BitwiseCast<uint8_t*, const char*>(aSecret.BeginReading());
  UniqueSECItem key(SECITEM_AllocItem(nullptr, nullptr, aSecret.Length()));
  if (!key) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  key->type = siBuffer;
  memcpy(key->data, p, aSecret.Length());
  key->len = aSecret.Length();
  UniquePK11SymKey symKey(
      PK11_ImportSymKey(mSlot.get(), CKM_AES_GCM, PK11_OriginUnwrap,
                        CKA_DECRYPT | CKA_ENCRYPT, key.get(), nullptr));
  if (!symKey) {
    MOZ_LOG(gNSSKeyStoreLog, LogLevel::Debug, ("Error creating NSS SymKey"));
    return NS_ERROR_FAILURE;
  }
  UniquePK11SymKey storedKey(
      PK11_ConvertSessionSymKeyToTokenSymKey(symKey.get(), nullptr));
  if (!storedKey) {
    MOZ_LOG(gNSSKeyStoreLog, LogLevel::Debug,
            ("Error storing NSS SymKey in DB"));
    return NS_ERROR_FAILURE;
  }
  SECStatus srv =
      PK11_SetSymKeyNickname(storedKey.get(), PromiseFlatCString(aLabel).get());
  if (srv != SECSuccess) {
    MOZ_LOG(gNSSKeyStoreLog, LogLevel::Debug, ("Error naming NSS SymKey"));
    (void)PK11_DeleteTokenSymKey(storedKey.get());
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult NSSKeyStore::DeleteSecret(const nsACString& aLabel) {
  NS_ENSURE_STATE(mSlot);
  if (NS_FAILED(Unlock())) {
    MOZ_LOG(gNSSKeyStoreLog, LogLevel::Debug, ("Error unlocking NSS key db"));
    return NS_ERROR_FAILURE;
  }

  UniquePK11SymKey symKey(PK11_ListFixedKeysInSlot(
      mSlot.get(), const_cast<char*>(PromiseFlatCString(aLabel).get()),
      nullptr));
  if (!symKey) {
    // Couldn't find the key or something is wrong. Be nice.
    return NS_OK;
  }
  for (PK11SymKey* tmp = symKey.get(); tmp; tmp = PK11_GetNextSymKey(tmp)) {
    SECStatus srv = PK11_DeleteTokenSymKey(tmp);
    if (srv != SECSuccess) {
      MOZ_LOG(gNSSKeyStoreLog, LogLevel::Debug, ("Error deleting NSS SymKey"));
      return NS_ERROR_FAILURE;
    }
  }
  return NS_OK;
}

bool NSSKeyStore::SecretAvailable(const nsACString& aLabel) {
  if (!mSlot) {
    return false;
  }
  if (NS_FAILED(Unlock())) {
    MOZ_LOG(gNSSKeyStoreLog, LogLevel::Debug, ("Error unlocking NSS key db"));
    return false;
  }

  UniquePK11SymKey symKey(PK11_ListFixedKeysInSlot(
      mSlot.get(), const_cast<char*>(PromiseFlatCString(aLabel).get()),
      nullptr));
  if (!symKey) {
    return false;
  }
  return true;
}

nsresult NSSKeyStore::EncryptDecrypt(const nsACString& aLabel,
                                     const std::vector<uint8_t>& inBytes,
                                     std::vector<uint8_t>& outBytes,
                                     bool encrypt) {
  NS_ENSURE_STATE(mSlot);
  if (NS_FAILED(Unlock())) {
    MOZ_LOG(gNSSKeyStoreLog, LogLevel::Debug, ("Error unlocking NSS key db"));
    return NS_ERROR_FAILURE;
  }

  UniquePK11SymKey symKey(PK11_ListFixedKeysInSlot(
      mSlot.get(), const_cast<char*>(PromiseFlatCString(aLabel).get()),
      nullptr));
  if (!symKey) {
    MOZ_LOG(gNSSKeyStoreLog, LogLevel::Debug,
            ("Error finding key for given label"));
    return NS_ERROR_FAILURE;
  }
  return DoCipher(symKey, inBytes, outBytes, encrypt);
}

// Because NSSKeyStore overrides AbstractOSKeyStore's EncryptDecrypt and
// SecretAvailable functions, this isn't necessary.
nsresult NSSKeyStore::RetrieveSecret(const nsACString& aLabel,
                                     /* out */ nsACString& aSecret) {
  return NS_ERROR_NOT_IMPLEMENTED;
}
