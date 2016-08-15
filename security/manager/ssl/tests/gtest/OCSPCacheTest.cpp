/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CertVerifier.h"
#include "OCSPCache.h"
#include "gtest/gtest.h"
#include "mozilla/Casting.h"
#include "mozilla/Sprintf.h"
#include "nss.h"
#include "pkix/pkixtypes.h"
#include "pkixtestutil.h"
#include "prerr.h"
#include "secerr.h"

using namespace mozilla::pkix;
using namespace mozilla::pkix::test;
using namespace mozilla::psm;

template <size_t N>
inline Input
LiteralInput(const char(&valueString)[N])
{
  // Ideally we would use mozilla::BitwiseCast() here rather than
  // reinterpret_cast for better type checking, but the |N - 1| part trips
  // static asserts.
  return Input(reinterpret_cast<const uint8_t(&)[N - 1]>(valueString));
}

const int MaxCacheEntries = 1024;

class psm_OCSPCacheTest : public ::testing::Test
{
protected:
  psm_OCSPCacheTest() : now(Now()) { }

  static void SetUpTestCase()
  {
    NSS_NoDB_Init(nullptr);
    mozilla::psm::InitCertVerifierLog();
  }

  const Time now;
  mozilla::psm::OCSPCache cache;
};

static void
PutAndGet(OCSPCache& cache, const CertID& certID, Result result,
          Time time)
{
  // The first time is thisUpdate. The second is validUntil.
  // The caller is expecting the validUntil returned with Get
  // to be equal to the passed-in time. Since these values will
  // be different in practice, make thisUpdate less than validUntil.
  Time thisUpdate(time);
  ASSERT_EQ(Success, thisUpdate.SubtractSeconds(10));
  Result rv = cache.Put(certID, result, thisUpdate, time);
  ASSERT_TRUE(rv == Success);
  Result resultOut;
  Time timeOut(Time::uninitialized);
  ASSERT_TRUE(cache.Get(certID, resultOut, timeOut));
  ASSERT_EQ(result, resultOut);
  ASSERT_EQ(time, timeOut);
}

Input fakeIssuer1(LiteralInput("CN=issuer1"));
Input fakeKey000(LiteralInput("key000"));
Input fakeKey001(LiteralInput("key001"));
Input fakeSerial0000(LiteralInput("0000"));

TEST_F(psm_OCSPCacheTest, TestPutAndGet)
{
  Input fakeSerial000(LiteralInput("000"));
  Input fakeSerial001(LiteralInput("001"));

  SCOPED_TRACE("");
  PutAndGet(cache, CertID(fakeIssuer1, fakeKey000, fakeSerial001),
            Success, now);
  Result resultOut;
  Time timeOut(Time::uninitialized);
  ASSERT_FALSE(cache.Get(CertID(fakeIssuer1, fakeKey001, fakeSerial000),
                         resultOut, timeOut));
}

TEST_F(psm_OCSPCacheTest, TestVariousGets)
{
  SCOPED_TRACE("");
  for (int i = 0; i < MaxCacheEntries; i++) {
    uint8_t serialBuf[8];
    snprintf(mozilla::BitwiseCast<char*, uint8_t*>(serialBuf), sizeof(serialBuf),
             "%04d", i);
    Input fakeSerial;
    ASSERT_EQ(Success, fakeSerial.Init(serialBuf, 4));
    Time timeIn(now);
    ASSERT_EQ(Success, timeIn.AddSeconds(i));
    PutAndGet(cache, CertID(fakeIssuer1, fakeKey000, fakeSerial),
              Success, timeIn);
  }

  Time timeIn(now);
  Result resultOut;
  Time timeOut(Time::uninitialized);

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
  Time timeInPlus512(now);
  ASSERT_EQ(Success, timeInPlus512.AddSeconds(512));

  static const Input fakeSerial0512(LiteralInput("0512"));
  CertID cert0512(fakeIssuer1, fakeKey000, fakeSerial0512);
  ASSERT_TRUE(cache.Get(cert0512, resultOut, timeOut));
  ASSERT_EQ(Success, resultOut);
  ASSERT_EQ(timeInPlus512, timeOut);
  ASSERT_TRUE(cache.Get(cert0512, resultOut, timeOut));
  ASSERT_EQ(Success, resultOut);
  ASSERT_EQ(timeInPlus512, timeOut);

  // We've never seen this certificate
  static const Input fakeSerial1111(LiteralInput("1111"));
  ASSERT_FALSE(cache.Get(CertID(fakeIssuer1, fakeKey000, fakeSerial1111),
                         resultOut, timeOut));
}

