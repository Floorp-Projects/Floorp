/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "gtest/gtest.h"
#include "mozilla/gtest/MozAssertions.h"

#include "nsINIParser.h"

TEST(INIParser, DeleteString)
{
  nsINIParser* parser = new nsINIParser();
  nsresult rv = parser->InitFromString(
      "[sec]\r\
key1=val1\r\
key2=val2\r\
key3=val3\r\
key4=val4"_ns);
  EXPECT_NS_SUCCEEDED(rv);

  rv = parser->DeleteString("sec", "key3");
  EXPECT_NS_SUCCEEDED(rv);
  rv = parser->DeleteString("sec", "key4");
  EXPECT_NS_SUCCEEDED(rv);
  rv = parser->DeleteString("sec", "key1");
  EXPECT_NS_SUCCEEDED(rv);
  rv = parser->DeleteString("sec", "key2");
  EXPECT_NS_SUCCEEDED(rv);

  delete parser;
}

TEST(INIParser, DeleteSection)
{
  nsINIParser* parser = new nsINIParser();
  nsresult rv = parser->InitFromString(
      "[sec1]\r\
key=val\r\
\r\
[sec2]\r\
key=val\r\
[sec3]\r\
key=val\r\
[sec4]\r\
key=val"_ns);
  EXPECT_NS_SUCCEEDED(rv);

  rv = parser->DeleteSection("sec3");
  EXPECT_NS_SUCCEEDED(rv);
  rv = parser->DeleteSection("sec4");
  EXPECT_NS_SUCCEEDED(rv);
  rv = parser->DeleteSection("sec1");
  EXPECT_NS_SUCCEEDED(rv);
  rv = parser->DeleteSection("sec2");
  EXPECT_NS_SUCCEEDED(rv);

  delete parser;
}
