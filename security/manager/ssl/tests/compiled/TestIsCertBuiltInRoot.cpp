/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScopedNSSTypes.h"
#include "TestHarness.h"
#include "cert.h"
#include "certdb.h"
#include "nsISimpleEnumerator.h"
#include "nsIX509Cert.h"
#include "nsIX509CertDB.h"
#include "nsIX509CertList.h"
#include "nsServiceManagerUtils.h"
#include "nss.h"
#include "prerror.h"
#include "secerr.h"

// This is a certificate that (currently) ships with the platform. This test
// loads this certificate into the read/write certificate database, which
// simulates the situation where a built-in certificate's trust settings have
// been changed. It should still be considered a built-in root.
static char sGeoTrustPEM[] = "-----BEGIN CERTIFICATE-----\n\
MIICrjCCAjWgAwIBAgIQPLL0SAoA4v7rJDteYD7DazAKBggqhkjOPQQDAzCBmDEL\n\
MAkGA1UEBhMCVVMxFjAUBgNVBAoTDUdlb1RydXN0IEluYy4xOTA3BgNVBAsTMChj\n\
KSAyMDA3IEdlb1RydXN0IEluYy4gLSBGb3IgYXV0aG9yaXplZCB1c2Ugb25seTE2\n\
MDQGA1UEAxMtR2VvVHJ1c3QgUHJpbWFyeSBDZXJ0aWZpY2F0aW9uIEF1dGhvcml0\n\
eSAtIEcyMB4XDTA3MTEwNTAwMDAwMFoXDTM4MDExODIzNTk1OVowgZgxCzAJBgNV\n\
BAYTAlVTMRYwFAYDVQQKEw1HZW9UcnVzdCBJbmMuMTkwNwYDVQQLEzAoYykgMjAw\n\
NyBHZW9UcnVzdCBJbmMuIC0gRm9yIGF1dGhvcml6ZWQgdXNlIG9ubHkxNjA0BgNV\n\
BAMTLUdlb1RydXN0IFByaW1hcnkgQ2VydGlmaWNhdGlvbiBBdXRob3JpdHkgLSBH\n\
MjB2MBAGByqGSM49AgEGBSuBBAAiA2IABBWx6P0DFUPlrOuHNxFi79KDNlJ9RVcL\n\
So17VDs6bl8VAsBQps8lL33KSLjHUGMcKiEIfJo22Av+0SbFWDEwKCXzXV2juLal\n\
tJLtbCyf691DiaI8S0iRHVDsJt/WYC69IaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAO\n\
BgNVHQ8BAf8EBAMCAQYwHQYDVR0OBBYEFBVfNVdRVfslsq0DafwBo/q+EVXVMAoG\n\
CCqGSM49BAMDA2cAMGQCMGSWWaboCd6LuvpaiIjwH5HTRqjySkwCY/tsXzjbLkGT\n\
qQ7mndwxHLKgpxgceeHHNgIwOlavmnRs9vuD4DPTCF+hnMJbn0bWtsuRBmOiBucz\n\
rD6ogRLQy7rQkgu2npaqBA+K\n\
-----END CERTIFICATE-----";

static char sGeoTrustNickname[] =
  "GeoTrust Primary Certification Authority - G2";

static char sGeoTrustCertDBKey[] = "AAAAAAAAAAAAAAAQAAAAmzyy9EgK\n\
AOL+6yQ7XmA+w2swgZgxCzAJBgNVBAYTAlVTMRYwFAYDVQQKEw1HZW9UcnVzdCBJ\n\
bmMuMTkwNwYDVQQLEzAoYykgMjAwNyBHZW9UcnVzdCBJbmMuIC0gRm9yIGF1dGhv\n\
cml6ZWQgdXNlIG9ubHkxNjA0BgNVBAMTLUdlb1RydXN0IFByaW1hcnkgQ2VydGlm\n\
aWNhdGlvbiBBdXRob3JpdHkgLSBHMg==";

// This is the DB key (see nsIX509Cert.idl) of another built-in certificate.
// This test makes no changes to its trust settings. It should be considered a
// built-in root.
static char sVeriSignCertDBKey[] = "AAAAAAAAAAAAAAAQAAAAzS+A/iOM\n\
DiIPSGcSKJGHrLMwgcoxCzAJBgNVBAYTAlVTMRcwFQYDVQQKEw5WZXJpU2lnbiwg\n\
SW5jLjEfMB0GA1UECxMWVmVyaVNpZ24gVHJ1c3QgTmV0d29yazE6MDgGA1UECxMx\n\
KGMpIDIwMDcgVmVyaVNpZ24sIEluYy4gLSBGb3IgYXV0aG9yaXplZCB1c2Ugb25s\n\
eTFFMEMGA1UEAxM8VmVyaVNpZ24gQ2xhc3MgMyBQdWJsaWMgUHJpbWFyeSBDZXJ0\n\
aWZpY2F0aW9uIEF1dGhvcml0eSAtIEc0";

