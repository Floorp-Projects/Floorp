/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Witte (dwitte@stanford.edu)
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

#include "TestCommon.h"
#include "nsIServiceManager.h"
#include "nsICookieService.h"
#include <stdio.h>
#include "plstr.h"
#include "nsNetUtil.h"
#include "nsNetCID.h"
#include "nsXPIDLString.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"

static NS_DEFINE_CID(kCookieServiceCID, NS_COOKIESERVICE_CID);
static NS_DEFINE_CID(kPrefServiceCID,   NS_PREFSERVICE_CID);

// various pref strings
static const char kCookiesPermissions[] = "network.cookie.cookieBehavior";
static const char kCookiesDisabledForMailNews[] = "network.cookie.disableCookieForMailNews";
static const char kCookiesLifetimeEnabled[] = "network.cookie.lifetime.enabled";
static const char kCookiesLifetimeDays[] = "network.cookie.lifetime.days";
static const char kCookiesLifetimeCurrentSession[] = "network.cookie.lifetime.behavior";
static const char kCookiesP3PString[] = "network.cookie.p3p";
static const char kCookiesAskPermission[] = "network.cookie.warnAboutCookies";

nsresult
SetACookie(nsICookieService *aCookieService, const char *aSpec1, const char *aSpec2, const char* aCookieString, const char *aServerTime)
{
    nsCOMPtr<nsIURI> uri1, uri2;
    NS_NewURI(getter_AddRefs(uri1), aSpec1);
    if (aSpec2)
        NS_NewURI(getter_AddRefs(uri2), aSpec2);

    printf("    for host \"%s\": SET ", aSpec1);
    nsresult rv = aCookieService->SetCookieStringFromHttp(uri1, uri2, nsnull, (char *)aCookieString, aServerTime, nsnull);
    // the following code is useless. the cookieservice blindly returns NS_OK
    // from SetCookieString. we have to call GetCookie to see if the cookie was
    // set correctly...
    if (NS_FAILED(rv)) {
        printf("nothing\n");
    } else {
        printf("\"%s\"\n", aCookieString);
    }
    return rv;
}

// returns PR_TRUE if cookie(s) for the given host were found; else PR_FALSE.
// the cookie string is returned via aCookie.
PRBool
GetACookie(nsICookieService *aCookieService, const char *aSpec1, const char *aSpec2, char **aCookie)
{
    nsCOMPtr<nsIURI> uri1, uri2;
    NS_NewURI(getter_AddRefs(uri1), aSpec1);
    if (aSpec2)
        NS_NewURI(getter_AddRefs(uri2), aSpec2);

    printf("             \"%s\": GOT ", aSpec1);
    nsresult rv = aCookieService->GetCookieStringFromHttp(uri1, uri2, nsnull, aCookie);
    if (NS_FAILED(rv)) printf("XXX GetCookieString() failed!\n");
    if (!*aCookie) {
        printf("nothing\n");
    } else {
        printf("\"%s\"\n", *aCookie);
    }
    return *aCookie != nsnull;
}

// some #defines for comparison rules
#define MUST_BE_NULL     0
#define MUST_EQUAL       1
#define MUST_CONTAIN     2
#define MUST_NOT_CONTAIN 3

// a simple helper function to improve readability:
// takes one of the #defined rules above, and performs the appropriate test.
// PR_TRUE means the test passed; PR_FALSE means the test failed.
static inline PRBool
CheckResult(const char *aLhs, PRUint32 aRule, const char *aRhs = nsnull)
{
    switch (aRule) {
        case MUST_BE_NULL:
            return !aLhs;

        case MUST_EQUAL:
            return !PL_strcmp(aLhs, aRhs);

        case MUST_CONTAIN:
            return PL_strstr(aLhs, aRhs) != nsnull;

        case MUST_NOT_CONTAIN:
            return PL_strstr(aLhs, aRhs) == nsnull;

        default:
            return PR_FALSE; // failure
    }
}

