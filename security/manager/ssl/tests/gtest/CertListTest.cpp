/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "nsCOMPtr.h"
#include "nsIPrefService.h"
#include "nsISimpleEnumerator.h"
#include "nsIX509Cert.h"
#include "nsIX509CertDB.h"
#include "nsIX509CertList.h"
#include "nsNSSCertificate.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"

// certspec (for pycert.py)
//
// issuer:ca
// subject:ca
// extension:basicConstraints:cA,
// extension:keyUsage:cRLSign,keyCertSign
// serialNumber:1
const char* kCaPem =
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
// serialNumber:2
const char* kCaIntermediatePem =
    "-----BEGIN CERTIFICATE-----\n"
    "MIICvzCCAamgAwIBAgIBAjALBgkqhkiG9w0BAQswDTELMAkGA1UEAwwCY2EwIhgP\n"
    "MjAxNTExMjgwMDAwMDBaGA8yMDE4MDIwNTAwMDAwMFowGjEYMBYGA1UEAwwPY2Et\n"
    "aW50ZXJtZWRpYXRlMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuohR\n"
    "qESOFtZB/W62iAY2ED08E9nq5DVKtOz1aFdsJHvBxyWo4NgfvbGcBptuGobya+Kv\n"
    "WnVramRxCHqlWqdFh/cc1SScAn7NQ/weadA4ICmTqyDDSeTbuUzCa2wO7RWCD/F+\n"
    "rWkasdMCOosqQe6ncOAPDY39ZgsrsCSSpH25iGF5kLFXkD3SO8XguEgfqDfTiEPv\n"
    "JxbYVbdmWqp+ApAvOnsQgAYkzBxsl62WYVu34pYSwHUxowyR3bTK9/ytHSXTCe+5\n"
    "Fw6naOGzey8ib2njtIqVYR3uJtYlnauRCE42yxwkBCy/Fosv5fGPmRcxuLP+SSP6\n"
    "clHEMdUDrNoYCjXtjQIDAQABox0wGzAMBgNVHRMEBTADAQH/MAsGA1UdDwQEAwIB\n"
    "BjALBgkqhkiG9w0BAQsDggEBAC0ys8UOmYgvH5rrTeV6u79ocHqdQFwdmR7/4d08\n"
    "i3asC7b70dw0ehA5vi4cq5mwBvQOGZq4wvsR4jSJW0+0hjWL1dr2M6VxmCfjdqhU\n"
    "NQHPlY6y7lLfYQbFfUHX99ZgygJjdmmm7H8MBP4UgPb8jl6Xq53FgYykiX/qPmfb\n"
    "pzpOFHDi+Tk67DLCvPz03UUDYNx1H0OhRimj0DWhdYGUg2DHfLQkOEYvrYG4wYB8\n"
    "AB/0hrG51yFsuXrzhYcinTKby11Qk6PjnOQ/aZvK00Jffep/RHs8lIOWty9SarMG\n"
    "oNSECn+6I9AgStJdo6LuP1QPyrQe3DZtAHhRJAPAoU7BSqM=\n"
    "-----END CERTIFICATE-----";

