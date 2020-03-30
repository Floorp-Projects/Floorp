/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const TRACKING_PAGE = "https://tracking.example.org";
const BENIGN_PAGE = "https://example.com";
const HTTP_TRACKING_PAGE = "http://tracking.example.org";

const { UrlClassifierTestUtils } = ChromeUtils.import(
  "resource://testing-common/UrlClassifierTestUtils.jsm"
);
const { SiteDataTestUtils } = ChromeUtils.import(
  "resource://testing-common/SiteDataTestUtils.jsm"
);
const { PermissionTestUtils } = ChromeUtils.import(
  "resource://testing-common/PermissionTestUtils.jsm"
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

  // Enables us to test localStorage in xpcshell.
  Services.prefs.setBoolPref("dom.storage.client_validation", false);
});

/**
 * Test that cookies indexedDB and localStorage are purged if the cookie is found
 * on the tracking list and does not have an Interaction Permission.
 */
add_task(async function() {
  await UrlClassifierTestUtils.addTestTrackers();

  PermissionTestUtils.add(
    TRACKING_PAGE,
    "storageAccessAPI",
    Services.perms.ALLOW_ACTION
  );

  SiteDataTestUtils.addToLocalStorage(TRACKING_PAGE);
  SiteDataTestUtils.addToCookies(BENIGN_PAGE);
  SiteDataTestUtils.addToCookies(TRACKING_PAGE);
  await SiteDataTestUtils.addToIndexedDB(TRACKING_PAGE);

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

  UrlClassifierTestUtils.cleanupTestTrackers();
});
