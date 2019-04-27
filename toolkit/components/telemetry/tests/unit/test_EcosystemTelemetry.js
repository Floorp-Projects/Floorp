/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

ChromeUtils.import("resource://gre/modules/TelemetryController.jsm", this);
ChromeUtils.import("resource://gre/modules/TelemetryUtils.jsm", this);
ChromeUtils.import("resource://gre/modules/Preferences.jsm", this);

XPCOMUtils.defineLazyModuleGetters(this, {
  ONLOGIN_NOTIFICATION: "resource://gre/modules/FxAccountsCommon.js",
  ONLOGOUT_NOTIFICATION: "resource://gre/modules/FxAccountsCommon.js",
});
ChromeUtils.defineModuleGetter(this, "EcosystemTelemetry",
                               "resource://gre/modules/EcosystemTelemetry.jsm");

const WEAVE_EVENT = "weave:service:login:change";
const TEST_PING_TYPE = "test-ping-type";

function fakeIdleNotification(topic) {
  let scheduler = ChromeUtils.import("resource://gre/modules/TelemetryScheduler.jsm", null);
  return scheduler.TelemetryScheduler.observe(null, topic, null);
}

function checkPingStructure(ping, type, reason) {
  Assert.equal(ping.type, type, "Should be an ecosystem ping.");

  Assert.ok(!("clientId" in ping), "Ping must not contain a client ID.");
  Assert.ok("environment" in ping, "Ping must contain an environment.");
  let environment = ping.environment;

  // Check that the environment is indeed minimal
  const ALLOWED_ENVIRONMENT_KEYS = ["settings", "system", "profile"];
  Assert.deepEqual(ALLOWED_ENVIRONMENT_KEYS, Object.keys(environment), "Environment should only contain a limited set of keys.");

  // Check that fields of the environment are indeed minimal
  Assert.deepEqual(["locale"], Object.keys(environment.settings), "Settings environment should only contain locale");
  Assert.deepEqual(["cpu", "memoryMB", "os"], Object.keys(environment.system).sort(),
    "System environment should contain a limited set of keys");
  Assert.deepEqual(["locale", "name", "version"], Object.keys(environment.system.os).sort(),
    "system.environment.os should contain a limited set of keys");

  // Check the payload for required fields.
  let payload = ping.payload;
  Assert.equal(payload.reason, reason, "Ping reason must match.");
  Assert.ok(payload.duration >= 0, "Payload must have a duration greater or equal to 0");
  Assert.ok("ecosystemClientId" in payload, "Payload must contain the ecosystem client ID");

  Assert.ok("scalars" in payload, "Payload must contain scalars");
  Assert.ok("keyedScalars" in payload, "Payload must contain keyed scalars");
  Assert.ok("histograms" in payload, "Payload must contain histograms");
  Assert.ok("keyedHistograms" in payload, "Payload must contain keyed histograms");

  Assert.ok("telemetry.ecosystem_old_send_time" in payload.scalars.parent, "Old send time should be set");
  Assert.ok("telemetry.ecosystem_new_send_time" in payload.scalars.parent, "New send time should be set");
}

function sendPing() {
  return TelemetryController.submitExternalPing(TEST_PING_TYPE, {}, {});
}

function fakeFxaUid(fn) {
  const m = ChromeUtils.import("resource://gre/modules/EcosystemTelemetry.jsm", null);
  let oldFn = m.Policy.fxaUid;
  m.Policy.fxaUid = fn;
  return oldFn;
}

registerCleanupFunction(function() {
  PingServer.stop();
});

add_task(async function setup() {
  // Trigger a proper telemetry init.
  do_get_profile(true);
  // Make sure we don't generate unexpected pings due to pref changes.
  await setEmptyPrefWatchlist();

  // Start the local ping server and setup Telemetry to use it during the tests.
  PingServer.start();
  Preferences.set(TelemetryUtils.Preferences.Server, "http://localhost:" + PingServer.port);
  TelemetrySend.setServer("http://localhost:" + PingServer.port);

  await TelemetryController.testSetup();
});

// We make absolute sure the Ecosystem ping is never triggered on Fennec/Non-unified Telemetry
add_task({
  skip_if: () => !gIsAndroid,
}, async function test_no_ecosystem_ping_on_fennec() {
  // Force preference to true, we should have an additional check on Android/Unified Telemetry
  Preferences.set(TelemetryUtils.Preferences.EcosystemTelemetryEnabled, true);
  EcosystemTelemetry.testReset();

  // This is invoked in regular intervals by the timer.
  // Would trigger ping sending.
  EcosystemTelemetry.periodicPing();
  // Let's send a test ping to ensure we receive _something_.
  sendPing();

  let ping = await PingServer.promiseNextPing();
  Assert.equal(ping.type, TEST_PING_TYPE, "Should be a test ping.");
});

add_task({
  skip_if: () => gIsAndroid,
}, async function test_nosending_if_disabled() {
  Preferences.set(TelemetryUtils.Preferences.EcosystemTelemetryEnabled, false);
  EcosystemTelemetry.testReset();

  // This is invoked in regular intervals by the timer.
  // Would trigger ping sending.
  EcosystemTelemetry.periodicPing();
  // Let's send a test ping to ensure we receive _something_.
  sendPing();

  let ping = await PingServer.promiseNextPing();
  Assert.equal(ping.type, TEST_PING_TYPE, "Should be a test ping.");
});

