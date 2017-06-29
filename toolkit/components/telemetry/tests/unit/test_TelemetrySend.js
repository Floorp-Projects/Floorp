/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/
*/

// This tests the public Telemetry API for submitting pings.

"use strict";

Cu.import("resource://gre/modules/TelemetryController.jsm", this);
Cu.import("resource://testing-common/ContentTaskUtils.jsm", this);
Cu.import("resource://gre/modules/TelemetrySession.jsm", this);
Cu.import("resource://gre/modules/TelemetrySend.jsm", this);
Cu.import("resource://gre/modules/TelemetryStorage.jsm", this);
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
  const id = HEAD + type + TAIL.slice(type.length, -N.length) + N;
  fakeGeneratePingId(() => id);
  return id;
}

var checkPingsSaved = async function(pingIds) {
  let allFound = true;
  for (let id of pingIds) {
    const path = OS.Path.join(TelemetryStorage.pingDirectoryPath, id);
    let exists = false;
    try {
      exists = await OS.File.exists(path);
    } catch (ex) {}

    if (!exists) {
      dump("checkPingsSaved - failed to find ping: " + path + "\n");
      allFound = false;
    }
  }

  return allFound;
};

function histogramValueCount(h) {
  return h.counts.reduce((a, b) => a + b);
}

add_task(async function test_setup() {
  // Trigger a proper telemetry init.
  do_get_profile(true);
  // Make sure we don't generate unexpected pings due to pref changes.
  await setEmptyPrefWatchlist();
  Services.prefs.setBoolPref(PREF_TELEMETRY_ENABLED, true);
});