// This is a certificate that does not ship with the platform.
// It should not be considered a built-in root.
static char sLetsEncryptPEM[] = "-----BEGIN CERTIFICATE-----\n\
MIIEqDCCA5CgAwIBAgIRAJgT9HUT5XULQ+dDHpceRL0wDQYJKoZIhvcNAQELBQAw\n\
PzEkMCIGA1UEChMbRGlnaXRhbCBTaWduYXR1cmUgVHJ1c3QgQ28uMRcwFQYDVQQD\n\
Ew5EU1QgUm9vdCBDQSBYMzAeFw0xNTEwMTkyMjMzMzZaFw0yMDEwMTkyMjMzMzZa\n\
MEoxCzAJBgNVBAYTAlVTMRYwFAYDVQQKEw1MZXQncyBFbmNyeXB0MSMwIQYDVQQD\n\
ExpMZXQncyBFbmNyeXB0IEF1dGhvcml0eSBYMTCCASIwDQYJKoZIhvcNAQEBBQAD\n\
ggEPADCCAQoCggEBAJzTDPBa5S5Ht3JdN4OzaGMw6tc1Jhkl4b2+NfFwki+3uEtB\n\
BaupnjUIWOyxKsRohwuj43Xk5vOnYnG6eYFgH9eRmp/z0HhncchpDpWRz/7mmelg\n\
PEjMfspNdxIknUcbWuu57B43ABycrHunBerOSuu9QeU2mLnL/W08lmjfIypCkAyG\n\
dGfIf6WauFJhFBM/ZemCh8vb+g5W9oaJ84U/l4avsNwa72sNlRZ9xCugZbKZBDZ1\n\
gGusSvMbkEl4L6KWTyogJSkExnTA0DHNjzE4lRa6qDO4Q/GxH8Mwf6J5MRM9LTb4\n\
4/zyM2q5OTHFr8SNDR1kFjOq+oQpttQLwNh9w5MCAwEAAaOCAZIwggGOMBIGA1Ud\n\
EwEB/wQIMAYBAf8CAQAwDgYDVR0PAQH/BAQDAgGGMH8GCCsGAQUFBwEBBHMwcTAy\n\
BggrBgEFBQcwAYYmaHR0cDovL2lzcmcudHJ1c3RpZC5vY3NwLmlkZW50cnVzdC5j\n\
b20wOwYIKwYBBQUHMAKGL2h0dHA6Ly9hcHBzLmlkZW50cnVzdC5jb20vcm9vdHMv\n\
ZHN0cm9vdGNheDMucDdjMB8GA1UdIwQYMBaAFMSnsaR7LHH62+FLkHX/xBVghYkQ\n\
MFQGA1UdIARNMEswCAYGZ4EMAQIBMD8GCysGAQQBgt8TAQEBMDAwLgYIKwYBBQUH\n\
AgEWImh0dHA6Ly9jcHMucm9vdC14MS5sZXRzZW5jcnlwdC5vcmcwPAYDVR0fBDUw\n\
MzAxoC+gLYYraHR0cDovL2NybC5pZGVudHJ1c3QuY29tL0RTVFJPT1RDQVgzQ1JM\n\
LmNybDATBgNVHR4EDDAKoQgwBoIELm1pbDAdBgNVHQ4EFgQUqEpqYwR93brm0Tm3\n\
pkVl7/Oo7KEwDQYJKoZIhvcNAQELBQADggEBANHIIkus7+MJiZZQsY14cCoBG1hd\n\
v0J20/FyWo5ppnfjL78S2k4s2GLRJ7iD9ZDKErndvbNFGcsW+9kKK/TnY21hp4Dd\n\
ITv8S9ZYQ7oaoqs7HwhEMY9sibED4aXw09xrJZTC9zK1uIfW6t5dHQjuOWv+HHoW\n\
ZnupyxpsEUlEaFb+/SCI4KCSBdAsYxAcsHYI5xxEI4LutHp6s3OT2FuO90WfdsIk\n\
6q78OMSdn875bNjdBYAqxUp2/LEIHfDBkLoQz0hFJmwAbYahqKaLn73PAAm1X2kj\n\
f1w8DdnkabOLGeOVcj9LQ+s67vBykx4anTjURkbqZslUEUsn2k5xeua2zUk=\n\
-----END CERTIFICATE-----";

static char sLetsEncryptNickname[] = "Let's Encrypt Authority X1";

static char sLetsEncryptCertDBKey[] = "AAAAAAAAAAAAAAARAAAAQQCYE\n\
/R1E+V1C0PnQx6XHkS9MD8xJDAiBgNVBAoTG0RpZ2l0YWwgU2lnbmF0dXJlIFRyd\n\
XN0IENvLjEXMBUGA1UEAxMORFNUIFJvb3QgQ0EgWDM=";

static SECItem* sCertDER = nullptr;

static SECStatus
GetCertDER(void*, SECItem** certs, int numcerts)
{
  if (numcerts != 1) {
    fail("numcerts should be 1");
    PR_SetError(SEC_ERROR_INVALID_ARGS, 0);
    return SECFailure;
  }
  sCertDER = SECITEM_DupItem(certs[0]);
  if (!sCertDER) {
    fail("failed to copy data out (out of memory?)");
    PR_SetError(SEC_ERROR_NO_MEMORY, 0);
    return SECFailure;
  }
  passed("GetCertDER succeeded");
  return SECSuccess;
}

