/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>

#include "gtest/gtest.h"
#include "nsNetUtil.h"
#include "nsISiteSecurityService.h"
#include "nsIURI.h"

void
TestSuccess(const char* hdr, bool extraTokens,
            uint64_t expectedMaxAge, bool expectedIncludeSubdomains,
            nsISiteSecurityService* sss)
{
  nsCOMPtr<nsIURI> dummyUri;
  nsresult rv = NS_NewURI(getter_AddRefs(dummyUri), "https://foo.com/bar.html");
  ASSERT_TRUE(NS_SUCCEEDED(rv)) << "Failed to create URI";

  uint64_t maxAge = 0;
  bool includeSubdomains = false;
  rv = sss->UnsafeProcessHeader(nsISiteSecurityService::HEADER_HSTS, dummyUri,
                                hdr, 0, &maxAge, &includeSubdomains, nullptr);
  ASSERT_TRUE(NS_SUCCEEDED(rv)) << "Failed to process valid header: " << hdr;

  ASSERT_EQ(maxAge, expectedMaxAge) << "Did not correctly parse maxAge";
  EXPECT_EQ(includeSubdomains, expectedIncludeSubdomains) <<
    "Did not correctly parse presence/absence of includeSubdomains";

  if (extraTokens) {
    EXPECT_EQ(rv, NS_SUCCESS_LOSS_OF_INSIGNIFICANT_DATA) <<
      "Extra tokens were expected when parsing, but were not encountered.";
  } else {
    EXPECT_EQ(rv, NS_OK) << "Unexpected tokens found during parsing.";
  }

  printf("%s\n", hdr);
}

void TestFailure(const char* hdr,
                 nsISiteSecurityService* sss)
{
  nsCOMPtr<nsIURI> dummyUri;
  nsresult rv = NS_NewURI(getter_AddRefs(dummyUri), "https://foo.com/bar.html");
  ASSERT_TRUE(NS_SUCCEEDED(rv)) << "Failed to create URI";

  rv = sss->UnsafeProcessHeader(nsISiteSecurityService::HEADER_HSTS, dummyUri,
                                hdr, 0, nullptr, nullptr, nullptr);
  ASSERT_TRUE(NS_FAILED(rv)) << "Parsed invalid header: " << hdr;

  printf("%s\n", hdr);
}

TEST(psm_STSParser, Test)
{
    nsresult rv;

    // grab handle to the service
    nsCOMPtr<nsISiteSecurityService> sss;
    sss = do_GetService("@mozilla.org/ssservice;1", &rv);
    ASSERT_TRUE(NS_SUCCEEDED(rv));

    // *** parsing tests
    printf("*** Attempting to parse valid STS headers ...\n");

    // SHOULD SUCCEED:
    TestSuccess("max-age=100", false, 100, false, sss);
    TestSuccess("max-age  =100", false, 100, false, sss);
    TestSuccess(" max-age=100", false, 100, false, sss);
    TestSuccess("max-age = 100 ", false, 100, false, sss);
    TestSuccess(R"(max-age = "100" )", false, 100, false, sss);
    TestSuccess(R"(max-age="100")", false, 100, false, sss);
    TestSuccess(R"( max-age ="100" )", false, 100, false, sss);
    TestSuccess("\tmax-age\t=\t\"100\"\t", false, 100, false, sss);
    TestSuccess("max-age  =       100             ", false, 100, false, sss);

    TestSuccess("maX-aGe=100", false, 100, false, sss);
    TestSuccess("MAX-age  =100", false, 100, false, sss);
    TestSuccess("max-AGE=100", false, 100, false, sss);
    TestSuccess("Max-Age = 100 ", false, 100, false, sss);
    TestSuccess("MAX-AGE = 100 ", false, 100, false, sss);

    TestSuccess("max-age=100;includeSubdomains", false, 100, true, sss);
    TestSuccess("max-age=100\t; includeSubdomains", false, 100, true, sss);
    TestSuccess(" max-age=100; includeSubdomains", false, 100, true, sss);
    TestSuccess("max-age = 100 ; includeSubdomains", false, 100, true, sss);
    TestSuccess("max-age  =       100             ; includeSubdomains",
                false, 100, true, sss);

    TestSuccess("maX-aGe=100; includeSUBDOMAINS", false, 100, true, sss);
    TestSuccess("MAX-age  =100; includeSubDomains", false, 100, true, sss);
    TestSuccess("max-AGE=100; iNcLuDeSuBdoMaInS", false, 100, true, sss);
    TestSuccess("Max-Age = 100; includesubdomains ", false, 100, true, sss);
    TestSuccess("INCLUDESUBDOMAINS;MaX-AgE = 100 ", false, 100, true, sss);
    // Turns out, the actual directive is entirely optional (hence the
    // trailing semicolon)
    TestSuccess("max-age=100;includeSubdomains;", true, 100, true, sss);

    // these are weird tests, but are testing that some extended syntax is
    // still allowed (but it is ignored)
    TestSuccess("max-age=100 ; includesubdomainsSomeStuff",
                true, 100, false, sss);
    TestSuccess("\r\n\t\t    \tcompletelyUnrelated = foobar; max-age= 34520103"
                "\t \t; alsoUnrelated;asIsThis;\tincludeSubdomains\t\t \t",
                true, 34520103, true, sss);
    TestSuccess(R"(max-age=100; unrelated="quoted \"thingy\"")",
                true, 100, false, sss);

    // SHOULD FAIL:
    printf("* Attempting to parse invalid STS headers (should not parse)...\n");
    // invalid max-ages
    TestFailure("max-age", sss);
    TestFailure("max-age ", sss);
    TestFailure("max-age=p", sss);
    TestFailure("max-age=*1p2", sss);
    TestFailure("max-age=.20032", sss);
    TestFailure("max-age=!20032", sss);
    TestFailure("max-age==20032", sss);

    // invalid headers
    TestFailure("foobar", sss);
    TestFailure("maxage=100", sss);
    TestFailure("maxa-ge=100", sss);
    TestFailure("max-ag=100", sss);
    TestFailure("includesubdomains", sss);
    TestFailure(";", sss);
    TestFailure(R"(max-age="100)", sss);
    // The max-age directive here doesn't conform to the spec, so it MUST
    // be ignored. Consequently, the REQUIRED max-age directive is not
    // present in this header, and so it is invalid.
    TestFailure("max-age=100, max-age=200; includeSubdomains", sss);
    TestFailure("max-age=100 includesubdomains", sss);
    TestFailure("max-age=100 bar foo", sss);
    TestFailure("max-age=100randomstuffhere", sss);
    // All directives MUST appear only once in an STS header field.
    TestFailure("max-age=100; max-age=200", sss);
    TestFailure("includeSubdomains; max-age=200; includeSubdomains", sss);
    TestFailure("max-age=200; includeSubdomains; includeSubdomains", sss);
    // The includeSubdomains directive is valueless.
    TestFailure("max-age=100; includeSubdomains=unexpected", sss);
    // LWS must have at least one space or horizontal tab
    TestFailure("\r\nmax-age=200", sss);
}
