/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/
*/

// This tests the public Telemetry API for submitting pings.

"use strict";

Cu.import("resource://gre/modules/TelemetryController.jsm", this);
Cu.import("resource://gre/modules/TelemetrySession.jsm", this);
Cu.import("resource://gre/modules/TelemetrySend.jsm", this);
Cu.import("resource://gre/modules/TelemetryUtils.jsm", this);
Cu.import("resource://gre/modules/Services.jsm", this);
Cu.import("resource://gre/modules/Preferences.jsm", this);
Cu.import("resource://gre/modules/osfile.jsm", this);

const PREF_TELEMETRY_SERVER = "toolkit.telemetry.server";

const MS_IN_A_MINUTE = 60 * 1000;

function countPingTypes(pings) {
  let countByType = new Map();
  for (let p of pings) {
    countByType.set(p.type, 1 + (countByType.get(p.type) || 0));
  }
  return countByType;
}

function setPingLastModified(id, timestamp) {
  const path = OS.Path.join(TelemetryStorage.pingDirectoryPath, id);
  return OS.File.setDates(path, null, timestamp);
}

// Mock out the send timer activity.
function waitForTimer() {
  return new Promise(resolve => {
    fakePingSendTimer((callback, timeout) => {
      resolve([callback, timeout]);
    }, () => {});
  });
}

// Allow easy faking of readable ping ids.
// This helps with debugging issues with e.g. ordering in the send logic.
function fakePingId(type, number) {
  const HEAD = "93bd0011-2c8f-4e1c-bee0-";
  const TAIL = "000000000000";
  const N = String(number);
  const id = HEAD + type + TAIL.slice(type.length, - N.length) + N;
  fakeGeneratePingId(() => id);
  return id;
}

var checkPingsSaved = Task.async(function* (pingIds) {
  let allFound = true;
  for (let id of pingIds) {
    const path = OS.Path.join(TelemetryStorage.pingDirectoryPath, id);
    let exists = false;
    try {
      exists = yield OS.File.exists(path);
    } catch (ex) {}

    if (!exists) {
      dump("checkPingsSaved - failed to find ping: " + path + "\n");
      allFound = false;
    }
  }

  return allFound;
});

add_task(function* test_setup() {
  // Trigger a proper telemetry init.
  do_get_profile(true);
  // Make sure we don't generate unexpected pings due to pref changes.
  yield setEmptyPrefWatchlist();
  Services.prefs.setBoolPref(PREF_TELEMETRY_ENABLED, true);
});

