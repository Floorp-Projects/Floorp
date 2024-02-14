/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "nss_scoped_ptrs.h"

#include "pk11_keygen.h"
#include "pk11pub.h"

#include "blapi.h"
#include "secport.h"

namespace nss_test {

class Pkcs11KEMTest : public ::testing::Test {
 protected:
  PK11SymKey *Encapsulate(const ScopedSECKEYPublicKey &pub,
                          CK_MECHANISM_TYPE target, PK11AttrFlags attrFlags,
                          CK_FLAGS opFlags, ScopedSECItem *ciphertext) {
    PK11SymKey *sharedSecretRawPtr;
    SECItem *ciphertextRawPtr;

    EXPECT_EQ(SECSuccess,
              PK11_Encapsulate(pub.get(), target, attrFlags, opFlags,
                               &sharedSecretRawPtr, &ciphertextRawPtr));

    ciphertext->reset(ciphertextRawPtr);

    return sharedSecretRawPtr;
  }

  PK11SymKey *Decapsulate(const ScopedSECKEYPrivateKey &priv,
                          const ScopedSECItem &ciphertext,
                          CK_MECHANISM_TYPE target, PK11AttrFlags attrFlags,
                          CK_FLAGS opFlags) {
    PK11SymKey *sharedSecretRawPtr;

    EXPECT_EQ(SECSuccess,
              PK11_Decapsulate(priv.get(), ciphertext.get(), target, attrFlags,
                               opFlags, &sharedSecretRawPtr));

    return sharedSecretRawPtr;
  }

  SECItem *getRawKeyData(const ScopedPK11SymKey &key) {
    SECStatus rv = PK11_ExtractKeyValue(key.get());
    EXPECT_EQ(SECSuccess, rv);

    SECItem *keyData = PK11_GetKeyData(key.get());
    EXPECT_NE(nullptr, keyData);
    EXPECT_NE(nullptr, keyData->data);

    return keyData;
  }

  void checkSymKeyAttributeValue(const ScopedPK11SymKey &key,
                                 CK_ATTRIBUTE_TYPE attr,
                                 uint8_t *expectedValue) {
    SECItem attrValue;

    EXPECT_EQ(SECSuccess, PK11_ReadRawAttribute(PK11_TypeSymKey, key.get(),
                                                attr, &attrValue));
    EXPECT_EQ(0, memcmp(expectedValue, attrValue.data, attrValue.len));

    SECITEM_FreeItem(&attrValue, PR_FALSE);
  }
};

TEST_F(Pkcs11KEMTest, KemConsistencyTest) {
  Pkcs11KeyPairGenerator generator(CKM_NSS_KYBER_KEY_PAIR_GEN);
  ScopedSECKEYPrivateKey priv;
  ScopedSECKEYPublicKey pub;
  generator.GenerateKey(&priv, &pub, false);

  ASSERT_EQ((unsigned int)KYBER768_PUBLIC_KEY_BYTES,
            (unsigned int)pub->u.kyber.publicValue.len);

  // Copy the public key to simulate receiving the key as an octet string
  ScopedSECKEYPublicKey pubCopy(SECKEY_CopyPublicKey(pub.get()));
  ASSERT_NE(nullptr, pubCopy);

  ScopedPK11SlotInfo slot(PK11_GetBestSlot(CKM_NSS_KYBER, nullptr));
  ASSERT_NE(nullptr, slot);

  ASSERT_NE((unsigned int)CK_INVALID_HANDLE,
            PK11_ImportPublicKey(slot.get(), pubCopy.get(), PR_FALSE));

  ScopedSECItem ciphertext;
  ScopedPK11SymKey sharedSecret(Encapsulate(
      pubCopy, CKM_SALSA20_POLY1305, PK11_ATTR_PRIVATE | PK11_ATTR_UNMODIFIABLE,
      CKF_ENCRYPT, &ciphertext));

  ASSERT_EQ((unsigned int)KYBER768_CIPHERTEXT_BYTES,
            (unsigned int)ciphertext->len);

  ASSERT_EQ(CKM_SALSA20_POLY1305, PK11_GetMechanism(sharedSecret.get()));

  CK_BBOOL ckTrue = CK_TRUE;
  CK_BBOOL ckFalse = CK_FALSE;
  checkSymKeyAttributeValue(sharedSecret, CKA_PRIVATE, &ckTrue);
  checkSymKeyAttributeValue(sharedSecret, CKA_MODIFIABLE, &ckFalse);
  checkSymKeyAttributeValue(sharedSecret, CKA_ENCRYPT, &ckTrue);

  ScopedPK11SymKey sharedSecret2(
      Decapsulate(priv, ciphertext, CKM_SALSA20_POLY1305,
                  PK11_ATTR_PRIVATE | PK11_ATTR_UNMODIFIABLE, CKF_ENCRYPT));

  ASSERT_EQ(CKM_SALSA20_POLY1305, PK11_GetMechanism(sharedSecret2.get()));

  checkSymKeyAttributeValue(sharedSecret2, CKA_PRIVATE, &ckTrue);
  checkSymKeyAttributeValue(sharedSecret2, CKA_MODIFIABLE, &ckFalse);
  checkSymKeyAttributeValue(sharedSecret2, CKA_ENCRYPT, &ckTrue);

  SECItem *item1 = getRawKeyData(sharedSecret);
  SECItem *item2 = getRawKeyData(sharedSecret2);
  NSS_DECLASSIFY(item1->data, item1->len);
  NSS_DECLASSIFY(item2->data, item2->len);
  EXPECT_EQ(0, SECITEM_CompareItem(item1, item2));
}

}  // namespace nss_test
