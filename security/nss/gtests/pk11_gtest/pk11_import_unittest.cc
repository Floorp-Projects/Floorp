/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <memory>
#include "nss.h"
#include "pk11pub.h"
#include "pk11pqg.h"
#include "prerror.h"
#include "secoid.h"

#include "cpputil.h"
#include "nss_scoped_ptrs.h"
#include "gtest/gtest.h"
#include "databuffer.h"

namespace nss_test {

// This deleter deletes a set of objects, unlike the deleter on
// ScopedPK11GenericObject, which only deletes one.
struct PK11GenericObjectsDeleter {
  void operator()(PK11GenericObject* objs) {
    if (objs) {
      PK11_DestroyGenericObjects(objs);
    }
  }
};

class Pk11KeyImportTestBase : public ::testing::Test {
 public:
  Pk11KeyImportTestBase(CK_MECHANISM_TYPE mech) : mech_(mech) {}
  virtual ~Pk11KeyImportTestBase() = default;

  void SetUp() override {
    slot_.reset(PK11_GetInternalKeySlot());
    ASSERT_TRUE(slot_);

    static const uint8_t pw[] = "pw";
    SECItem pwItem = {siBuffer, toUcharPtr(pw), sizeof(pw)};
    password_.reset(SECITEM_DupItem(&pwItem));
  }

  void Test() {
    // Generate a key and export it.
    KeyType key_type;
    ScopedSECKEYEncryptedPrivateKeyInfo key_info;
    ScopedSECItem public_value;
    GenerateAndExport(&key_type, &key_info, &public_value);
    ASSERT_NE(nullptr, key_info);
    ASSERT_NE(nullptr, public_value);

    // Now import the encrypted key.
    static const uint8_t nick[] = "nick";
    SECItem nickname = {siBuffer, toUcharPtr(nick), sizeof(nick)};
    SECKEYPrivateKey* priv_tmp;
    SECStatus rv = PK11_ImportEncryptedPrivateKeyInfoAndReturnKey(
        slot_.get(), key_info.get(), password_.get(), &nickname,
        public_value.get(), PR_TRUE, PR_TRUE, key_type, 0, &priv_tmp, NULL);
    ASSERT_EQ(SECSuccess, rv) << "PK11_ImportEncryptedPrivateKeyInfo failed "
                              << PORT_ErrorToName(PORT_GetError());
    ScopedSECKEYPrivateKey priv_key(priv_tmp);
    ASSERT_NE(nullptr, priv_key);

    CheckForPublicKey(priv_key, public_value.get());
  }

 protected:
  class ParamHolder {
   public:
    virtual ~ParamHolder() = default;
    virtual void* get() = 0;
  };

  virtual std::unique_ptr<ParamHolder> MakeParams() = 0;

  CK_MECHANISM_TYPE mech_;

 private:
  void CheckForPublicKey(const ScopedSECKEYPrivateKey& priv_key,
                         const SECItem* expected_public) {
    // Verify the public key exists.
    StackSECItem priv_id;
    SECStatus rv = PK11_ReadRawAttribute(PK11_TypePrivKey, priv_key.get(),
                                         CKA_ID, &priv_id);
    ASSERT_EQ(SECSuccess, rv) << "Couldn't read CKA_ID from private key: "
                              << PORT_ErrorToName(PORT_GetError());

    CK_ATTRIBUTE_TYPE value_type = CKA_VALUE;
    switch (SECKEY_GetPrivateKeyType(priv_key.get())) {
      case rsaKey:
        value_type = CKA_MODULUS;
        break;

      case dhKey:
      case dsaKey:
        value_type = CKA_VALUE;
        break;

      case ecKey:
        value_type = CKA_EC_POINT;
        break;

      default:
        FAIL() << "unknown key type";
    }

    std::unique_ptr<PK11GenericObject, PK11GenericObjectsDeleter> objs(
        PK11_FindGenericObjects(slot_.get(), CKO_PUBLIC_KEY));
    ASSERT_NE(nullptr, objs);
    for (PK11GenericObject* obj = objs.get(); obj != nullptr;
         obj = PK11_GetNextGenericObject(obj)) {
      StackSECItem pub_id;
      rv = PK11_ReadRawAttribute(PK11_TypeGeneric, obj, CKA_ID, &pub_id);
      if (rv != SECSuccess) {
        // Can't read CKA_ID from object.
        continue;
      }
      if (!SECITEM_ItemsAreEqual(&priv_id, &pub_id)) {
        // This isn't the object we're looking for.
        continue;
      }

      StackSECItem token;
      rv = PK11_ReadRawAttribute(PK11_TypeGeneric, obj, CKA_TOKEN, &token);
      ASSERT_EQ(SECSuccess, rv);
      ASSERT_EQ(1U, token.len);
      ASSERT_NE(0, token.data[0]);

      StackSECItem value;
      rv = PK11_ReadRawAttribute(PK11_TypeGeneric, obj, value_type, &value);
      ASSERT_EQ(SECSuccess, rv);

      // CKA_EC_POINT isn't stable, see Bug 1520649.
      if (value_type == CKA_EC_POINT) {
        continue;
      }

      ASSERT_TRUE(SECITEM_ItemsAreEqual(expected_public, &value))
          << "expected: "
          << DataBuffer(expected_public->data, expected_public->len)
          << std::endl
          << "actual: " << DataBuffer(value.data, value.len) << std::endl;
    }
  }

