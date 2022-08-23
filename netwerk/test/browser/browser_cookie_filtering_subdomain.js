/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

const {
  HTTPS_EXAMPLE_ORG,
  HTTPS_EXAMPLE_COM,
  HTTP_EXAMPLE_COM,
  browserTestPath,
  waitForAllExpectedTests,
  cleanupObservers,
  checkExpectedCookies,
  fetchHelper,
  preclean_test,
  cleanup_test,
} = ChromeUtils.import("resource://testing-common/cookie_filtering_helper.jsm");

const HTTPS_SUBDOMAIN_1_EXAMPLE_COM = "https://test1.example.com";
const HTTP_SUBDOMAIN_1_EXAMPLE_COM = "http://test1.example.com";
const HTTPS_SUBDOMAIN_2_EXAMPLE_COM = "https://test2.example.com";
const HTTP_SUBDOMAIN_2_EXAMPLE_COM = "http://test2.example.com";

// run suite with content listener
// 1. initializes the content process and observer
// 2. runs the test gamut
// 3. cleans up the content process
async function runSuiteWithContentListener(name, triggerSuiteFunc, expected) {
  return async function(browser) {
    info("Running content suite: " + name);
    await SpecialPowers.spawn(browser, [expected, name], checkExpectedCookies);
    await triggerSuiteFunc();
    await SpecialPowers.spawn(browser, [], waitForAllExpectedTests);
    await SpecialPowers.spawn(browser, [], cleanupObservers);
    info("Complete content suite: " + name);
  };
}

// TEST: domain receives subdomain cookies
async function test_domain() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: browserTestPath(HTTPS_EXAMPLE_COM),
    },
    await runSuiteWithContentListener(
      "test_domain",
      triggerSuite,
      cookiesFromSuite()
    )
  );
}

// TEST: insecure domain receives base and sub-domain insecure cookies
async function test_insecure_domain() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: browserTestPath(HTTP_EXAMPLE_COM),
    },
    await runSuiteWithContentListener(
      "test_insecure_domain",
      triggerSuite,
      suiteMatchingDomain(HTTP_EXAMPLE_COM).concat(
        suiteMatchingDomain(HTTP_SUBDOMAIN_1_EXAMPLE_COM)
      )
    )
  );
}

// TEST: subdomain receives base domain and other sub-domain cookies
async function test_subdomain() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: browserTestPath(HTTPS_SUBDOMAIN_2_EXAMPLE_COM),
    },
    await runSuiteWithContentListener(
      "test_subdomain",
      triggerSuite,
      cookiesFromSuite()
    )
  );
}

// TEST: insecure subdomain receives base and sub-domain insecure cookies
async function test_insecure_subdomain() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: browserTestPath(HTTP_SUBDOMAIN_2_EXAMPLE_COM),
    },
    await runSuiteWithContentListener(
      "test_insecure_domain",
      triggerSuite,
      suiteMatchingDomain(HTTP_EXAMPLE_COM).concat(
        suiteMatchingDomain(HTTP_SUBDOMAIN_1_EXAMPLE_COM)
      )
    )
  );
}

function suite() {
  var suite = [];
  suite.push(["test-cookie=domain", HTTPS_EXAMPLE_COM]);
  suite.push(["test-cookie=subdomain", HTTPS_SUBDOMAIN_1_EXAMPLE_COM]);
  suite.push(["test-cookie-insecure=insecure_domain", HTTP_EXAMPLE_COM]);
  suite.push([
    "test-cookie-insecure=insecure_subdomain",
    HTTP_SUBDOMAIN_1_EXAMPLE_COM,
  ]);
  suite.push(["test-cookie=sentinel", HTTPS_EXAMPLE_COM]);
  return suite;
}

function cookiesFromSuite() {
  var cookies = [];
  for (var [cookie] of suite()) {
    cookies.push(cookie);
  }
  return cookies;
}

function suiteMatchingDomain(domain) {
  var s = suite();
  var result = [];
  for (var [cookie, dom] of s) {
    if (dom == domain) {
      result.push(cookie);
    }
  }
  return result;
}

function justSitename(maybeSchemefulMaybeSubdomainSite) {
  let mssArray = maybeSchemefulMaybeSubdomainSite.split("://");
  let maybesubdomain = mssArray[mssArray.length - 1];
  let msdArray = maybesubdomain.split(".");
  return msdArray.slice(msdArray.length - 2, msdArray.length).join(".");
}

// triggers set-cookie, which will trigger cookie-changed messages
// messages will be filtered against the cookie list created from above
// only unfiltered messages should make it to the content process
async function triggerSuite() {
  let triggerCookies = suite();
  for (var [cookie, schemefulDomain] of triggerCookies) {
    let secure = false;
    if (schemefulDomain.includes("https")) {
      secure = true;
    }

    var url =
      browserTestPath(schemefulDomain) + "cookie_filtering_resource.sjs";
    await fetchHelper(url, cookie, secure, justSitename(schemefulDomain));
    Services.cookies.removeAll(); // clean cookies across secure/insecure runs
  }
}

add_task(preclean_test);
add_task(test_domain); // 5
add_task(test_insecure_domain); // 2
add_task(test_subdomain); // 5
add_task(test_insecure_subdomain); // 2
add_task(cleanup_test);
