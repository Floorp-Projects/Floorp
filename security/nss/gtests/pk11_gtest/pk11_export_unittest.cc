/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <memory>
#include "nss.h"
#include "pk11pub.h"

#include "gtest/gtest.h"
#include "nss_scoped_ptrs.h"

namespace nss_test {

class Pkcs11ExportTest : public ::testing::Test {
 public:
  void Derive(bool is_export) {
    ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
    EXPECT_TRUE(slot.get());

    uint8_t keyData[48] = {0};
    SECItem keyItem = {siBuffer, (unsigned char*)keyData, sizeof(keyData)};

    CK_MECHANISM_TYPE mechanism = CKM_NSS_TLS_KEY_AND_MAC_DERIVE_SHA256;
    ScopedPK11SymKey baseKey(PK11_ImportSymKey(
        slot.get(), mechanism, PK11_OriginUnwrap, CKA_WRAP, &keyItem, nullptr));
    EXPECT_TRUE(baseKey.get());

    CK_SSL3_KEY_MAT_OUT kmo;
    kmo.hClientMacSecret = CK_INVALID_HANDLE;
    kmo.hServerMacSecret = CK_INVALID_HANDLE;
    kmo.hClientKey = CK_INVALID_HANDLE;
    kmo.hServerKey = CK_INVALID_HANDLE;

    CK_BYTE iv[8];
    kmo.pIVClient = iv;
    kmo.pIVServer = iv;

    CK_SSL3_KEY_MAT_PARAMS kmp;
    kmp.ulMacSizeInBits = 256;
    kmp.ulKeySizeInBits = 128;
    kmp.ulIVSizeInBits = 64;
    kmp.pReturnedKeyMaterial = &kmo;
    kmp.bIsExport = is_export;

    unsigned char random[32] = {0};
    kmp.RandomInfo.pClientRandom = random;
    kmp.RandomInfo.ulClientRandomLen = sizeof(random);
    kmp.RandomInfo.pServerRandom = random;
    kmp.RandomInfo.ulServerRandomLen = sizeof(random);

    SECItem params = {siBuffer, (unsigned char*)&kmp, sizeof(kmp)};
    ScopedPK11SymKey symKey(PK11_Derive(baseKey.get(), mechanism, &params,
                                        CKM_SHA512_HMAC, CKA_SIGN, 16));

    // Deriving must fail when is_export=true.
    EXPECT_EQ(!symKey.get(), is_export);
  }
};

TEST_F(Pkcs11ExportTest, DeriveNonExport) { Derive(false); }

TEST_F(Pkcs11ExportTest, DeriveExport) { Derive(true); }

}  // namespace nss_test
