#include <string>
#include "gtest/gtest.h"

#include "ScopedNSSTypes.h"
#include "mozilla/gtest/MozAssertions.h"
#include "mozilla/Span.h"
#include "nss.h"
#include "secoidt.h"

// From RFC 2202
const unsigned char kTestKey[] = "Jefe";
const unsigned char kTestInput[] = "what do ya want for nothing?";

struct HMACTestCase {
  SECOidTag hashAlg;
  std::string expectedOutput;
};

#define EXPECTED_RESULT(val) std::string(val, sizeof(val) - 1)

static const HMACTestCase HMACTestCases[] = {
    {
        SEC_OID_MD5,
        EXPECTED_RESULT(
            "\x75\x0c\x78\x3e\x6a\xb0\xb5\x03\xea\xa8\x6e\x31\x0a\x5d\xb7\x38"),
    },
    {
        SEC_OID_SHA256,
        EXPECTED_RESULT(
            "\x5b\xdc\xc1\x46\xbf\x60\x75\x4e\x6a\x04\x24\x26\x08\x95\x75\xc7"
            "\x5a\x00\x3f\x08\x9d\x27\x39\x83\x9d\xec\x58\xb9\x64\xec\x38\x43"),
    },
};

#undef EXPECTED_RESULT

class psm_HMAC : public ::testing::Test,
                 public ::testing::WithParamInterface<HMACTestCase> {
 public:
  void SetUp() override { NSS_NoDB_Init(nullptr); }
};

TEST_P(psm_HMAC, Test) {
  mozilla::HMAC hmac;
  const HMACTestCase& testCase(GetParam());
  nsresult rv = hmac.Begin(testCase.hashAlg,
                           mozilla::Span(kTestKey, sizeof(kTestKey) - 1));
  ASSERT_NS_SUCCEEDED(rv);
  rv = hmac.Update(reinterpret_cast<const unsigned char*>(kTestInput),
                   sizeof(kTestInput) - 1);
  ASSERT_NS_SUCCEEDED(rv);
  nsTArray<uint8_t> output;
  rv = hmac.End(output);
  ASSERT_NS_SUCCEEDED(rv);
  EXPECT_EQ(output.Length(), testCase.expectedOutput.length());
  for (size_t i = 0; i < output.Length(); i++) {
    EXPECT_EQ(char(output[i]), testCase.expectedOutput[i]);
  }
}

INSTANTIATE_TEST_SUITE_P(psm_HMAC, psm_HMAC,
                         ::testing::ValuesIn(HMACTestCases));