// Test the ping sending logic.
add_task(function* test_sendPendingPings() {
  const TYPE_PREFIX = "test-sendPendingPings-";
  const TEST_TYPE_A = TYPE_PREFIX + "A";
  const TEST_TYPE_B = TYPE_PREFIX + "B";

  const TYPE_A_COUNT = 20;
  const TYPE_B_COUNT = 5;

  // Fake a current date.
  let now = TelemetryUtils.truncateToDays(new Date());
  now = fakeNow(futureDate(now, 10 * 60 * MS_IN_A_MINUTE));

  // Enable test-mode for TelemetrySend, otherwise we won't store pending pings
  // before the module is fully initialized later.
  TelemetrySend.setTestModeEnabled(true);

  // Submit some pings without the server and telemetry started yet.
  for (let i = 0; i < TYPE_A_COUNT; ++i) {
    fakePingId("a", i);
    const id = yield TelemetryController.submitExternalPing(TEST_TYPE_A, {});
    yield setPingLastModified(id, now.getTime() + (i * 1000));
  }

  Assert.equal(TelemetrySend.pendingPingCount, TYPE_A_COUNT,
               "Should have correct pending ping count");

  // Submit some more pings of a different type.
  now = fakeNow(futureDate(now, 5 * MS_IN_A_MINUTE));
  for (let i = 0; i < TYPE_B_COUNT; ++i) {
    fakePingId("b", i);
    const id = yield TelemetryController.submitExternalPing(TEST_TYPE_B, {});
    yield setPingLastModified(id, now.getTime() + (i * 1000));
  }

  Assert.equal(TelemetrySend.pendingPingCount, TYPE_A_COUNT + TYPE_B_COUNT,
               "Should have correct pending ping count");

  // Now enable sending to the ping server.
  now = fakeNow(futureDate(now, MS_IN_A_MINUTE));
  PingServer.start();
  Preferences.set(PREF_TELEMETRY_SERVER, "http://localhost:" + PingServer.port);

  let timerPromise = waitForTimer();
  yield TelemetryController.testReset();
  let [pingSendTimerCallback, pingSendTimeout] = yield timerPromise;
  Assert.ok(!!pingSendTimerCallback, "Should have a timer callback");

  // We should have received 10 pings from the first send batch:
  // 5 of type B and 5 of type A, as sending is newest-first.
  // The other pings should be delayed by the 10-pings-per-minute limit.
  let pings = yield PingServer.promiseNextPings(10);
  Assert.equal(TelemetrySend.pendingPingCount, TYPE_A_COUNT - 5,
               "Should have correct pending ping count");
  PingServer.registerPingHandler(() => Assert.ok(false, "Should not have received any pings now"));
  let countByType = countPingTypes(pings);

  Assert.equal(countByType.get(TEST_TYPE_B), TYPE_B_COUNT,
               "Should have received the correct amount of type B pings");
  Assert.equal(countByType.get(TEST_TYPE_A), 10 - TYPE_B_COUNT,
               "Should have received the correct amount of type A pings");

  // As we hit the ping send limit and still have pending pings, a send tick should
  // be scheduled in a minute.
  Assert.ok(!!pingSendTimerCallback, "Timer callback should be set");
  Assert.equal(pingSendTimeout, MS_IN_A_MINUTE, "Send tick timeout should be correct");

  // Trigger the next tick - we should receive the next 10 type A pings.
  PingServer.resetPingHandler();
  now = fakeNow(futureDate(now, pingSendTimeout));
  timerPromise = waitForTimer();
  pingSendTimerCallback();
  [pingSendTimerCallback, pingSendTimeout] = yield timerPromise;

  pings = yield PingServer.promiseNextPings(10);
  PingServer.registerPingHandler(() => Assert.ok(false, "Should not have received any pings now"));
  countByType = countPingTypes(pings);

  Assert.equal(countByType.get(TEST_TYPE_A), 10, "Should have received the correct amount of type A pings");

  // We hit the ping send limit again and still have pending pings, a send tick should
  // be scheduled in a minute.
  Assert.equal(pingSendTimeout, MS_IN_A_MINUTE, "Send tick timeout should be correct");

  // Trigger the next tick - we should receive the remaining type A pings.
  PingServer.resetPingHandler();
  now = fakeNow(futureDate(now, pingSendTimeout));
  yield pingSendTimerCallback();

  pings = yield PingServer.promiseNextPings(5);
  PingServer.registerPingHandler(() => Assert.ok(false, "Should not have received any pings now"));
  countByType = countPingTypes(pings);

  Assert.equal(countByType.get(TEST_TYPE_A), 5, "Should have received the correct amount of type A pings");

  yield TelemetrySend.testWaitOnOutgoingPings();
  PingServer.resetPingHandler();
});

add_task(function* test_sendDateHeader() {
  let now = fakeNow(new Date(Date.UTC(2011, 1, 1, 11, 0, 0)));
  yield TelemetrySend.reset();

  let pingId = yield TelemetryController.submitExternalPing("test-send-date-header", {});
  let req = yield PingServer.promiseNextRequest();
  let ping = decodeRequestPayload(req);
  Assert.equal(req.getHeader("Date"), "Tue, 01 Feb 2011 11:00:00 GMT",
               "Telemetry should send the correct Date header with requests.");
  Assert.equal(ping.id, pingId, "Should have received the correct ping id.");
});

