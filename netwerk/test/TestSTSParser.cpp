/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//#define MOZILLA_INTERNAL_API

#include "TestHarness.h"
#include <stdio.h>
#include "plstr.h"
#include "nsNetUtil.h"
#include "nsStringGlue.h"
#include "nsISiteSecurityService.h"
#include "nsIPermissionManager.h"

#define EXPECT_SUCCESS(rv, ...) \
  PR_BEGIN_MACRO \
  if (NS_FAILED(rv)) { \
    fail(__VA_ARGS__); \
    return false; \
  } \
  PR_END_MACRO


#define EXPECT_FAILURE(rv, ...) \
  PR_BEGIN_MACRO \
  if (NS_SUCCEEDED(rv)) { \
    fail(__VA_ARGS__); \
    return false; \
  } \
  PR_END_MACRO

#define REQUIRE_EQUAL(a, b, ...) \
  PR_BEGIN_MACRO \
  if (a != b) { \
    fail(__VA_ARGS__); \
    return false; \
  } \
  PR_END_MACRO

bool
TestSuccess(const char* hdr, bool extraTokens,
            uint64_t expectedMaxAge, bool expectedIncludeSubdomains,
            nsISiteSecurityService* sss,
            nsIPermissionManager* pm)
{
  nsCOMPtr<nsIURI> dummyUri;
  nsresult rv = NS_NewURI(getter_AddRefs(dummyUri), "https://foo.com/bar.html");
  EXPECT_SUCCESS(rv, "Failed to create URI");

  uint64_t maxAge = 0;
  bool includeSubdomains = false;
  rv = sss->ProcessHeader(nsISiteSecurityService::HEADER_HSTS, dummyUri, hdr,
                          0, &maxAge, &includeSubdomains);
  EXPECT_SUCCESS(rv, "Failed to process valid header: %s", hdr);

  REQUIRE_EQUAL(maxAge, expectedMaxAge, "Did not correctly parse maxAge");
  REQUIRE_EQUAL(includeSubdomains, expectedIncludeSubdomains, "Did not correctly parse presence/absence of includeSubdomains");

  if (extraTokens) {
    REQUIRE_EQUAL(rv, NS_SUCCESS_LOSS_OF_INSIGNIFICANT_DATA,
                  "Extra tokens were expected when parsing, but were not encountered.");
  } else {
    REQUIRE_EQUAL(rv, NS_OK, "Unexpected tokens found during parsing.");
  }

  passed(hdr);
  return true;
}

bool TestFailure(const char* hdr,
                 nsISiteSecurityService* sss,
                 nsIPermissionManager* pm)
{
  nsCOMPtr<nsIURI> dummyUri;
  nsresult rv = NS_NewURI(getter_AddRefs(dummyUri), "https://foo.com/bar.html");
  EXPECT_SUCCESS(rv, "Failed to create URI");

  rv = sss->ProcessHeader(nsISiteSecurityService::HEADER_HSTS, dummyUri, hdr,
                          0, NULL, NULL);
  EXPECT_FAILURE(rv, "Parsed invalid header: %s", hdr);
  passed(hdr);
  return true;
}


