/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "SandboxLogging.h"

#include <errno.h>

namespace mozilla {

TEST(SandboxLogging, ErrorName1)
{
  char buf[32];
  ssize_t n = GetLibcErrorName(buf, sizeof(buf), EINVAL);
  EXPECT_EQ(n, 6);
  EXPECT_STREQ(buf, "EINVAL");
}

TEST(SandboxLogging, ErrorName2)
{
  char buf[32];
  ssize_t n = GetLibcErrorName(buf, sizeof(buf), EIO);
  EXPECT_EQ(n, 3);
  EXPECT_STREQ(buf, "EIO");
}

TEST(SandboxLogging, ErrorName3)
{
  char buf[32];
  ssize_t n = GetLibcErrorName(buf, sizeof(buf), ESTALE);
  EXPECT_EQ(n, 6);
  EXPECT_STREQ(buf, "ESTALE");
}

TEST(SandboxLogging, ErrorNameFail)
{
  char buf[32];
  ssize_t n = GetLibcErrorName(buf, sizeof(buf), 4095);
  EXPECT_EQ(n, 10);
  EXPECT_STREQ(buf, "error 4095");
}

TEST(SandboxLogging, ErrorNameTrunc)
{
  char buf[] = "++++++++";
  ssize_t n = GetLibcErrorName(buf, 3, EINVAL);
  EXPECT_EQ(n, 6);
  EXPECT_STREQ(buf, "EI");
  EXPECT_STREQ(buf + 3, "+++++");
}

}  // namespace mozilla
