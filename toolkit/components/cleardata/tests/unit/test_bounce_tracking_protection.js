/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "bounceTrackingProtection",
  "@mozilla.org/bounce-tracking-protection;1",
  "nsIBounceTrackingProtection"
);

const { CLEAR_BOUNCE_TRACKING_PROTECTION_STATE } = Ci.nsIClearDataService;

const OA_DEFAULT = {};
const OA_PRIVATE_BROWSING = { privateBrowsingId: 1 };
const OA_CONTAINER = { userContextId: 1 };

function addTestData() {
  info("Adding test data.");
  // Add test data for OriginAttributes {} => normal browsing with no container.
  bounceTrackingProtection.testAddBounceTrackerCandidate(
    OA_DEFAULT,
    "common-bounce-tracker.com",
    100
  );
  bounceTrackingProtection.testAddBounceTrackerCandidate(
    OA_DEFAULT,
    "bounce-tracker-normal-browsing.com",
    200
  );
  bounceTrackingProtection.testAddBounceTrackerCandidate(
    OA_DEFAULT,
    "bounce-tracker-normal-browsing2.com",
    300
  );
  bounceTrackingProtection.testAddUserActivation(
    OA_DEFAULT,
    "common-user-activation.com",
    400
  );
  bounceTrackingProtection.testAddUserActivation(
    OA_DEFAULT,
    "user-activation-normal-browsing.com",
    500
  );
  bounceTrackingProtection.testAddUserActivation(
    OA_DEFAULT,
    "user-activation-normal-browsing2.com",
    600
  );

  // Add test data for private browsing.
  bounceTrackingProtection.testAddBounceTrackerCandidate(
    OA_PRIVATE_BROWSING,
    "common-bounce-tracker.com",
    700
  );
  bounceTrackingProtection.testAddBounceTrackerCandidate(
    OA_PRIVATE_BROWSING,
    "bounce-tracker-private-browsing.com",
    800
  );
  bounceTrackingProtection.testAddBounceTrackerCandidate(
    OA_PRIVATE_BROWSING,
    "bounce-tracker-private-browsing2.com",
    900
  );
  bounceTrackingProtection.testAddUserActivation(
    OA_PRIVATE_BROWSING,
    "common-user-activation.com",
    1000
  );
  bounceTrackingProtection.testAddUserActivation(
    OA_PRIVATE_BROWSING,
    "user-activation-private-browsing.com",
    1100
  );
  bounceTrackingProtection.testAddUserActivation(
    OA_PRIVATE_BROWSING,
    "user-activation-private-browsing2.com",
    1200
  );

  // Add test data for a container.
  bounceTrackingProtection.testAddBounceTrackerCandidate(
    OA_CONTAINER,
    "common-bounce-tracker.com",
    1300
  );
  bounceTrackingProtection.testAddBounceTrackerCandidate(
    OA_CONTAINER,
    "bounce-tracker-container.com",
    1400
  );
  bounceTrackingProtection.testAddBounceTrackerCandidate(
    OA_CONTAINER,
    "bounce-tracker-container2.com",
    1500
  );
  bounceTrackingProtection.testAddUserActivation(
    OA_CONTAINER,
    "common-user-activation.com",
    1600
  );
  bounceTrackingProtection.testAddUserActivation(
    OA_CONTAINER,
    "user-activation-container.com",
    1700
  );
  bounceTrackingProtection.testAddUserActivation(
    OA_CONTAINER,
    "user-activation-container2.com",
    1800
  );
}

