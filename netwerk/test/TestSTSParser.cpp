/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Strict-Transport-Security.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Sid Stamm <sid@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

//#define MOZILLA_INTERNAL_API

#include "TestHarness.h"
#include <stdio.h>
#include "plstr.h"
#include "nsNetUtil.h"
#include "nsStringGlue.h"
#include "nsIStrictTransportSecurityService.h"
#include "nsIPermissionManager.h"

#define EXPECT_SUCCESS(rv, ...) \
  PR_BEGIN_MACRO \
  if (NS_FAILED(rv)) { \
    fail(__VA_ARGS__); \
    return PR_FALSE; \
  } \
  PR_END_MACRO


#define EXPECT_FAILURE(rv, ...) \
  PR_BEGIN_MACRO \
  if (NS_SUCCEEDED(rv)) { \
    fail(__VA_ARGS__); \
    return PR_FALSE; \
  } \
  PR_END_MACRO

#define REQUIRE_EQUAL(a, b, ...) \
  PR_BEGIN_MACRO \
  if (a != b) { \
    fail(__VA_ARGS__); \
    return PR_FALSE; \
  } \
  PR_END_MACRO

bool
TestSuccess(const char* hdr, bool extraTokens,
            nsIStrictTransportSecurityService* stss,
            nsIPermissionManager* pm)
{
  nsCOMPtr<nsIURI> dummyUri;
  nsresult rv = NS_NewURI(getter_AddRefs(dummyUri), "https://foo.com/bar.html");
  EXPECT_SUCCESS(rv, "Failed to create URI");

  rv = stss->ProcessStsHeader(dummyUri, hdr);
  EXPECT_SUCCESS(rv, "Failed to process valid header: %s", hdr);

  if (extraTokens) {
    REQUIRE_EQUAL(rv, NS_SUCCESS_LOSS_OF_INSIGNIFICANT_DATA,
                  "Extra tokens were expected when parsing, but were not encountered.");
  } else {
    REQUIRE_EQUAL(rv, NS_OK, "Unexpected tokens found during parsing.");
  }

  passed(hdr);
  return PR_TRUE;
}

bool TestFailure(const char* hdr,
                   nsIStrictTransportSecurityService* stss,
                   nsIPermissionManager* pm)
{
  nsCOMPtr<nsIURI> dummyUri;
  nsresult rv = NS_NewURI(getter_AddRefs(dummyUri), "https://foo.com/bar.html");
  EXPECT_SUCCESS(rv, "Failed to create URI");

  rv = stss->ProcessStsHeader(dummyUri, hdr);
  EXPECT_FAILURE(rv, "Parsed invalid header: %s", hdr);
  passed(hdr);
  return PR_TRUE;
}


