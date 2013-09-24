/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TestCommon.h"
#include "nsIServiceManager.h"
#include "nsICookieService.h"
#include "nsICookieManager.h"
#include "nsICookieManager2.h"
#include "nsICookie2.h"
#include <stdio.h>
#include "plstr.h"
#include "prprf.h"
#include "nsNetUtil.h"
#include "nsNetCID.h"
#include "nsStringAPI.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"

static NS_DEFINE_CID(kCookieServiceCID, NS_COOKIESERVICE_CID);
static NS_DEFINE_CID(kPrefServiceCID,   NS_PREFSERVICE_CID);

// various pref strings
static const char kCookiesPermissions[] = "network.cookie.cookieBehavior";
static const char kCookiesLifetimeEnabled[] = "network.cookie.lifetime.enabled";
static const char kCookiesLifetimeDays[] = "network.cookie.lifetime.days";
static const char kCookiesLifetimeCurrentSession[] = "network.cookie.lifetime.behavior";
static const char kCookiesAskPermission[] = "network.cookie.warnAboutCookies";
static const char kCookiesMaxPerHost[] = "network.cookie.maxPerHost";

static char *sBuffer;

nsresult
SetACookie(nsICookieService *aCookieService, const char *aSpec1, const char *aSpec2, const char* aCookieString, const char *aServerTime)
{
    nsCOMPtr<nsIURI> uri1, uri2;
    NS_NewURI(getter_AddRefs(uri1), aSpec1);
    if (aSpec2)
        NS_NewURI(getter_AddRefs(uri2), aSpec2);

    sBuffer = PR_sprintf_append(sBuffer, "    for host \"%s\": SET ", aSpec1);
    nsresult rv = aCookieService->SetCookieStringFromHttp(uri1, uri2, nullptr, (char *)aCookieString, aServerTime, nullptr);
    // the following code is useless. the cookieservice blindly returns NS_OK
    // from SetCookieString. we have to call GetCookie to see if the cookie was
    // set correctly...
    if (NS_FAILED(rv)) {
        sBuffer = PR_sprintf_append(sBuffer, "nothing\n");
    } else {
        sBuffer = PR_sprintf_append(sBuffer, "\"%s\"\n", aCookieString);
    }
    return rv;
}

nsresult
SetACookieNoHttp(nsICookieService *aCookieService, const char *aSpec, const char* aCookieString)
{
    nsCOMPtr<nsIURI> uri;
    NS_NewURI(getter_AddRefs(uri), aSpec);

    sBuffer = PR_sprintf_append(sBuffer, "    for host \"%s\": SET ", aSpec);
    nsresult rv = aCookieService->SetCookieString(uri, nullptr, (char *)aCookieString, nullptr);
    // the following code is useless. the cookieservice blindly returns NS_OK
    // from SetCookieString. we have to call GetCookie to see if the cookie was
    // set correctly...
    if (NS_FAILED(rv)) {
        sBuffer = PR_sprintf_append(sBuffer, "nothing\n");
    } else {
        sBuffer = PR_sprintf_append(sBuffer, "\"%s\"\n", aCookieString);
    }
    return rv;
}

// returns true if cookie(s) for the given host were found; else false.
// the cookie string is returned via aCookie.
bool
GetACookie(nsICookieService *aCookieService, const char *aSpec1, const char *aSpec2, char **aCookie)
{
    nsCOMPtr<nsIURI> uri1, uri2;
    NS_NewURI(getter_AddRefs(uri1), aSpec1);
    if (aSpec2)
        NS_NewURI(getter_AddRefs(uri2), aSpec2);

    sBuffer = PR_sprintf_append(sBuffer, "             \"%s\": GOT ", aSpec1);
    nsresult rv = aCookieService->GetCookieStringFromHttp(uri1, uri2, nullptr, aCookie);
    if (NS_FAILED(rv)) {
      sBuffer = PR_sprintf_append(sBuffer, "XXX GetCookieString() failed!\n");
    }
    if (!*aCookie) {
        sBuffer = PR_sprintf_append(sBuffer, "nothing\n");
    } else {
        sBuffer = PR_sprintf_append(sBuffer, "\"%s\"\n", *aCookie);
    }
    return *aCookie != nullptr;
}

// returns true if cookie(s) for the given host were found; else false.
// the cookie string is returned via aCookie.
bool
GetACookieNoHttp(nsICookieService *aCookieService, const char *aSpec, char **aCookie)
{
    nsCOMPtr<nsIURI> uri;
    NS_NewURI(getter_AddRefs(uri), aSpec);

    sBuffer = PR_sprintf_append(sBuffer, "             \"%s\": GOT ", aSpec);
    nsresult rv = aCookieService->GetCookieString(uri, nullptr, aCookie);
    if (NS_FAILED(rv)) {
      sBuffer = PR_sprintf_append(sBuffer, "XXX GetCookieString() failed!\n");
    }
    if (!*aCookie) {
        sBuffer = PR_sprintf_append(sBuffer, "nothing\n");
    } else {
        sBuffer = PR_sprintf_append(sBuffer, "\"%s\"\n", *aCookie);
    }
    return *aCookie != nullptr;
}

// some #defines for comparison rules
#define MUST_BE_NULL     0
#define MUST_EQUAL       1
#define MUST_CONTAIN     2
#define MUST_NOT_CONTAIN 3
#define MUST_NOT_EQUAL   4