async function runDeleteBySiteHostTest(clearDataServiceFn) {
  addTestData();

  let baseDomain = "common-bounce-tracker.com";
  info("Deleting by base domain " + baseDomain);
  await new Promise(function (resolve) {
    clearDataServiceFn(
      baseDomain,
      true,
      CLEAR_BOUNCE_TRACKING_PROTECTION_STATE,
      failedFlags => {
        Assert.equal(failedFlags, 0, "Clearing should have succeeded");
        resolve();
      }
    );
  });

  Assert.deepEqual(
    bounceTrackingProtection
      .testGetBounceTrackerCandidateHosts(OA_DEFAULT)
      .map(entry => entry.siteHost)
      .sort(),
    [
      "bounce-tracker-normal-browsing.com",
      "bounce-tracker-normal-browsing2.com",
    ],
    "Should have deleted only 'common-bounce-tracker.com' for default OA."
  );
  Assert.deepEqual(
    bounceTrackingProtection
      .testGetUserActivationHosts(OA_DEFAULT)
      .map(entry => entry.siteHost)
      .sort(),
    [
      "common-user-activation.com",
      "user-activation-normal-browsing.com",
      "user-activation-normal-browsing2.com",
    ],
    "Should not have deleted any user activations for default OA."
  );

  Assert.deepEqual(
    bounceTrackingProtection
      .testGetBounceTrackerCandidateHosts(OA_PRIVATE_BROWSING)
      .map(entry => entry.siteHost)
      .sort(),
    [
      "bounce-tracker-private-browsing.com",
      "bounce-tracker-private-browsing2.com",
    ],
    "Should have deleted only 'common-bounce-tracker.com' for private browsing."
  );
  Assert.deepEqual(
    bounceTrackingProtection
      .testGetUserActivationHosts(OA_PRIVATE_BROWSING)
      .map(entry => entry.siteHost)
      .sort(),
    [
      "common-user-activation.com",
      "user-activation-private-browsing.com",
      "user-activation-private-browsing2.com",
    ],
    "Should not have deleted any user activations for private browsing."
  );

  Assert.deepEqual(
    bounceTrackingProtection
      .testGetBounceTrackerCandidateHosts(OA_CONTAINER)
      .map(entry => entry.siteHost)
      .sort(),
    ["bounce-tracker-container.com", "bounce-tracker-container2.com"],
    "Should have deleted only 'common-bounce-tracker.com' for container."
  );

  Assert.deepEqual(
    bounceTrackingProtection
      .testGetUserActivationHosts(OA_CONTAINER)
      .map(entry => entry.siteHost)
      .sort(),
    [
      "common-user-activation.com",
      "user-activation-container.com",
      "user-activation-container2.com",
    ],
    "Should not have deleted any user activations for container."
  );

  // Cleanup.
  bounceTrackingProtection.clearAll();
}

do_get_profile();

add_task(async function test_deleteAll() {
  addTestData();

  info("Deleting all data.");
  await new Promise(function (resolve) {
    Services.clearData.deleteData(
      CLEAR_BOUNCE_TRACKING_PROTECTION_STATE,
      failedFlags => {
        Assert.equal(failedFlags, 0, "Clearing should have succeeded");
        resolve();
      }
    );
  });

  Assert.equal(
    bounceTrackingProtection.testGetBounceTrackerCandidateHosts(OA_DEFAULT)
      .length,
    0,
    "All bounce tracker candidates for default OA should be deleted."
  );
  Assert.equal(
    bounceTrackingProtection.testGetUserActivationHosts(OA_DEFAULT).length,
    0,
    "All user activations for default OA should be deleted."
  );

  Assert.equal(
    bounceTrackingProtection.testGetBounceTrackerCandidateHosts(
      OA_PRIVATE_BROWSING
    ).length,
    0,
    "All bounce tracker candidates for private browsing should be deleted."
  );
  Assert.equal(
    bounceTrackingProtection.testGetUserActivationHosts(OA_PRIVATE_BROWSING)
      .length,
    0,
    "All user activations for private browsing should be deleted."
  );

  Assert.equal(
    bounceTrackingProtection.testGetBounceTrackerCandidateHosts(OA_CONTAINER)
      .length,
    0,
    "All bounce tracker candidates for container 1 should be deleted."
  );
  Assert.equal(
    bounceTrackingProtection.testGetUserActivationHosts(OA_CONTAINER).length,
    0,
    "All user activations for container 1 should be deleted."
  );
});

