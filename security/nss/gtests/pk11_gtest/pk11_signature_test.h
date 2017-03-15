/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <memory>
#include "nss.h"
#include "pk11pub.h"
#include "sechash.h"

#include "cpputil.h"
#include "scoped_ptrs.h"

#include "gtest/gtest.h"

namespace nss_test {

class Pk11SignatureTest : public ::testing::Test {
 protected:
  virtual CK_MECHANISM_TYPE mechanism() = 0;
  virtual SECItem* parameters() = 0;
  virtual SECOidTag hashOID() = 0;

  ScopedSECKEYPrivateKey ImportPrivateKey(const uint8_t* pkcs8,
                                          size_t pkcs8_len) {
    ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
    if (!slot) {
      return nullptr;
    }

    SECItem pkcs8Item = {siBuffer, toUcharPtr(pkcs8),
                         static_cast<unsigned int>(pkcs8_len)};

    SECKEYPrivateKey* key = nullptr;
    SECStatus rv = PK11_ImportDERPrivateKeyInfoAndReturnKey(
        slot.get(), &pkcs8Item, nullptr, nullptr, false, false, KU_ALL, &key,
        nullptr);

    if (rv != SECSuccess) {
      return nullptr;
    }

    return ScopedSECKEYPrivateKey(key);
  }

  ScopedSECKEYPublicKey ImportPublicKey(const uint8_t* spki, size_t spki_len) {
    SECItem spkiItem = {siBuffer, toUcharPtr(spki),
                        static_cast<unsigned int>(spki_len)};

    ScopedCERTSubjectPublicKeyInfo certSpki(
        SECKEY_DecodeDERSubjectPublicKeyInfo(&spkiItem));

    return ScopedSECKEYPublicKey(SECKEY_ExtractPublicKey(certSpki.get()));
  }

  ScopedSECItem ComputeHash(const uint8_t* data, size_t len) {
    unsigned int hLen = HASH_ResultLenByOidTag(hashOID());
    ScopedSECItem hash(SECITEM_AllocItem(nullptr, nullptr, hLen));
    if (!hash) {
      return nullptr;
    }

    SECStatus rv = PK11_HashBuf(hashOID(), hash->data, data, len);
    if (rv != SECSuccess) {
      return nullptr;
    }

    return hash;
  }

  ScopedSECItem SignHashedData(ScopedSECKEYPrivateKey& privKey,
                               ScopedSECItem& hash) {
    unsigned int sLen = PK11_SignatureLen(privKey.get());
    ScopedSECItem sig(SECITEM_AllocItem(nullptr, nullptr, sLen));
    if (!sig) {
      return nullptr;
    }

    SECStatus rv = PK11_SignWithMechanism(privKey.get(), mechanism(),
                                          parameters(), sig.get(), hash.get());
    if (rv != SECSuccess) {
      return nullptr;
    }

    return sig;
  }

  ScopedSECItem ImportPrivateKeyAndSignHashedData(const uint8_t* pkcs8,
                                                  size_t pkcs8_len,
                                                  const uint8_t* data,
                                                  size_t data_len) {
    ScopedSECKEYPrivateKey privKey(ImportPrivateKey(pkcs8, pkcs8_len));
    if (!privKey) {
      return nullptr;
    }

    ScopedSECItem hash(ComputeHash(data, data_len));
    if (!hash) {
      return nullptr;
    }

    return ScopedSECItem(SignHashedData(privKey, hash));
  }

  void Verify(const uint8_t* spki, size_t spki_len, const uint8_t* data,
              size_t data_len, const uint8_t* sig, size_t sig_len) {
    ScopedSECKEYPublicKey pubKey(ImportPublicKey(spki, spki_len));
    ASSERT_TRUE(pubKey);

    ScopedSECItem hash(ComputeHash(data, data_len));
    ASSERT_TRUE(hash);

    SECItem sigItem = {siBuffer, toUcharPtr(sig),
                       static_cast<unsigned int>(sig_len)};

    // Verify.
    SECStatus rv = PK11_VerifyWithMechanism(
        pubKey.get(), mechanism(), parameters(), &sigItem, hash.get(), nullptr);
    EXPECT_EQ(rv, SECSuccess);
  }

  void SignAndVerify(const uint8_t* pkcs8, size_t pkcs8_len,
                     const uint8_t* spki, size_t spki_len, const uint8_t* data,
                     size_t data_len) {
    ScopedSECItem sig(
        ImportPrivateKeyAndSignHashedData(pkcs8, pkcs8_len, data, data_len));
    ASSERT_TRUE(sig);

    Verify(spki, spki_len, data, data_len, sig->data, sig->len);
  }
};

#define SIG_TEST_VECTOR_VERIFY(spki, data, sig) \
  Verify(spki, sizeof(spki), data, sizeof(data), sig, sizeof(sig));

#define SIG_TEST_VECTOR_SIGN_VERIFY(pkcs8, spki, data) \
  SignAndVerify(pkcs8, sizeof(pkcs8), spki, sizeof(spki), data, sizeof(data));

}  // namespace nss_test