TEST_F(psm_OCSPCacheTest, TestEviction)
{
  SCOPED_TRACE("");
  // By putting more distinct entries in the cache than it can hold,
  // we cause the least recently used entry to be evicted.
  for (int i = 0; i < MaxCacheEntries + 1; i++) {
    uint8_t serialBuf[8];
    snprintf(mozilla::BitwiseCast<char*, uint8_t*>(serialBuf), sizeof(serialBuf),
             "%04d", i);
    Input fakeSerial;
    ASSERT_EQ(Success, fakeSerial.Init(serialBuf, 4));
    Time timeIn(now);
    ASSERT_EQ(Success, timeIn.AddSeconds(i));
    PutAndGet(cache, CertID(fakeIssuer1, fakeKey000, fakeSerial),
              Success, timeIn);
  }

  Result resultOut;
  Time timeOut(Time::uninitialized);
  ASSERT_FALSE(cache.Get(CertID(fakeIssuer1, fakeKey001, fakeSerial0000),
                         resultOut, timeOut));
}

TEST_F(psm_OCSPCacheTest, TestNoEvictionForRevokedResponses)
{
  SCOPED_TRACE("");
  CertID notEvicted(fakeIssuer1, fakeKey000, fakeSerial0000);
  Time timeIn(now);
  PutAndGet(cache, notEvicted, Result::ERROR_REVOKED_CERTIFICATE, timeIn);
  // By putting more distinct entries in the cache than it can hold,
  // we cause the least recently used entry that isn't revoked to be evicted.
  for (int i = 1; i < MaxCacheEntries + 1; i++) {
    uint8_t serialBuf[8];
    snprintf(mozilla::BitwiseCast<char*, uint8_t*>(serialBuf), sizeof(serialBuf),
             "%04d", i);
    Input fakeSerial;
    ASSERT_EQ(Success, fakeSerial.Init(serialBuf, 4));
    Time timeIn(now);
    ASSERT_EQ(Success, timeIn.AddSeconds(i));
    PutAndGet(cache, CertID(fakeIssuer1, fakeKey000, fakeSerial),
              Success, timeIn);
  }
  Result resultOut;
  Time timeOut(Time::uninitialized);
  ASSERT_TRUE(cache.Get(notEvicted, resultOut, timeOut));
  ASSERT_EQ(Result::ERROR_REVOKED_CERTIFICATE, resultOut);
  ASSERT_EQ(timeIn, timeOut);

  Input fakeSerial0001(LiteralInput("0001"));
  CertID evicted(fakeIssuer1, fakeKey000, fakeSerial0001);
  ASSERT_FALSE(cache.Get(evicted, resultOut, timeOut));
}

TEST_F(psm_OCSPCacheTest, TestEverythingIsRevoked)
{
  SCOPED_TRACE("");
  Time timeIn(now);
  // Fill up the cache with revoked responses.
  for (int i = 0; i < MaxCacheEntries; i++) {
    uint8_t serialBuf[8];
    snprintf(mozilla::BitwiseCast<char*, uint8_t*>(serialBuf), sizeof(serialBuf),
             "%04d", i);
    Input fakeSerial;
    ASSERT_EQ(Success, fakeSerial.Init(serialBuf, 4));
    Time timeIn(now);
    ASSERT_EQ(Success, timeIn.AddSeconds(i));
    PutAndGet(cache, CertID(fakeIssuer1, fakeKey000, fakeSerial),
              Result::ERROR_REVOKED_CERTIFICATE, timeIn);
  }
  static const Input fakeSerial1025(LiteralInput("1025"));
  CertID good(fakeIssuer1, fakeKey000, fakeSerial1025);
  // This will "succeed", allowing verification to continue. However,
  // nothing was actually put in the cache.
  Time timeInPlus1025(timeIn);
  ASSERT_EQ(Success, timeInPlus1025.AddSeconds(1025));
  Time timeInPlus1025Minus50(timeInPlus1025);
  ASSERT_EQ(Success, timeInPlus1025Minus50.SubtractSeconds(50));
  Result result = cache.Put(good, Success, timeInPlus1025Minus50,
                            timeInPlus1025);
  ASSERT_EQ(Success, result);
  Result resultOut;
  Time timeOut(Time::uninitialized);
  ASSERT_FALSE(cache.Get(good, resultOut, timeOut));

  static const Input fakeSerial1026(LiteralInput("1026"));
  CertID revoked(fakeIssuer1, fakeKey000, fakeSerial1026);
  // This will fail, causing verification to fail.
  Time timeInPlus1026(timeIn);
  ASSERT_EQ(Success, timeInPlus1026.AddSeconds(1026));
  Time timeInPlus1026Minus50(timeInPlus1026);
  ASSERT_EQ(Success, timeInPlus1026Minus50.SubtractSeconds(50));
  result = cache.Put(revoked, Result::ERROR_REVOKED_CERTIFICATE,
                     timeInPlus1026Minus50, timeInPlus1026);
  ASSERT_EQ(Result::ERROR_REVOKED_CERTIFICATE, result);
}

