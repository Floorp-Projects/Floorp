/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let bounceTrackingProtection;

async function test_purge_duration(isDryRunMode) {
  await SpecialPowers.pushPrefEnv({
    set: [["privacy.bounceTrackingProtection.enableDryRunMode", isDryRunMode]],
  });

  is(
    Glean.bounceTrackingProtection.purgeDuration.testGetValue(),
    null,
    "Histogram should not exist initially."
  );

  info("Run server bounce with cookie.");
  await runTestBounce({
    bounceType: "server",
    setState: "cookie-server",
    postBounceCallback: () => {
      is(
        Glean.bounceTrackingProtection.purgeDuration.testGetValue(),
        null,
        "Histogram should still be empty after bounce, because we haven't purged yet."
      );
    },
  });

  let events = Glean.bounceTrackingProtection.purgeDuration.testGetValue();
  if (isDryRunMode) {
    is(events, null, "Should not collect purge timining in dry mode");
  } else {
    is(events.count, 1, "Histogram should contain one value.");
  }

  // Cleanup
  Services.fog.testResetFOG();
  await SpecialPowers.popPrefEnv();
  bounceTrackingProtection.clearAll();
}

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.bounceTrackingProtection.requireStatefulBounces", true],
      ["privacy.bounceTrackingProtection.bounceTrackingGracePeriodSec", 0],
    ],
  });
  bounceTrackingProtection = Cc[
    "@mozilla.org/bounce-tracking-protection;1"
  ].getService(Ci.nsIBounceTrackingProtection);

  // Clear telemetry before test.
  Services.fog.testResetFOG();
});

add_task(async function test_purge_duration_dry_mode() {
  await test_purge_duration(true);
});

add_task(async function test_purge_duration_enabled() {
  await test_purge_duration(false);
});
