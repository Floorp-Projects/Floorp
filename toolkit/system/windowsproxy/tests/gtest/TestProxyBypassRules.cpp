/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "ProxyUtils.h"

using namespace mozilla::toolkit::system;

TEST(WindowsProxy, TestProxyBypassRules)
{
  EXPECT_TRUE(IsHostProxyEntry("mozilla.org"_ns, "mozilla.org"_ns));
  EXPECT_TRUE(IsHostProxyEntry("mozilla.org"_ns, "*mozilla.org"_ns));
  EXPECT_TRUE(IsHostProxyEntry("mozilla.org"_ns, "*.mozilla.org"_ns));
  EXPECT_FALSE(IsHostProxyEntry("notmozilla.org"_ns, "*.mozilla.org"_ns));
  EXPECT_TRUE(IsHostProxyEntry("www.mozilla.org"_ns, "*mozilla.org"_ns));
  EXPECT_TRUE(IsHostProxyEntry("www.mozilla.org"_ns, "*.mozilla.org"_ns));
  EXPECT_TRUE(IsHostProxyEntry("www.mozilla.com"_ns, "*.mozilla.*"_ns));
}

TEST(WindowsProxy, TestProxyBypassRulesIPv4)
{
  EXPECT_TRUE(IsHostProxyEntry("192.168.1.1"_ns, "192.168.1.*"_ns));
  EXPECT_FALSE(IsHostProxyEntry("192.168.1.1"_ns, "192.168.2.*"_ns));

  EXPECT_TRUE(IsHostProxyEntry("10.1.2.3"_ns, "10.0.0.0/8"_ns));
  EXPECT_TRUE(IsHostProxyEntry("192.168.192.1"_ns, "192.168/16"_ns));
  EXPECT_FALSE(IsHostProxyEntry("192.168.192.1"_ns, "192.168/17"_ns));
  EXPECT_TRUE(IsHostProxyEntry("192.168.192.1"_ns, "192.168.128/17"_ns));
  EXPECT_TRUE(IsHostProxyEntry("192.168.1.1"_ns, "192.168.1.1/32"_ns));
}

TEST(WindowsProxy, TestProxyBypassRulesIPv6)
{
  EXPECT_TRUE(IsHostProxyEntry("2001:0DB8:ABCD:0012:0123:4567:89AB:CDEF"_ns,
                               "2001:db8:abcd:0012::0/64"_ns));
  EXPECT_TRUE(IsHostProxyEntry("2001:0DB8:ABCD:0012:0000:4567:89AB:CDEF"_ns,
                               "2001:db8:abcd:0012::0/80"_ns));
  EXPECT_FALSE(IsHostProxyEntry("2001:0DB8:ABCD:0012:0123:4567:89AB:CDEF"_ns,
                                "2001:db8:abcd:0012::0/80"_ns));
  EXPECT_TRUE(IsHostProxyEntry("2001:0DB8:ABCD:0012:0000:0000:89AB:CDEF"_ns,
                               "2001:db8:abcd:0012::0/96"_ns));
  EXPECT_FALSE(IsHostProxyEntry("2001:0DB8:ABCD:0012:0123:4567:89AB:CDEF"_ns,
                                "2001:db8:abcd:0012::0/96"_ns));
}
