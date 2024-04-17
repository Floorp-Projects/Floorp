/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let bounceTrackingProtection;

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

/**
 * Tests that purge events are recorded correctly.
 */
add_task(async function test_purge_event() {
  is(
    Glean.bounceTrackingProtection.actionPurge.testGetValue(),
    null,
    "There should be no purge events initially."
  );

  info("Run server bounce with cookie.");
  await runTestBounce({
    bounceType: "server",
    setState: "cookie-server",
    postBounceCallback: () => {
      is(
        Glean.bounceTrackingProtection.actionPurge.testGetValue(),
        null,
        "There should still be no purge events after bounce, because we haven't purged yet."
      );
    },
  });

  let events = Glean.bounceTrackingProtection.actionPurge.testGetValue();
  is(events.length, 1, "There should be one purge event after bounce.");

  let [event] = events;
  is(
    event.extra.site_host,
    "itisatracker.org",
    "The site host should be correct."
  );
  ok(event.extra.success, "Purging should have succeeded");

  let bounceTimeInt = Number.parseInt(event.extra.bounce_time, 10);
  Assert.greater(bounceTimeInt, 0, "The bounce time should be greater than 0.");
  Assert.less(
    bounceTimeInt,
    Date.now() / 1000,
    "The bounce time should be before the current time."
  );

  // Cleanup
  // runTestBounceHelper already cleans up bounce tracking protection state.
  Services.fog.testResetFOG();
});
