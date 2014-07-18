/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CertVerifier.h"
#include "OCSPCache.h"
#include "gtest/gtest.h"
#include "nss.h"
#include "pkix/pkixtypes.h"
#include "prerr.h"
#include "prprf.h"
#include "secerr.h"

using namespace mozilla::pkix;
using namespace mozilla::psm;

const int MaxCacheEntries = 1024;

class OCSPCacheTest : public ::testing::Test
{
  protected:
    static void SetUpTestCase()
    {
      NSS_NoDB_Init(nullptr);
      mozilla::psm::InitCertVerifierLog();
    }

    mozilla::psm::OCSPCache cache;
};

static void
PutAndGet(OCSPCache& cache, const CertID& certID, Result result,
          PRTime time)
{
  // The first time is thisUpdate. The second is validUntil.
  // The caller is expecting the validUntil returned with Get
  // to be equal to the passed-in time. Since these values will
  // be different in practice, make thisUpdate less than validUntil.
  ASSERT_TRUE(time >= 10);
  Result rv = cache.Put(certID, result, time - 10, time);
  ASSERT_TRUE(rv == Success);
  Result resultOut;
  PRTime timeOut;
  ASSERT_TRUE(cache.Get(certID, resultOut, timeOut));
  ASSERT_EQ(result, resultOut);
  ASSERT_EQ(time, timeOut);
}

static const SECItem fakeIssuer1 = {
  siBuffer,
  const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>("CN=issuer1")),
  10
};
static const SECItem fakeKey000 = {
  siBuffer,
  const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>("key000")),
  6
};
static const SECItem fakeKey001 = {
  siBuffer,
  const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>("key001")),
  6
};
static const SECItem fakeSerial0000 = {
  siBuffer,
  const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>("0000")),
  4
};

TEST_F(OCSPCacheTest, TestPutAndGet)
{
  static const SECItem fakeSerial000 = {
    siBuffer,
    const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>("000")),
    3
  };
  static const SECItem fakeSerial001 = {
    siBuffer,
    const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>("001")),
    3
  };

  SCOPED_TRACE("");
  PutAndGet(cache, CertID(fakeIssuer1, fakeKey000, fakeSerial001),
            Success, PR_Now());
  Result resultOut;
  PRTime timeOut;
  ASSERT_FALSE(cache.Get(CertID(fakeIssuer1, fakeKey001, fakeSerial000),
                         resultOut, timeOut));
}

TEST_F(OCSPCacheTest, TestVariousGets)
{
  SCOPED_TRACE("");
  PRTime timeIn = PR_Now();
  for (int i = 0; i < MaxCacheEntries; i++) {
    char serialBuf[8];
    PR_snprintf(serialBuf, sizeof(serialBuf), "%04d", i);
    const SECItem fakeSerial = {
      siBuffer,
      const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(serialBuf)),
      4
    };
    PutAndGet(cache, CertID(fakeIssuer1, fakeKey000, fakeSerial),
              Success, timeIn + i);
  }

  Result resultOut;
  PRTime timeOut;

  // This will be at the end of the list in the cache
  CertID cert0000(fakeIssuer1, fakeKey000, fakeSerial0000);
  ASSERT_TRUE(cache.Get(cert0000, resultOut, timeOut));
  ASSERT_EQ(Success, resultOut);
  ASSERT_EQ(timeIn, timeOut);
  // Once we access it, it goes to the front
  ASSERT_TRUE(cache.Get(cert0000, resultOut, timeOut));
  ASSERT_EQ(Success, resultOut);
  ASSERT_EQ(timeIn, timeOut);

  // This will be in the middle
  static const SECItem fakeSerial0512 = {
    siBuffer,
    const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>("0512")),
    4
  };
  CertID cert0512(fakeIssuer1, fakeKey000, fakeSerial0512);
  ASSERT_TRUE(cache.Get(cert0512, resultOut, timeOut));
  ASSERT_EQ(Success, resultOut);
  ASSERT_EQ(timeIn + 512, timeOut);
  ASSERT_TRUE(cache.Get(cert0512, resultOut, timeOut));
  ASSERT_EQ(Success, resultOut);
  ASSERT_EQ(timeIn + 512, timeOut);

  // We've never seen this certificate
  static const SECItem fakeSerial1111 = {
    siBuffer,
    const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>("1111")),
    4
  };
  ASSERT_FALSE(cache.Get(CertID(fakeIssuer1, fakeKey000, fakeSerial1111),
                         resultOut, timeOut));
}

