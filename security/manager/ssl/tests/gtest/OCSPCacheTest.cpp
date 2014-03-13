/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CertVerifier.h"
#include "OCSPCache.h"
#include "nss.h"
#include "prprf.h"
#include "secerr.h"

#include "gtest/gtest.h"

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

// Makes a fake certificate with just the fields we need for testing here.
// (And those values are almost entirely bogus.)
// stackCert should be stack-allocated memory.
static void
MakeFakeCert(CERTCertificate* stackCert, const char* subjectValue,
             const char* issuerValue, const char* serialNumberValue,
             const char* publicKeyValue)
{
  stackCert->derSubject.data = (unsigned char*)subjectValue;
  stackCert->derSubject.len = strlen(subjectValue);
  stackCert->derIssuer.data = (unsigned char*)issuerValue;
  stackCert->derIssuer.len = strlen(issuerValue);
  stackCert->serialNumber.data = (unsigned char*)serialNumberValue;
  stackCert->serialNumber.len = strlen(serialNumberValue);
  stackCert->derPublicKey.data = (unsigned char*)publicKeyValue;
  stackCert->derPublicKey.len = strlen(publicKeyValue);
  CERTName *subject = CERT_AsciiToName(subjectValue); // TODO: this will leak...
  ASSERT_TRUE(subject);
  stackCert->subject.arena = subject->arena;
  stackCert->subject.rdns = subject->rdns;
}

static void
PutAndGet(mozilla::psm::OCSPCache& cache, CERTCertificate* subject,
          CERTCertificate* issuer,
          PRErrorCode error, PRTime time)
{
  // The first time is thisUpdate. The second is validUntil.
  // The caller is expecting the validUntil returned with Get
  // to be equal to the passed-in time. Since these values will
  // be different in practice, make thisUpdate less than validUntil.
  ASSERT_TRUE(time >= 10);
  SECStatus rv = cache.Put(subject, issuer, error, time - 10, time);
  ASSERT_TRUE(rv == SECSuccess);
  PRErrorCode errorOut;
  PRTime timeOut;
  ASSERT_TRUE(cache.Get(subject, issuer, errorOut, timeOut));
  ASSERT_TRUE(error == errorOut && time == timeOut);
}

TEST_F(OCSPCacheTest, TestPutAndGet)
{
  SCOPED_TRACE("");
  CERTCertificate subject;
  MakeFakeCert(&subject, "CN=subject1", "CN=issuer1", "001", "key001");
  CERTCertificate issuer;
  MakeFakeCert(&issuer, "CN=issuer1", "CN=issuer1", "000", "key000");
  PutAndGet(cache, &subject, &issuer, 0, PR_Now());
  PRErrorCode errorOut;
  PRTime timeOut;
  ASSERT_FALSE(cache.Get(&issuer, &issuer, errorOut, timeOut));
}

TEST_F(OCSPCacheTest, TestVariousGets)
{
  SCOPED_TRACE("");
  CERTCertificate issuer;
  MakeFakeCert(&issuer, "CN=issuer1", "CN=issuer1", "000", "key000");
  PRTime timeIn = PR_Now();
  for (int i = 0; i < MaxCacheEntries; i++) {
    CERTCertificate subject;
    char subjectBuf[64];
    PR_snprintf(subjectBuf, sizeof(subjectBuf), "CN=subject%04d", i);
    char serialBuf[8];
    PR_snprintf(serialBuf, sizeof(serialBuf), "%04d", i);
    MakeFakeCert(&subject, subjectBuf, "CN=issuer1", serialBuf, "key000");
    PutAndGet(cache, &subject, &issuer, 0, timeIn + i);
  }
  CERTCertificate subject;
  // This will be at the end of the list in the cache
  PRErrorCode errorOut;
  PRTime timeOut;
  MakeFakeCert(&subject, "CN=subject0000", "CN=issuer1", "0000", "key000");
  ASSERT_TRUE(cache.Get(&subject, &issuer, errorOut, timeOut));
  ASSERT_TRUE(errorOut == 0 && timeOut == timeIn);
  // Once we access it, it goes to the front
  ASSERT_TRUE(cache.Get(&subject, &issuer, errorOut, timeOut));
  ASSERT_TRUE(errorOut == 0 && timeOut == timeIn);
  MakeFakeCert(&subject, "CN=subject0512", "CN=issuer1", "0512", "key000");
  // This will be in the middle
  ASSERT_TRUE(cache.Get(&subject, &issuer, errorOut, timeOut));
  ASSERT_TRUE(errorOut == 0 && timeOut == timeIn + 512);
  ASSERT_TRUE(cache.Get(&subject, &issuer, errorOut, timeOut));
  ASSERT_TRUE(errorOut == 0 && timeOut == timeIn + 512);
  // We've never seen this certificate
  MakeFakeCert(&subject, "CN=subject1111", "CN=issuer1", "1111", "key000");
  ASSERT_FALSE(cache.Get(&subject, &issuer, errorOut, timeOut));
}

