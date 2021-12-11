/*
 * Copyright 2018 WebAssembly Community Group participants
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <cassert>
#include <cstdio>
#include <thread>
#include <vector>

#include "gtest/gtest.h"

#include "src/literal.h"

using namespace wabt;

namespace {

enum ParseIntTypeCombo {
  UnsignedOnly,
  SignedAndUnsigned,
  Both,
};

template <typename T, typename F>
void AssertIntEquals(T expected,
                     const char* s,
                     F&& parse_int,
                     ParseIntTypeCombo parse_type = Both) {
  const char* const end = s + strlen(s);
  T actual;
  if (parse_type == UnsignedOnly || parse_type == Both) {
    ASSERT_EQ(Result::Ok,
              parse_int(s, end, &actual, ParseIntType::UnsignedOnly))
        << s;
    ASSERT_EQ(expected, actual);
  } else {
    ASSERT_EQ(Result::Error,
              parse_int(s, end, &actual, ParseIntType::UnsignedOnly))
        << s;
  }

  if (parse_type == SignedAndUnsigned || parse_type == Both) {
    ASSERT_EQ(Result::Ok,
              parse_int(s, end, &actual, ParseIntType::SignedAndUnsigned))
        << s;
    ASSERT_EQ(expected, actual);
  } else {
    ASSERT_EQ(Result::Error,
              parse_int(s, end, &actual, ParseIntType::SignedAndUnsigned))
        << s;
  }
}

void AssertInt8Equals(uint8_t expected,
                      const char* s,
                      ParseIntTypeCombo parse_type = Both) {
  AssertIntEquals(expected, s, ParseInt8, parse_type);
}

void AssertInt16Equals(uint16_t expected,
                       const char* s,
                       ParseIntTypeCombo parse_type = Both) {
  AssertIntEquals(expected, s, ParseInt16, parse_type);
}

void AssertInt32Equals(uint32_t expected,
                       const char* s,
                       ParseIntTypeCombo parse_type = Both) {
  AssertIntEquals(expected, s, ParseInt32, parse_type);
}

void AssertInt64Equals(uint64_t expected,
                       const char* s,
                       ParseIntTypeCombo parse_type = Both) {
  AssertIntEquals(expected, s, ParseInt64, parse_type);
}

void AssertUint128Equals(v128 expected,
                         const char* s) {
  const char* const end = s + strlen(s);
  v128 actual;
  ASSERT_EQ(Result::Ok, ParseUint128(s, end, &actual)) << s;
  ASSERT_EQ(expected, actual);
}

void AssertInt8Fails(const char* s) {
  const char* const end = s + strlen(s);
  uint8_t actual;
  ASSERT_EQ(Result::Error,
            ParseInt8(s, end, &actual, ParseIntType::SignedAndUnsigned))
      << s;
  ASSERT_EQ(Result::Error,
            ParseInt8(s, end, &actual, ParseIntType::UnsignedOnly))
      << s;
}

void AssertInt16Fails(const char* s) {
  const char* const end = s + strlen(s);
  uint16_t actual;
  ASSERT_EQ(Result::Error,
            ParseInt16(s, end, &actual, ParseIntType::SignedAndUnsigned))
      << s;
  ASSERT_EQ(Result::Error,
            ParseInt16(s, end, &actual, ParseIntType::UnsignedOnly))
      << s;
}

void AssertInt32Fails(const char* s) {
  const char* const end = s + strlen(s);
  uint32_t actual;
  ASSERT_EQ(Result::Error,
            ParseInt32(s, end, &actual, ParseIntType::SignedAndUnsigned))
      << s;
  ASSERT_EQ(Result::Error,
            ParseInt32(s, end, &actual, ParseIntType::UnsignedOnly))
      << s;
}

void AssertInt64Fails(const char* s) {
  const char* const end = s + strlen(s);
  uint64_t actual;
  ASSERT_EQ(Result::Error,
            ParseInt64(s, end, &actual, ParseIntType::SignedAndUnsigned))
      << s;
  ASSERT_EQ(Result::Error,
            ParseInt64(s, end, &actual, ParseIntType::UnsignedOnly))
      << s;
}

void AssertUint64Equals(uint64_t expected, const char* s) {
  uint64_t actual;
  ASSERT_EQ(Result::Ok, ParseUint64(s, s + strlen(s), &actual)) << s;
  ASSERT_EQ(expected, actual);
}

void AssertUint64Fails(const char* s) {
  uint64_t actual_bits;
  ASSERT_EQ(Result::Error, ParseUint64(s, s + strlen(s), &actual_bits)) << s;
}

void AssertUint128Fails(const char* s) {
  v128 actual;
  ASSERT_EQ(Result::Error, ParseUint128(s, s + strlen(s), &actual)) << s;
}

void AssertHexFloatEquals(uint32_t expected_bits, const char* s) {
  uint32_t actual_bits;
  ASSERT_EQ(Result::Ok,
            ParseFloat(LiteralType::Hexfloat, s, s + strlen(s), &actual_bits))
      << s;
  ASSERT_EQ(expected_bits, actual_bits) << s;
}

void AssertHexFloatFails(const char* s) {
  uint32_t actual_bits;
  ASSERT_EQ(Result::Error,
            ParseFloat(LiteralType::Hexfloat, s, s + strlen(s), &actual_bits))
      << s;
}

void AssertHexDoubleEquals(uint64_t expected_bits, const char* s) {
  uint64_t actual_bits;
  ASSERT_EQ(Result::Ok,
            ParseDouble(LiteralType::Hexfloat, s, s + strlen(s), &actual_bits))
      << s;
  ASSERT_EQ(expected_bits, actual_bits);
}

void AssertHexDoubleFails(const char* s) {
  uint64_t actual_bits;
  ASSERT_EQ(Result::Error,
            ParseDouble(LiteralType::Hexfloat, s, s + strlen(s), &actual_bits))
      << s;
}

}  // end anonymous namespace

TEST(ParseInt8, Both) {
  AssertInt8Equals(0, "0");
  AssertInt8Equals(100, "100");
  AssertInt8Equals(123, "123");
  AssertInt8Equals(127, "127");
  AssertInt8Equals(255, "255");
  AssertInt8Equals(0xca, "0xca");
  AssertInt8Equals(0x7f, "0x7f");
  AssertInt8Equals(0x80, "0x80");
  AssertInt8Equals(0xff, "0xff");
}

TEST(ParseInt8, SignedAndUnsigned) {
  AssertInt8Equals(128, "-128", SignedAndUnsigned);
  AssertInt8Equals(-0x80, "-0x80", SignedAndUnsigned);
  AssertInt8Equals(255, "-1", SignedAndUnsigned);
  AssertInt8Equals(-1, "-0x1", SignedAndUnsigned);
  AssertInt8Equals(1, "+1", SignedAndUnsigned);
  AssertInt8Equals(-0x7b, "-0x7B", SignedAndUnsigned);
  AssertInt8Equals(0xab, "+0xab", SignedAndUnsigned);
}

TEST(ParseInt8, Invalid) {
  AssertInt8Fails("");
  AssertInt8Fails("-100hello");
  AssertInt8Fails("0XAB");
  AssertInt8Fails("0xga");
  AssertInt8Fails("two");
}

TEST(ParseInt8, Underscores) {
  AssertInt8Equals(123, "12_3", Both);
  AssertInt8Equals(123, "+12_3", SignedAndUnsigned);
  AssertInt8Equals(-123, "-1_23", SignedAndUnsigned);
  AssertInt8Equals(19, "1______9", Both);
  AssertInt8Equals(0xab, "0xa_b", Both);
  AssertInt8Equals(0xab, "+0xa_b", SignedAndUnsigned);
  AssertInt8Equals(-0x7b, "-0x7_b", SignedAndUnsigned);
}

TEST(ParseInt8, Overflow) {
  AssertInt8Fails("256");
  AssertInt8Fails("-129");
  AssertInt8Fails("0x100");
  AssertInt8Fails("-0x81");
  AssertInt8Fails("1231231231231231231231");
}

TEST(ParseInt16, Both) {
  AssertInt16Equals(0, "0");
  AssertInt16Equals(1000, "1000");
  AssertInt16Equals(12345, "12345");
  AssertInt16Equals(32767, "32767");
  AssertInt16Equals(65535, "65535");
  AssertInt16Equals(0xcafe, "0xcafe");
  AssertInt16Equals(0x7fff, "0x7fff");
  AssertInt16Equals(0x8000, "0x8000");
  AssertInt16Equals(0xffff, "0xffff");
}

TEST(ParseInt16, SignedAndUnsigned) {
  AssertInt16Equals(32768, "-32768", SignedAndUnsigned);
  AssertInt16Equals(-0x8000, "-0x8000", SignedAndUnsigned);
  AssertInt16Equals(65535, "-1", SignedAndUnsigned);
  AssertInt16Equals(-1, "-0x1", SignedAndUnsigned);
  AssertInt16Equals(1, "+1", SignedAndUnsigned);
  AssertInt16Equals(-0x7bcd, "-0x7BCD", SignedAndUnsigned);
  AssertInt16Equals(0xabcd, "+0xabcd", SignedAndUnsigned);
}

TEST(ParseInt16, Invalid) {
  AssertInt16Fails("");
  AssertInt16Fails("-100hello");
  AssertInt16Fails("0XABCD");
  AssertInt16Fails("0xgabb");
  AssertInt16Fails("two");
}

TEST(ParseInt16, Underscores) {
  AssertInt16Equals(12345, "12_345", Both);
  AssertInt16Equals(12345, "+12_345", SignedAndUnsigned);
  AssertInt16Equals(-12345, "-123_45", SignedAndUnsigned);
  AssertInt16Equals(19, "1______9", Both);
  AssertInt16Equals(0xabcd, "0xa_b_c_d", Both);
  AssertInt16Equals(0xabcd, "+0xa_b_c_d", SignedAndUnsigned);
  AssertInt16Equals(-0x7bcd, "-0x7_b_c_d", SignedAndUnsigned);
}

TEST(ParseInt16, Overflow) {
  AssertInt16Fails("65536");
  AssertInt16Fails("-32769");
  AssertInt16Fails("0x10000");
  AssertInt16Fails("-0x8001");
  AssertInt16Fails("1231231231231231231231");
}

TEST(ParseInt32, Both) {
  AssertInt32Equals(0, "0");
  AssertInt32Equals(1000, "1000");
  AssertInt32Equals(123456789, "123456789");
  AssertInt32Equals(2147483647, "2147483647");
  AssertInt32Equals(4294967295u, "4294967295");
  AssertInt32Equals(0xcafef00du, "0xcafef00d");
  AssertInt32Equals(0x7fffffff, "0x7fffffff");
  AssertInt32Equals(0x80000000u, "0x80000000");
  AssertInt32Equals(0xffffffffu, "0xffffffff");
}

TEST(ParseInt32, SignedAndUnsigned) {
  AssertInt32Equals(2147483648, "-2147483648", SignedAndUnsigned);
  AssertInt32Equals(-0x80000000u, "-0x80000000", SignedAndUnsigned);
  AssertInt32Equals(4294967295u, "-1", SignedAndUnsigned);
  AssertInt32Equals(-1, "-0x1", SignedAndUnsigned);
  AssertInt32Equals(1, "+1", SignedAndUnsigned);
  AssertInt32Equals(-0xabcd, "-0xABCD", SignedAndUnsigned);
  AssertInt32Equals(0xabcd, "+0xabcd", SignedAndUnsigned);
}

TEST(ParseInt32, Invalid) {
  AssertInt32Fails("");
  AssertInt32Fails("-100hello");
  AssertInt32Fails("0XABCDEF");
  AssertInt32Fails("0xgabba");
  AssertInt32Fails("two");
}

TEST(ParseInt32, Underscores) {
  AssertInt32Equals(123456789, "12_345_6789", Both);
  AssertInt32Equals(123456789, "+12_345_6789", SignedAndUnsigned);
  AssertInt32Equals(-123456789, "-12345_6789", SignedAndUnsigned);
  AssertInt32Equals(19, "1______9", Both);
  AssertInt32Equals(0xabcd, "0xa_b_c_d", Both);
  AssertInt32Equals(0xabcd, "+0xa_b_c_d", SignedAndUnsigned);
  AssertInt32Equals(-0xabcd, "-0xa_b_c_d", SignedAndUnsigned);
}

TEST(ParseInt32, Overflow) {
  AssertInt32Fails("4294967296");
  AssertInt32Fails("-2147483649");
  AssertInt32Fails("0x100000000");
  AssertInt32Fails("-0x80000001");
  AssertInt32Fails("1231231231231231231231");
}

TEST(ParseInt64, Both) {
  AssertInt64Equals(0, "0");
  AssertInt64Equals(1000, "1000");
  AssertInt64Equals(123456789, "123456789");
  AssertInt64Equals(9223372036854775807ull, "9223372036854775807");
  AssertInt64Equals(18446744073709551615ull, "18446744073709551615");
  AssertInt64Equals(0x7fffffffffffffffull, "0x7fffffffffffffff");
  AssertInt64Equals(0x8000000000000000ull, "0x8000000000000000");
  AssertInt64Equals(0xffffffffffffffffull, "0xffffffffffffffff");
}

TEST(ParseInt64, SignedAndUnsigned) {
  AssertInt64Equals(9223372036854775808ull, "-9223372036854775808",
                    SignedAndUnsigned);
  AssertInt64Equals(18446744073709551615ull, "-1", SignedAndUnsigned);
  AssertInt64Equals(-1, "-0x1", SignedAndUnsigned);
  AssertInt64Equals(1, "+1", SignedAndUnsigned);
  AssertInt64Equals(-0x0bcdefabcdefabcdull, "-0x0BCDEFABCDEFABCD",
                    SignedAndUnsigned);
  AssertInt64Equals(0xabcdefabcdefabcdull, "+0xabcdefabcdefabcd",
                    SignedAndUnsigned);
}

TEST(ParseInt64, Invalid) {
  AssertInt64Fails("");
  AssertInt64Fails("-100hello");
  AssertInt64Fails("0XABCDEF");
  AssertInt64Fails("0xgabba");
  AssertInt64Fails("two");
}

TEST(ParseInt64, Underscores) {
  AssertInt64Equals(123456789, "12_345_6789", Both);
  AssertInt64Equals(123456789, "+12_345_6789", SignedAndUnsigned);
  AssertInt64Equals(-123456789, "-12345_6789", SignedAndUnsigned);
  AssertInt64Equals(19, "1______9", Both);
  AssertInt64Equals(0xabcd, "0xa_b_c_d", Both);
  AssertInt64Equals(0xabcd, "+0xa_b_c_d", SignedAndUnsigned);
  AssertInt64Equals(-0xabcd, "-0xa_b_c_d", SignedAndUnsigned);
}

TEST(ParseInt64, Overflow) {
  AssertInt64Fails("18446744073709551616");
  AssertInt64Fails("-9223372036854775809");
  AssertInt32Fails("0x10000000000000000");
  AssertInt32Fails("-0x80000000000000001");
  AssertInt64Fails("1231231231231231231231");
}

TEST(ParseUint64, Basic) {
  AssertUint64Equals(0, "0");
  AssertUint64Equals(1000, "1000");
  AssertUint64Equals(123456789, "123456789");
  AssertUint64Equals(1844674407370955161ull, "1844674407370955161");
  AssertUint64Equals(18446744073709551615ull, "18446744073709551615");

  AssertUint64Equals(0, "0x0");
  AssertUint64Equals(0x1000, "0x1000");
  AssertUint64Equals(0x123456789, "0x123456789");
  AssertUint64Equals(0xabcdef, "0xabcdef");
  AssertUint64Equals(0xffffffffffffffull, "0xffffffffffffff");
  AssertUint64Equals(0xfffffffffffffffull, "0xfffffffffffffff");

  AssertUint64Equals(0xabcdefabcdefabcdull, "0xabcdefabcdefabcd");
}

TEST(ParseUint64, NoOctal) {
  AssertUint64Equals(100, "0100");
  AssertUint64Equals(888, "0000888");
}

TEST(ParseUint64, Invalid) {
  AssertUint64Fails("");
  AssertUint64Fails("-100");
  AssertUint64Fails("0XABCDEF");
  AssertUint64Fails("0xgabba");
  AssertUint64Fails("two");
}

TEST(ParseUint64, Underscores) {
  AssertUint64Equals(123456789, "12_345_6789");
  AssertUint64Equals(19, "1______9");
  AssertUint64Equals(0xabcd, "0xa_b_c_d");
}

TEST(ParseUint64, Overflow) {
  AssertUint64Fails("0x10000000000000000");
  AssertUint64Fails("18446744073709551616");
  AssertUint64Fails("62857453058642199420");
  AssertUint64Fails("82000999361882825820");
  AssertUint64Fails("126539114687237086210");
  AssertUint64Fails("10000000000000000000000000000000000000000");
}

TEST(ParseUint128, Basic) {
  AssertUint128Equals({0, 0, 0, 0}, "0");
  AssertUint128Equals({1, 0, 0, 0}, "1");
  AssertUint128Equals({0x100f0e0d, 0x0c0b0a09, 0x08070605, 0x04030201},
                      "5332529520247008778714484145835150861");
  AssertUint128Equals({0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff},
                      "340282366920938463463374607431768211455");
  AssertUint128Equals({0, 0, 1, 0}, "18446744073709551616");
}

TEST(ParseUint128, Invalid) {
  AssertUint128Fails("");
  AssertUint128Fails("-1");
  AssertUint128Fails("340282366920938463463374607431768211456");
  AssertUint128Fails("123a456");
}

TEST(ParseFloat, NonCanonical) {
  AssertHexFloatEquals(0x3f800000, "0x00000000000000000000001.0p0");
  AssertHexFloatEquals(0x3f800000, "0x1.00000000000000000000000p0");
  AssertHexFloatEquals(0x3f800000, "0x0.0000000000000000000001p88");
}

TEST(ParseFloat, Basic) {
  AssertHexFloatEquals(0, "0x0p0");
  AssertHexFloatEquals(0x3f800000, "0x1");
}

TEST(ParseFloat, Rounding) {
  // |------- 23 bits -----| V-- extra bit
  //
  // 11111111111111111111101 0  ==> no rounding
  AssertHexFloatEquals(0x7f7ffffd, "0x1.fffffap127");
  // 11111111111111111111101 1  ==> round up
  AssertHexFloatEquals(0x7f7ffffe, "0x1.fffffbp127");
  // 11111111111111111111110 0  ==> no rounding
  AssertHexFloatEquals(0x7f7ffffe, "0x1.fffffcp127");
  // 11111111111111111111110 1  ==> round down
  AssertHexFloatEquals(0x7f7ffffe, "0x1.fffffdp127");
  // 11111111111111111111111 0  ==> no rounding
  AssertHexFloatEquals(0x7f7fffff, "0x1.fffffep127");
}

// Duplicate the spec tests here for easier debugging.
TEST(ParseFloat, RoundingSpec) {
  static const struct {
    const char* input;
    uint32_t output;
  } kTests[] = {
    {"+0x1.00000100000000000p-50", 0x26800000},
    {"-0x1.00000100000000000p-50", 0xa6800000},
    {"+0x1.00000100000000001p-50", 0x26800001},
    {"-0x1.00000100000000001p-50", 0xa6800001},
    {"+0x1.000001fffffffffffp-50", 0x26800001},
    {"-0x1.000001fffffffffffp-50", 0xa6800001},
    {"+0x1.00000200000000000p-50", 0x26800001},
    {"-0x1.00000200000000000p-50", 0xa6800001},
    {"+0x1.00000200000000001p-50", 0x26800001},
    {"-0x1.00000200000000001p-50", 0xa6800001},
    {"+0x1.000002fffffffffffp-50", 0x26800001},
    {"-0x1.000002fffffffffffp-50", 0xa6800001},
    {"+0x1.00000300000000000p-50", 0x26800002},
    {"-0x1.00000300000000000p-50", 0xa6800002},
    {"+0x1.00000300000000001p-50", 0x26800002},
    {"-0x1.00000300000000001p-50", 0xa6800002},
    {"+0x1.000003fffffffffffp-50", 0x26800002},
    {"-0x1.000003fffffffffffp-50", 0xa6800002},
    {"+0x1.00000400000000000p-50", 0x26800002},
    {"-0x1.00000400000000000p-50", 0xa6800002},
    {"+0x1.00000400000000001p-50", 0x26800002},
    {"-0x1.00000400000000001p-50", 0xa6800002},
    {"+0x1.000004fffffffffffp-50", 0x26800002},
    {"-0x1.000004fffffffffffp-50", 0xa6800002},
    {"+0x1.00000500000000000p-50", 0x26800002},
    {"-0x1.00000500000000000p-50", 0xa6800002},
    {"+0x1.00000500000000001p-50", 0x26800003},
    {"-0x1.00000500000000001p-50", 0xa6800003},
    {"+0x4000.004000000p-64", 0x26800000},
    {"-0x4000.004000000p-64", 0xa6800000},
    {"+0x4000.004000001p-64", 0x26800001},
    {"-0x4000.004000001p-64", 0xa6800001},
    {"+0x4000.007ffffffp-64", 0x26800001},
    {"-0x4000.007ffffffp-64", 0xa6800001},
    {"+0x4000.008000000p-64", 0x26800001},
    {"-0x4000.008000000p-64", 0xa6800001},
    {"+0x4000.008000001p-64", 0x26800001},
    {"-0x4000.008000001p-64", 0xa6800001},
    {"+0x4000.00bffffffp-64", 0x26800001},
    {"-0x4000.00bffffffp-64", 0xa6800001},
    {"+0x4000.00c000000p-64", 0x26800002},
    {"-0x4000.00c000000p-64", 0xa6800002},
    {"+0x4000.00c000001p-64", 0x26800002},
    {"-0x4000.00c000001p-64", 0xa6800002},
    {"+0x4000.00fffffffp-64", 0x26800002},
    {"-0x4000.00fffffffp-64", 0xa6800002},
    {"+0x4000.010000001p-64", 0x26800002},
    {"-0x4000.010000001p-64", 0xa6800002},
    {"+0x4000.013ffffffp-64", 0x26800002},
    {"-0x4000.013ffffffp-64", 0xa6800002},
    {"+0x4000.014000001p-64", 0x26800003},
    {"-0x4000.014000001p-64", 0xa6800003},
    {"+0x1.00000100000000000p+50", 0x58800000},
    {"-0x1.00000100000000000p+50", 0xd8800000},
    {"+0x1.00000100000000001p+50", 0x58800001},
    {"-0x1.00000100000000001p+50", 0xd8800001},
    {"+0x1.000001fffffffffffp+50", 0x58800001},
    {"-0x1.000001fffffffffffp+50", 0xd8800001},
    {"+0x1.00000200000000000p+50", 0x58800001},
    {"-0x1.00000200000000000p+50", 0xd8800001},
    {"+0x1.00000200000000001p+50", 0x58800001},
    {"-0x1.00000200000000001p+50", 0xd8800001},
    {"+0x1.000002fffffffffffp+50", 0x58800001},
    {"-0x1.000002fffffffffffp+50", 0xd8800001},
    {"+0x1.00000300000000000p+50", 0x58800002},
    {"-0x1.00000300000000000p+50", 0xd8800002},
    {"+0x1.00000300000000001p+50", 0x58800002},
    {"-0x1.00000300000000001p+50", 0xd8800002},
    {"+0x1.000003fffffffffffp+50", 0x58800002},
    {"-0x1.000003fffffffffffp+50", 0xd8800002},
    {"+0x1.00000400000000000p+50", 0x58800002},
    {"-0x1.00000400000000000p+50", 0xd8800002},
    {"+0x1.00000400000000001p+50", 0x58800002},
    {"-0x1.00000400000000001p+50", 0xd8800002},
    {"+0x1.000004fffffffffffp+50", 0x58800002},
    {"-0x1.000004fffffffffffp+50", 0xd8800002},
    {"+0x1.00000500000000000p+50", 0x58800002},
    {"-0x1.00000500000000000p+50", 0xd8800002},
    {"+0x1.00000500000000001p+50", 0x58800003},
    {"-0x1.00000500000000001p+50", 0xd8800003},
    {"+0x4000004000000", 0x58800000},
    {"-0x4000004000000", 0xd8800000},
    {"+0x4000004000001", 0x58800001},
    {"-0x4000004000001", 0xd8800001},
    {"+0x4000007ffffff", 0x58800001},
    {"-0x4000007ffffff", 0xd8800001},
    {"+0x4000008000000", 0x58800001},
    {"-0x4000008000000", 0xd8800001},
    {"+0x4000008000001", 0x58800001},
    {"-0x4000008000001", 0xd8800001},
    {"+0x400000bffffff", 0x58800001},
    {"-0x400000bffffff", 0xd8800001},
    {"+0x400000c000000", 0x58800002},
    {"-0x400000c000000", 0xd8800002},
    {"+0x0.00000100000000000p-126", 0x0},
    {"-0x0.00000100000000000p-126", 0x80000000},
    {"+0x0.00000100000000001p-126", 0x1},
    {"-0x0.00000100000000001p-126", 0x80000001},
    {"+0x0.00000101000000000p-126", 0x1},
    {"+0x0.000001fffffffffffp-126", 0x1},
    {"-0x0.000001fffffffffffp-126", 0x80000001},
    {"+0x0.00000200000000000p-126", 0x1},
    {"-0x0.00000200000000000p-126", 0x80000001},
    {"+0x0.00000200000000001p-126", 0x1},
    {"-0x0.00000200000000001p-126", 0x80000001},
    {"+0x0.000002fffffffffffp-126", 0x1},
    {"-0x0.000002fffffffffffp-126", 0x80000001},
    {"+0x0.00000300000000000p-126", 0x2},
    {"-0x0.00000300000000000p-126", 0x80000002},
    {"+0x0.00000300000000001p-126", 0x2},
    {"-0x0.00000300000000001p-126", 0x80000002},
    {"+0x0.000003fffffffffffp-126", 0x2},
    {"-0x0.000003fffffffffffp-126", 0x80000002},
    {"+0x0.00000400000000000p-126", 0x2},
    {"-0x0.00000400000000000p-126", 0x80000002},
    {"+0x0.00000400000000001p-126", 0x2},
    {"-0x0.00000400000000001p-126", 0x80000002},
    {"+0x0.000004fffffffffffp-126", 0x2},
    {"-0x0.000004fffffffffffp-126", 0x80000002},
    {"+0x0.00000500000000000p-126", 0x2},
    {"-0x0.00000500000000000p-126", 0x80000002},
    {"+0x0.00000500000000001p-126", 0x3},
    {"-0x0.00000500000000001p-126", 0x80000003},
    {"+0x1.fffffe8p127", 0x7f7fffff},
    {"-0x1.fffffe8p127", 0xff7fffff},
    {"+0x1.fffffefffffff8p127", 0x7f7fffff},
    {"-0x1.fffffefffffff8p127", 0xff7fffff},
    {"+0x1.fffffefffffffffffp127", 0x7f7fffff},
    {"-0x1.fffffefffffffffffp127", 0xff7fffff},
  };

  for (auto test: kTests) {
    AssertHexFloatEquals(test.output, test.input);
  }
}

TEST(ParseFloat, OutOfRange) {
  AssertHexFloatFails("0x1p128");
  AssertHexFloatFails("-0x1p128");
  AssertHexFloatFails("0x1.ffffffp127");
  AssertHexFloatFails("-0x1.ffffffp127");
}

TEST(ParseDouble, NonCanonical) {
  AssertHexDoubleEquals(0x3ff0000000000000, "0x00000000000000000000001.0p0");
  AssertHexDoubleEquals(0x3ff0000000000000, "0x1.00000000000000000000000p0");
  AssertHexDoubleEquals(0x3ff0000000000000, "0x0.0000000000000000000001p88");
}

TEST(ParseDouble, Rounding) {
  // |-------------------- 52 bits ---------------------| V-- extra bit
  //
  // 1111111111111111111111111111111111111111111111111101 0  ==> no rounding
  AssertHexDoubleEquals(0x7feffffffffffffd, "0x1.ffffffffffffd0p1023");
  // 1111111111111111111111111111111111111111111111111101 1  ==> round up
  AssertHexDoubleEquals(0x7feffffffffffffe, "0x1.ffffffffffffd8p1023");
  // 1111111111111111111111111111111111111111111111111110 0  ==> no rounding
  AssertHexDoubleEquals(0x7feffffffffffffe, "0x1.ffffffffffffe0p1023");
  // 1111111111111111111111111111111111111111111111111110 1  ==> round down
  AssertHexDoubleEquals(0x7feffffffffffffe, "0x1.ffffffffffffe8p1023");
  // 1111111111111111111111111111111111111111111111111111 0  ==> no rounding
  AssertHexDoubleEquals(0x7fefffffffffffff, "0x1.fffffffffffff0p1023");
}

TEST(ParseDouble, OutOfRange) {
  AssertHexDoubleFails("0x1p1024");
  AssertHexDoubleFails("-0x1p1024");
  AssertHexDoubleFails("0x1.fffffffffffff8p1023");
  AssertHexDoubleFails("-0x1.fffffffffffff8p1023");
}

// Duplicate the spec tests here for easier debugging.
TEST(ParseDouble, RoundingSpec) {
  static const struct {
    const char* input;
    uint64_t output;
  } kTests[] = {
    {"+0x1.000000000000080000000000p-600", 1905022642377719808ull},
    {"-0x1.000000000000080000000000p-600", 11128394679232495616ull},
    {"+0x1.000000000000080000000001p-600", 1905022642377719809ull},
    {"-0x1.000000000000080000000001p-600", 11128394679232495617ull},
    {"+0x1.0000000000000fffffffffffp-600", 1905022642377719809ull},
    {"-0x1.0000000000000fffffffffffp-600", 11128394679232495617ull},
    {"+0x1.000000000000100000000000p-600", 1905022642377719809ull},
    {"-0x1.000000000000100000000000p-600", 11128394679232495617ull},
    {"+0x1.000000000000100000000001p-600", 1905022642377719809ull},
    {"-0x1.000000000000100000000001p-600", 11128394679232495617ull},
    {"+0x1.00000000000017ffffffffffp-600", 1905022642377719809ull},
    {"-0x1.00000000000017ffffffffffp-600", 11128394679232495617ull},
    {"+0x1.000000000000180000000000p-600", 1905022642377719810ull},
    {"-0x1.000000000000180000000000p-600", 11128394679232495618ull},
    {"+0x1.000000000000180000000001p-600", 1905022642377719810ull},
    {"-0x1.000000000000180000000001p-600", 11128394679232495618ull},
    {"+0x1.0000000000001fffffffffffp-600", 1905022642377719810ull},
    {"-0x1.0000000000001fffffffffffp-600", 11128394679232495618ull},
    {"+0x1.000000000000200000000000p-600", 1905022642377719810ull},
    {"-0x1.000000000000200000000000p-600", 11128394679232495618ull},
    {"+0x1.000000000000200000000001p-600", 1905022642377719810ull},
    {"-0x1.000000000000200000000001p-600", 11128394679232495618ull},
    {"+0x1.00000000000027ffffffffffp-600", 1905022642377719810ull},
    {"-0x1.00000000000027ffffffffffp-600", 11128394679232495618ull},
    {"+0x1.000000000000280000000001p-600", 1905022642377719811ull},
    {"-0x1.000000000000280000000001p-600", 11128394679232495619ull},
    {"+0x8000000.000000400000000000p-627", 1905022642377719808ull},
    {"-0x8000000.000000400000000000p-627", 11128394679232495616ull},
    {"+0x8000000.000000400000000001p-627", 1905022642377719809ull},
    {"-0x8000000.000000400000000001p-627", 11128394679232495617ull},
    {"+0x8000000.0000007fffffffffffp-627", 1905022642377719809ull},
    {"-0x8000000.0000007fffffffffffp-627", 11128394679232495617ull},
    {"+0x8000000.000000800000000000p-627", 1905022642377719809ull},
    {"-0x8000000.000000800000000000p-627", 11128394679232495617ull},
    {"+0x8000000.000000800000000001p-627", 1905022642377719809ull},
    {"-0x8000000.000000800000000001p-627", 11128394679232495617ull},
    {"+0x8000000.000000bfffffffffffp-627", 1905022642377719809ull},
    {"-0x8000000.000000bfffffffffffp-627", 11128394679232495617ull},
    {"+0x8000000.000000c00000000000p-627", 1905022642377719810ull},
    {"-0x8000000.000000c00000000000p-627", 11128394679232495618ull},
    {"+0x8000000.000000c00000000001p-627", 1905022642377719810ull},
    {"-0x8000000.000000c00000000001p-627", 11128394679232495618ull},
    {"+0x8000000.000000ffffffffffffp-627", 1905022642377719810ull},
    {"-0x8000000.000000ffffffffffffp-627", 11128394679232495618ull},
    {"+0x8000000.000001000000000000p-627", 1905022642377719810ull},
    {"-0x8000000.000001000000000000p-627", 11128394679232495618ull},
    {"+0x8000000.000001000000000001p-627", 1905022642377719810ull},
    {"-0x8000000.000001000000000001p-627", 11128394679232495618ull},
    {"+0x8000000.0000013fffffffffffp-627", 1905022642377719810ull},
    {"-0x8000000.0000013fffffffffffp-627", 11128394679232495618ull},
    {"+0x8000000.000001400000000001p-627", 1905022642377719811ull},
    {"-0x8000000.000001400000000001p-627", 11128394679232495619ull},
    {"+0x1.000000000000080000000000p+600", 7309342195222315008ull},
    {"-0x1.000000000000080000000000p+600", 16532714232077090816ull},
    {"+0x1.000000000000080000000001p+600", 7309342195222315009ull},
    {"-0x1.000000000000080000000001p+600", 16532714232077090817ull},
    {"+0x1.0000000000000fffffffffffp+600", 7309342195222315009ull},
    {"-0x1.0000000000000fffffffffffp+600", 16532714232077090817ull},
    {"+0x1.000000000000100000000000p+600", 7309342195222315009ull},
    {"-0x1.000000000000100000000000p+600", 16532714232077090817ull},
    {"+0x1.000000000000100000000001p+600", 7309342195222315009ull},
    {"-0x1.000000000000100000000001p+600", 16532714232077090817ull},
    {"+0x1.00000000000017ffffffffffp+600", 7309342195222315009ull},
    {"-0x1.00000000000017ffffffffffp+600", 16532714232077090817ull},
    {"+0x1.000000000000180000000000p+600", 7309342195222315010ull},
    {"-0x1.000000000000180000000000p+600", 16532714232077090818ull},
    {"+0x1.000000000000180000000001p+600", 7309342195222315010ull},
    {"-0x1.000000000000180000000001p+600", 16532714232077090818ull},
    {"+0x1.0000000000001fffffffffffp+600", 7309342195222315010ull},
    {"-0x1.0000000000001fffffffffffp+600", 16532714232077090818ull},
    {"+0x1.000000000000200000000000p+600", 7309342195222315010ull},
    {"-0x1.000000000000200000000000p+600", 16532714232077090818ull},
    {"+0x1.000000000000200000000001p+600", 7309342195222315010ull},
    {"-0x1.000000000000200000000001p+600", 16532714232077090818ull},
    {"+0x1.00000000000027ffffffffffp+600", 7309342195222315010ull},
    {"-0x1.00000000000027ffffffffffp+600", 16532714232077090818ull},
    {"+0x1.000000000000280000000000p+600", 7309342195222315010ull},
    {"-0x1.000000000000280000000000p+600", 16532714232077090818ull},
    {"+0x1.000000000000280000000001p+600", 7309342195222315011ull},
    {"-0x1.000000000000280000000001p+600", 16532714232077090819ull},
    {"+0x2000000000000100000000000", 5044031582654955520ull},
    {"-0x2000000000000100000000000", 14267403619509731328ull},
    {"+0x2000000000000100000000001", 5044031582654955521ull},
    {"-0x2000000000000100000000001", 14267403619509731329ull},
    {"+0x20000000000001fffffffffff", 5044031582654955521ull},
    {"-0x20000000000001fffffffffff", 14267403619509731329ull},
    {"+0x2000000000000200000000000", 5044031582654955521ull},
    {"-0x2000000000000200000000000", 14267403619509731329ull},
    {"+0x2000000000000200000000001", 5044031582654955521ull},
    {"-0x2000000000000200000000001", 14267403619509731329ull},
    {"+0x20000000000002fffffffffff", 5044031582654955521ull},
    {"-0x20000000000002fffffffffff", 14267403619509731329ull},
    {"+0x2000000000000300000000000", 5044031582654955522ull},
    {"-0x2000000000000300000000000", 14267403619509731330ull},
    {"+0x2000000000000300000000001", 5044031582654955522ull},
    {"-0x2000000000000300000000001", 14267403619509731330ull},
    {"+0x20000000000003fffffffffff", 5044031582654955522ull},
    {"-0x20000000000003fffffffffff", 14267403619509731330ull},
    {"+0x2000000000000400000000000", 5044031582654955522ull},
    {"-0x2000000000000400000000000", 14267403619509731330ull},
    {"+0x2000000000000400000000001", 5044031582654955522ull},
    {"-0x2000000000000400000000001", 14267403619509731330ull},
    {"+0x20000000000004fffffffffff", 5044031582654955522ull},
    {"-0x20000000000004fffffffffff", 14267403619509731330ull},
    {"+0x2000000000000500000000000", 5044031582654955522ull},
    {"-0x2000000000000500000000000", 14267403619509731330ull},
    {"+0x2000000000000500000000001", 5044031582654955523ull},
    {"-0x2000000000000500000000001", 14267403619509731331ull},
    {"+0x0.000000000000080000000000p-1022", 0ull},
    {"-0x0.000000000000080000000000p-1022", 9223372036854775808ull},
    {"+0x0.000000000000080000000001p-1022", 1ull},
    {"-0x0.000000000000080000000001p-1022", 9223372036854775809ull},
    {"+0x0.0000000000000fffffffffffp-1022", 1ull},
    {"-0x0.0000000000000fffffffffffp-1022", 9223372036854775809ull},
    {"+0x0.000000000000100000000000p-1022", 1ull},
    {"-0x0.000000000000100000000000p-1022", 9223372036854775809ull},
    {"+0x0.000000000000100000000001p-1022", 1ull},
    {"-0x0.000000000000100000000001p-1022", 9223372036854775809ull},
    {"+0x0.00000000000017ffffffffffp-1022", 1ull},
    {"-0x0.00000000000017ffffffffffp-1022", 9223372036854775809ull},
    {"+0x0.000000000000180000000000p-1022", 2ull},
    {"-0x0.000000000000180000000000p-1022", 9223372036854775810ull},
    {"+0x0.000000000000180000000001p-1022", 2ull},
    {"-0x0.000000000000180000000001p-1022", 9223372036854775810ull},
    {"+0x0.0000000000001fffffffffffp-1022", 2ull},
    {"-0x0.0000000000001fffffffffffp-1022", 9223372036854775810ull},
    {"+0x0.000000000000200000000000p-1022", 2ull},
    {"-0x0.000000000000200000000000p-1022", 9223372036854775810ull},
    {"+0x0.000000000000200000000001p-1022", 2ull},
    {"-0x0.000000000000200000000001p-1022", 9223372036854775810ull},
    {"+0x0.00000000000027ffffffffffp-1022", 2ull},
    {"-0x0.00000000000027ffffffffffp-1022", 9223372036854775810ull},
    {"+0x0.000000000000280000000000p-1022", 2ull},
    {"-0x0.000000000000280000000000p-1022", 9223372036854775810ull},
    {"+0x1.000000000000280000000001p-1022", 4503599627370499ull},
    {"-0x1.000000000000280000000001p-1022", 9227875636482146307ull},
    {"+0x1.fffffffffffff4p1023", 9218868437227405311ull},
    {"-0x1.fffffffffffff4p1023", 18442240474082181119ull},
    {"+0x1.fffffffffffff7ffffffp1023", 9218868437227405311ull},
    {"-0x1.fffffffffffff7ffffffp1023", 18442240474082181119ull},
  };

  for (auto test: kTests) {
    AssertHexDoubleEquals(test.output, test.input);
  }
}

void AssertWriteUint128Equals(const v128& value, const std::string& expected) {
  assert(expected.length() < 128);
  char buffer[128];
  WriteUint128(buffer, 128, value);
  std::string actual(buffer, buffer + expected.length());
  ASSERT_EQ(expected, actual);
  ASSERT_EQ(buffer[expected.length()], '\0');
}

TEST(WriteUint128, Basic) {
  AssertWriteUint128Equals({0, 0, 0, 0}, "0");
  AssertWriteUint128Equals({1, 0, 0, 0}, "1");
  AssertWriteUint128Equals({0x100f0e0d, 0x0c0b0a09, 0x08070605, 0x04030201},
                           "5332529520247008778714484145835150861");
  AssertWriteUint128Equals({0x00112233, 0x44556677, 0x8899aabb, 0xccddeeff},
                           "272314856204801878456120017448021860915");
  AssertWriteUint128Equals({0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff},
                           "340282366920938463463374607431768211455");
  AssertWriteUint128Equals({0, 0, 1, 0}, "18446744073709551616");
}

TEST(WriteUint128, BufferTooSmall) {
  {
    char buffer[20];
    WriteUint128(buffer, 20, {0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff});
    ASSERT_EQ(buffer[19], '\0');
    std::string actual(buffer, buffer + 19);
    ASSERT_EQ("3402823669209384634", actual);
  }

  {
    char buffer[3];
    WriteUint128(buffer, 3, {123, 0, 0, 0});
    ASSERT_EQ(buffer[0], '1');
    ASSERT_EQ(buffer[1], '2');
    ASSERT_EQ(buffer[2], '\0');
  }
}