// helper function that ensures the first aSize elements of aResult are
// PR_TRUE (i.e. all tests succeeded). prints the result of the tests (if any
// tests failed, it prints the zero-based index of each failed test).
PRBool
PrintResult(const PRBool aResult[], PRUint32 aSize)
{
    PRBool failed = PR_FALSE;
    printf("*** tests ");
    for (PRUint32 i = 0; i < aSize; ++i) {
        if (!aResult[i]) {
            failed = PR_TRUE;
            printf("%d ", i);
        }
    }
    if (failed) {
        printf("FAILED!\a\n");
    } else {
        printf("passed.\n");
    }
    return !failed;
}

void
InitPrefs(nsIPrefBranch *aPrefBranch)
{
    // init some relevant prefs, so the tests don't go awry.
    // we use the most restrictive set of prefs we can.
    aPrefBranch->SetIntPref(kCookiesPermissions, 1); // 'reject foreign'
    aPrefBranch->SetBoolPref(kCookiesDisabledForMailNews, PR_TRUE);
    aPrefBranch->SetBoolPref(kCookiesLifetimeEnabled, PR_TRUE);
    aPrefBranch->SetIntPref(kCookiesLifetimeCurrentSession, 0);
    aPrefBranch->SetIntPref(kCookiesLifetimeDays, 1);
    aPrefBranch->SetBoolPref(kCookiesAskPermission, PR_FALSE);
}

