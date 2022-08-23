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

// TEST: Different domains (org)
// * example.org cookies go to example.org process
// * exmaple.com cookies do not go to example.org process
async function test_basic_suite_org() {
  // example.org - start content process when loading page
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: browserTestPath(HTTPS_EXAMPLE_ORG),
    },
    await runSuiteWithContentListener(
      "basic suite org",
      triggerBasicSuite,
      basicSuiteMatchingDomain(HTTPS_EXAMPLE_ORG)
    )
  );
}

// TEST: Different domains (com)
// * example.com cookies go to example.com process
// * example.org cookies do not go to example.com process
// * insecure example.com cookies go to secure com process
async function test_basic_suite_com() {
  // example.com - start content process when loading page
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: browserTestPath(HTTPS_EXAMPLE_COM),
    },
    await runSuiteWithContentListener(
      "basic suite com",
      triggerBasicSuite,
      basicSuiteMatchingDomain(HTTPS_EXAMPLE_COM).concat(
        basicSuiteMatchingDomain(HTTP_EXAMPLE_COM)
      )
    )
  );
}

// TEST: Duplicate domain (org)
// * example.org cookies go to multiple example.org processes
async function test_basic_suite_org_duplicate() {
  let expected = basicSuiteMatchingDomain(HTTPS_EXAMPLE_ORG);
  let t1 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    browserTestPath(HTTPS_EXAMPLE_ORG)
  );
  let testStruct1 = {
    name: "example.org primary",
    browser: gBrowser.getBrowserForTab(t1),
    tab: t1,
    expected,
  };

  // example.org dup
  let t3 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    browserTestPath(HTTPS_EXAMPLE_ORG)
  );
  let testStruct3 = {
    name: "example.org dup",
    browser: gBrowser.getBrowserForTab(t3),
    tab: t3,
    expected,
  };

  let parentpid = Services.appinfo.processID;
  let pid1 = testStruct1.browser.frameLoader.remoteTab.osPid;
  let pid3 = testStruct3.browser.frameLoader.remoteTab.osPid;
  ok(
    parentpid != pid1,
    "Parent pid should differ from content process for 1st example.org"
  );
  ok(
    parentpid != pid3,
    "Parent pid should differ from content process for 2nd example.org"
  );
  ok(pid1 != pid3, "Content pids should differ from each other");

  await SpecialPowers.spawn(
    testStruct1.browser,
    [testStruct1.expected, testStruct1.name],
    checkExpectedCookies
  );

  await SpecialPowers.spawn(
    testStruct3.browser,
    [testStruct3.expected, testStruct3.name],
    checkExpectedCookies
  );

  await triggerBasicSuite();

  // wait for all tests and cleanup
  await SpecialPowers.spawn(testStruct1.browser, [], waitForAllExpectedTests);
  await SpecialPowers.spawn(testStruct3.browser, [], waitForAllExpectedTests);
  await SpecialPowers.spawn(testStruct1.browser, [], cleanupObservers);
  await SpecialPowers.spawn(testStruct3.browser, [], cleanupObservers);
  BrowserTestUtils.removeTab(testStruct1.tab);
  BrowserTestUtils.removeTab(testStruct3.tab);
}

function basicSuite() {
  var suite = [];
  suite.push(["test-cookie=aaa", HTTPS_EXAMPLE_ORG]);
  suite.push(["test-cookie=bbb", HTTPS_EXAMPLE_ORG]);
  suite.push(["test-cookie=dad", HTTPS_EXAMPLE_ORG]);
  suite.push(["test-cookie=rad", HTTPS_EXAMPLE_ORG]);
  suite.push(["test-cookie=orgwontsee", HTTPS_EXAMPLE_COM]);
  suite.push(["test-cookie=sentinelorg", HTTPS_EXAMPLE_ORG]);
  suite.push(["test-cookie=sentinelcom", HTTPS_EXAMPLE_COM]);
  return suite;
}

function basicSuiteMatchingDomain(domain) {
  var suite = basicSuite();
  var result = [];
  for (var [cookie, dom] of suite) {
    if (dom == domain) {
      result.push(cookie);
    }
  }
  return result;
}

// triggers set-cookie, which will trigger cookie-changed messages
// messages will be filtered against the cookie list created from above
// only unfiltered messages should make it to the content process
async function triggerBasicSuite() {
  let triggerCookies = basicSuite();
  for (var [cookie, domain] of triggerCookies) {
    let secure = false;
    if (domain.includes("https")) {
      secure = true;
    }

    //trigger
    var url = browserTestPath(domain) + "cookie_filtering_resource.sjs";
    await fetchHelper(url, cookie, secure);
  }
}

add_task(preclean_test);
add_task(test_basic_suite_org); // 5
add_task(test_basic_suite_com); // 2
add_task(test_basic_suite_org_duplicate); // 13
add_task(cleanup_test);
