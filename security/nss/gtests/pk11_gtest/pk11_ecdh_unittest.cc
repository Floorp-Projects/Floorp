/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>
#include <memory>
#include "nss.h"
#include "pk11pub.h"
#include "prerror.h"

#include "cpputil.h"
#include "json_reader.h"
#include "gtest/gtest.h"
#include "nss_scoped_ptrs.h"
#include "testvectors_base/test-structs.h"

namespace nss_test {

class Pkcs11EcdhTest : public ::testing::Test {
 protected:
  void Derive(const std::string& curve, const EcdhTestVector& vec) {
    std::cout << "Run test " << vec.id << std::endl;

    SECItem spki_item = {siBuffer, toUcharPtr(vec.public_key.data()),
                         static_cast<unsigned int>(vec.public_key.size())};
    ScopedCERTSubjectPublicKeyInfo cert_spki(
        SECKEY_DecodeDERSubjectPublicKeyInfo(&spki_item));
    if (vec.valid) {
      ASSERT_TRUE(!!cert_spki);
    } else if (!cert_spki) {
      ASSERT_TRUE(vec.invalid_asn);
      return;
    }

    ScopedSECKEYPublicKey pub_key(SECKEY_ExtractPublicKey(cert_spki.get()));
    if (vec.valid) {
      ASSERT_TRUE(!!pub_key);
    } else if (!pub_key) {
      return;
    }

    ScopedSECKEYPrivateKey priv_key = ImportPrivateKey(curve, vec);
    if (vec.valid) {
      ASSERT_TRUE(priv_key);
    } else if (!priv_key) {
      return;
    }

    ScopedPK11SymKey sym_key(
        PK11_PubDeriveWithKDF(priv_key.get(), pub_key.get(), false, nullptr,
                              nullptr, CKM_ECDH1_DERIVE, CKM_SHA512_HMAC,
                              CKA_DERIVE, 0, CKD_NULL, nullptr, nullptr));
    if (vec.valid) {
      ASSERT_TRUE(!!sym_key);

      SECStatus rv = PK11_ExtractKeyValue(sym_key.get());
      EXPECT_EQ(SECSuccess, rv);

      SECItem expect_item = {siBuffer, toUcharPtr(vec.secret.data()),
                             static_cast<unsigned int>(vec.secret.size())};

      SECItem* derived_key = PK11_GetKeyData(sym_key.get());
      EXPECT_EQ(0, SECITEM_CompareItem(derived_key, &expect_item));
    } else if (!vec.invalid_asn) {
      // Invalid encodings could produce an output if we get here, so only
      // check when the encoding is valid.
      ASSERT_FALSE(!!sym_key);
    }
  };

  static void ReadTestAttr(EcdhTestVector& t, const std::string& n,
                           JsonReader& r) {
    if (n == "public") {
      t.public_key = r.ReadHex();
    } else if (n == "private") {
      t.private_key = r.ReadHex();
    } else if (n == "shared") {
      t.secret = r.ReadHex();
    } else {
      FAIL() << "unsupported test case field: " << n;
    }
  }

  void RunGroup(JsonReader& r) {
    std::vector<EcdhTestVector> tests;
    std::string curve;
    while (r.NextItem()) {
      std::string n = r.ReadLabel();
      if (n == "") {
        break;
      }
      if (n == "curve") {
        curve = r.ReadString();
      } else if (n == "encoding") {
        ASSERT_EQ("asn", r.ReadString());
      } else if (n == "type") {
        ASSERT_EQ("EcdhTest", r.ReadString());
      } else if (n == "tests") {
        WycheproofReadTests(r, &tests, ReadTestAttr, false,
                            [](EcdhTestVector& t, const std::string&,
                               const std::vector<std::string>& flags) {
                              t.invalid_asn =
                                  std::find(flags.begin(), flags.end(),
                                            "InvalidAsn") != flags.end();
                            });
      } else {
        FAIL() << "unknown group label: " << n;
      }
    }

    for (auto& t : tests) {
      Derive(curve, t);
    }
  }

  void Run(const std::string& file) {
    WycheproofHeader(file, "ECDH", "ecdh_test_schema.json",
                     [this](JsonReader& r) { RunGroup(r); });
  }

