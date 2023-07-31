/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { SiteDataTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/SiteDataTestUtils.sys.mjs"
);

let btp;
let bounceTrackingGracePeriodSec;
let bounceTrackingActivationLifetimeSec;

/**
 * Adds brackets to a host if it's an IPv6 address.
 * @param {string} host - Host which may be an IPv6.
 * @returns {string} bracketed IPv6 or host if host is not an IPv6.
 */
function maybeFixupIpv6(host) {
  if (!host.includes(":")) {
    return host;
  }
  return `[${host}]`;
}

/**
 * Adds cookies and indexedDB test data for the given host.
 * @param {string} host
 */
async function addStateForHost(host) {
  info(`adding state for host ${host}`);
  SiteDataTestUtils.addToCookies({ host });
  await SiteDataTestUtils.addToIndexedDB(`https://${maybeFixupIpv6(host)}`);
}

/**
 * Checks if the given host as cookies or indexedDB data.
 * @param {string} host
 * @returns {boolean}
 */
async function hasStateForHost(host) {
  let origin = `https://${maybeFixupIpv6(host)}`;
  if (SiteDataTestUtils.hasCookies(origin)) {
    return true;
  }
  return SiteDataTestUtils.hasIndexedDB(origin);
}

/**
 * Assert that there are no bounce tracker candidates or user activations
 * recorded.
 */
function assertEmpty() {
  Assert.equal(
    btp.bounceTrackerCandidateHosts.length,
    0,
    "No tracker candidates."
  );
  Assert.equal(btp.userActivationHosts.length, 0, "No user activation hosts.");
}

add_setup(function () {
  // Need a profile to data clearing calls.
  do_get_profile();

  btp = Cc["@mozilla.org/bounce-tracking-protection;1"].getService(
    Ci.nsIBounceTrackingProtection
  );

  // Reset global bounce tracking state.
  btp.reset();

  bounceTrackingGracePeriodSec = Services.prefs.getIntPref(
    "privacy.bounceTrackingProtection.bounceTrackingGracePeriodSec"
  );
  bounceTrackingActivationLifetimeSec = Services.prefs.getIntPref(
    "privacy.bounceTrackingProtection.bounceTrackingActivationLifetimeSec"
  );
});

/**
 * When both maps are empty running PurgeBounceTrackers should be a no-op.
 */
add_task(async function test_empty() {
  assertEmpty();

  info("Run PurgeBounceTrackers");
  await btp.testRunPurgeBounceTrackers();

  assertEmpty();
});

/**
 * Tests that the PurgeBounceTrackers behaves as expected by adding site state
 * and adding simulated bounce state and user activations.
 */
