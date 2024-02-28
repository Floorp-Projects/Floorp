/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/SpinEventLoopUntil.h"

#include "linux_utils.h"

#define ASSERT_EQ_UNSIGNED(v, e) ASSERT_EQ((v), (uint32_t)(e))

using namespace mozilla;

class CrashReporter : public ::testing::Test {};

TEST_F(CrashReporter, ElfSoNoVersion) {
  VersionComponents version;
  bool rv = ElfFileSoVersion("libdbus1.so", &version);
  ASSERT_TRUE(rv);
  ASSERT_EQ_UNSIGNED(version.size(), 0);
}

TEST_F(CrashReporter, ElfSo6) {
  VersionComponents version;
  bool rv = ElfFileSoVersion("libm.so.6", &version);
  ASSERT_TRUE(rv);
  ASSERT_EQ_UNSIGNED(version.size(), 1);
  ASSERT_EQ_UNSIGNED(version[0], 6);
}

TEST_F(CrashReporter, ElfSoNormalShort) {
  VersionComponents version;
  bool rv = ElfFileSoVersion("libdbus1.so.1.2", &version);
  ASSERT_TRUE(rv);
  ASSERT_EQ_UNSIGNED(version.size(), 2);
  ASSERT_EQ_UNSIGNED(version[0], 1);
  ASSERT_EQ_UNSIGNED(version[1], 2);
}

TEST_F(CrashReporter, ElfSoNormalComplete) {
  VersionComponents version;
  bool rv = ElfFileSoVersion("libdbus1.so.1.2.3", &version);
  ASSERT_TRUE(rv);
  ASSERT_EQ_UNSIGNED(version.size(), 3);
  ASSERT_EQ_UNSIGNED(version[0], 1);
  ASSERT_EQ_UNSIGNED(version[1], 2);
  ASSERT_EQ_UNSIGNED(version[2], 3);
}

TEST_F(CrashReporter, ElfSoNormalPrerelease) {
  VersionComponents version;
  bool rv = ElfFileSoVersion("libdbus1.so.1.2.3.98", &version);
  ASSERT_TRUE(rv);
  ASSERT_EQ_UNSIGNED(version.size(), 4);
  ASSERT_EQ_UNSIGNED(version[0], 1);
  ASSERT_EQ_UNSIGNED(version[1], 2);
  ASSERT_EQ_UNSIGNED(version[2], 3);
  ASSERT_EQ_UNSIGNED(version[3], 98);
}

TEST_F(CrashReporter, ElfSoNormalPrereleaseToomuch) {
  VersionComponents version;
  bool rv = ElfFileSoVersion("libdbus1.so.1.2.3.98.9.2.3", &version);
  ASSERT_TRUE(rv);
  ASSERT_EQ_UNSIGNED(version.size(), 4);
  ASSERT_EQ_UNSIGNED(version[0], 1);
  ASSERT_EQ_UNSIGNED(version[1], 2);
  ASSERT_EQ_UNSIGNED(version[2], 3);
  ASSERT_EQ_UNSIGNED(version[3], 98);
}

TEST_F(CrashReporter, ElfSoBig) {
  VersionComponents version;
  bool rv = ElfFileSoVersion("libatk-1.0.so.0.25009.1", &version);
  ASSERT_TRUE(rv);
  ASSERT_EQ_UNSIGNED(version.size(), 3);
  ASSERT_EQ_UNSIGNED(version[0], 0);
  ASSERT_EQ_UNSIGNED(version[1], 25009);
  ASSERT_EQ_UNSIGNED(version[2], 1);
}

TEST_F(CrashReporter, ElfSoCairo) {
  VersionComponents version;
  bool rv = ElfFileSoVersion("libcairo.so.2.11800.3", &version);
  ASSERT_TRUE(rv);
  ASSERT_EQ_UNSIGNED(version.size(), 3);
  ASSERT_EQ_UNSIGNED(version[0], 2);
  ASSERT_EQ_UNSIGNED(version[1], 11800);
  ASSERT_EQ_UNSIGNED(version[2], 3);
}

TEST_F(CrashReporter, ElfSoMax) {
  VersionComponents version;
  bool rv = ElfFileSoVersion(
      "libcairo.so.2147483647.2147483647.2147483647.2147483647", &version);
  ASSERT_TRUE(rv);
  ASSERT_EQ_UNSIGNED(version.size(), 4);
  ASSERT_EQ_UNSIGNED(version[0], INT32_MAX);
  ASSERT_EQ_UNSIGNED(version[1], INT32_MAX);
  ASSERT_EQ_UNSIGNED(version[2], INT32_MAX);
  ASSERT_EQ_UNSIGNED(version[3], INT32_MAX);
}

TEST_F(CrashReporter, ElfSoTimestamp) {
  VersionComponents version;
  bool rv = ElfFileSoVersion("libabsl_time_zone.so.20220623.0.0", &version);
  ASSERT_TRUE(rv);
  ASSERT_EQ_UNSIGNED(version.size(), 3);
  ASSERT_EQ_UNSIGNED(version[0], 20220623);
  ASSERT_EQ_UNSIGNED(version[1], 0);
  ASSERT_EQ_UNSIGNED(version[2], 0);
}

TEST_F(CrashReporter, ElfSoChars) {
  VersionComponents version;
  bool rv = ElfFileSoVersion("libabsl_time_zone.so.1.2.3rc4", &version);
  ASSERT_TRUE(rv);
  ASSERT_EQ_UNSIGNED(version.size(), 3);
  ASSERT_EQ_UNSIGNED(version[0], 1);
  ASSERT_EQ_UNSIGNED(version[1], 2);
  ASSERT_EQ_UNSIGNED(version[2], 34);
}

TEST_F(CrashReporter, ElfSoCharsMore) {
  VersionComponents version;
  bool rv = ElfFileSoVersion("libabsl_time_zone.so.1.2.3rc4.9", &version);
  ASSERT_TRUE(rv);
  ASSERT_EQ_UNSIGNED(version.size(), 4);
  ASSERT_EQ_UNSIGNED(version[0], 1);
  ASSERT_EQ_UNSIGNED(version[1], 2);
  ASSERT_EQ_UNSIGNED(version[2], 34);
  ASSERT_EQ_UNSIGNED(version[3], 9);
}

TEST_F(CrashReporter, ElfSoCharsOnly) {
  VersionComponents version;
  bool rv = ElfFileSoVersion("libabsl_time_zone.so.final", &version);
  ASSERT_TRUE(rv);
  ASSERT_EQ_UNSIGNED(version.size(), 0);
}

TEST_F(CrashReporter, ElfSoNullVersion) {
  bool rv = ElfFileSoVersion("libabsl_time_zone.so.1", nullptr);
  ASSERT_FALSE(rv);
}
