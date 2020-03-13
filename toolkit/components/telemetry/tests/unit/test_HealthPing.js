/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/
 */

// This tests the public Telemetry API for submitting Health pings.

"use strict";

ChromeUtils.import("resource://gre/modules/TelemetryController.jsm", this);
ChromeUtils.import("resource://gre/modules/TelemetryStorage.jsm", this);
ChromeUtils.import("resource://gre/modules/TelemetryUtils.jsm", this);
ChromeUtils.import("resource://gre/modules/Preferences.jsm", this);
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm", this);
ChromeUtils.import(
  "resource://testing-common/TelemetryArchiveTesting.jsm",
  this
);

ChromeUtils.defineModuleGetter(
  this,
  "TelemetryHealthPing",
  "resource://gre/modules/HealthPing.jsm"
);

function checkHealthPingStructure(ping, expectedFailuresDict) {
  let payload = ping.payload;
  Assert.equal(
    ping.type,
    TelemetryHealthPing.HEALTH_PING_TYPE,
    "Should have recorded a health ping."
  );

  for (let [key, value] of Object.entries(expectedFailuresDict)) {
    Assert.deepEqual(
      payload[key],
      value,
      "Should have recorded correct entry with key: " + key
    );
  }
}

function fakeHealthSchedulerTimer(set, clear) {
  let telemetryHealthPing = ChromeUtils.import(
    "resource://gre/modules/HealthPing.jsm",
    null
  );
  telemetryHealthPing.Policy.setSchedulerTickTimeout = set;
  telemetryHealthPing.Policy.clearSchedulerTickTimeout = clear;
}

async function waitForConditionWithPromise(
  promiseFn,
  timeoutMsg,
  tryCount = 30
) {
  const SINGLE_TRY_TIMEOUT = 100;
  let tries = 0;
  do {
    try {
      return await promiseFn();
    } catch (ex) {}
    await new Promise(resolve => do_timeout(SINGLE_TRY_TIMEOUT, resolve));
  } while (++tries <= tryCount);
  throw new Error(timeoutMsg);
}

function fakeSendSubmissionTimeout(timeOut) {
  let telemetryHealthPing = ChromeUtils.import(
    "resource://gre/modules/TelemetrySend.jsm",
    null
  );
  telemetryHealthPing.Policy.pingSubmissionTimeout = () => timeOut;
}

add_task(async function setup() {
  // Trigger a proper telemetry init.
  do_get_profile(true);
  // Make sure we don't generate unexpected pings due to pref changes.
  await setEmptyPrefWatchlist();
  Preferences.set(TelemetryUtils.Preferences.HealthPingEnabled, true);

  await TelemetryController.testSetup();
  PingServer.start();
  TelemetrySend.setServer("http://localhost:" + PingServer.port);
  Preferences.set(
    TelemetryUtils.Preferences.Server,
    "http://localhost:" + PingServer.port
  );
});

add_task(async function test_sendImmediately() {
  PingServer.clearRequests();
  TelemetryHealthPing.testReset();

  await TelemetryHealthPing.recordSendFailure("testProblem");
  let ping = await PingServer.promiseNextPing();
  checkHealthPingStructure(ping, {
    [TelemetryHealthPing.FailureType.SEND_FAILURE]: {
      testProblem: 1,
    },
    os: TelemetryHealthPing.OsInfo,
    reason: TelemetryHealthPing.Reason.IMMEDIATE,
  });
});

add_task(async function test_sendOnDelay() {
  PingServer.clearRequests();
  TelemetryHealthPing.testReset();

  // This first failure should immediately trigger a ping. After this, subsequent failures should be throttled.
  await TelemetryHealthPing.recordSendFailure("testFailure");
  let testPing = await PingServer.promiseNextPing();
  Assert.equal(
    testPing.type,
    TelemetryHealthPing.HEALTH_PING_TYPE,
    "Should have recorded a health ping."
  );

  // Retrieve delayed call back.
  let pingSubmissionCallBack = null;
  fakeHealthSchedulerTimer(
    callBack => (pingSubmissionCallBack = callBack),
    () => {}
  );

  // Record two failures, health ping must not be send now.
  await TelemetryHealthPing.recordSendFailure("testFailure");
  await TelemetryHealthPing.recordSendFailure("testFailure");

  // Wait for sending delayed health ping.
  await pingSubmissionCallBack();

  let ping = await PingServer.promiseNextPing();
  checkHealthPingStructure(ping, {
    [TelemetryHealthPing.FailureType.SEND_FAILURE]: {
      testFailure: 2,
    },
    os: TelemetryHealthPing.OsInfo,
    reason: TelemetryHealthPing.Reason.DELAYED,
  });
});

