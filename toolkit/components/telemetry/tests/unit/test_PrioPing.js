/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

ChromeUtils.import("resource://gre/modules/TelemetryController.jsm", this);

ChromeUtils.defineModuleGetter(
  this,
  "TelemetryPrioPing",
  "resource://gre/modules/PrioPing.jsm"
);

function checkPingStructure(type, payload, options) {
  Assert.equal(
    type,
    TelemetryPrioPing.PRIO_PING_TYPE,
    "Should be a prio ping."
  );
  // Check the payload for required fields.
  Assert.ok("version" in payload, "Payload must have version.");
  Assert.ok("reason" in payload, "Payload must have reason.");
  Assert.ok(
    Object.values(TelemetryPrioPing.Reason).some(
      reason => payload.reason === reason
    ),
    "Should be a known reason."
  );
  Assert.ok(
    Array.isArray(payload.prioData),
    "Payload prioData must be present and an array."
  );
  payload.prioData.forEach(prioData => {
    Assert.ok("encoding" in prioData, "All prioData must have encodings.");
    Assert.ok("prio" in prioData, "All prioData must have prio blocks.");
  });
  // Ensure we forbid client id and environment
  Assert.equal(options.addClientId, false, "Must forbid client Id.");
  Assert.equal(options.addEnvironment, false, "Must forbid Environment.");
}

function fakePolicy(set, clear, send, snapshot) {
  let mod = ChromeUtils.import("resource://gre/modules/PrioPing.jsm", null);
  mod.Policy.setTimeout = set;
  mod.Policy.clearTimeout = clear;
  mod.Policy.sendPing = send;
  mod.Policy.getEncodedOriginSnapshot = snapshot;
}

function pass() {
  /* intentionally empty */
}
function fail() {
  Assert.ok(false, "Not allowed");
}
function fakeSnapshot() {
  return [
    {
      encoding: "telemetry.test-1-1",
      prio: {},
    },
    {
      encoding: "telemetry.test-1-1",
      prio: {},
    },
  ];
}

add_task(async function setup() {
  // Trigger a proper telemetry init.
  do_get_profile(true);
  // Make sure we don't generate unexpected pings due to pref changes.
  await setEmptyPrefWatchlist();

  await TelemetryController.testSetup();
  TelemetryPrioPing.testReset();
});

// Similarly to test_EventPing tests in this file often follow the form:
// 1: Fake out timeout, ping submission, and snapshotting
// 2: Trigger a "prio" ping to happen
// 3: Inside the fake ping submission, ensure the ping is correctly formed.
// In sinon this would be replaced with spies and .wasCalledWith().

add_task(async function test_limit_reached() {
  // Ensure that on being notified of the limit we immediately trigger a ping
  // with reason "max"

  fakePolicy(
    pass,
    pass,
    (type, payload, options) => {
      checkPingStructure(type, payload, options);
      Assert.equal(
        payload.reason,
        TelemetryPrioPing.Reason.MAX,
        "Sent using max reason."
      );
    },
    fakeSnapshot
  );
  Services.obs.notifyObservers(null, "origin-telemetry-storage-limit-reached");
});

add_task(async function test_periodic() {
  fakePolicy(
    pass,
    pass,
    (type, payload, options) => {
      checkPingStructure(type, payload, options);
      Assert.equal(
        payload.reason,
        TelemetryPrioPing.Reason.PERIODIC,
        "Sent with periodic reason."
      );
    },
    fakeSnapshot
  );

  // This is normally triggered by the scheduler once a day
  TelemetryPrioPing.periodicPing();
});

add_task(async function test_shutdown() {
  fakePolicy(
    fail,
    pass,
    (type, payload, options) => {
      checkPingStructure(type, payload, options);
      Assert.equal(
        payload.reason,
        TelemetryPrioPing.Reason.SHUTDOWN,
        "Sent with shutdown reason."
      );
    },
    fakeSnapshot
  );
  await TelemetryPrioPing.shutdown();
});