// Test the backoff timeout behavior after send failures.
add_task(function* test_backoffTimeout() {
  const TYPE_PREFIX = "test-backoffTimeout-";
  const TEST_TYPE_C = TYPE_PREFIX + "C";
  const TEST_TYPE_D = TYPE_PREFIX + "D";
  const TEST_TYPE_E = TYPE_PREFIX + "E";

  // Failing a ping send now should trigger backoff behavior.
  let now = fakeNow(2010, 1, 1, 11, 0, 0);
  yield TelemetrySend.reset();
  PingServer.stop();
  fakePingId("c", 0);
  now = fakeNow(futureDate(now, MS_IN_A_MINUTE));
  let timerPromise = waitForTimer();
  yield TelemetryController.submitExternalPing(TEST_TYPE_C, {});
  let [pingSendTimerCallback, pingSendTimeout] = yield timerPromise;
  Assert.equal(TelemetrySend.pendingPingCount, 1, "Should have one pending ping.");

  const MAX_BACKOFF_TIMEOUT = 120 * MS_IN_A_MINUTE;
  for (let timeout = 2 * MS_IN_A_MINUTE; timeout <= MAX_BACKOFF_TIMEOUT; timeout *= 2) {
    Assert.ok(!!pingSendTimerCallback, "Should have received a timer callback");
    Assert.equal(pingSendTimeout, timeout, "Send tick timeout should be correct");

    let callback = pingSendTimerCallback;
    now = fakeNow(futureDate(now, pingSendTimeout));
    timerPromise = waitForTimer();
    yield callback();
    [pingSendTimerCallback, pingSendTimeout] = yield timerPromise;
  }

  timerPromise = waitForTimer();
  yield pingSendTimerCallback();
  [pingSendTimerCallback, pingSendTimeout] = yield timerPromise;
  Assert.equal(pingSendTimeout, MAX_BACKOFF_TIMEOUT, "Tick timeout should be capped");

  // Submitting a new ping should reset the backoff behavior.
  fakePingId("d", 0);
  now = fakeNow(futureDate(now, MS_IN_A_MINUTE));
  timerPromise = waitForTimer();
  yield TelemetryController.submitExternalPing(TEST_TYPE_D, {});
  [pingSendTimerCallback, pingSendTimeout] = yield timerPromise;
  Assert.equal(pingSendTimeout, 2 * MS_IN_A_MINUTE, "Send tick timeout should be correct");

  // With the server running again, we should send out the pending pings immediately
  // when a new ping is submitted.
  PingServer.start();
  TelemetrySend.setServer("http://localhost:" + PingServer.port);
  fakePingId("e", 0);
  now = fakeNow(futureDate(now, MS_IN_A_MINUTE));
  timerPromise = waitForTimer();
  yield TelemetryController.submitExternalPing(TEST_TYPE_E, {});

  let pings = yield PingServer.promiseNextPings(3);
  let countByType = countPingTypes(pings);

  Assert.equal(countByType.get(TEST_TYPE_C), 1, "Should have received the correct amount of type C pings");
  Assert.equal(countByType.get(TEST_TYPE_D), 1, "Should have received the correct amount of type D pings");
  Assert.equal(countByType.get(TEST_TYPE_E), 1, "Should have received the correct amount of type E pings");

  yield TelemetrySend.testWaitOnOutgoingPings();
  Assert.equal(TelemetrySend.pendingPingCount, 0, "Should have no pending pings left");
});

