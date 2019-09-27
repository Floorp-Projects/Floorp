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
#include "pk11_keygen.h"

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
  Pk11KeyImportTestBase() = default;
  virtual ~Pk11KeyImportTestBase() = default;

  void SetUp() override {
    slot_.reset(PK11_GetInternalKeySlot());
    ASSERT_TRUE(slot_);

    static const uint8_t pw[] = "pw";
    SECItem pwItem = {siBuffer, toUcharPtr(pw), sizeof(pw)};
    password_.reset(SECITEM_DupItem(&pwItem));
  }

  void Test(const Pkcs11KeyPairGenerator& generator) {
    // Generate a key and export it.
    KeyType key_type = nullKey;
    ScopedSECKEYEncryptedPrivateKeyInfo key_info;
    ScopedSECItem public_value;
    GenerateAndExport(generator, &key_type, &key_info, &public_value);

    // Note: NSS is currently unable export wrapped DH keys, so this doesn't
    // test those beyond generate and verify.
    if (key_type == dhKey) {
      return;
    }
    ASSERT_NE(nullptr, public_value);
    ASSERT_NE(nullptr, key_info);

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

 private:
  SECItem GetPublicComponent(ScopedSECKEYPublicKey& pub_key) {
    SECItem null = {siBuffer, NULL, 0};
    switch (SECKEY_GetPublicKeyType(pub_key.get())) {
      case rsaKey:
      case rsaPssKey:
      case rsaOaepKey:
        return pub_key->u.rsa.modulus;
      case keaKey:
        return pub_key->u.kea.publicValue;
      case dsaKey:
        return pub_key->u.dsa.publicValue;
      case dhKey:
        return pub_key->u.dh.publicValue;
      case ecKey:
        return pub_key->u.ec.publicValue;
      case fortezzaKey: /* depricated */
      case nullKey:
        /* didn't use default here so we can catch new key types at compile time
         */
        break;
    }
    return null;
  }
  void CheckForPublicKey(const ScopedSECKEYPrivateKey& priv_key,
                         const SECItem* expected_public) {
    // Verify the public key exists.
    StackSECItem priv_id;
    KeyType type = SECKEY_GetPrivateKeyType(priv_key.get());
    SECStatus rv = PK11_ReadRawAttribute(PK11_TypePrivKey, priv_key.get(),
                                         CKA_ID, &priv_id);
    ASSERT_EQ(SECSuccess, rv) << "Couldn't read CKA_ID from private key: "
                              << PORT_ErrorToName(PORT_GetError());

    CK_ATTRIBUTE_TYPE value_type = CKA_VALUE;
    switch (type) {
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

    // Scan public key objects until we find one with the same CKA_ID as
    // priv_key
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

      StackSECItem raw_value;
      SECItem decoded_value;
      rv = PK11_ReadRawAttribute(PK11_TypeGeneric, obj, value_type, &raw_value);
      ASSERT_EQ(SECSuccess, rv);
      SECItem value = raw_value;

      // Decode the EC_POINT and check the output against expected.
      // CKA_EC_POINT isn't stable, see Bug 1520649.
      ScopedPLArenaPool arena(PORT_NewArena(DER_DEFAULT_CHUNKSIZE));
      ASSERT_TRUE(arena);
      if (value_type == CKA_EC_POINT) {
        // If this fails due to the noted inconsistency, we may need to
        // check the whole raw_value, or remove a leading UNCOMPRESSED_POINT tag
        rv = SEC_QuickDERDecodeItem(arena.get(), &decoded_value,
                                    SEC_ASN1_GET(SEC_OctetStringTemplate),
                                    &raw_value);
        ASSERT_EQ(SECSuccess, rv);
        value = decoded_value;
      }
      ASSERT_TRUE(SECITEM_ItemsAreEqual(expected_public, &value))
          << "expected: "
          << DataBuffer(expected_public->data, expected_public->len)
          << std::endl
          << "actual: " << DataBuffer(value.data, value.len) << std::endl;

      // Finally, convert the private to public and ensure it matches.
      ScopedSECKEYPublicKey pub_key(SECKEY_ConvertToPublicKey(priv_key.get()));
      ASSERT_TRUE(pub_key);
      SECItem converted_public = GetPublicComponent(pub_key);
      ASSERT_TRUE(converted_public.len != 0);

      ASSERT_TRUE(SECITEM_ItemsAreEqual(expected_public, &converted_public))
          << "expected: "
          << DataBuffer(expected_public->data, expected_public->len)
          << std::endl
          << "actual: "
          << DataBuffer(converted_public.data, converted_public.len)
          << std::endl;
    }
  }

  void GenerateAndExport(const Pkcs11KeyPairGenerator& generator,
                         KeyType* key_type,
                         ScopedSECKEYEncryptedPrivateKeyInfo* key_info,
                         ScopedSECItem* public_value) {
    ScopedSECKEYPrivateKey priv_key;
    ScopedSECKEYPublicKey pub_key;
    generator.GenerateKey(&priv_key, &pub_key);
    ASSERT_TRUE(priv_key);

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
    // Note: NSS is currently unable export wrapped DH keys, so this doesn't
    // test those beyond generate and verify.
    if (t == dhKey) {
      return;
    }
    public_value->reset(SECITEM_DupItem(pub_val));

    // Wrap and export the key.
    ScopedSECKEYEncryptedPrivateKeyInfo epki(PK11_ExportEncryptedPrivKeyInfo(
        slot_.get(), SEC_OID_AES_256_CBC, password_.get(), priv_key.get(), 1,
        nullptr));
    ASSERT_NE(nullptr, epki) << "PK11_ExportEncryptedPrivKeyInfo failed: "
                             << PORT_ErrorToName(PORT_GetError());

    key_info->swap(epki);
  }

  ScopedPK11SlotInfo slot_;
  ScopedSECItem password_;
};

class Pk11KeyImportTest
    : public Pk11KeyImportTestBase,
      public ::testing::WithParamInterface<CK_MECHANISM_TYPE> {
 public:
  Pk11KeyImportTest() = default;
  virtual ~Pk11KeyImportTest() = default;
};

TEST_P(Pk11KeyImportTest, GenerateExportImport) {
  Test(Pkcs11KeyPairGenerator(GetParam()));
}

INSTANTIATE_TEST_CASE_P(Pk11KeyImportTest, Pk11KeyImportTest,
                        ::testing::Values(CKM_RSA_PKCS_KEY_PAIR_GEN,
                                          CKM_DSA_KEY_PAIR_GEN,
                                          CKM_DH_PKCS_KEY_PAIR_GEN));

class Pk11KeyImportTestEC : public Pk11KeyImportTestBase,
                            public ::testing::WithParamInterface<SECOidTag> {
 public:
  Pk11KeyImportTestEC() = default;
  virtual ~Pk11KeyImportTestEC() = default;
};

TEST_P(Pk11KeyImportTestEC, GenerateExportImport) {
  Test(Pkcs11KeyPairGenerator(CKM_EC_KEY_PAIR_GEN, GetParam()));
}

INSTANTIATE_TEST_CASE_P(Pk11KeyImportTestEC, Pk11KeyImportTestEC,
                        ::testing::Values(SEC_OID_SECG_EC_SECP256R1,
                                          SEC_OID_SECG_EC_SECP384R1,
                                          SEC_OID_SECG_EC_SECP521R1,
                                          SEC_OID_CURVE25519));

}  // namespace nss_test
