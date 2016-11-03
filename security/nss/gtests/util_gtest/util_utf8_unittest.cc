// -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// vim: set ts=2 et sw=2 tw=80:
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at http://mozilla.org/MPL/2.0/.

#include "secport.h"

#include "gtest/gtest.h"
#include "prnetdb.h"

#include <stdint.h>
#include <string.h>
#include <string>

namespace nss_test {

// Structures to represent test cases.  These are small enough that
// passing by value isn't a problem.

struct Ucs4Case {
  PRUint32 c;
  const char *utf8;
};

struct Ucs2Case {
  PRUint16 c;
  const char *utf8;
};

struct Utf16Case {
  PRUint32 c;
  PRUint16 w[2];
};

struct Utf16BadCase {
  PRUint16 w[3];
};

// Test classes for parameterized tests:

class Ucs4Test : public ::testing::TestWithParam<Ucs4Case> {};

class Ucs2Test : public ::testing::TestWithParam<Ucs2Case> {};

class Utf16Test : public ::testing::TestWithParam<Utf16Case> {};

class BadUtf8Test : public ::testing::TestWithParam<const char *> {};

class BadUtf16Test : public ::testing::TestWithParam<Utf16BadCase> {};

class Iso88591Test : public ::testing::TestWithParam<Ucs2Case> {};

// Tests of sec_port_ucs4_utf8_conversion_function, by itself, on
// valid inputs:

TEST_P(Ucs4Test, ToUtf8) {
  const Ucs4Case testCase = GetParam();
  PRUint32 nc = PR_htonl(testCase.c);
  unsigned char utf8[8] = {0};
  unsigned int len = 0;

  PRBool result = sec_port_ucs4_utf8_conversion_function(
      PR_FALSE, (unsigned char *)&nc, sizeof(nc), utf8, sizeof(utf8), &len);

  ASSERT_TRUE(result);
  ASSERT_LT(len, sizeof(utf8));
  EXPECT_EQ(std::string(testCase.utf8), std::string((char *)utf8, len));
  EXPECT_EQ('\0', utf8[len]);
}

TEST_P(Ucs4Test, FromUtf8) {
  const Ucs4Case testCase = GetParam();
  PRUint32 nc;
  unsigned int len = 0;

  PRBool result = sec_port_ucs4_utf8_conversion_function(
      PR_TRUE, (unsigned char *)testCase.utf8, strlen(testCase.utf8),
      (unsigned char *)&nc, sizeof(nc), &len);

  ASSERT_TRUE(result);
  ASSERT_EQ(sizeof(nc), len);
  EXPECT_EQ(testCase.c, PR_ntohl(nc));
}

TEST_P(Ucs4Test, DestTooSmall) {
  const Ucs4Case testCase = GetParam();
  PRUint32 nc = PR_htonl(testCase.c);
  unsigned char utf8[8];
  unsigned char *utf8end = utf8 + sizeof(utf8);
  unsigned int len = strlen(testCase.utf8) - 1;

  ASSERT_LE(len, sizeof(utf8));

  PRBool result = sec_port_ucs4_utf8_conversion_function(
      PR_FALSE, (unsigned char *)&nc, sizeof(nc), utf8end - len, len, &len);

  ASSERT_FALSE(result);
  ASSERT_EQ(strlen(testCase.utf8), len);
}

// Tests of sec_port_ucs2_utf8_conversion_function, by itself, on
// valid inputs:

TEST_P(Ucs2Test, ToUtf8) {
  const Ucs2Case testCase = GetParam();
  PRUint16 nc = PR_htons(testCase.c);
  unsigned char utf8[8] = {0};
  unsigned int len = 0;

  PRBool result = sec_port_ucs2_utf8_conversion_function(
      PR_FALSE, (unsigned char *)&nc, sizeof(nc), utf8, sizeof(utf8), &len);

  ASSERT_TRUE(result);
  ASSERT_LT(len, sizeof(utf8));
  EXPECT_EQ(std::string(testCase.utf8), std::string((char *)utf8, len));
  EXPECT_EQ('\0', utf8[len]);
}

TEST_P(Ucs2Test, FromUtf8) {
  const Ucs2Case testCase = GetParam();
  PRUint16 nc;
  unsigned int len = 0;

  PRBool result = sec_port_ucs2_utf8_conversion_function(
      PR_TRUE, (unsigned char *)testCase.utf8, strlen(testCase.utf8),
      (unsigned char *)&nc, sizeof(nc), &len);

  ASSERT_EQ(PR_TRUE, result);
  ASSERT_EQ(sizeof(nc), len);
  EXPECT_EQ(testCase.c, PR_ntohs(nc));
}

TEST_P(Ucs2Test, DestTooSmall) {
  const Ucs2Case testCase = GetParam();
  PRUint16 nc = PR_htons(testCase.c);
  unsigned char utf8[8];
  unsigned char *utf8end = utf8 + sizeof(utf8);
  unsigned int len = strlen(testCase.utf8) - 1;

  ASSERT_LE(len, sizeof(utf8));

  PRBool result = sec_port_ucs2_utf8_conversion_function(
      PR_FALSE, (unsigned char *)&nc, sizeof(nc), utf8end - len, len, &len);

  ASSERT_EQ(result, PR_FALSE);
  ASSERT_EQ(strlen(testCase.utf8), len);
}

// Tests using UTF-16 and UCS-4 conversion together:

TEST_P(Utf16Test, From16To32) {
  const Utf16Case testCase = GetParam();
  PRUint16 from[2] = {PR_htons(testCase.w[0]), PR_htons(testCase.w[1])};
  PRUint32 to;
  unsigned char utf8[8];
  unsigned int len = 0;

  PRBool result = sec_port_ucs2_utf8_conversion_function(
      PR_FALSE, (unsigned char *)&from, sizeof(from), utf8, sizeof(utf8), &len);

  ASSERT_EQ(PR_TRUE, result);

  result = sec_port_ucs4_utf8_conversion_function(
      PR_TRUE, utf8, len, (unsigned char *)&to, sizeof(to), &len);

  ASSERT_EQ(PR_TRUE, result);
  ASSERT_EQ(sizeof(to), len);
  EXPECT_EQ(testCase.c, PR_ntohl(to));
}

TEST_P(Utf16Test, From32To16) {
  const Utf16Case testCase = GetParam();
  PRUint32 from = PR_htonl(testCase.c);
  unsigned char utf8[8];
  unsigned int len = 0;

  PRBool result = sec_port_ucs4_utf8_conversion_function(
      PR_FALSE, (unsigned char *)&from, sizeof(from), utf8, sizeof(utf8), &len);

  ASSERT_EQ(PR_TRUE, result);
  const std::string utf8copy((char *)utf8, len);
  PRUint16 to[2];

  result = sec_port_ucs2_utf8_conversion_function(
      PR_TRUE, utf8, len, (unsigned char *)&to, sizeof(to), &len);

  ASSERT_EQ(PR_TRUE, result);
  ASSERT_EQ(sizeof(to), len);
  EXPECT_EQ(testCase.w[0], PR_ntohs(to[0]));
  EXPECT_EQ(testCase.w[1], PR_ntohs(to[1]));
}

TEST_P(Utf16Test, SameUtf8) {
  const Utf16Case testCase = GetParam();
  PRUint32 from32 = PR_htonl(testCase.c);
  unsigned char utf8from32[8];
  unsigned int lenFrom32 = 0;

  PRBool result = sec_port_ucs4_utf8_conversion_function(
      PR_FALSE, (unsigned char *)&from32, sizeof(from32), utf8from32,
      sizeof(utf8from32), &lenFrom32);

  ASSERT_TRUE(result);
  ASSERT_LE(lenFrom32, sizeof(utf8from32));

  PRUint16 from16[2] = {PR_htons(testCase.w[0]), PR_htons(testCase.w[1])};
  unsigned char utf8from16[8];
  unsigned int lenFrom16 = 0;

  result = sec_port_ucs2_utf8_conversion_function(
      PR_FALSE, (unsigned char *)&from16, sizeof(from16), utf8from16,
      sizeof(utf8from16), &lenFrom16);

  ASSERT_TRUE(result);
  ASSERT_LE(lenFrom16, sizeof(utf8from16));

  EXPECT_EQ(std::string((char *)utf8from32, lenFrom32),
            std::string((char *)utf8from16, lenFrom16));
}

// Tests of invalid UTF-8 input:

TEST_P(BadUtf8Test, HasNoUcs2) {
  const char *const utf8 = GetParam();
  unsigned char destBuf[30];
  unsigned int len = 0;

  PRBool result = sec_port_ucs2_utf8_conversion_function(
      PR_TRUE, (unsigned char *)utf8, strlen(utf8), destBuf, sizeof(destBuf),
      &len);

  EXPECT_FALSE(result);
}

TEST_P(BadUtf8Test, HasNoUcs4) {
  const char *const utf8 = GetParam();
  unsigned char destBuf[30];
  unsigned int len = 0;

  PRBool result = sec_port_ucs4_utf8_conversion_function(
      PR_TRUE, (unsigned char *)utf8, strlen(utf8), destBuf, sizeof(destBuf),
      &len);

  EXPECT_FALSE(result);
}

// Tests of invalid UTF-16 input:

TEST_P(BadUtf16Test, HasNoUtf8) {
  const Utf16BadCase testCase = GetParam();
  Utf16BadCase srcBuf;
  unsigned int len;
  static const size_t maxLen = PR_ARRAY_SIZE(srcBuf.w);

  size_t srcLen = 0;
  while (testCase.w[srcLen] != 0) {
    srcBuf.w[srcLen] = PR_htons(testCase.w[srcLen]);
    srcLen++;
    ASSERT_LT(srcLen, maxLen);
  }

  unsigned char destBuf[18];
  PRBool result = sec_port_ucs2_utf8_conversion_function(
      PR_FALSE, (unsigned char *)srcBuf.w, srcLen * sizeof(PRUint16), destBuf,
      sizeof(destBuf), &len);

  EXPECT_FALSE(result);
}

// Tests of sec_port_iso88591_utf8_conversion_function on valid inputs:

TEST_P(Iso88591Test, ToUtf8) {
  const Ucs2Case testCase = GetParam();
  unsigned char iso88591 = testCase.c;
  unsigned char utf8[3] = {0};
  unsigned int len = 0;

  ASSERT_EQ(testCase.c, (PRUint16)iso88591);

  PRBool result = sec_port_iso88591_utf8_conversion_function(
      &iso88591, 1, utf8, sizeof(utf8), &len);

  ASSERT_TRUE(result);
  ASSERT_LT(len, sizeof(utf8));
  EXPECT_EQ(std::string(testCase.utf8), std::string((char *)utf8, len));
  EXPECT_EQ(0U, utf8[len]);
}

// Tests for the various representations of NUL (which the above
// NUL-terminated test cases omitted):

TEST(Utf8Zeroes, From32To8) {
  unsigned int len;
  PRUint32 from = 0;
  unsigned char to;

  PRBool result = sec_port_ucs4_utf8_conversion_function(
      PR_FALSE, (unsigned char *)&from, sizeof(from), &to, sizeof(to), &len);

  ASSERT_TRUE(result);
  ASSERT_EQ(sizeof(to), len);
  EXPECT_EQ(0U, to);
}

TEST(Utf8Zeroes, From16To8) {
  unsigned int len;
  PRUint16 from = 0;
  unsigned char to;

  PRBool result = sec_port_ucs2_utf8_conversion_function(
      PR_FALSE, (unsigned char *)&from, sizeof(from), &to, sizeof(to), &len);

  ASSERT_TRUE(result);
  ASSERT_EQ(sizeof(to), len);
  EXPECT_EQ(0U, to);
}

TEST(Utf8Zeroes, From8To32) {
  unsigned int len;
  unsigned char from = 0;
  PRUint32 to;

  PRBool result = sec_port_ucs4_utf8_conversion_function(
      PR_TRUE, &from, sizeof(from), (unsigned char *)&to, sizeof(to), &len);

  ASSERT_TRUE(result);
  ASSERT_EQ(sizeof(to), len);
  EXPECT_EQ(0U, to);
}

TEST(Utf8Zeroes, From8To16) {
  unsigned int len;
  unsigned char from = 0;
  PRUint16 to;

  PRBool result = sec_port_ucs2_utf8_conversion_function(
      PR_TRUE, &from, sizeof(from), (unsigned char *)&to, sizeof(to), &len);

  ASSERT_TRUE(result);
  ASSERT_EQ(sizeof(to), len);
  EXPECT_EQ(0U, to);
}

// UCS-4 <-> UTF-8 cases

const Ucs4Case kUcs4Cases[] = {
    {0x00000001, "\x01"},
    {0x00000002, "\x02"},
    {0x00000003, "\x03"},
    {0x00000004, "\x04"},
    {0x00000007, "\x07"},
    {0x00000008, "\x08"},
    {0x0000000F, "\x0F"},
    {0x00000010, "\x10"},
    {0x0000001F, "\x1F"},
    {0x00000020, "\x20"},
    {0x0000003F, "\x3F"},
    {0x00000040, "\x40"},
    {0x0000007F, "\x7F"},

    {0x00000080, "\xC2\x80"},
    {0x00000081, "\xC2\x81"},
    {0x00000082, "\xC2\x82"},
    {0x00000084, "\xC2\x84"},
    {0x00000088, "\xC2\x88"},
    {0x00000090, "\xC2\x90"},
    {0x000000A0, "\xC2\xA0"},
    {0x000000C0, "\xC3\x80"},
    {0x000000FF, "\xC3\xBF"},
    {0x00000100, "\xC4\x80"},
    {0x00000101, "\xC4\x81"},
    {0x00000102, "\xC4\x82"},
    {0x00000104, "\xC4\x84"},
    {0x00000108, "\xC4\x88"},
    {0x00000110, "\xC4\x90"},
    {0x00000120, "\xC4\xA0"},
    {0x00000140, "\xC5\x80"},
    {0x00000180, "\xC6\x80"},
    {0x000001FF, "\xC7\xBF"},
    {0x00000200, "\xC8\x80"},
    {0x00000201, "\xC8\x81"},
    {0x00000202, "\xC8\x82"},
    {0x00000204, "\xC8\x84"},
    {0x00000208, "\xC8\x88"},
    {0x00000210, "\xC8\x90"},
    {0x00000220, "\xC8\xA0"},
    {0x00000240, "\xC9\x80"},
    {0x00000280, "\xCA\x80"},
    {0x00000300, "\xCC\x80"},
    {0x000003FF, "\xCF\xBF"},
    {0x00000400, "\xD0\x80"},
    {0x00000401, "\xD0\x81"},
    {0x00000402, "\xD0\x82"},
    {0x00000404, "\xD0\x84"},
    {0x00000408, "\xD0\x88"},
    {0x00000410, "\xD0\x90"},
    {0x00000420, "\xD0\xA0"},
    {0x00000440, "\xD1\x80"},
    {0x00000480, "\xD2\x80"},
    {0x00000500, "\xD4\x80"},
    {0x00000600, "\xD8\x80"},
    {0x000007FF, "\xDF\xBF"},

    {0x00000800, "\xE0\xA0\x80"},
    {0x00000801, "\xE0\xA0\x81"},
    {0x00000802, "\xE0\xA0\x82"},
    {0x00000804, "\xE0\xA0\x84"},
    {0x00000808, "\xE0\xA0\x88"},
    {0x00000810, "\xE0\xA0\x90"},
    {0x00000820, "\xE0\xA0\xA0"},
    {0x00000840, "\xE0\xA1\x80"},
    {0x00000880, "\xE0\xA2\x80"},
    {0x00000900, "\xE0\xA4\x80"},
    {0x00000A00, "\xE0\xA8\x80"},
    {0x00000C00, "\xE0\xB0\x80"},
    {0x00000FFF, "\xE0\xBF\xBF"},
    {0x00001000, "\xE1\x80\x80"},
    {0x00001001, "\xE1\x80\x81"},
    {0x00001002, "\xE1\x80\x82"},
    {0x00001004, "\xE1\x80\x84"},
    {0x00001008, "\xE1\x80\x88"},
    {0x00001010, "\xE1\x80\x90"},
    {0x00001020, "\xE1\x80\xA0"},
    {0x00001040, "\xE1\x81\x80"},
    {0x00001080, "\xE1\x82\x80"},
    {0x00001100, "\xE1\x84\x80"},
    {0x00001200, "\xE1\x88\x80"},
    {0x00001400, "\xE1\x90\x80"},
    {0x00001800, "\xE1\xA0\x80"},
    {0x00001FFF, "\xE1\xBF\xBF"},
    {0x00002000, "\xE2\x80\x80"},
    {0x00002001, "\xE2\x80\x81"},
    {0x00002002, "\xE2\x80\x82"},
    {0x00002004, "\xE2\x80\x84"},
    {0x00002008, "\xE2\x80\x88"},
    {0x00002010, "\xE2\x80\x90"},
    {0x00002020, "\xE2\x80\xA0"},
    {0x00002040, "\xE2\x81\x80"},
    {0x00002080, "\xE2\x82\x80"},
    {0x00002100, "\xE2\x84\x80"},
    {0x00002200, "\xE2\x88\x80"},
    {0x00002400, "\xE2\x90\x80"},
    {0x00002800, "\xE2\xA0\x80"},
    {0x00003000, "\xE3\x80\x80"},
    {0x00003FFF, "\xE3\xBF\xBF"},
    {0x00004000, "\xE4\x80\x80"},
    {0x00004001, "\xE4\x80\x81"},
    {0x00004002, "\xE4\x80\x82"},
    {0x00004004, "\xE4\x80\x84"},
    {0x00004008, "\xE4\x80\x88"},
    {0x00004010, "\xE4\x80\x90"},
    {0x00004020, "\xE4\x80\xA0"},
    {0x00004040, "\xE4\x81\x80"},
    {0x00004080, "\xE4\x82\x80"},
    {0x00004100, "\xE4\x84\x80"},
    {0x00004200, "\xE4\x88\x80"},
    {0x00004400, "\xE4\x90\x80"},
    {0x00004800, "\xE4\xA0\x80"},
    {0x00005000, "\xE5\x80\x80"},
    {0x00006000, "\xE6\x80\x80"},
    {0x00007FFF, "\xE7\xBF\xBF"},
    {0x00008000, "\xE8\x80\x80"},
    {0x00008001, "\xE8\x80\x81"},
    {0x00008002, "\xE8\x80\x82"},
    {0x00008004, "\xE8\x80\x84"},
    {0x00008008, "\xE8\x80\x88"},
    {0x00008010, "\xE8\x80\x90"},
    {0x00008020, "\xE8\x80\xA0"},
    {0x00008040, "\xE8\x81\x80"},
    {0x00008080, "\xE8\x82\x80"},
    {0x00008100, "\xE8\x84\x80"},
    {0x00008200, "\xE8\x88\x80"},
    {0x00008400, "\xE8\x90\x80"},
    {0x00008800, "\xE8\xA0\x80"},
    {0x00009000, "\xE9\x80\x80"},
    {0x0000A000, "\xEA\x80\x80"},
    {0x0000C000, "\xEC\x80\x80"},
    {0x0000FFFF, "\xEF\xBF\xBF"},

    {0x00010000, "\xF0\x90\x80\x80"},
    {0x00010001, "\xF0\x90\x80\x81"},
    {0x00010002, "\xF0\x90\x80\x82"},
    {0x00010004, "\xF0\x90\x80\x84"},
    {0x00010008, "\xF0\x90\x80\x88"},
    {0x00010010, "\xF0\x90\x80\x90"},
    {0x00010020, "\xF0\x90\x80\xA0"},
    {0x00010040, "\xF0\x90\x81\x80"},
    {0x00010080, "\xF0\x90\x82\x80"},
    {0x00010100, "\xF0\x90\x84\x80"},
    {0x00010200, "\xF0\x90\x88\x80"},
    {0x00010400, "\xF0\x90\x90\x80"},
    {0x00010800, "\xF0\x90\xA0\x80"},
    {0x00011000, "\xF0\x91\x80\x80"},
    {0x00012000, "\xF0\x92\x80\x80"},
    {0x00014000, "\xF0\x94\x80\x80"},
    {0x00018000, "\xF0\x98\x80\x80"},
    {0x0001FFFF, "\xF0\x9F\xBF\xBF"},
    {0x00020000, "\xF0\xA0\x80\x80"},
    {0x00020001, "\xF0\xA0\x80\x81"},
    {0x00020002, "\xF0\xA0\x80\x82"},
    {0x00020004, "\xF0\xA0\x80\x84"},
    {0x00020008, "\xF0\xA0\x80\x88"},
    {0x00020010, "\xF0\xA0\x80\x90"},
    {0x00020020, "\xF0\xA0\x80\xA0"},
    {0x00020040, "\xF0\xA0\x81\x80"},
    {0x00020080, "\xF0\xA0\x82\x80"},
    {0x00020100, "\xF0\xA0\x84\x80"},
    {0x00020200, "\xF0\xA0\x88\x80"},
    {0x00020400, "\xF0\xA0\x90\x80"},
    {0x00020800, "\xF0\xA0\xA0\x80"},
    {0x00021000, "\xF0\xA1\x80\x80"},
    {0x00022000, "\xF0\xA2\x80\x80"},
    {0x00024000, "\xF0\xA4\x80\x80"},
    {0x00028000, "\xF0\xA8\x80\x80"},
    {0x00030000, "\xF0\xB0\x80\x80"},
    {0x0003FFFF, "\xF0\xBF\xBF\xBF"},
    {0x00040000, "\xF1\x80\x80\x80"},
    {0x00040001, "\xF1\x80\x80\x81"},
    {0x00040002, "\xF1\x80\x80\x82"},
    {0x00040004, "\xF1\x80\x80\x84"},
    {0x00040008, "\xF1\x80\x80\x88"},
    {0x00040010, "\xF1\x80\x80\x90"},
    {0x00040020, "\xF1\x80\x80\xA0"},
    {0x00040040, "\xF1\x80\x81\x80"},
    {0x00040080, "\xF1\x80\x82\x80"},
    {0x00040100, "\xF1\x80\x84\x80"},
    {0x00040200, "\xF1\x80\x88\x80"},
    {0x00040400, "\xF1\x80\x90\x80"},
    {0x00040800, "\xF1\x80\xA0\x80"},
    {0x00041000, "\xF1\x81\x80\x80"},
    {0x00042000, "\xF1\x82\x80\x80"},
    {0x00044000, "\xF1\x84\x80\x80"},
    {0x00048000, "\xF1\x88\x80\x80"},
    {0x00050000, "\xF1\x90\x80\x80"},
    {0x00060000, "\xF1\xA0\x80\x80"},
    {0x0007FFFF, "\xF1\xBF\xBF\xBF"},
    {0x00080000, "\xF2\x80\x80\x80"},
    {0x00080001, "\xF2\x80\x80\x81"},
    {0x00080002, "\xF2\x80\x80\x82"},
    {0x00080004, "\xF2\x80\x80\x84"},
    {0x00080008, "\xF2\x80\x80\x88"},
    {0x00080010, "\xF2\x80\x80\x90"},
    {0x00080020, "\xF2\x80\x80\xA0"},
    {0x00080040, "\xF2\x80\x81\x80"},
    {0x00080080, "\xF2\x80\x82\x80"},
    {0x00080100, "\xF2\x80\x84\x80"},
    {0x00080200, "\xF2\x80\x88\x80"},
    {0x00080400, "\xF2\x80\x90\x80"},
    {0x00080800, "\xF2\x80\xA0\x80"},
    {0x00081000, "\xF2\x81\x80\x80"},
    {0x00082000, "\xF2\x82\x80\x80"},
    {0x00084000, "\xF2\x84\x80\x80"},
    {0x00088000, "\xF2\x88\x80\x80"},
    {0x00090000, "\xF2\x90\x80\x80"},
    {0x000A0000, "\xF2\xA0\x80\x80"},
    {0x000C0000, "\xF3\x80\x80\x80"},
    {0x000FFFFF, "\xF3\xBF\xBF\xBF"},
    {0x00100000, "\xF4\x80\x80\x80"},
    {0x00100001, "\xF4\x80\x80\x81"},
    {0x00100002, "\xF4\x80\x80\x82"},
    {0x00100004, "\xF4\x80\x80\x84"},
    {0x00100008, "\xF4\x80\x80\x88"},
    {0x00100010, "\xF4\x80\x80\x90"},
    {0x00100020, "\xF4\x80\x80\xA0"},
    {0x00100040, "\xF4\x80\x81\x80"},
    {0x00100080, "\xF4\x80\x82\x80"},
    {0x00100100, "\xF4\x80\x84\x80"},
    {0x00100200, "\xF4\x80\x88\x80"},
    {0x00100400, "\xF4\x80\x90\x80"},
    {0x00100800, "\xF4\x80\xA0\x80"},
    {0x00101000, "\xF4\x81\x80\x80"},
    {0x00102000, "\xF4\x82\x80\x80"},
    {0x00104000, "\xF4\x84\x80\x80"},
    {0x00108000, "\xF4\x88\x80\x80"},
    {0x0010FFFF, "\xF4\x8F\xBF\xBF"},
};

// UCS-2 <-> UTF-8 cases (divided into ISO-8859-1 vs. not).

const Ucs2Case kIso88591Cases[] = {
    {0x0001, "\x01"},     {0x0002, "\x02"},     {0x0003, "\x03"},
    {0x0004, "\x04"},     {0x0007, "\x07"},     {0x0008, "\x08"},
    {0x000F, "\x0F"},     {0x0010, "\x10"},     {0x001F, "\x1F"},
    {0x0020, "\x20"},     {0x003F, "\x3F"},     {0x0040, "\x40"},
    {0x007F, "\x7F"},

    {0x0080, "\xC2\x80"}, {0x0081, "\xC2\x81"}, {0x0082, "\xC2\x82"},
    {0x0084, "\xC2\x84"}, {0x0088, "\xC2\x88"}, {0x0090, "\xC2\x90"},
    {0x00A0, "\xC2\xA0"}, {0x00C0, "\xC3\x80"}, {0x00FF, "\xC3\xBF"},
};

const Ucs2Case kUcs2Cases[] = {
    {0x0100, "\xC4\x80"},     {0x0101, "\xC4\x81"},
    {0x0102, "\xC4\x82"},     {0x0104, "\xC4\x84"},
    {0x0108, "\xC4\x88"},     {0x0110, "\xC4\x90"},
    {0x0120, "\xC4\xA0"},     {0x0140, "\xC5\x80"},
    {0x0180, "\xC6\x80"},     {0x01FF, "\xC7\xBF"},
    {0x0200, "\xC8\x80"},     {0x0201, "\xC8\x81"},
    {0x0202, "\xC8\x82"},     {0x0204, "\xC8\x84"},
    {0x0208, "\xC8\x88"},     {0x0210, "\xC8\x90"},
    {0x0220, "\xC8\xA0"},     {0x0240, "\xC9\x80"},
    {0x0280, "\xCA\x80"},     {0x0300, "\xCC\x80"},
    {0x03FF, "\xCF\xBF"},     {0x0400, "\xD0\x80"},
    {0x0401, "\xD0\x81"},     {0x0402, "\xD0\x82"},
    {0x0404, "\xD0\x84"},     {0x0408, "\xD0\x88"},
    {0x0410, "\xD0\x90"},     {0x0420, "\xD0\xA0"},
    {0x0440, "\xD1\x80"},     {0x0480, "\xD2\x80"},
    {0x0500, "\xD4\x80"},     {0x0600, "\xD8\x80"},
    {0x07FF, "\xDF\xBF"},

    {0x0800, "\xE0\xA0\x80"}, {0x0801, "\xE0\xA0\x81"},
    {0x0802, "\xE0\xA0\x82"}, {0x0804, "\xE0\xA0\x84"},
    {0x0808, "\xE0\xA0\x88"}, {0x0810, "\xE0\xA0\x90"},
    {0x0820, "\xE0\xA0\xA0"}, {0x0840, "\xE0\xA1\x80"},
    {0x0880, "\xE0\xA2\x80"}, {0x0900, "\xE0\xA4\x80"},
    {0x0A00, "\xE0\xA8\x80"}, {0x0C00, "\xE0\xB0\x80"},
    {0x0FFF, "\xE0\xBF\xBF"}, {0x1000, "\xE1\x80\x80"},
    {0x1001, "\xE1\x80\x81"}, {0x1002, "\xE1\x80\x82"},
    {0x1004, "\xE1\x80\x84"}, {0x1008, "\xE1\x80\x88"},
    {0x1010, "\xE1\x80\x90"}, {0x1020, "\xE1\x80\xA0"},
    {0x1040, "\xE1\x81\x80"}, {0x1080, "\xE1\x82\x80"},
    {0x1100, "\xE1\x84\x80"}, {0x1200, "\xE1\x88\x80"},
    {0x1400, "\xE1\x90\x80"}, {0x1800, "\xE1\xA0\x80"},
    {0x1FFF, "\xE1\xBF\xBF"}, {0x2000, "\xE2\x80\x80"},
    {0x2001, "\xE2\x80\x81"}, {0x2002, "\xE2\x80\x82"},
    {0x2004, "\xE2\x80\x84"}, {0x2008, "\xE2\x80\x88"},
    {0x2010, "\xE2\x80\x90"}, {0x2020, "\xE2\x80\xA0"},
    {0x2040, "\xE2\x81\x80"}, {0x2080, "\xE2\x82\x80"},
    {0x2100, "\xE2\x84\x80"}, {0x2200, "\xE2\x88\x80"},
    {0x2400, "\xE2\x90\x80"}, {0x2800, "\xE2\xA0\x80"},
    {0x3000, "\xE3\x80\x80"}, {0x3FFF, "\xE3\xBF\xBF"},
    {0x4000, "\xE4\x80\x80"}, {0x4001, "\xE4\x80\x81"},
    {0x4002, "\xE4\x80\x82"}, {0x4004, "\xE4\x80\x84"},
    {0x4008, "\xE4\x80\x88"}, {0x4010, "\xE4\x80\x90"},
    {0x4020, "\xE4\x80\xA0"}, {0x4040, "\xE4\x81\x80"},
    {0x4080, "\xE4\x82\x80"}, {0x4100, "\xE4\x84\x80"},
    {0x4200, "\xE4\x88\x80"}, {0x4400, "\xE4\x90\x80"},
    {0x4800, "\xE4\xA0\x80"}, {0x5000, "\xE5\x80\x80"},
    {0x6000, "\xE6\x80\x80"}, {0x7FFF, "\xE7\xBF\xBF"},
    {0x8000, "\xE8\x80\x80"}, {0x8001, "\xE8\x80\x81"},
    {0x8002, "\xE8\x80\x82"}, {0x8004, "\xE8\x80\x84"},
    {0x8008, "\xE8\x80\x88"}, {0x8010, "\xE8\x80\x90"},
    {0x8020, "\xE8\x80\xA0"}, {0x8040, "\xE8\x81\x80"},
    {0x8080, "\xE8\x82\x80"}, {0x8100, "\xE8\x84\x80"},
    {0x8200, "\xE8\x88\x80"}, {0x8400, "\xE8\x90\x80"},
    {0x8800, "\xE8\xA0\x80"}, {0x9000, "\xE9\x80\x80"},
    {0xA000, "\xEA\x80\x80"}, {0xC000, "\xEC\x80\x80"},
    {0xFB01, "\xEF\xAC\x81"}, {0xFFFF, "\xEF\xBF\xBF"}};

// UTF-16 <-> UCS-4 cases

const Utf16Case kUtf16Cases[] = {{0x00010000, {0xD800, 0xDC00}},
                                 {0x00010001, {0xD800, 0xDC01}},
                                 {0x00010002, {0xD800, 0xDC02}},
                                 {0x00010003, {0xD800, 0xDC03}},
                                 {0x00010004, {0xD800, 0xDC04}},
                                 {0x00010007, {0xD800, 0xDC07}},
                                 {0x00010008, {0xD800, 0xDC08}},
                                 {0x0001000F, {0xD800, 0xDC0F}},
                                 {0x00010010, {0xD800, 0xDC10}},
                                 {0x0001001F, {0xD800, 0xDC1F}},
                                 {0x00010020, {0xD800, 0xDC20}},
                                 {0x0001003F, {0xD800, 0xDC3F}},
                                 {0x00010040, {0xD800, 0xDC40}},
                                 {0x0001007F, {0xD800, 0xDC7F}},
                                 {0x00010080, {0xD800, 0xDC80}},
                                 {0x00010081, {0xD800, 0xDC81}},
                                 {0x00010082, {0xD800, 0xDC82}},
                                 {0x00010084, {0xD800, 0xDC84}},
                                 {0x00010088, {0xD800, 0xDC88}},
                                 {0x00010090, {0xD800, 0xDC90}},
                                 {0x000100A0, {0xD800, 0xDCA0}},
                                 {0x000100C0, {0xD800, 0xDCC0}},
                                 {0x000100FF, {0xD800, 0xDCFF}},
                                 {0x00010100, {0xD800, 0xDD00}},
                                 {0x00010101, {0xD800, 0xDD01}},
                                 {0x00010102, {0xD800, 0xDD02}},
                                 {0x00010104, {0xD800, 0xDD04}},
                                 {0x00010108, {0xD800, 0xDD08}},
                                 {0x00010110, {0xD800, 0xDD10}},
                                 {0x00010120, {0xD800, 0xDD20}},
                                 {0x00010140, {0xD800, 0xDD40}},
                                 {0x00010180, {0xD800, 0xDD80}},
                                 {0x000101FF, {0xD800, 0xDDFF}},
                                 {0x00010200, {0xD800, 0xDE00}},
                                 {0x00010201, {0xD800, 0xDE01}},
                                 {0x00010202, {0xD800, 0xDE02}},
                                 {0x00010204, {0xD800, 0xDE04}},
                                 {0x00010208, {0xD800, 0xDE08}},
                                 {0x00010210, {0xD800, 0xDE10}},
                                 {0x00010220, {0xD800, 0xDE20}},
                                 {0x00010240, {0xD800, 0xDE40}},
                                 {0x00010280, {0xD800, 0xDE80}},
                                 {0x00010300, {0xD800, 0xDF00}},
                                 {0x000103FF, {0xD800, 0xDFFF}},
                                 {0x00010400, {0xD801, 0xDC00}},
                                 {0x00010401, {0xD801, 0xDC01}},
                                 {0x00010402, {0xD801, 0xDC02}},
                                 {0x00010404, {0xD801, 0xDC04}},
                                 {0x00010408, {0xD801, 0xDC08}},
                                 {0x00010410, {0xD801, 0xDC10}},
                                 {0x00010420, {0xD801, 0xDC20}},
                                 {0x00010440, {0xD801, 0xDC40}},
                                 {0x00010480, {0xD801, 0xDC80}},
                                 {0x00010500, {0xD801, 0xDD00}},
                                 {0x00010600, {0xD801, 0xDE00}},
                                 {0x000107FF, {0xD801, 0xDFFF}},
                                 {0x00010800, {0xD802, 0xDC00}},
                                 {0x00010801, {0xD802, 0xDC01}},
                                 {0x00010802, {0xD802, 0xDC02}},
                                 {0x00010804, {0xD802, 0xDC04}},
                                 {0x00010808, {0xD802, 0xDC08}},
                                 {0x00010810, {0xD802, 0xDC10}},
                                 {0x00010820, {0xD802, 0xDC20}},
                                 {0x00010840, {0xD802, 0xDC40}},
                                 {0x00010880, {0xD802, 0xDC80}},
                                 {0x00010900, {0xD802, 0xDD00}},
                                 {0x00010A00, {0xD802, 0xDE00}},
                                 {0x00010C00, {0xD803, 0xDC00}},
                                 {0x00010FFF, {0xD803, 0xDFFF}},
                                 {0x00011000, {0xD804, 0xDC00}},
                                 {0x00011001, {0xD804, 0xDC01}},
                                 {0x00011002, {0xD804, 0xDC02}},
                                 {0x00011004, {0xD804, 0xDC04}},
                                 {0x00011008, {0xD804, 0xDC08}},
                                 {0x00011010, {0xD804, 0xDC10}},
                                 {0x00011020, {0xD804, 0xDC20}},
                                 {0x00011040, {0xD804, 0xDC40}},
                                 {0x00011080, {0xD804, 0xDC80}},
                                 {0x00011100, {0xD804, 0xDD00}},
                                 {0x00011200, {0xD804, 0xDE00}},
                                 {0x00011400, {0xD805, 0xDC00}},
                                 {0x00011800, {0xD806, 0xDC00}},
                                 {0x00011FFF, {0xD807, 0xDFFF}},
                                 {0x00012000, {0xD808, 0xDC00}},
                                 {0x00012001, {0xD808, 0xDC01}},
                                 {0x00012002, {0xD808, 0xDC02}},
                                 {0x00012004, {0xD808, 0xDC04}},
                                 {0x00012008, {0xD808, 0xDC08}},
                                 {0x00012010, {0xD808, 0xDC10}},
                                 {0x00012020, {0xD808, 0xDC20}},
                                 {0x00012040, {0xD808, 0xDC40}},
                                 {0x00012080, {0xD808, 0xDC80}},
                                 {0x00012100, {0xD808, 0xDD00}},
                                 {0x00012200, {0xD808, 0xDE00}},
                                 {0x00012400, {0xD809, 0xDC00}},
                                 {0x00012800, {0xD80A, 0xDC00}},
                                 {0x00013000, {0xD80C, 0xDC00}},
                                 {0x00013FFF, {0xD80F, 0xDFFF}},
                                 {0x00014000, {0xD810, 0xDC00}},
                                 {0x00014001, {0xD810, 0xDC01}},
                                 {0x00014002, {0xD810, 0xDC02}},
                                 {0x00014004, {0xD810, 0xDC04}},
                                 {0x00014008, {0xD810, 0xDC08}},
                                 {0x00014010, {0xD810, 0xDC10}},
                                 {0x00014020, {0xD810, 0xDC20}},
                                 {0x00014040, {0xD810, 0xDC40}},
                                 {0x00014080, {0xD810, 0xDC80}},
                                 {0x00014100, {0xD810, 0xDD00}},
                                 {0x00014200, {0xD810, 0xDE00}},
                                 {0x00014400, {0xD811, 0xDC00}},
                                 {0x00014800, {0xD812, 0xDC00}},
                                 {0x00015000, {0xD814, 0xDC00}},
                                 {0x00016000, {0xD818, 0xDC00}},
                                 {0x00017FFF, {0xD81F, 0xDFFF}},
                                 {0x00018000, {0xD820, 0xDC00}},
                                 {0x00018001, {0xD820, 0xDC01}},
                                 {0x00018002, {0xD820, 0xDC02}},
                                 {0x00018004, {0xD820, 0xDC04}},
                                 {0x00018008, {0xD820, 0xDC08}},
                                 {0x00018010, {0xD820, 0xDC10}},
                                 {0x00018020, {0xD820, 0xDC20}},
                                 {0x00018040, {0xD820, 0xDC40}},
                                 {0x00018080, {0xD820, 0xDC80}},
                                 {0x00018100, {0xD820, 0xDD00}},
                                 {0x00018200, {0xD820, 0xDE00}},
                                 {0x00018400, {0xD821, 0xDC00}},
                                 {0x00018800, {0xD822, 0xDC00}},
                                 {0x00019000, {0xD824, 0xDC00}},
                                 {0x0001A000, {0xD828, 0xDC00}},
                                 {0x0001C000, {0xD830, 0xDC00}},
                                 {0x0001FFFF, {0xD83F, 0xDFFF}},
                                 {0x00020000, {0xD840, 0xDC00}},
                                 {0x00020001, {0xD840, 0xDC01}},
                                 {0x00020002, {0xD840, 0xDC02}},
                                 {0x00020004, {0xD840, 0xDC04}},
                                 {0x00020008, {0xD840, 0xDC08}},
                                 {0x00020010, {0xD840, 0xDC10}},
                                 {0x00020020, {0xD840, 0xDC20}},
                                 {0x00020040, {0xD840, 0xDC40}},
                                 {0x00020080, {0xD840, 0xDC80}},
                                 {0x00020100, {0xD840, 0xDD00}},
                                 {0x00020200, {0xD840, 0xDE00}},
                                 {0x00020400, {0xD841, 0xDC00}},
                                 {0x00020800, {0xD842, 0xDC00}},
                                 {0x00021000, {0xD844, 0xDC00}},
                                 {0x00022000, {0xD848, 0xDC00}},
                                 {0x00024000, {0xD850, 0xDC00}},
                                 {0x00028000, {0xD860, 0xDC00}},
                                 {0x0002FFFF, {0xD87F, 0xDFFF}},
                                 {0x00030000, {0xD880, 0xDC00}},
                                 {0x00030001, {0xD880, 0xDC01}},
                                 {0x00030002, {0xD880, 0xDC02}},
                                 {0x00030004, {0xD880, 0xDC04}},
                                 {0x00030008, {0xD880, 0xDC08}},
                                 {0x00030010, {0xD880, 0xDC10}},
                                 {0x00030020, {0xD880, 0xDC20}},
                                 {0x00030040, {0xD880, 0xDC40}},
                                 {0x00030080, {0xD880, 0xDC80}},
                                 {0x00030100, {0xD880, 0xDD00}},
                                 {0x00030200, {0xD880, 0xDE00}},
                                 {0x00030400, {0xD881, 0xDC00}},
                                 {0x00030800, {0xD882, 0xDC00}},
                                 {0x00031000, {0xD884, 0xDC00}},
                                 {0x00032000, {0xD888, 0xDC00}},
                                 {0x00034000, {0xD890, 0xDC00}},
                                 {0x00038000, {0xD8A0, 0xDC00}},
                                 {0x0003FFFF, {0xD8BF, 0xDFFF}},
                                 {0x00040000, {0xD8C0, 0xDC00}},
                                 {0x00040001, {0xD8C0, 0xDC01}},
                                 {0x00040002, {0xD8C0, 0xDC02}},
                                 {0x00040004, {0xD8C0, 0xDC04}},
                                 {0x00040008, {0xD8C0, 0xDC08}},
                                 {0x00040010, {0xD8C0, 0xDC10}},
                                 {0x00040020, {0xD8C0, 0xDC20}},
                                 {0x00040040, {0xD8C0, 0xDC40}},
                                 {0x00040080, {0xD8C0, 0xDC80}},
                                 {0x00040100, {0xD8C0, 0xDD00}},
                                 {0x00040200, {0xD8C0, 0xDE00}},
                                 {0x00040400, {0xD8C1, 0xDC00}},
                                 {0x00040800, {0xD8C2, 0xDC00}},
                                 {0x00041000, {0xD8C4, 0xDC00}},
                                 {0x00042000, {0xD8C8, 0xDC00}},
                                 {0x00044000, {0xD8D0, 0xDC00}},
                                 {0x00048000, {0xD8E0, 0xDC00}},
                                 {0x0004FFFF, {0xD8FF, 0xDFFF}},
                                 {0x00050000, {0xD900, 0xDC00}},
                                 {0x00050001, {0xD900, 0xDC01}},
                                 {0x00050002, {0xD900, 0xDC02}},
                                 {0x00050004, {0xD900, 0xDC04}},
                                 {0x00050008, {0xD900, 0xDC08}},
                                 {0x00050010, {0xD900, 0xDC10}},
                                 {0x00050020, {0xD900, 0xDC20}},
                                 {0x00050040, {0xD900, 0xDC40}},
                                 {0x00050080, {0xD900, 0xDC80}},
                                 {0x00050100, {0xD900, 0xDD00}},
                                 {0x00050200, {0xD900, 0xDE00}},
                                 {0x00050400, {0xD901, 0xDC00}},
                                 {0x00050800, {0xD902, 0xDC00}},
                                 {0x00051000, {0xD904, 0xDC00}},
                                 {0x00052000, {0xD908, 0xDC00}},
                                 {0x00054000, {0xD910, 0xDC00}},
                                 {0x00058000, {0xD920, 0xDC00}},
                                 {0x00060000, {0xD940, 0xDC00}},
                                 {0x00070000, {0xD980, 0xDC00}},
                                 {0x0007FFFF, {0xD9BF, 0xDFFF}},
                                 {0x00080000, {0xD9C0, 0xDC00}},
                                 {0x00080001, {0xD9C0, 0xDC01}},
                                 {0x00080002, {0xD9C0, 0xDC02}},
                                 {0x00080004, {0xD9C0, 0xDC04}},
                                 {0x00080008, {0xD9C0, 0xDC08}},
                                 {0x00080010, {0xD9C0, 0xDC10}},
                                 {0x00080020, {0xD9C0, 0xDC20}},
                                 {0x00080040, {0xD9C0, 0xDC40}},
                                 {0x00080080, {0xD9C0, 0xDC80}},
                                 {0x00080100, {0xD9C0, 0xDD00}},
                                 {0x00080200, {0xD9C0, 0xDE00}},
                                 {0x00080400, {0xD9C1, 0xDC00}},
                                 {0x00080800, {0xD9C2, 0xDC00}},
                                 {0x00081000, {0xD9C4, 0xDC00}},
                                 {0x00082000, {0xD9C8, 0xDC00}},
                                 {0x00084000, {0xD9D0, 0xDC00}},
                                 {0x00088000, {0xD9E0, 0xDC00}},
                                 {0x0008FFFF, {0xD9FF, 0xDFFF}},
                                 {0x00090000, {0xDA00, 0xDC00}},
                                 {0x00090001, {0xDA00, 0xDC01}},
                                 {0x00090002, {0xDA00, 0xDC02}},
                                 {0x00090004, {0xDA00, 0xDC04}},
                                 {0x00090008, {0xDA00, 0xDC08}},
                                 {0x00090010, {0xDA00, 0xDC10}},
                                 {0x00090020, {0xDA00, 0xDC20}},
                                 {0x00090040, {0xDA00, 0xDC40}},
                                 {0x00090080, {0xDA00, 0xDC80}},
                                 {0x00090100, {0xDA00, 0xDD00}},
                                 {0x00090200, {0xDA00, 0xDE00}},
                                 {0x00090400, {0xDA01, 0xDC00}},
                                 {0x00090800, {0xDA02, 0xDC00}},
                                 {0x00091000, {0xDA04, 0xDC00}},
                                 {0x00092000, {0xDA08, 0xDC00}},
                                 {0x00094000, {0xDA10, 0xDC00}},
                                 {0x00098000, {0xDA20, 0xDC00}},
                                 {0x000A0000, {0xDA40, 0xDC00}},
                                 {0x000B0000, {0xDA80, 0xDC00}},
                                 {0x000C0000, {0xDAC0, 0xDC00}},
                                 {0x000D0000, {0xDB00, 0xDC00}},
                                 {0x000FFFFF, {0xDBBF, 0xDFFF}},
                                 {0x0010FFFF, {0xDBFF, 0xDFFF}}

};

// Invalid UTF-8 sequences

const char *const kUtf8BadCases[] = {
    "\xC0\x80",
    "\xC1\xBF",
    "\xE0\x80\x80",
    "\xE0\x9F\xBF",
    "\xF0\x80\x80\x80",
    "\xF0\x8F\xBF\xBF",
    "\xF4\x90\x80\x80",
    "\xF7\xBF\xBF\xBF",
    "\xF8\x80\x80\x80\x80",
    "\xF8\x88\x80\x80\x80",
    "\xF8\x92\x80\x80\x80",
    "\xF8\x9F\xBF\xBF\xBF",
    "\xF8\xA0\x80\x80\x80",
    "\xF8\xA8\x80\x80\x80",
    "\xF8\xB0\x80\x80\x80",
    "\xF8\xBF\xBF\xBF\xBF",
    "\xF9\x80\x80\x80\x88",
    "\xF9\x84\x80\x80\x80",
    "\xF9\xBF\xBF\xBF\xBF",
    "\xFA\x80\x80\x80\x80",
    "\xFA\x90\x80\x80\x80",
    "\xFB\xBF\xBF\xBF\xBF",
    "\xFC\x84\x80\x80\x80\x81",
    "\xFC\x85\x80\x80\x80\x80",
    "\xFC\x86\x80\x80\x80\x80",
    "\xFC\x87\xBF\xBF\xBF\xBF",
    "\xFC\x88\xA0\x80\x80\x80",
    "\xFC\x89\x80\x80\x80\x80",
    "\xFC\x8A\x80\x80\x80\x80",
    "\xFC\x90\x80\x80\x80\x82",
    "\xFD\x80\x80\x80\x80\x80",
    "\xFD\xBF\xBF\xBF\xBF\xBF",
    "\x80",
    "\xC3",
    "\xC3\xC3\x80",
    "\xED\xA0\x80",
    "\xED\xBF\x80",
    "\xED\xBF\xBF",
    "\xED\xA0\x80\xE0\xBF\xBF",
};

// Invalid UTF-16 sequences (0-terminated)

const Utf16BadCase kUtf16BadCases[] = {
    // Leading surrogate not followed by trailing surrogate:
    {{0xD800, 0, 0}},
    {{0xD800, 0x41, 0}},
    {{0xD800, 0xfe, 0}},
    {{0xD800, 0x3bb, 0}},
    {{0xD800, 0xD800, 0}},
    {{0xD800, 0xFEFF, 0}},
    {{0xD800, 0xFFFD, 0}},
    // Trailing surrogate, not preceded by a leading one.
    {{0xDC00, 0, 0}},
    {{0xDE6D, 0xD834, 0}},
};

// Parameterized test instantiations:

INSTANTIATE_TEST_CASE_P(Ucs4TestCases, Ucs4Test,
                        ::testing::ValuesIn(kUcs4Cases));

INSTANTIATE_TEST_CASE_P(Iso88591TestCases, Ucs2Test,
                        ::testing::ValuesIn(kIso88591Cases));

INSTANTIATE_TEST_CASE_P(Ucs2TestCases, Ucs2Test,
                        ::testing::ValuesIn(kUcs2Cases));

INSTANTIATE_TEST_CASE_P(Utf16TestCases, Utf16Test,
                        ::testing::ValuesIn(kUtf16Cases));

INSTANTIATE_TEST_CASE_P(BadUtf8TestCases, BadUtf8Test,
                        ::testing::ValuesIn(kUtf8BadCases));

INSTANTIATE_TEST_CASE_P(BadUtf16TestCases, BadUtf16Test,
                        ::testing::ValuesIn(kUtf16BadCases));

INSTANTIATE_TEST_CASE_P(Iso88591TestCases, Iso88591Test,
                        ::testing::ValuesIn(kIso88591Cases));
;

}  // namespace nss_test
