// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at http://mozilla.org/MPL/2.0/.

#include "testvectors/gcm-vectors.h"
#include "gtest/gtest.h"
#include "util.h"

#include "gcm.h"

namespace nss_test {

class GHashTest : public ::testing::TestWithParam<gcm_kat_value> {
 protected:
  void TestGHash(const gcm_kat_value val, bool sw) {
    // Read test data.
    std::vector<uint8_t> hash_key = hex_string_to_bytes(val.hash_key);
    ASSERT_EQ(16UL, hash_key.size());
    std::vector<uint8_t> additional_data =
        hex_string_to_bytes(val.additional_data);
    std::vector<uint8_t> result = hex_string_to_bytes(val.result);
    std::vector<uint8_t> cipher_text(result.begin(), result.end() - 16);
    std::vector<uint8_t> expected = hex_string_to_bytes(val.ghash);
    ASSERT_EQ(16UL, expected.size());

    // Prepare context.
    gcmHashContext ghashCtx;
    ASSERT_EQ(SECSuccess, gcmHash_InitContext(&ghashCtx, hash_key.data(), sw));

    // Hash additional_data, cipher_text.
    gcmHash_Reset(&ghashCtx,
                  const_cast<const unsigned char *>(additional_data.data()),
                  additional_data.size());
    gcmHash_Update(&ghashCtx,
                   const_cast<const unsigned char *>(cipher_text.data()),
                   cipher_text.size());

    // Finalise (hash in the length).
    uint8_t result_bytes[16];
    unsigned int out_len;
    ASSERT_EQ(SECSuccess, gcmHash_Final(&ghashCtx, result_bytes, &out_len, 16));
    ASSERT_EQ(16U, out_len);
    EXPECT_EQ(expected, std::vector<uint8_t>(result_bytes, result_bytes + 16));
  }
};

#ifdef NSS_X86_OR_X64
TEST_P(GHashTest, KAT_X86_HW) { TestGHash(GetParam(), false); }
#endif
TEST_P(GHashTest, KAT_Sftw) { TestGHash(GetParam(), true); }

INSTANTIATE_TEST_CASE_P(NISTTestVector, GHashTest,
                        ::testing::ValuesIn(kGcmKatValues));

}  // nss_test