add_task(function* test_discardBigPings() {
  const TEST_PING_TYPE = "test-ping-type";

  // Generate a 2MB string and create an oversized payload.
  const OVERSIZED_PAYLOAD = generateRandomString(2 * 1024 * 1024);

  // Reset the histograms.
  Telemetry.getHistogramById("TELEMETRY_PING_SIZE_EXCEEDED_SEND").clear();
  Telemetry.getHistogramById("TELEMETRY_DISCARDED_SEND_PINGS_SIZE_MB").clear();

  // Submit a ping of a normal size and check that we don't count it in the histogram.
  yield TelemetryController.submitExternalPing(TEST_PING_TYPE, { test: "test" });
  yield TelemetrySend.testWaitOnOutgoingPings();
  let h = Telemetry.getHistogramById("TELEMETRY_PING_SIZE_EXCEEDED_SEND").snapshot();
  Assert.equal(h.sum, 0, "Telemetry must report no oversized ping submitted.");
  h = Telemetry.getHistogramById("TELEMETRY_DISCARDED_SEND_PINGS_SIZE_MB").snapshot();
  Assert.equal(h.sum, 0, "Telemetry must report no oversized pings.");

  // Submit an oversized ping and check that it gets discarded.
  yield TelemetryController.submitExternalPing(TEST_PING_TYPE, OVERSIZED_PAYLOAD);
  yield TelemetrySend.testWaitOnOutgoingPings();
  h = Telemetry.getHistogramById("TELEMETRY_PING_SIZE_EXCEEDED_SEND").snapshot();
  Assert.equal(h.sum, 1, "Telemetry must report 1 oversized ping submitted.");
  h = Telemetry.getHistogramById("TELEMETRY_DISCARDED_SEND_PINGS_SIZE_MB").snapshot();
  Assert.equal(h.counts[2], 1, "Telemetry must report a 2MB, oversized, ping submitted.");
});

add_task(function* test_evictedOnServerErrors() {
  const TEST_TYPE = "test-evicted";

  yield TelemetrySend.reset();

  // Write a custom ping handler which will return 403. This will trigger ping eviction
  // on client side.
  PingServer.registerPingHandler((req, res) => {
    res.setStatusLine(null, 403, "Forbidden");
    res.processAsync();
    res.finish();
  });

  // Clear the histogram and submit a ping.
  Telemetry.getHistogramById("TELEMETRY_PING_EVICTED_FOR_SERVER_ERRORS").clear();
  let pingId = yield TelemetryController.submitExternalPing(TEST_TYPE, {});
  yield TelemetrySend.testWaitOnOutgoingPings();

  let h = Telemetry.getHistogramById("TELEMETRY_PING_EVICTED_FOR_SERVER_ERRORS").snapshot();
  Assert.equal(h.sum, 1, "Telemetry must report a ping evicted due to server errors");

  // The ping should not be persisted.
  yield Assert.rejects(TelemetryStorage.loadPendingPing(pingId), "The ping must not be persisted.");

  // Reset the ping handler and submit a new ping.
  PingServer.resetPingHandler();
  pingId = yield TelemetryController.submitExternalPing(TEST_TYPE, {});

  let ping = yield PingServer.promiseNextPings(1);
  Assert.equal(ping[0].id, pingId, "The correct ping must be received");

  // We should not have updated the error histogram.
  h = Telemetry.getHistogramById("TELEMETRY_PING_EVICTED_FOR_SERVER_ERRORS").snapshot();
  Assert.equal(h.sum, 1, "Telemetry must report a ping evicted due to server errors");
});

// Test that the current, non-persisted pending pings are properly saved on shutdown.
add_task(function* test_persistCurrentPingsOnShutdown() {
  const TEST_TYPE = "test-persistCurrentPingsOnShutdown";
  const PING_COUNT = 5;
  yield TelemetrySend.reset();
  PingServer.stop();
  Assert.equal(TelemetrySend.pendingPingCount, 0, "Should have no pending pings yet");

  // Submit new pings that shouldn't be persisted yet.
  let ids = [];
  for (let i=0; i<5; ++i) {
    ids.push(fakePingId("f", i));
    TelemetryController.submitExternalPing(TEST_TYPE, {});
  }

  Assert.equal(TelemetrySend.pendingPingCount, PING_COUNT, "Should have the correct pending ping count");

  // Triggering a shutdown should persist the pings.
  yield TelemetrySend.shutdown();
  Assert.ok((yield checkPingsSaved(ids)), "All pending pings should have been persisted");

  // After a restart the pings should have been found when scanning.
  yield TelemetrySend.reset();
  Assert.equal(TelemetrySend.pendingPingCount, PING_COUNT, "Should have the correct pending ping count");
});