 private:
  void OidForCurve(const std::string& curve, std::vector<uint8_t>* der) {
    SECOidTag tag;
    if (curve == "secp256r1") {
      tag = SEC_OID_SECG_EC_SECP256R1;
    } else if (curve == "secp384r1") {
      tag = SEC_OID_SECG_EC_SECP384R1;
    } else if (curve == "secp521r1") {
      tag = SEC_OID_SECG_EC_SECP521R1;
    } else {
      FAIL() << "unknown curve: " << curve;
    }
    SECOidData* oid_data = SECOID_FindOIDByTag(tag);
    ASSERT_TRUE(oid_data);
    der->push_back(SEC_ASN1_OBJECT_ID);
    der->push_back(oid_data->oid.len);
    der->insert(der->end(), oid_data->oid.data,
                oid_data->oid.data + oid_data->oid.len);
  }

  // Construct a garbage public value for the given curve.
  // NSS needs a value for this, but it doesn't care what it is.
  void PublicValue(const std::string& curve, std::vector<uint8_t>* der) {
    size_t len;
    if (curve == "secp256r1") {
      len = 32;
    } else if (curve == "secp384r1") {
      len = 48;
    } else if (curve == "secp521r1") {
      len = 64;
    } else {
      FAIL() << "unknown curve: " << curve;
    }
    der->push_back(0x04);
    for (size_t i = 0; i < len * 2; ++i) {
      der->push_back(0x00);
    }
  }

  void InsertLength(std::vector<uint8_t>* der, size_t offset) {
    size_t len = der->size() - offset;
    ASSERT_GT(256u, len) << "unsupported length for DER";
    if (len > 127) {
      der->insert(der->begin() + offset, 0x81);
      offset++;
    }
    der->insert(der->begin() + offset, static_cast<uint8_t>(len));
  }

  // A very hacking PKCS#8 encoder that is sufficient to dupe NSS into
  // thinking that it is a valid EC private key.
  std::vector<uint8_t> BuildDerPrivateKey(const std::string& curve,
                                          const EcdhTestVector& vec) {
    std::vector<uint8_t> der;
    std::vector<size_t> length_inserts;

    der.push_back(0x30);
    length_inserts.push_back(der.size());
    der.insert(der.end(), {0x02, 0x01, 0x00, 0x30});
    size_t oid_length_insert = der.size();
    der.insert(der.end(),
               {0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01});
    OidForCurve(curve, &der);
    InsertLength(&der, oid_length_insert);

    der.push_back(0x04);
    length_inserts.push_back(der.size());
    der.push_back(0x30);
    length_inserts.push_back(der.size());

    der.insert(der.end(), {0x02, 0x01, 0x01, 0x04});
    der.push_back(vec.private_key.size());
    der.insert(der.end(), vec.private_key.begin(), vec.private_key.end());

    der.push_back(0xa1);
    length_inserts.push_back(der.size());
    der.push_back(0x03);
    length_inserts.push_back(der.size());
    der.push_back(0x00);
    PublicValue(curve, &der);

    for (auto i = length_inserts.rbegin(); i != length_inserts.rend(); ++i) {
      InsertLength(&der, *i);
    }
    return der;
  }

  ScopedSECKEYPrivateKey ImportPrivateKey(const std::string& curve,
                                          const EcdhTestVector& vec) {
    ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
    EXPECT_TRUE(slot);
    if (!slot) {
      return nullptr;
    }

    std::vector<uint8_t> der = BuildDerPrivateKey(curve, vec);
    SECItem der_item = {siBuffer, const_cast<uint8_t*>(der.data()),
                        static_cast<unsigned int>(der.size())};
    SECKEYPrivateKey* key = nullptr;
    SECStatus rv = PK11_ImportDERPrivateKeyInfoAndReturnKey(
        slot.get(), &der_item, nullptr, nullptr, false, true, KU_KEY_AGREEMENT,
        &key, nullptr);
    if (vec.valid) {
      EXPECT_EQ(SECSuccess, rv)
          << "unable to load private key DER for test " << vec.id << ": "
          << PORT_ErrorToString(PORT_GetError());
    }

    return ScopedSECKEYPrivateKey(key);
  }
};

TEST_F(Pkcs11EcdhTest, P256) { Run("ecdh_secp256r1"); }
TEST_F(Pkcs11EcdhTest, P384) { Run("ecdh_secp384r1"); }
TEST_F(Pkcs11EcdhTest, P521) { Run("ecdh_secp521r1"); }

}  // namespace nss_test
