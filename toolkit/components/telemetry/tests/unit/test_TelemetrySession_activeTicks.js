/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/
*/

ChromeUtils.import("resource://gre/modules/TelemetryController.jsm", this);
ChromeUtils.import("resource://gre/modules/TelemetrySession.jsm", this);


add_task(async function test_setup() {
  do_get_profile();
  // Make sure we don't generate unexpected pings due to pref changes.
  await setEmptyPrefWatchlist();
});

add_task(async function test_record_activeTicks() {
  await TelemetryController.testSetup();

  let checkActiveTicks = (expected) => {
    // Scalars are only present in subsession payloads.
    let payload = TelemetrySession.getPayload("main");
    Assert.equal(payload.simpleMeasurements.activeTicks, expected,
                 "TelemetrySession must record the expected number of active ticks (in simpleMeasurements).");
    // Subsessions are not yet supported on Android.
    if (!gIsAndroid) {
      Assert.equal(payload.processes.parent.scalars["browser.engagement.active_ticks"], expected,
                   "TelemetrySession must record the expected number of active ticks (in scalars).");
    }
  };

  for (let i = 0; i < 3; i++) {
    Services.obs.notifyObservers(null, "user-interaction-active");
  }
  checkActiveTicks(3);

  // Now send inactive. This must not increment the active ticks.
  Services.obs.notifyObservers(null, "user-interaction-inactive");
  checkActiveTicks(3);

  // If we send active again, this should be counted as inactive.
  Services.obs.notifyObservers(null, "user-interaction-active");
  checkActiveTicks(3);

  // If we send active again, this should be counted as active.
  Services.obs.notifyObservers(null, "user-interaction-active");
  checkActiveTicks(4);

  Services.obs.notifyObservers(null, "user-interaction-active");
  checkActiveTicks(5);

  await TelemetryController.testShutdown();
});