  void GenerateAndExport(KeyType* key_type,
                         ScopedSECKEYEncryptedPrivateKeyInfo* key_info,
                         ScopedSECItem* public_value) {
    auto params = MakeParams();
    ASSERT_NE(nullptr, params);

    SECKEYPublicKey* pub_tmp;
    ScopedSECKEYPrivateKey priv_key(
        PK11_GenerateKeyPair(slot_.get(), mech_, params->get(), &pub_tmp,
                             PR_FALSE, PR_TRUE, nullptr));
    ASSERT_NE(nullptr, priv_key) << "PK11_GenerateKeyPair failed: "
                                 << PORT_ErrorToName(PORT_GetError());
    ScopedSECKEYPublicKey pub_key(pub_tmp);
    ASSERT_NE(nullptr, pub_key);

    // Wrap and export the key.
    ScopedSECKEYEncryptedPrivateKeyInfo epki(PK11_ExportEncryptedPrivKeyInfo(
        slot_.get(), SEC_OID_AES_256_CBC, password_.get(), priv_key.get(), 1,
        nullptr));
    ASSERT_NE(nullptr, epki) << "PK11_ExportEncryptedPrivKeyInfo failed: "
                             << PORT_ErrorToName(PORT_GetError());

    // Save the public value, which we will need on import */
    SECItem* pub_val;
    KeyType t = SECKEY_GetPublicKeyType(pub_key.get());
    switch (t) {
      case rsaKey:
        pub_val = &pub_key->u.rsa.modulus;
        break;
      case dhKey:
        pub_val = &pub_key->u.dh.publicValue;
        break;
      case dsaKey:
        pub_val = &pub_key->u.dsa.publicValue;
        break;
      case ecKey:
        pub_val = &pub_key->u.ec.publicValue;
        break;
      default:
        FAIL() << "Unknown key type";
    }

    CheckForPublicKey(priv_key, pub_val);

    *key_type = t;
    key_info->swap(epki);
    public_value->reset(SECITEM_DupItem(pub_val));
  }

