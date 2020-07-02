/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const TRACKING_PAGE = "https://tracking.example.org";
const BENIGN_PAGE = "https://example.com";

const { UrlClassifierTestUtils } = ChromeUtils.import(
  "resource://testing-common/UrlClassifierTestUtils.jsm"
);
const { SiteDataTestUtils } = ChromeUtils.import(
  "resource://testing-common/SiteDataTestUtils.jsm"
);
const { PermissionTestUtils } = ChromeUtils.import(
  "resource://testing-common/PermissionTestUtils.jsm"
);
const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "PurgeTrackerService",
  "@mozilla.org/purge-tracker-service;1",
  "nsIPurgeTrackerService"
);

add_task(async function setup() {
  Services.prefs.setIntPref(
    "network.cookie.cookieBehavior",
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER
  );
  Services.prefs.setBoolPref("privacy.purge_trackers.enabled", true);
  Services.prefs.setStringPref(
    "urlclassifier.trackingAnnotationTable.testEntries",
    "tracking.example.org"
  );
  Services.prefs.setBoolPref(
    "toolkit.telemetry.testing.overrideProductsCheck",
    true
  );

  // Enables us to test localStorage in xpcshell.
  Services.prefs.setBoolPref("dom.storage.client_validation", false);
});

/**
 * Test telemetry for cookie purging.
 */
add_task(async function() {
  await UrlClassifierTestUtils.addTestTrackers();

  let FIVE_DAYS = 5 * 24 * 60 * 60 * 1000;

  PermissionTestUtils.add(
    TRACKING_PAGE,
    "storageAccessAPI",
    Services.perms.ALLOW_ACTION,
    Services.perms.EXPIRE_TIME,
    Date.now() + FIVE_DAYS
  );

  SiteDataTestUtils.addToLocalStorage(TRACKING_PAGE);
  SiteDataTestUtils.addToCookies(BENIGN_PAGE);
  SiteDataTestUtils.addToCookies(TRACKING_PAGE);
  await SiteDataTestUtils.addToIndexedDB(TRACKING_PAGE);

  let purgedHistogram = TelemetryTestUtils.getAndClearHistogram(
    "COOKIE_PURGING_ORIGINS_PURGED"
  );
  let notPurgedHistogram = TelemetryTestUtils.getAndClearHistogram(
    "COOKIE_PURGING_TRACKERS_WITH_USER_INTERACTION"
  );
  let remainingDaysHistogram = TelemetryTestUtils.getAndClearHistogram(
    "COOKIE_PURGING_TRACKERS_USER_INTERACTION_REMAINING_DAYS"
  );
  let intervalHistogram = TelemetryTestUtils.getAndClearHistogram(
    "COOKIE_PURGING_INTERVAL_HOURS"
  );

  // Purge while storage access permission exists.
  await PurgeTrackerService.purgeTrackingCookieJars();

  ok(
    SiteDataTestUtils.hasCookies(TRACKING_PAGE),
    "cookie remains while storage access permission exists."
  );
  ok(
    SiteDataTestUtils.hasLocalStorage(TRACKING_PAGE),
    "localStorage should not have been removed while storage access permission exists."
  );
  Assert.greater(
    await SiteDataTestUtils.getQuotaUsage(TRACKING_PAGE),
    0,
    `We have data for ${TRACKING_PAGE}`
  );

  TelemetryTestUtils.assertHistogram(purgedHistogram, 0, 1);
  TelemetryTestUtils.assertHistogram(notPurgedHistogram, 1, 1);
  TelemetryTestUtils.assertHistogram(remainingDaysHistogram, 4, 2);
  TelemetryTestUtils.assertHistogram(intervalHistogram, 0, 1);

  purgedHistogram = TelemetryTestUtils.getAndClearHistogram(
    "COOKIE_PURGING_ORIGINS_PURGED"
  );
  notPurgedHistogram = TelemetryTestUtils.getAndClearHistogram(
    "COOKIE_PURGING_TRACKERS_WITH_USER_INTERACTION"
  );
  intervalHistogram = TelemetryTestUtils.getAndClearHistogram(
    "COOKIE_PURGING_INTERVAL_HOURS"
  );

  // Run purge after storage access permission has been removed.
  PermissionTestUtils.remove(TRACKING_PAGE, "storageAccessAPI");
  await PurgeTrackerService.purgeTrackingCookieJars();

  ok(
    SiteDataTestUtils.hasCookies(BENIGN_PAGE),
    "A non-tracking page should retain cookies after purging"
  );

  // Cookie should have been removed.
  ok(
    !SiteDataTestUtils.hasCookies(TRACKING_PAGE),
    "cookie is removed after purge with no storage access permission."
  );
  ok(
    !SiteDataTestUtils.hasLocalStorage(TRACKING_PAGE),
    "localStorage should not have been removed while storage access permission exists."
  );
  Assert.equal(
    await SiteDataTestUtils.getQuotaUsage(TRACKING_PAGE),
    0,
    "quota storage was deleted"
  );

  TelemetryTestUtils.assertHistogram(purgedHistogram, 1, 1);
  Assert.equal(
    notPurgedHistogram.snapshot().sum,
    0,
    "no origins with user interaction"
  );
  TelemetryTestUtils.assertHistogram(intervalHistogram, 0, 1);

  UrlClassifierTestUtils.cleanupTestTrackers();
});

/**
 * Test counting correctly across cookies batches
 */
add_task(async function() {
  await UrlClassifierTestUtils.addTestTrackers();

  // Enforce deleting the same origin twice by adding two cookies and setting
  // the max number of cookies per batch to 1.
  SiteDataTestUtils.addToCookies(TRACKING_PAGE, "cookie1");
  SiteDataTestUtils.addToCookies(TRACKING_PAGE, "cookie2");
  Services.prefs.setIntPref("privacy.purge_trackers.max_purge_count", 1);

  let purgedHistogram = TelemetryTestUtils.getAndClearHistogram(
    "COOKIE_PURGING_ORIGINS_PURGED"
  );

  await PurgeTrackerService.purgeTrackingCookieJars();

  // Cookie should have been removed.
  await TestUtils.waitForCondition(
    () => !SiteDataTestUtils.hasCookies(TRACKING_PAGE),
    "cookie is removed after purge."
  );

  TelemetryTestUtils.assertHistogram(purgedHistogram, 1, 1);

  Services.prefs.clearUserPref("privacy.purge_trackers.max_purge_count");
  UrlClassifierTestUtils.cleanupTestTrackers();
});
