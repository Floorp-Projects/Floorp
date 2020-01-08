/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <memory>
#include "nss.h"
#include "pk11pub.h"
#include "secerr.h"
#include "sechash.h"
#include "stdio.h"

#include "blapi.h"

#include "gtest/gtest.h"
#include "nss_scoped_ptrs.h"
#include "util.h"

namespace nss_test {
class Pkcs11KbkdfTest : public ::testing::Test {
 protected:
  ScopedPK11SymKey ImportKey(CK_MECHANISM_TYPE mech, SECItem *key_item) {
    ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
    if (!slot) {
      ADD_FAILURE() << "Can't get slot";
      return nullptr;
    }

    ScopedPK11SymKey result(PK11_ImportSymKey(
        slot.get(), mech, PK11_OriginUnwrap, CKA_SIGN, key_item, nullptr));

    return result;
  }

  void RunKDF(CK_MECHANISM_TYPE kdfMech, CK_SP800_108_KDF_PARAMS_PTR kdfParams,
              CK_BYTE_PTR inputKey, unsigned int inputKeyLen,
              CK_BYTE_PTR expectedKey, unsigned int expectedKeyLen,
              CK_BYTE_PTR expectedAdditional,
              unsigned int expectedAdditionalLen) {
    SECItem keyItem = {siBuffer, inputKey, inputKeyLen};
    ScopedPK11SymKey p11Key = ImportKey(kdfParams->prfType, &keyItem);

    ASSERT_NE(kdfParams, nullptr);
    SECItem paramsItem = {siBuffer, (unsigned char *)kdfParams,
                          sizeof(*kdfParams)};

    ScopedPK11SymKey result(PK11_Derive(p11Key.get(), kdfMech, &paramsItem,
                                        CKM_SHA512_HMAC, CKA_SIGN,
                                        expectedKeyLen));
    ASSERT_NE(result, nullptr);

    ASSERT_EQ(PK11_ExtractKeyValue(result.get()), SECSuccess);

    /* We don't need to free this -- it is just a reference... */
    SECItem *actualItem = PK11_GetKeyData(result.get());
    ASSERT_NE(actualItem, nullptr);

    SECItem expectedItem = {siBuffer, expectedKey, expectedKeyLen};
    ASSERT_EQ(SECITEM_CompareItem(actualItem, &expectedItem), 0);

    /* Extract the additional key. */
    if (expectedAdditional == NULL || kdfParams->ulAdditionalDerivedKeys != 1) {
      return;
    }

    ScopedPK11SlotInfo slot(PK11_GetSlotFromKey(result.get()));

    CK_OBJECT_HANDLE_PTR keyHandle = kdfParams->pAdditionalDerivedKeys[0].phKey;
    ScopedPK11SymKey additionalKey(
        PK11_SymKeyFromHandle(slot.get(), result.get(), PK11_OriginDerive,
                              CKM_SHA512_HMAC, *keyHandle, PR_FALSE, NULL));

    ASSERT_EQ(PK11_ExtractKeyValue(additionalKey.get()), SECSuccess);

    /* We don't need to free this -- it is just a reference... */
    actualItem = PK11_GetKeyData(additionalKey.get());
    ASSERT_NE(actualItem, nullptr);

    expectedItem = {siBuffer, expectedAdditional, expectedAdditionalLen};
    ASSERT_EQ(SECITEM_CompareItem(actualItem, &expectedItem), 0);
  }
};

TEST_F(Pkcs11KbkdfTest, TestAdditionalKey) {
  /* Test number 11 of NIST CAVP vectors for Counter mode KDF, with counter
   * after a fixed input (AES/128 CMAC). Resulting key (of size 256 bits)
   * split into two 128-bit chunks since that aligns with a PRF invocation
   * boundary. */
  CK_BYTE inputKey[] = {0x23, 0xeb, 0x06, 0x5b, 0xe1, 0x27, 0xa8, 0x81,
                        0xe3, 0x5a, 0x65, 0x14, 0xd4, 0x35, 0x67, 0x9f};
  CK_BYTE expectedKey[] = {0xea, 0x4e, 0xbb, 0xb4, 0xef, 0xff, 0x4b, 0x01,
                           0x68, 0x40, 0x12, 0xed, 0x8f, 0xf9, 0xc6, 0x4e};
  CK_BYTE expectedAdditional[] = {0x70, 0xae, 0x38, 0x19, 0x7c, 0x36,
                                  0x44, 0x5a, 0x6c, 0x80, 0x4a, 0x0e,
                                  0x44, 0x81, 0x9a, 0xc3};

  CK_SP800_108_COUNTER_FORMAT iterator = {CK_FALSE, 8};
  CK_BYTE fixedData[] = {
      0xe6, 0x79, 0x86, 0x1a, 0x61, 0x34, 0x65, 0xa6, 0x73, 0x85, 0x37, 0x26,
      0x71, 0xb1, 0x07, 0xe6, 0xb8, 0x95, 0xa2, 0xf6, 0x40, 0x43, 0xc9, 0x34,
      0xff, 0x42, 0x56, 0xa7, 0xe6, 0x3c, 0xfb, 0x8b, 0xfa, 0xcc, 0x21, 0x24,
      0x25, 0x1c, 0x90, 0xfa, 0x67, 0x0d, 0x45, 0x74, 0x5c, 0x1c, 0x35, 0xda,
      0x9b, 0x6e, 0x05, 0xaf, 0x77, 0xea, 0x9c, 0x4a, 0xd4, 0x86, 0xfd, 0x1a};

  CK_PRF_DATA_PARAM dataParams[] = {
      {CK_SP800_108_BYTE_ARRAY, fixedData,
       sizeof(fixedData) / sizeof(*fixedData)},
      {CK_SP800_108_ITERATION_VARIABLE, &iterator, sizeof(iterator)}};

  CK_KEY_TYPE ckGeneric = CKK_GENERIC_SECRET;
  CK_OBJECT_CLASS ckClass = CKO_SECRET_KEY;
  CK_ULONG derivedLength = 16;

  CK_ATTRIBUTE derivedTemplate[] = {
      {CKA_CLASS, &ckClass, sizeof(ckClass)},
      {CKA_KEY_TYPE, &ckGeneric, sizeof(ckGeneric)},
      {CKA_VALUE_LEN, &derivedLength, sizeof(derivedLength)}};

  CK_OBJECT_HANDLE keyHandle;
  CK_DERIVED_KEY derivedKey = {
      derivedTemplate, sizeof(derivedTemplate) / sizeof(*derivedTemplate),
      &keyHandle};

  CK_SP800_108_KDF_PARAMS kdfParams = {CKM_AES_CMAC,
                                       sizeof(dataParams) / sizeof(*dataParams),
                                       dataParams, 1, &derivedKey};

  RunKDF(CKM_SP800_108_COUNTER_KDF, &kdfParams, inputKey,
         sizeof(inputKey) / sizeof(*inputKey), expectedKey,
         sizeof(expectedKey) / sizeof(*expectedKey), expectedAdditional,
         sizeof(expectedAdditional) / sizeof(*expectedAdditional));
}

// Close the namespace
}
