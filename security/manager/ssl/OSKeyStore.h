/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Generic key store implementation for platforms that we don't support with OS
// specific implementations.

#ifndef OSKeyStore_h
#define OSKeyStore_h

#include "nsCOMPtr.h"
#include "nsIOSKeyStore.h"
#include "nsString.h"
#include "ScopedNSSTypes.h"

#include <memory>
#include <vector>

class AbstractOSKeyStore {
 public:
  // Retrieve a secret with the given label.
  virtual nsresult RetrieveSecret(const nsACString& aLabel,
                                  /* out */ nsACString& aSecret) = 0;
  // Store a new secret with the given label.
  virtual nsresult StoreSecret(const nsACString& secret,
                               const nsACString& label) = 0;
  // Delete the secret with the given label.
  virtual nsresult DeleteSecret(const nsACString& label) = 0;
  virtual ~AbstractOSKeyStore() = default;

  // Returns true if the secret with the given label is available in the key
  // store, false otherwise.
  virtual bool SecretAvailable(const nsACString& label);
  // Perform encryption or decryption operation with the given secret and input
  // bytes. The output is written in outBytes. This function can make use of the
  // AesGcm class to use NSS for encryption and decryption.
  virtual nsresult EncryptDecrypt(const nsACString& label,
                                  const std::vector<uint8_t>& inBytes,
                                  std::vector<uint8_t>& outBytes, bool encrypt);

  size_t GetKeyByteLength() { return mKeyByteLength; }

 protected:
  /* These helper functions are implemented in OSKeyStore.cpp and implement
   * common functionality of the abstract key store to encrypt and decrypt.
   */
  nsresult DoCipher(const mozilla::UniquePK11SymKey& aSymKey,
                    const std::vector<uint8_t>& inBytes,
                    std::vector<uint8_t>& outBytes, bool aEncrypt);
  nsresult BuildAesGcmKey(std::vector<uint8_t> keyBytes,
                          /* out */ mozilla::UniquePK11SymKey& aKey);

 private:
  const size_t mKeyByteLength = 16;
  const size_t mIVLength = 12;
};

#define NS_OSKEYSTORE_CONTRACTID "@mozilla.org/security/oskeystore;1"
#define NS_OSKEYSTORE_CID                            \
  {                                                  \
    0x57972956, 0x5718, 0x42d2, {                    \
      0x80, 0x70, 0xb3, 0xfc, 0x72, 0x21, 0x2e, 0xaf \
    }                                                \
  }

nsresult GetPromise(JSContext* aCx,
                    /* out */ RefPtr<mozilla::dom::Promise>& aPromise);

class OSKeyStore final : public nsIOSKeyStore {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOSKEYSTORE

  OSKeyStore();
  nsresult GenerateSecret(const nsACString& aLabel,
                          /* out */ nsACString& aRecoveryPhrase);
  nsresult SecretAvailable(const nsACString& aLabel,
                           /* out */ bool* aAvailable);
  nsresult RecoverSecret(const nsACString& aLabel,
                         const nsACString& aRecoveryPhrase);
  nsresult DeleteSecret(const nsACString& aLabel);
  nsresult EncryptBytes(const nsACString& aLabel,
                        const std::vector<uint8_t>& aInBytes,
                        /*out*/ nsACString& aEncryptedBase64Text);
  nsresult DecryptBytes(const nsACString& aLabel,
                        const nsACString& aEncryptedBase64Text,
                        /*out*/ uint32_t* outLen,
                        /*out*/ uint8_t** outBytes);

 private:
  ~OSKeyStore() = default;

  std::unique_ptr<AbstractOSKeyStore> mKs;
};

#endif  // OSKeyStore_h