int
main(PRInt32 argc, char *argv[])
{
    nsresult rv;
    ScopedXPCOM xpcom("STS Parser Tests");
    if (xpcom.failed())
      return -1;

    // grab handle to the service
    nsCOMPtr<nsIStrictTransportSecurityService> stss;
    stss = do_GetService("@mozilla.org/stsservice;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIPermissionManager> pm;
    pm = do_GetService("@mozilla.org/permissionmanager;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    int rv0, rv1;

    nsTArray<bool> rvs(24);

    // *** parsing tests
    printf("*** Attempting to parse valid STS headers ...\n");

    // SHOULD SUCCEED:
    rvs.AppendElement(TestSuccess("max-age=100", PR_FALSE, stss, pm));
    rvs.AppendElement(TestSuccess("max-age  =100", PR_FALSE, stss, pm));
    rvs.AppendElement(TestSuccess(" max-age=100", PR_FALSE, stss, pm));
    rvs.AppendElement(TestSuccess("max-age = 100 ", PR_FALSE, stss, pm));
    rvs.AppendElement(TestSuccess("max-age  =       100             ", PR_FALSE, stss, pm));

    rvs.AppendElement(TestSuccess("maX-aGe=100", PR_FALSE, stss, pm));
    rvs.AppendElement(TestSuccess("MAX-age  =100", PR_FALSE, stss, pm));
    rvs.AppendElement(TestSuccess("max-AGE=100", PR_FALSE, stss, pm));
    rvs.AppendElement(TestSuccess("Max-Age = 100 ", PR_FALSE, stss, pm));
    rvs.AppendElement(TestSuccess("MAX-AGE = 100 ", PR_FALSE, stss, pm));

    rvs.AppendElement(TestSuccess("max-age=100;includeSubdomains", PR_FALSE, stss, pm));
    rvs.AppendElement(TestSuccess("max-age=100; includeSubdomains", PR_FALSE, stss, pm));
    rvs.AppendElement(TestSuccess(" max-age=100; includeSubdomains", PR_FALSE, stss, pm));
    rvs.AppendElement(TestSuccess("max-age = 100 ; includeSubdomains", PR_FALSE, stss, pm));
    rvs.AppendElement(TestSuccess("max-age  =       100             ; includeSubdomains", PR_FALSE, stss, pm));

    rvs.AppendElement(TestSuccess("maX-aGe=100; includeSUBDOMAINS", PR_FALSE, stss, pm));
    rvs.AppendElement(TestSuccess("MAX-age  =100; includeSubDomains", PR_FALSE, stss, pm));
    rvs.AppendElement(TestSuccess("max-AGE=100; iNcLuDeSuBdoMaInS", PR_FALSE, stss, pm));
    rvs.AppendElement(TestSuccess("Max-Age = 100; includesubdomains ", PR_FALSE, stss, pm));
    rvs.AppendElement(TestSuccess("INCLUDESUBDOMAINS;MaX-AgE = 100 ", PR_FALSE, stss, pm));

    // these are weird tests, but are testing that some extended syntax is
    // still allowed (but it is ignored)
    rvs.AppendElement(TestSuccess("max-age=100randomstuffhere", PR_TRUE, stss, pm));
    rvs.AppendElement(TestSuccess("max-age=100 includesubdomains", PR_TRUE, stss, pm));
    rvs.AppendElement(TestSuccess("max-age=100 bar foo", PR_TRUE, stss, pm));
    rvs.AppendElement(TestSuccess("max-age=100 ; includesubdomainsSomeStuff", PR_TRUE, stss, pm));

    rv0 = rvs.Contains(PR_FALSE) ? 1 : 0;
    if (rv0 == 0)
      passed("Successfully Parsed STS headers with mixed case and LWS");

    rvs.Clear();

    // SHOULD FAIL:
    printf("*** Attempting to parse invalid STS headers (should not parse)...\n");
    // invalid max-ages
    rvs.AppendElement(TestFailure("max-age ", stss, pm));
    rvs.AppendElement(TestFailure("max-age=p", stss, pm));
    rvs.AppendElement(TestFailure("max-age=*1p2", stss, pm));
    rvs.AppendElement(TestFailure("max-age=.20032", stss, pm));
    rvs.AppendElement(TestFailure("max-age=!20032", stss, pm));
    rvs.AppendElement(TestFailure("max-age==20032", stss, pm));

    // invalid headers
    rvs.AppendElement(TestFailure("foobar", stss, pm));
    rvs.AppendElement(TestFailure("maxage=100", stss, pm));
    rvs.AppendElement(TestFailure("maxa-ge=100", stss, pm));
    rvs.AppendElement(TestFailure("max-ag=100", stss, pm));
    rvs.AppendElement(TestFailure("includesubdomains", stss, pm));
    rvs.AppendElement(TestFailure(";", stss, pm));

    rv1 = rvs.Contains(PR_FALSE) ? 1 : 0;
    if (rv1 == 0)
      passed("Avoided parsing invalid STS headers");

    return (rv0 + rv1);
}
