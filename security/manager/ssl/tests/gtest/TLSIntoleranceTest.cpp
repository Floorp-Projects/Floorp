/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNSSIOLayer.h"
#include "sslproto.h"
#include "sslerr.h"

#include "gtest/gtest.h"

NS_NAMED_LITERAL_CSTRING(HOST, "example.org");
const int16_t PORT = 443;

class TLSIntoleranceTest : public ::testing::Test
{
protected:
  nsSSLIOLayerHelpers helpers;
};

TEST_F(TLSIntoleranceTest, Test_Full_Fallback_Process)
{
  helpers.mVersionFallbackLimit = SSL_LIBRARY_VERSION_3_0;

  // No adjustment made when there is no entry for the site.
  {
    SSLVersionRange range = { SSL_LIBRARY_VERSION_3_0,
                              SSL_LIBRARY_VERSION_TLS_1_2 };
    StrongCipherStatus strongCipherStatus = StrongCipherStatusUnknown;
    helpers.adjustForTLSIntolerance(HOST, PORT, range, strongCipherStatus);
    ASSERT_EQ(SSL_LIBRARY_VERSION_3_0, range.min);
    ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_2, range.max);
    ASSERT_EQ(StrongCipherStatusUnknown, strongCipherStatus);

    ASSERT_TRUE(helpers.rememberStrongCiphersFailed(HOST, PORT));
  }

  {
    SSLVersionRange range = { SSL_LIBRARY_VERSION_3_0,
                              SSL_LIBRARY_VERSION_TLS_1_2 };
    StrongCipherStatus strongCipherStatus = StrongCipherStatusUnknown;
    helpers.adjustForTLSIntolerance(HOST, PORT, range, strongCipherStatus);
    ASSERT_EQ(SSL_LIBRARY_VERSION_3_0, range.min);
    ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_2, range.max);
    ASSERT_EQ(StrongCiphersFailed, strongCipherStatus);

    ASSERT_FALSE(helpers.rememberStrongCiphersFailed(HOST, PORT));
    ASSERT_TRUE(helpers.rememberIntolerantAtVersion(HOST, PORT,
                                                    range.min, range.max, 0));
  }

  {
    SSLVersionRange range = { SSL_LIBRARY_VERSION_3_0,
                              SSL_LIBRARY_VERSION_TLS_1_2 };
    StrongCipherStatus strongCipherStatus = StrongCipherStatusUnknown;
    helpers.adjustForTLSIntolerance(HOST, PORT, range, strongCipherStatus);
    ASSERT_EQ(SSL_LIBRARY_VERSION_3_0, range.min);
    ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_1, range.max);
    ASSERT_EQ(StrongCiphersFailed, strongCipherStatus);

    ASSERT_FALSE(helpers.rememberStrongCiphersFailed(HOST, PORT));
    ASSERT_TRUE(helpers.rememberIntolerantAtVersion(HOST, PORT,
                                                    range.min, range.max, 0));
  }

  {
    SSLVersionRange range = { SSL_LIBRARY_VERSION_3_0,
                              SSL_LIBRARY_VERSION_TLS_1_2 };
    StrongCipherStatus strongCipherStatus = StrongCipherStatusUnknown;
    helpers.adjustForTLSIntolerance(HOST, PORT, range, strongCipherStatus);
    ASSERT_EQ(SSL_LIBRARY_VERSION_3_0, range.min);
    ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_0, range.max);
    ASSERT_EQ(StrongCiphersFailed, strongCipherStatus);

    ASSERT_FALSE(helpers.rememberStrongCiphersFailed(HOST, PORT));
    ASSERT_TRUE(helpers.rememberIntolerantAtVersion(HOST, PORT,
                                                    range.min, range.max, 0));
  }

  {
    SSLVersionRange range = { SSL_LIBRARY_VERSION_3_0,
                              SSL_LIBRARY_VERSION_TLS_1_2 };

    StrongCipherStatus strongCipherStatus = StrongCipherStatusUnknown;
    helpers.adjustForTLSIntolerance(HOST, PORT, range, strongCipherStatus);
    ASSERT_EQ(SSL_LIBRARY_VERSION_3_0, range.min);
    ASSERT_EQ(SSL_LIBRARY_VERSION_3_0, range.max);
    ASSERT_EQ(StrongCiphersFailed, strongCipherStatus);

    ASSERT_FALSE(helpers.rememberStrongCiphersFailed(HOST, PORT));
    // false because we reached the floor set by range.min
    ASSERT_FALSE(helpers.rememberIntolerantAtVersion(HOST, PORT,
                                                     range.min, range.max, 0));
  }

  {
    SSLVersionRange range = { SSL_LIBRARY_VERSION_3_0,
                              SSL_LIBRARY_VERSION_TLS_1_2 };
    StrongCipherStatus strongCipherStatus = StrongCipherStatusUnknown;
    helpers.adjustForTLSIntolerance(HOST, PORT, range, strongCipherStatus);
    ASSERT_EQ(SSL_LIBRARY_VERSION_3_0, range.min);
    // When rememberIntolerantAtVersion returns false, it also resets the
    // intolerance information for the server.
    ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_2, range.max);
    ASSERT_EQ(StrongCipherStatusUnknown, strongCipherStatus);
  }
}