// Test the ping sending logic.
add_task(async function test_sendPendingPings() {
  const TYPE_PREFIX = "test-sendPendingPings-";
  const TEST_TYPE_A = TYPE_PREFIX + "A";
  const TEST_TYPE_B = TYPE_PREFIX + "B";

  const TYPE_A_COUNT = 20;
  const TYPE_B_COUNT = 5;

  let histSuccess = Telemetry.getHistogramById("TELEMETRY_SUCCESS");
  let histSendTimeSuccess = Telemetry.getHistogramById("TELEMETRY_SEND_SUCCESS");
  let histSendTimeFail = Telemetry.getHistogramById("TELEMETRY_SEND_FAILURE");
  histSuccess.clear();
  histSendTimeSuccess.clear();
  histSendTimeFail.clear();

  // Fake a current date.
  let now = TelemetryUtils.truncateToDays(new Date());
  now = fakeNow(futureDate(now, 10 * 60 * MS_IN_A_MINUTE));

  // Enable test-mode for TelemetrySend, otherwise we won't store pending pings
  // before the module is fully initialized later.
  TelemetrySend.setTestModeEnabled(true);

  // Submit some pings without the server and telemetry started yet.
  for (let i = 0; i < TYPE_A_COUNT; ++i) {
    fakePingId("a", i);
    const id = await TelemetryController.submitExternalPing(TEST_TYPE_A, {});
    await setPingLastModified(id, now.getTime() + (i * 1000));
  }

  Assert.equal(TelemetrySend.pendingPingCount, TYPE_A_COUNT,
               "Should have correct pending ping count");

  // Submit some more pings of a different type.
  now = fakeNow(futureDate(now, 5 * MS_IN_A_MINUTE));
  for (let i = 0; i < TYPE_B_COUNT; ++i) {
    fakePingId("b", i);
    const id = await TelemetryController.submitExternalPing(TEST_TYPE_B, {});
    await setPingLastModified(id, now.getTime() + (i * 1000));
  }

  Assert.equal(TelemetrySend.pendingPingCount, TYPE_A_COUNT + TYPE_B_COUNT,
               "Should have correct pending ping count");

  Assert.deepEqual(histSuccess.snapshot().counts, [0, 0, 0],
               "Should not have recorded any sending in histograms yet.");
  Assert.equal(histSendTimeSuccess.snapshot().sum, 0,
               "Should not have recorded any sending in histograms yet.");
  Assert.equal(histSendTimeFail.snapshot().sum, 0,
               "Should not have recorded any sending in histograms yet.");

  // Now enable sending to the ping server.
  now = fakeNow(futureDate(now, MS_IN_A_MINUTE));
  PingServer.start();
  Preferences.set(PREF_TELEMETRY_SERVER, "http://localhost:" + PingServer.port);

  let timerPromise = waitForTimer();
  await TelemetryController.testReset();
  let [pingSendTimerCallback, pingSendTimeout] = await timerPromise;
  Assert.ok(!!pingSendTimerCallback, "Should have a timer callback");

  // We should have received 10 pings from the first send batch:
  // 5 of type B and 5 of type A, as sending is newest-first.
  // The other pings should be delayed by the 10-pings-per-minute limit.
  let pings = await PingServer.promiseNextPings(10);
  Assert.equal(TelemetrySend.pendingPingCount, TYPE_A_COUNT - 5,
               "Should have correct pending ping count");
  PingServer.registerPingHandler(() => Assert.ok(false, "Should not have received any pings now"));
  let countByType = countPingTypes(pings);

  Assert.equal(countByType.get(TEST_TYPE_B), TYPE_B_COUNT,
               "Should have received the correct amount of type B pings");
  Assert.equal(countByType.get(TEST_TYPE_A), 10 - TYPE_B_COUNT,
               "Should have received the correct amount of type A pings");

  Assert.deepEqual(histSuccess.snapshot().counts, [0, 10, 0],
               "Should have recorded sending success in histograms.");
  Assert.equal(histogramValueCount(histSendTimeSuccess.snapshot()), 10,
               "Should have recorded successful send times in histograms.");
  Assert.equal(histogramValueCount(histSendTimeFail.snapshot()), 0,
               "Should not have recorded any failed sending in histograms yet.");

  // As we hit the ping send limit and still have pending pings, a send tick should
  // be scheduled in a minute.
  Assert.ok(!!pingSendTimerCallback, "Timer callback should be set");
  Assert.equal(pingSendTimeout, MS_IN_A_MINUTE, "Send tick timeout should be correct");

  // Trigger the next tick - we should receive the next 10 type A pings.
  PingServer.resetPingHandler();
  now = fakeNow(futureDate(now, pingSendTimeout));
  timerPromise = waitForTimer();
  pingSendTimerCallback();
  [pingSendTimerCallback, pingSendTimeout] = await timerPromise;

  pings = await PingServer.promiseNextPings(10);
  PingServer.registerPingHandler(() => Assert.ok(false, "Should not have received any pings now"));
  countByType = countPingTypes(pings);

  Assert.equal(countByType.get(TEST_TYPE_A), 10, "Should have received the correct amount of type A pings");

  // We hit the ping send limit again and still have pending pings, a send tick should
  // be scheduled in a minute.
  Assert.equal(pingSendTimeout, MS_IN_A_MINUTE, "Send tick timeout should be correct");

  // Trigger the next tick - we should receive the remaining type A pings.
  PingServer.resetPingHandler();
  now = fakeNow(futureDate(now, pingSendTimeout));
  await pingSendTimerCallback();

  pings = await PingServer.promiseNextPings(5);
  PingServer.registerPingHandler(() => Assert.ok(false, "Should not have received any pings now"));
  countByType = countPingTypes(pings);

  Assert.equal(countByType.get(TEST_TYPE_A), 5, "Should have received the correct amount of type A pings");

  await TelemetrySend.testWaitOnOutgoingPings();
  PingServer.resetPingHandler();
});

add_task(async function test_sendDateHeader() {
  fakeNow(new Date(Date.UTC(2011, 1, 1, 11, 0, 0)));
  await TelemetrySend.reset();

  let pingId = await TelemetryController.submitExternalPing("test-send-date-header", {});
  let req = await PingServer.promiseNextRequest();
  let ping = decodeRequestPayload(req);
  Assert.equal(req.getHeader("Date"), "Tue, 01 Feb 2011 11:00:00 GMT",
               "Telemetry should send the correct Date header with requests.");
  Assert.equal(ping.id, pingId, "Should have received the correct ping id.");
});

