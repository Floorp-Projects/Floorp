/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "ProxyUtils.h"

using namespace mozilla::toolkit::system;

TEST(OSXProxy, TestProxyBypassRules)
{
  EXPECT_TRUE(IsHostProxyEntry(NS_LITERAL_CSTRING("mozilla.org"), NS_LITERAL_CSTRING("mozilla.org")));
  EXPECT_TRUE(IsHostProxyEntry(NS_LITERAL_CSTRING("mozilla.org"),NS_LITERAL_CSTRING("*mozilla.org")));
  EXPECT_TRUE(IsHostProxyEntry(NS_LITERAL_CSTRING("mozilla.org"), NS_LITERAL_CSTRING("*.mozilla.org")));
  EXPECT_FALSE(IsHostProxyEntry(NS_LITERAL_CSTRING("notmozilla.org"), NS_LITERAL_CSTRING("*.mozilla.org")));
  EXPECT_TRUE(IsHostProxyEntry(NS_LITERAL_CSTRING("www.mozilla.org"), NS_LITERAL_CSTRING("*mozilla.org")));
  EXPECT_TRUE(IsHostProxyEntry(NS_LITERAL_CSTRING("www.mozilla.org"), NS_LITERAL_CSTRING("*.mozilla.org")));
  EXPECT_TRUE(IsHostProxyEntry(NS_LITERAL_CSTRING("www.mozilla.com"), NS_LITERAL_CSTRING("*.mozilla.*")));
}

TEST(OSXProxy, TestProxyBypassRulesIPv4)
{
  EXPECT_TRUE(IsHostProxyEntry(NS_LITERAL_CSTRING("192.168.1.1"), NS_LITERAL_CSTRING("192.168.1.*")));
  EXPECT_FALSE(IsHostProxyEntry(NS_LITERAL_CSTRING("192.168.1.1"), NS_LITERAL_CSTRING("192.168.2.*")));

  EXPECT_TRUE(IsHostProxyEntry(NS_LITERAL_CSTRING("10.1.2.3"), NS_LITERAL_CSTRING("10.0.0.0/8")));
  EXPECT_TRUE(IsHostProxyEntry(NS_LITERAL_CSTRING("192.168.192.1"), NS_LITERAL_CSTRING("192.168/16")));
  EXPECT_FALSE(IsHostProxyEntry(NS_LITERAL_CSTRING("192.168.192.1"), NS_LITERAL_CSTRING("192.168/17")));
  EXPECT_TRUE(IsHostProxyEntry(NS_LITERAL_CSTRING("192.168.192.1"), NS_LITERAL_CSTRING("192.168.128/17")));
  EXPECT_TRUE(IsHostProxyEntry(NS_LITERAL_CSTRING("192.168.1.1"), NS_LITERAL_CSTRING("192.168.1.1/32")));
}

TEST(OSXProxy, TestProxyBypassRulesIPv6)
{
  EXPECT_TRUE(IsHostProxyEntry(NS_LITERAL_CSTRING("2001:0DB8:ABCD:0012:0123:4567:89AB:CDEF"), NS_LITERAL_CSTRING("2001:db8:abcd:0012::0/64")));
  EXPECT_TRUE(IsHostProxyEntry(NS_LITERAL_CSTRING("2001:0DB8:ABCD:0012:0000:4567:89AB:CDEF"), NS_LITERAL_CSTRING("2001:db8:abcd:0012::0/80")));
  EXPECT_FALSE(IsHostProxyEntry(NS_LITERAL_CSTRING("2001:0DB8:ABCD:0012:0123:4567:89AB:CDEF"), NS_LITERAL_CSTRING("2001:db8:abcd:0012::0/80")));
  EXPECT_TRUE(IsHostProxyEntry(NS_LITERAL_CSTRING("2001:0DB8:ABCD:0012:0000:0000:89AB:CDEF"), NS_LITERAL_CSTRING("2001:db8:abcd:0012::0/96")));
  EXPECT_FALSE(IsHostProxyEntry(NS_LITERAL_CSTRING("2001:0DB8:ABCD:0012:0123:4567:89AB:CDEF"), NS_LITERAL_CSTRING("2001:db8:abcd:0012::0/96")));
}
