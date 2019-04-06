/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "nsCOMPtr.h"
#include "nsIPrefService.h"
#include "nsIX509Cert.h"
#include "nsServiceManagerUtils.h"
#include "TrustOverrideUtils.h"

// certspec (for pycert.py)
//
// issuer:ca
// subject:ca
// extension:basicConstraints:cA,
// extension:keyUsage:cRLSign,keyCertSign
// serialNumber:1
const char* kOverrideCaPem =
    "-----BEGIN CERTIFICATE-----\n"
    "MIICsjCCAZygAwIBAgIBATALBgkqhkiG9w0BAQswDTELMAkGA1UEAwwCY2EwIhgP\n"
    "MjAxNTExMjgwMDAwMDBaGA8yMDE4MDIwNTAwMDAwMFowDTELMAkGA1UEAwwCY2Ew\n"
    "ggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQC6iFGoRI4W1kH9braIBjYQ\n"
    "PTwT2erkNUq07PVoV2wke8HHJajg2B+9sZwGm24ahvJr4q9adWtqZHEIeqVap0WH\n"
    "9xzVJJwCfs1D/B5p0DggKZOrIMNJ5Nu5TMJrbA7tFYIP8X6taRqx0wI6iypB7qdw\n"
    "4A8Njf1mCyuwJJKkfbmIYXmQsVeQPdI7xeC4SB+oN9OIQ+8nFthVt2Zaqn4CkC86\n"
    "exCABiTMHGyXrZZhW7filhLAdTGjDJHdtMr3/K0dJdMJ77kXDqdo4bN7LyJvaeO0\n"
    "ipVhHe4m1iWdq5EITjbLHCQELL8Wiy/l8Y+ZFzG4s/5JI/pyUcQx1QOs2hgKNe2N\n"
    "AgMBAAGjHTAbMAwGA1UdEwQFMAMBAf8wCwYDVR0PBAQDAgEGMAsGCSqGSIb3DQEB\n"
    "CwOCAQEAchHf1yV+blE6fvS53L3DGmvxEpn9+t+xwOvWczBmLFEzUPdncakdaWlQ\n"
    "v7q81BPyjBqkYbQi15Ws81hY3dnXn8LT1QktCL9guvc3z4fMdQbRjpjcIReCYt3E\n"
    "PB22Jl2FCm6ii4XL0qDFD26WK3zMe2Uks6t55f8VeDTBGNoPp2JMsWY1Pi4vR6wK\n"
    "AY96WoXS/qrYkmMEOgFu907pApeAeE8VJzXjqMLF6/W1VN7ISnGzWQ8zKQnlp3YA\n"
    "mvWZQcD6INK8mvpZxIeu6NtHaKEXGw7tlGekmkVhapPtQZYnWcsXybRrZf5g3hOh\n"
    "JFPl8kW42VoxXL11PP5NX2ylTsJ//g==\n"
    "-----END CERTIFICATE-----";

// certspec (for pycert.py)
//
// issuer:ca
// subject:ca-intermediate
// extension:basicConstraints:cA,
// extension:keyUsage:cRLSign,keyCertSign
// subjectKey:secp384r1
// serialNumber:2
const char* kOverrideCaIntermediatePem =
    "-----BEGIN CERTIFICATE-----\n"
    "MIICFDCB/aADAgECAgECMA0GCSqGSIb3DQEBCwUAMA0xCzAJBgNVBAMMAmNhMCIY\n"
    "DzIwMTYxMTI3MDAwMDAwWhgPMjAxOTAyMDUwMDAwMDBaMBoxGDAWBgNVBAMMD2Nh\n"
    "LWludGVybWVkaWF0ZTB2MBAGByqGSM49AgEGBSuBBAAiA2IABKFockM2K1x7GInz\n"
    "eRVGFaHHP7SN7oY+AikV22COJS3ktxMtqM6Y6DFTTmqcDAsJyNY5regyBuW6gTRz\n"
    "oR+jMOBdqMluQ4P+J4c9qXEDviiIz/AC8Fr3Gh/dzIN0qm6pzqMdMBswDAYDVR0T\n"
    "BAUwAwEB/zALBgNVHQ8EBAMCAQYwDQYJKoZIhvcNAQELBQADggEBAAvDaCiJdFvr\n"
    "wlLCqTM7qB9sS9vjz3lL8zbSssUlx5fXtIQACg0zJTKUyLJf/nTlit/txhPO0Q7T\n"
    "PjKpKjE4ohYMEBW+EHs9U0/yYoxVwhjcVGVzEVsVQXzgll34t1W5rVtxof6DrH/l\n"
    "Mc9Y7vhXFWnT8jsiqXtdIXh3fi8AjCzYSAjsfQfwpHRnM8uY2GaAb8SovYiGIk2t\n"
    "CSdCBkA6V7hTeQSyLCj4JsCzX1JzMx33ebw3Z10Mq2AgRk3Uw5/L+b+PKScZXw42\n"
    "lJNniSZbH7lHr08bWhWFXVK8aD2VGUeT/FswTlx3XGYVFrbXQ8WDEe0VkA+4aN33\n"
    "+mbXPBnW8ao=\n"
    "-----END CERTIFICATE-----";