// Test the backoff timeout behavior after send failures.
add_task(async function test_backoffTimeout() {
  const TYPE_PREFIX = "test-backoffTimeout-";
  const TEST_TYPE_C = TYPE_PREFIX + "C";
  const TEST_TYPE_D = TYPE_PREFIX + "D";
  const TEST_TYPE_E = TYPE_PREFIX + "E";

  let histSuccess = Telemetry.getHistogramById("TELEMETRY_SUCCESS");
  let histSendTimeSuccess = Telemetry.getHistogramById("TELEMETRY_SEND_SUCCESS");
  let histSendTimeFail = Telemetry.getHistogramById("TELEMETRY_SEND_FAILURE");

  // Failing a ping send now should trigger backoff behavior.
  let now = fakeNow(2010, 1, 1, 11, 0, 0);
  await TelemetrySend.reset();
  PingServer.stop();

  histSuccess.clear();
  histSendTimeSuccess.clear();
  histSendTimeFail.clear();

  fakePingId("c", 0);
  now = fakeNow(futureDate(now, MS_IN_A_MINUTE));
  let sendAttempts = 0;
  let timerPromise = waitForTimer();
  await TelemetryController.submitExternalPing(TEST_TYPE_C, {});
  let [pingSendTimerCallback, pingSendTimeout] = await timerPromise;
  Assert.equal(TelemetrySend.pendingPingCount, 1, "Should have one pending ping.");
  ++sendAttempts;

  const MAX_BACKOFF_TIMEOUT = 120 * MS_IN_A_MINUTE;
  for (let timeout = 2 * MS_IN_A_MINUTE; timeout <= MAX_BACKOFF_TIMEOUT; timeout *= 2) {
    Assert.ok(!!pingSendTimerCallback, "Should have received a timer callback");
    Assert.equal(pingSendTimeout, timeout, "Send tick timeout should be correct");

    let callback = pingSendTimerCallback;
    now = fakeNow(futureDate(now, pingSendTimeout));
    timerPromise = waitForTimer();
    await callback();
    [pingSendTimerCallback, pingSendTimeout] = await timerPromise;
    ++sendAttempts;
  }

  timerPromise = waitForTimer();
  await pingSendTimerCallback();
  [pingSendTimerCallback, pingSendTimeout] = await timerPromise;
  Assert.equal(pingSendTimeout, MAX_BACKOFF_TIMEOUT, "Tick timeout should be capped");
  ++sendAttempts;

  Assert.deepEqual(histSuccess.snapshot().counts, [sendAttempts, 0, 0],
               "Should have recorded sending failure in histograms.");
  Assert.equal(histSendTimeSuccess.snapshot().sum, 0,
               "Should not have recorded any sending success in histograms yet.");
  Assert.greater(histSendTimeFail.snapshot().sum, 0,
               "Should have recorded send failure times in histograms.");
  Assert.equal(histogramValueCount(histSendTimeFail.snapshot()), sendAttempts,
               "Should have recorded send failure times in histograms.");

  // Submitting a new ping should reset the backoff behavior.
  fakePingId("d", 0);
  now = fakeNow(futureDate(now, MS_IN_A_MINUTE));
  timerPromise = waitForTimer();
  await TelemetryController.submitExternalPing(TEST_TYPE_D, {});
  [pingSendTimerCallback, pingSendTimeout] = await timerPromise;
  Assert.equal(pingSendTimeout, 2 * MS_IN_A_MINUTE, "Send tick timeout should be correct");
  sendAttempts += 2;

  // With the server running again, we should send out the pending pings immediately
  // when a new ping is submitted.
  PingServer.start();
  TelemetrySend.setServer("http://localhost:" + PingServer.port);
  fakePingId("e", 0);
  now = fakeNow(futureDate(now, MS_IN_A_MINUTE));
  timerPromise = waitForTimer();
  await TelemetryController.submitExternalPing(TEST_TYPE_E, {});

  let pings = await PingServer.promiseNextPings(3);
  let countByType = countPingTypes(pings);

  Assert.equal(countByType.get(TEST_TYPE_C), 1, "Should have received the correct amount of type C pings");
  Assert.equal(countByType.get(TEST_TYPE_D), 1, "Should have received the correct amount of type D pings");
  Assert.equal(countByType.get(TEST_TYPE_E), 1, "Should have received the correct amount of type E pings");

  await TelemetrySend.testWaitOnOutgoingPings();
  Assert.equal(TelemetrySend.pendingPingCount, 0, "Should have no pending pings left");

  Assert.deepEqual(histSuccess.snapshot().counts, [sendAttempts, 3, 0],
               "Should have recorded sending failure in histograms.");
  Assert.greater(histSendTimeSuccess.snapshot().sum, 0,
               "Should have recorded sending success in histograms.");
  Assert.equal(histogramValueCount(histSendTimeSuccess.snapshot()), 3,
               "Should have recorded sending success in histograms.");
  Assert.equal(histogramValueCount(histSendTimeFail.snapshot()), sendAttempts,
               "Should have recorded send failure times in histograms.");
});

