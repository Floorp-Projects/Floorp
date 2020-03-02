/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* import-globals-from antitracking_head.js */

XPCOMUtils.defineLazyServiceGetter(
  this,
  "PurgeTrackerService",
  "@mozilla.org/purge-tracker-service;1",
  "nsIPurgeTrackerService"
);

const TRACKING_PAGE = "https://tracking.example.org";
const HTTP_TRACKING_PAGE = "http://tracking.example.org";
const { SiteDataTestUtils } = ChromeUtils.import(
  "resource://testing-common/SiteDataTestUtils.jsm"
);

function checkDataForURI(uri) {
  return new Promise(resolve => {
    let data = true;
    uri = Services.io.newURI(uri);
    let principal = Services.scriptSecurityManager.createContentPrincipal(
      uri,
      {}
    );
    let request = indexedDB.openForPrincipal(principal, "TestDatabase", 1);
    request.onupgradeneeded = function(e) {
      data = false;
    };
    request.onsuccess = function(e) {
      resolve(data);
    };
  });
}

/** test the cookies indexDB and localStorage are purged if the cookie is found
 * on the tracking list and does not have an Interaction Permission.
 */
add_task(async function() {
  await SpecialPowers.flushPrefEnv();
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "network.cookie.cookieBehavior",
        Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER,
      ],
      ["privacy.purge_trackers.enabled", true],
      [
        "urlclassifier.trackingAnnotationTable.testEntries",
        "tracking.example.org",
      ],
    ],
  });

  await UrlClassifierTestUtils.addTestTrackers();
  // Open test page
  let tab = BrowserTestUtils.addTab(gBrowser, TEST_TOP_PAGE);
  gBrowser.selectedTab = tab;

  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  let uri = Services.io.newURI(TRACKING_PAGE);
  let principal = Services.scriptSecurityManager.createContentPrincipal(
    uri,
    {}
  );

  // Open a tracking page
  let trackingTab = BrowserTestUtils.addTab(gBrowser, TRACKING_PAGE);
  let trackingBrowser = gBrowser.getBrowserForTab(trackingTab);
  await BrowserTestUtils.browserLoaded(trackingBrowser);

  await SpecialPowers.spawn(trackingBrowser, [], async function() {
    // Simulate user interaction permission.
    content.document.userInteractionForTesting();
    // Add something to localstorage belonging to the tracking page.
    content.localStorage.setItem("test", "testValue");
    is(
      content.localStorage.getItem("test"),
      "testValue",
      "localStorage was correctly set."
    );
  });

  is(
    PermissionTestUtils.testPermission(uri, "storageAccessAPI"),
    Ci.nsIPermissionManager.ALLOW_ACTION,
    "We have a user Interaction Permission"
  );

  // Add a cookie belonging to a non-tracking page
  SiteDataTestUtils.addToCookies("https://example.com");

  // Add a cookie belonging to the tracking page.
  Services.cookies.add(
    uri.host,
    uri.pathQueryRef,
    "cookie-name",
    "data",
    false,
    false,
    false,
    Date.now() + 24000 * 60 * 60, // expires 1 day in the future
    {},
    Ci.nsICookie.SAMESITE_NONE
  );

  // Add something to indexDB
  await SiteDataTestUtils.addToIndexedDB(TRACKING_PAGE, "foo", "bar", {});
  ok(await checkDataForURI(TRACKING_PAGE), `We have data for ${TRACKING_PAGE}`);

  // purge while storage access permission exists.
  await PurgeTrackerService.purgeTrackingCookieJars();
  // cookie should not have been purged
  is(
    Services.cookies.getCookiesFromHost(uri.host, {}).length,
    1,
    "cookie remains while storage access permission exists."
  );
  // localStorage should not have been removed while storage access permission exists
  await SpecialPowers.spawn(trackingBrowser, [], async function() {
    is(
      content.localStorage.getItem("test"),
      "testValue",
      "localStorage should not have been removed while storage access permission exists."
    );
  });
  ok(
    await checkDataForURI(TRACKING_PAGE),
    `We have data for ${TRACKING_PAGE}, while storage access permission exists.`
  );

  // Remove storage access permission.
  Services.perms.removeFromPrincipal(principal, "storageAccessAPI");
  // Cookie should still exist.
  is(
    Services.cookies.getCookiesFromHost(uri.host, {}).length,
    1,
    "cookie should exist after storage access permission removed, before purge."
  );
  // localStorage should exist after storage access permission removed, before purge.
  await SpecialPowers.spawn(trackingBrowser, [], async function() {
    is(
      content.localStorage.getItem("test"),
      "testValue",
      "localStorage should exist after storage access permission removed, before purge."
    );
  });
  ok(
    await checkDataForURI(TRACKING_PAGE),
    `We have data for ${TRACKING_PAGE}, after storage access permission removed, before purge.`
  );

  // Run purge after storage access permission has been removed.
  await PurgeTrackerService.purgeTrackingCookieJars();
  // Cookie should have been removed.
  is(
    Services.cookies.getCookiesFromHost(uri.host, {}).length,
    0,
    "cookie should has removed after purge with no storage access permission."
  );
  // localStorage should have been removed after purge with no storage access permission.
  await SpecialPowers.spawn(trackingBrowser, [], async function() {
    is(
      content.localStorage.getItem("test"),
      null,
      "localStorage has been removed after purge with no storage access permission."
    );
  });
  ok(
    !(await checkDataForURI(TRACKING_PAGE)),
    `indexDB data for ${TRACKING_PAGE} has been removed after purge with no storage access permission.`
  );

  BrowserTestUtils.removeTab(tab);
  BrowserTestUtils.removeTab(trackingTab);
  UrlClassifierTestUtils.cleanupTestTrackers();
});