TEST_F(psm_OCSPCacheTest, VariousIssuers)
{
  SCOPED_TRACE("");
  Time timeIn(now);
  static const Input fakeIssuer2(LiteralInput("CN=issuer2"));
  static const Input fakeSerial001(LiteralInput("001"));
  CertID subject(fakeIssuer1, fakeKey000, fakeSerial001);
  PutAndGet(cache, subject, Success, now);
  Result resultOut;
  Time timeOut(Time::uninitialized);
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

TEST_F(psm_OCSPCacheTest, Times)
{
  SCOPED_TRACE("");
  CertID certID(fakeIssuer1, fakeKey000, fakeSerial0000);
  PutAndGet(cache, certID, Result::ERROR_OCSP_UNKNOWN_CERT,
            TimeFromElapsedSecondsAD(100));
  PutAndGet(cache, certID, Success, TimeFromElapsedSecondsAD(200));
  // This should not override the more recent entry.
  ASSERT_EQ(Success,
            cache.Put(certID, Result::ERROR_OCSP_UNKNOWN_CERT,
                      TimeFromElapsedSecondsAD(100),
                      TimeFromElapsedSecondsAD(100)));
  Result resultOut;
  Time timeOut(Time::uninitialized);
  ASSERT_TRUE(cache.Get(certID, resultOut, timeOut));
  // Here we see the more recent time.
  ASSERT_EQ(Success, resultOut);
  ASSERT_EQ(TimeFromElapsedSecondsAD(200), timeOut);

  // Result::ERROR_REVOKED_CERTIFICATE overrides everything
  PutAndGet(cache, certID, Result::ERROR_REVOKED_CERTIFICATE,
            TimeFromElapsedSecondsAD(50));
}

TEST_F(psm_OCSPCacheTest, NetworkFailure)
{
  SCOPED_TRACE("");
  CertID certID(fakeIssuer1, fakeKey000, fakeSerial0000);
  PutAndGet(cache, certID, Result::ERROR_CONNECT_REFUSED,
            TimeFromElapsedSecondsAD(100));
  PutAndGet(cache, certID, Success, TimeFromElapsedSecondsAD(200));
  // This should not override the already present entry.
  ASSERT_EQ(Success,
            cache.Put(certID, Result::ERROR_CONNECT_REFUSED,
                      TimeFromElapsedSecondsAD(300),
                      TimeFromElapsedSecondsAD(350)));
  Result resultOut;
  Time timeOut(Time::uninitialized);
  ASSERT_TRUE(cache.Get(certID, resultOut, timeOut));
  ASSERT_EQ(Success, resultOut);
  ASSERT_EQ(TimeFromElapsedSecondsAD(200), timeOut);

  PutAndGet(cache, certID, Result::ERROR_OCSP_UNKNOWN_CERT,
            TimeFromElapsedSecondsAD(400));
  // This should not override the already present entry.
  ASSERT_EQ(Success,
            cache.Put(certID, Result::ERROR_CONNECT_REFUSED,
                      TimeFromElapsedSecondsAD(500),
                      TimeFromElapsedSecondsAD(550)));
  ASSERT_TRUE(cache.Get(certID, resultOut, timeOut));
  ASSERT_EQ(Result::ERROR_OCSP_UNKNOWN_CERT, resultOut);
  ASSERT_EQ(TimeFromElapsedSecondsAD(400), timeOut);

  PutAndGet(cache, certID, Result::ERROR_REVOKED_CERTIFICATE,
            TimeFromElapsedSecondsAD(600));
  // This should not override the already present entry.
  ASSERT_EQ(Success,
            cache.Put(certID, Result::ERROR_CONNECT_REFUSED,
                      TimeFromElapsedSecondsAD(700),
                      TimeFromElapsedSecondsAD(750)));
  ASSERT_TRUE(cache.Get(certID, resultOut, timeOut));
  ASSERT_EQ(Result::ERROR_REVOKED_CERTIFICATE, resultOut);
  ASSERT_EQ(TimeFromElapsedSecondsAD(600), timeOut);
}