add_task({
  skip_if: () => gIsAndroid,
}, async function test_simple_send() {
  Preferences.set(TelemetryUtils.Preferences.EcosystemTelemetryEnabled, true);
  EcosystemTelemetry.testReset();

  // This is invoked in regular intervals by the timer.
  EcosystemTelemetry.periodicPing();

  let ping = await PingServer.promiseNextPing();
  checkPingStructure(ping, "pre-account", "periodic");
});

add_task({
  skip_if: () => gIsAndroid,
}, async function test_login_workflow() {
  // Fake the whole login/logout workflow by triggering the events directly.

  Preferences.set(TelemetryUtils.Preferences.EcosystemTelemetryEnabled, true);
  EcosystemTelemetry.testReset();

  let originalFxaUid = fakeFxaUid(() => null);
  let ping;

  // 1. No user, timer invoked
  EcosystemTelemetry.periodicPing();
  ping = await PingServer.promiseNextPing();
  checkPingStructure(ping, "pre-account", "periodic");
  Assert.ok(!("uid" in ping.payload), "Ping should not contain a UID");

  // 2. User logs in, no uid available.
  //    No ping will be generated.
  EcosystemTelemetry.observe(null, ONLOGIN_NOTIFICATION, null);
  sendPing();
  ping = await PingServer.promiseNextPing();
  Assert.equal(ping.type, TEST_PING_TYPE, "Should be a test ping.");

  // 3. uid becomes available, weave syncs.
  fakeFxaUid(() => "hashed-id");
  EcosystemTelemetry.observe(null, WEAVE_EVENT, null);
  ping = await PingServer.promiseNextPing();
  checkPingStructure(ping, "pre-account", "login");
  Assert.ok("uid" in ping.payload, "Ping should contain hashed ID");

  // 4. User is logged in, timer invokes different ping
  EcosystemTelemetry.periodicPing();
  ping = await PingServer.promiseNextPing();
  checkPingStructure(ping, "pre-account", "periodic");
  Assert.ok("uid" in ping.payload, "Ping should contain hashed ID");

  // 5. User disconnects account, no uid available
  fakeFxaUid(() => null);
  EcosystemTelemetry.observe(null, ONLOGOUT_NOTIFICATION, null);
  ping = await PingServer.promiseNextPing();
  checkPingStructure(ping, "pre-account", "logout");
  Assert.ok(!("uid" in ping.payload), "Ping should not contain a UID");

  // 6. No user, timer invoked
  EcosystemTelemetry.periodicPing();
  ping = await PingServer.promiseNextPing();
  checkPingStructure(ping, "pre-account", "periodic");
  Assert.ok(!("uid" in ping.payload), "Ping should not contain a UID");

  // 7. Shutdown
  EcosystemTelemetry.shutdown();
  ping = await PingServer.promiseNextPing();
  checkPingStructure(ping, "pre-account", "shutdown");
  Assert.ok(!("uid" in ping.payload), "Ping should not contain a UID");

  // Reset policy.
  fakeFxaUid(originalFxaUid);
});

// Test that a periodic ping is triggered by the scheduler at midnight
//
// Based on `test_TelemetrySession#test_DailyDueAndIdle`.
add_task({
  skip_if: () => gIsAndroid,
}, async function test_periodic_ping() {
  await TelemetryStorage.testClearPendingPings();
  PingServer.clearRequests();

  const pingType = "pre-account";

  let receivedPing = null;
  // Register a ping handler that will assert when receiving multiple ecosystem pings.
  // We can ignore other pings, such as the periodic ping.
  PingServer.registerPingHandler(req => {
    const ping = decodeRequestPayload(req);
    if (ping.type == pingType) {
      Assert.ok(!receivedPing, "Telemetry must only send one periodic ecosystem ping.");
      receivedPing = ping;
    }
  });

  // Faking scheduler timer has to happen before resetting TelemetryController
  // to be effective.
  let schedulerTickCallback = null;
  let now = new Date(2040, 1, 1, 0, 0, 0);
  fakeNow(now);
  // Fake scheduler functions to control periodic collection flow in tests.
  fakeSchedulerTimer(callback => schedulerTickCallback = callback, () => {});
  await TelemetryController.testReset();

  Preferences.set(TelemetryUtils.Preferences.EcosystemTelemetryEnabled, true);
  EcosystemTelemetry.testReset();

  // Trigger the periodic ecosystem ping.
  let firstPeriodicDue = new Date(2040, 1, 2, 0, 0, 0);
  fakeNow(firstPeriodicDue);

  // Run a scheduler tick: it should trigger the periodic ping.
  Assert.ok(!!schedulerTickCallback);
  let tickPromise = schedulerTickCallback();

  // Send an idle and then an active user notification.
  fakeIdleNotification("idle");
  fakeIdleNotification("active");

  // Wait on the tick promise.
  await tickPromise;

  await TelemetrySend.testWaitOnOutgoingPings();

  // Decode the ping contained in the request and check that's a periodic ping.
  Assert.ok(receivedPing, "Telemetry must send one ecosystem periodic ping.");
  checkPingStructure(receivedPing, pingType, "periodic");
});