TEST_F(TLSIntoleranceTest, Test_Disable_Fallback_With_High_Limit)
{
  // this value disables version fallback entirely: with this value, all efforts
  // to mark an origin as version intolerant fail
  helpers.mVersionFallbackLimit = SSL_LIBRARY_VERSION_TLS_1_2;
  ASSERT_FALSE(helpers.rememberIntolerantAtVersion(HOST, PORT,
                                                   SSL_LIBRARY_VERSION_3_0,
                                                   SSL_LIBRARY_VERSION_TLS_1_2,
                                                   0));
  ASSERT_FALSE(helpers.rememberIntolerantAtVersion(HOST, PORT,
                                                   SSL_LIBRARY_VERSION_3_0,
                                                   SSL_LIBRARY_VERSION_TLS_1_1,
                                                   0));
  ASSERT_FALSE(helpers.rememberIntolerantAtVersion(HOST, PORT,
                                                   SSL_LIBRARY_VERSION_3_0,
                                                   SSL_LIBRARY_VERSION_TLS_1_0,
                                                   0));
}

TEST_F(TLSIntoleranceTest, Test_Fallback_Limit_Default)
{
  // the default limit prevents SSL 3.0 fallback
  ASSERT_EQ(helpers.mVersionFallbackLimit, SSL_LIBRARY_VERSION_TLS_1_0);
  ASSERT_TRUE(helpers.rememberIntolerantAtVersion(HOST, PORT,
                                                  SSL_LIBRARY_VERSION_3_0,
                                                  SSL_LIBRARY_VERSION_TLS_1_1,
                                                  0));
  ASSERT_FALSE(helpers.rememberIntolerantAtVersion(HOST, PORT,
                                                   SSL_LIBRARY_VERSION_3_0,
                                                   SSL_LIBRARY_VERSION_TLS_1_0,
                                                   0));
}

TEST_F(TLSIntoleranceTest, Test_Fallback_Limit_Below_Min)
{
  // check that we still respect the minimum version,
  // when it is higher than the fallback limit
  ASSERT_TRUE(helpers.rememberIntolerantAtVersion(HOST, PORT,
                                                  SSL_LIBRARY_VERSION_TLS_1_1,
                                                  SSL_LIBRARY_VERSION_TLS_1_2,
                                                  0));
  {
    SSLVersionRange range = { SSL_LIBRARY_VERSION_3_0,
                              SSL_LIBRARY_VERSION_TLS_1_2 };
    StrongCipherStatus strongCipherStatus = StrongCipherStatusUnknown;
    helpers.adjustForTLSIntolerance(HOST, PORT, range, strongCipherStatus);
    ASSERT_EQ(SSL_LIBRARY_VERSION_3_0, range.min);
    ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_1, range.max);
    ASSERT_EQ(StrongCipherStatusUnknown, strongCipherStatus);
  }

  ASSERT_FALSE(helpers.rememberIntolerantAtVersion(HOST, PORT,
                                                   SSL_LIBRARY_VERSION_TLS_1_1,
                                                   SSL_LIBRARY_VERSION_TLS_1_1,
                                                   0));
}