add_task(async function test_sendOverSizedPing() {
  TelemetryHealthPing.testReset();
  PingServer.clearRequests();
  let OVER_SIZED_PING_TYPE = "over-sized-ping";
  let overSizedData = generateRandomString(2 * 1024 * 1024);

  await TelemetryController.submitExternalPing(OVER_SIZED_PING_TYPE, {
    data: overSizedData,
  });
  let ping = await PingServer.promiseNextPing();

  checkHealthPingStructure(ping, {
    [TelemetryHealthPing.FailureType.DISCARDED_FOR_SIZE]: {
      [OVER_SIZED_PING_TYPE]: 1,
    },
    os: TelemetryHealthPing.OsInfo,
    reason: TelemetryHealthPing.Reason.IMMEDIATE,
  });
});

add_task(async function test_healthPingOnTop() {
  PingServer.clearRequests();
  TelemetryHealthPing.testReset();

  let PING_TYPE = "priority-ping";

  // Fake now to be in throttled state.
  let now = fakeNow(2050, 1, 2, 0, 0, 0);
  fakeMidnightPingFuzzingDelay(60 * 1000);

  for (let value of [PING_TYPE, PING_TYPE, "health", PING_TYPE]) {
    TelemetryController.submitExternalPing(value, {});
  }

  // Now trigger sending pings again.
  fakeNow(futureDate(now, 5 * 60 * 1000));
  await TelemetrySend.notifyCanUpload();
  let scheduler = ChromeUtils.import(
    "resource://gre/modules/TelemetrySend.jsm",
    null
  );
  scheduler.SendScheduler.triggerSendingPings(true);

  let pings = await PingServer.promiseNextPings(4);
  Assert.equal(
    pings[0].type,
    "health",
    "Should have received the health ping first."
  );
});

add_task(async function test_sendOnTimeout() {
  TelemetryHealthPing.testReset();
  await TelemetrySend.reset();
  PingServer.clearRequests();
  let PING_TYPE = "ping-on-timeout";

  // Disable send retry to make this test more deterministic.
  fakePingSendTimer(
    () => {},
    () => {}
  );

  // Set up small ping submission timeout to always have timeout error.
  fakeSendSubmissionTimeout(2);

  await TelemetryController.submitExternalPing(PING_TYPE, {});

  let response;
  PingServer.registerPingHandler((req, res) => {
    PingServer.resetPingHandler();
    // We don't finish the response yet to make sure to trigger a timeout.
    res.processAsync();
    response = res;
  });

  // Wait for health ping.
  let ac = new TelemetryArchiveTesting.Checker();
  await ac.promiseInit();
  await waitForConditionWithPromise(() => {
    ac.promiseFindPing("health", []);
  }, "Failed to find health ping");

  if (response) {
    response.finish();
  }

  let telemetryHealthPing = ChromeUtils.import(
    "resource://gre/modules/TelemetrySend.jsm",
    null
  );
  fakeSendSubmissionTimeout(telemetryHealthPing.PING_SUBMIT_TIMEOUT_MS);
  PingServer.resetPingHandler();
  TelemetrySend.notifyCanUpload();

  let pings = await PingServer.promiseNextPings(2);
  let healthPing = pings.find(ping => ping.type === "health");
  checkHealthPingStructure(healthPing, {
    [TelemetryHealthPing.FailureType.SEND_FAILURE]: {
      timeout: 1,
    },
    os: TelemetryHealthPing.OsInfo,
    reason: TelemetryHealthPing.Reason.IMMEDIATE,
  });
  await TelemetryStorage.testClearPendingPings();
});

add_task(async function test_sendOnlyTopTenDiscardedPings() {
  TelemetryHealthPing.testReset();
  await TelemetrySend.reset();
  PingServer.clearRequests();
  let PING_TYPE = "sort-discarded";

  // This first failure should immediately trigger a ping. After this, subsequent failures should be throttled.
  await TelemetryHealthPing.recordSendFailure("testFailure");
  let testPing = await PingServer.promiseNextPing();
  Assert.equal(
    testPing.type,
    TelemetryHealthPing.HEALTH_PING_TYPE,
    "Should have recorded a health ping."
  );

  // Retrieve delayed call back.
  let pingSubmissionCallBack = null;
  fakeHealthSchedulerTimer(
    callBack => (pingSubmissionCallBack = callBack),
    () => {}
  );

  // Add failures
  for (let i = 1; i < 12; i++) {
    for (let j = 1; j < i; j++) {
      TelemetryHealthPing.recordDiscardedPing(PING_TYPE + i);
    }
  }

  await TelemetrySend.reset();
  await pingSubmissionCallBack();
  let ping = await PingServer.promiseNextPing();

  checkHealthPingStructure(ping, {
    os: TelemetryHealthPing.OsInfo,
    reason: TelemetryHealthPing.Reason.DELAYED,
    [TelemetryHealthPing.FailureType.DISCARDED_FOR_SIZE]: {
      [PING_TYPE + 11]: 10,
      [PING_TYPE + 10]: 9,
      [PING_TYPE + 9]: 8,
      [PING_TYPE + 8]: 7,
      [PING_TYPE + 7]: 6,
      [PING_TYPE + 6]: 5,
      [PING_TYPE + 5]: 4,
      [PING_TYPE + 4]: 3,
      [PING_TYPE + 3]: 2,
      [PING_TYPE + 2]: 1,
    },
  });
});

