/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <memory>
#include "nss.h"
#include "pk11pub.h"
#include "prerror.h"
#include "cpputil.h"
#include "nss_scoped_ptrs.h"

#include "testvectors/curve25519-vectors.h"
#include "gtest/gtest.h"

namespace nss_test {

class Pkcs11Curve25519Test
    : public ::testing::TestWithParam<EcdhTestVectorStr> {
 protected:
  void Derive(const uint8_t* pkcs8, size_t pkcs8_len, const uint8_t* spki,
              size_t spki_len, const uint8_t* secret, size_t secret_len,
              bool expect_success) {
    ScopedPK11SlotInfo slot(PK11_GetInternalKeySlot());
    ASSERT_TRUE(slot);

    SECItem pkcs8_item = {siBuffer, toUcharPtr(pkcs8),
                          static_cast<unsigned int>(pkcs8_len)};

    SECKEYPrivateKey* key = nullptr;
    SECStatus rv = PK11_ImportDERPrivateKeyInfoAndReturnKey(
        slot.get(), &pkcs8_item, nullptr, nullptr, false, false, KU_ALL, &key,
        nullptr);
    EXPECT_EQ(SECSuccess, rv);

    ScopedSECKEYPrivateKey priv_key_sess(key);
    ASSERT_TRUE(priv_key_sess);

    SECItem spki_item = {siBuffer, toUcharPtr(spki),
                         static_cast<unsigned int>(spki_len)};

    ScopedCERTSubjectPublicKeyInfo cert_spki(
        SECKEY_DecodeDERSubjectPublicKeyInfo(&spki_item));
    if (!expect_success && !cert_spki) {
      return;
    }
    ASSERT_TRUE(cert_spki);

    ScopedSECKEYPublicKey pub_key_remote(
        SECKEY_ExtractPublicKey(cert_spki.get()));
    ASSERT_TRUE(pub_key_remote);

    // sym_key_sess = ECDH(session_import(private_test), public_test)
    ScopedPK11SymKey sym_key_sess(PK11_PubDeriveWithKDF(
        priv_key_sess.get(), pub_key_remote.get(), false, nullptr, nullptr,
        CKM_ECDH1_DERIVE, CKM_SHA512_HMAC, CKA_DERIVE, 0, CKD_NULL, nullptr,
        nullptr));
    ASSERT_EQ(expect_success, !!sym_key_sess);

    if (expect_success) {
      rv = PK11_ExtractKeyValue(sym_key_sess.get());
      EXPECT_EQ(SECSuccess, rv);

      SECItem* key_data = PK11_GetKeyData(sym_key_sess.get());
      EXPECT_EQ(secret_len, key_data->len);
      EXPECT_EQ(memcmp(key_data->data, secret, secret_len), 0);

      // Perform wrapped export on the imported private, import it as
      // permanent, and verify we derive the same shared secret
      static const uint8_t pw[] = "pw";
      SECItem pwItem = {siBuffer, toUcharPtr(pw), sizeof(pw)};
      ScopedSECKEYEncryptedPrivateKeyInfo epki(PK11_ExportEncryptedPrivKeyInfo(
          slot.get(), SEC_OID_AES_256_CBC, &pwItem, priv_key_sess.get(), 1,
          nullptr));
      ASSERT_NE(nullptr, epki) << "PK11_ExportEncryptedPrivKeyInfo failed: "
                               << PORT_ErrorToName(PORT_GetError());

      ScopedSECKEYPublicKey pub_key_local(
          SECKEY_ConvertToPublicKey(priv_key_sess.get()));

      SECKEYPrivateKey* priv_key_tok = nullptr;
      rv = PK11_ImportEncryptedPrivateKeyInfoAndReturnKey(
          slot.get(), epki.get(), &pwItem, nullptr,
          &pub_key_local->u.ec.publicValue, PR_TRUE, PR_TRUE, ecKey, 0,
          &priv_key_tok, nullptr);
      ASSERT_EQ(SECSuccess, rv) << "PK11_ImportEncryptedPrivateKeyInfo failed "
                                << PORT_ErrorToName(PORT_GetError());
      ASSERT_TRUE(priv_key_tok);

      // sym_key_tok = ECDH(token_import(export(private_test)),
      // public_test)
      ScopedPK11SymKey sym_key_tok(PK11_PubDeriveWithKDF(
          priv_key_tok, pub_key_remote.get(), false, nullptr, nullptr,
          CKM_ECDH1_DERIVE, CKM_SHA512_HMAC, CKA_DERIVE, 0, CKD_NULL, nullptr,
          nullptr));
      EXPECT_TRUE(sym_key_tok);

      if (sym_key_tok) {
        rv = PK11_ExtractKeyValue(sym_key_tok.get());
        EXPECT_EQ(SECSuccess, rv);

        key_data = PK11_GetKeyData(sym_key_tok.get());
        EXPECT_EQ(secret_len, key_data->len);
        EXPECT_EQ(memcmp(key_data->data, secret, secret_len), 0);
      }
      rv = PK11_DeleteTokenPrivateKey(priv_key_tok, true);
      EXPECT_EQ(SECSuccess, rv);
    }
  };

  void Derive(const EcdhTestVectorStr testvector) {
    Derive(testvector.private_key.data(), testvector.private_key.size(),
           testvector.public_key.data(), testvector.public_key.size(),
           testvector.secret.data(), testvector.secret.size(),
           testvector.valid);
  };
};

TEST_P(Pkcs11Curve25519Test, TestVectors) { Derive(GetParam()); }

INSTANTIATE_TEST_SUITE_P(NSSTestVector, Pkcs11Curve25519Test,
                         ::testing::ValuesIn(kCurve25519Vectors));

INSTANTIATE_TEST_SUITE_P(WycheproofTestVector, Pkcs11Curve25519Test,
                         ::testing::ValuesIn(kCurve25519WycheproofVectors));

}  // namespace nss_test
