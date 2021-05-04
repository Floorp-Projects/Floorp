/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <memory>
#include "nss.h"
#include "pk11pub.h"
#include "sechash.h"

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
  Pk11SignatureTest(CK_MECHANISM_TYPE mech, SECOidTag hash_oid,
                    CK_MECHANISM_TYPE combo)
      : mechanism_(mech), hash_oid_(hash_oid), combo_(combo) {
    skip_raw_ = false;
  }

  virtual const SECItem* parameters() const { return nullptr; }
  CK_MECHANISM_TYPE mechanism() const { return mechanism_; }
  void setSkipRaw(bool skip_raw) { skip_raw_ = true; }

  ScopedSECKEYPrivateKey ImportPrivateKey(const DataBuffer& pkcs8);
  ScopedSECKEYPublicKey ImportPublicKey(const DataBuffer& spki);

  bool ComputeHash(const DataBuffer& data, DataBuffer* hash) {
    hash->Allocate(static_cast<size_t>(HASH_ResultLenByOidTag(hash_oid_)));
    SECStatus rv =
        PK11_HashBuf(hash_oid_, hash->data(), data.data(), data.len());
    return rv == SECSuccess;
  }

  bool SignHashedData(ScopedSECKEYPrivateKey& privKey, const DataBuffer& hash,
                      DataBuffer* sig);
  bool SignData(ScopedSECKEYPrivateKey& privKey, const DataBuffer& data,
                DataBuffer* sig);
  bool ImportPrivateKeyAndSignHashedData(const DataBuffer& pkcs8,
                                         const DataBuffer& data,
                                         DataBuffer* sig, DataBuffer* sig2);
  void Verify(const Pkcs11SignatureTestParams& params, const DataBuffer& sig,
              bool valid);

  void Verify(const Pkcs11SignatureTestParams& params, bool valid) {
    Verify(params, params.signature_, valid);
  }

  void Verify(const Pkcs11SignatureTestParams& params) {
    Verify(params, params.signature_, true);
  }

  void SignAndVerify(const Pkcs11SignatureTestParams& params) {
    DataBuffer sig;
    DataBuffer sig2;
    ASSERT_TRUE(ImportPrivateKeyAndSignHashedData(params.pkcs8_, params.data_,
                                                  &sig, &sig2));
    Verify(params, sig, true);
    Verify(params, sig2, true);
  }

 private:
  CK_MECHANISM_TYPE mechanism_;
  SECOidTag hash_oid_;
  CK_MECHANISM_TYPE combo_;
  bool skip_raw_;
};

}  // namespace nss_test