// /CN=ca
// SHA256 Fingerprint: A3:05:0C:44:CD:6D:1E:BE:A2:18:80:09:93:69:90:7F
//                     8C:E3:9F:A4:33:CB:E3:E9:3C:D1:8E:8C:89:23:1B:4A

// clang-format off
// Invocation: security/manager/tools/crtshToIdentifyingStruct/crtshToIdentifyingStruct.py -listname OverrideCaDNs -dn /tmp/overrideCa.pem
static const uint8_t CAcaDN[15] = {
  0x30, 0x0D, 0x31, 0x0B, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0C, 0x02,
  0x63, 0x61,
};
// clang-format on

static const DataAndLength OverrideCaDNs[] = {
    {CAcaDN, sizeof(CAcaDN)},
};

// clang-format off
// Invocation: security/manager/tools/crtshToIdentifyingStruct/crtshToIdentifyingStruct.py -listname OverrideCaSPKIs -spki /tmp/overrideCa.pem
static const uint8_t CAcaSPKI[294] = {
  0x30, 0x82, 0x01, 0x22, 0x30, 0x0D, 0x06, 0x09, 0x2A, 0x86, 0x48, 0x86, 0xF7,
  0x0D, 0x01, 0x01, 0x01, 0x05, 0x00, 0x03, 0x82, 0x01, 0x0F, 0x00, 0x30, 0x82,
  0x01, 0x0A, 0x02, 0x82, 0x01, 0x01, 0x00, 0xBA, 0x88, 0x51, 0xA8, 0x44, 0x8E,
  0x16, 0xD6, 0x41, 0xFD, 0x6E, 0xB6, 0x88, 0x06, 0x36, 0x10, 0x3D, 0x3C, 0x13,
  0xD9, 0xEA, 0xE4, 0x35, 0x4A, 0xB4, 0xEC, 0xF5, 0x68, 0x57, 0x6C, 0x24, 0x7B,
  0xC1, 0xC7, 0x25, 0xA8, 0xE0, 0xD8, 0x1F, 0xBD, 0xB1, 0x9C, 0x06, 0x9B, 0x6E,
  0x1A, 0x86, 0xF2, 0x6B, 0xE2, 0xAF, 0x5A, 0x75, 0x6B, 0x6A, 0x64, 0x71, 0x08,
  0x7A, 0xA5, 0x5A, 0xA7, 0x45, 0x87, 0xF7, 0x1C, 0xD5, 0x24, 0x9C, 0x02, 0x7E,
  0xCD, 0x43, 0xFC, 0x1E, 0x69, 0xD0, 0x38, 0x20, 0x29, 0x93, 0xAB, 0x20, 0xC3,
  0x49, 0xE4, 0xDB, 0xB9, 0x4C, 0xC2, 0x6B, 0x6C, 0x0E, 0xED, 0x15, 0x82, 0x0F,
  0xF1, 0x7E, 0xAD, 0x69, 0x1A, 0xB1, 0xD3, 0x02, 0x3A, 0x8B, 0x2A, 0x41, 0xEE,
  0xA7, 0x70, 0xE0, 0x0F, 0x0D, 0x8D, 0xFD, 0x66, 0x0B, 0x2B, 0xB0, 0x24, 0x92,
  0xA4, 0x7D, 0xB9, 0x88, 0x61, 0x79, 0x90, 0xB1, 0x57, 0x90, 0x3D, 0xD2, 0x3B,
  0xC5, 0xE0, 0xB8, 0x48, 0x1F, 0xA8, 0x37, 0xD3, 0x88, 0x43, 0xEF, 0x27, 0x16,
  0xD8, 0x55, 0xB7, 0x66, 0x5A, 0xAA, 0x7E, 0x02, 0x90, 0x2F, 0x3A, 0x7B, 0x10,
  0x80, 0x06, 0x24, 0xCC, 0x1C, 0x6C, 0x97, 0xAD, 0x96, 0x61, 0x5B, 0xB7, 0xE2,
  0x96, 0x12, 0xC0, 0x75, 0x31, 0xA3, 0x0C, 0x91, 0xDD, 0xB4, 0xCA, 0xF7, 0xFC,
  0xAD, 0x1D, 0x25, 0xD3, 0x09, 0xEF, 0xB9, 0x17, 0x0E, 0xA7, 0x68, 0xE1, 0xB3,
  0x7B, 0x2F, 0x22, 0x6F, 0x69, 0xE3, 0xB4, 0x8A, 0x95, 0x61, 0x1D, 0xEE, 0x26,
  0xD6, 0x25, 0x9D, 0xAB, 0x91, 0x08, 0x4E, 0x36, 0xCB, 0x1C, 0x24, 0x04, 0x2C,
  0xBF, 0x16, 0x8B, 0x2F, 0xE5, 0xF1, 0x8F, 0x99, 0x17, 0x31, 0xB8, 0xB3, 0xFE,
  0x49, 0x23, 0xFA, 0x72, 0x51, 0xC4, 0x31, 0xD5, 0x03, 0xAC, 0xDA, 0x18, 0x0A,
  0x35, 0xED, 0x8D, 0x02, 0x03, 0x01, 0x00, 0x01,
};
// clang-format on

