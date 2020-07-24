/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const TRACKING_PAGE = "https://tracking.example.org";
const TRACKING_PAGE2 =
  "https://tracking.example.org^partitionKey=(https,example.com)";
const BENIGN_PAGE = "https://example.com";
const FOREIGN_PAGE = "https://example.net";
const FOREIGN_PAGE2 = "https://example.net^partitionKey=(https,example.com)";
const FOREIGN_PAGE3 = "https://example.net^partitionKey=(https,example.org)";

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

async function setupTest(aCookieBehavior) {
  Services.prefs.setIntPref("network.cookie.cookieBehavior", aCookieBehavior);
  Services.prefs.setBoolPref("privacy.purge_trackers.enabled", true);
  Services.prefs.setCharPref("privacy.purge_trackers.logging.level", "Debug");
  Services.prefs.setStringPref(
    "urlclassifier.trackingAnnotationTable.testEntries",
    "tracking.example.org"
  );

  // Enables us to test localStorage in xpcshell.
  Services.prefs.setBoolPref("dom.storage.client_validation", false);
}

/**
 * Test that cookies indexedDB and localStorage are purged if the cookie is found
 * on the tracking list and does not have an Interaction Permission.
 */
async function testIndexedDBAndLocalStorage() {
  await UrlClassifierTestUtils.addTestTrackers();

  PermissionTestUtils.add(
    TRACKING_PAGE,
    "storageAccessAPI",
    Services.perms.ALLOW_ACTION
  );

  SiteDataTestUtils.addToCookies(BENIGN_PAGE);
  for (let url of [
    TRACKING_PAGE,
    TRACKING_PAGE2,
    FOREIGN_PAGE,
    FOREIGN_PAGE2,
    FOREIGN_PAGE3,
  ]) {
    SiteDataTestUtils.addToLocalStorage(url);
    SiteDataTestUtils.addToCookies(url);
    await SiteDataTestUtils.addToIndexedDB(url);
  }

  // Purge while storage access permission exists.
  await PurgeTrackerService.purgeTrackingCookieJars();

  for (let url of [TRACKING_PAGE, TRACKING_PAGE2]) {
    ok(
      SiteDataTestUtils.hasCookies(url),
      "cookie remains while storage access permission exists."
    );
    ok(
      SiteDataTestUtils.hasLocalStorage(url),
      "localStorage should not have been removed while storage access permission exists."
    );
    Assert.greater(
      await SiteDataTestUtils.getQuotaUsage(url),
      0,
      `We have data for ${url}`
    );
  }

  // Run purge after storage access permission has been removed.
  PermissionTestUtils.remove(TRACKING_PAGE, "storageAccessAPI");
  await PurgeTrackerService.purgeTrackingCookieJars();

  ok(
    SiteDataTestUtils.hasCookies(BENIGN_PAGE),
    "A non-tracking page should retain cookies after purging"
  );

  for (let url of [FOREIGN_PAGE, FOREIGN_PAGE2, FOREIGN_PAGE3]) {
    ok(
      SiteDataTestUtils.hasCookies(url),
      `A non-tracking foreign page should retain cookies after purging`
    );
    ok(
      SiteDataTestUtils.hasLocalStorage(url),
      `localStorage for ${url} should not have been removed.`
    );
    Assert.greater(
      await SiteDataTestUtils.getQuotaUsage(url),
      0,
      `We have data for ${url}`
    );
  }

  // Cookie should have been removed.

  for (let url of [TRACKING_PAGE, TRACKING_PAGE2]) {
    ok(
      !SiteDataTestUtils.hasCookies(url),
      "cookie is removed after purge with no storage access permission."
    );
    ok(
      !SiteDataTestUtils.hasLocalStorage(url),
      "localStorage should have been removed"
    );
    Assert.equal(
      await SiteDataTestUtils.getQuotaUsage(url),
      0,
      "quota storage was deleted"
    );
  }

  UrlClassifierTestUtils.cleanupTestTrackers();
}

/**
 * Test that trackers are treated based on their base domain, not origin.
 */