add_task(async function test_deleteByPrincipal() {
  addTestData();

  let principal = Services.scriptSecurityManager.createContentPrincipal(
    Services.io.newURI("https://common-bounce-tracker.com"),
    {}
  );
  console.debug("principal", principal.origin);

  info("Deleting by principal " + principal.origin);
  await new Promise(function (resolve) {
    Services.clearData.deleteDataFromPrincipal(
      principal,
      false,
      CLEAR_BOUNCE_TRACKING_PROTECTION_STATE,
      failedFlags => {
        Assert.equal(failedFlags, 0, "Clearing should have succeeded");
        resolve();
      }
    );
  });

  Assert.deepEqual(
    bounceTrackingProtection
      .testGetBounceTrackerCandidateHosts(OA_DEFAULT)
      .map(entry => entry.siteHost)
      .sort(),
    [
      "bounce-tracker-normal-browsing.com",
      "bounce-tracker-normal-browsing2.com",
    ],
    "Should have deleted only 'common-bounce-tracker.com' for default OA."
  );
  Assert.deepEqual(
    bounceTrackingProtection
      .testGetUserActivationHosts(OA_DEFAULT)
      .map(entry => entry.siteHost)
      .sort(),
    [
      "common-user-activation.com",
      "user-activation-normal-browsing.com",
      "user-activation-normal-browsing2.com",
    ],
    "Should not have deleted any user activations for default OA."
  );

  Assert.deepEqual(
    bounceTrackingProtection
      .testGetBounceTrackerCandidateHosts(OA_PRIVATE_BROWSING)
      .map(entry => entry.siteHost)
      .sort(),
    [
      "bounce-tracker-private-browsing.com",
      "bounce-tracker-private-browsing2.com",
      "common-bounce-tracker.com",
    ],
    "Should not have deleted 'common-bounce-tracker.com' for private browsing."
  );
  Assert.deepEqual(
    bounceTrackingProtection
      .testGetUserActivationHosts(OA_PRIVATE_BROWSING)
      .map(entry => entry.siteHost)
      .sort(),
    [
      "common-user-activation.com",
      "user-activation-private-browsing.com",
      "user-activation-private-browsing2.com",
    ],
    "Should not have deleted any user activations for private browsing."
  );

  Assert.deepEqual(
    bounceTrackingProtection
      .testGetBounceTrackerCandidateHosts(OA_CONTAINER)
      .map(entry => entry.siteHost)
      .sort(),
    [
      "bounce-tracker-container.com",
      "bounce-tracker-container2.com",
      "common-bounce-tracker.com",
    ],
    "Should not have deleted 'common-bounce-tracker.com' for container."
  );
  Assert.deepEqual(
    bounceTrackingProtection
      .testGetUserActivationHosts(OA_CONTAINER)
      .map(entry => entry.siteHost)
      .sort(),
    [
      "common-user-activation.com",
      "user-activation-container.com",
      "user-activation-container2.com",
    ],
    "Should not have deleted any user activations for container."
  );

  let principal2 = Services.scriptSecurityManager.createContentPrincipal(
    Services.io.newURI("https://common-user-activation.com"),
    OA_CONTAINER
  );

  info("Deleting by principal " + principal2.origin);
  await new Promise(function (resolve) {
    Services.clearData.deleteDataFromPrincipal(
      principal2,
      false,
      CLEAR_BOUNCE_TRACKING_PROTECTION_STATE,
      failedFlags => {
        Assert.equal(failedFlags, 0, "Clearing should have succeeded");
        resolve();
      }
    );
  });

  Assert.deepEqual(
    bounceTrackingProtection
      .testGetBounceTrackerCandidateHosts(OA_DEFAULT)
      .map(entry => entry.siteHost)
      .sort(),
    [
      "bounce-tracker-normal-browsing.com",
      "bounce-tracker-normal-browsing2.com",
    ],
    "Should not have deleted any bounce tracker candidates for default OA."
  );
  Assert.deepEqual(
    bounceTrackingProtection
      .testGetUserActivationHosts(OA_DEFAULT)
      .map(entry => entry.siteHost)
      .sort(),
    [
      "common-user-activation.com",
      "user-activation-normal-browsing.com",
      "user-activation-normal-browsing2.com",
    ],
    "Should not have deleted 'common-user-activation.com' for default OA."
  );

  Assert.deepEqual(
    bounceTrackingProtection
      .testGetBounceTrackerCandidateHosts(OA_PRIVATE_BROWSING)
      .map(entry => entry.siteHost)
      .sort(),
    [
      "bounce-tracker-private-browsing.com",
      "bounce-tracker-private-browsing2.com",
      "common-bounce-tracker.com",
    ],
    "Should not have deleted any bounce tracker candidates for private browsing."
  );
  Assert.deepEqual(
    bounceTrackingProtection
      .testGetUserActivationHosts(OA_PRIVATE_BROWSING)
      .map(entry => entry.siteHost)
      .sort(),
    [
      "common-user-activation.com",
      "user-activation-private-browsing.com",
      "user-activation-private-browsing2.com",
    ],
    "Should not have deleted 'common-user-activation.com' for private browsing."
  );

  Assert.deepEqual(
    bounceTrackingProtection
      .testGetBounceTrackerCandidateHosts(OA_CONTAINER)
      .map(entry => entry.siteHost)
      .sort(),
    [
      "bounce-tracker-container.com",
      "bounce-tracker-container2.com",
      "common-bounce-tracker.com",
    ],
    "Should not have deleted any bounce tracker candidates for container."
  );
  Assert.deepEqual(
    bounceTrackingProtection
      .testGetUserActivationHosts(OA_CONTAINER)
      .map(entry => entry.siteHost)
      .sort(),
    ["user-activation-container.com", "user-activation-container2.com"],
    "Should have deleted 'common-user-activation.com' for private browsing.."
  );

  // Cleanup.
  bounceTrackingProtection.clearAll();
});

