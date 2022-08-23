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

async function runSuiteWithContentListener(name, trigger_suite_func, expected) {
  return async function(browser) {
    info("Running content suite: " + name);
    await SpecialPowers.spawn(browser, [expected, name], checkExpectedCookies);
    await trigger_suite_func();
    await SpecialPowers.spawn(browser, [], waitForAllExpectedTests);
    await SpecialPowers.spawn(browser, [], cleanupObservers);
    info("Complete content suite: " + name);
  };
}

// TEST: In/Secure (insecure com)
// * secure example.com cookie do not go to insecure example.com process
// * insecure cookies go to insecure process
// * secure request with insecure cookie will go to insecure process
async function test_insecure_suite_insecure_com() {
  var expected = [];
  expected.push("test-cookie=png1");
  expected.push("test-cookie=png2");
  // insecure com will not recieve the secure com request with secure cookie
  expected.push("test-cookie=png3");
  info(expected);

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: browserTestPath(HTTP_EXAMPLE_COM),
    },
    await runSuiteWithContentListener(
      "insecure suite insecure com",
      triggerInsecureSuite,
      expected
    )
  );
}

// TEST: In/Secure (secure com)
// * secure page will recieve all secure/insecure cookies
async function test_insecure_suite_secure_com() {
  var expected = [];
  expected.push("test-cookie=png1");
  expected.push("test-cookie=png2");
  expected.push("test-cookie=secure-png");
  expected.push("test-cookie=png3");
  info(expected);
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: browserTestPath(HTTPS_EXAMPLE_COM),
    },
    await runSuiteWithContentListener(
      "insecure suite secure com",
      triggerInsecureSuite,
      expected
    )
  );
}

async function triggerInsecureSuite() {
  const cookieSjsFilename = "cookie_filtering_resource.sjs";

  // insecure page, insecure cookie
  var url = browserTestPath(HTTP_EXAMPLE_COM) + cookieSjsFilename;
  await fetchHelper(url, "test-cookie=png1", false);

  // secure page req, insecure cookie
  url = browserTestPath(HTTPS_EXAMPLE_COM) + cookieSjsFilename;
  await fetchHelper(url, "test-cookie=png2", false);

  // secure page, secure cookie
  url = browserTestPath(HTTPS_EXAMPLE_COM) + cookieSjsFilename;
  await fetchHelper(url, "test-cookie=secure-png", true);

  // not testing insecure server returning secure cookie --

  // sentinel
  url = browserTestPath(HTTPS_EXAMPLE_COM) + cookieSjsFilename;
  await fetchHelper(url, "test-cookie=png3", false);
}

add_task(preclean_test);
add_task(test_insecure_suite_insecure_com); // 3
add_task(test_insecure_suite_secure_com); // 4
add_task(cleanup_test);