TEST_F(OCSPCacheTest, TestEviction)
{
  SCOPED_TRACE("");
  CERTCertificate issuer;
  PRTime timeIn = PR_Now();
  MakeFakeCert(&issuer, "CN=issuer1", "CN=issuer1", "000", "key000");
  // By putting more distinct entries in the cache than it can hold,
  // we cause the least recently used entry to be evicted.
  for (int i = 0; i < MaxCacheEntries + 1; i++) {
    CERTCertificate subject;
    char subjectBuf[64];
    PR_snprintf(subjectBuf, sizeof(subjectBuf), "CN=subject%04d", i);
    char serialBuf[8];
    PR_snprintf(serialBuf, sizeof(serialBuf), "%04d", i);
    MakeFakeCert(&subject, subjectBuf, "CN=issuer1", serialBuf, "key000");
    PutAndGet(cache, &subject, &issuer, 0, timeIn + i);
  }
  CERTCertificate evictedSubject;
  MakeFakeCert(&evictedSubject, "CN=subject0000", "CN=issuer1", "0000", "key000");
  PRErrorCode errorOut;
  PRTime timeOut;
  ASSERT_FALSE(cache.Get(&evictedSubject, &issuer, errorOut, timeOut));
}

TEST_F(OCSPCacheTest, TestNoEvictionForRevokedResponses)
{
  SCOPED_TRACE("");
  CERTCertificate issuer;
  MakeFakeCert(&issuer, "CN=issuer1", "CN=issuer1", "000", "key000");
  CERTCertificate notEvictedSubject;
  MakeFakeCert(&notEvictedSubject, "CN=subject0000", "CN=issuer1", "0000", "key000");
  PRTime timeIn = PR_Now();
  PutAndGet(cache, &notEvictedSubject, &issuer, SEC_ERROR_REVOKED_CERTIFICATE, timeIn);
  // By putting more distinct entries in the cache than it can hold,
  // we cause the least recently used entry that isn't revoked to be evicted.
  for (int i = 1; i < MaxCacheEntries + 1; i++) {
    CERTCertificate subject;
    char subjectBuf[64];
    PR_snprintf(subjectBuf, sizeof(subjectBuf), "CN=subject%04d", i);
    char serialBuf[8];
    PR_snprintf(serialBuf, sizeof(serialBuf), "%04d", i);
    MakeFakeCert(&subject, subjectBuf, "CN=issuer1", serialBuf, "key000");
    PutAndGet(cache, &subject, &issuer, 0, timeIn + i);
  }
  PRErrorCode errorOut;
  PRTime timeOut;
  ASSERT_TRUE(cache.Get(&notEvictedSubject, &issuer, errorOut, timeOut));
  ASSERT_TRUE(errorOut == SEC_ERROR_REVOKED_CERTIFICATE && timeOut == timeIn);
  CERTCertificate evictedSubject;
  MakeFakeCert(&evictedSubject, "CN=subject0001", "CN=issuer1", "0001", "key000");
  ASSERT_FALSE(cache.Get(&evictedSubject, &issuer, errorOut, timeOut));
}

