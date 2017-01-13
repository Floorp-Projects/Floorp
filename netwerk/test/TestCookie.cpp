/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TestCommon.h"
#include "gtest/gtest.h"
#include "nsIServiceManager.h"
#include "nsICookieService.h"
#include "nsICookieManager.h"
#include "nsICookieManager2.h"
#include "nsICookie2.h"
#include <stdio.h>
#include "plstr.h"
#include "prprf.h"
#include "nsNetUtil.h"
#include "nsISimpleEnumerator.h"
#include "nsServiceManagerUtils.h"
#include "nsNetCID.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "mozilla/Unused.h"
#include "nsIURI.h"

using mozilla::Unused;

static NS_DEFINE_CID(kCookieServiceCID, NS_COOKIESERVICE_CID);
static NS_DEFINE_CID(kPrefServiceCID,   NS_PREFSERVICE_CID);

// various pref strings
static const char kCookiesPermissions[] = "network.cookie.cookieBehavior";
static const char kCookiesLifetimeEnabled[] = "network.cookie.lifetime.enabled";
static const char kCookiesLifetimeDays[] = "network.cookie.lifetime.days";
static const char kCookiesLifetimeCurrentSession[] = "network.cookie.lifetime.behavior";
static const char kCookiesMaxPerHost[] = "network.cookie.maxPerHost";
static const char kCookieLeaveSecurityAlone[] = "network.cookie.leave-secure-alone";

#define OFFSET_ONE_WEEK int64_t(604800) * PR_USEC_PER_SEC
#define OFFSET_ONE_DAY int64_t(86400) * PR_USEC_PER_SEC

//Set server time or expiry time
void
SetTime(PRTime offsetTime,nsAutoCString& serverString,nsAutoCString& cookieString,bool expiry)
{
    char timeStringPreset[40];
    PRTime CurrentTime = PR_Now();
    PRTime SetCookieTime = CurrentTime + offsetTime;
    PRTime SetExpiryTime;
    if (expiry) {
      SetExpiryTime = SetCookieTime - OFFSET_ONE_DAY;
    } else {
      SetExpiryTime = SetCookieTime + OFFSET_ONE_DAY;
    }

    // Set server time string
    PRExplodedTime explodedTime;
    PR_ExplodeTime(SetCookieTime , PR_GMTParameters, &explodedTime);
    PR_FormatTimeUSEnglish(timeStringPreset, 40, "%c GMT", &explodedTime);
    serverString.Assign(timeStringPreset);

    // Set cookie string
    PR_ExplodeTime(SetExpiryTime , PR_GMTParameters, &explodedTime);
    PR_FormatTimeUSEnglish(timeStringPreset, 40, "%c GMT", &explodedTime);
    cookieString.Replace(0, strlen("test=expiry; expires=") + strlen(timeStringPreset) + 1, "test=expiry; expires=");
    cookieString.Append(timeStringPreset);
}

void
SetACookie(nsICookieService *aCookieService, const char *aSpec1, const char *aSpec2, const char* aCookieString, const char *aServerTime)
{
    nsCOMPtr<nsIURI> uri1, uri2;
    NS_NewURI(getter_AddRefs(uri1), aSpec1);
    if (aSpec2)
        NS_NewURI(getter_AddRefs(uri2), aSpec2);

    nsresult rv = aCookieService->SetCookieStringFromHttp(uri1, uri2, nullptr, (char *)aCookieString, aServerTime, nullptr);
    EXPECT_TRUE(NS_SUCCEEDED(rv));
}

void
SetACookieNoHttp(nsICookieService *aCookieService, const char *aSpec, const char* aCookieString)
{
    nsCOMPtr<nsIURI> uri;
    NS_NewURI(getter_AddRefs(uri), aSpec);

    nsresult rv = aCookieService->SetCookieString(uri, nullptr, (char *)aCookieString, nullptr);
    EXPECT_TRUE(NS_SUCCEEDED(rv));
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

    Unused << aCookieService->GetCookieStringFromHttp(uri1, uri2, nullptr, aCookie);
    return *aCookie != nullptr;
}

// returns true if cookie(s) for the given host were found; else false.
// the cookie string is returned via aCookie.
bool
GetACookieNoHttp(nsICookieService *aCookieService, const char *aSpec, char **aCookie)
{
    nsCOMPtr<nsIURI> uri;
    NS_NewURI(getter_AddRefs(uri), aSpec);

    Unused << aCookieService->GetCookieString(uri, nullptr, aCookie);
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
    aPrefBranch->SetBoolPref(kCookieLeaveSecurityAlone, true);
    // Set the base domain limit to 50 so we have a known value.
    aPrefBranch->SetIntPref(kCookiesMaxPerHost, 50);
}