TEST_F(TLSIntoleranceTest, Test_Tolerant_Overrides_Intolerant_1)
{
  ASSERT_TRUE(helpers.rememberIntolerantAtVersion(HOST, PORT,
                                                  SSL_LIBRARY_VERSION_3_0,
                                                  SSL_LIBRARY_VERSION_TLS_1_1,
                                                  0));
  helpers.rememberTolerantAtVersion(HOST, PORT, SSL_LIBRARY_VERSION_TLS_1_1);
  SSLVersionRange range = { SSL_LIBRARY_VERSION_3_0,
                            SSL_LIBRARY_VERSION_TLS_1_2 };
  StrongCipherStatus strongCipherStatus = StrongCipherStatusUnknown;
  helpers.adjustForTLSIntolerance(HOST, PORT, range, strongCipherStatus);
  ASSERT_EQ(SSL_LIBRARY_VERSION_3_0, range.min);
  ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_1, range.max);
  ASSERT_EQ(StrongCiphersWorked, strongCipherStatus);
}

TEST_F(TLSIntoleranceTest, Test_Tolerant_Overrides_Intolerant_2)
{
  ASSERT_TRUE(helpers.rememberIntolerantAtVersion(HOST, PORT,
                                                  SSL_LIBRARY_VERSION_3_0,
                                                  SSL_LIBRARY_VERSION_TLS_1_1,
                                                  0));
  helpers.rememberTolerantAtVersion(HOST, PORT, SSL_LIBRARY_VERSION_TLS_1_2);
  SSLVersionRange range = { SSL_LIBRARY_VERSION_3_0,
                            SSL_LIBRARY_VERSION_TLS_1_2 };
  StrongCipherStatus strongCipherStatus = StrongCipherStatusUnknown;
  helpers.adjustForTLSIntolerance(HOST, PORT, range, strongCipherStatus);
  ASSERT_EQ(SSL_LIBRARY_VERSION_3_0, range.min);
  ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_2, range.max);
  ASSERT_EQ(StrongCiphersWorked, strongCipherStatus);
}

TEST_F(TLSIntoleranceTest, Test_Intolerant_Does_Not_Override_Tolerant)
{
  // No adjustment made when there is no entry for the site.
  helpers.rememberTolerantAtVersion(HOST, PORT, SSL_LIBRARY_VERSION_TLS_1_1);
  // false because we reached the floor set by rememberTolerantAtVersion.
  ASSERT_FALSE(helpers.rememberIntolerantAtVersion(HOST, PORT,
                                                   SSL_LIBRARY_VERSION_3_0,
                                                   SSL_LIBRARY_VERSION_TLS_1_1,
                                                   0));
  SSLVersionRange range = { SSL_LIBRARY_VERSION_3_0,
                            SSL_LIBRARY_VERSION_TLS_1_2 };
  StrongCipherStatus strongCipherStatus = StrongCipherStatusUnknown;
  helpers.adjustForTLSIntolerance(HOST, PORT, range, strongCipherStatus);
  ASSERT_EQ(SSL_LIBRARY_VERSION_3_0, range.min);
  ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_2, range.max);
  ASSERT_EQ(StrongCiphersWorked, strongCipherStatus);
}

