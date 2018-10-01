/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <memory>
#include "nss.h"
#include "pk11pub.h"
#include "sechash.h"

#include "cpputil.h"
#include "nss_scoped_ptrs.h"
#include "databuffer.h"

#include "gtest/gtest.h"

namespace nss_test {

// For test vectors.
struct Pkcs11SignatureTestParams {
  const DataBuffer pkcs8_;
  const DataBuffer spki_;
  const DataBuffer data_;
  const DataBuffer signature_;
};

class Pk11SignatureTest : public ::testing::Test {
 protected:
  Pk11SignatureTest(CK_MECHANISM_TYPE mech, SECOidTag hash_oid)
      : mechanism_(mech), hash_oid_(hash_oid) {}

  virtual const SECItem* parameters() const { return nullptr; }
  CK_MECHANISM_TYPE mechanism() const { return mechanism_; }

  ScopedSECKEYPrivateKey ImportPrivateKey(const DataBuffer& pkcs8) {
    ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
    if (!slot) {
      ADD_FAILURE() << "No slot";
      return nullptr;
    }

    SECItem pkcs8Item = {siBuffer, toUcharPtr(pkcs8.data()),
                         static_cast<unsigned int>(pkcs8.len())};

    SECKEYPrivateKey* key = nullptr;
    SECStatus rv = PK11_ImportDERPrivateKeyInfoAndReturnKey(
        slot.get(), &pkcs8Item, nullptr, nullptr, false, false, KU_ALL, &key,
        nullptr);

    if (rv != SECSuccess) {
      return nullptr;
    }

    return ScopedSECKEYPrivateKey(key);
  }

  ScopedSECKEYPublicKey ImportPublicKey(const DataBuffer& spki) {
    SECItem spkiItem = {siBuffer, toUcharPtr(spki.data()),
                        static_cast<unsigned int>(spki.len())};

    ScopedCERTSubjectPublicKeyInfo certSpki(
        SECKEY_DecodeDERSubjectPublicKeyInfo(&spkiItem));

    return ScopedSECKEYPublicKey(SECKEY_ExtractPublicKey(certSpki.get()));
  }

  bool ComputeHash(const DataBuffer& data, DataBuffer* hash) {
    hash->Allocate(static_cast<size_t>(HASH_ResultLenByOidTag(hash_oid_)));
    SECStatus rv =
        PK11_HashBuf(hash_oid_, hash->data(), data.data(), data.len());
    return rv == SECSuccess;
  }

  bool SignHashedData(ScopedSECKEYPrivateKey& privKey, const DataBuffer& hash,
                      DataBuffer* sig) {
    SECItem hashItem = {siBuffer, toUcharPtr(hash.data()),
                        static_cast<unsigned int>(hash.len())};
    int sigLen = PK11_SignatureLen(privKey.get());
    EXPECT_LT(0, sigLen);
    sig->Allocate(static_cast<size_t>(sigLen));
    SECItem sigItem = {siBuffer, toUcharPtr(sig->data()),
                       static_cast<unsigned int>(sig->len())};
    SECStatus rv = PK11_SignWithMechanism(privKey.get(), mechanism_,
                                          parameters(), &sigItem, &hashItem);
    return rv == SECSuccess;
  }

  bool ImportPrivateKeyAndSignHashedData(const DataBuffer& pkcs8,
                                         const DataBuffer& data,
                                         DataBuffer* sig) {
    ScopedSECKEYPrivateKey privKey(ImportPrivateKey(pkcs8));
    if (!privKey) {
      return false;
    }

    DataBuffer hash;
    if (!ComputeHash(data, &hash)) {
      ADD_FAILURE() << "Failed to compute hash";
      return false;
    }
    return SignHashedData(privKey, hash, sig);
  }

  void Verify(const Pkcs11SignatureTestParams& params, const DataBuffer& sig) {
    ScopedSECKEYPublicKey pubKey(ImportPublicKey(params.spki_));
    ASSERT_TRUE(pubKey);

    DataBuffer hash;
    ASSERT_TRUE(ComputeHash(params.data_, &hash));

    // Verify.
    SECItem hashItem = {siBuffer, toUcharPtr(hash.data()),
                        static_cast<unsigned int>(hash.len())};
    SECItem sigItem = {siBuffer, toUcharPtr(sig.data()),
                       static_cast<unsigned int>(sig.len())};
    SECStatus rv = PK11_VerifyWithMechanism(
        pubKey.get(), mechanism_, parameters(), &sigItem, &hashItem, nullptr);
    EXPECT_EQ(rv, SECSuccess);
  }

  void Verify(const Pkcs11SignatureTestParams& params) {
    Verify(params, params.signature_);
  }

  void SignAndVerify(const Pkcs11SignatureTestParams& params) {
    DataBuffer sig;
    ASSERT_TRUE(
        ImportPrivateKeyAndSignHashedData(params.pkcs8_, params.data_, &sig));
    Verify(params, sig);
  }

 private:
  CK_MECHANISM_TYPE mechanism_;
  SECOidTag hash_oid_;
};

}  // namespace nss_test