/** test that both http and https tracker data is removed if either are found
 * on the tracking list and does not have an Interaction Permission.
 */
add_task(async function() {
  await SpecialPowers.flushPrefEnv();
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "network.cookie.cookieBehavior",
        Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER,
      ],
      ["privacy.purge_trackers.enabled", true],
      [
        "urlclassifier.trackingAnnotationTable.testEntries",
        "tracking.example.org",
      ],
    ],
  });

  // Open a tracking page
  let trackingTab = BrowserTestUtils.addTab(gBrowser, TRACKING_PAGE);
  let trackingBrowser = gBrowser.getBrowserForTab(trackingTab);
  await BrowserTestUtils.browserLoaded(trackingBrowser);
  let httpsUri = Services.io.newURI(TRACKING_PAGE);
  let httpUri = Services.io.newURI(HTTP_TRACKING_PAGE);
  let principal = Services.scriptSecurityManager.createContentPrincipal(
    httpsUri,
    {}
  );

  await SpecialPowers.spawn(trackingBrowser, [], async function() {
    // Simulate user interaction permission on the http page.
    content.document.userInteractionForTesting();
  });

  // Add a cookie belonging to the http tracking page.
  Services.cookies.add(
    httpUri.host,
    httpUri.pathQueryRef,
    "cookie-name",
    "data",
    false,
    false,
    false,
    Date.now() + 24000 * 60 * 60, // expires 1 day in the future
    {},
    Ci.nsICookie.SAMESITE_NONE
  );
  // Add a cookie belonging to the https tracking page.
  Services.cookies.add(
    httpsUri.host,
    httpsUri.pathQueryRef,
    "cookie-name",
    "data",
    false,
    false,
    false,
    Date.now() + 24000 * 60 * 60, // expires 1 day in the future
    {},
    Ci.nsICookie.SAMESITE_NONE
  );

  // Remove storage access permission from http page.
  Services.perms.removeFromPrincipal(principal, "storageAccessAPI");
  await PurgeTrackerService.purgeTrackingCookieJars();

  // Cookie should have been removed from both http and https hosts.
  is(
    Services.cookies.getCookiesFromHost(httpsUri.host, {}).length,
    0,
    "cookie is removed after purge with no storage access permission."
  );
  is(
    Services.cookies.getCookiesFromHost(httpUri.host, {}).length,
    0,
    "cookie is removed after purge with no storage access permission."
  );

  BrowserTestUtils.removeTab(trackingTab);
  UrlClassifierTestUtils.cleanupTestTrackers();
});

add_task(async function() {
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, () =>
      resolve()
    );
  });
});