TEST_F(TLSIntoleranceTest, Test_Port_Is_Relevant)
{
  helpers.rememberTolerantAtVersion(HOST, 1, SSL_LIBRARY_VERSION_TLS_1_2);
  ASSERT_FALSE(helpers.rememberIntolerantAtVersion(HOST, 1,
                                                   SSL_LIBRARY_VERSION_3_0,
                                                   SSL_LIBRARY_VERSION_TLS_1_2,
                                                   0));
  ASSERT_TRUE(helpers.rememberIntolerantAtVersion(HOST, 2,
                                                  SSL_LIBRARY_VERSION_3_0,
                                                  SSL_LIBRARY_VERSION_TLS_1_2,
                                                  0));

  {
    SSLVersionRange range = { SSL_LIBRARY_VERSION_3_0,
                              SSL_LIBRARY_VERSION_TLS_1_2 };
    StrongCipherStatus strongCipherStatus = StrongCipherStatusUnknown;
    helpers.adjustForTLSIntolerance(HOST, 1, range, strongCipherStatus);
    ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_2, range.max);
  }

  {
    SSLVersionRange range = { SSL_LIBRARY_VERSION_3_0,
                              SSL_LIBRARY_VERSION_TLS_1_2 };
    StrongCipherStatus strongCipherStatus = StrongCipherStatusUnknown;
    helpers.adjustForTLSIntolerance(HOST, 2, range, strongCipherStatus);
    ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_1, range.max);
  }
}

TEST_F(TLSIntoleranceTest, Test_Intolerance_Reason_Initial)
{
  ASSERT_EQ(0, helpers.getIntoleranceReason(HOST, 1));

  helpers.rememberTolerantAtVersion(HOST, 2, SSL_LIBRARY_VERSION_TLS_1_2);
  ASSERT_EQ(0, helpers.getIntoleranceReason(HOST, 2));
}

TEST_F(TLSIntoleranceTest, Test_Intolerance_Reason_Stored)
{
  helpers.rememberIntolerantAtVersion(HOST, 1, SSL_LIBRARY_VERSION_3_0,
                                      SSL_LIBRARY_VERSION_TLS_1_2,
                                      SSL_ERROR_BAD_SERVER);
  ASSERT_EQ(SSL_ERROR_BAD_SERVER, helpers.getIntoleranceReason(HOST, 1));

  helpers.rememberIntolerantAtVersion(HOST, 1, SSL_LIBRARY_VERSION_3_0,
                                      SSL_LIBRARY_VERSION_TLS_1_1,
                                      SSL_ERROR_BAD_MAC_READ);
  ASSERT_EQ(SSL_ERROR_BAD_MAC_READ, helpers.getIntoleranceReason(HOST, 1));
}

TEST_F(TLSIntoleranceTest, Test_Intolerance_Reason_Cleared)
{
  ASSERT_EQ(0, helpers.getIntoleranceReason(HOST, 1));

  helpers.rememberIntolerantAtVersion(HOST, 1, SSL_LIBRARY_VERSION_3_0,
                                      SSL_LIBRARY_VERSION_TLS_1_2,
                                      SSL_ERROR_HANDSHAKE_UNEXPECTED_ALERT);
  ASSERT_EQ(SSL_ERROR_HANDSHAKE_UNEXPECTED_ALERT,
            helpers.getIntoleranceReason(HOST, 1));

  helpers.rememberTolerantAtVersion(HOST, 1, SSL_LIBRARY_VERSION_TLS_1_2);
  ASSERT_EQ(0, helpers.getIntoleranceReason(HOST, 1));
}

