/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "pk11pub.h"
#include "nssutil.h"
#include <stdio.h>
#include "prerror.h"
#include "nss.h"
#include "gtest/gtest.h"
#include "nss_scoped_ptrs.h"
#include "cpputil.h"
#include "databuffer.h"
#include "util.h"

#define MAX_KEY_SIZE 24

namespace nss_test {

static const uint8_t kIv[] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
                              0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,
                              0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77};
static const uint8_t kInput[] = {
    0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00, 0xff, 0xee, 0xdd, 0xcc,
    0xbb, 0xaa, 0x99, 0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00};

class EncryptDeriveTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<CK_MECHANISM_TYPE> {
 public:
  void TestEncryptDerive() {
    ScopedPK11SymKey derived_key(PK11_Derive(key_.get(), derive_mech(),
                                             derive_param(), encrypt_mech(),
                                             CKA_DECRYPT, keysize()));
    ASSERT_TRUE(derived_key);

    uint8_t derived_key_data[MAX_KEY_SIZE];
    ASSERT_GE(sizeof(derived_key_data), keysize());
    GetKeyData(derived_key, derived_key_data, keysize());
    RemoveChecksum(derived_key_data);

    uint8_t reference_key_data[MAX_KEY_SIZE];
    unsigned int reference_len = 0;
    SECStatus rv = PK11_Encrypt(key_.get(), encrypt_mech(), encrypt_param(),
                                reference_key_data, &reference_len, keysize(),
                                kInput, keysize());
    ASSERT_EQ(SECSuccess, rv);
    ASSERT_EQ(keysize(), static_cast<size_t>(reference_len));
    RemoveChecksum(reference_key_data);

    EXPECT_EQ(DataBuffer(reference_key_data, keysize()),
              DataBuffer(derived_key_data, keysize()));
  }

 protected:
  unsigned int keysize() const { return 16; }

 private:
  CK_MECHANISM_TYPE encrypt_mech() const { return GetParam(); }

  CK_MECHANISM_TYPE derive_mech() const {
    switch (encrypt_mech()) {
      case CKM_DES3_ECB:
        return CKM_DES3_ECB_ENCRYPT_DATA;
      case CKM_DES3_CBC:
        return CKM_DES3_CBC_ENCRYPT_DATA;
      case CKM_AES_ECB:
        return CKM_AES_ECB_ENCRYPT_DATA;
      case CKM_AES_CBC:
        return CKM_AES_CBC_ENCRYPT_DATA;
      case CKM_CAMELLIA_ECB:
        return CKM_CAMELLIA_ECB_ENCRYPT_DATA;
      case CKM_CAMELLIA_CBC:
        return CKM_CAMELLIA_CBC_ENCRYPT_DATA;
      case CKM_SEED_ECB:
        return CKM_SEED_ECB_ENCRYPT_DATA;
      case CKM_SEED_CBC:
        return CKM_SEED_CBC_ENCRYPT_DATA;
      default:
        ADD_FAILURE() << "Unknown mechanism";
        break;
    }
    return CKM_INVALID_MECHANISM;
  }

  SECItem* derive_param() const {
    static CK_AES_CBC_ENCRYPT_DATA_PARAMS aes_data;
    static CK_DES_CBC_ENCRYPT_DATA_PARAMS des_data;
    static CK_KEY_DERIVATION_STRING_DATA string_data;
    static SECItem param = {siBuffer, NULL, 0};

    switch (encrypt_mech()) {
      case CKM_DES3_ECB:
      case CKM_AES_ECB:
      case CKM_CAMELLIA_ECB:
      case CKM_SEED_ECB:
        string_data.pData = toUcharPtr(kInput);
        string_data.ulLen = keysize();
        param.data = reinterpret_cast<uint8_t*>(&string_data);
        param.len = sizeof(string_data);
        break;

      case CKM_DES3_CBC:
        des_data.pData = toUcharPtr(kInput);
        des_data.length = keysize();
        PORT_Memcpy(des_data.iv, kIv, 8);
        param.data = reinterpret_cast<uint8_t*>(&des_data);
        param.len = sizeof(des_data);
        break;

      case CKM_AES_CBC:
      case CKM_CAMELLIA_CBC:
      case CKM_SEED_CBC:
        aes_data.pData = toUcharPtr(kInput);
        aes_data.length = keysize();
        PORT_Memcpy(aes_data.iv, kIv, keysize());
        param.data = reinterpret_cast<uint8_t*>(&aes_data);
        param.len = sizeof(aes_data);
        break;

      default:
        ADD_FAILURE() << "Unknown mechanism";
        break;
    }
    return &param;
  }

