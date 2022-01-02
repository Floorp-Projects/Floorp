/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNSSIOLayer.h"
#include "sslproto.h"
#include "sslerr.h"

#include "gtest/gtest.h"

constexpr auto HOST = "example.org"_ns;
const int16_t PORT = 443;

class psm_TLSIntoleranceTest : public ::testing::Test {
 protected:
  nsSSLIOLayerHelpers helpers;
};

TEST_F(psm_TLSIntoleranceTest, FullFallbackProcess) {
  ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_0, helpers.mVersionFallbackLimit);

  // No adjustment made when there is no entry for the site.
  {
    SSLVersionRange range = {SSL_LIBRARY_VERSION_TLS_1_0,
                             SSL_LIBRARY_VERSION_TLS_1_2};
    helpers.adjustForTLSIntolerance(HOST, PORT, range);
    ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_0, range.min);
    ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_2, range.max);
  }

  {
    SSLVersionRange range = {SSL_LIBRARY_VERSION_TLS_1_0,
                             SSL_LIBRARY_VERSION_TLS_1_2};
    helpers.adjustForTLSIntolerance(HOST, PORT, range);
    ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_0, range.min);
    ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_2, range.max);

    ASSERT_TRUE(helpers.rememberIntolerantAtVersion(HOST, PORT, range.min,
                                                    range.max, 0));
  }

  {
    SSLVersionRange range = {SSL_LIBRARY_VERSION_TLS_1_0,
                             SSL_LIBRARY_VERSION_TLS_1_2};
    helpers.adjustForTLSIntolerance(HOST, PORT, range);
    ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_0, range.min);
    ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_1, range.max);

    ASSERT_TRUE(helpers.rememberIntolerantAtVersion(HOST, PORT, range.min,
                                                    range.max, 0));
  }

  {
    SSLVersionRange range = {SSL_LIBRARY_VERSION_TLS_1_0,
                             SSL_LIBRARY_VERSION_TLS_1_2};
    helpers.adjustForTLSIntolerance(HOST, PORT, range);
    ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_0, range.min);
    ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_0, range.max);

    ASSERT_FALSE(helpers.rememberIntolerantAtVersion(HOST, PORT, range.min,
                                                     range.max, 0));
  }

  {
    SSLVersionRange range = {SSL_LIBRARY_VERSION_TLS_1_0,
                             SSL_LIBRARY_VERSION_TLS_1_2};
    helpers.adjustForTLSIntolerance(HOST, PORT, range);
    ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_0, range.min);
    // When rememberIntolerantAtVersion returns false, it also resets the
    // intolerance information for the server.
    ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_2, range.max);
  }
}

TEST_F(psm_TLSIntoleranceTest, DisableFallbackWithHighLimit) {
  // this value disables version fallback entirely: with this value, all efforts
  // to mark an origin as version intolerant fail
  helpers.mVersionFallbackLimit = SSL_LIBRARY_VERSION_TLS_1_2;
  ASSERT_FALSE(helpers.rememberIntolerantAtVersion(
      HOST, PORT, SSL_LIBRARY_VERSION_TLS_1_0, SSL_LIBRARY_VERSION_TLS_1_2, 0));
  ASSERT_FALSE(helpers.rememberIntolerantAtVersion(
      HOST, PORT, SSL_LIBRARY_VERSION_TLS_1_0, SSL_LIBRARY_VERSION_TLS_1_1, 0));
  ASSERT_FALSE(helpers.rememberIntolerantAtVersion(
      HOST, PORT, SSL_LIBRARY_VERSION_TLS_1_0, SSL_LIBRARY_VERSION_TLS_1_0, 0));
}

TEST_F(psm_TLSIntoleranceTest, FallbackLimitBelowMin) {
  // check that we still respect the minimum version,
  // when it is higher than the fallback limit
  ASSERT_TRUE(helpers.rememberIntolerantAtVersion(
      HOST, PORT, SSL_LIBRARY_VERSION_TLS_1_1, SSL_LIBRARY_VERSION_TLS_1_2, 0));
  {
    SSLVersionRange range = {SSL_LIBRARY_VERSION_TLS_1_0,
                             SSL_LIBRARY_VERSION_TLS_1_2};
    helpers.adjustForTLSIntolerance(HOST, PORT, range);
    ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_0, range.min);
    ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_1, range.max);
  }

  ASSERT_FALSE(helpers.rememberIntolerantAtVersion(
      HOST, PORT, SSL_LIBRARY_VERSION_TLS_1_1, SSL_LIBRARY_VERSION_TLS_1_1, 0));
}