TEST_F(TLSIntoleranceTest, Test_Strong_Ciphers_Failed)
{
  helpers.mVersionFallbackLimit = SSL_LIBRARY_VERSION_TLS_1_1;

  ASSERT_TRUE(helpers.rememberStrongCiphersFailed(HOST, PORT));

  {
    SSLVersionRange range = { SSL_LIBRARY_VERSION_3_0,
                              SSL_LIBRARY_VERSION_TLS_1_2 };
    StrongCipherStatus strongCipherStatus = StrongCipherStatusUnknown;
    helpers.adjustForTLSIntolerance(HOST, PORT, range, strongCipherStatus);
    ASSERT_EQ(SSL_LIBRARY_VERSION_3_0, range.min);
    ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_2, range.max);
    ASSERT_EQ(StrongCiphersFailed, strongCipherStatus);

    ASSERT_TRUE(helpers.rememberIntolerantAtVersion(HOST, PORT,
                                                    range.min, range.max, 0));
  }

  {
    SSLVersionRange range = { SSL_LIBRARY_VERSION_3_0,
                              SSL_LIBRARY_VERSION_TLS_1_2 };
    StrongCipherStatus strongCipherStatus = StrongCipherStatusUnknown;
    helpers.adjustForTLSIntolerance(HOST, PORT, range, strongCipherStatus);
    ASSERT_EQ(SSL_LIBRARY_VERSION_3_0, range.min);
    ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_1, range.max);
    ASSERT_EQ(StrongCiphersFailed, strongCipherStatus);

    ASSERT_FALSE(helpers.rememberIntolerantAtVersion(HOST, PORT,
                                                     range.min, range.max, 0));
  }

  {
    SSLVersionRange range = { SSL_LIBRARY_VERSION_3_0,
                              SSL_LIBRARY_VERSION_TLS_1_2 };
    StrongCipherStatus strongCipherStatus = StrongCipherStatusUnknown;
    helpers.adjustForTLSIntolerance(HOST, PORT, range, strongCipherStatus);
    ASSERT_EQ(SSL_LIBRARY_VERSION_3_0, range.min);
    // When rememberIntolerantAtVersion returns false, it also resets the
    // intolerance information for the server.
    ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_2, range.max);
    ASSERT_EQ(StrongCipherStatusUnknown, strongCipherStatus);
  }
}

TEST_F(TLSIntoleranceTest, Test_Strong_Ciphers_Failed_At_1_1)
{
  helpers.mVersionFallbackLimit = SSL_LIBRARY_VERSION_3_0;

  // No adjustment made when there is no entry for the site.
  {
    SSLVersionRange range = { SSL_LIBRARY_VERSION_3_0,
                              SSL_LIBRARY_VERSION_TLS_1_2 };
    StrongCipherStatus strongCipherStatus = StrongCipherStatusUnknown;
    helpers.adjustForTLSIntolerance(HOST, PORT, range, strongCipherStatus);
    ASSERT_TRUE(helpers.rememberIntolerantAtVersion(HOST, PORT,
                                                    range.min, range.max, 0));
  }

  {
    SSLVersionRange range = { SSL_LIBRARY_VERSION_3_0,
                              SSL_LIBRARY_VERSION_TLS_1_2 };
    StrongCipherStatus strongCipherStatus = StrongCipherStatusUnknown;
    helpers.adjustForTLSIntolerance(HOST, PORT, range, strongCipherStatus);
    ASSERT_TRUE(helpers.rememberStrongCiphersFailed(HOST, PORT));
  }

  {
    SSLVersionRange range = { SSL_LIBRARY_VERSION_3_0,
                              SSL_LIBRARY_VERSION_TLS_1_2 };
    StrongCipherStatus strongCipherStatus = StrongCipherStatusUnknown;
    helpers.adjustForTLSIntolerance(HOST, PORT, range, strongCipherStatus);
    ASSERT_EQ(SSL_LIBRARY_VERSION_3_0, range.min);
    ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_1, range.max);
    ASSERT_EQ(StrongCiphersFailed, strongCipherStatus);

    ASSERT_TRUE(helpers.rememberIntolerantAtVersion(HOST, PORT,
                                                    range.min, range.max, 0));
  }

  {
    SSLVersionRange range = { SSL_LIBRARY_VERSION_3_0,
                              SSL_LIBRARY_VERSION_TLS_1_2 };
    StrongCipherStatus strongCipherStatus = StrongCipherStatusUnknown;
    helpers.adjustForTLSIntolerance(HOST, PORT, range, strongCipherStatus);
    ASSERT_EQ(SSL_LIBRARY_VERSION_3_0, range.min);
    ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_0, range.max);
    ASSERT_EQ(StrongCiphersFailed, strongCipherStatus);
  }
}