  ScopedPK11SlotInfo slot_;
  ScopedSECItem password_;
};

class Pk11KeyImportTest
    : public Pk11KeyImportTestBase,
      public ::testing::WithParamInterface<CK_MECHANISM_TYPE> {
 public:
  Pk11KeyImportTest() : Pk11KeyImportTestBase(GetParam()) {}
  virtual ~Pk11KeyImportTest() = default;

 protected:
  std::unique_ptr<ParamHolder> MakeParams() override {
    switch (mech_) {
      case CKM_RSA_PKCS_KEY_PAIR_GEN:
        return std::unique_ptr<ParamHolder>(new RsaParamHolder());

      case CKM_DSA_KEY_PAIR_GEN:
      case CKM_DH_PKCS_KEY_PAIR_GEN: {
        PQGParams* pqg_params = nullptr;
        PQGVerify* pqg_verify = nullptr;
        const unsigned int key_size = 1024;
        SECStatus rv = PK11_PQG_ParamGenV2(key_size, 0, key_size / 16,
                                           &pqg_params, &pqg_verify);
        if (rv != SECSuccess) {
          ADD_FAILURE() << "PK11_PQG_ParamGenV2 failed";
          return nullptr;
        }
        EXPECT_NE(nullptr, pqg_verify);
        EXPECT_NE(nullptr, pqg_params);
        PK11_PQG_DestroyVerify(pqg_verify);
        if (mech_ == CKM_DSA_KEY_PAIR_GEN) {
          return std::unique_ptr<ParamHolder>(new PqgParamHolder(pqg_params));
        }
        return std::unique_ptr<ParamHolder>(new DhParamHolder(pqg_params));
      }

      default:
        ADD_FAILURE() << "unknown OID " << mech_;
    }
    return nullptr;
  }

 private:
  class RsaParamHolder : public ParamHolder {
   public:
    RsaParamHolder()
        : params_({/*.keySizeInBits = */ 1024, /*.pe = */ 0x010001}) {}
    ~RsaParamHolder() = default;

    void* get() override { return &params_; }

   private:
    PK11RSAGenParams params_;
  };

  class PqgParamHolder : public ParamHolder {
   public:
    PqgParamHolder(PQGParams* params) : params_(params) {}
    ~PqgParamHolder() = default;

    void* get() override { return params_.get(); }

   private:
    ScopedPQGParams params_;
  };

  class DhParamHolder : public PqgParamHolder {
   public:
    DhParamHolder(PQGParams* params)
        : PqgParamHolder(params),
          params_({/*.arena = */ nullptr,
                   /*.prime = */ params->prime,
                   /*.base = */ params->base}) {}
    ~DhParamHolder() = default;

    void* get() override { return &params_; }

   private:
    SECKEYDHParams params_;
  };
};

TEST_P(Pk11KeyImportTest, GenerateExportImport) { Test(); }

INSTANTIATE_TEST_CASE_P(Pk11KeyImportTest, Pk11KeyImportTest,
                        ::testing::Values(CKM_RSA_PKCS_KEY_PAIR_GEN,
                                          CKM_DSA_KEY_PAIR_GEN));
// Note: NSS is currently unable export wrapped DH keys, so this doesn't test
// CKM_DH_PKCS_KEY_PAIR_GEN.

class Pk11KeyImportTestEC : public Pk11KeyImportTestBase,
                            public ::testing::WithParamInterface<SECOidTag> {
 public:
  Pk11KeyImportTestEC() : Pk11KeyImportTestBase(CKM_EC_KEY_PAIR_GEN) {}
  virtual ~Pk11KeyImportTestEC() = default;

 protected:
  std::unique_ptr<ParamHolder> MakeParams() override {
    return std::unique_ptr<ParamHolder>(new EcParamHolder(GetParam()));
  }

 private:
  class EcParamHolder : public ParamHolder {
   public:
    EcParamHolder(SECOidTag curve_oid) {
      SECOidData* curve = SECOID_FindOIDByTag(curve_oid);
      EXPECT_NE(nullptr, curve);

      size_t plen = curve->oid.len + 2;
      extra_.reset(new uint8_t[plen]);
      extra_[0] = SEC_ASN1_OBJECT_ID;
      extra_[1] = static_cast<uint8_t>(curve->oid.len);
      memcpy(&extra_[2], curve->oid.data, curve->oid.len);

      ec_params_ = {/*.type = */ siBuffer,
                    /*.data = */ extra_.get(),
                    /*.len = */ static_cast<unsigned int>(plen)};
    }
    ~EcParamHolder() = default;

    void* get() override { return &ec_params_; }

   private:
    SECKEYECParams ec_params_;
    std::unique_ptr<uint8_t[]> extra_;
  };
};

TEST_P(Pk11KeyImportTestEC, GenerateExportImport) { Test(); }

INSTANTIATE_TEST_CASE_P(Pk11KeyImportTestEC, Pk11KeyImportTestEC,
                        ::testing::Values(SEC_OID_SECG_EC_SECP256R1,
                                          SEC_OID_SECG_EC_SECP384R1,
                                          SEC_OID_SECG_EC_SECP521R1,
                                          SEC_OID_CURVE25519));

}  // namespace nss_test