TEST_F(psm_TLSIntoleranceTest, TolerantOverridesIntolerant1) {
  ASSERT_TRUE(helpers.rememberIntolerantAtVersion(
      HOST, PORT, SSL_LIBRARY_VERSION_TLS_1_0, SSL_LIBRARY_VERSION_TLS_1_1, 0));
  helpers.rememberTolerantAtVersion(HOST, PORT, SSL_LIBRARY_VERSION_TLS_1_1);
  SSLVersionRange range = {SSL_LIBRARY_VERSION_TLS_1_0,
                           SSL_LIBRARY_VERSION_TLS_1_2};
  helpers.adjustForTLSIntolerance(HOST, PORT, range);
  ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_0, range.min);
  ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_1, range.max);
}

TEST_F(psm_TLSIntoleranceTest, TolerantOverridesIntolerant2) {
  ASSERT_TRUE(helpers.rememberIntolerantAtVersion(
      HOST, PORT, SSL_LIBRARY_VERSION_TLS_1_0, SSL_LIBRARY_VERSION_TLS_1_1, 0));
  helpers.rememberTolerantAtVersion(HOST, PORT, SSL_LIBRARY_VERSION_TLS_1_2);
  SSLVersionRange range = {SSL_LIBRARY_VERSION_TLS_1_0,
                           SSL_LIBRARY_VERSION_TLS_1_2};
  helpers.adjustForTLSIntolerance(HOST, PORT, range);
  ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_0, range.min);
  ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_2, range.max);
}

TEST_F(psm_TLSIntoleranceTest, IntolerantDoesNotOverrideTolerant) {
  // No adjustment made when there is no entry for the site.
  helpers.rememberTolerantAtVersion(HOST, PORT, SSL_LIBRARY_VERSION_TLS_1_1);
  // false because we reached the floor set by rememberTolerantAtVersion.
  ASSERT_FALSE(helpers.rememberIntolerantAtVersion(
      HOST, PORT, SSL_LIBRARY_VERSION_TLS_1_0, SSL_LIBRARY_VERSION_TLS_1_1, 0));
  SSLVersionRange range = {SSL_LIBRARY_VERSION_TLS_1_0,
                           SSL_LIBRARY_VERSION_TLS_1_2};
  helpers.adjustForTLSIntolerance(HOST, PORT, range);
  ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_0, range.min);
  ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_2, range.max);
}

TEST_F(psm_TLSIntoleranceTest, PortIsRelevant) {
  helpers.rememberTolerantAtVersion(HOST, 1, SSL_LIBRARY_VERSION_TLS_1_2);
  ASSERT_FALSE(helpers.rememberIntolerantAtVersion(
      HOST, 1, SSL_LIBRARY_VERSION_TLS_1_0, SSL_LIBRARY_VERSION_TLS_1_2, 0));
  ASSERT_TRUE(helpers.rememberIntolerantAtVersion(
      HOST, 2, SSL_LIBRARY_VERSION_TLS_1_0, SSL_LIBRARY_VERSION_TLS_1_2, 0));

  {
    SSLVersionRange range = {SSL_LIBRARY_VERSION_TLS_1_0,
                             SSL_LIBRARY_VERSION_TLS_1_2};
    helpers.adjustForTLSIntolerance(HOST, 1, range);
    ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_2, range.max);
  }

  {
    SSLVersionRange range = {SSL_LIBRARY_VERSION_TLS_1_0,
                             SSL_LIBRARY_VERSION_TLS_1_2};
    helpers.adjustForTLSIntolerance(HOST, 2, range);
    ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_1, range.max);
  }
}

TEST_F(psm_TLSIntoleranceTest, IntoleranceReasonInitial) {
  ASSERT_EQ(0, helpers.getIntoleranceReason(HOST, 1));

  helpers.rememberTolerantAtVersion(HOST, 2, SSL_LIBRARY_VERSION_TLS_1_2);
  ASSERT_EQ(0, helpers.getIntoleranceReason(HOST, 2));
}