TEST_F(TLSIntoleranceTest, Test_Strong_Ciphers_Failed_With_High_Limit)
{
  // this value disables version fallback entirely: with this value, all efforts
  // to mark an origin as version intolerant fail
  helpers.mVersionFallbackLimit = SSL_LIBRARY_VERSION_TLS_1_2;
  // ...but weak ciphers fallback will not be disabled
  ASSERT_TRUE(helpers.rememberStrongCiphersFailed(HOST, PORT));
  ASSERT_FALSE(helpers.rememberIntolerantAtVersion(HOST, PORT,
                                                   SSL_LIBRARY_VERSION_3_0,
                                                   SSL_LIBRARY_VERSION_TLS_1_2,
                                                   0));
  ASSERT_FALSE(helpers.rememberIntolerantAtVersion(HOST, PORT,
                                                   SSL_LIBRARY_VERSION_3_0,
                                                   SSL_LIBRARY_VERSION_TLS_1_1,
                                                   0));
  ASSERT_FALSE(helpers.rememberIntolerantAtVersion(HOST, PORT,
                                                   SSL_LIBRARY_VERSION_3_0,
                                                   SSL_LIBRARY_VERSION_TLS_1_0,
                                                   0));
}

TEST_F(TLSIntoleranceTest, Test_Tolerant_Does_Not_Override_Weak_Ciphers_Fallback)
{
  ASSERT_TRUE(helpers.rememberStrongCiphersFailed(HOST, PORT));
  // No adjustment made when intolerant is zero.
  helpers.rememberTolerantAtVersion(HOST, PORT, SSL_LIBRARY_VERSION_TLS_1_1);
  SSLVersionRange range = { SSL_LIBRARY_VERSION_3_0,
                            SSL_LIBRARY_VERSION_TLS_1_2 };
  StrongCipherStatus strongCipherStatus = StrongCipherStatusUnknown;
  helpers.adjustForTLSIntolerance(HOST, PORT, range, strongCipherStatus);
  ASSERT_EQ(SSL_LIBRARY_VERSION_3_0, range.min);
  ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_2, range.max);
  ASSERT_EQ(StrongCiphersFailed, strongCipherStatus);
}

TEST_F(TLSIntoleranceTest, Test_Weak_Ciphers_Fallback_Does_Not_Override_Tolerant)
{
  // No adjustment made when there is no entry for the site.
  helpers.rememberTolerantAtVersion(HOST, PORT, SSL_LIBRARY_VERSION_TLS_1_1);
  // false because strongCipherWorked is set by rememberTolerantAtVersion.
  ASSERT_FALSE(helpers.rememberStrongCiphersFailed(HOST, PORT));
  SSLVersionRange range = { SSL_LIBRARY_VERSION_3_0,
                            SSL_LIBRARY_VERSION_TLS_1_2 };
  StrongCipherStatus strongCipherStatus = StrongCipherStatusUnknown;
  helpers.adjustForTLSIntolerance(HOST, PORT, range, strongCipherStatus);
  ASSERT_EQ(SSL_LIBRARY_VERSION_3_0, range.min);
  ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_2, range.max);
  ASSERT_EQ(StrongCiphersWorked, strongCipherStatus);
}

TEST_F(TLSIntoleranceTest, TLS_Forget_Intolerance)
{
  {
    ASSERT_TRUE(helpers.rememberIntolerantAtVersion(HOST, PORT,
                                                    SSL_LIBRARY_VERSION_3_0,
                                                    SSL_LIBRARY_VERSION_TLS_1_2,
                                                    0));

    SSLVersionRange range = { SSL_LIBRARY_VERSION_3_0,
                              SSL_LIBRARY_VERSION_TLS_1_2 };
    StrongCipherStatus strongCipherStatus = StrongCipherStatusUnknown;
    helpers.adjustForTLSIntolerance(HOST, PORT, range, strongCipherStatus);
    ASSERT_EQ(SSL_LIBRARY_VERSION_3_0, range.min);
    ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_1, range.max);
    ASSERT_EQ(StrongCipherStatusUnknown, strongCipherStatus);
  }

  {
    helpers.forgetIntolerance(HOST, PORT);

    SSLVersionRange range = { SSL_LIBRARY_VERSION_3_0,
                              SSL_LIBRARY_VERSION_TLS_1_2 };
    StrongCipherStatus strongCipherStatus = StrongCipherStatusUnknown;
    helpers.adjustForTLSIntolerance(HOST, PORT, range, strongCipherStatus);
    ASSERT_EQ(SSL_LIBRARY_VERSION_3_0, range.min);
    ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_2, range.max);
    ASSERT_EQ(StrongCipherStatusUnknown, strongCipherStatus);
  }
}

