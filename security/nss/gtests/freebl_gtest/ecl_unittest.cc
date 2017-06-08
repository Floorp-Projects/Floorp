// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at http://mozilla.org/MPL/2.0/.

#include "gtest/gtest.h"

#include <stdint.h>

#include "blapi.h"
#include "scoped_ptrs.h"
#include "secerr.h"

namespace nss_test {

class ECLTest : public ::testing::Test {
 protected:
  const ECCurveName GetCurveName(std::string name) {
    if (name == "P256") return ECCurve_NIST_P256;
    if (name == "P384") return ECCurve_NIST_P384;
    if (name == "P521") return ECCurve_NIST_P521;
    return ECCurve_pastLastCurve;
  }
  std::vector<uint8_t> hexStringToBytes(std::string s) {
    std::vector<uint8_t> bytes;
    for (size_t i = 0; i < s.length(); i += 2) {
      bytes.push_back(std::stoul(s.substr(i, 2), nullptr, 16));
    }
    return bytes;
  }
  std::string bytesToHexString(std::vector<uint8_t> bytes) {
    std::stringstream s;
    for (auto b : bytes) {
      s << std::setfill('0') << std::setw(2) << std::uppercase << std::hex
        << static_cast<int>(b);
    }
    return s.str();
  }
  void ecName2params(const std::string curve, SECItem *params) {
    SECOidData *oidData = nullptr;

    switch (GetCurveName(curve)) {
      case ECCurve_NIST_P256:
        oidData = SECOID_FindOIDByTag(SEC_OID_ANSIX962_EC_PRIME256V1);
        break;
      case ECCurve_NIST_P384:
        oidData = SECOID_FindOIDByTag(SEC_OID_SECG_EC_SECP384R1);
        break;
      case ECCurve_NIST_P521:
        oidData = SECOID_FindOIDByTag(SEC_OID_SECG_EC_SECP521R1);
        break;
      default:
        FAIL();
    }
    ASSERT_NE(oidData, nullptr);

    if (SECITEM_AllocItem(nullptr, params, (2 + oidData->oid.len)) == nullptr) {
      FAIL() << "Couldn't allocate memory for OID.";
    }
    params->data[0] = SEC_ASN1_OBJECT_ID;
    params->data[1] = oidData->oid.len;
    memcpy(params->data + 2, oidData->oid.data, oidData->oid.len);
  }

  void TestECDH_Derive(const std::string p, const std::string secret,
                       const std::string group_name, const std::string result,
                       const SECStatus expected_status) {
    ECParams ecParams = {0};
    ScopedSECItem ecEncodedParams(SECITEM_AllocItem(nullptr, nullptr, 0U));
    ScopedPLArenaPool arena(PORT_NewArena(DER_DEFAULT_CHUNKSIZE));

    ASSERT_TRUE(arena && ecEncodedParams);

    ecName2params(group_name, ecEncodedParams.get());
    EC_FillParams(arena.get(), ecEncodedParams.get(), &ecParams);

    std::vector<uint8_t> p_bytes = hexStringToBytes(p);
    ASSERT_GT(p_bytes.size(), 0U);
    SECItem public_value = {siBuffer, p_bytes.data(),
                            static_cast<unsigned int>(p_bytes.size())};

    std::vector<uint8_t> secret_bytes = hexStringToBytes(secret);
    ASSERT_GT(secret_bytes.size(), 0U);
    SECItem secret_value = {siBuffer, secret_bytes.data(),
                            static_cast<unsigned int>(secret_bytes.size())};

    ScopedSECItem derived_secret(SECITEM_AllocItem(nullptr, nullptr, 0U));

    SECStatus rv = ECDH_Derive(&public_value, &ecParams, &secret_value, false,
                               derived_secret.get());
    ASSERT_EQ(expected_status, rv);
    if (expected_status != SECSuccess) {
      // Abort when we expect an error.
      return;
    }

    std::string derived_result = bytesToHexString(std::vector<uint8_t>(
        derived_secret->data, derived_secret->data + derived_secret->len));
    std::cout << "derived secret: " << derived_result << std::endl;
    EXPECT_EQ(derived_result, result);
  }
};

TEST_F(ECLTest, TestECDH_DeriveP256) {
  TestECDH_Derive(
      "045ce5c643dffa402bc1837bbcbc223e51d06f20200470d341adfa9deed1bba10e850a16"
      "368b673732a5c220a778990b22a0e74cdc3b22c7410b9dd552a5635497",
      "971", "P256", "0", SECFailure);
}
TEST_F(ECLTest, TestECDH_DeriveP521) {
  TestECDH_Derive(
      "04"
      "00c6858e06b70404e9cd9e3ecb662395b4429c648139053fb521f828af606b4d3dbaa14b"
      "5e77efe75928fe1dc127a2ffa8de3348b3c1856a429bf97e7e31c2e5bd66"
      "011839296a789a3bc0045c8a5fb42c7d1bd998f54449579b446817afbd17273e662c97ee"
      "72995ef42640c550b9013fad0761353c7086a272c24088be94769fd16650",
      "01fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffa5186"
      "8783bf2f966b7fcc0148f709a5d03bb5c9b8899c47aebb6fb71e913863f7",
      "P521",
      "01BC33425E72A12779EACB2EDCC5B63D1281F7E86DBC7BF99A7ABD0CFE367DE4666D6EDB"
      "B8525BFFE5222F0702C3096DEC0884CE572F5A15C423FDF44D01DD99C61D",
      SECSuccess);
}

}  // nss_test