add_task(async function test_discardBigPings() {
  const TEST_PING_TYPE = "test-ping-type";

  let histSizeExceeded = Telemetry.getHistogramById("TELEMETRY_PING_SIZE_EXCEEDED_SEND");
  let histDiscardedSize = Telemetry.getHistogramById("TELEMETRY_DISCARDED_SEND_PINGS_SIZE_MB");
  let histSuccess = Telemetry.getHistogramById("TELEMETRY_SUCCESS");
  let histSendTimeSuccess = Telemetry.getHistogramById("TELEMETRY_SEND_SUCCESS");
  let histSendTimeFail = Telemetry.getHistogramById("TELEMETRY_SEND_FAILURE");
  for (let h of [histSizeExceeded, histDiscardedSize, histSuccess, histSendTimeSuccess, histSendTimeFail]) {
    h.clear();
  }

  // Generate a 2MB string and create an oversized payload.
  const OVERSIZED_PAYLOAD = {"data": generateRandomString(2 * 1024 * 1024)};

  // Submit a ping of a normal size and check that we don't count it in the histogram.
  await TelemetryController.submitExternalPing(TEST_PING_TYPE, { test: "test" });
  await TelemetrySend.testWaitOnOutgoingPings();

  Assert.equal(histSizeExceeded.snapshot().sum, 0, "Telemetry must report no oversized ping submitted.");
  Assert.equal(histDiscardedSize.snapshot().sum, 0, "Telemetry must report no oversized pings.");
  Assert.deepEqual(histSuccess.snapshot().counts, [0, 1, 0], "Should have recorded sending success.");
  Assert.equal(histogramValueCount(histSendTimeSuccess.snapshot()), 1, "Should have recorded send success time.");
  Assert.greater(histSendTimeSuccess.snapshot().sum, 0, "Should have recorded send success time.");
  Assert.equal(histogramValueCount(histSendTimeFail.snapshot()), 0, "Should not have recorded send failure time.");

  // Submit an oversized ping and check that it gets discarded.
  await TelemetryController.submitExternalPing(TEST_PING_TYPE, OVERSIZED_PAYLOAD);
  await TelemetrySend.testWaitOnOutgoingPings();

  Assert.equal(histSizeExceeded.snapshot().sum, 1, "Telemetry must report 1 oversized ping submitted.");
  Assert.equal(histDiscardedSize.snapshot().counts[2], 1, "Telemetry must report a 2MB, oversized, ping submitted.");
  Assert.deepEqual(histSuccess.snapshot().counts, [0, 1, 0], "Should have recorded sending success.");
  Assert.equal(histogramValueCount(histSendTimeSuccess.snapshot()), 1, "Should have recorded send success time.");
  Assert.greater(histSendTimeSuccess.snapshot().sum, 0, "Should have recorded send success time.");
  Assert.equal(histogramValueCount(histSendTimeFail.snapshot()), 0, "Should not have recorded send failure time.");
});