TEST(TestCookie,TestCookieMain)
{
    nsresult rv0;

    nsCOMPtr<nsICookieService> cookieService =
        do_GetService(kCookieServiceCID, &rv0);
    ASSERT_TRUE(NS_SUCCEEDED(rv0));

    nsCOMPtr<nsIPrefBranch> prefBranch =
        do_GetService(kPrefServiceCID, &rv0);
    ASSERT_TRUE(NS_SUCCEEDED(rv0));

    InitPrefs(prefBranch);

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
     * NOTE: this testsuite is not yet comprehensive or complete, and is
     * somewhat contrived - still under development, and needs improving!
     */

    // test some basic variations of the domain & path
    SetACookie(cookieService, "http://www.basic.com", nullptr, "test=basic", nullptr);
    GetACookie(cookieService, "http://www.basic.com", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_EQUAL, "test=basic"));
    GetACookie(cookieService, "http://www.basic.com/testPath/testfile.txt", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_EQUAL, "test=basic"));
    GetACookie(cookieService, "http://www.basic.com./", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_BE_NULL));
    GetACookie(cookieService, "http://www.basic.com.", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_BE_NULL));
    GetACookie(cookieService, "http://www.basic.com./testPath/testfile.txt", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_BE_NULL));
    GetACookie(cookieService, "http://www.basic2.com/", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_BE_NULL));
    SetACookie(cookieService, "http://www.basic.com", nullptr, "test=basic; max-age=-1", nullptr);
    GetACookie(cookieService, "http://www.basic.com/", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_BE_NULL));

    // *** domain tests

    // test some variations of the domain & path, for different domains of
    // a domain cookie
    SetACookie(cookieService, "http://www.domain.com", nullptr, "test=domain; domain=domain.com", nullptr);
    GetACookie(cookieService, "http://domain.com", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_EQUAL, "test=domain"));
    GetACookie(cookieService, "http://domain.com.", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_BE_NULL));
    GetACookie(cookieService, "http://www.domain.com", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_EQUAL, "test=domain"));
    GetACookie(cookieService, "http://foo.domain.com", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_EQUAL, "test=domain"));
    SetACookie(cookieService, "http://www.domain.com", nullptr, "test=domain; domain=domain.com; max-age=-1", nullptr);
    GetACookie(cookieService, "http://domain.com", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_BE_NULL));

    SetACookie(cookieService, "http://www.domain.com", nullptr, "test=domain; domain=.domain.com", nullptr);
    GetACookie(cookieService, "http://domain.com", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_EQUAL, "test=domain"));
    GetACookie(cookieService, "http://www.domain.com", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_EQUAL, "test=domain"));
    GetACookie(cookieService, "http://bah.domain.com", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_EQUAL, "test=domain"));
    SetACookie(cookieService, "http://www.domain.com", nullptr, "test=domain; domain=.domain.com; max-age=-1", nullptr);
    GetACookie(cookieService, "http://domain.com", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_BE_NULL));

    SetACookie(cookieService, "http://www.domain.com", nullptr, "test=domain; domain=.foo.domain.com", nullptr);
    GetACookie(cookieService, "http://foo.domain.com", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_BE_NULL));

    SetACookie(cookieService, "http://www.domain.com", nullptr, "test=domain; domain=moose.com", nullptr);
    GetACookie(cookieService, "http://foo.domain.com", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_BE_NULL));

    SetACookie(cookieService, "http://www.domain.com", nullptr, "test=domain; domain=domain.com.", nullptr);
    GetACookie(cookieService, "http://foo.domain.com", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_BE_NULL));

    SetACookie(cookieService, "http://www.domain.com", nullptr, "test=domain; domain=..domain.com", nullptr);
    GetACookie(cookieService, "http://foo.domain.com", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_BE_NULL));

    SetACookie(cookieService, "http://www.domain.com", nullptr, "test=domain; domain=..domain.com.", nullptr);
    GetACookie(cookieService, "http://foo.domain.com", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_BE_NULL));

    SetACookie(cookieService, "http://path.net/path/file", nullptr, R"(test=taco; path="/bogus")", nullptr);
    GetACookie(cookieService, "http://path.net/path/file", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_EQUAL, "test=taco"));
    SetACookie(cookieService, "http://path.net/path/file", nullptr, "test=taco; max-age=-1", nullptr);
    GetACookie(cookieService, "http://path.net/path/file", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_BE_NULL));

    // *** path tests

    // test some variations of the domain & path, for different paths of
    // a path cookie
    SetACookie(cookieService, "http://path.net/path/file", nullptr, "test=path; path=/path", nullptr);
    GetACookie(cookieService, "http://path.net/path", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_EQUAL, "test=path"));
    GetACookie(cookieService, "http://path.net/path/", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_EQUAL, "test=path"));
    GetACookie(cookieService, "http://path.net/path/hithere.foo", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_EQUAL, "test=path"));
    GetACookie(cookieService, "http://path.net/path?hithere/foo", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_EQUAL, "test=path"));
    GetACookie(cookieService, "http://path.net/path2", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_BE_NULL));
    GetACookie(cookieService, "http://path.net/path2/", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_BE_NULL));
    SetACookie(cookieService, "http://path.net/path/file", nullptr, "test=path; path=/path; max-age=-1", nullptr);
    GetACookie(cookieService, "http://path.net/path/", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_BE_NULL));

    SetACookie(cookieService, "http://path.net/path/file", nullptr, "test=path; path=/path/", nullptr);
    GetACookie(cookieService, "http://path.net/path", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_EQUAL, "test=path"));
    GetACookie(cookieService, "http://path.net/path/", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_EQUAL, "test=path"));
    SetACookie(cookieService, "http://path.net/path/file", nullptr, "test=path; path=/path/; max-age=-1", nullptr);
    GetACookie(cookieService, "http://path.net/path/", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_BE_NULL));

    // note that a site can set a cookie for a path it's not on.
    // this is an intentional deviation from spec (see comments in
    // nsCookieService::CheckPath()), so we test this functionality too
    SetACookie(cookieService, "http://path.net/path/file", nullptr, "test=path; path=/foo/", nullptr);
    GetACookie(cookieService, "http://path.net/path", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_BE_NULL));
    GetACookie(cookieService, "http://path.net/foo", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_EQUAL, "test=path"));
    SetACookie(cookieService, "http://path.net/path/file", nullptr, "test=path; path=/foo/; max-age=-1", nullptr);
    GetACookie(cookieService, "http://path.net/foo/", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_BE_NULL));

    // bug 373228: make sure cookies with paths longer than 1024 bytes,
    // and cookies with paths or names containing tabs, are rejected.
    // the following cookie has a path > 1024 bytes explicitly specified in the cookie
    SetACookie(cookieService, "http://path.net/", nullptr, "test=path; path=/1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890/", nullptr);
    GetACookie(cookieService, "http://path.net/1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_BE_NULL));
    // the following cookie has a path > 1024 bytes implicitly specified by the uri path
    SetACookie(cookieService, "http://path.net/1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890/", nullptr, "test=path", nullptr);
    GetACookie(cookieService, "http://path.net/1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890/", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_BE_NULL));
    // the following cookie includes a tab in the path
    SetACookie(cookieService, "http://path.net/", nullptr, "test=path; path=/foo\tbar/", nullptr);
    GetACookie(cookieService, "http://path.net/foo\tbar/", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_BE_NULL));
    // the following cookie includes a tab in the name
    SetACookie(cookieService, "http://path.net/", nullptr, "test\ttabs=tab", nullptr);
    GetACookie(cookieService, "http://path.net/", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_BE_NULL));
    // the following cookie includes a tab in the value - allowed
    SetACookie(cookieService, "http://path.net/", nullptr, "test=tab\ttest", nullptr);
    GetACookie(cookieService, "http://path.net/", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_EQUAL, "test=tab\ttest"));
    SetACookie(cookieService, "http://path.net/", nullptr, "test=tab\ttest; max-age=-1", nullptr);
    GetACookie(cookieService, "http://path.net/", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_BE_NULL));


    // *** expiry & deletion tests
    // XXX add server time str parsing tests here

    // test some variations of the expiry time,
    // and test deletion of previously set cookies
    SetACookie(cookieService, "http://expireme.org/", nullptr, "test=expiry; max-age=-1", nullptr);
    GetACookie(cookieService, "http://expireme.org/", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_BE_NULL));
    SetACookie(cookieService, "http://expireme.org/", nullptr, "test=expiry; max-age=0", nullptr);
    GetACookie(cookieService, "http://expireme.org/", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_BE_NULL));
    SetACookie(cookieService, "http://expireme.org/", nullptr, "test=expiry; expires=bad", nullptr);
    GetACookie(cookieService, "http://expireme.org/", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_EQUAL, "test=expiry"));
    SetACookie(cookieService, "http://expireme.org/", nullptr, "test=expiry; expires=Thu, 10 Apr 1980 16:33:12 GMT", nullptr);
    GetACookie(cookieService, "http://expireme.org/", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_BE_NULL));
    SetACookie(cookieService, "http://expireme.org/", nullptr, R"(test=expiry; expires="Thu, 10 Apr 1980 16:33:12 GMT)", nullptr);
    GetACookie(cookieService, "http://expireme.org/", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_BE_NULL));
    SetACookie(cookieService, "http://expireme.org/", nullptr, R"(test=expiry; expires="Thu, 10 Apr 1980 16:33:12 GMT")", nullptr);
    GetACookie(cookieService, "http://expireme.org/", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_BE_NULL));

    SetACookie(cookieService, "http://expireme.org/", nullptr, "test=expiry; max-age=60", nullptr);
    GetACookie(cookieService, "http://expireme.org/", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_EQUAL, "test=expiry"));
    SetACookie(cookieService, "http://expireme.org/", nullptr, "test=expiry; max-age=-20", nullptr);
    GetACookie(cookieService, "http://expireme.org/", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_BE_NULL));
    SetACookie(cookieService, "http://expireme.org/", nullptr, "test=expiry; max-age=60", nullptr);
    GetACookie(cookieService, "http://expireme.org/", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_EQUAL, "test=expiry"));
    SetACookie(cookieService, "http://expireme.org/", nullptr, "test=expiry; expires=Thu, 10 Apr 1980 16:33:12 GMT", nullptr);
    GetACookie(cookieService, "http://expireme.org/", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_BE_NULL));
    SetACookie(cookieService, "http://expireme.org/", nullptr, "test=expiry; max-age=60", nullptr);
    SetACookie(cookieService, "http://expireme.org/", nullptr, "newtest=expiry; max-age=60", nullptr);
    GetACookie(cookieService, "http://expireme.org/", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_CONTAIN, "test=expiry"));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_CONTAIN, "newtest=expiry"));
    SetACookie(cookieService, "http://expireme.org/", nullptr, "test=differentvalue; max-age=0", nullptr);
    GetACookie(cookieService, "http://expireme.org/", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_EQUAL, "newtest=expiry"));
    SetACookie(cookieService, "http://expireme.org/", nullptr, "newtest=evendifferentvalue; max-age=0", nullptr);
    GetACookie(cookieService, "http://expireme.org/", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_BE_NULL));

    SetACookie(cookieService, "http://foo.expireme.org/", nullptr, "test=expiry; domain=.expireme.org; max-age=60", nullptr);
    GetACookie(cookieService, "http://expireme.org/", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_EQUAL, "test=expiry"));
    SetACookie(cookieService, "http://bar.expireme.org/", nullptr, "test=differentvalue; domain=.expireme.org; max-age=0", nullptr);
    GetACookie(cookieService, "http://expireme.org/", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_BE_NULL));

    nsAutoCString ServerTime;
    nsAutoCString CookieString;

    SetTime(-OFFSET_ONE_WEEK, ServerTime, CookieString, true);
    SetACookie(cookieService, "http://expireme.org/", nullptr, CookieString.get(), ServerTime.get());
    GetACookie(cookieService, "http://expireme.org/", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_BE_NULL));
    // Set server time earlier than client time for one year + one day, and expirty time earlier than server time for one day.
    SetTime(-(OFFSET_ONE_DAY + OFFSET_ONE_WEEK), ServerTime, CookieString, false);
    SetACookie(cookieService, "http://expireme.org/", nullptr, CookieString.get(), ServerTime.get());
    GetACookie(cookieService, "http://expireme.org/", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_BE_NULL));
    // Set server time later than client time for one year, and expiry time later than server time for one day.
    SetTime(OFFSET_ONE_WEEK, ServerTime, CookieString, false);
    SetACookie(cookieService, "http://expireme.org/", nullptr, CookieString.get(), ServerTime.get());
    GetACookie(cookieService, "http://expireme.org/", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_EQUAL, "test=expiry"));
    // Set server time later than client time for one year + one day, and expiry time earlier than server time for one day.
    SetTime((OFFSET_ONE_DAY + OFFSET_ONE_WEEK), ServerTime, CookieString, true);
    SetACookie(cookieService, "http://expireme.org/", nullptr, CookieString.get(), ServerTime.get());
    GetACookie(cookieService, "http://expireme.org/", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_EQUAL, "test=expiry"));

    // *** multiple cookie tests

    // test the setting of multiple cookies, and test the order of precedence
    // (a later cookie overwriting an earlier one, in the same header string)
    SetACookie(cookieService, "http://multiple.cookies/", nullptr, "test=multiple; domain=.multiple.cookies \n test=different \n test=same; domain=.multiple.cookies \n newtest=ciao \n newtest=foo; max-age=-6 \n newtest=reincarnated", nullptr);
    GetACookie(cookieService, "http://multiple.cookies/", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_NOT_CONTAIN, "test=multiple"));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_CONTAIN, "test=different"));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_CONTAIN, "test=same"));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_NOT_CONTAIN, "newtest=ciao"));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_NOT_CONTAIN, "newtest=foo"));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_CONTAIN, "newtest=reincarnated"));
    SetACookie(cookieService, "http://multiple.cookies/", nullptr, "test=expiry; domain=.multiple.cookies; max-age=0", nullptr);
    GetACookie(cookieService, "http://multiple.cookies/", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_NOT_CONTAIN, "test=same"));
    SetACookie(cookieService, "http://multiple.cookies/", nullptr,  "\n test=different; max-age=0 \n", nullptr);
    GetACookie(cookieService, "http://multiple.cookies/", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_NOT_CONTAIN, "test=different"));
    SetACookie(cookieService, "http://multiple.cookies/", nullptr,  "newtest=dead; max-age=0", nullptr);
    GetACookie(cookieService, "http://multiple.cookies/", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_BE_NULL));


    // *** parser tests

    // test the cookie header parser, under various circumstances.
    SetACookie(cookieService, "http://parser.test/", nullptr, "test=parser; domain=.parser.test; ;; ;=; ,,, ===,abc,=; abracadabra! max-age=20;=;;", nullptr);
    GetACookie(cookieService, "http://parser.test/", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_EQUAL, "test=parser"));
    SetACookie(cookieService, "http://parser.test/", nullptr, "test=parser; domain=.parser.test; max-age=0", nullptr);
    GetACookie(cookieService, "http://parser.test/", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_BE_NULL));
    SetACookie(cookieService, "http://parser.test/", nullptr, "test=\"fubar! = foo;bar\\\";\" parser; domain=.parser.test; max-age=6\nfive; max-age=2.63,", nullptr);
    GetACookie(cookieService, "http://parser.test/", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_CONTAIN, R"(test="fubar! = foo)"));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_CONTAIN, "five"));
    SetACookie(cookieService, "http://parser.test/", nullptr, "test=kill; domain=.parser.test; max-age=0 \n five; max-age=0", nullptr);
    GetACookie(cookieService, "http://parser.test/", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_BE_NULL));

    // test the handling of VALUE-only cookies (see bug 169091),
    // i.e. "six" should assume an empty NAME, which allows other VALUE-only
    // cookies to overwrite it
    SetACookie(cookieService, "http://parser.test/", nullptr, "six", nullptr);
    GetACookie(cookieService, "http://parser.test/", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_EQUAL, "six"));
    SetACookie(cookieService, "http://parser.test/", nullptr, "seven", nullptr);
    GetACookie(cookieService, "http://parser.test/", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_EQUAL, "seven"));
    SetACookie(cookieService, "http://parser.test/", nullptr, " =eight", nullptr);
    GetACookie(cookieService, "http://parser.test/", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_EQUAL, "eight"));
    SetACookie(cookieService, "http://parser.test/", nullptr, "test=six", nullptr);
    GetACookie(cookieService, "http://parser.test/", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_CONTAIN, "test=six"));

    // *** path ordering tests

    // test that cookies are returned in path order - longest to shortest.
    // if the header doesn't specify a path, it's taken from the host URI.
    SetACookie(cookieService, "http://multi.path.tests/", nullptr, "test1=path; path=/one/two/three", nullptr);
    SetACookie(cookieService, "http://multi.path.tests/", nullptr, "test2=path; path=/one \n test3=path; path=/one/two/three/four \n test4=path; path=/one/two \n test5=path; path=/one/two/", nullptr);
    SetACookie(cookieService, "http://multi.path.tests/one/two/three/four/five/", nullptr, "test6=path", nullptr);
    SetACookie(cookieService, "http://multi.path.tests/one/two/three/four/five/six/", nullptr, "test7=path; path=", nullptr);
    SetACookie(cookieService, "http://multi.path.tests/", nullptr, "test8=path; path=/", nullptr);
    GetACookie(cookieService, "http://multi.path.tests/one/two/three/four/five/six/", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_EQUAL, "test7=path; test6=path; test3=path; test1=path; test5=path; test4=path; test2=path; test8=path"));


    // *** httponly tests

    // Since this cookie is NOT set via http, setting it fails
    SetACookieNoHttp(cookieService, "http://httponly.test/", "test=httponly; httponly");
    GetACookie(cookieService, "http://httponly.test/", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_BE_NULL));
    // Since this cookie is set via http, it can be retrieved
    SetACookie(cookieService, "http://httponly.test/", nullptr, "test=httponly; httponly", nullptr);
    GetACookie(cookieService, "http://httponly.test/", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_EQUAL, "test=httponly"));
    // ... but not by web content
    GetACookieNoHttp(cookieService, "http://httponly.test/", getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_BE_NULL));
    // Non-Http cookies should not replace HttpOnly cookies
    SetACookie(cookieService, "http://httponly.test/", nullptr, "test=httponly; httponly", nullptr);
    SetACookieNoHttp(cookieService, "http://httponly.test/", "test=not-httponly");
    GetACookie(cookieService, "http://httponly.test/", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_EQUAL, "test=httponly"));
    // ... and, if an HttpOnly cookie already exists, should not be set at all
    GetACookieNoHttp(cookieService, "http://httponly.test/", getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_BE_NULL));
    // Non-Http cookies should not delete HttpOnly cookies
    SetACookie(cookieService, "http://httponly.test/", nullptr, "test=httponly; httponly", nullptr);
    SetACookieNoHttp(cookieService, "http://httponly.test/", "test=httponly; max-age=-1");
    GetACookie(cookieService, "http://httponly.test/", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_EQUAL, "test=httponly"));
    // ... but HttpOnly cookies should
    SetACookie(cookieService, "http://httponly.test/", nullptr, "test=httponly; httponly; max-age=-1", nullptr);
    GetACookie(cookieService, "http://httponly.test/", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_BE_NULL));
    // Non-Httponly cookies can replace HttpOnly cookies when set over http
    SetACookie(cookieService, "http://httponly.test/", nullptr, "test=httponly; httponly", nullptr);
    SetACookie(cookieService, "http://httponly.test/", nullptr, "test=not-httponly", nullptr);
    GetACookieNoHttp(cookieService, "http://httponly.test/", getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_EQUAL, "test=not-httponly"));
    // scripts should not be able to set httponly cookies by replacing an existing non-httponly cookie
    SetACookie(cookieService, "http://httponly.test/", nullptr, "test=not-httponly", nullptr);
    SetACookieNoHttp(cookieService, "http://httponly.test/", "test=httponly; httponly");
    GetACookieNoHttp(cookieService, "http://httponly.test/", getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_EQUAL, "test=not-httponly"));

    // *** Cookie prefix tests

    // prefixed cookies can't be set from insecure HTTP
    SetACookie(cookieService, "http://prefixed.test/", nullptr, "__Secure-test1=test", nullptr);
    SetACookie(cookieService, "http://prefixed.test/", nullptr, "__Secure-test2=test; secure", nullptr);
    SetACookie(cookieService, "http://prefixed.test/", nullptr, "__Host-test1=test", nullptr);
    SetACookie(cookieService, "http://prefixed.test/", nullptr, "__Host-test2=test; secure", nullptr);
    GetACookie(cookieService, "http://prefixed.test/", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_BE_NULL));

    // prefixed cookies won't be set without the secure flag
    SetACookie(cookieService, "https://prefixed.test/", nullptr, "__Secure-test=test", nullptr);
    SetACookie(cookieService, "https://prefixed.test/", nullptr, "__Host-test=test", nullptr);
    GetACookie(cookieService, "https://prefixed.test/", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_BE_NULL));

    // prefixed cookies can be set when done correctly
    SetACookie(cookieService, "https://prefixed.test/", nullptr, "__Secure-test=test; secure", nullptr);
    SetACookie(cookieService, "https://prefixed.test/", nullptr, "__Host-test=test; secure", nullptr);
    GetACookie(cookieService, "https://prefixed.test/", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_CONTAIN, "__Secure-test=test"));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_CONTAIN, "__Host-test=test"));

    // but when set must not be returned to the host insecurely
    GetACookie(cookieService, "http://prefixed.test/", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_BE_NULL));

    // Host-prefixed cookies cannot specify a domain
    SetACookie(cookieService, "https://host.prefixed.test/", nullptr, "__Host-a=test; secure; domain=prefixed.test", nullptr);
    SetACookie(cookieService, "https://host.prefixed.test/", nullptr, "__Host-b=test; secure; domain=.prefixed.test", nullptr);
    SetACookie(cookieService, "https://host.prefixed.test/", nullptr, "__Host-c=test; secure; domain=host.prefixed.test", nullptr);
    SetACookie(cookieService, "https://host.prefixed.test/", nullptr, "__Host-d=test; secure; domain=.host.prefixed.test", nullptr);
    GetACookie(cookieService, "https://host.prefixed.test/", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_BE_NULL));

    // Host-prefixed cookies can only have a path of "/"
    SetACookie(cookieService, "https://host.prefixed.test/some/path", nullptr, "__Host-e=test; secure", nullptr);
    SetACookie(cookieService, "https://host.prefixed.test/some/path", nullptr, "__Host-f=test; secure; path=/", nullptr);
    SetACookie(cookieService, "https://host.prefixed.test/some/path", nullptr, "__Host-g=test; secure; path=/some", nullptr);
    GetACookie(cookieService, "https://host.prefixed.test/", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_EQUAL, "__Host-f=test"));

    // *** leave-secure-alone tests

    // testing items 0 & 1 for 3.1 of spec Deprecate modification of ’secure’
    // cookies from non-secure origins
    SetACookie(cookieService, "http://www.security.test/", nullptr, "test=non-security; secure", nullptr);
    GetACookieNoHttp(cookieService, "https://www.security.test/", getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_BE_NULL));
    SetACookie(cookieService, "https://www.security.test/path/", nullptr, "test=security; secure; path=/path/", nullptr);
    GetACookieNoHttp(cookieService, "https://www.security.test/path/", getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_EQUAL, "test=security"));
    // testing items 2 & 3 & 4 for 3.2 of spec Deprecate modification of ’secure’
    // cookies from non-secure origins
    // Secure site can modify cookie value
    SetACookie(cookieService, "https://www.security.test/path/", nullptr, "test=security2; secure; path=/path/", nullptr);
    GetACookieNoHttp(cookieService, "https://www.security.test/path/", getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_EQUAL, "test=security2"));
    // If new cookie contains same name, same host and partially matching path with
    // an existing security cookie on non-security site, it can't modify an existing
    // security cookie.
    SetACookie(cookieService, "http://www.security.test/path/foo/", nullptr, "test=non-security; path=/path/foo", nullptr);
    GetACookieNoHttp(cookieService, "https://www.security.test/path/foo/", getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_EQUAL, "test=security2"));
    // Non-secure cookie can set by same name, same host and non-matching path.
    SetACookie(cookieService, "http://www.security.test/bar/", nullptr, "test=non-security; path=/bar", nullptr);
    GetACookieNoHttp(cookieService, "http://www.security.test/bar/", getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_EQUAL, "test=non-security"));
    // Modify value and downgrade secure level.
    SetACookie(cookieService, "https://www.security.test/", nullptr, "test_modify_cookie=security-cookie; secure; domain=.security.test", nullptr);
    GetACookieNoHttp(cookieService, "https://www.security.test/", getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_EQUAL, "test_modify_cookie=security-cookie"));
    SetACookie(cookieService, "https://www.security.test/", nullptr, "test_modify_cookie=non-security-cookie; domain=.security.test", nullptr);
    GetACookieNoHttp(cookieService, "https://www.security.test/", getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_EQUAL, "test_modify_cookie=non-security-cookie"));
    // Test the non-security cookie can set when domain or path not same to secure cookie of same name.
    SetACookie(cookieService, "https://www.security.test/", nullptr, "test=security3", nullptr);
    GetACookieNoHttp(cookieService, "http://www.security.test/", getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_CONTAIN, "test=security3"));
    SetACookie(cookieService, "http://www.security.test/", nullptr, "test=non-security2; domain=security.test", nullptr);
    GetACookieNoHttp(cookieService, "http://www.security.test/", getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_CONTAIN, "test=non-security2"));

    // *** nsICookieManager{2} interface tests
    nsCOMPtr<nsICookieManager> cookieMgr = do_GetService(NS_COOKIEMANAGER_CONTRACTID, &rv0);
    ASSERT_TRUE(NS_SUCCEEDED(rv0));

    nsCOMPtr<nsICookieManager2> cookieMgr2 = do_QueryInterface(cookieMgr);
    ASSERT_TRUE(cookieMgr2);

    mozilla::OriginAttributes attrs;

    // first, ensure a clean slate
    EXPECT_TRUE(NS_SUCCEEDED(cookieMgr->RemoveAll()));
    // add some cookies
    EXPECT_TRUE(NS_SUCCEEDED(cookieMgr2->AddNative(NS_LITERAL_CSTRING("cookiemgr.test"), // domain
                                                   NS_LITERAL_CSTRING("/foo"),           // path
                                                   NS_LITERAL_CSTRING("test1"),          // name
                                                   NS_LITERAL_CSTRING("yes"),            // value
                                                   false,                             // is secure
                                                   false,                             // is httponly
                                                   true,                              // is session
                                                   INT64_MAX,                            // expiry time
                                                   &attrs)));                         // originAttributes
    EXPECT_TRUE(NS_SUCCEEDED(cookieMgr2->AddNative(NS_LITERAL_CSTRING("cookiemgr.test"), // domain
                                                   NS_LITERAL_CSTRING("/foo"),           // path
                                                   NS_LITERAL_CSTRING("test2"),          // name
                                                   NS_LITERAL_CSTRING("yes"),            // value
                                                   false,                             // is secure
                                                   true,                              // is httponly
                                                   true,                              // is session
                                                   PR_Now() / PR_USEC_PER_SEC + 2,       // expiry time
                                                   &attrs)));                         // originAttributes
    EXPECT_TRUE(NS_SUCCEEDED(cookieMgr2->AddNative(NS_LITERAL_CSTRING("new.domain"),     // domain
                                                   NS_LITERAL_CSTRING("/rabbit"),        // path
                                                   NS_LITERAL_CSTRING("test3"),          // name
                                                   NS_LITERAL_CSTRING("yes"),            // value
                                                   false,                             // is secure
                                                   false,                             // is httponly
                                                   true,                              // is session
                                                   INT64_MAX,                            // expiry time
                                                   &attrs)));                         // originAttributes
    // confirm using enumerator
    nsCOMPtr<nsISimpleEnumerator> enumerator;
    EXPECT_TRUE(NS_SUCCEEDED(cookieMgr->GetEnumerator(getter_AddRefs(enumerator))));
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
        if (name.EqualsLiteral("test2"))
            expiredCookie = cookie2;
        else if (name.EqualsLiteral("test3"))
            newDomainCookie = cookie2;
    }
    EXPECT_EQ(i, 3);
    // check the httpOnly attribute of the second cookie is honored
    GetACookie(cookieService, "http://cookiemgr.test/foo/", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_CONTAIN, "test2=yes"));
    GetACookieNoHttp(cookieService, "http://cookiemgr.test/foo/", getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_NOT_CONTAIN, "test2=yes"));
    // check CountCookiesFromHost()
    uint32_t hostCookies = 0;
    EXPECT_TRUE(NS_SUCCEEDED(cookieMgr2->CountCookiesFromHost(NS_LITERAL_CSTRING("cookiemgr.test"), &hostCookies)));
    EXPECT_EQ(hostCookies, 2u);
    // check CookieExistsNative() using the third cookie
    bool found;
    EXPECT_TRUE(NS_SUCCEEDED(cookieMgr2->CookieExistsNative(newDomainCookie, &attrs,  &found)));
    EXPECT_TRUE(found);


    // remove the cookie, block it, and ensure it can't be added again
    EXPECT_TRUE(NS_SUCCEEDED(cookieMgr->RemoveNative(NS_LITERAL_CSTRING("new.domain"), // domain
                                                     NS_LITERAL_CSTRING("test3"),      // name
                                                     NS_LITERAL_CSTRING("/rabbit"),    // path
                                                     true,                             // is blocked
                                                     &attrs)));                         // originAttributes
    EXPECT_TRUE(NS_SUCCEEDED(cookieMgr2->CookieExistsNative(newDomainCookie, &attrs, &found)));
    EXPECT_FALSE(found);
    EXPECT_TRUE(NS_SUCCEEDED(cookieMgr2->AddNative(NS_LITERAL_CSTRING("new.domain"),     // domain
                                                   NS_LITERAL_CSTRING("/rabbit"),        // path
                                                   NS_LITERAL_CSTRING("test3"),          // name
                                                   NS_LITERAL_CSTRING("yes"),            // value
                                                   false,                             // is secure
                                                   false,                             // is httponly
                                                   true,                              // is session
                                                   INT64_MIN,                            // expiry time
                                                   &attrs)));                         // originAttributes
    EXPECT_TRUE(NS_SUCCEEDED(cookieMgr2->CookieExistsNative(newDomainCookie, &attrs, &found)));
    EXPECT_FALSE(found);
    // sleep four seconds, to make sure the second cookie has expired
    PR_Sleep(4 * PR_TicksPerSecond());
    // check that both CountCookiesFromHost() and CookieExistsNative() count the
    // expired cookie
    EXPECT_TRUE(NS_SUCCEEDED(cookieMgr2->CountCookiesFromHost(NS_LITERAL_CSTRING("cookiemgr.test"), &hostCookies)));
    EXPECT_EQ(hostCookies, 2u);
    EXPECT_TRUE(NS_SUCCEEDED(cookieMgr2->CookieExistsNative(expiredCookie, &attrs, &found)));
    EXPECT_TRUE(found);
    // double-check RemoveAll() using the enumerator
    EXPECT_TRUE(NS_SUCCEEDED(cookieMgr->RemoveAll()));
    EXPECT_TRUE(NS_SUCCEEDED(cookieMgr->GetEnumerator(getter_AddRefs(enumerator))) &&
                NS_SUCCEEDED(enumerator->HasMoreElements(&more)) &&
                !more);

    // *** eviction and creation ordering tests

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
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_EQUAL, expected.get()));

    // *** eviction and creation ordering tests after enable network.cookie.leave-secure-alone
    // reset cookie
    cookieMgr->RemoveAll();

    for (int32_t i = 0; i < 60; ++i) {
      name = NS_LITERAL_CSTRING("test");
      name.AppendInt(i);
      name += NS_LITERAL_CSTRING("=delete_non_security");

      // Create 50 cookies that include the secure flag.
      if (i < 50) {
        name += NS_LITERAL_CSTRING("; secure");
        SetACookie(cookieService, "https://creation.ordering.tests/", nullptr, name.get(), nullptr);
      } else {
        // non-security cookies will be removed beside the latest cookie that be created.
        SetACookie(cookieService, "http://creation.ordering.tests/", nullptr, name.get(), nullptr);
      }
    }
    GetACookie(cookieService, "http://creation.ordering.tests/", nullptr, getter_Copies(cookie));
    EXPECT_TRUE(CheckResult(cookie.get(), MUST_BE_NULL));

    // XXX the following are placeholders: add these tests please!
    // *** "noncompliant cookie" tests
    // *** IP address tests
    // *** speed tests
}