async function testBaseDomain() {
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
}

/**
 * Test that trackers are not cleared if they are associated
 * with an entry on the entity list that has user interaction.
 */
async function testUserInteraction() {
  Services.prefs.setBoolPref(
    "privacy.purge_trackers.consider_entity_list",
    true
  );
  // The test URL for the entity list for annotation is
  // itisatrap.org/?resource=example.org, so we need to
  // add example.org as a tracker.
  Services.prefs.setCharPref(
    "urlclassifier.trackingAnnotationTable.testEntries",
    "example.org"
  );
  await UrlClassifierTestUtils.addTestTrackers();

  // These are hard coded test values on the entity list.
  const OWNER_PAGE = "https://itisatrap.org";
  const RESOURCE_PAGE = "https://example.org";

  PermissionTestUtils.add(
    OWNER_PAGE,
    "storageAccessAPI",
    Services.perms.ALLOW_ACTION
  );

  SiteDataTestUtils.addToCookies(RESOURCE_PAGE);

  // Add another tracker to verify we're actually purging.
  SiteDataTestUtils.addToCookies("https://another-tracking.example.net");

  await PurgeTrackerService.purgeTrackingCookieJars();

  ok(
    SiteDataTestUtils.hasCookies(RESOURCE_PAGE),
    `${RESOURCE_PAGE} should have retained its cookies when permission is set for ${OWNER_PAGE}.`
  );

  ok(
    !SiteDataTestUtils.hasCookies("https://another-tracking.example.net"),
    "cookie is removed after purge with no storage access permission."
  );

  Services.prefs.setBoolPref(
    "privacy.purge_trackers.consider_entity_list",
    false
  );

  await PurgeTrackerService.purgeTrackingCookieJars();

  ok(
    !SiteDataTestUtils.hasCookies(RESOURCE_PAGE),
    `${RESOURCE_PAGE} should not have retained its cookies when permission is set for ${OWNER_PAGE} and the entity list pref is off.`
  );

  PermissionTestUtils.remove(OWNER_PAGE, "storageAccessAPI");
  await SiteDataTestUtils.clear();

  Services.prefs.clearUserPref("privacy.purge_trackers.consider_entity_list");
  UrlClassifierTestUtils.cleanupTestTrackers();
}

/**
 * Test that quota storage (even without cookies) is considered when purging trackers.
 */
async function testQuotaStorage() {
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
      for (let url of [
        TRACKING_PAGE,
        TRACKING_PAGE2,
        BENIGN_PAGE,
        FOREIGN_PAGE,
        FOREIGN_PAGE2,
        FOREIGN_PAGE3,
      ]) {
        SiteDataTestUtils.addToLocalStorage(url);
      }
    }

    if (indexedDB) {
      for (let url of [
        TRACKING_PAGE,
        TRACKING_PAGE2,
        BENIGN_PAGE,
        FOREIGN_PAGE,
        FOREIGN_PAGE2,
        FOREIGN_PAGE3,
      ]) {
        await SiteDataTestUtils.addToIndexedDB(url);
      }
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
      for (let url of [
        TRACKING_PAGE,
        TRACKING_PAGE2,
        FOREIGN_PAGE,
        FOREIGN_PAGE2,
        FOREIGN_PAGE3,
      ]) {
        Assert.greater(
          await SiteDataTestUtils.getQuotaUsage(url),
          0,
          `We have data for ${url}`
        );
      }
    }

    // Run purge after storage access permission has been removed.
    PermissionTestUtils.remove(TRACKING_PAGE, "storageAccessAPI");
    await PurgeTrackerService.purgeTrackingCookieJars();

    if (localStorage) {
      for (let url of [
        BENIGN_PAGE,
        FOREIGN_PAGE,
        FOREIGN_PAGE2,
        FOREIGN_PAGE3,
      ]) {
        ok(
          SiteDataTestUtils.hasLocalStorage(url),
          "localStorage should not have been removed for non-tracking page."
        );
      }
      for (let url of [TRACKING_PAGE, TRACKING_PAGE2]) {
        ok(
          !SiteDataTestUtils.hasLocalStorage(url),
          "localStorage should have been removed."
        );
      }
    }

    if (indexedDB) {
      for (let url of [
        BENIGN_PAGE,
        FOREIGN_PAGE,
        FOREIGN_PAGE2,
        FOREIGN_PAGE3,
      ]) {
        Assert.greater(
          await SiteDataTestUtils.getQuotaUsage(url),
          0,
          "quota storage for non-tracking page was not deleted"
        );
      }

      for (let url of [TRACKING_PAGE, TRACKING_PAGE2]) {
        Assert.equal(
          await SiteDataTestUtils.getQuotaUsage(url),
          0,
          "quota storage was deleted"
        );
      }
    }

    await SiteDataTestUtils.clear();
  }

  UrlClassifierTestUtils.cleanupTestTrackers();
}

