/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that expired user activations are cleared by the the helper method
 * testClearExpiredUserActivations.
 */
add_task(async function test() {
  // Need a profile to data clearing calls.
  do_get_profile();

  let btp = Cc["@mozilla.org/bounce-tracking-protection;1"].getService(
    Ci.nsIBounceTrackingProtection
  );

  // Reset global bounce tracking state.
  btp.clearAll();

  // Assert initial test state.
  Assert.deepEqual(
    btp.testGetBounceTrackerCandidateHosts({}),
    [],
    "No tracker candidates initially."
  );
  Assert.deepEqual(
    btp.testGetUserActivationHosts({}),
    [],
    "No user activation hosts initially."
  );

  // Get the bounce tracking activation lifetime. The pref is in seconds, we
  // need to convert it to microseconds, as the user activation timestamps are
  // in microseconds (PRTime).
  let bounceTrackingActivationLifetimeUSec =
    1000 *
    1000 *
    Services.prefs.getIntPref(
      "privacy.bounceTrackingProtection.bounceTrackingActivationLifetimeSec"
    );

  // Add some test data for user activation.
  btp.testAddUserActivation({}, "not-expired1.com", Date.now() * 1000);
  btp.testAddUserActivation(
    {},
    "not-expired2.com",
    Date.now() * 1000 - bounceTrackingActivationLifetimeUSec / 2
  );
  btp.testAddUserActivation(
    { privateBrowsingId: 1 },
    "pbm-not-expired.com",
    Date.now() * 1000
  );
  btp.testAddUserActivation(
    {},
    "expired1.com",
    Date.now() * 1000 - bounceTrackingActivationLifetimeUSec * 2
  );
  btp.testAddUserActivation(
    {},
    "expired2.com",
    Date.now() * 1000 - (bounceTrackingActivationLifetimeUSec + 1000 * 1000)
  );
  btp.testAddUserActivation({ privateBrowsingId: 1 }, "pbm-expired.com", 1);

  // Clear expired user activations.
  btp.testClearExpiredUserActivations();

  // Assert that expired user activations have been cleared.
  Assert.deepEqual(
    btp
      .testGetUserActivationHosts({})
      .map(entry => entry.siteHost)
      .sort(),
    ["not-expired1.com", "not-expired2.com"],
    "Expired user activation flags have been cleared for normal browsing."
  );

  Assert.deepEqual(
    btp
      .testGetUserActivationHosts({ privateBrowsingId: 1 })
      .map(entry => entry.siteHost)
      .sort(),
    ["pbm-not-expired.com"],
    "Expired user activation flags have been cleared for private browsing."
  );

  // Reset global bounce tracking state.
  btp.clearAll();
});