int
main(PRInt32 argc, char *argv[])
{
    if (test_common_init(&argc, &argv) != 0)
        return -1;

    nsresult rv0 = NS_InitXPCOM2(nsnull, nsnull, nsnull);
    if (NS_FAILED(rv0)) return -1;

    nsCOMPtr<nsICookieService> cookieService =
             do_GetService(kCookieServiceCID, &rv0);
	  if (NS_FAILED(rv0)) return -1;

    nsCOMPtr<nsIPrefBranch> prefBranch =
             do_GetService(kPrefServiceCID, &rv0);
    if (NS_FAILED(rv0)) return -1;

    InitPrefs(prefBranch);

    PRBool allTestsPassed = PR_TRUE;
    PRBool rv[20];
    nsXPIDLCString cookie;

    // call NS_NewURI just to force chrome registrations, so all our
    // printf'ed messages are together.
    {
        nsCOMPtr<nsIURI> foo;
        NS_NewURI(getter_AddRefs(foo), "http://foo.com");
    }
    printf("\n");


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
     * outcomes (all successes), by PrintResult()). the overall result of all
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
    printf("*** Beginning basic tests...\n");

    // test some basic variations of the domain & path
    SetACookie(cookieService, "http://www.basic.com", nsnull, "test=basic", nsnull);
    GetACookie(cookieService, "http://www.basic.com", nsnull, getter_Copies(cookie));
    rv[0] = CheckResult(cookie, MUST_EQUAL, "test=basic");
    GetACookie(cookieService, "http://www.basic.com/testPath/testfile.txt", nsnull, getter_Copies(cookie));
    rv[1] = CheckResult(cookie, MUST_EQUAL, "test=basic");
    GetACookie(cookieService, "http://www.basic.com./", nsnull, getter_Copies(cookie));
    rv[2] = CheckResult(cookie, MUST_EQUAL, "test=basic");
    GetACookie(cookieService, "http://www.basic.com.", nsnull, getter_Copies(cookie));
    rv[3] = CheckResult(cookie, MUST_EQUAL, "test=basic");
    GetACookie(cookieService, "http://www.basic.com./testPath/testfile.txt", nsnull, getter_Copies(cookie));
    rv[4] = CheckResult(cookie, MUST_EQUAL, "test=basic");
    GetACookie(cookieService, "http://www.basic2.com/", nsnull, getter_Copies(cookie));
    rv[5] = CheckResult(cookie, MUST_BE_NULL);
    SetACookie(cookieService, "http://www.basic.com", nsnull, "test=basic; max-age=-1", nsnull);
    GetACookie(cookieService, "http://www.basic.com/", nsnull, getter_Copies(cookie));
    rv[6] = CheckResult(cookie, MUST_BE_NULL);

    allTestsPassed = PrintResult(rv, 7) && allTestsPassed;


    // *** domain tests
    printf("*** Beginning domain tests...\n");

    // test some variations of the domain & path, for different domains of
    // a domain cookie
    SetACookie(cookieService, "http://www.domain.com", nsnull, "test=domain; domain=domain.com", nsnull);
    GetACookie(cookieService, "http://domain.com", nsnull, getter_Copies(cookie));
    rv[0] = CheckResult(cookie, MUST_EQUAL, "test=domain");
    GetACookie(cookieService, "http://domain.com.", nsnull, getter_Copies(cookie));
    rv[1] = CheckResult(cookie, MUST_EQUAL, "test=domain");
    GetACookie(cookieService, "http://www.domain.com", nsnull, getter_Copies(cookie));
    rv[2] = CheckResult(cookie, MUST_EQUAL, "test=domain");
    GetACookie(cookieService, "http://foo.domain.com", nsnull, getter_Copies(cookie));
    rv[3] = CheckResult(cookie, MUST_EQUAL, "test=domain");
    SetACookie(cookieService, "http://www.domain.com", nsnull, "test=domain; domain=domain.com; max-age=-1", nsnull);
    GetACookie(cookieService, "http://domain.com", nsnull, getter_Copies(cookie));
    rv[4] = CheckResult(cookie, MUST_BE_NULL);

    SetACookie(cookieService, "http://www.domain.com", nsnull, "test=domain; domain=.domain.com", nsnull);
    GetACookie(cookieService, "http://domain.com", nsnull, getter_Copies(cookie));
    rv[5] = CheckResult(cookie, MUST_EQUAL, "test=domain");
    GetACookie(cookieService, "http://www.domain.com", nsnull, getter_Copies(cookie));
    rv[6] = CheckResult(cookie, MUST_EQUAL, "test=domain");
    GetACookie(cookieService, "http://bah.domain.com", nsnull, getter_Copies(cookie));
    rv[7] = CheckResult(cookie, MUST_EQUAL, "test=domain");
    SetACookie(cookieService, "http://www.domain.com", nsnull, "test=domain; domain=.domain.com; max-age=-1", nsnull);
    GetACookie(cookieService, "http://domain.com", nsnull, getter_Copies(cookie));
    rv[8] = CheckResult(cookie, MUST_BE_NULL);

    SetACookie(cookieService, "http://www.domain.com", nsnull, "test=domain; domain=.foo.domain.com", nsnull);
    GetACookie(cookieService, "http://foo.domain.com", nsnull, getter_Copies(cookie));
    rv[9] = CheckResult(cookie, MUST_BE_NULL);

    SetACookie(cookieService, "http://www.domain.com", nsnull, "test=domain; domain=moose.com", nsnull);
    GetACookie(cookieService, "http://foo.domain.com", nsnull, getter_Copies(cookie));
    rv[10] = CheckResult(cookie, MUST_BE_NULL);

    allTestsPassed = PrintResult(rv, 11) && allTestsPassed;


    // *** path tests
    printf("*** Beginning path tests...\n");

    // test some variations of the domain & path, for different paths of
    // a path cookie
    SetACookie(cookieService, "http://path.net/path/file", nsnull, "test=path; path=/path", nsnull);
    GetACookie(cookieService, "http://path.net/path", nsnull, getter_Copies(cookie));
    rv[0] = CheckResult(cookie, MUST_EQUAL, "test=path");
    GetACookie(cookieService, "http://path.net/path/", nsnull, getter_Copies(cookie));
    rv[1] = CheckResult(cookie, MUST_EQUAL, "test=path");
    GetACookie(cookieService, "http://path.net/path/hithere.foo", nsnull, getter_Copies(cookie));
    rv[2] = CheckResult(cookie, MUST_EQUAL, "test=path");
    GetACookie(cookieService, "http://path.net/path?hithere/foo", nsnull, getter_Copies(cookie));
    rv[3] = CheckResult(cookie, MUST_EQUAL, "test=path");
    GetACookie(cookieService, "http://path.net/path2", nsnull, getter_Copies(cookie));
    rv[4] = CheckResult(cookie, MUST_BE_NULL);
    GetACookie(cookieService, "http://path.net/path2/", nsnull, getter_Copies(cookie));
    rv[5] = CheckResult(cookie, MUST_BE_NULL);
    SetACookie(cookieService, "http://path.net/path/file", nsnull, "test=path; path=/path; max-age=-1", nsnull);
    GetACookie(cookieService, "http://path.net/path/", nsnull, getter_Copies(cookie));
    rv[6] = CheckResult(cookie, MUST_BE_NULL);

    SetACookie(cookieService, "http://path.net/path/file", nsnull, "test=path; path=/path/", nsnull);
    GetACookie(cookieService, "http://path.net/path", nsnull, getter_Copies(cookie));
    rv[7] = CheckResult(cookie, MUST_EQUAL, "test=path");
    GetACookie(cookieService, "http://path.net/path/", nsnull, getter_Copies(cookie));
    rv[8] = CheckResult(cookie, MUST_EQUAL, "test=path");
    SetACookie(cookieService, "http://path.net/path/file", nsnull, "test=path; path=/path/; max-age=-1", nsnull);
    GetACookie(cookieService, "http://path.net/path/", nsnull, getter_Copies(cookie));
    rv[9] = CheckResult(cookie, MUST_BE_NULL);

    // note that a site can set a cookie for a path it's not on.
    // this is an intentional deviation from spec (see comments in
    // nsCookieService::CheckPath()), so we test this functionality too
    SetACookie(cookieService, "http://path.net/path/file", nsnull, "test=path; path=/foo/", nsnull);
    GetACookie(cookieService, "http://path.net/path", nsnull, getter_Copies(cookie));
    rv[10] = CheckResult(cookie, MUST_BE_NULL);
    GetACookie(cookieService, "http://path.net/foo", nsnull, getter_Copies(cookie));
    rv[11] = CheckResult(cookie, MUST_EQUAL, "test=path");
    SetACookie(cookieService, "http://path.net/path/file", nsnull, "test=path; path=/foo/; max-age=-1", nsnull);
    GetACookie(cookieService, "http://path.net/foo/", nsnull, getter_Copies(cookie));
    rv[12] = CheckResult(cookie, MUST_BE_NULL);

    allTestsPassed = PrintResult(rv, 13) && allTestsPassed;


    // *** expiry & deletion tests
    // XXX add server time str parsing tests here
    printf("*** Beginning expiry & deletion tests...\n");

    // test some variations of the expiry time,
    // and test deletion of previously set cookies
    SetACookie(cookieService, "http://expireme.org/", nsnull, "test=expiry; max-age=-1", nsnull);
    GetACookie(cookieService, "http://expireme.org/", nsnull, getter_Copies(cookie));
    rv[0] = CheckResult(cookie, MUST_BE_NULL);
    SetACookie(cookieService, "http://expireme.org/", nsnull, "test=expiry; max-age=0", nsnull);
    GetACookie(cookieService, "http://expireme.org/", nsnull, getter_Copies(cookie));
    rv[1] = CheckResult(cookie, MUST_BE_NULL);
    SetACookie(cookieService, "http://expireme.org/", nsnull, "test=expiry; expires=Thu, 10 Apr 1980 16:33:12 GMT", nsnull);
    GetACookie(cookieService, "http://expireme.org/", nsnull, getter_Copies(cookie));
    rv[2] = CheckResult(cookie, MUST_BE_NULL);

    SetACookie(cookieService, "http://expireme.org/", nsnull, "test=expiry; max-age=60", nsnull);
    GetACookie(cookieService, "http://expireme.org/", nsnull, getter_Copies(cookie));
    rv[3] = CheckResult(cookie, MUST_EQUAL, "test=expiry");
    SetACookie(cookieService, "http://expireme.org/", nsnull, "test=expiry; max-age=-20", nsnull);
    GetACookie(cookieService, "http://expireme.org/", nsnull, getter_Copies(cookie));
    rv[4] = CheckResult(cookie, MUST_BE_NULL);
    SetACookie(cookieService, "http://expireme.org/", nsnull, "test=expiry; max-age=60", nsnull);
    GetACookie(cookieService, "http://expireme.org/", nsnull, getter_Copies(cookie));
    rv[5] = CheckResult(cookie, MUST_EQUAL, "test=expiry");
    SetACookie(cookieService, "http://expireme.org/", nsnull, "test=expiry; expires=Thu, 10 Apr 1980 16:33:12 GMT", nsnull);
    GetACookie(cookieService, "http://expireme.org/", nsnull, getter_Copies(cookie));
    rv[6] = CheckResult(cookie, MUST_BE_NULL);
    SetACookie(cookieService, "http://expireme.org/", nsnull, "test=expiry; max-age=60", nsnull);
    SetACookie(cookieService, "http://expireme.org/", nsnull, "newtest=expiry; max-age=60", nsnull);
    GetACookie(cookieService, "http://expireme.org/", nsnull, getter_Copies(cookie));
    rv[7] = CheckResult(cookie, MUST_CONTAIN, "test=expiry");
    rv[8] = CheckResult(cookie, MUST_CONTAIN, "newtest=expiry");
    SetACookie(cookieService, "http://expireme.org/", nsnull, "test=differentvalue; max-age=0", nsnull);
    GetACookie(cookieService, "http://expireme.org/", nsnull, getter_Copies(cookie));
    rv[9] = CheckResult(cookie, MUST_EQUAL, "newtest=expiry");
    SetACookie(cookieService, "http://expireme.org/", nsnull, "newtest=evendifferentvalue; max-age=0", nsnull);
    GetACookie(cookieService, "http://expireme.org/", nsnull, getter_Copies(cookie));
    rv[10] = CheckResult(cookie, MUST_BE_NULL);

    SetACookie(cookieService, "http://foo.expireme.org/", nsnull, "test=expiry; domain=.expireme.org; max-age=60", nsnull);
    GetACookie(cookieService, "http://expireme.org/", nsnull, getter_Copies(cookie));
    rv[11] = CheckResult(cookie, MUST_EQUAL, "test=expiry");
    SetACookie(cookieService, "http://bar.expireme.org/", nsnull, "test=differentvalue; domain=.expireme.org; max-age=0", nsnull);
    GetACookie(cookieService, "http://expireme.org/", nsnull, getter_Copies(cookie));
    rv[12] = CheckResult(cookie, MUST_BE_NULL);

    allTestsPassed = PrintResult(rv, 13) && allTestsPassed;


    // *** multiple cookie tests
    printf("*** Beginning multiple cookie tests...\n");

    // test the setting of multiple cookies, and test the order of precedence
    // (a later cookie overwriting an earlier one, in the same header string)
    SetACookie(cookieService, "http://multiple.cookies/", nsnull, "test=multiple; domain=.multiple.cookies \n test=different \n test=same; domain=.multiple.cookies \n newtest=ciao \n newtest=foo; max-age=-6 \n newtest=reincarnated", nsnull);
    GetACookie(cookieService, "http://multiple.cookies/", nsnull, getter_Copies(cookie));
    rv[0] = CheckResult(cookie, MUST_NOT_CONTAIN, "test=multiple");
    rv[1] = CheckResult(cookie, MUST_CONTAIN, "test=different");
    rv[2] = CheckResult(cookie, MUST_CONTAIN, "test=same");
    rv[3] = CheckResult(cookie, MUST_NOT_CONTAIN, "newtest=ciao");
    rv[4] = CheckResult(cookie, MUST_NOT_CONTAIN, "newtest=foo");
    rv[5] = CheckResult(cookie, MUST_CONTAIN, "newtest=reincarnated") != nsnull;
    SetACookie(cookieService, "http://multiple.cookies/", nsnull, "test=expiry; domain=.multiple.cookies; max-age=0", nsnull);
    GetACookie(cookieService, "http://multiple.cookies/", nsnull, getter_Copies(cookie));
    rv[6] = CheckResult(cookie, MUST_NOT_CONTAIN, "test=same");
    SetACookie(cookieService, "http://multiple.cookies/", nsnull,  "\n test=different; max-age=0 \n", nsnull);
    GetACookie(cookieService, "http://multiple.cookies/", nsnull, getter_Copies(cookie));
    rv[7] = CheckResult(cookie, MUST_NOT_CONTAIN, "test=different");
    SetACookie(cookieService, "http://multiple.cookies/", nsnull,  "newtest=dead; max-age=0", nsnull);
    GetACookie(cookieService, "http://multiple.cookies/", nsnull, getter_Copies(cookie));
    rv[8] = CheckResult(cookie, MUST_BE_NULL);

    allTestsPassed = PrintResult(rv, 9) && allTestsPassed;


    // *** foreign cookie tests
    printf("*** Beginning foreign cookie tests...\n");

    // test the blocking of foreign cookies, under various circumstances.
    // order of URI arguments is hostURI, firstURI
    SetACookie(cookieService, "http://yahoo.com/", "http://yahoo.com/", "test=foreign; domain=.yahoo.com", nsnull);
    GetACookie(cookieService, "http://yahoo.com/", "http://yahoo.com/", getter_Copies(cookie));
    rv[0] = CheckResult(cookie, MUST_EQUAL, "test=foreign");
    SetACookie(cookieService, "http://weather.yahoo.com/", "http://yahoo.com/", "test=foreign; domain=.yahoo.com", nsnull);
    GetACookie(cookieService, "http://notweather.yahoo.com/", "http://sport.yahoo.com/", getter_Copies(cookie));
    rv[1] = CheckResult(cookie, MUST_EQUAL, "test=foreign");
    SetACookie(cookieService, "http://moose.yahoo.com/", "http://canada.yahoo.com/", "test=foreign; domain=.yahoo.com", nsnull);
    GetACookie(cookieService, "http://yahoo.com/", "http://sport.yahoo.com/", getter_Copies(cookie));
    rv[2] = CheckResult(cookie, MUST_BE_NULL);
    GetACookie(cookieService, "http://sport.yahoo.com/", "http://yahoo.com/", getter_Copies(cookie));
    rv[3] = CheckResult(cookie, MUST_EQUAL, "test=foreign");
    SetACookie(cookieService, "http://jack.yahoo.com/", "http://jill.yahoo.com/", "test=foreign; domain=.yahoo.com; max-age=0", nsnull);
    GetACookie(cookieService, "http://jane.yahoo.com/", "http://yahoo.com/", getter_Copies(cookie));
    rv[4] = CheckResult(cookie, MUST_BE_NULL);

    SetACookie(cookieService, "http://moose.yahoo.com/", "http://foo.moose.yahoo.com/", "test=foreign; domain=.yahoo.com", nsnull);
    GetACookie(cookieService, "http://yahoo.com/", "http://yahoo.com/", getter_Copies(cookie));
    rv[5] = CheckResult(cookie, MUST_BE_NULL);
    SetACookie(cookieService, "http://foo.bar.yahoo.com/", "http://yahoo.com/", "test=foreign; domain=.yahoo.com", nsnull);
    GetACookie(cookieService, "http://yahoo.com/", "http://yahoo.com/", getter_Copies(cookie));
    rv[6] = CheckResult(cookie, MUST_EQUAL, "test=foreign");
    SetACookie(cookieService, "http://foo.bar.yahoo.com/", "http://yahoo.com/", "test=foreign; domain=.yahoo.com; max-age=0", nsnull);
    GetACookie(cookieService, "http://yahoo.com/", "http://yahoo.com/", getter_Copies(cookie));
    rv[7] = CheckResult(cookie, MUST_BE_NULL);

    // test handling of IP addresses by the foreign blocking algo
    SetACookie(cookieService, "http://192.168.54.33/", "http://192.168.54.33/", "test=foreign; domain=192.168.54.33", nsnull);
    GetACookie(cookieService, "http://192.168.54.33/", "http://192.168.54.33/", getter_Copies(cookie));
    rv[8] = CheckResult(cookie, MUST_EQUAL, "test=foreign");
    GetACookie(cookieService, "http://192.168.54.33./", "http://.192.168.54.33../", getter_Copies(cookie));
    rv[9] = CheckResult(cookie, MUST_EQUAL, "test=foreign");
    GetACookie(cookieService, "http://192.168.54.33/", nsnull, getter_Copies(cookie));
    rv[10] = CheckResult(cookie, MUST_EQUAL, "test=foreign");
    GetACookie(cookieService, "http://192.168.54.33/", "http://148.168.54.33", getter_Copies(cookie));
    rv[11] = CheckResult(cookie, MUST_BE_NULL);
    SetACookie(cookieService, "http://192.168.54.33/", "http://192.168.54.33/", "test=foreign; domain=192.168.54.33; max-age=0", nsnull);
    GetACookie(cookieService, "http://192.168.54.33/", "http://192.168.54.33/", getter_Copies(cookie));
    rv[12] = CheckResult(cookie, MUST_BE_NULL);
    SetACookie(cookieService, "http://192.168.54.33/", "http://148.168.54.33/", "test=foreign; domain=192.168.54.33", nsnull);
    GetACookie(cookieService, "http://192.168.54.33/", "http://192.168.54.33/", getter_Copies(cookie));
    rv[13] = CheckResult(cookie, MUST_BE_NULL);

    allTestsPassed = PrintResult(rv, 14) && allTestsPassed;


    // *** parser tests
    printf("*** Beginning parser tests...\n");

    // test the cookie header parser, under various circumstances.
    SetACookie(cookieService, "http://parser.test/", nsnull, "test=parser; domain=.parser.test; ;; ;=; ,,, ===,abc,=; abracadabra! max-age=20;=;;", nsnull);
    GetACookie(cookieService, "http://parser.test/", nsnull, getter_Copies(cookie));
    rv[0] = CheckResult(cookie, MUST_EQUAL, "test=parser");
    SetACookie(cookieService, "http://parser.test/", nsnull, "test=parser; domain=.parser.test; max-age=0", nsnull);
    GetACookie(cookieService, "http://parser.test/", nsnull, getter_Copies(cookie));
    rv[1] = CheckResult(cookie, MUST_BE_NULL);
    SetACookie(cookieService, "http://parser.test/", nsnull, "test=\"fubar! = foo;bar\\\";\" parser; domain=.parser.test; max-age=6\nfive; max-age=2.63,", nsnull);
    GetACookie(cookieService, "http://parser.test/", nsnull, getter_Copies(cookie));
    rv[2] = CheckResult(cookie, MUST_CONTAIN, "test=\"fubar! = foo;bar\\\";\"");
    rv[3] = CheckResult(cookie, MUST_CONTAIN, "five");
    SetACookie(cookieService, "http://parser.test/", nsnull, "test=kill; domain=.parser.test; max-age=0 \n five; max-age=0", nsnull);
    GetACookie(cookieService, "http://parser.test/", nsnull, getter_Copies(cookie));
    rv[4] = CheckResult(cookie, MUST_BE_NULL);

    // test the handling of VALUE-only cookies (see bug 169091),
    // i.e. "six" should assume an empty NAME, which allows other VALUE-only
    // cookies to overwrite it
    SetACookie(cookieService, "http://parser.test/", nsnull, "six", nsnull);
    GetACookie(cookieService, "http://parser.test/", nsnull, getter_Copies(cookie));
    rv[5] = CheckResult(cookie, MUST_EQUAL, "six");
    SetACookie(cookieService, "http://parser.test/", nsnull, "seven", nsnull);
    GetACookie(cookieService, "http://parser.test/", nsnull, getter_Copies(cookie));
    rv[6] = CheckResult(cookie, MUST_EQUAL, "seven");
    SetACookie(cookieService, "http://parser.test/", nsnull, " =eight", nsnull);
    GetACookie(cookieService, "http://parser.test/", nsnull, getter_Copies(cookie));
    rv[7] = CheckResult(cookie, MUST_EQUAL, "eight");
    SetACookie(cookieService, "http://parser.test/", nsnull, "test=six", nsnull);
    GetACookie(cookieService, "http://parser.test/", nsnull, getter_Copies(cookie));
    rv[9] = CheckResult(cookie, MUST_CONTAIN, "test=six");

    allTestsPassed = PrintResult(rv, 10) && allTestsPassed;


    // *** mailnews tests
    printf("*** Beginning mailnews tests...\n");

    // test some mailnews cookies to ensure blockage.
    // we use null firstURI's deliberately, since we have hacks to deal with
    // this situation...
    SetACookie(cookieService, "mailbox://mail.co.uk/", nsnull, "test=mailnews", nsnull);
    GetACookie(cookieService, "mailbox://mail.co.uk/", nsnull, getter_Copies(cookie));
    rv[0] = CheckResult(cookie, MUST_BE_NULL);
    GetACookie(cookieService, "http://mail.co.uk/", nsnull, getter_Copies(cookie));
    rv[1] = CheckResult(cookie, MUST_BE_NULL);
    SetACookie(cookieService, "http://mail.co.uk/", nsnull, "test=mailnews", nsnull);
    GetACookie(cookieService, "mailbox://mail.co.uk/", nsnull, getter_Copies(cookie));
    rv[2] = CheckResult(cookie, MUST_BE_NULL);
    GetACookie(cookieService, "http://mail.co.uk/", nsnull, getter_Copies(cookie));
    rv[3] = CheckResult(cookie, MUST_EQUAL, "test=mailnews");
    SetACookie(cookieService, "http://mail.co.uk/", nsnull, "test=mailnews; max-age=0", nsnull);
    GetACookie(cookieService, "http://mail.co.uk/", nsnull, getter_Copies(cookie));
    rv[4] = CheckResult(cookie, MUST_BE_NULL);

    // test non-null firstURI's, i) from mailnews ii) not from mailnews
    SetACookie(cookieService, "mailbox://mail.co.uk/", "http://mail.co.uk/", "test=mailnews", nsnull);
    GetACookie(cookieService, "http://mail.co.uk/", nsnull, getter_Copies(cookie));
    rv[5] = CheckResult(cookie, MUST_BE_NULL);
    SetACookie(cookieService, "http://mail.co.uk/", "mailbox://mail.co.uk/", "test=mailnews", nsnull);
    GetACookie(cookieService, "http://mail.co.uk/", nsnull, getter_Copies(cookie));
    rv[6] = CheckResult(cookie, MUST_BE_NULL);

    allTestsPassed = PrintResult(rv, 7) && allTestsPassed;


    // *** path ordering tests
    printf("*** Beginning path ordering tests...\n");

    // test that cookies are returned in path order - longest to shortest.
    // if the header doesn't specify a path, it's taken from the host URI.
    SetACookie(cookieService, "http://multi.path.tests/", nsnull, "test1=path; path=/one/two/three", nsnull);
    SetACookie(cookieService, "http://multi.path.tests/", nsnull, "test2=path; path=/one \n test3=path; path=/one/two/three/four \n test4=path; path=/one/two \n test5=path; path=/one/two/", nsnull);
    SetACookie(cookieService, "http://multi.path.tests/one/two/three/four/five/", nsnull, "test6=path", nsnull);
    SetACookie(cookieService, "http://multi.path.tests/one/two/three/four/five/six/", nsnull, "test7=path; path=", nsnull);
    SetACookie(cookieService, "http://multi.path.tests/", nsnull, "test8=path; path=/", nsnull);
    GetACookie(cookieService, "http://multi.path.tests/one/two/three/four/five/six/", nsnull, getter_Copies(cookie));
    rv[0] = CheckResult(cookie, MUST_EQUAL, "test7=path; test6=path; test3=path; test1=path; test5=path; test4=path; test2=path; test8=path");

    allTestsPassed = PrintResult(rv, 1) && allTestsPassed;

    // XXX the following are placeholders: add these tests please!
    // *** "noncompliant cookie" tests
    // *** IP address tests
    // *** speed tests


    printf("\n*** Result: %s!\n\n", allTestsPassed ? "all tests passed" : "TEST(S) FAILED");

    NS_ShutdownXPCOM(nsnull);
    
    return 0;
}