add_task(async function test_deleteByBaseDomain() {
  await runDeleteBySiteHostTest(Services.clearData.deleteDataFromBaseDomain);
});

add_task(async function test_deleteByRange() {
  addTestData();

  let startTime = 200;
  let endTime = 1300;

  info(`Deleting by range ${startTime} - ${endTime}`);
  await new Promise(function (resolve) {
    Services.clearData.deleteDataInTimeRange(
      startTime,
      endTime,
      true,
      CLEAR_BOUNCE_TRACKING_PROTECTION_STATE,
      failedFlags => {
        Assert.equal(failedFlags, 0, "Clearing should have succeeded");
        resolve();
      }
    );
  });

  Assert.deepEqual(
    bounceTrackingProtection
      .testGetBounceTrackerCandidateHosts(OA_DEFAULT)
      .map(entry => entry.siteHost)
      .sort(),
    ["common-bounce-tracker.com"],
    "Should have only kept 'common-bounce-tracker.com' for default OA."
  );
  Assert.deepEqual(
    bounceTrackingProtection
      .testGetUserActivationHosts(OA_DEFAULT)
      .map(entry => entry.siteHost)
      .sort(),
    [],
    "Should not have kept any user activations for default OA."
  );

  Assert.deepEqual(
    bounceTrackingProtection
      .testGetBounceTrackerCandidateHosts(OA_PRIVATE_BROWSING)
      .map(entry => entry.siteHost)
      .sort(),
    [],
    "Should not have kept any bounces for private browsing."
  );
  Assert.deepEqual(
    bounceTrackingProtection
      .testGetUserActivationHosts(OA_PRIVATE_BROWSING)
      .map(entry => entry.siteHost)
      .sort(),
    [],
    "Should not have kept any user activations for private browsing."
  );

  Assert.deepEqual(
    bounceTrackingProtection
      .testGetBounceTrackerCandidateHosts(OA_CONTAINER)
      .map(entry => entry.siteHost)
      .sort(),
    ["bounce-tracker-container.com", "bounce-tracker-container2.com"],
    "Should have only kept some bouncer trackers for container."
  );
  Assert.deepEqual(
    bounceTrackingProtection
      .testGetUserActivationHosts(OA_CONTAINER)
      .map(entry => entry.siteHost)
      .sort(),
    [
      "common-user-activation.com",
      "user-activation-container.com",
      "user-activation-container2.com",
    ],
    "Should have kept all user activations for container."
  );

  // Cleanup.
  bounceTrackingProtection.clearAll();
});

add_task(async function test_deleteByHost() {
  await runDeleteBySiteHostTest(Services.clearData.deleteDataFromHost);
});