TEST_F(OCSPCacheTest, TestEverythingIsRevoked)
{
  SCOPED_TRACE("");
  CERTCertificate issuer;
  MakeFakeCert(&issuer, "CN=issuer1", "CN=issuer1", "000", "key000");
  PRTime timeIn = PR_Now();
  // Fill up the cache with revoked responses.
  for (int i = 0; i < MaxCacheEntries; i++) {
    CERTCertificate subject;
    char subjectBuf[64];
    PR_snprintf(subjectBuf, sizeof(subjectBuf), "CN=subject%04d", i);
    char serialBuf[8];
    PR_snprintf(serialBuf, sizeof(serialBuf), "%04d", i);
    MakeFakeCert(&subject, subjectBuf, "CN=issuer1", serialBuf, "key000");
    PutAndGet(cache, &subject, &issuer, SEC_ERROR_REVOKED_CERTIFICATE, timeIn + i);
  }
  CERTCertificate goodSubject;
  MakeFakeCert(&goodSubject, "CN=subject1025", "CN=issuer1", "1025", "key000");
  // This will "succeed", allowing verification to continue. However,
  // nothing was actually put in the cache.
  SECStatus result = cache.Put(&goodSubject, &issuer, 0, timeIn + 1025 - 50,
                               timeIn + 1025);
  ASSERT_TRUE(result == SECSuccess);
  PRErrorCode errorOut;
  PRTime timeOut;
  ASSERT_FALSE(cache.Get(&goodSubject, &issuer, errorOut, timeOut));

  CERTCertificate revokedSubject;
  MakeFakeCert(&revokedSubject, "CN=subject1026", "CN=issuer1", "1026", "key000");
  // This will fail, causing verification to fail.
  result = cache.Put(&revokedSubject, &issuer, SEC_ERROR_REVOKED_CERTIFICATE,
                     timeIn + 1026 - 50, timeIn + 1026);
  PRErrorCode error = PR_GetError();
  ASSERT_TRUE(result == SECFailure);
  ASSERT_TRUE(error == SEC_ERROR_REVOKED_CERTIFICATE);
}

TEST_F(OCSPCacheTest, VariousIssuers)
{
  SCOPED_TRACE("");
  CERTCertificate issuer1;
  MakeFakeCert(&issuer1, "CN=issuer1", "CN=issuer1", "000", "key000");
  CERTCertificate issuer2;
  MakeFakeCert(&issuer2, "CN=issuer2", "CN=issuer2", "000", "key001");
  CERTCertificate issuer3;
  // Note: same CN as issuer1
  MakeFakeCert(&issuer3, "CN=issuer1", "CN=issuer3", "000", "key003");
  CERTCertificate subject;
  MakeFakeCert(&subject, "CN=subject", "CN=issuer1", "001", "key002");
  PRTime timeIn = PR_Now();
  PutAndGet(cache, &subject, &issuer1, 0, timeIn);
  PRErrorCode errorOut;
  PRTime timeOut;
  ASSERT_TRUE(cache.Get(&subject, &issuer1, errorOut, timeOut));
  ASSERT_TRUE(errorOut == 0 && timeOut == timeIn);
  ASSERT_FALSE(cache.Get(&subject, &issuer2, errorOut, timeOut));
  ASSERT_FALSE(cache.Get(&subject, &issuer3, errorOut, timeOut));
}

TEST_F(OCSPCacheTest, Times)
{
  SCOPED_TRACE("");
  CERTCertificate subject;
  MakeFakeCert(&subject, "CN=subject1", "CN=issuer1", "001", "key001");
  CERTCertificate issuer;
  MakeFakeCert(&issuer, "CN=issuer1", "CN=issuer1", "000", "key000");
  PutAndGet(cache, &subject, &issuer, SEC_ERROR_OCSP_UNKNOWN_CERT, 100);
  PutAndGet(cache, &subject, &issuer, 0, 200);
  // This should not override the more recent entry.
  SECStatus rv = cache.Put(&subject, &issuer, SEC_ERROR_OCSP_UNKNOWN_CERT, 100, 100);
  ASSERT_TRUE(rv == SECSuccess);
  PRErrorCode errorOut;
  PRTime timeOut;
  ASSERT_TRUE(cache.Get(&subject, &issuer, errorOut, timeOut));
  // Here we see the more recent time.
  ASSERT_TRUE(errorOut == 0 && timeOut == 200);

  // SEC_ERROR_REVOKED_CERTIFICATE overrides everything
  PutAndGet(cache, &subject, &issuer, SEC_ERROR_REVOKED_CERTIFICATE, 50);
}