add_task(async function test_evictedOnServerErrors() {
  const TEST_TYPE = "test-evicted";

  await TelemetrySend.reset();

  let histEvicted = Telemetry.getHistogramById("TELEMETRY_PING_EVICTED_FOR_SERVER_ERRORS");
  let histSuccess = Telemetry.getHistogramById("TELEMETRY_SUCCESS");
  let histSendTimeSuccess = Telemetry.getHistogramById("TELEMETRY_SEND_SUCCESS");
  let histSendTimeFail = Telemetry.getHistogramById("TELEMETRY_SEND_FAILURE");
  for (let h of [histEvicted, histSuccess, histSendTimeSuccess, histSendTimeFail]) {
    h.clear();
  }

  // Write a custom ping handler which will return 403. This will trigger ping eviction
  // on client side.
  PingServer.registerPingHandler((req, res) => {
    res.setStatusLine(null, 403, "Forbidden");
    res.processAsync();
    res.finish();
  });

  // Clear the histogram and submit a ping.
  let pingId = await TelemetryController.submitExternalPing(TEST_TYPE, {});
  await TelemetrySend.testWaitOnOutgoingPings();

  Assert.equal(histEvicted.snapshot().sum, 1,
               "Telemetry must report a ping evicted due to server errors");
  Assert.deepEqual(histSuccess.snapshot().counts, [0, 1, 0]);
  Assert.equal(histogramValueCount(histSendTimeSuccess.snapshot()), 1);
  Assert.greater(histSendTimeSuccess.snapshot().sum, 0);
  Assert.equal(histogramValueCount(histSendTimeFail.snapshot()), 0);

  // The ping should not be persisted.
  await Assert.rejects(TelemetryStorage.loadPendingPing(pingId), "The ping must not be persisted.");

  // Reset the ping handler and submit a new ping.
  PingServer.resetPingHandler();
  pingId = await TelemetryController.submitExternalPing(TEST_TYPE, {});

  let ping = await PingServer.promiseNextPings(1);
  Assert.equal(ping[0].id, pingId, "The correct ping must be received");

  // We should not have updated the error histogram.
  await TelemetrySend.testWaitOnOutgoingPings();
  Assert.equal(histEvicted.snapshot().sum, 1, "Telemetry must report only one ping evicted due to server errors");
  Assert.deepEqual(histSuccess.snapshot().counts, [0, 2, 0]);
  Assert.equal(histogramValueCount(histSendTimeSuccess.snapshot()), 2);
  Assert.equal(histogramValueCount(histSendTimeFail.snapshot()), 0);
});

// Test that the current, non-persisted pending pings are properly saved on shutdown.
add_task(async function test_persistCurrentPingsOnShutdown() {
  const TEST_TYPE = "test-persistCurrentPingsOnShutdown";
  const PING_COUNT = 5;
  await TelemetrySend.reset();
  PingServer.stop();
  Assert.equal(TelemetrySend.pendingPingCount, 0, "Should have no pending pings yet");

  // Submit new pings that shouldn't be persisted yet.
  let ids = [];
  for (let i = 0; i < 5; ++i) {
    ids.push(fakePingId("f", i));
    TelemetryController.submitExternalPing(TEST_TYPE, {});
  }

  Assert.equal(TelemetrySend.pendingPingCount, PING_COUNT, "Should have the correct pending ping count");

  // Triggering a shutdown should persist the pings.
  await TelemetrySend.shutdown();
  Assert.ok((await checkPingsSaved(ids)), "All pending pings should have been persisted");

  // After a restart the pings should have been found when scanning.
  await TelemetrySend.reset();
  Assert.equal(TelemetrySend.pendingPingCount, PING_COUNT, "Should have the correct pending ping count");
});

