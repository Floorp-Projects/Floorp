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
PutAndGet(OCSPCache& cache, const CertID& certID, PRErrorCode error,
          PRTime time)
{
  // The first time is thisUpdate. The second is validUntil.
  // The caller is expecting the validUntil returned with Get
  // to be equal to the passed-in time. Since these values will
  // be different in practice, make thisUpdate less than validUntil.
  ASSERT_TRUE(time >= 10);
  SECStatus rv = cache.Put(certID, error, time - 10, time);
  ASSERT_TRUE(rv == SECSuccess);
  PRErrorCode errorOut;
  PRTime timeOut;
  ASSERT_TRUE(cache.Get(certID, errorOut, timeOut));
  ASSERT_TRUE(error == errorOut && time == timeOut);
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
  PutAndGet(cache, CertID(fakeIssuer1, fakeKey000, fakeSerial001), 0, PR_Now());
  PRErrorCode errorOut;
  PRTime timeOut;
  ASSERT_FALSE(cache.Get(CertID(fakeIssuer1, fakeKey001, fakeSerial000),
                         errorOut, timeOut));
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
    PutAndGet(cache, CertID(fakeIssuer1, fakeKey000, fakeSerial), 0, timeIn + i);
  }

  PRErrorCode errorOut;
  PRTime timeOut;

  // This will be at the end of the list in the cache
  CertID cert0000(fakeIssuer1, fakeKey000, fakeSerial0000);
  ASSERT_TRUE(cache.Get(cert0000, errorOut, timeOut));
  ASSERT_TRUE(errorOut == 0 && timeOut == timeIn);
  // Once we access it, it goes to the front
  ASSERT_TRUE(cache.Get(cert0000, errorOut, timeOut));
  ASSERT_TRUE(errorOut == 0 && timeOut == timeIn);

  // This will be in the middle
  static const SECItem fakeSerial0512 = {
    siBuffer,
    const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>("0512")),
    4
  };
  CertID cert0512(fakeIssuer1, fakeKey000, fakeSerial0512);
  ASSERT_TRUE(cache.Get(cert0512, errorOut, timeOut));
  ASSERT_TRUE(errorOut == 0 && timeOut == timeIn + 512);
  ASSERT_TRUE(cache.Get(cert0512, errorOut, timeOut));
  ASSERT_TRUE(errorOut == 0 && timeOut == timeIn + 512);

  // We've never seen this certificate
  static const SECItem fakeSerial1111 = {
    siBuffer,
    const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>("1111")),
    4
  };
  ASSERT_FALSE(cache.Get(CertID(fakeIssuer1, fakeKey000, fakeSerial1111),
                         errorOut, timeOut));
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
    PutAndGet(cache, CertID(fakeIssuer1, fakeKey000, fakeSerial), 0, timeIn + i);
  }

  PRErrorCode errorOut;
  PRTime timeOut;
  ASSERT_FALSE(cache.Get(CertID(fakeIssuer1, fakeKey001, fakeSerial0000),
                         errorOut, timeOut));
}