TEST_F(TLSIntoleranceTest, TLS_Forget_Strong_Cipher_Failed)
{
  {
    ASSERT_TRUE(helpers.rememberStrongCiphersFailed(HOST, PORT));

    SSLVersionRange range = { SSL_LIBRARY_VERSION_3_0,
                              SSL_LIBRARY_VERSION_TLS_1_2 };
    StrongCipherStatus strongCipherStatus = StrongCipherStatusUnknown;
    helpers.adjustForTLSIntolerance(HOST, PORT, range, strongCipherStatus);
    ASSERT_EQ(StrongCiphersFailed, strongCipherStatus);
  }

  {
    helpers.forgetIntolerance(HOST, PORT);

    SSLVersionRange range = { SSL_LIBRARY_VERSION_3_0,
                              SSL_LIBRARY_VERSION_TLS_1_2 };
    StrongCipherStatus strongCipherStatus = StrongCipherStatusUnknown;
    helpers.adjustForTLSIntolerance(HOST, PORT, range, strongCipherStatus);
    ASSERT_EQ(StrongCipherStatusUnknown, strongCipherStatus);
  }
}

TEST_F(TLSIntoleranceTest, TLS_Dont_Forget_Tolerance)
{
  {
    helpers.rememberTolerantAtVersion(HOST, PORT, SSL_LIBRARY_VERSION_TLS_1_1);

    SSLVersionRange range = { SSL_LIBRARY_VERSION_3_0,
                              SSL_LIBRARY_VERSION_TLS_1_2 };
    StrongCipherStatus strongCipherStatus = StrongCipherStatusUnknown;
    helpers.adjustForTLSIntolerance(HOST, PORT, range, strongCipherStatus);
    ASSERT_EQ(SSL_LIBRARY_VERSION_3_0, range.min);
    ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_2, range.max);
    ASSERT_EQ(StrongCiphersWorked, strongCipherStatus);
  }

  {
    ASSERT_TRUE(helpers.rememberIntolerantAtVersion(HOST, PORT,
                                                    SSL_LIBRARY_VERSION_3_0,
                                                    SSL_LIBRARY_VERSION_TLS_1_2,
                                                    0));

    SSLVersionRange range = { SSL_LIBRARY_VERSION_3_0,
                              SSL_LIBRARY_VERSION_TLS_1_2 };
    StrongCipherStatus strongCipherStatus = StrongCipherStatusUnknown;
    helpers.adjustForTLSIntolerance(HOST, PORT, range, strongCipherStatus);
    ASSERT_EQ(SSL_LIBRARY_VERSION_3_0, range.min);
    ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_1, range.max);
    ASSERT_EQ(StrongCiphersWorked, strongCipherStatus);
  }

  {
    helpers.forgetIntolerance(HOST, PORT);

    SSLVersionRange range = { SSL_LIBRARY_VERSION_3_0,
                              SSL_LIBRARY_VERSION_TLS_1_2 };
    StrongCipherStatus strongCipherStatus = StrongCipherStatusUnknown;
    helpers.adjustForTLSIntolerance(HOST, PORT, range, strongCipherStatus);
    ASSERT_EQ(SSL_LIBRARY_VERSION_3_0, range.min);
    ASSERT_EQ(SSL_LIBRARY_VERSION_TLS_1_2, range.max);
    ASSERT_EQ(StrongCiphersWorked, strongCipherStatus);
  }
}
