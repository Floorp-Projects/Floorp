/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.bounceTrackingProtection.requireStatefulBounces", true],
      ["privacy.bounceTrackingProtection.bounceTrackingGracePeriodSec", 0],
    ],
  });
});

// Tests that bounces in PBM don't affect state in normal browsing.
add_task(async function test_pbm_data_isolated() {
  await runTestBounce({
    bounceType: "client",
    setState: "cookie-client",
    originAttributes: { privateBrowsingId: 1 },
    postBounceCallback: () => {
      // After the PBM bounce assert that we haven't recorded any data for normal browsing.
      Assert.equal(
        bounceTrackingProtection.testGetBounceTrackerCandidateHosts({}).length,
        0,
        "No bounce tracker candidates for normal browsing."
      );
      Assert.equal(
        bounceTrackingProtection.testGetUserActivationHosts({}).length,
        0,
        "No user activations for normal browsing."
      );
    },
  });
});

// Tests that bounces in PBM don't affect state in normal browsing.
add_task(async function test_containers_isolated() {
  await runTestBounce({
    bounceType: "server",
    setState: "cookie-server",
    originAttributes: { userContextId: 2 },
    postBounceCallback: () => {
      // After the bounce in the container tab assert that we haven't recorded any data for normal browsing.
      Assert.equal(
        bounceTrackingProtection.testGetBounceTrackerCandidateHosts({}).length,
        0,
        "No bounce tracker candidates for normal browsing."
      );
      Assert.equal(
        bounceTrackingProtection.testGetUserActivationHosts({}).length,
        0,
        "No user activations for normal browsing."
      );

      // Or in another container tab.
      Assert.equal(
        bounceTrackingProtection.testGetBounceTrackerCandidateHosts({
          userContextId: 1,
        }).length,
        0,
        "No bounce tracker candidates for container tab 1."
      );
      Assert.equal(
        bounceTrackingProtection.testGetUserActivationHosts({
          userContextId: 1,
        }).length,
        0,
        "No user activations for container tab 1."
      );
    },
  });
});
