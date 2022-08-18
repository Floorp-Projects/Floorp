/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <memory>
#include "nss.h"
#include "pk11pub.h"
#include "prerror.h"
#include "prsystem.h"
#include "secoid.h"

#include "nss_scoped_ptrs.h"
#include "gtest/gtest.h"
#include "databuffer.h"

namespace nss_test {

class Pkcs11ModuleTest : public ::testing::Test {
 public:
  Pkcs11ModuleTest() {}

  void SetUp() override {
    ASSERT_EQ(SECSuccess, SECMOD_AddNewModule("Pkcs11ModuleTest", DLL_PREFIX
                                              "pkcs11testmodule." DLL_SUFFIX,
                                              0, 0))
        << PORT_ErrorToName(PORT_GetError());
  }

  void TearDown() override {
    int type;
    ASSERT_EQ(SECSuccess, SECMOD_DeleteModule("Pkcs11ModuleTest", &type));
    ASSERT_EQ(SECMOD_EXTERNAL, type);
  }
};

TEST_F(Pkcs11ModuleTest, LoadUnload) {
  ScopedSECMODModule module(SECMOD_FindModule("Pkcs11ModuleTest"));
  EXPECT_NE(nullptr, module);
}

TEST_F(Pkcs11ModuleTest, ListSlots) {
  ScopedPK11SlotList slots(
      PK11_GetAllTokens(CKM_INVALID_MECHANISM, PR_FALSE, PR_FALSE, nullptr));
  ASSERT_NE(nullptr, slots);

  PK11SlotListElement *element = PK11_GetFirstSafe(slots.get());
  ASSERT_NE(nullptr, element);

  // These tokens are always present.
  const std::vector<std::string> kSlotsWithToken = {
      "NSS Internal Cryptographic Services",
      "NSS User Private Key and Certificate Services",
      "Test PKCS11 Public Certs Slot", "Test PKCS11 Slot 二"};
  std::vector<std::string> foundSlots;

  do {
    std::string name = PK11_GetSlotName(element->slot);
    foundSlots.push_back(name);
    std::cerr << "loaded slot: " << name << std::endl;
  } while ((element = PK11_GetNextSafe(slots.get(), element, PR_FALSE)) !=
           nullptr);

  std::sort(foundSlots.begin(), foundSlots.end());
  EXPECT_TRUE(std::equal(kSlotsWithToken.begin(), kSlotsWithToken.end(),
                         foundSlots.begin()));
}

TEST_F(Pkcs11ModuleTest, PublicCertificatesToken) {
  const std::string kRegularToken = "Test PKCS11 Tokeñ 2 Label";
  const std::string kPublicCertificatesToken = "Test PKCS11 Public Certs Token";

  ScopedPK11SlotInfo slot1(PK11_FindSlotByName(kRegularToken.c_str()));
  ASSERT_NE(nullptr, slot1);
  EXPECT_FALSE(PK11_IsFriendly(slot1.get()));

  ScopedPK11SlotInfo slot2(
      PK11_FindSlotByName(kPublicCertificatesToken.c_str()));
  ASSERT_NE(nullptr, slot2);
  EXPECT_TRUE(PK11_IsFriendly(slot2.get()));
}

TEST_F(Pkcs11ModuleTest, PublicCertificatesTokenLookup) {
  const std::string kCertUrl =
      "pkcs11:id=%10%11%12%13%14%15%16%17%18%19%1a%1b%1c%1d%1e%1f";

  ScopedCERTCertList certsByUrl(
      PK11_FindCertsFromURI(kCertUrl.c_str(), nullptr));
  EXPECT_NE(nullptr, certsByUrl.get());

  size_t count = 0;
  CERTCertificate *certByUrl = nullptr;
  for (CERTCertListNode *node = CERT_LIST_HEAD(certsByUrl);
       !CERT_LIST_END(node, certsByUrl); node = CERT_LIST_NEXT(node)) {
    if (count == 0) {
      certByUrl = node->cert;
    }
    count++;
  }
  EXPECT_EQ(1UL, count);
  EXPECT_NE(nullptr, certByUrl);

  EXPECT_EQ(
      0, strcmp(certByUrl->nickname, "Test PKCS11 Public Certs Token:cert2"));
}

TEST_F(Pkcs11ModuleTest, PublicCertificatesTokenLookupNoMatch) {
  const std::string kCertUrl =
      "pkcs11:id=%00%01%02%03%04%05%06%07%08%09%0a%0b%0c%0d%0e%0e";

  ScopedCERTCertList certsByUrl(
      PK11_FindCertsFromURI(kCertUrl.c_str(), nullptr));
  EXPECT_EQ(nullptr, certsByUrl.get());
}

#if defined(_WIN32)
#include <windows.h>

class Pkcs11NonAsciiTest : public ::testing::Test {
  WCHAR nonAsciiModuleName[MAX_PATH];

 public:
  Pkcs11NonAsciiTest() {}

  void SetUp() override {
    WCHAR originalModuleName[MAX_PATH];
    LPWSTR filePart;
    DWORD count = SearchPathW(NULL, L"pkcs11testmodule.dll", NULL, MAX_PATH,
                              nonAsciiModuleName, &filePart);
    ASSERT_TRUE(count);
    wcscpy(originalModuleName, nonAsciiModuleName);
    wcscpy(filePart, L"pkcs11testmodule\u2665.dll");
    BOOL result = CopyFileW(originalModuleName, nonAsciiModuleName, TRUE);
    ASSERT_TRUE(result);
    ASSERT_EQ(SECSuccess,
              SECMOD_AddNewModule("Pkcs11NonAsciiTest", DLL_PREFIX
                                  "pkcs11testmodule\xE2\x99\xA5." DLL_SUFFIX,
                                  0, 0))
        << PORT_ErrorToName(PORT_GetError());
  }

  void TearDown() override {
    int type;
    ASSERT_EQ(SECSuccess, SECMOD_DeleteModule("Pkcs11NonAsciiTest", &type));
    ASSERT_EQ(SECMOD_EXTERNAL, type);
    BOOL result = DeleteFileW(nonAsciiModuleName);
    ASSERT_TRUE(result);
  }
};

TEST_F(Pkcs11NonAsciiTest, LoadUnload) {
  ScopedSECMODModule module(SECMOD_FindModule("Pkcs11NonAsciiTest"));
  EXPECT_NE(nullptr, module);
}
#endif  // defined(_WIN32)

}  // namespace nss_test