// a simple helper function to improve readability:
// takes one of the #defined rules above, and performs the appropriate test.
// true means the test passed; false means the test failed.
static inline bool
CheckResult(const char *aLhs, uint32_t aRule, const char *aRhs = nullptr)
{
    switch (aRule) {
        case MUST_BE_NULL:
            return !aLhs || !*aLhs;

        case MUST_EQUAL:
            return !PL_strcmp(aLhs, aRhs);

        case MUST_NOT_EQUAL:
            return PL_strcmp(aLhs, aRhs);

        case MUST_CONTAIN:
            return PL_strstr(aLhs, aRhs) != nullptr;

        case MUST_NOT_CONTAIN:
            return PL_strstr(aLhs, aRhs) == nullptr;

        default:
            return false; // failure
    }
}

// helper function that ensures the first aSize elements of aResult are
// true (i.e. all tests succeeded). prints the result of the tests (if any
// tests failed, it prints the zero-based index of each failed test).
bool
PrintResult(const bool aResult[], uint32_t aSize)
{
    bool failed = false;
    sBuffer = PR_sprintf_append(sBuffer, "*** tests ");
    for (uint32_t i = 0; i < aSize; ++i) {
        if (!aResult[i]) {
            failed = true;
            sBuffer = PR_sprintf_append(sBuffer, "%d ", i);
        }
    }
    if (failed) {
        sBuffer = PR_sprintf_append(sBuffer, "FAILED!\a\n");
    } else {
        sBuffer = PR_sprintf_append(sBuffer, "passed.\n");
    }
    return !failed;
}

void
InitPrefs(nsIPrefBranch *aPrefBranch)
{
    // init some relevant prefs, so the tests don't go awry.
    // we use the most restrictive set of prefs we can;
    // however, we don't test third party blocking here.
    aPrefBranch->SetIntPref(kCookiesPermissions, 0); // accept all
    aPrefBranch->SetBoolPref(kCookiesLifetimeEnabled, true);
    aPrefBranch->SetIntPref(kCookiesLifetimeCurrentSession, 0);
    aPrefBranch->SetIntPref(kCookiesLifetimeDays, 1);
    aPrefBranch->SetBoolPref(kCookiesAskPermission, false);
    // Set the base domain limit to 50 so we have a known value.
    aPrefBranch->SetIntPref(kCookiesMaxPerHost, 50);
}

class ScopedXPCOM
{
public:
  ScopedXPCOM() : rv(NS_InitXPCOM2(nullptr, nullptr, nullptr)) { }
  ~ScopedXPCOM()
  {
    if (NS_SUCCEEDED(rv))
      NS_ShutdownXPCOM(nullptr);
  }

  nsresult rv;
};

