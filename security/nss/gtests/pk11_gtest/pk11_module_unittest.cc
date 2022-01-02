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
  EXPECT_NE(nullptr, slots);

  PK11SlotListElement* element = PK11_GetFirstSafe(slots.get());
  EXPECT_NE(nullptr, element);

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
  EXPECT_NE(nullptr, slot1);
  EXPECT_FALSE(PK11_IsFriendly(slot1.get()));

  ScopedPK11SlotInfo slot2(
      PK11_FindSlotByName(kPublicCertificatesToken.c_str()));
  EXPECT_NE(nullptr, slot2);
  EXPECT_TRUE(PK11_IsFriendly(slot2.get()));
}

}  // namespace nss_test
