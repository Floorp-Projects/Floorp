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
  triggerSetCookieFromHttp,
  triggerSetCookieFromHttpPrivate,
  preclean_test,
  cleanup_test,
} = ChromeUtils.import("resource://testing-common/cookie_filtering_helper.jsm");

// TEST: OriginAttributes
// * example.com OA-changed cookies don't go to example.com & vice-versa
async function test_origin_attributes() {
  var suite = oaSuite();

  // example.com - start content process when loading page
  let t2 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    browserTestPath(HTTPS_EXAMPLE_COM)
  );
  let testStruct2 = {
    name: "OA example.com",
    browser: gBrowser.getBrowserForTab(t2),
    tab: t2,
    expected: [suite[0], suite[4]],
  };

  // open a tab with altered OA: userContextId
  let t4 = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: (function() {
      return function() {
        // info("calling addTab from lambda");
        gBrowser.selectedTab = BrowserTestUtils.addTab(
          gBrowser,
          HTTPS_EXAMPLE_COM,
          { userContextId: 1 }
        );
      };
    })(),
  });
  let testStruct4 = {
    name: "OA example.com (contextId)",
    browser: gBrowser.getBrowserForTab(t4),
    tab: t4,
    expected: [suite[2], suite[5]],
  };

  // example.com
  await SpecialPowers.spawn(
    testStruct2.browser,
    [testStruct2.expected, testStruct2.name],
    checkExpectedCookies
  );
  // example.com with different OA: userContextId
  await SpecialPowers.spawn(
    testStruct4.browser,
    [testStruct4.expected, testStruct4.name],
    checkExpectedCookies
  );

  await triggerOriginAttributesEmulatedSuite();

  await SpecialPowers.spawn(testStruct2.browser, [], waitForAllExpectedTests);
  await SpecialPowers.spawn(testStruct4.browser, [], waitForAllExpectedTests);
  await SpecialPowers.spawn(testStruct2.browser, [], cleanupObservers);
  await SpecialPowers.spawn(testStruct4.browser, [], cleanupObservers);
  BrowserTestUtils.removeTab(testStruct2.tab);
  BrowserTestUtils.removeTab(testStruct4.tab);
}

// TEST: Private
// * example.com private cookies don't go to example.com process & vice-v
async function test_private() {
  var suite = oaSuite();

  // example.com
  let t2 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    browserTestPath(HTTPS_EXAMPLE_COM)
  );
  let testStruct2 = {
    name: "non-priv example.com",
    browser: gBrowser.getBrowserForTab(t2),
    tab: t2,
    expected: [suite[0], suite[4]],
  };

  // private window example.com
  let privateBrowserWindow = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });
  let privateTab = (privateBrowserWindow.gBrowser.selectedTab = BrowserTestUtils.addTab(
    privateBrowserWindow.gBrowser,
    browserTestPath(HTTPS_EXAMPLE_COM)
  ));
  let testStruct5 = {
    name: "private example.com",
    browser: privateBrowserWindow.gBrowser.getBrowserForTab(privateTab),
    tab: privateTab,
    expected: [suite[3], suite[6]],
  };
  await BrowserTestUtils.browserLoaded(testStruct5.tab.linkedBrowser);

  let parentpid = Services.appinfo.processID;
  let privatePid = testStruct5.browser.frameLoader.remoteTab.osPid;
  let pid = testStruct2.browser.frameLoader.remoteTab.osPid;
  ok(parentpid != privatePid, "Parent and private processes are unique");
  ok(parentpid != pid, "Parent and non-private processes are unique");
  ok(privatePid != pid, "Private and non-private processes are unique");

  // example.com
  await SpecialPowers.spawn(
    testStruct2.browser,
    [testStruct2.expected, testStruct2.name],
    checkExpectedCookies
  );

  // example.com private
  await SpecialPowers.spawn(
    testStruct5.browser,
    [testStruct5.expected, testStruct5.name],
    checkExpectedCookies
  );

  await triggerOriginAttributesEmulatedSuite();

  // wait for all tests and cleanup
  await SpecialPowers.spawn(testStruct2.browser, [], waitForAllExpectedTests);
  await SpecialPowers.spawn(testStruct5.browser, [], waitForAllExpectedTests);
  await SpecialPowers.spawn(testStruct2.browser, [], cleanupObservers);
  await SpecialPowers.spawn(testStruct5.browser, [], cleanupObservers);
  BrowserTestUtils.removeTab(testStruct2.tab);
  BrowserTestUtils.removeTab(testStruct5.tab);
  await BrowserTestUtils.closeWindow(privateBrowserWindow);
}

function oaSuite() {
  var suite = [];
  suite.push("test-cookie=orgwontsee"); // 0
  suite.push("test-cookie=firstparty"); // 1
  suite.push("test-cookie=usercontext"); // 2
  suite.push("test-cookie=privateonly"); // 3
  suite.push("test-cookie=sentinelcom"); // 4
  suite.push("test-cookie=sentineloa"); // 5
  suite.push("test-cookie=sentinelprivate"); // 6
  return suite;
}

// emulated because we are not making actual page requests
// we are just directly invoking the api
async function triggerOriginAttributesEmulatedSuite() {
  var suite = oaSuite();

  let uriCom = NetUtil.newURI(HTTPS_EXAMPLE_COM);
  triggerSetCookieFromHttp(uriCom, suite[0]);

  // FPD (OA) changed
  triggerSetCookieFromHttp(uriCom, suite[1], "foo.com");

  // context id (OA) changed
  triggerSetCookieFromHttp(uriCom, suite[2], "", 1);

  // private
  triggerSetCookieFromHttpPrivate(uriCom, suite[3]);

  // sentinels
  triggerSetCookieFromHttp(uriCom, suite[4]);
  triggerSetCookieFromHttp(uriCom, suite[5], "", 1);
  triggerSetCookieFromHttpPrivate(uriCom, suite[6]);
}

add_task(preclean_test);
add_task(test_origin_attributes); // 4
add_task(test_private); // 7
add_task(cleanup_test);
