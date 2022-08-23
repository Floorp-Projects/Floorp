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

// TEST: Cross Origin Resource (com)
// * process receives only COR cookies pertaining to same page
async function test_cross_origin_resource_com() {
  let comExpected = [];
  comExpected.push("test-cookie=comhtml"); // 1
  comExpected.push("test-cookie=png"); // 2
  comExpected.push("test-cookie=orghtml"); // 3
  // nothing for 4, 5, 6, 7 -> goes to .org process
  comExpected.push("test-cookie=png"); // 8

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: browserTestPath(HTTPS_EXAMPLE_COM),
    },
    await runSuiteWithContentListener(
      "COR example.com",
      triggerCrossOriginSuite,
      comExpected
    )
  );
  Services.cookies.removeAll();
}

// TEST: Cross Origin Resource (org)
// * received COR cookies only pertaining to the process's page
async function test_cross_origin_resource_org() {
  let orgExpected = [];
  // nothing for 1, 2 and 3 -> goes to .com
  orgExpected.push("test-cookie=png"); // 4
  orgExpected.push("test-cookie=orghtml"); // 5
  orgExpected.push("test-cookie=png"); // 6
  orgExpected.push("test-cookie=comhtml"); // 7
  // 8 nothing for 8 -> goes to .com process
  orgExpected.push("test-cookie=png"); // 9. Sentinel to verify no more came in

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: browserTestPath(HTTPS_EXAMPLE_ORG),
    },
    await runSuiteWithContentListener(
      "COR example.org",
      triggerCrossOriginSuite,
      orgExpected
    )
  );
}

// seems to block better than fetch for secondary resource
// using for more predicatable recving
async function requestBrowserPageWithFilename(
  testName,
  requestFrom,
  filename,
  param = ""
) {
  let url = requestFrom + "/browser/netwerk/test/browser/" + filename;

  // add param if necessary
  if (param != "") {
    url += "?" + param;
  }

  info("requesting " + url + " (" + testName + ")");
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url,
    },
    async () => {}
  );
}

async function triggerCrossOriginSuite() {
  // SameSite - 1 com page, 2 com png
  await requestBrowserPageWithFilename(
    "SameSite resource (com)",
    HTTPS_EXAMPLE_COM,
    "cookie_filtering_secure_resource_com.html"
  );

  // COR - 3 com page, 4 org png
  await requestBrowserPageWithFilename(
    "COR (com-org)",
    HTTPS_EXAMPLE_COM,
    "cookie_filtering_secure_resource_org.html"
  );

  // SameSite - 5 org page, 6 org png
  await requestBrowserPageWithFilename(
    "SameSite resource (org)",
    HTTPS_EXAMPLE_ORG,
    "cookie_filtering_secure_resource_org.html"
  );

  // COR - 7 org page, 8 com png
  await requestBrowserPageWithFilename(
    "SameSite resource (org-com)",
    HTTPS_EXAMPLE_ORG,
    "cookie_filtering_secure_resource_com.html"
  );

  // Sentinel to verify no more cookies come in after last true test
  await requestBrowserPageWithFilename(
    "COR sentinel",
    HTTPS_EXAMPLE_ORG,
    "cookie_filtering_square.png"
  );
}

add_task(preclean_test);
add_task(test_cross_origin_resource_com); // 4
add_task(test_cross_origin_resource_org); // 5
add_task(cleanup_test);