int
main(int32_t argc, char *argv[])
{
    nsresult rv;
    ScopedXPCOM xpcom("STS Parser Tests");
    if (xpcom.failed())
      return -1;
    // Initialize a profile folder to ensure a clean shutdown.
    nsCOMPtr<nsIFile> profile = xpcom.GetProfileDirectory();
    if (!profile) {
      fail("Couldn't get the profile directory.");
      return -1;
    }

    // grab handle to the service
    nsCOMPtr<nsISiteSecurityService> sss;
    sss = do_GetService("@mozilla.org/ssservice;1", &rv);
    NS_ENSURE_SUCCESS(rv, -1);

    nsCOMPtr<nsIPermissionManager> pm;
    pm = do_GetService("@mozilla.org/permissionmanager;1", &rv);
    NS_ENSURE_SUCCESS(rv, -1);

    int rv0, rv1;

    nsTArray<bool> rvs(24);

    // *** parsing tests
    printf("*** Attempting to parse valid STS headers ...\n");

    // SHOULD SUCCEED:
    rvs.AppendElement(TestSuccess("max-age=100", false, 100, false, sss, pm));
    rvs.AppendElement(TestSuccess("max-age  =100", false, 100, false, sss, pm));
    rvs.AppendElement(TestSuccess(" max-age=100", false, 100, false, sss, pm));
    rvs.AppendElement(TestSuccess("max-age = 100 ", false, 100, false, sss, pm));
    rvs.AppendElement(TestSuccess("max-age = \"100\" ", false, 100, false, sss, pm));
    rvs.AppendElement(TestSuccess("max-age=\"100\"", false, 100, false, sss, pm));
    rvs.AppendElement(TestSuccess(" max-age =\"100\" ", false, 100, false, sss, pm));
    rvs.AppendElement(TestSuccess("\tmax-age\t=\t\"100\"\t", false, 100, false, sss, pm));
    rvs.AppendElement(TestSuccess("max-age  =       100             ", false, 100, false, sss, pm));

    rvs.AppendElement(TestSuccess("maX-aGe=100", false, 100, false, sss, pm));
    rvs.AppendElement(TestSuccess("MAX-age  =100", false, 100, false, sss, pm));
    rvs.AppendElement(TestSuccess("max-AGE=100", false, 100, false, sss, pm));
    rvs.AppendElement(TestSuccess("Max-Age = 100 ", false, 100, false, sss, pm));
    rvs.AppendElement(TestSuccess("MAX-AGE = 100 ", false, 100, false, sss, pm));

    rvs.AppendElement(TestSuccess("max-age=100;includeSubdomains", false, 100, true, sss, pm));
    rvs.AppendElement(TestSuccess("max-age=100\t; includeSubdomains", false, 100, true, sss, pm));
    rvs.AppendElement(TestSuccess(" max-age=100; includeSubdomains", false, 100, true, sss, pm));
    rvs.AppendElement(TestSuccess("max-age = 100 ; includeSubdomains", false, 100, true, sss, pm));
    rvs.AppendElement(TestSuccess("max-age  =       100             ; includeSubdomains", false, 100, true, sss, pm));

    rvs.AppendElement(TestSuccess("maX-aGe=100; includeSUBDOMAINS", false, 100, true, sss, pm));
    rvs.AppendElement(TestSuccess("MAX-age  =100; includeSubDomains", false, 100, true, sss, pm));
    rvs.AppendElement(TestSuccess("max-AGE=100; iNcLuDeSuBdoMaInS", false, 100, true, sss, pm));
    rvs.AppendElement(TestSuccess("Max-Age = 100; includesubdomains ", false, 100, true, sss, pm));
    rvs.AppendElement(TestSuccess("INCLUDESUBDOMAINS;MaX-AgE = 100 ", false, 100, true, sss, pm));
    // Turns out, the actual directive is entirely optional (hence the
    // trailing semicolon)
    rvs.AppendElement(TestSuccess("max-age=100;includeSubdomains;", true, 100, true, sss, pm));

    // these are weird tests, but are testing that some extended syntax is
    // still allowed (but it is ignored)
    rvs.AppendElement(TestSuccess("max-age=100 ; includesubdomainsSomeStuff", true, 100, false, sss, pm));
    rvs.AppendElement(TestSuccess("\r\n\t\t    \tcompletelyUnrelated = foobar; max-age= 34520103    \t \t; alsoUnrelated;asIsThis;\tincludeSubdomains\t\t \t", true, 34520103, true, sss, pm));
    rvs.AppendElement(TestSuccess("max-age=100; unrelated=\"quoted \\\"thingy\\\"\"", true, 100, false, sss, pm));

    rv0 = rvs.Contains(false) ? 1 : 0;
    if (rv0 == 0)
      passed("Successfully Parsed STS headers with mixed case and LWS");

    rvs.Clear();

    // SHOULD FAIL:
    printf("*** Attempting to parse invalid STS headers (should not parse)...\n");
    // invalid max-ages
    rvs.AppendElement(TestFailure("max-age", sss, pm));
    rvs.AppendElement(TestFailure("max-age ", sss, pm));
    rvs.AppendElement(TestFailure("max-age=p", sss, pm));
    rvs.AppendElement(TestFailure("max-age=*1p2", sss, pm));
    rvs.AppendElement(TestFailure("max-age=.20032", sss, pm));
    rvs.AppendElement(TestFailure("max-age=!20032", sss, pm));
    rvs.AppendElement(TestFailure("max-age==20032", sss, pm));

    // invalid headers
    rvs.AppendElement(TestFailure("foobar", sss, pm));
    rvs.AppendElement(TestFailure("maxage=100", sss, pm));
    rvs.AppendElement(TestFailure("maxa-ge=100", sss, pm));
    rvs.AppendElement(TestFailure("max-ag=100", sss, pm));
    rvs.AppendElement(TestFailure("includesubdomains", sss, pm));
    rvs.AppendElement(TestFailure(";", sss, pm));
    rvs.AppendElement(TestFailure("max-age=\"100", sss, pm));
    // The max-age directive here doesn't conform to the spec, so it MUST
    // be ignored. Consequently, the REQUIRED max-age directive is not
    // present in this header, and so it is invalid.
    rvs.AppendElement(TestFailure("max-age=100, max-age=200; includeSubdomains", sss, pm));
    rvs.AppendElement(TestFailure("max-age=100 includesubdomains", sss, pm));
    rvs.AppendElement(TestFailure("max-age=100 bar foo", sss, pm));
    rvs.AppendElement(TestFailure("max-age=100randomstuffhere", sss, pm));
    // All directives MUST appear only once in an STS header field.
    rvs.AppendElement(TestFailure("max-age=100; max-age=200", sss, pm));
    rvs.AppendElement(TestFailure("includeSubdomains; max-age=200; includeSubdomains", sss, pm));
    rvs.AppendElement(TestFailure("max-age=200; includeSubdomains; includeSubdomains", sss, pm));
    // The includeSubdomains directive is valueless.
    rvs.AppendElement(TestFailure("max-age=100; includeSubdomains=unexpected", sss, pm));
    // LWS must have at least one space or horizontal tab
    rvs.AppendElement(TestFailure("\r\nmax-age=200", sss, pm));

    rv1 = rvs.Contains(false) ? 1 : 0;
    if (rv1 == 0)
      passed("Avoided parsing invalid STS headers");

    return (rv0 + rv1);
}
