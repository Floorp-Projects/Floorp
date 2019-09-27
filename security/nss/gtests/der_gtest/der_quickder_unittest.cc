/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdint.h>

#include "gtest/gtest.h"
#include "scoped_ptrs_util.h"

#include "nss.h"
#include "prerror.h"
#include "secasn1.h"
#include "secder.h"
#include "secerr.h"
#include "secitem.h"

namespace nss_test {

struct TemplateAndInput {
  const SEC_ASN1Template* t;
  SECItem input;
};

class QuickDERTest : public ::testing::Test,
                     public ::testing::WithParamInterface<TemplateAndInput> {};

static const uint8_t kBitstringTag = 0x03;
static const uint8_t kNullTag = 0x05;
static const uint8_t kLongLength = 0x80;

const SEC_ASN1Template kBitstringTemplate[] = {
    {SEC_ASN1_BIT_STRING, 0, NULL, sizeof(SECItem)}, {0}};

// Empty bitstring with unused bits.
static uint8_t kEmptyBitstringUnused[] = {kBitstringTag, 1, 1};

// Bitstring with 8 unused bits.
static uint8_t kBitstring8Unused[] = {kBitstringTag, 3, 8, 0xff, 0x00};

// Bitstring with >8 unused bits.
static uint8_t kBitstring9Unused[] = {kBitstringTag, 3, 9, 0xff, 0x80};

const SEC_ASN1Template kNullTemplate[] = {
    {SEC_ASN1_NULL, 0, NULL, sizeof(SECItem)}, {0}};

// Length of zero wrongly encoded as 0x80 instead of 0x00.
static uint8_t kOverlongLength_0_0[] = {kNullTag, kLongLength | 0};

// Length of zero wrongly encoded as { 0x81, 0x00 } instead of 0x00.
static uint8_t kOverlongLength_1_0[] = {kNullTag, kLongLength | 1, 0x00};

// Length of zero wrongly encoded as:
//
//     { 0x90, <arbitrary junk of 12 bytes>,
//       0x00, 0x00, 0x00, 0x00 }
//
// instead of 0x00. Note in particular that if there is an integer overflow
// then the arbitrary junk is likely get left-shifted away, as long as there
// are at least sizeof(length) bytes following it. This would be a good way to
// smuggle arbitrary input into DER-encoded data in a way that an non-careful
// parser would ignore.
static uint8_t kOverlongLength_16_0[] = {kNullTag, kLongLength | 0x10,
                                         0x11,     0x22,
                                         0x33,     0x44,
                                         0x55,     0x66,
                                         0x77,     0x88,
                                         0x99,     0xAA,
                                         0xBB,     0xCC,
                                         0x00,     0x00,
                                         0x00,     0x00};

#define TI(t, x)                  \
  {                               \
    t, { siBuffer, x, sizeof(x) } \
  }
static const TemplateAndInput kInvalidDER[] = {
    TI(kBitstringTemplate, kEmptyBitstringUnused),
    TI(kBitstringTemplate, kBitstring8Unused),
    TI(kBitstringTemplate, kBitstring9Unused),
    TI(kNullTemplate, kOverlongLength_0_0),
    TI(kNullTemplate, kOverlongLength_1_0),
    TI(kNullTemplate, kOverlongLength_16_0),
};
#undef TI

TEST_P(QuickDERTest, InvalidLengths) {
  const SECItem& original_input(GetParam().input);

  ScopedSECItem copy_of_input(SECITEM_AllocItem(nullptr, nullptr, 0U));
  ASSERT_TRUE(copy_of_input);
  ASSERT_EQ(SECSuccess,
            SECITEM_CopyItem(nullptr, copy_of_input.get(), &original_input));

  PORTCheapArenaPool pool;
  PORT_InitCheapArena(&pool, DER_DEFAULT_CHUNKSIZE);
  StackSECItem parsed_value;
  ASSERT_EQ(SECFailure,
            SEC_QuickDERDecodeItem(&pool.arena, &parsed_value, GetParam().t,
                                   copy_of_input.get()));
  ASSERT_EQ(SEC_ERROR_BAD_DER, PR_GetError());
  PORT_DestroyCheapArena(&pool);
}

INSTANTIATE_TEST_CASE_P(QuickderTestsInvalidLengths, QuickDERTest,
                        testing::ValuesIn(kInvalidDER));

}  // namespace nss_test