TEST_F(OCSPCacheTest, TestEviction)
{
  SCOPED_TRACE("");
  PRTime timeIn = PR_Now();

  // By putting more distinct entries in the cache than it can hold,
  // we cause the least recently used entry to be evicted.
  for (int i = 0; i < MaxCacheEntries + 1; i++) {
    char serialBuf[8];
    PR_snprintf(serialBuf, sizeof(serialBuf), "%04d", i);
    const SECItem fakeSerial = {
      siBuffer,
      const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(serialBuf)),
      4
    };
    PutAndGet(cache, CertID(fakeIssuer1, fakeKey000, fakeSerial),
              Success, timeIn + i);
  }

  Result resultOut;
  PRTime timeOut;
  ASSERT_FALSE(cache.Get(CertID(fakeIssuer1, fakeKey001, fakeSerial0000),
                         resultOut, timeOut));
}

TEST_F(OCSPCacheTest, TestNoEvictionForRevokedResponses)
{
  SCOPED_TRACE("");
  PRTime timeIn = PR_Now();
  CertID notEvicted(fakeIssuer1, fakeKey000, fakeSerial0000);
  PutAndGet(cache, notEvicted, Result::ERROR_REVOKED_CERTIFICATE, timeIn);
  // By putting more distinct entries in the cache than it can hold,
  // we cause the least recently used entry that isn't revoked to be evicted.
  for (int i = 1; i < MaxCacheEntries + 1; i++) {
    char serialBuf[8];
    PR_snprintf(serialBuf, sizeof(serialBuf), "%04d", i);
    const SECItem fakeSerial = {
      siBuffer,
      reinterpret_cast<uint8_t*>(serialBuf),
      4
    };
    PutAndGet(cache, CertID(fakeIssuer1, fakeKey000, fakeSerial),
              Success, timeIn + i);
  }
  Result resultOut;
  PRTime timeOut;
  ASSERT_TRUE(cache.Get(notEvicted, resultOut, timeOut));
  ASSERT_EQ(Result::ERROR_REVOKED_CERTIFICATE, resultOut);
  ASSERT_EQ(timeIn, timeOut);

  const SECItem fakeSerial0001 = {
    siBuffer,
    const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>("0001")),
    4
  };
  CertID evicted(fakeIssuer1, fakeKey000, fakeSerial0001);
  ASSERT_FALSE(cache.Get(evicted, resultOut, timeOut));
}

TEST_F(OCSPCacheTest, TestEverythingIsRevoked)
{
  SCOPED_TRACE("");
  PRTime timeIn = PR_Now();
  // Fill up the cache with revoked responses.
  for (int i = 0; i < MaxCacheEntries; i++) {
    char serialBuf[8];
    PR_snprintf(serialBuf, sizeof(serialBuf), "%04d", i);
    const SECItem fakeSerial = {
      siBuffer,
      reinterpret_cast<uint8_t*>(serialBuf),
      4
    };
    PutAndGet(cache, CertID(fakeIssuer1, fakeKey000, fakeSerial),
              Result::ERROR_REVOKED_CERTIFICATE, timeIn + i);
  }
  const SECItem fakeSerial1025 = {
    siBuffer,
    const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>("1025")),
    4
  };
  CertID good(fakeIssuer1, fakeKey000, fakeSerial1025);
  // This will "succeed", allowing verification to continue. However,
  // nothing was actually put in the cache.
  Result result = cache.Put(good, Success, timeIn + 1025 - 50, timeIn + 1025);
  ASSERT_EQ(Success, result);
  Result resultOut;
  PRTime timeOut;
  ASSERT_FALSE(cache.Get(good, resultOut, timeOut));

  const SECItem fakeSerial1026 = {
    siBuffer,
    const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>("1026")),
    4
  };
  CertID revoked(fakeIssuer1, fakeKey000, fakeSerial1026);
  // This will fail, causing verification to fail.
  result = cache.Put(revoked, Result::ERROR_REVOKED_CERTIFICATE,
                     timeIn + 1026 - 50, timeIn + 1026);
  ASSERT_EQ(Result::ERROR_REVOKED_CERTIFICATE, result);
}

