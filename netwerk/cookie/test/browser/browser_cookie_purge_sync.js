/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test checks that cookie purge broadcast correctly
// updates content process memory as triggered by:
// 1. stale-cookie update from content process
// 2. delete cookie by host from parent process
// 3. clear all cookies from parent process

const URL_EXAMPLE = "https://example.com";
const COOKIE_NAMEVALUE = "name=value";
const COOKIE_NAMEVALUE_2 = COOKIE_NAMEVALUE + "2";
const COOKIE_STRING = COOKIE_NAMEVALUE + "; Secure; SameSite=None";
const COOKIE_STRING_2 = COOKIE_NAMEVALUE_2 + "; Secure; SameSite=None";
const MAX_AGE_OLD = 2; // seconds
const MAX_AGE_NEW = 100; // 100 sec

registerCleanupFunction(() => {
  info("Cleaning up the test setup");
  Services.prefs.clearUserPref("network.cookie.sameSite.laxByDefault");
  Services.cookies.removeAll();
});

add_setup(async function () {
  info("Setting up the test");
  Services.prefs.setBoolPref("network.cookie.sameSite.laxByDefault", false);
});

function waitForNotificationPromise(notification, expected) {
  return new Promise(resolve => {
    function observer(subject, topic, data) {
      is(content.document.cookie, expected);
      Services.obs.removeObserver(observer, notification);
      resolve();
    }
    Services.obs.addObserver(observer, notification);
  });
}

add_task(async function test_purge_sync_batch_and_deleted() {
  const tab1 = BrowserTestUtils.addTab(gBrowser, URL_EXAMPLE);
  const browser = gBrowser.getBrowserForTab(tab1);
  await BrowserTestUtils.browserLoaded(browser);

  const tab2 = BrowserTestUtils.addTab(gBrowser, URL_EXAMPLE);
  const browser2 = gBrowser.getBrowserForTab(tab2);
  await BrowserTestUtils.browserLoaded(browser2);

  let firstCookieAdded = SpecialPowers.spawn(
    browser2,
    ["content-added-cookie", COOKIE_NAMEVALUE],
    waitForNotificationPromise
  );
  await TestUtils.waitForTick(); // waiting helps --verify

  // set old cookie in tab 1 and check it in tab 2
  await SpecialPowers.spawn(
    browser,
    [COOKIE_STRING, MAX_AGE_OLD],
    (cookie, max_age) => {
      content.document.cookie = cookie + ";Max-Age=" + max_age;
    }
  );
  await firstCookieAdded;

  // wait until the first cookie expires
  await SpecialPowers.spawn(browser2, [], () => {
    return ContentTaskUtils.waitForCondition(
      () => content.document.cookie == "",
      "cookie did not expire in time",
      200
    ).catch(msg => {
      is(false, "Cookie did not expire in time");
    });
  });

  // BATCH_DELETED/BatchDeleted pathway
  let batchDeletedPromise = SpecialPowers.spawn(
    browser,
    ["content-batch-deleted-cookies", COOKIE_NAMEVALUE_2],
    waitForNotificationPromise
  );
  await TestUtils.waitForTick(); // waiting helps --verify
  await SpecialPowers.spawn(
    browser,
    [COOKIE_STRING_2, MAX_AGE_NEW],
    (cookie, max_age) => {
      content.document.cookie = cookie + ";Max-Age=" + max_age;
    }
  );
  await batchDeletedPromise;

  // COOKIE_DELETED/RemoveCookie pathway
  let cookieRemovedPromise = SpecialPowers.spawn(
    browser,
    ["content-removed-cookie", ""],
    waitForNotificationPromise
  );
  let cookieRemovedPromise2 = SpecialPowers.spawn(
    browser2,
    ["content-removed-cookie", ""],
    waitForNotificationPromise
  );
  await TestUtils.waitForTick();
  Services.cookies.removeCookiesFromExactHost(
    "example.com",
    JSON.stringify({})
  );
  await cookieRemovedPromise;
  await cookieRemovedPromise2;

  // cleanup prep
  let anotherCookieAdded = SpecialPowers.spawn(
    browser2,
    ["content-added-cookie", COOKIE_NAMEVALUE],
    waitForNotificationPromise
  );
  await TestUtils.waitForTick(); // waiting helps --verify
  await SpecialPowers.spawn(
    browser,
    [COOKIE_STRING, MAX_AGE_NEW],
    (cookie, max_age) => {
      content.document.cookie = cookie + ";Max-Age=" + max_age;
    }
  );
  await anotherCookieAdded;

  // ALL_COOKIES_CLEARED/RemoveAll pathway
  let cleanup = SpecialPowers.spawn(
    browser2,
    ["content-removed-all-cookies", ""],
    waitForNotificationPromise
  );
  await TestUtils.waitForTick();
  Services.cookies.removeAll();
  await cleanup;

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
});