// certspec (for pycert.py)
//
// issuer:ca-intermediate
// subject:ca-second-intermediate
// extension:basicConstraints:cA,
// extension:keyUsage:cRLSign,keyCertSign
// serialNumber:3
const char* kCaSecondIntermediatePem =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIC0zCCAb2gAwIBAgIBAzALBgkqhkiG9w0BAQswGjEYMBYGA1UEAwwPY2EtaW50\n"
    "ZXJtZWRpYXRlMCIYDzIwMTUxMTI4MDAwMDAwWhgPMjAxODAyMDUwMDAwMDBaMCEx\n"
    "HzAdBgNVBAMMFmNhLXNlY29uZC1pbnRlcm1lZGlhdGUwggEiMA0GCSqGSIb3DQEB\n"
    "AQUAA4IBDwAwggEKAoIBAQC6iFGoRI4W1kH9braIBjYQPTwT2erkNUq07PVoV2wk\n"
    "e8HHJajg2B+9sZwGm24ahvJr4q9adWtqZHEIeqVap0WH9xzVJJwCfs1D/B5p0Dgg\n"
    "KZOrIMNJ5Nu5TMJrbA7tFYIP8X6taRqx0wI6iypB7qdw4A8Njf1mCyuwJJKkfbmI\n"
    "YXmQsVeQPdI7xeC4SB+oN9OIQ+8nFthVt2Zaqn4CkC86exCABiTMHGyXrZZhW7fi\n"
    "lhLAdTGjDJHdtMr3/K0dJdMJ77kXDqdo4bN7LyJvaeO0ipVhHe4m1iWdq5EITjbL\n"
    "HCQELL8Wiy/l8Y+ZFzG4s/5JI/pyUcQx1QOs2hgKNe2NAgMBAAGjHTAbMAwGA1Ud\n"
    "EwQFMAMBAf8wCwYDVR0PBAQDAgEGMAsGCSqGSIb3DQEBCwOCAQEAaK6K7/0Y+PkG\n"
    "MQJjumTlt6XUQjQ3Y6zuSOMlZ1wmJoBqWabYhJ4qXfcSMQiw+kZ+mQTFk+IdurGC\n"
    "RHrAKwDGNRqmjnQ56qjwHNTTxhJozP09vBCgs3fIQQY/Nq/uISoQvOZmoIriFZf6\n"
    "8czHMlj1vIC6zp4XHWdqkQ7aF4YFsTfM0kBPrm0Y6Nn0VKzWNdmaIs/X5OcR6ZAG\n"
    "zGN9UZV+ZftcfdqI0XSVCVRAK5MeEa+twLr5PE/Nl7/Ig/tUJMWGSbcrWRZQTXQu\n"
    "Rx7CSKcoatyMhJOd2YT4BvoijEJCxTKWMJzFe2uZAphQHUlVmE9IbUQM0T1N6RNd\n"
    "1dH4o4UeuQ==\n"
    "-----END CERTIFICATE-----";

// certspec (for pycert.py)
//
// issuer:ca-second-intermediate
// subject:ee
const char* kEePem =
    "-----BEGIN CERTIFICATE-----\n"
    "MIICujCCAaSgAwIBAgIUMy8NE67P/4jkaCra7rOVVvX4+GswCwYJKoZIhvcNAQEL\n"
    "MCExHzAdBgNVBAMMFmNhLXNlY29uZC1pbnRlcm1lZGlhdGUwIhgPMjAxNTExMjgw\n"
    "MDAwMDBaGA8yMDE4MDIwNTAwMDAwMFowDTELMAkGA1UEAwwCZWUwggEiMA0GCSqG\n"
    "SIb3DQEBAQUAA4IBDwAwggEKAoIBAQC6iFGoRI4W1kH9braIBjYQPTwT2erkNUq0\n"
    "7PVoV2wke8HHJajg2B+9sZwGm24ahvJr4q9adWtqZHEIeqVap0WH9xzVJJwCfs1D\n"
    "/B5p0DggKZOrIMNJ5Nu5TMJrbA7tFYIP8X6taRqx0wI6iypB7qdw4A8Njf1mCyuw\n"
    "JJKkfbmIYXmQsVeQPdI7xeC4SB+oN9OIQ+8nFthVt2Zaqn4CkC86exCABiTMHGyX\n"
    "rZZhW7filhLAdTGjDJHdtMr3/K0dJdMJ77kXDqdo4bN7LyJvaeO0ipVhHe4m1iWd\n"
    "q5EITjbLHCQELL8Wiy/l8Y+ZFzG4s/5JI/pyUcQx1QOs2hgKNe2NAgMBAAEwCwYJ\n"
    "KoZIhvcNAQELA4IBAQCE5V5YiFPtbb1dOCIMGC5X/6kfQkQmIfvEZIol0MRXmP4g\n"
    "CsOPbTI+BNxYVNk5RHIlr+6e0d8TNiABem4FZK3kea4ugN8ez3IsK7ug7qdrooNA\n"
    "MiHOvrLmAw2nQWexdDRf7OPeVj03BwELzGTOGPjAqDktTsK57OfXyFTm9nl75WQo\n"
    "+EWX+CdV4L1o2rgABvSiMnMdycftCC73Hr/3ypADqY7nDrKpxYdrGgzAQvx3DjPv\n"
    "b7nBKH/gXg3kzoWpeQmJYPl9Vd+DvGljS5i71oLbvCwlDX7ZswGcvb8pQ7Tni5HA\n"
    "VYpAYLokxIDFnyVT9oCACJuJ5LvpBBrhd0+1uUPE\n"
    "-----END CERTIFICATE-----";