TEST_F(OCSPCacheTest, TestNoEvictionForRevokedResponses)
{
  SCOPED_TRACE("");
  PRTime timeIn = PR_Now();
  CertID notEvicted(fakeIssuer1, fakeKey000, fakeSerial0000);
  PutAndGet(cache, notEvicted, SEC_ERROR_REVOKED_CERTIFICATE, timeIn);
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
    PutAndGet(cache, CertID(fakeIssuer1, fakeKey000, fakeSerial), 0, timeIn + i);
  }
  PRErrorCode errorOut;
  PRTime timeOut;
  ASSERT_TRUE(cache.Get(notEvicted, errorOut, timeOut));
  ASSERT_TRUE(errorOut == SEC_ERROR_REVOKED_CERTIFICATE && timeOut == timeIn);

  const SECItem fakeSerial0001 = {
    siBuffer,
    const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>("0001")),
    4
  };
  CertID evicted(fakeIssuer1, fakeKey000, fakeSerial0001);
  ASSERT_FALSE(cache.Get(evicted, errorOut, timeOut));
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
              SEC_ERROR_REVOKED_CERTIFICATE, timeIn + i);
  }
  const SECItem fakeSerial1025 = {
    siBuffer,
    const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>("1025")),
    4
  };
  CertID good(fakeIssuer1, fakeKey000, fakeSerial1025);
  // This will "succeed", allowing verification to continue. However,
  // nothing was actually put in the cache.
  SECStatus result = cache.Put(good, 0, timeIn + 1025 - 50, timeIn + 1025);
  ASSERT_TRUE(result == SECSuccess);
  PRErrorCode errorOut;
  PRTime timeOut;
  ASSERT_FALSE(cache.Get(good, errorOut, timeOut));

  const SECItem fakeSerial1026 = {
    siBuffer,
    const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>("1026")),
    4
  };
  CertID revoked(fakeIssuer1, fakeKey000, fakeSerial1026);
  // This will fail, causing verification to fail.
  result = cache.Put(revoked, SEC_ERROR_REVOKED_CERTIFICATE,
                     timeIn + 1026 - 50, timeIn + 1026);
  PRErrorCode error = PR_GetError();
  ASSERT_TRUE(result == SECFailure);
  ASSERT_TRUE(error == SEC_ERROR_REVOKED_CERTIFICATE);
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
  PutAndGet(cache, subject, 0, timeIn);
  PRErrorCode errorOut;
  PRTime timeOut;
  ASSERT_TRUE(cache.Get(subject, errorOut, timeOut));
  ASSERT_TRUE(errorOut == 0 && timeOut == timeIn);
  // Test that we don't match a different issuer DN
  ASSERT_FALSE(cache.Get(CertID(fakeIssuer2, fakeKey000, fakeSerial001),
                         errorOut, timeOut));
  // Test that we don't match a different issuer key
  ASSERT_FALSE(cache.Get(CertID(fakeIssuer1, fakeKey001, fakeSerial001),
                         errorOut, timeOut));
}

TEST_F(OCSPCacheTest, Times)
{
  SCOPED_TRACE("");
  CertID certID(fakeIssuer1, fakeKey000, fakeSerial0000);
  PutAndGet(cache, certID, SEC_ERROR_OCSP_UNKNOWN_CERT, 100);
  PutAndGet(cache, certID, 0, 200);
  // This should not override the more recent entry.
  ASSERT_EQ(SECSuccess, cache.Put(certID, SEC_ERROR_OCSP_UNKNOWN_CERT, 100,
                                  100));
  PRErrorCode errorOut;
  PRTime timeOut;
  ASSERT_TRUE(cache.Get(certID, errorOut, timeOut));
  // Here we see the more recent time.
  ASSERT_TRUE(errorOut == 0 && timeOut == 200);

  // SEC_ERROR_REVOKED_CERTIFICATE overrides everything
  PutAndGet(cache, certID, SEC_ERROR_REVOKED_CERTIFICATE, 50);
}

TEST_F(OCSPCacheTest, NetworkFailure)
{
  SCOPED_TRACE("");
  CertID certID(fakeIssuer1, fakeKey000, fakeSerial0000);
  PutAndGet(cache, certID, PR_CONNECT_REFUSED_ERROR, 100);
  PutAndGet(cache, certID, 0, 200);
  // This should not override the already present entry.
  SECStatus rv = cache.Put(certID, PR_CONNECT_REFUSED_ERROR, 300, 350);
  ASSERT_TRUE(rv == SECSuccess);
  PRErrorCode errorOut;
  PRTime timeOut;
  ASSERT_TRUE(cache.Get(certID, errorOut, timeOut));
  ASSERT_TRUE(errorOut == 0 && timeOut == 200);

  PutAndGet(cache, certID, SEC_ERROR_OCSP_UNKNOWN_CERT, 400);
  // This should not override the already present entry.
  rv = cache.Put(certID, PR_CONNECT_REFUSED_ERROR, 500, 550);
  ASSERT_TRUE(rv == SECSuccess);
  ASSERT_TRUE(cache.Get(certID, errorOut, timeOut));
  ASSERT_TRUE(errorOut == SEC_ERROR_OCSP_UNKNOWN_CERT && timeOut == 400);

  PutAndGet(cache, certID, SEC_ERROR_REVOKED_CERTIFICATE, 600);
  // This should not override the already present entry.
  rv = cache.Put(certID, PR_CONNECT_REFUSED_ERROR, 700, 750);
  ASSERT_TRUE(rv == SECSuccess);
  ASSERT_TRUE(cache.Get(certID, errorOut, timeOut));
  ASSERT_TRUE(errorOut == SEC_ERROR_REVOKED_CERTIFICATE && timeOut == 600);
}