int
main(int32_t argc, char *argv[])
{
    if (test_common_init(&argc, &argv) != 0)
        return -1;

    bool allTestsPassed = true;

    ScopedXPCOM xpcom;
    if (NS_FAILED(xpcom.rv))
      return -1;

    {
      nsresult rv0;

      nsCOMPtr<nsICookieService> cookieService =
        do_GetService(kCookieServiceCID, &rv0);
      if (NS_FAILED(rv0)) return -1;

      nsCOMPtr<nsIPrefBranch> prefBranch =
        do_GetService(kPrefServiceCID, &rv0);
      if (NS_FAILED(rv0)) return -1;

      InitPrefs(prefBranch);

      bool rv[20];
      nsCString cookie;

      /* The basic idea behind these tests is the following:
       *
       * we set() some cookie, then try to get() it in various ways. we have
       * several possible tests we perform on the cookie string returned from
       * get():
       *
       * a) check whether the returned string is null (i.e. we got no cookies
       *    back). this is used e.g. to ensure a given cookie was deleted
       *    correctly, or to ensure a certain cookie wasn't returned to a given
       *    host.
       * b) check whether the returned string exactly matches a given string.
       *    this is used where we want to make sure our cookie service adheres to
       *    some strict spec (e.g. ordering of multiple cookies), or where we
       *    just know exactly what the returned string should be.
       * c) check whether the returned string contains/does not contain a given
       *    string. this is used where we don't know/don't care about the
       *    ordering of multiple cookies - we just want to make sure the cookie
       *    string contains them all, in some order.
       *
       * the results of each individual testing operation from CheckResult() is
       * stored in an array of bools, which is then checked against the expected
       * outcomes (all successes), by PrintResult(). the overall result of all
       * tests to date is kept in |allTestsPassed|, for convenient display at the
       * end.
       *
       * Interpreting the output:
       * each setting/getting operation will print output saying exactly what
       * it's doing and the outcome, respectively. this information is only
       * useful for debugging purposes; the actual result of the tests is
       * printed at the end of each block of tests. this will either be "all
       * tests passed" or "tests X Y Z failed", where X, Y, Z are the indexes
       * of rv (i.e. zero-based). at the conclusion of all tests, the overall
       * passed/failed result is printed.
       *
       * NOTE: this testsuite is not yet comprehensive or complete, and is
       * somewhat contrived - still under development, and needs improving!
       */

      // *** basic tests
      sBuffer = PR_sprintf_append(sBuffer, "*** Beginning basic tests...\n");

      // test some basic variations of the domain & path
      SetACookie(cookieService, "http://www.basic.com", nullptr, "test=basic", nullptr);
      GetACookie(cookieService, "http://www.basic.com", nullptr, getter_Copies(cookie));
      rv[0] = CheckResult(cookie.get(), MUST_EQUAL, "test=basic");
      GetACookie(cookieService, "http://www.basic.com/testPath/testfile.txt", nullptr, getter_Copies(cookie));
      rv[1] = CheckResult(cookie.get(), MUST_EQUAL, "test=basic");
      GetACookie(cookieService, "http://www.basic.com./", nullptr, getter_Copies(cookie));
      rv[2] = CheckResult(cookie.get(), MUST_BE_NULL);
      GetACookie(cookieService, "http://www.basic.com.", nullptr, getter_Copies(cookie));
      rv[3] = CheckResult(cookie.get(), MUST_BE_NULL);
      GetACookie(cookieService, "http://www.basic.com./testPath/testfile.txt", nullptr, getter_Copies(cookie));
      rv[4] = CheckResult(cookie.get(), MUST_BE_NULL);
      GetACookie(cookieService, "http://www.basic2.com/", nullptr, getter_Copies(cookie));
      rv[5] = CheckResult(cookie.get(), MUST_BE_NULL);
      SetACookie(cookieService, "http://www.basic.com", nullptr, "test=basic; max-age=-1", nullptr);
      GetACookie(cookieService, "http://www.basic.com/", nullptr, getter_Copies(cookie));
      rv[6] = CheckResult(cookie.get(), MUST_BE_NULL);

      allTestsPassed = PrintResult(rv, 7) && allTestsPassed;


      // *** domain tests
      sBuffer = PR_sprintf_append(sBuffer, "*** Beginning domain tests...\n");

      // test some variations of the domain & path, for different domains of
      // a domain cookie
      SetACookie(cookieService, "http://www.domain.com", nullptr, "test=domain; domain=domain.com", nullptr);
      GetACookie(cookieService, "http://domain.com", nullptr, getter_Copies(cookie));
      rv[0] = CheckResult(cookie.get(), MUST_EQUAL, "test=domain");
      GetACookie(cookieService, "http://domain.com.", nullptr, getter_Copies(cookie));
      rv[1] = CheckResult(cookie.get(), MUST_BE_NULL);
      GetACookie(cookieService, "http://www.domain.com", nullptr, getter_Copies(cookie));
      rv[2] = CheckResult(cookie.get(), MUST_EQUAL, "test=domain");
      GetACookie(cookieService, "http://foo.domain.com", nullptr, getter_Copies(cookie));
      rv[3] = CheckResult(cookie.get(), MUST_EQUAL, "test=domain");
      SetACookie(cookieService, "http://www.domain.com", nullptr, "test=domain; domain=domain.com; max-age=-1", nullptr);
      GetACookie(cookieService, "http://domain.com", nullptr, getter_Copies(cookie));
      rv[4] = CheckResult(cookie.get(), MUST_BE_NULL);

      SetACookie(cookieService, "http://www.domain.com", nullptr, "test=domain; domain=.domain.com", nullptr);
      GetACookie(cookieService, "http://domain.com", nullptr, getter_Copies(cookie));
      rv[5] = CheckResult(cookie.get(), MUST_EQUAL, "test=domain");
      GetACookie(cookieService, "http://www.domain.com", nullptr, getter_Copies(cookie));
      rv[6] = CheckResult(cookie.get(), MUST_EQUAL, "test=domain");
      GetACookie(cookieService, "http://bah.domain.com", nullptr, getter_Copies(cookie));
      rv[7] = CheckResult(cookie.get(), MUST_EQUAL, "test=domain");
      SetACookie(cookieService, "http://www.domain.com", nullptr, "test=domain; domain=.domain.com; max-age=-1", nullptr);
      GetACookie(cookieService, "http://domain.com", nullptr, getter_Copies(cookie));
      rv[8] = CheckResult(cookie.get(), MUST_BE_NULL);

      SetACookie(cookieService, "http://www.domain.com", nullptr, "test=domain; domain=.foo.domain.com", nullptr);
      GetACookie(cookieService, "http://foo.domain.com", nullptr, getter_Copies(cookie));
      rv[9] = CheckResult(cookie.get(), MUST_BE_NULL);

      SetACookie(cookieService, "http://www.domain.com", nullptr, "test=domain; domain=moose.com", nullptr);
      GetACookie(cookieService, "http://foo.domain.com", nullptr, getter_Copies(cookie));
      rv[10] = CheckResult(cookie.get(), MUST_BE_NULL);

      SetACookie(cookieService, "http://www.domain.com", nullptr, "test=domain; domain=domain.com.", nullptr);
      GetACookie(cookieService, "http://foo.domain.com", nullptr, getter_Copies(cookie));
      rv[11] = CheckResult(cookie.get(), MUST_BE_NULL);

      SetACookie(cookieService, "http://www.domain.com", nullptr, "test=domain; domain=..domain.com", nullptr);
      GetACookie(cookieService, "http://foo.domain.com", nullptr, getter_Copies(cookie));
      rv[12] = CheckResult(cookie.get(), MUST_BE_NULL);

      SetACookie(cookieService, "http://www.domain.com", nullptr, "test=domain; domain=..domain.com.", nullptr);
      GetACookie(cookieService, "http://foo.domain.com", nullptr, getter_Copies(cookie));
      rv[13] = CheckResult(cookie.get(), MUST_BE_NULL);

      SetACookie(cookieService, "http://path.net/path/file", nullptr, "test=taco; path=\"/bogus\"", nullptr);
      GetACookie(cookieService, "http://path.net/path/file", nullptr, getter_Copies(cookie));
      rv[14] = CheckResult(cookie.get(), MUST_EQUAL, "test=taco");
      SetACookie(cookieService, "http://path.net/path/file", nullptr, "test=taco; max-age=-1", nullptr);
      GetACookie(cookieService, "http://path.net/path/file", nullptr, getter_Copies(cookie));
      rv[15] = CheckResult(cookie.get(), MUST_BE_NULL);

      allTestsPassed = PrintResult(rv, 16) && allTestsPassed;


      // *** path tests
      sBuffer = PR_sprintf_append(sBuffer, "*** Beginning path tests...\n");

      // test some variations of the domain & path, for different paths of
      // a path cookie
      SetACookie(cookieService, "http://path.net/path/file", nullptr, "test=path; path=/path", nullptr);
      GetACookie(cookieService, "http://path.net/path", nullptr, getter_Copies(cookie));
      rv[0] = CheckResult(cookie.get(), MUST_EQUAL, "test=path");
      GetACookie(cookieService, "http://path.net/path/", nullptr, getter_Copies(cookie));
      rv[1] = CheckResult(cookie.get(), MUST_EQUAL, "test=path");
      GetACookie(cookieService, "http://path.net/path/hithere.foo", nullptr, getter_Copies(cookie));
      rv[2] = CheckResult(cookie.get(), MUST_EQUAL, "test=path");
      GetACookie(cookieService, "http://path.net/path?hithere/foo", nullptr, getter_Copies(cookie));
      rv[3] = CheckResult(cookie.get(), MUST_EQUAL, "test=path");
      GetACookie(cookieService, "http://path.net/path2", nullptr, getter_Copies(cookie));
      rv[4] = CheckResult(cookie.get(), MUST_BE_NULL);
      GetACookie(cookieService, "http://path.net/path2/", nullptr, getter_Copies(cookie));
      rv[5] = CheckResult(cookie.get(), MUST_BE_NULL);
      SetACookie(cookieService, "http://path.net/path/file", nullptr, "test=path; path=/path; max-age=-1", nullptr);
      GetACookie(cookieService, "http://path.net/path/", nullptr, getter_Copies(cookie));
      rv[6] = CheckResult(cookie.get(), MUST_BE_NULL);

      SetACookie(cookieService, "http://path.net/path/file", nullptr, "test=path; path=/path/", nullptr);
      GetACookie(cookieService, "http://path.net/path", nullptr, getter_Copies(cookie));
      rv[7] = CheckResult(cookie.get(), MUST_EQUAL, "test=path");
      GetACookie(cookieService, "http://path.net/path/", nullptr, getter_Copies(cookie));
      rv[8] = CheckResult(cookie.get(), MUST_EQUAL, "test=path");
      SetACookie(cookieService, "http://path.net/path/file", nullptr, "test=path; path=/path/; max-age=-1", nullptr);
      GetACookie(cookieService, "http://path.net/path/", nullptr, getter_Copies(cookie));
      rv[9] = CheckResult(cookie.get(), MUST_BE_NULL);

      // note that a site can set a cookie for a path it's not on.
      // this is an intentional deviation from spec (see comments in
      // nsCookieService::CheckPath()), so we test this functionality too
      SetACookie(cookieService, "http://path.net/path/file", nullptr, "test=path; path=/foo/", nullptr);
      GetACookie(cookieService, "http://path.net/path", nullptr, getter_Copies(cookie));
      rv[10] = CheckResult(cookie.get(), MUST_BE_NULL);
      GetACookie(cookieService, "http://path.net/foo", nullptr, getter_Copies(cookie));
      rv[11] = CheckResult(cookie.get(), MUST_EQUAL, "test=path");
      SetACookie(cookieService, "http://path.net/path/file", nullptr, "test=path; path=/foo/; max-age=-1", nullptr);
      GetACookie(cookieService, "http://path.net/foo/", nullptr, getter_Copies(cookie));
      rv[12] = CheckResult(cookie.get(), MUST_BE_NULL);

      // bug 373228: make sure cookies with paths longer than 1024 bytes,
      // and cookies with paths or names containing tabs, are rejected.
      // the following cookie has a path > 1024 bytes explicitly specified in the cookie
      SetACookie(cookieService, "http://path.net/", nullptr, "test=path; path=/1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890/", nullptr);
      GetACookie(cookieService, "http://path.net/1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890", nullptr, getter_Copies(cookie));
      rv[13] = CheckResult(cookie.get(), MUST_BE_NULL);
      // the following cookie has a path > 1024 bytes implicitly specified by the uri path
      SetACookie(cookieService, "http://path.net/1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890/", nullptr, "test=path", nullptr);
      GetACookie(cookieService, "http://path.net/1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890/", nullptr, getter_Copies(cookie));
      rv[14] = CheckResult(cookie.get(), MUST_BE_NULL);
      // the following cookie includes a tab in the path
      SetACookie(cookieService, "http://path.net/", nullptr, "test=path; path=/foo\tbar/", nullptr);
      GetACookie(cookieService, "http://path.net/foo\tbar/", nullptr, getter_Copies(cookie));
      rv[15] = CheckResult(cookie.get(), MUST_BE_NULL);
      // the following cookie includes a tab in the name
      SetACookie(cookieService, "http://path.net/", nullptr, "test\ttabs=tab", nullptr);
      GetACookie(cookieService, "http://path.net/", nullptr, getter_Copies(cookie));
      rv[16] = CheckResult(cookie.get(), MUST_BE_NULL);
      // the following cookie includes a tab in the value - allowed
      SetACookie(cookieService, "http://path.net/", nullptr, "test=tab\ttest", nullptr);
      GetACookie(cookieService, "http://path.net/", nullptr, getter_Copies(cookie));
      rv[17] = CheckResult(cookie.get(), MUST_EQUAL, "test=tab\ttest");
      SetACookie(cookieService, "http://path.net/", nullptr, "test=tab\ttest; max-age=-1", nullptr);
      GetACookie(cookieService, "http://path.net/", nullptr, getter_Copies(cookie));
      rv[18] = CheckResult(cookie.get(), MUST_BE_NULL);

      allTestsPassed = PrintResult(rv, 19) && allTestsPassed;


      // *** expiry & deletion tests
      // XXX add server time str parsing tests here
      sBuffer = PR_sprintf_append(sBuffer, "*** Beginning expiry & deletion tests...\n");

      // test some variations of the expiry time,
      // and test deletion of previously set cookies
      SetACookie(cookieService, "http://expireme.org/", nullptr, "test=expiry; max-age=-1", nullptr);
      GetACookie(cookieService, "http://expireme.org/", nullptr, getter_Copies(cookie));
      rv[0] = CheckResult(cookie.get(), MUST_BE_NULL);
      SetACookie(cookieService, "http://expireme.org/", nullptr, "test=expiry; max-age=0", nullptr);
      GetACookie(cookieService, "http://expireme.org/", nullptr, getter_Copies(cookie));
      rv[1] = CheckResult(cookie.get(), MUST_BE_NULL);
      SetACookie(cookieService, "http://expireme.org/", nullptr, "test=expiry; expires=bad", nullptr);
      GetACookie(cookieService, "http://expireme.org/", nullptr, getter_Copies(cookie));
      rv[2] = CheckResult(cookie.get(), MUST_EQUAL, "test=expiry");
      SetACookie(cookieService, "http://expireme.org/", nullptr, "test=expiry; expires=Thu, 10 Apr 1980 16:33:12 GMT", nullptr);
      GetACookie(cookieService, "http://expireme.org/", nullptr, getter_Copies(cookie));
      rv[3] = CheckResult(cookie.get(), MUST_BE_NULL);
      SetACookie(cookieService, "http://expireme.org/", nullptr, "test=expiry; expires=\"Thu, 10 Apr 1980 16:33:12 GMT", nullptr);
      GetACookie(cookieService, "http://expireme.org/", nullptr, getter_Copies(cookie));
      rv[4] = CheckResult(cookie.get(), MUST_BE_NULL);
      SetACookie(cookieService, "http://expireme.org/", nullptr, "test=expiry; expires=\"Thu, 10 Apr 1980 16:33:12 GMT\"", nullptr);
      GetACookie(cookieService, "http://expireme.org/", nullptr, getter_Copies(cookie));
      rv[5] = CheckResult(cookie.get(), MUST_BE_NULL);

      SetACookie(cookieService, "http://expireme.org/", nullptr, "test=expiry; max-age=60", nullptr);
      GetACookie(cookieService, "http://expireme.org/", nullptr, getter_Copies(cookie));
      rv[6] = CheckResult(cookie.get(), MUST_EQUAL, "test=expiry");
      SetACookie(cookieService, "http://expireme.org/", nullptr, "test=expiry; max-age=-20", nullptr);
      GetACookie(cookieService, "http://expireme.org/", nullptr, getter_Copies(cookie));
      rv[7] = CheckResult(cookie.get(), MUST_BE_NULL);
      SetACookie(cookieService, "http://expireme.org/", nullptr, "test=expiry; max-age=60", nullptr);
      GetACookie(cookieService, "http://expireme.org/", nullptr, getter_Copies(cookie));
      rv[8] = CheckResult(cookie.get(), MUST_EQUAL, "test=expiry");
      SetACookie(cookieService, "http://expireme.org/", nullptr, "test=expiry; expires=Thu, 10 Apr 1980 16:33:12 GMT", nullptr);
      GetACookie(cookieService, "http://expireme.org/", nullptr, getter_Copies(cookie));
      rv[9] = CheckResult(cookie.get(), MUST_BE_NULL);
      SetACookie(cookieService, "http://expireme.org/", nullptr, "test=expiry; max-age=60", nullptr);
      SetACookie(cookieService, "http://expireme.org/", nullptr, "newtest=expiry; max-age=60", nullptr);
      GetACookie(cookieService, "http://expireme.org/", nullptr, getter_Copies(cookie));
      rv[10] = CheckResult(cookie.get(), MUST_CONTAIN, "test=expiry");
      rv[11] = CheckResult(cookie.get(), MUST_CONTAIN, "newtest=expiry");
      SetACookie(cookieService, "http://expireme.org/", nullptr, "test=differentvalue; max-age=0", nullptr);
      GetACookie(cookieService, "http://expireme.org/", nullptr, getter_Copies(cookie));
      rv[12] = CheckResult(cookie.get(), MUST_EQUAL, "newtest=expiry");
      SetACookie(cookieService, "http://expireme.org/", nullptr, "newtest=evendifferentvalue; max-age=0", nullptr);
      GetACookie(cookieService, "http://expireme.org/", nullptr, getter_Copies(cookie));
      rv[13] = CheckResult(cookie.get(), MUST_BE_NULL);

      SetACookie(cookieService, "http://foo.expireme.org/", nullptr, "test=expiry; domain=.expireme.org; max-age=60", nullptr);
      GetACookie(cookieService, "http://expireme.org/", nullptr, getter_Copies(cookie));
      rv[14] = CheckResult(cookie.get(), MUST_EQUAL, "test=expiry");
      SetACookie(cookieService, "http://bar.expireme.org/", nullptr, "test=differentvalue; domain=.expireme.org; max-age=0", nullptr);
      GetACookie(cookieService, "http://expireme.org/", nullptr, getter_Copies(cookie));
      rv[15] = CheckResult(cookie.get(), MUST_BE_NULL);

      allTestsPassed = PrintResult(rv, 16) && allTestsPassed;


      // *** multiple cookie tests
      sBuffer = PR_sprintf_append(sBuffer, "*** Beginning multiple cookie tests...\n");

      // test the setting of multiple cookies, and test the order of precedence
      // (a later cookie overwriting an earlier one, in the same header string)
      SetACookie(cookieService, "http://multiple.cookies/", nullptr, "test=multiple; domain=.multiple.cookies \n test=different \n test=same; domain=.multiple.cookies \n newtest=ciao \n newtest=foo; max-age=-6 \n newtest=reincarnated", nullptr);
      GetACookie(cookieService, "http://multiple.cookies/", nullptr, getter_Copies(cookie));
      rv[0] = CheckResult(cookie.get(), MUST_NOT_CONTAIN, "test=multiple");
      rv[1] = CheckResult(cookie.get(), MUST_CONTAIN, "test=different");
      rv[2] = CheckResult(cookie.get(), MUST_CONTAIN, "test=same");
      rv[3] = CheckResult(cookie.get(), MUST_NOT_CONTAIN, "newtest=ciao");
      rv[4] = CheckResult(cookie.get(), MUST_NOT_CONTAIN, "newtest=foo");
      rv[5] = CheckResult(cookie.get(), MUST_CONTAIN, "newtest=reincarnated");
      SetACookie(cookieService, "http://multiple.cookies/", nullptr, "test=expiry; domain=.multiple.cookies; max-age=0", nullptr);
      GetACookie(cookieService, "http://multiple.cookies/", nullptr, getter_Copies(cookie));
      rv[6] = CheckResult(cookie.get(), MUST_NOT_CONTAIN, "test=same");
      SetACookie(cookieService, "http://multiple.cookies/", nullptr,  "\n test=different; max-age=0 \n", nullptr);
      GetACookie(cookieService, "http://multiple.cookies/", nullptr, getter_Copies(cookie));
      rv[7] = CheckResult(cookie.get(), MUST_NOT_CONTAIN, "test=different");
      SetACookie(cookieService, "http://multiple.cookies/", nullptr,  "newtest=dead; max-age=0", nullptr);
      GetACookie(cookieService, "http://multiple.cookies/", nullptr, getter_Copies(cookie));
      rv[8] = CheckResult(cookie.get(), MUST_BE_NULL);

      allTestsPassed = PrintResult(rv, 9) && allTestsPassed;


      // *** parser tests
      sBuffer = PR_sprintf_append(sBuffer, "*** Beginning parser tests...\n");

      // test the cookie header parser, under various circumstances.
      SetACookie(cookieService, "http://parser.test/", nullptr, "test=parser; domain=.parser.test; ;; ;=; ,,, ===,abc,=; abracadabra! max-age=20;=;;", nullptr);
      GetACookie(cookieService, "http://parser.test/", nullptr, getter_Copies(cookie));
      rv[0] = CheckResult(cookie.get(), MUST_EQUAL, "test=parser");
      SetACookie(cookieService, "http://parser.test/", nullptr, "test=parser; domain=.parser.test; max-age=0", nullptr);
      GetACookie(cookieService, "http://parser.test/", nullptr, getter_Copies(cookie));
      rv[1] = CheckResult(cookie.get(), MUST_BE_NULL);
      SetACookie(cookieService, "http://parser.test/", nullptr, "test=\"fubar! = foo;bar\\\";\" parser; domain=.parser.test; max-age=6\nfive; max-age=2.63,", nullptr);
      GetACookie(cookieService, "http://parser.test/", nullptr, getter_Copies(cookie));
      rv[2] = CheckResult(cookie.get(), MUST_CONTAIN, "test=\"fubar! = foo");
      rv[3] = CheckResult(cookie.get(), MUST_CONTAIN, "five");
      SetACookie(cookieService, "http://parser.test/", nullptr, "test=kill; domain=.parser.test; max-age=0 \n five; max-age=0", nullptr);
      GetACookie(cookieService, "http://parser.test/", nullptr, getter_Copies(cookie));
      rv[4] = CheckResult(cookie.get(), MUST_BE_NULL);

      // test the handling of VALUE-only cookies (see bug 169091),
      // i.e. "six" should assume an empty NAME, which allows other VALUE-only
      // cookies to overwrite it
      SetACookie(cookieService, "http://parser.test/", nullptr, "six", nullptr);
      GetACookie(cookieService, "http://parser.test/", nullptr, getter_Copies(cookie));
      rv[5] = CheckResult(cookie.get(), MUST_EQUAL, "six");
      SetACookie(cookieService, "http://parser.test/", nullptr, "seven", nullptr);
      GetACookie(cookieService, "http://parser.test/", nullptr, getter_Copies(cookie));
      rv[6] = CheckResult(cookie.get(), MUST_EQUAL, "seven");
      SetACookie(cookieService, "http://parser.test/", nullptr, " =eight", nullptr);
      GetACookie(cookieService, "http://parser.test/", nullptr, getter_Copies(cookie));
      rv[7] = CheckResult(cookie.get(), MUST_EQUAL, "eight");
      SetACookie(cookieService, "http://parser.test/", nullptr, "test=six", nullptr);
      GetACookie(cookieService, "http://parser.test/", nullptr, getter_Copies(cookie));
      rv[9] = CheckResult(cookie.get(), MUST_CONTAIN, "test=six");

      allTestsPassed = PrintResult(rv, 10) && allTestsPassed;


      // *** path ordering tests
      sBuffer = PR_sprintf_append(sBuffer, "*** Beginning path ordering tests...\n");

      // test that cookies are returned in path order - longest to shortest.
      // if the header doesn't specify a path, it's taken from the host URI.
      SetACookie(cookieService, "http://multi.path.tests/", nullptr, "test1=path; path=/one/two/three", nullptr);
      SetACookie(cookieService, "http://multi.path.tests/", nullptr, "test2=path; path=/one \n test3=path; path=/one/two/three/four \n test4=path; path=/one/two \n test5=path; path=/one/two/", nullptr);
      SetACookie(cookieService, "http://multi.path.tests/one/two/three/four/five/", nullptr, "test6=path", nullptr);
      SetACookie(cookieService, "http://multi.path.tests/one/two/three/four/five/six/", nullptr, "test7=path; path=", nullptr);
      SetACookie(cookieService, "http://multi.path.tests/", nullptr, "test8=path; path=/", nullptr);
      GetACookie(cookieService, "http://multi.path.tests/one/two/three/four/five/six/", nullptr, getter_Copies(cookie));
      rv[0] = CheckResult(cookie.get(), MUST_EQUAL, "test7=path; test6=path; test3=path; test1=path; test5=path; test4=path; test2=path; test8=path");

      allTestsPassed = PrintResult(rv, 1) && allTestsPassed;


      // *** httponly tests 
      sBuffer = PR_sprintf_append(sBuffer, "*** Beginning httponly tests...\n");

      // Since this cookie is NOT set via http, setting it fails
      SetACookieNoHttp(cookieService, "http://httponly.test/", "test=httponly; httponly");
      GetACookie(cookieService, "http://httponly.test/", nullptr, getter_Copies(cookie));
      rv[0] = CheckResult(cookie.get(), MUST_BE_NULL);
      // Since this cookie is set via http, it can be retrieved
      SetACookie(cookieService, "http://httponly.test/", nullptr, "test=httponly; httponly", nullptr);
      GetACookie(cookieService, "http://httponly.test/", nullptr, getter_Copies(cookie));
      rv[1] = CheckResult(cookie.get(), MUST_EQUAL, "test=httponly");
      // ... but not by web content
      GetACookieNoHttp(cookieService, "http://httponly.test/", getter_Copies(cookie));
      rv[2] = CheckResult(cookie.get(), MUST_BE_NULL);
      // Non-Http cookies should not replace HttpOnly cookies
      SetACookie(cookieService, "http://httponly.test/", nullptr, "test=httponly; httponly", nullptr);
      SetACookieNoHttp(cookieService, "http://httponly.test/", "test=not-httponly");
      GetACookie(cookieService, "http://httponly.test/", nullptr, getter_Copies(cookie));
      rv[3] = CheckResult(cookie.get(), MUST_EQUAL, "test=httponly");
      // ... and, if an HttpOnly cookie already exists, should not be set at all
      GetACookieNoHttp(cookieService, "http://httponly.test/", getter_Copies(cookie));
      rv[4] = CheckResult(cookie.get(), MUST_BE_NULL);
      // Non-Http cookies should not delete HttpOnly cookies
      SetACookie(cookieService, "http://httponly.test/", nullptr, "test=httponly; httponly", nullptr);
      SetACookieNoHttp(cookieService, "http://httponly.test/", "test=httponly; max-age=-1");
      GetACookie(cookieService, "http://httponly.test/", nullptr, getter_Copies(cookie));
      rv[5] = CheckResult(cookie.get(), MUST_EQUAL, "test=httponly");
      // ... but HttpOnly cookies should
      SetACookie(cookieService, "http://httponly.test/", nullptr, "test=httponly; httponly; max-age=-1", nullptr);
      GetACookie(cookieService, "http://httponly.test/", nullptr, getter_Copies(cookie));
      rv[6] = CheckResult(cookie.get(), MUST_BE_NULL);
      // Non-Httponly cookies can replace HttpOnly cookies when set over http
      SetACookie(cookieService, "http://httponly.test/", nullptr, "test=httponly; httponly", nullptr);
      SetACookie(cookieService, "http://httponly.test/", nullptr, "test=not-httponly", nullptr);
      GetACookieNoHttp(cookieService, "http://httponly.test/", getter_Copies(cookie));
      rv[7] = CheckResult(cookie.get(), MUST_EQUAL, "test=not-httponly");
      // scripts should not be able to set httponly cookies by replacing an existing non-httponly cookie
      SetACookie(cookieService, "http://httponly.test/", nullptr, "test=not-httponly", nullptr);
      SetACookieNoHttp(cookieService, "http://httponly.test/", "test=httponly; httponly");
      GetACookieNoHttp(cookieService, "http://httponly.test/", getter_Copies(cookie));
      rv[8] = CheckResult(cookie.get(), MUST_EQUAL, "test=not-httponly");

      allTestsPassed = PrintResult(rv, 9) && allTestsPassed;


      // *** nsICookieManager{2} interface tests
      sBuffer = PR_sprintf_append(sBuffer, "*** Beginning nsICookieManager{2} interface tests...\n");
      nsCOMPtr<nsICookieManager> cookieMgr = do_GetService(NS_COOKIEMANAGER_CONTRACTID, &rv0);
      if (NS_FAILED(rv0)) return -1;
      nsCOMPtr<nsICookieManager2> cookieMgr2 = do_QueryInterface(cookieMgr);
      if (!cookieMgr2) return -1;
      
      // first, ensure a clean slate
      rv[0] = NS_SUCCEEDED(cookieMgr->RemoveAll());
      // add some cookies
      rv[1] = NS_SUCCEEDED(cookieMgr2->Add(NS_LITERAL_CSTRING("cookiemgr.test"), // domain
                                           NS_LITERAL_CSTRING("/foo"),           // path
                                           NS_LITERAL_CSTRING("test1"),          // name
                                           NS_LITERAL_CSTRING("yes"),            // value
                                           false,                             // is secure
                                           false,                             // is httponly
                                           true,                              // is session
                                           INT64_MAX));                          // expiry time
      rv[2] = NS_SUCCEEDED(cookieMgr2->Add(NS_LITERAL_CSTRING("cookiemgr.test"), // domain
                                           NS_LITERAL_CSTRING("/foo"),           // path
                                           NS_LITERAL_CSTRING("test2"),          // name
                                           NS_LITERAL_CSTRING("yes"),            // value
                                           false,                             // is secure
                                           true,                              // is httponly
                                           true,                              // is session
                                           PR_Now() / PR_USEC_PER_SEC + 2));     // expiry time
      rv[3] = NS_SUCCEEDED(cookieMgr2->Add(NS_LITERAL_CSTRING("new.domain"),     // domain
                                           NS_LITERAL_CSTRING("/rabbit"),        // path
                                           NS_LITERAL_CSTRING("test3"),          // name
                                           NS_LITERAL_CSTRING("yes"),            // value
                                           false,                             // is secure
                                           false,                             // is httponly
                                           true,                              // is session
                                           INT64_MAX));                          // expiry time
      // confirm using enumerator
      nsCOMPtr<nsISimpleEnumerator> enumerator;
      rv[4] = NS_SUCCEEDED(cookieMgr->GetEnumerator(getter_AddRefs(enumerator)));
      int32_t i = 0;
      bool more;
      nsCOMPtr<nsICookie2> expiredCookie, newDomainCookie;
      while (NS_SUCCEEDED(enumerator->HasMoreElements(&more)) && more) {
        nsCOMPtr<nsISupports> cookie;
        if (NS_FAILED(enumerator->GetNext(getter_AddRefs(cookie)))) break;
        ++i;
        
        // keep tabs on the second and third cookies, so we can check them later
        nsCOMPtr<nsICookie2> cookie2(do_QueryInterface(cookie));
        if (!cookie2) break;
        nsAutoCString name;
        cookie2->GetName(name);
        if (name == NS_LITERAL_CSTRING("test2"))
          expiredCookie = cookie2;
        else if (name == NS_LITERAL_CSTRING("test3"))
          newDomainCookie = cookie2;
      }
      rv[5] = i == 3;
      // check the httpOnly attribute of the second cookie is honored
      GetACookie(cookieService, "http://cookiemgr.test/foo/", nullptr, getter_Copies(cookie));
      rv[6] = CheckResult(cookie.get(), MUST_CONTAIN, "test2=yes");
      GetACookieNoHttp(cookieService, "http://cookiemgr.test/foo/", getter_Copies(cookie));
      rv[7] = CheckResult(cookie.get(), MUST_NOT_CONTAIN, "test2=yes");
      // check CountCookiesFromHost()
      uint32_t hostCookies = 0;
      rv[8] = NS_SUCCEEDED(cookieMgr2->CountCookiesFromHost(NS_LITERAL_CSTRING("cookiemgr.test"), &hostCookies)) &&
              hostCookies == 2;
      // check CookieExists() using the third cookie
      bool found;
      rv[9] = NS_SUCCEEDED(cookieMgr2->CookieExists(newDomainCookie, &found)) && found;
      // remove the cookie, block it, and ensure it can't be added again
      rv[10] = NS_SUCCEEDED(cookieMgr->Remove(NS_LITERAL_CSTRING("new.domain"), // domain
                                              NS_LITERAL_CSTRING("test3"),      // name
                                              NS_LITERAL_CSTRING("/rabbit"),    // path
                                              true));                        // is blocked
      rv[11] = NS_SUCCEEDED(cookieMgr2->CookieExists(newDomainCookie, &found)) && !found;
      rv[12] = NS_SUCCEEDED(cookieMgr2->Add(NS_LITERAL_CSTRING("new.domain"),     // domain
                                            NS_LITERAL_CSTRING("/rabbit"),        // path
                                            NS_LITERAL_CSTRING("test3"),          // name
                                            NS_LITERAL_CSTRING("yes"),            // value
                                            false,                             // is secure
                                            false,                             // is httponly
                                            true,                              // is session
                                            INT64_MIN));                          // expiry time
      rv[13] = NS_SUCCEEDED(cookieMgr2->CookieExists(newDomainCookie, &found)) && !found;
      // sleep four seconds, to make sure the second cookie has expired
      PR_Sleep(4 * PR_TicksPerSecond());
      // check that both CountCookiesFromHost() and CookieExists() count the
      // expired cookie
      rv[14] = NS_SUCCEEDED(cookieMgr2->CountCookiesFromHost(NS_LITERAL_CSTRING("cookiemgr.test"), &hostCookies)) &&
              hostCookies == 2;
      rv[15] = NS_SUCCEEDED(cookieMgr2->CookieExists(expiredCookie, &found)) && found;
      // double-check RemoveAll() using the enumerator
      rv[16] = NS_SUCCEEDED(cookieMgr->RemoveAll());
      rv[17] = NS_SUCCEEDED(cookieMgr->GetEnumerator(getter_AddRefs(enumerator))) &&
               NS_SUCCEEDED(enumerator->HasMoreElements(&more)) &&
               !more;

      allTestsPassed = PrintResult(rv, 18) && allTestsPassed;


      // *** eviction and creation ordering tests
      sBuffer = PR_sprintf_append(sBuffer, "*** Beginning eviction and creation ordering tests...\n");

      // test that cookies are
      // a) returned by order of creation time (oldest first, newest last)
      // b) evicted by order of lastAccessed time, if the limit on cookies per host (50) is reached
      nsAutoCString name;
      nsAutoCString expected;
      for (int32_t i = 0; i < 60; ++i) {
        name = NS_LITERAL_CSTRING("test");
        name.AppendInt(i);
        name += NS_LITERAL_CSTRING("=creation");
        SetACookie(cookieService, "http://creation.ordering.tests/", nullptr, name.get(), nullptr);

        if (i >= 10) {
          expected += name;
          if (i < 59)
            expected += NS_LITERAL_CSTRING("; ");
        }
      }
      GetACookie(cookieService, "http://creation.ordering.tests/", nullptr, getter_Copies(cookie));
      rv[0] = CheckResult(cookie.get(), MUST_EQUAL, expected.get());

      allTestsPassed = PrintResult(rv, 1) && allTestsPassed;


      // XXX the following are placeholders: add these tests please!
      // *** "noncompliant cookie" tests
      // *** IP address tests
      // *** speed tests


      sBuffer = PR_sprintf_append(sBuffer, "\n*** Result: %s!\n\n", allTestsPassed ? "all tests passed" : "TEST(S) FAILED");
    }

    if (!allTestsPassed) {
      // print the entire log
      printf("%s", sBuffer);
      return 1;
    }

    PR_smprintf_free(sBuffer);
    sBuffer = nullptr;

    return 0;
}