add_task(async function test_deleteByOriginAttributes() {
  addTestData();

  info("Deleting by origin attributes. " + JSON.stringify(OA_CONTAINER));
  await new Promise(function (resolve) {
    Services.clearData.deleteDataFromOriginAttributesPattern(
      OA_CONTAINER,
      failedFlags => {
        Assert.equal(failedFlags, 0, "Clearing should have succeeded");
        resolve();
      }
    );
  });

  Assert.deepEqual(
    bounceTrackingProtection
      .testGetBounceTrackerCandidateHosts(OA_DEFAULT)
      .map(entry => entry.siteHost)
      .sort(),
    [
      "bounce-tracker-normal-browsing.com",
      "bounce-tracker-normal-browsing2.com",
      "common-bounce-tracker.com",
    ],
    "Should not have deleted any bounce tracker candidates for default OA."
  );
  Assert.deepEqual(
    bounceTrackingProtection
      .testGetUserActivationHosts(OA_DEFAULT)
      .map(entry => entry.siteHost)
      .sort(),
    [
      "common-user-activation.com",
      "user-activation-normal-browsing.com",
      "user-activation-normal-browsing2.com",
    ],
    "Should not have deleted any user activations for default OA."
  );

  Assert.deepEqual(
    bounceTrackingProtection
      .testGetBounceTrackerCandidateHosts(OA_PRIVATE_BROWSING)
      .map(entry => entry.siteHost)
      .sort(),
    [
      "bounce-tracker-private-browsing.com",
      "bounce-tracker-private-browsing2.com",
      "common-bounce-tracker.com",
    ],
    "Should not have deleted any bounce tracker candidates for private browsing."
  );
  Assert.deepEqual(
    bounceTrackingProtection
      .testGetUserActivationHosts(OA_PRIVATE_BROWSING)
      .map(entry => entry.siteHost)
      .sort(),
    [
      "common-user-activation.com",
      "user-activation-private-browsing.com",
      "user-activation-private-browsing2.com",
    ],
    "Should not have deleted any user activations for private browsing."
  );

  Assert.deepEqual(
    bounceTrackingProtection
      .testGetBounceTrackerCandidateHosts(OA_CONTAINER)
      .map(entry => entry.siteHost)
      .sort(),
    [],
    "Should have deleted all bounce tracker candidates for container."
  );
  Assert.deepEqual(
    bounceTrackingProtection
      .testGetUserActivationHosts(OA_CONTAINER)
      .map(entry => entry.siteHost)
      .sort(),
    [],
    "Should have deleted all user activations for container."
  );

  info("Deleting by origin attributes {} (all).");
  await new Promise(function (resolve) {
    Services.clearData.deleteDataFromOriginAttributesPattern(
      OA_DEFAULT,
      failedFlags => {
        Assert.equal(failedFlags, 0, "Clearing should have succeeded");
        resolve();
      }
    );
  });

  Assert.equal(
    bounceTrackingProtection.testGetBounceTrackerCandidateHosts(OA_DEFAULT)
      .length,
    0,
    "Should have deleted all bounce tracker candidates for default OA."
  );
  Assert.equal(
    bounceTrackingProtection.testGetUserActivationHosts(OA_DEFAULT).length,
    0,
    "Should have deleted all user activations for default OA."
  );

  Assert.equal(
    bounceTrackingProtection.testGetBounceTrackerCandidateHosts(
      OA_PRIVATE_BROWSING
    ).length,
    0,
    "Should have deleted all bounce tracker candidates for private browsing."
  );
  Assert.equal(
    bounceTrackingProtection.testGetUserActivationHosts(OA_PRIVATE_BROWSING)
      .length,
    0,
    "Should have deleted all user activations for private browsing."
  );

  Assert.equal(
    bounceTrackingProtection.testGetBounceTrackerCandidateHosts(OA_CONTAINER)
      .length,
    0,
    "Should have deleted all bounce tracker candidates for container."
  );
  Assert.equal(
    bounceTrackingProtection.testGetUserActivationHosts(OA_CONTAINER).length,
    0,
    "Should have deleted all user activations for container."
  );

  // Cleanup.
  bounceTrackingProtection.clearAll();
});