static const DataAndLength OverrideCaSPKIs[] = {
    {CAcaSPKI, sizeof(CAcaSPKI)},
};

static mozilla::UniqueCERTCertificate CertFromString(const char* aPem) {
  nsCOMPtr<nsIX509Cert> cert =
      nsNSSCertificate::ConstructFromDER(const_cast<char*>(aPem), strlen(aPem));
  if (!cert) {
    return nullptr;
  }

  mozilla::UniqueCERTCertificate nssCert(cert->GetCert());
  return nssCert;
}

class psm_TrustOverrideTest : public ::testing::Test {
 protected:
  void SetUp() override {
    nsCOMPtr<nsIPrefBranch> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID));
    ASSERT_TRUE(prefs != nullptr)
    << "couldn't get nsIPrefBranch";

    // When PSM initializes, it attempts to get some localized strings.
    // As a result, Android flips out if this isn't set.
    nsresult rv = prefs->SetBoolPref("intl.locale.matchOS", true);
    ASSERT_TRUE(NS_SUCCEEDED(rv))
    << "couldn't set pref 'intl.locale.matchOS'";

    nsCOMPtr<nsIX509CertDB> certdb(do_GetService(NS_X509CERTDB_CONTRACTID));
    ASSERT_TRUE(certdb != nullptr)
    << "couldn't get certdb";
  }
};

TEST_F(psm_TrustOverrideTest, CheckCertDNIsInList) {
  mozilla::UniqueCERTCertificate caObj = CertFromString(kOverrideCaPem);
  ASSERT_TRUE(caObj != nullptr)
  << "Should have parsed";
  mozilla::UniqueCERTCertificate intObj =
      CertFromString(kOverrideCaIntermediatePem);
  ASSERT_TRUE(intObj != nullptr)
  << "Should have parsed";

  EXPECT_TRUE(CertDNIsInList(caObj.get(), OverrideCaDNs))
      << "CA should be in the DN list";
  EXPECT_FALSE(CertDNIsInList(intObj.get(), OverrideCaDNs))
      << "Int should not be in the DN list";
}

TEST_F(psm_TrustOverrideTest, CheckCertSPKIIsInList) {
  mozilla::UniqueCERTCertificate caObj = CertFromString(kOverrideCaPem);
  ASSERT_TRUE(caObj != nullptr)
  << "Should have parsed";
  mozilla::UniqueCERTCertificate intObj =
      CertFromString(kOverrideCaIntermediatePem);
  ASSERT_TRUE(intObj != nullptr)
  << "Should have parsed";

  EXPECT_TRUE(CertSPKIIsInList(caObj.get(), OverrideCaSPKIs))
      << "CA should be in the SPKI list";
  EXPECT_FALSE(CertSPKIIsInList(intObj.get(), OverrideCaSPKIs))
      << "Int should not be in the SPKI list";
}