add_task(async function test_discardedForSizePending() {
  TelemetryHealthPing.testReset();
  PingServer.clearRequests();

  const PING_TYPE = "discarded-for-size-pending";

  const OVERSIZED_PING_ID = "9b21ec8f-f762-4d28-a2c1-44e1c4694f24";
  // Create a pending oversized ping.
  let overSizedPayload = generateRandomString(2 * 1024 * 1024);
  const OVERSIZED_PING = {
    id: OVERSIZED_PING_ID,
    type: PING_TYPE,
    creationDate: new Date().toISOString(),
    // Generate a 2MB string to use as the ping payload.
    payload: overSizedPayload,
  };

  // Test loadPendingPing.
  await TelemetryStorage.savePendingPing(OVERSIZED_PING);
  // Try to manually load the oversized ping.
  await Assert.rejects(
    TelemetryStorage.loadPendingPing(OVERSIZED_PING_ID),
    /loadPendingPing - exceeded the maximum ping size/,
    "The oversized ping should have been pruned."
  );

  let ping = await PingServer.promiseNextPing();
  checkHealthPingStructure(ping, {
    [TelemetryHealthPing.FailureType.DISCARDED_FOR_SIZE]: {
      "<unknown>": 1,
    },
    os: TelemetryHealthPing.OsInfo,
    reason: TelemetryHealthPing.Reason.IMMEDIATE,
  });

  // Test _scanPendingPings.
  TelemetryHealthPing.testReset();
  await TelemetryStorage.savePendingPing(OVERSIZED_PING);
  await TelemetryStorage.loadPendingPingList();

  ping = await PingServer.promiseNextPing();
  checkHealthPingStructure(ping, {
    [TelemetryHealthPing.FailureType.DISCARDED_FOR_SIZE]: {
      "<unknown>": 1,
    },
    os: TelemetryHealthPing.OsInfo,
    reason: TelemetryHealthPing.Reason.IMMEDIATE,
  });
});

add_task(async function test_usePingSenderOnShutdown() {
  if (
    gIsAndroid ||
    (AppConstants.platform == "linux" && OS.Constants.Sys.bits == 32)
  ) {
    // We don't support the pingsender on Android, yet, see bug 1335917.
    // We also don't support the pingsender testing on Treeherder for
    // Linux 32 bit (due to missing libraries). So skip it there too.
    // See bug 1310703 comment 78.
    return;
  }

  TelemetryHealthPing.testReset();
  await TelemetrySend.reset();
  PingServer.clearRequests();

  // This first failure should immediately trigger a ping.
  // After this, subsequent failures should be throttled.
  await TelemetryHealthPing.recordSendFailure("testFailure");
  await PingServer.promiseNextPing();

  TelemetryHealthPing.recordSendFailure("testFailure");
  let nextRequest = PingServer.promiseNextRequest();

  await TelemetryController.testReset();
  await TelemetryController.testShutdown();
  let request = await nextRequest;
  let ping = decodeRequestPayload(request);

  checkHealthPingStructure(ping, {
    [TelemetryHealthPing.FailureType.SEND_FAILURE]: {
      testFailure: 1,
    },
    os: TelemetryHealthPing.OsInfo,
    reason: TelemetryHealthPing.Reason.SHUT_DOWN,
  });

  // Check that the health ping is sent at shutdown using the pingsender.
  Assert.equal(
    request.getHeader("User-Agent"),
    "pingsender/1.0",
    "Should have received the correct user agent string."
  );
  Assert.equal(
    request.getHeader("X-PingSender-Version"),
    "1.0",
    "Should have received the correct PingSender version string."
  );
});

add_task(async function cleanup() {
  await PingServer.stop();
});