add_task(async function test_sendCheckOverride() {
  const TEST_PING_TYPE = "test-sendCheckOverride";
  const PREF_OVERRIDE_OFFICIAL_CHECK = "toolkit.telemetry.send.overrideOfficialCheck";

  // Clear any pending pings.
  await TelemetryController.testShutdown();
  await TelemetryStorage.testClearPendingPings();

  // Enable the ping server.
  PingServer.start();
  Preferences.set(PREF_TELEMETRY_SERVER, "http://localhost:" + PingServer.port);

  // Start Telemetry and disable the test-mode so pings don't get
  // sent unless we enable the override.
  await TelemetryController.testReset();

  // Submit a test ping and make sure it doesn't get sent. We only do
  // that if we're on unofficial builds: pings will always get sent otherwise.
  if (!Services.telemetry.isOfficialTelemetry) {
    TelemetrySend.setTestModeEnabled(false);
    PingServer.registerPingHandler(() => Assert.ok(false, "Should not have received any pings now"));

    await TelemetryController.submitExternalPing(TEST_PING_TYPE, { test: "test" });
    Assert.equal(TelemetrySend.pendingPingCount, 0, "Should have no pending pings");
  }

  // Enable the override and try to send again.
  Preferences.set(PREF_OVERRIDE_OFFICIAL_CHECK, true);
  PingServer.resetPingHandler();
  await TelemetrySend.reset();
  await TelemetryController.submitExternalPing(TEST_PING_TYPE, { test: "test" });

  // Make sure we received the ping.
  const ping = await PingServer.promiseNextPing();
  Assert.equal(ping.type, TEST_PING_TYPE, "Must receive a ping of the expected type");

  // Restore the test mode and disable the override.
  TelemetrySend.setTestModeEnabled(true);
  Preferences.reset(PREF_OVERRIDE_OFFICIAL_CHECK);
});

add_task(async function test_measurePingsSize() {
  const TEST_TYPE = "test-measure-ping-size";

  let histSuccessPingSize = Telemetry.getHistogramById("TELEMETRY_SUCCESSFUL_SEND_PINGS_SIZE_KB");
  let histFailedPingSize = Telemetry.getHistogramById("TELEMETRY_FAILED_SEND_PINGS_SIZE_KB");

  for (let h of [histSuccessPingSize, histFailedPingSize]) {
    h.clear();
  }

  await TelemetryController.submitExternalPing(TEST_TYPE, {});
  await TelemetrySend.testWaitOnOutgoingPings();

  // Check that we recorded the ping sizes correctly into histograms.
  Assert.equal(histogramValueCount(histSuccessPingSize.snapshot()), 1,
    "Should have recorded 1 successful ping into histogram.");
  Assert.equal(histogramValueCount(histFailedPingSize.snapshot()), 0,
    "Should have recorded 0 failed ping into histogram.");

  // Submit the same ping a second time.
  await TelemetryController.submitExternalPing(TEST_TYPE, {});
  await TelemetrySend.testWaitOnOutgoingPings();

  // Check that we recorded the ping sizes correctly into histograms.
  Assert.equal(histogramValueCount(histSuccessPingSize.snapshot()), 2,
    "Should have recorded 2 successful ping into histogram.");
  Assert.equal(histogramValueCount(histFailedPingSize.snapshot()), 0,
    "Should have recorded 0 failed ping into histogram.");

  // Register a custom ping handler which will return 601.
  PingServer.registerPingHandler((req, res) => {
    res.setStatusLine(null, 601, "Not Implemented");
    res.processAsync();
    res.finish();
  });

  await TelemetryController.submitExternalPing(TEST_TYPE, {});
  await ContentTaskUtils.waitForCondition(() => {
    return histogramValueCount(histFailedPingSize.snapshot()) > 0;
  });

  // Check that we recorded the ping sizes correctly into histograms.
  Assert.equal(histogramValueCount(histSuccessPingSize.snapshot()), 2,
    "Should have recorded 2 successful ping into histogram.");
  Assert.equal(histogramValueCount(histFailedPingSize.snapshot()), 1,
    "Should have recorded 1 failed ping into histogram.");
});


add_task(async function cleanup() {
  await PingServer.stop();
});