TEST_F(OCSPCacheTest, VariousIssuers)
{
  SCOPED_TRACE("");
  static const SECItem fakeIssuer2 = {
    siBuffer,
    const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>("CN=issuer2")),
    10
  };
  static const SECItem fakeSerial001 = {
    siBuffer,
    const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>("001")),
    3
  };

  PRTime timeIn = PR_Now();
  CertID subject(fakeIssuer1, fakeKey000, fakeSerial001);
  PutAndGet(cache, subject, Success, timeIn);
  Result resultOut;
  PRTime timeOut;
  ASSERT_TRUE(cache.Get(subject, resultOut, timeOut));
  ASSERT_EQ(Success, resultOut);
  ASSERT_EQ(timeIn, timeOut);
  // Test that we don't match a different issuer DN
  ASSERT_FALSE(cache.Get(CertID(fakeIssuer2, fakeKey000, fakeSerial001),
                         resultOut, timeOut));
  // Test that we don't match a different issuer key
  ASSERT_FALSE(cache.Get(CertID(fakeIssuer1, fakeKey001, fakeSerial001),
                         resultOut, timeOut));
}

TEST_F(OCSPCacheTest, Times)
{
  SCOPED_TRACE("");
  CertID certID(fakeIssuer1, fakeKey000, fakeSerial0000);
  PutAndGet(cache, certID, Result::ERROR_OCSP_UNKNOWN_CERT, 100);
  PutAndGet(cache, certID, Success, 200);
  // This should not override the more recent entry.
  ASSERT_EQ(Success,
            cache.Put(certID, Result::ERROR_OCSP_UNKNOWN_CERT, 100, 100));
  Result resultOut;
  PRTime timeOut;
  ASSERT_TRUE(cache.Get(certID, resultOut, timeOut));
  // Here we see the more recent time.
  ASSERT_EQ(Success, resultOut);
  ASSERT_EQ(200, timeOut);

  // Result::ERROR_REVOKED_CERTIFICATE overrides everything
  PutAndGet(cache, certID, Result::ERROR_REVOKED_CERTIFICATE, 50);
}

TEST_F(OCSPCacheTest, NetworkFailure)
{
  SCOPED_TRACE("");
  CertID certID(fakeIssuer1, fakeKey000, fakeSerial0000);
  PutAndGet(cache, certID, Result::ERROR_CONNECT_REFUSED, 100);
  PutAndGet(cache, certID, Success, 200);
  // This should not override the already present entry.
  ASSERT_EQ(Success,
            cache.Put(certID, Result::ERROR_CONNECT_REFUSED, 300, 350));
  Result resultOut;
  PRTime timeOut;
  ASSERT_TRUE(cache.Get(certID, resultOut, timeOut));
  ASSERT_EQ(Success, resultOut);
  ASSERT_EQ(200, timeOut);

  PutAndGet(cache, certID, Result::ERROR_OCSP_UNKNOWN_CERT, 400);
  // This should not override the already present entry.
  ASSERT_EQ(Success,
            cache.Put(certID, Result::ERROR_CONNECT_REFUSED, 500, 550));
  ASSERT_TRUE(cache.Get(certID, resultOut, timeOut));
  ASSERT_EQ(Result::ERROR_OCSP_UNKNOWN_CERT, resultOut);
  ASSERT_EQ(400, timeOut);

  PutAndGet(cache, certID, Result::ERROR_REVOKED_CERTIFICATE, 600);
  // This should not override the already present entry.
  ASSERT_EQ(Success,
            cache.Put(certID, Result::ERROR_CONNECT_REFUSED, 700, 750));
  ASSERT_TRUE(cache.Get(certID, resultOut, timeOut));
  ASSERT_EQ(Result::ERROR_REVOKED_CERTIFICATE, resultOut);
  ASSERT_EQ(600, timeOut);
}