TEST_F(psm_TLSIntoleranceTest, IntoleranceReasonStored) {
  helpers.rememberIntolerantAtVersion(HOST, 1, SSL_LIBRARY_VERSION_TLS_1_0,
                                      SSL_LIBRARY_VERSION_TLS_1_2,
                                      SSL_ERROR_BAD_SERVER);
  ASSERT_EQ(SSL_ERROR_BAD_SERVER, helpers.getIntoleranceReason(HOST, 1));

  helpers.rememberIntolerantAtVersion(HOST, 1, SSL_LIBRARY_VERSION_TLS_1_0,
                                      SSL_LIBRARY_VERSION_TLS_1_1,
                                      SSL_ERROR_BAD_MAC_READ);
  ASSERT_EQ(SSL_ERROR_BAD_MAC_READ, helpers.getIntoleranceReason(HOST, 1));
}

TEST_F(psm_TLSIntoleranceTest, IntoleranceReasonCleared) {
  ASSERT_EQ(0, helpers.getIntoleranceReason(HOST, 1));

  helpers.rememberIntolerantAtVersion(HOST, 1, SSL_LIBRARY_VERSION_TLS_1_0,
                                      SSL_LIBRARY_VERSION_TLS_1_2,
                                      SSL_ERROR_HANDSHAKE_UNEXPECTED_ALERT);
  ASSERT_EQ(SSL_ERROR_HANDSHAKE_UNEXPECTED_ALERT,
            helpers.getIntoleranceReason(HOST, 1));

  helpers.rememberTolerantAtVersion(HOST, 1, SSL_LIBRARY_VERSION_TLS_1_2);
  ASSERT_EQ(0, helpers.getIntoleranceReason(HOST, 1));
}

TEST_F(psm_TLSIntoleranceTest, TLSForgetIntolerance) {
  {
    ASSERT_TRUE(helpers.rememberIntolerantAtVersion(
        HOST, PORT, SSL_LIBRARY_VERSION_TLS_1_0, SSL_LIBRARY_VERSION_TLS_1_2,
        0));

    SSLVersionRange range = {SSL_LIBRARY_VERSION_TLS_1_0,
                             SSL_LIBRARY_VERSION_TLS_1_2};
    helpers.adjustForTLSIntolerance(HOST, PORT, range);
    ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_0, range.min);
    ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_1, range.max);
  }

  {
    helpers.forgetIntolerance(HOST, PORT);

    SSLVersionRange range = {SSL_LIBRARY_VERSION_TLS_1_0,
                             SSL_LIBRARY_VERSION_TLS_1_2};
    helpers.adjustForTLSIntolerance(HOST, PORT, range);
    ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_0, range.min);
    ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_2, range.max);
  }
}

TEST_F(psm_TLSIntoleranceTest, TLSDontForgetTolerance) {
  {
    helpers.rememberTolerantAtVersion(HOST, PORT, SSL_LIBRARY_VERSION_TLS_1_1);

    SSLVersionRange range = {SSL_LIBRARY_VERSION_TLS_1_0,
                             SSL_LIBRARY_VERSION_TLS_1_2};
    helpers.adjustForTLSIntolerance(HOST, PORT, range);
    ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_0, range.min);
    ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_2, range.max);
  }

  {
    ASSERT_TRUE(helpers.rememberIntolerantAtVersion(
        HOST, PORT, SSL_LIBRARY_VERSION_TLS_1_0, SSL_LIBRARY_VERSION_TLS_1_2,
        0));

    SSLVersionRange range = {SSL_LIBRARY_VERSION_TLS_1_0,
                             SSL_LIBRARY_VERSION_TLS_1_2};
    helpers.adjustForTLSIntolerance(HOST, PORT, range);
    ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_0, range.min);
    ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_1, range.max);
  }

  {
    helpers.forgetIntolerance(HOST, PORT);

    SSLVersionRange range = {SSL_LIBRARY_VERSION_TLS_1_0,
                             SSL_LIBRARY_VERSION_TLS_1_2};
    helpers.adjustForTLSIntolerance(HOST, PORT, range);
    ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_0, range.min);
    ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_2, range.max);
  }
}