/**
 * Test that we correctly delete cookies and storage for sites
 * with an expired interaction permission.
 */
async function testExpiredInteractionPermission() {
  await UrlClassifierTestUtils.addTestTrackers();

  PermissionTestUtils.add(
    TRACKING_PAGE,
    "storageAccessAPI",
    Services.perms.ALLOW_ACTION,
    Services.perms.EXPIRE_TIME,
    Date.now() + 500
  );

  for (let url of [
    TRACKING_PAGE,
    TRACKING_PAGE2,
    FOREIGN_PAGE,
    FOREIGN_PAGE2,
    FOREIGN_PAGE3,
  ]) {
    SiteDataTestUtils.addToLocalStorage(url);
    SiteDataTestUtils.addToCookies(url);
    await SiteDataTestUtils.addToIndexedDB(url);
  }

  // Purge while storage access permission exists.
  await PurgeTrackerService.purgeTrackingCookieJars();

  for (let url of [
    TRACKING_PAGE,
    TRACKING_PAGE2,
    FOREIGN_PAGE,
    FOREIGN_PAGE2,
    FOREIGN_PAGE3,
  ]) {
    ok(
      SiteDataTestUtils.hasCookies(url),
      "cookie remains while storage access permission exists."
    );
    ok(
      SiteDataTestUtils.hasLocalStorage(url),
      "localStorage should not have been removed while storage access permission exists."
    );
    Assert.greater(
      await SiteDataTestUtils.getQuotaUsage(url),
      0,
      `We have data for ${url}`
    );
  }

  // Run purge after storage access permission has been removed.
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(c => setTimeout(c, 500));
  await PurgeTrackerService.purgeTrackingCookieJars();

  // Cookie should have been removed.
  for (let url of [TRACKING_PAGE, TRACKING_PAGE2]) {
    ok(
      !SiteDataTestUtils.hasCookies(url),
      "cookie is removed after purge with no storage access permission."
    );
    ok(
      !SiteDataTestUtils.hasLocalStorage(url),
      "localStorage should not have been removed while storage access permission exists."
    );
    Assert.equal(
      await SiteDataTestUtils.getQuotaUsage(url),
      0,
      "quota storage was deleted"
    );
  }

  // Cookie should not have been removed.
  for (let url of [FOREIGN_PAGE, FOREIGN_PAGE2, FOREIGN_PAGE3]) {
    ok(
      SiteDataTestUtils.hasCookies(url),
      "cookie remains while storage access permission exists."
    );
    ok(
      SiteDataTestUtils.hasLocalStorage(url),
      "localStorage should not have been removed while storage access permission exists."
    );
  }

  UrlClassifierTestUtils.cleanupTestTrackers();
}

add_task(async function() {
  const cookieBehaviors = [
    Ci.nsICookieService.BEHAVIOR_REJECT_FOREIGN,
    Ci.nsICookieService.BEHAVIOR_LIMIT_FOREIGN,
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER,
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
  ];

  for (let cookieBehavior of cookieBehaviors) {
    await setupTest(cookieBehavior);
    await testIndexedDBAndLocalStorage();
    await testBaseDomain();
    await testUserInteraction();
    await testQuotaStorage();
    await testExpiredInteractionPermission();
  }
});
