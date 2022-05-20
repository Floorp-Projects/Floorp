/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "secasn1.h"

#include "gtest/gtest.h"

namespace nss_test {

class SECASN1DTest : public ::testing::Test {};

struct InnerSequenceItem {
  SECItem value;
};

struct OuterSequence {
  InnerSequenceItem *item;
};

static const SEC_ASN1Template InnerSequenceTemplate[] = {
    {SEC_ASN1_SEQUENCE, 0, NULL, sizeof(InnerSequenceItem)},
    {SEC_ASN1_ANY, offsetof(InnerSequenceItem, value)},
    {0}};

static const SEC_ASN1Template OuterSequenceTemplate[] = {
    {SEC_ASN1_SEQUENCE_OF, offsetof(OuterSequence, item), InnerSequenceTemplate,
     sizeof(OuterSequence)}};

TEST_F(SECASN1DTest, IndefiniteSequenceInIndefiniteGroup) {
  PLArenaPool *arena = PORT_NewArena(4096);
  OuterSequence *outer = nullptr;
  SECStatus rv;

  // echo "SEQUENCE indefinite {
  //         SEQUENCE indefinite {
  //            PrintableString { \"Test for Bug 1387919\" }
  //         }
  //       }" | ascii2der | xxd -i
  unsigned char ber[] = {0x30, 0x80, 0x30, 0x80, 0x13, 0x14, 0x54, 0x65,
                         0x73, 0x74, 0x20, 0x66, 0x6f, 0x72, 0x20, 0x42,
                         0x75, 0x67, 0x20, 0x31, 0x33, 0x38, 0x37, 0x39,
                         0x31, 0x39, 0x00, 0x00, 0x00, 0x00};

  // Decoding should fail if the trailing EOC is omitted (Bug 1387919)
  SECItem missingEOC = {siBuffer, ber, sizeof(ber) - 2};
  rv = SEC_ASN1DecodeItem(arena, &outer, OuterSequenceTemplate, &missingEOC);
  EXPECT_EQ(SECFailure, rv);

  // With the trailing EOC, this is well-formed BER.
  SECItem goodEncoding = {siBuffer, ber, sizeof(ber)};
  rv = SEC_ASN1DecodeItem(arena, &outer, OuterSequenceTemplate, &goodEncoding);
  EXPECT_EQ(SECSuccess, rv);

  // |outer| should now be a null terminated array of InnerSequenceItems

  // The first item is PrintableString { \"Test for Bug 1387919\" }
  EXPECT_EQ(outer[0].item->value.len, 22U);
  EXPECT_EQ(0, memcmp(outer[0].item->value.data, ber + 4, 22));

  // The second item is the null terminator
  EXPECT_EQ(outer[1].item, nullptr);

  PORT_FreeArena(arena, PR_FALSE);
}

}  // namespace nss_test