bool
AddCertificate(char* pem, char* nickname)
{
  if (CERT_DecodeCertPackage(pem, strlen(pem), GetCertDER, nullptr)
        != SECSuccess) {
    fail("CERT_DecodeCertPackage failed");
    return false;
  }

  if (!sCertDER) {
    fail("sCertDER didn't get set as expected");
    return false;
  }

  mozilla::UniqueCERTCertificate cert(
    CERT_NewTempCertificate(CERT_GetDefaultCertDB(), sCertDER, nickname, false,
                            true));
  if (!cert) {
    fail("CERT_NewTempCertificate failed");
    return false;
  }

  CERTCertTrust trust;
  trust.sslFlags = 0;
  trust.emailFlags = 0;
  trust.objectSigningFlags = 0;
  if (CERT_AddTempCertToPerm(cert.get(), nickname, &trust) != SECSuccess) {
    fail("CERT_AddTempCertToPerm failed");
    return false;
  }
  passed("AddCertificate succeeded");
  return true;
}

bool
PreloadNSSCertDB(const char* profilePath)
{
  if (NSS_IsInitialized()) {
    fail("NSS shouldn't already be initialized, or part of this test is moot");
    return false;
  }
  if (NSS_Initialize(profilePath, "", "", SECMOD_DB, 0) != SECSuccess) {
    fail("couldn't initialize NSS the first time");
    return false;
  }

  if (!AddCertificate(sGeoTrustPEM, sGeoTrustNickname)) {
    fail("couldn't add GeoTrust certificate to NSS");
    return false;
  }

  if (!AddCertificate(sLetsEncryptPEM, sLetsEncryptNickname)) {
    fail("couldn't add Let's Encrypt certificate to NSS");
    return false;
  }

  if (NSS_Shutdown() != SECSuccess) {
    fail("couldn't shut down NSS the first time");
    return false;
  }
  passed("PreloadNSSCertDB succeeded");
  return true;
}

bool
TestIsCertBuiltIn(const char* certDBKey, bool expectedIsBuiltIn)
{
  nsCOMPtr<nsIX509CertDB> certDB(do_GetService(NS_X509CERTDB_CONTRACTID));
  if (!certDB) {
    fail("couldn't get certDB");
    return false;
  }

  nsCOMPtr<nsIX509Cert> cert;
  if (NS_FAILED(certDB->FindCertByDBKey(certDBKey, getter_AddRefs(cert)))) {
    fail("couldn't find root certificate in database (maybe it was removed?)");
    return false;
  }
  if (!cert) {
    fail("FindCertByDBKey says it succeeded but it clearly didn't");
    return false;
  }

  bool isBuiltInRoot;
  if (NS_FAILED(cert->GetIsBuiltInRoot(&isBuiltInRoot))) {
    fail("couldn't determine if the certificate was a built-in or not");
    return false;
  }
  if (isBuiltInRoot != expectedIsBuiltIn) {
    fail("did not get expected value for isBuiltInRoot");
    return false;
  }
  passed("got expected value for isBuiltInRoot");
  return true;
}

int
main(int argc, char* argv[])
{
  ScopedXPCOM xpcom("TestIsCertBuiltInRoot");
  if (xpcom.failed()) {
    fail("couldn't initialize XPCOM");
    return 1;
  }
  nsCOMPtr<nsIFile> profileDirectory(xpcom.GetProfileDirectory());
  if (!profileDirectory) {
    fail("couldn't get profile directory");
    return 1;
  }
  nsAutoCString profilePath;
  if (NS_FAILED(profileDirectory->GetNativePath(profilePath))) {
    fail("couldn't get profile path");
    return 1;
  }
  // One of the cases we want to test is when (in a previous run of the
  // platform) a built-in root certificate has had its trust modified from the
  // defaults. We can't initialize XPCOM twice in this test, but we can use NSS
  // directly to create a certficate database (before instantiating any XPCOM
  // objects that rely on NSS and thus would initialize it) with the appropriate
  // certificates for these tests.
  if (!PreloadNSSCertDB(profilePath.get())) {
    fail("couldn't set up NSS certificate DB for test");
    return 1;
  }

  if (!TestIsCertBuiltIn(sVeriSignCertDBKey, true)) {
    fail("built-in root with no modified trust should be considered built-in");
  } else {
    passed("built-in root with no modified trust considered built-in");
  }

  if (!TestIsCertBuiltIn(sGeoTrustCertDBKey, true)) {
    fail("built-in root with modified trust should be considered built-in");
  } else {
    passed("built-in root with modified trust considered built-in");
  }

  if (!TestIsCertBuiltIn(sLetsEncryptCertDBKey, false)) {
    fail("non-built-in root should not be considered built-in");
  } else {
    passed("non-built-in root should not considered built-in");
  }

  return gFailCount;
}
