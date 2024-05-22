/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * helper function for testing that btp was initialized
 *
 * @param {Number} cookieMode: one of Ci.nsICookieService.BEHAVIOR* values
 * @param {Number} privateBrowsingId: run test in private/non-private mode
 */
async function testInit(cookieMode, privateBrowsingId) {
  if (privateBrowsingId != 0) {
    await SpecialPowers.pushPrefEnv({
      set: [["network.cookie.cookieBehavior", cookieMode]],
    });
  } else {
    await SpecialPowers.pushPrefEnv({
      set: [["network.cookie.cookieBehavior.pbmode", cookieMode]],
    });
  }

  let originAttributes = {
    privateBrowsingId,
  };

  info("Run server bounce with cookie.");
  await runTestBounce({
    bounceType: "server",
    setState: "cookie-server",
    originAttributes,
    postBounceCallback: () => {
      // Make sure we recorded bounceTracking
      let numTrackersPurged =
        bounceTrackingProtection.testGetBounceTrackerCandidateHosts(
          originAttributes
        ).length;
      Assert.equal(numTrackersPurged, 1, "All tracker candidates found.");
    },
  });

  // Cleanup
  await SpecialPowers.popPrefEnv();
  Services.fog.testResetFOG();
  bounceTrackingProtection.clearAll();
}

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.bounceTrackingProtection.requireStatefulBounces", true],
      ["privacy.bounceTrackingProtection.bounceTrackingGracePeriodSec", 0],
    ],
  });
});

add_task(async function () {
  for (let pbId = 0; pbId < 2; pbId++) {
    await testInit(Ci.nsICookieService.BEHAVIOR_REJECT_FOREIGN, pbId);
    await testInit(Ci.nsICookieService.BEHAVIOR_LIMIT_FOREIGN, pbId);
    await testInit(Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER, pbId);
  }
  Assert.equal(
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
    Ci.nsICookieService.BEHAVIOR_LAST,
    "test covers all cookie behaviours"
  );
});