TEST_F(psm_TLSIntoleranceTest, TLSPerSiteFallbackLimit) {
  constexpr auto example_com = "example.com"_ns;
  constexpr auto example_net = "example.net"_ns;
  constexpr auto example_org = "example.org"_ns;

  helpers.mVersionFallbackLimit = SSL_LIBRARY_VERSION_TLS_1_0;

  ASSERT_FALSE(
      helpers.fallbackLimitReached(example_com, SSL_LIBRARY_VERSION_TLS_1_2));
  ASSERT_FALSE(
      helpers.fallbackLimitReached(example_com, SSL_LIBRARY_VERSION_TLS_1_1));
  ASSERT_TRUE(
      helpers.fallbackLimitReached(example_com, SSL_LIBRARY_VERSION_TLS_1_0));
  ASSERT_FALSE(
      helpers.fallbackLimitReached(example_net, SSL_LIBRARY_VERSION_TLS_1_2));
  ASSERT_FALSE(
      helpers.fallbackLimitReached(example_net, SSL_LIBRARY_VERSION_TLS_1_1));
  ASSERT_TRUE(
      helpers.fallbackLimitReached(example_net, SSL_LIBRARY_VERSION_TLS_1_0));
  ASSERT_FALSE(
      helpers.fallbackLimitReached(example_org, SSL_LIBRARY_VERSION_TLS_1_2));
  ASSERT_FALSE(
      helpers.fallbackLimitReached(example_org, SSL_LIBRARY_VERSION_TLS_1_1));
  ASSERT_TRUE(
      helpers.fallbackLimitReached(example_org, SSL_LIBRARY_VERSION_TLS_1_0));

  helpers.mVersionFallbackLimit = SSL_LIBRARY_VERSION_TLS_1_2;

  ASSERT_TRUE(
      helpers.fallbackLimitReached(example_com, SSL_LIBRARY_VERSION_TLS_1_2));
  ASSERT_TRUE(
      helpers.fallbackLimitReached(example_com, SSL_LIBRARY_VERSION_TLS_1_1));
  ASSERT_TRUE(
      helpers.fallbackLimitReached(example_com, SSL_LIBRARY_VERSION_TLS_1_0));
  ASSERT_TRUE(
      helpers.fallbackLimitReached(example_net, SSL_LIBRARY_VERSION_TLS_1_2));
  ASSERT_TRUE(
      helpers.fallbackLimitReached(example_net, SSL_LIBRARY_VERSION_TLS_1_1));
  ASSERT_TRUE(
      helpers.fallbackLimitReached(example_net, SSL_LIBRARY_VERSION_TLS_1_0));
  ASSERT_TRUE(
      helpers.fallbackLimitReached(example_org, SSL_LIBRARY_VERSION_TLS_1_2));
  ASSERT_TRUE(
      helpers.fallbackLimitReached(example_org, SSL_LIBRARY_VERSION_TLS_1_1));
  ASSERT_TRUE(
      helpers.fallbackLimitReached(example_org, SSL_LIBRARY_VERSION_TLS_1_0));

  helpers.setInsecureFallbackSites(example_com);

  ASSERT_FALSE(
      helpers.fallbackLimitReached(example_com, SSL_LIBRARY_VERSION_TLS_1_2));
  ASSERT_FALSE(
      helpers.fallbackLimitReached(example_com, SSL_LIBRARY_VERSION_TLS_1_1));
  ASSERT_TRUE(
      helpers.fallbackLimitReached(example_com, SSL_LIBRARY_VERSION_TLS_1_0));
  ASSERT_TRUE(
      helpers.fallbackLimitReached(example_net, SSL_LIBRARY_VERSION_TLS_1_2));
  ASSERT_TRUE(
      helpers.fallbackLimitReached(example_net, SSL_LIBRARY_VERSION_TLS_1_1));
  ASSERT_TRUE(
      helpers.fallbackLimitReached(example_net, SSL_LIBRARY_VERSION_TLS_1_0));
  ASSERT_TRUE(
      helpers.fallbackLimitReached(example_org, SSL_LIBRARY_VERSION_TLS_1_2));
  ASSERT_TRUE(
      helpers.fallbackLimitReached(example_org, SSL_LIBRARY_VERSION_TLS_1_1));
  ASSERT_TRUE(
      helpers.fallbackLimitReached(example_org, SSL_LIBRARY_VERSION_TLS_1_0));

  helpers.setInsecureFallbackSites("example.com,example.net"_ns);

  ASSERT_FALSE(
      helpers.fallbackLimitReached(example_com, SSL_LIBRARY_VERSION_TLS_1_2));
  ASSERT_FALSE(
      helpers.fallbackLimitReached(example_com, SSL_LIBRARY_VERSION_TLS_1_1));
  ASSERT_TRUE(
      helpers.fallbackLimitReached(example_com, SSL_LIBRARY_VERSION_TLS_1_0));
  ASSERT_FALSE(
      helpers.fallbackLimitReached(example_net, SSL_LIBRARY_VERSION_TLS_1_2));
  ASSERT_FALSE(
      helpers.fallbackLimitReached(example_net, SSL_LIBRARY_VERSION_TLS_1_1));
  ASSERT_TRUE(
      helpers.fallbackLimitReached(example_net, SSL_LIBRARY_VERSION_TLS_1_0));
  ASSERT_TRUE(
      helpers.fallbackLimitReached(example_org, SSL_LIBRARY_VERSION_TLS_1_2));
  ASSERT_TRUE(
      helpers.fallbackLimitReached(example_org, SSL_LIBRARY_VERSION_TLS_1_1));
  ASSERT_TRUE(
      helpers.fallbackLimitReached(example_org, SSL_LIBRARY_VERSION_TLS_1_0));

  helpers.setInsecureFallbackSites(example_net);

  ASSERT_TRUE(
      helpers.fallbackLimitReached(example_com, SSL_LIBRARY_VERSION_TLS_1_2));
  ASSERT_TRUE(
      helpers.fallbackLimitReached(example_com, SSL_LIBRARY_VERSION_TLS_1_1));
  ASSERT_TRUE(
      helpers.fallbackLimitReached(example_com, SSL_LIBRARY_VERSION_TLS_1_0));
  ASSERT_FALSE(
      helpers.fallbackLimitReached(example_net, SSL_LIBRARY_VERSION_TLS_1_2));
  ASSERT_FALSE(
      helpers.fallbackLimitReached(example_net, SSL_LIBRARY_VERSION_TLS_1_1));
  ASSERT_TRUE(
      helpers.fallbackLimitReached(example_net, SSL_LIBRARY_VERSION_TLS_1_0));
  ASSERT_TRUE(
      helpers.fallbackLimitReached(example_org, SSL_LIBRARY_VERSION_TLS_1_2));
  ASSERT_TRUE(
      helpers.fallbackLimitReached(example_org, SSL_LIBRARY_VERSION_TLS_1_1));
  ASSERT_TRUE(
      helpers.fallbackLimitReached(example_org, SSL_LIBRARY_VERSION_TLS_1_0));

  helpers.setInsecureFallbackSites(""_ns);

  ASSERT_TRUE(
      helpers.fallbackLimitReached(example_com, SSL_LIBRARY_VERSION_TLS_1_2));
  ASSERT_TRUE(
      helpers.fallbackLimitReached(example_com, SSL_LIBRARY_VERSION_TLS_1_1));
  ASSERT_TRUE(
      helpers.fallbackLimitReached(example_com, SSL_LIBRARY_VERSION_TLS_1_0));
  ASSERT_TRUE(
      helpers.fallbackLimitReached(example_net, SSL_LIBRARY_VERSION_TLS_1_2));
  ASSERT_TRUE(
      helpers.fallbackLimitReached(example_net, SSL_LIBRARY_VERSION_TLS_1_1));
  ASSERT_TRUE(
      helpers.fallbackLimitReached(example_net, SSL_LIBRARY_VERSION_TLS_1_0));
  ASSERT_TRUE(
      helpers.fallbackLimitReached(example_org, SSL_LIBRARY_VERSION_TLS_1_2));
  ASSERT_TRUE(
      helpers.fallbackLimitReached(example_org, SSL_LIBRARY_VERSION_TLS_1_1));
  ASSERT_TRUE(
      helpers.fallbackLimitReached(example_org, SSL_LIBRARY_VERSION_TLS_1_0));
}