add_task(async function test_purge() {
  let now = Date.now();

  // Epoch in MS.
  let timestampWithinGracePeriod =
    now - (bounceTrackingGracePeriodSec * 1000) / 2;
  let timestampWithinGracePeriod2 =
    now - (bounceTrackingGracePeriodSec * 1000) / 4;
  let timestampOutsideGracePeriodFiveSeconds =
    now - (bounceTrackingGracePeriodSec + 5) * 1000;
  let timestampOutsideGracePeriodThreeDays =
    now - (bounceTrackingGracePeriodSec + 60 * 60 * 24 * 3) * 1000;
  let timestampFuture = now + bounceTrackingGracePeriodSec * 1000 * 2;

  let timestampValidUserActivation =
    now - (bounceTrackingActivationLifetimeSec * 1000) / 2;
  let timestampExpiredUserActivationFourSeconds =
    now - (bounceTrackingActivationLifetimeSec + 4) * 1000;
  let timestampExpiredUserActivationTenDays =
    now - (bounceTrackingActivationLifetimeSec + 60 * 60 * 24 * 10) * 1000;

  const TEST_TRACKERS = {
    "example.com": {
      bounceTime: timestampWithinGracePeriod,
      userActivationTime: null,
      message: "Should not purge within grace period.",
      shouldPurge: bounceTrackingGracePeriodSec == 0,
    },
    "example2.com": {
      bounceTime: timestampWithinGracePeriod2,
      userActivationTime: null,
      message: "Should not purge within grace period (2).",
      shouldPurge: bounceTrackingGracePeriodSec == 0,
    },
    "example.net": {
      bounceTime: timestampOutsideGracePeriodFiveSeconds,
      userActivationTime: null,
      message: "Should purge after grace period.",
      shouldPurge: true,
    },
    // Also ensure that clear data calls with IP sites succeed.
    "1.2.3.4": {
      bounceTime: timestampOutsideGracePeriodThreeDays,
      userActivationTime: null,
      message: "Should purge after grace period (2).",
      shouldPurge: true,
    },
    "2606:4700:4700::1111": {
      bounceTime: timestampOutsideGracePeriodThreeDays,
      userActivationTime: null,
      message: "Should purge after grace period (3).",
      shouldPurge: true,
    },
    "example.org": {
      bounceTime: timestampWithinGracePeriod,
      userActivationTime: null,
      message: "Should not purge within grace period.",
      shouldPurge: false,
    },
    "example2.org": {
      bounceTime: timestampFuture,
      userActivationTime: null,
      message: "Should not purge for future bounce time (within grace period).",
      shouldPurge: false,
    },
    "1.1.1.1": {
      bounceTime: null,
      userActivationTime: timestampValidUserActivation,
      message: "Should not purge without bounce (valid user activation).",
      shouldPurge: false,
    },
    // Also testing domains with trailing ".".
    "mozilla.org.": {
      bounceTime: null,
      userActivationTime: timestampExpiredUserActivationFourSeconds,
      message: "Should not purge without bounce (expired user activation).",
      shouldPurge: false,
    },
    "firefox.com": {
      bounceTime: null,
      userActivationTime: timestampExpiredUserActivationTenDays,
      message: "Should not purge without bounce (expired user activation) (2).",
      shouldPurge: false,
    },
  };

  info("Assert empty initially.");
  assertEmpty();

  info("Populate bounce and user activation sets.");

  let expectedBounceTrackerHosts = [];
  let expectedUserActivationHosts = [];

  let expiredUserActivationHosts = [];
  let expectedPurgedHosts = [];

  // This would normally happen over time while browsing.
  let initPromises = Object.entries(TEST_TRACKERS).map(
    async ([siteHost, { bounceTime, userActivationTime, shouldPurge }]) => {
      // Add site state so we can later assert it has been purged.
      await addStateForHost(siteHost);

      if (bounceTime != null) {
        if (userActivationTime != null) {
          throw new Error(
            "Attempting to construct invalid map state. bounceTrackerCandidateHosts and userActivationHosts must be disjoint."
          );
        }

        expectedBounceTrackerHosts.push(siteHost);

        // Convert bounceTime timestamp to nanoseconds (PRTime).
        info(
          `Adding bounce. siteHost: ${siteHost}, bounceTime: ${bounceTime} ms`
        );
        btp.testAddBounceTrackerCandidate(siteHost, bounceTime * 1000);
      }

      if (userActivationTime != null) {
        if (bounceTime != null) {
          throw new Error(
            "Attempting to construct invalid map state. bounceTrackerCandidateHosts and userActivationHosts must be disjoint."
          );
        }

        expectedUserActivationHosts.push(siteHost);
        if (
          userActivationTime + bounceTrackingActivationLifetimeSec * 1000 >
          now
        ) {
          expiredUserActivationHosts.push(siteHost);
        }

        // Convert userActivationTime timestamp to nanoseconds (PRTime).
        info(
          `Adding user interaction. siteHost: ${siteHost}, userActivationTime: ${userActivationTime} ms`
        );
        btp.testAddUserActivation(siteHost, userActivationTime * 1000);
      }

      if (shouldPurge) {
        expectedPurgedHosts.push(siteHost);
      }
    }
  );
  await Promise.all(initPromises);

  info(
    "Check that bounce and user activation data has been correctly recorded."
  );
  Assert.deepEqual(
    btp.bounceTrackerCandidateHosts.sort(),
    expectedBounceTrackerHosts.sort(),
    "Has added bounce tracker hosts."
  );
  Assert.deepEqual(
    btp.userActivationHosts.sort(),
    expectedUserActivationHosts.sort(),
    "Has added user activation hosts."
  );

  info("Run PurgeBounceTrackers");
  let actualPurgedHosts = await btp.testRunPurgeBounceTrackers();

  Assert.deepEqual(
    actualPurgedHosts.sort(),
    expectedPurgedHosts.sort(),
    "Should have purged all expected hosts."
  );

  let expectedBounceTrackerHostsAfterPurge = expectedBounceTrackerHosts
    .filter(host => !expectedPurgedHosts.includes(host))
    .sort();
  Assert.deepEqual(
    btp.bounceTrackerCandidateHosts.sort(),
    expectedBounceTrackerHostsAfterPurge.sort(),
    "After purge the bounce tracker candidate host set should be updated correctly."
  );

  Assert.deepEqual(
    btp.userActivationHosts.sort(),
    expiredUserActivationHosts.sort(),
    "After purge any expired user activation records should have been removed"
  );

  info("Test that we actually purged the correct sites.");
  for (let siteHost of expectedPurgedHosts) {
    Assert.ok(
      !(await hasStateForHost(siteHost)),
      `Site ${siteHost} should no longer have state.`
    );
  }
  for (let siteHost of expectedBounceTrackerHostsAfterPurge) {
    Assert.ok(
      await hasStateForHost(siteHost),
      `Site ${siteHost} should still have state.`
    );
  }

  info("Reset bounce tracking state.");
  btp.reset();
  assertEmpty();

  info("Clean up site data.");
  await SiteDataTestUtils.clear();
});