  SECItem* encrypt_param() const {
    static SECItem param = {siBuffer, NULL, 0};

    switch (encrypt_mech()) {
      case CKM_DES3_ECB:
      case CKM_AES_ECB:
      case CKM_CAMELLIA_ECB:
      case CKM_SEED_ECB:
        // No parameter needed here.
        break;

      case CKM_DES3_CBC:
      case CKM_AES_CBC:
      case CKM_CAMELLIA_CBC:
      case CKM_SEED_CBC:
        param.data = toUcharPtr(kIv);
        param.len = keysize();
        break;

      default:
        ADD_FAILURE() << "Unknown mechanism";
        break;
    }
    return &param;
  }

  virtual void SetUp() {
    slot_.reset(PK11_GetBestSlot(derive_mech(), NULL));
    ASSERT_TRUE(slot_);

    key_.reset(PK11_TokenKeyGenWithFlags(slot_.get(), encrypt_mech(), NULL,
                                         keysize(), NULL,
                                         CKF_ENCRYPT | CKF_DERIVE, 0, NULL));
    ASSERT_TRUE(key_);
  }

  void GetKeyData(ScopedPK11SymKey& key, uint8_t* buf, size_t max_len) const {
    ASSERT_EQ(SECSuccess, PK11_ExtractKeyValue(key.get()));
    SECItem* data = PK11_GetKeyData(key.get());
    ASSERT_TRUE(data);
    ASSERT_EQ(max_len, static_cast<size_t>(data->len));
    PORT_Memcpy(buf, data->data, data->len);
  }

  // Remove checksum if the key is a 3DES key.
  void RemoveChecksum(uint8_t* key_data) const {
    if (encrypt_mech() != CKM_DES3_CBC && encrypt_mech() != CKM_DES3_ECB) {
      return;
    }
    for (size_t i = 0; i < keysize(); ++i) {
      key_data[i] &= 0xfe;
    }
  }

  ScopedPK11SlotInfo slot_;
  ScopedPK11SymKey key_;
};

TEST_P(EncryptDeriveTest, Test) { TestEncryptDerive(); }

static const CK_MECHANISM_TYPE kEncryptDeriveMechanisms[] = {
    CKM_DES3_ECB,     CKM_DES3_CBC,     CKM_AES_ECB,  CKM_AES_ECB, CKM_AES_CBC,
    CKM_CAMELLIA_ECB, CKM_CAMELLIA_CBC, CKM_SEED_ECB, CKM_SEED_CBC};

INSTANTIATE_TEST_CASE_P(EncryptDeriveTests, EncryptDeriveTest,
                        ::testing::ValuesIn(kEncryptDeriveMechanisms));

// This class handles the case where 3DES takes a 192-bit key
// where all 24 octets will be used.
class EncryptDerive3Test : public EncryptDeriveTest {
 protected:
  unsigned int keysize() const { return 24; }
};

TEST_P(EncryptDerive3Test, Test) { TestEncryptDerive(); }

static const CK_MECHANISM_TYPE kDES3EncryptDeriveMechanisms[] = {CKM_DES3_ECB,
                                                                 CKM_DES3_CBC};

INSTANTIATE_TEST_CASE_P(Encrypt3DeriveTests, EncryptDerive3Test,
                        ::testing::ValuesIn(kDES3EncryptDeriveMechanisms));

}  // namespace nss_test