class psm_CertList : public ::testing::Test {
 protected:
  void SetUp() override {
    nsCOMPtr<nsIPrefBranch> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID));
    ASSERT_TRUE(prefs)
    << "couldn't get nsIPrefBranch";

    // When PSM initializes, it attempts to get some localized strings.
    // As a result, Android flips out if this isn't set.
    nsresult rv = prefs->SetBoolPref("intl.locale.matchOS", true);
    ASSERT_TRUE(NS_SUCCEEDED(rv))
    << "couldn't set pref 'intl.locale.matchOS'";

    nsCOMPtr<nsIX509CertDB> certdb(do_GetService(NS_X509CERTDB_CONTRACTID));
    ASSERT_TRUE(certdb)
    << "couldn't get certdb";
  }
};

static nsresult AddCertFromStringToList(const char* aPem,
                                        nsIX509CertList* aCertList) {
  if (!aCertList) {
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<nsIX509Cert> cert;
  cert =
      nsNSSCertificate::ConstructFromDER(const_cast<char*>(aPem), strlen(aPem));
  if (!cert) {
    return NS_ERROR_FAILURE;
  }

  return aCertList->AddCert(cert);
}

static int CountCertsInList(nsCOMPtr<nsIX509CertList>& aCertList) {
  int counter = 0;
  RefPtr<nsNSSCertList> certList = aCertList->GetCertList();
  certList->ForEachCertificateInChain(
      [&counter](nsCOMPtr<nsIX509Cert> aCert, bool aHasMore, bool& aContinue) {
        counter++;
        return NS_OK;
      });

  return counter;
}

TEST_F(psm_CertList, TestInvalidSegmenting) {
  RefPtr<nsNSSCertList> certList = new nsNSSCertList();

  nsCOMPtr<nsIX509Cert> rootCert;
  nsCOMPtr<nsIX509CertList> intCerts;
  nsCOMPtr<nsIX509Cert> eeCert;
  nsresult rv = certList->SegmentCertificateChain(rootCert, intCerts, eeCert);
  ASSERT_EQ(rv, NS_ERROR_INVALID_ARG) << "Empty lists can't be segmented";

  rv = AddCertFromStringToList(kCaPem, certList);
  ASSERT_EQ(rv, NS_OK) << "Should have loaded OK";

  // We should need to clear out the in-vars, but let's make sure behavior is OK
  rv = certList->SegmentCertificateChain(rootCert, intCerts, eeCert);
  ASSERT_EQ(rv, NS_ERROR_UNEXPECTED)
      << "Don't permit already-filled-in arguments";

  intCerts = nullptr;
  rootCert = nullptr;
  eeCert = nullptr;

  rv = certList->SegmentCertificateChain(rootCert, intCerts, eeCert);
  ASSERT_EQ(rv, NS_ERROR_INVALID_ARG) << "Lists of one can't be segmented";
}

TEST_F(psm_CertList, TestValidSegmenting) {
  RefPtr<nsNSSCertList> certList = new nsNSSCertList();

  nsresult rv = AddCertFromStringToList(kEePem, certList);
  ASSERT_EQ(rv, NS_OK) << "Should have loaded OK";
  rv = AddCertFromStringToList(kCaSecondIntermediatePem, certList);
  ASSERT_EQ(rv, NS_OK) << "Should have loaded OK";

  nsCOMPtr<nsIX509Cert> rootCert;
  nsCOMPtr<nsIX509CertList> intCerts;
  nsCOMPtr<nsIX509Cert> eeCert;

  rv = certList->SegmentCertificateChain(rootCert, intCerts, eeCert);
  ASSERT_EQ(rv, NS_OK) << "Should have segmented OK";
  ASSERT_TRUE(rootCert)
  << "Root cert should be filled in";
  ASSERT_TRUE(eeCert)
  << "End entity cert should be filled in";
  ASSERT_EQ(CountCertsInList(intCerts), 0)
      << "There should be no intermediates";

  bool selfSigned;
  ASSERT_TRUE(NS_SUCCEEDED(rootCert->GetIsSelfSigned(&selfSigned)))
  << "Getters should work.";
  ASSERT_FALSE(selfSigned)
  << "Roots are self signed, but this was ca-second-intermediate";

  nsAutoString rootCn;
  ASSERT_TRUE(NS_SUCCEEDED(rootCert->GetCommonName(rootCn)))
  << "Getters should work.";
  ASSERT_TRUE(rootCn.EqualsLiteral("ca-second-intermediate"))
  << "Second Intermediate CN should match";

  rv = AddCertFromStringToList(kCaIntermediatePem, certList);
  ASSERT_EQ(rv, NS_OK) << "Should have loaded OK";

  intCerts = nullptr;
  rootCert = nullptr;
  eeCert = nullptr;
  rv = certList->SegmentCertificateChain(rootCert, intCerts, eeCert);
  ASSERT_EQ(rv, NS_OK) << "Should have segmented OK";

  ASSERT_TRUE(rootCert)
  << "Root cert should be filled in";
  ASSERT_TRUE(eeCert)
  << "End entity cert should be filled in";
  ASSERT_EQ(CountCertsInList(intCerts), 1)
      << "There should be one intermediate";

  rv = AddCertFromStringToList(kCaPem, certList);
  ASSERT_EQ(rv, NS_OK) << "Should have loaded OK";

  intCerts = nullptr;
  rootCert = nullptr;
  eeCert = nullptr;
  rv = certList->SegmentCertificateChain(rootCert, intCerts, eeCert);
  ASSERT_EQ(rv, NS_OK) << "Should have segmented OK";

  ASSERT_TRUE(rootCert)
  << "Root cert should be filled in";
  ASSERT_TRUE(eeCert)
  << "End entity cert should be filled in";
  ASSERT_EQ(CountCertsInList(intCerts), 2)
      << "There should be two intermediates";

  ASSERT_TRUE(NS_SUCCEEDED(rootCert->GetIsSelfSigned(&selfSigned)))
  << "Getters should work.";
  ASSERT_TRUE(selfSigned)
  << "Roots are self signed";

  ASSERT_TRUE(NS_SUCCEEDED(rootCert->GetCommonName(rootCn)))
  << "Getters should work.";
  ASSERT_TRUE(rootCn.EqualsLiteral("ca"))
  << "Root CN should match";

  nsAutoString eeCn;
  ASSERT_TRUE(NS_SUCCEEDED(eeCert->GetCommonName(eeCn)))
  << "Getters should work.";
  ASSERT_TRUE(eeCn.EqualsLiteral("ee"))
  << "EE CN should match";

  rv = intCerts->GetCertList()->ForEachCertificateInChain(
      [](nsCOMPtr<nsIX509Cert> aCert, bool aHasMore, bool& aContinue) {
        nsAutoString cn;
        nsresult rv = aCert->GetCommonName(cn);
        if (NS_FAILED(rv)) {
          return rv;
        }

        if (aHasMore) {
          if (!cn.EqualsLiteral("ca-second-intermediate")) {
            return NS_ERROR_FAILURE;
          }
        } else {
          if (!cn.EqualsLiteral("ca-intermediate")) {
            return NS_ERROR_FAILURE;
          }
        }
        return NS_OK;
      });
  ASSERT_EQ(rv, NS_OK) << "Should have looped OK.";
}

TEST_F(psm_CertList, TestForEach) {
  RefPtr<nsNSSCertList> certList = new nsNSSCertList();

  bool called = false;
  nsresult rv = certList->ForEachCertificateInChain(
      [&called](nsCOMPtr<nsIX509Cert> aCert, bool aHasMore, bool& aContinue) {
        called = true;
        return NS_OK;
      });
  ASSERT_EQ(rv, NS_OK) << "Should have iterated OK";
  ASSERT_FALSE(called)
  << "There are no certificates in this chain, it shouldn't be called";

  // Add two certificates
  rv = AddCertFromStringToList(kCaPem, certList);
  ASSERT_EQ(rv, NS_OK) << "Should have loaded OK";
  rv = AddCertFromStringToList(kCaIntermediatePem, certList);
  ASSERT_EQ(rv, NS_OK) << "Should have loaded OK";

  int counter = 0;
  rv = certList->ForEachCertificateInChain(
      [&counter](nsCOMPtr<nsIX509Cert> aCert, bool aHasMore,
                 bool& aContinue) -> nsresult {
        if (!aCert) {
          GTEST_NONFATAL_FAILURE_("Unexpected arguments");
          return NS_ERROR_FAILURE;
        }

        nsAutoString cn;
        nsresult rv = aCert->GetCommonName(cn);
        if (NS_FAILED(rv)) {
          return rv;
        }

        switch (counter) {
          case 0:
            if (!aHasMore) {
              return NS_ERROR_FAILURE;
            }

            if (!cn.EqualsLiteral("ca")) {
              return NS_ERROR_FAILURE;
            }
            break;
          case 1:
            if (aHasMore) {
              return NS_ERROR_FAILURE;
            }

            if (!cn.EqualsLiteral("ca-intermediate")) {
              return NS_ERROR_FAILURE;
            }
            break;
          default:
            GTEST_NONFATAL_FAILURE_("Unexpected count");
            return NS_ERROR_UNEXPECTED;
        }
        counter++;
        return NS_OK;
      });
  ASSERT_EQ(rv, NS_OK) << "Should have iterated OK";
}

TEST_F(psm_CertList, TestForEachContinueSafety) {
  RefPtr<nsNSSCertList> certList = new nsNSSCertList();

  // Add two certificates
  nsresult rv = AddCertFromStringToList(kCaSecondIntermediatePem, certList);
  ASSERT_EQ(rv, NS_OK) << "Should have loaded OK";
  rv = AddCertFromStringToList(kEePem, certList);
  ASSERT_EQ(rv, NS_OK) << "Should have loaded OK";

  int counter = 0;
  rv = certList->ForEachCertificateInChain(
      [&counter](nsCOMPtr<nsIX509Cert> aCert, bool aHasMore, bool& aContinue) {
        counter++;
        aContinue = true;  // Try to keep it going
        return NS_OK;
      });
  ASSERT_EQ(counter, 2)
      << "There should have been only two iterations regardless!";
  ASSERT_EQ(rv, NS_OK) << "Should complete OK.";
}

TEST_F(psm_CertList, TestForEachStopEarly) {
  RefPtr<nsNSSCertList> certList = new nsNSSCertList();

  // Add two certificates
  nsresult rv = AddCertFromStringToList(kCaSecondIntermediatePem, certList);
  ASSERT_EQ(rv, NS_OK) << "Should have loaded OK";
  rv = AddCertFromStringToList(kEePem, certList);
  ASSERT_EQ(rv, NS_OK) << "Should have loaded OK";

  int counter = 0;
  rv = certList->ForEachCertificateInChain(
      [&counter](nsCOMPtr<nsIX509Cert> aCert, bool aHasMore, bool& aContinue) {
        counter++;
        aContinue = false;
        return NS_OK;
      });
  ASSERT_EQ(counter, 1) << "There should have been only the one call";
  ASSERT_EQ(rv, NS_OK) << "Should complete OK.";
}

TEST_F(psm_CertList, TestForEachStopOnError) {
  RefPtr<nsNSSCertList> certList = new nsNSSCertList();

  // Add two certificates
  nsresult rv = AddCertFromStringToList(kCaSecondIntermediatePem, certList);
  ASSERT_EQ(rv, NS_OK) << "Should have loaded OK";
  rv = AddCertFromStringToList(kEePem, certList);
  ASSERT_EQ(rv, NS_OK) << "Should have loaded OK";

  int counter = 0;
  rv = certList->ForEachCertificateInChain(
      [&counter](nsCOMPtr<nsIX509Cert> aCert, bool aHasMore, bool& aContinue) {
        counter++;
        return NS_ERROR_FAILURE;
      });
  ASSERT_EQ(counter, 1) << "There should have been only the one call";
  ASSERT_EQ(rv, NS_ERROR_FAILURE) << "Should propagate the error.";
}

TEST_F(psm_CertList, TestGetRootCertificateChainTwo) {
  RefPtr<nsNSSCertList> certList = new nsNSSCertList();

  nsresult rv = AddCertFromStringToList(kCaIntermediatePem, certList);
  ASSERT_EQ(NS_OK, rv) << "Should have loaded OK";
  rv = AddCertFromStringToList(kCaPem, certList);
  ASSERT_EQ(NS_OK, rv) << "Should have loaded OK";

  nsCOMPtr<nsIX509Cert> rootCert;
  rv = certList->GetRootCertificate(rootCert);
  EXPECT_EQ(NS_OK, rv) << "Should have fetched the root OK";
  ASSERT_TRUE(rootCert)
  << "Root cert should be filled in";

  bool selfSigned;
  EXPECT_TRUE(NS_SUCCEEDED(rootCert->GetIsSelfSigned(&selfSigned)))
      << "Getters should work.";
  EXPECT_TRUE(selfSigned) << "Roots are self signed";

  nsAutoString rootCn;
  EXPECT_TRUE(NS_SUCCEEDED(rootCert->GetCommonName(rootCn)))
      << "Getters should work.";
  EXPECT_TRUE(rootCn.EqualsLiteral("ca")) << "Root CN should match";

  // Re-fetch and ensure we get the same certificate.
  nsCOMPtr<nsIX509Cert> rootCertRepeat;
  rv = certList->GetRootCertificate(rootCertRepeat);
  EXPECT_EQ(NS_OK, rv) << "Should have fetched the root OK the second time";
  ASSERT_TRUE(rootCertRepeat)
  << "Root cert should still be filled in";

  nsAutoString rootRepeatCn;
  EXPECT_TRUE(NS_SUCCEEDED(rootCertRepeat->GetCommonName(rootRepeatCn)))
      << "Getters should work.";
  EXPECT_TRUE(rootRepeatCn.EqualsLiteral("ca")) << "Root CN should still match";
}

TEST_F(psm_CertList, TestGetRootCertificateChainFour) {
  RefPtr<nsNSSCertList> certList = new nsNSSCertList();

  nsresult rv = AddCertFromStringToList(kEePem, certList);
  ASSERT_EQ(NS_OK, rv) << "Should have loaded OK";
  rv = AddCertFromStringToList(kCaSecondIntermediatePem, certList);
  ASSERT_EQ(NS_OK, rv) << "Should have loaded OK";
  rv = AddCertFromStringToList(kCaIntermediatePem, certList);
  ASSERT_EQ(NS_OK, rv) << "Should have loaded OK";
  rv = AddCertFromStringToList(kCaPem, certList);
  ASSERT_EQ(NS_OK, rv) << "Should have loaded OK";

  nsCOMPtr<nsIX509Cert> rootCert;
  rv = certList->GetRootCertificate(rootCert);
  EXPECT_EQ(NS_OK, rv) << "Should have again fetched the root OK";
  ASSERT_TRUE(rootCert)
  << "Root cert should be filled in";

  bool selfSigned;
  EXPECT_TRUE(NS_SUCCEEDED(rootCert->GetIsSelfSigned(&selfSigned)))
      << "Getters should work.";
  EXPECT_TRUE(selfSigned) << "Roots are self signed";

  nsAutoString rootCn;
  EXPECT_TRUE(NS_SUCCEEDED(rootCert->GetCommonName(rootCn)))
      << "Getters should work.";
  EXPECT_TRUE(rootCn.EqualsLiteral("ca")) << "Root CN should match";
}

TEST_F(psm_CertList, TestGetRootCertificateChainEmpty) {
  RefPtr<nsNSSCertList> certList = new nsNSSCertList();

  nsCOMPtr<nsIX509Cert> rootCert;
  nsresult rv = certList->GetRootCertificate(rootCert);
  EXPECT_EQ(NS_OK, rv) << "Should have again fetched the root OK";
  EXPECT_FALSE(rootCert) << "Root cert should be empty";
}
