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
const { setTimeout } = ChromeUtils.import("resource://gre/modules/Timer.jsm");

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

/**
 * Test that trackers are treated based on their base domain, not origin.
 */
add_task(async function() {
  await UrlClassifierTestUtils.addTestTrackers();

  let associatedOrigins = [
    "https://itisatracker.org",
    "https://sub.itisatracker.org",
    "https://www.itisatracker.org",
    "https://sub.sub.sub.itisatracker.org",
    "http://itisatracker.org",
    "http://sub.itisatracker.org",
  ];

  for (let permissionOrigin of associatedOrigins) {
    // Only one of the associated origins gets permission, but
    // all should be exempt from purging.
    PermissionTestUtils.add(
      permissionOrigin,
      "storageAccessAPI",
      Services.perms.ALLOW_ACTION
    );

    for (let origin of associatedOrigins) {
      SiteDataTestUtils.addToCookies(origin);
    }

    // Add another tracker to verify we're actually purging.
    SiteDataTestUtils.addToCookies(TRACKING_PAGE);

    await PurgeTrackerService.purgeTrackingCookieJars();

    for (let origin of associatedOrigins) {
      ok(
        SiteDataTestUtils.hasCookies(origin),
        `${origin} should have retained its cookies when permission is set for ${permissionOrigin}.`
      );
    }

    ok(
      !SiteDataTestUtils.hasCookies(TRACKING_PAGE),
      "cookie is removed after purge with no storage access permission."
    );

    PermissionTestUtils.remove(permissionOrigin, "storageAccessAPI");
    await SiteDataTestUtils.clear();
  }

  UrlClassifierTestUtils.cleanupTestTrackers();
});

/**
 * Test that quota storage (even without cookies) is considered when purging trackers.
 */
add_task(async function() {
  await UrlClassifierTestUtils.addTestTrackers();

  let testCases = [
    { localStorage: true, indexedDB: true },
    { localStorage: false, indexedDB: true },
    { localStorage: true, indexedDB: false },
  ];

  for (let { localStorage, indexedDB } of testCases) {
    PermissionTestUtils.add(
      TRACKING_PAGE,
      "storageAccessAPI",
      Services.perms.ALLOW_ACTION
    );

    if (localStorage) {
      SiteDataTestUtils.addToLocalStorage(TRACKING_PAGE);
      SiteDataTestUtils.addToLocalStorage(BENIGN_PAGE);
    }

    if (indexedDB) {
      await SiteDataTestUtils.addToIndexedDB(TRACKING_PAGE);
      await SiteDataTestUtils.addToIndexedDB(BENIGN_PAGE);
    }

    // Purge while storage access permission exists.
    await PurgeTrackerService.purgeTrackingCookieJars();

    if (localStorage) {
      ok(
        SiteDataTestUtils.hasLocalStorage(TRACKING_PAGE),
        "localStorage should not have been removed while storage access permission exists."
      );
    }

    if (indexedDB) {
      Assert.greater(
        await SiteDataTestUtils.getQuotaUsage(TRACKING_PAGE),
        0,
        `We have data for ${TRACKING_PAGE}`
      );
    }

    // Run purge after storage access permission has been removed.
    PermissionTestUtils.remove(TRACKING_PAGE, "storageAccessAPI");
    await PurgeTrackerService.purgeTrackingCookieJars();

    if (localStorage) {
      ok(
        SiteDataTestUtils.hasLocalStorage(BENIGN_PAGE),
        "localStorage should not have been removed for benign page."
      );
      ok(
        !SiteDataTestUtils.hasLocalStorage(TRACKING_PAGE),
        "localStorage should have been removed."
      );
    }

    if (indexedDB) {
      Assert.greater(
        await SiteDataTestUtils.getQuotaUsage(BENIGN_PAGE),
        0,
        "quota storage for benign page was not deleted"
      );
      Assert.equal(
        await SiteDataTestUtils.getQuotaUsage(TRACKING_PAGE),
        0,
        "quota storage was deleted"
      );
    }

    await SiteDataTestUtils.clear();
  }

  UrlClassifierTestUtils.cleanupTestTrackers();
});

/**
 * Test that we correctly delete cookies and storage for sites
 * with an expired interaction permission.
 */
add_task(async function() {
  await UrlClassifierTestUtils.addTestTrackers();

  PermissionTestUtils.add(
    TRACKING_PAGE,
    "storageAccessAPI",
    Services.perms.ALLOW_ACTION,
    Services.perms.EXPIRE_TIME,
    Date.now() + 500
  );

  SiteDataTestUtils.addToLocalStorage(TRACKING_PAGE);
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
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(c => setTimeout(c, 500));
  await PurgeTrackerService.purgeTrackingCookieJars();

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
