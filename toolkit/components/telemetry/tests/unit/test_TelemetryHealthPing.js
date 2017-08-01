/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/
 */

// This tests the public Telemetry API for submitting Health pings.

"use strict";

Cu.import("resource://gre/modules/TelemetryController.jsm", this);
Cu.import("resource://gre/modules/TelemetryStorage.jsm", this);
Cu.import("resource://gre/modules/TelemetryUtils.jsm", this);
Cu.import("resource://gre/modules/Preferences.jsm", this);
Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);

XPCOMUtils.defineLazyModuleGetter(this, "TelemetryHealthPing",
                                  "resource://gre/modules/TelemetryHealthPing.jsm");

function checkHealthPingStructure(ping, expectedFailuresDict) {
  let payload = ping.payload;
  Assert.equal(ping.type, TelemetryHealthPing.HEALTH_PING_TYPE, "Should have recorded a health ping.");

  for (let [key, value] of Object.entries(expectedFailuresDict)) {
    Assert.deepEqual(payload[key], value, "Should have recorded correct entry with key: " + key);
  }
}

function fakeHealthSchedulerTimer(set, clear) {
  let telemetryHealthPing = Cu.import("resource://gre/modules/TelemetryHealthPing.jsm", {});
  telemetryHealthPing.Policy.setSchedulerTickTimeout = set;
  telemetryHealthPing.Policy.clearSchedulerTickTimeout = clear;
}

add_task(async function setup() {
  // Trigger a proper telemetry init.
  do_get_profile(true);
  // Make sure we don't generate unexpected pings due to pref changes.
  await setEmptyPrefWatchlist();
  Services.prefs.setBoolPref("toolkit.telemetry.enabled", true);
  Preferences.set(TelemetryUtils.Preferences.HealthPingEnabled, true);

  await TelemetryController.testSetup();
  PingServer.start();
  TelemetrySend.setServer("http://localhost:" + PingServer.port);
  Preferences.set(TelemetryUtils.Preferences.Server, "http://localhost:" + PingServer.port);
});

add_task(async function test_sendImmediately() {
  PingServer.clearRequests();
  TelemetryHealthPing.testReset();

  await TelemetryHealthPing.recordSendFailure("testProblem");
  let ping = await PingServer.promiseNextPing();
  checkHealthPingStructure(ping, {
    [TelemetryHealthPing.FailureType.SEND_FAILURE]: {
      "testProblem": 1
    },
    "os": TelemetryHealthPing.OsInfo,
    "reason": TelemetryHealthPing.Reason.IMMEDIATE
  });
});

add_task(async function test_sendOnDelay() {
  PingServer.clearRequests();
  TelemetryHealthPing.testReset();

  // This first failure should immediately trigger a ping. After this, subsequent failures should be throttled.
  await TelemetryHealthPing.recordSendFailure("testFailure");
  let testPing = await PingServer.promiseNextPing();
  Assert.equal(testPing.type, TelemetryHealthPing.HEALTH_PING_TYPE, "Should have recorded a health ping.");

  // Retrieve delayed call back.
  let pingSubmissionCallBack = null;
  fakeHealthSchedulerTimer((callBack) => pingSubmissionCallBack = callBack, () => {
  });

  // Record two failures, health ping must not be send now.
  await TelemetryHealthPing.recordSendFailure("testFailure");
  await TelemetryHealthPing.recordSendFailure("testFailure");

  // Wait for sending delayed health ping.
  await pingSubmissionCallBack();

  let ping = await PingServer.promiseNextPing();
  checkHealthPingStructure(ping, {
    [TelemetryHealthPing.FailureType.SEND_FAILURE]: {
      "testFailure": 2
    },
    "os": TelemetryHealthPing.OsInfo,
    "reason": TelemetryHealthPing.Reason.DELAYED
  });
});

add_task(async function test_sendOverSizedPing() {
  TelemetryHealthPing.testReset();
  PingServer.clearRequests();
  let OVER_SIZED_PING_TYPE = "over-sized-ping";
  let overSizedData = generateRandomString(2 * 1024 * 1024);

  await TelemetryController.submitExternalPing(OVER_SIZED_PING_TYPE, {"data": overSizedData});
  let ping = await PingServer.promiseNextPing();

  checkHealthPingStructure(ping, {
    [TelemetryHealthPing.FailureType.DISCARDED_FOR_SIZE]: {
      [OVER_SIZED_PING_TYPE]: 1
    },
    "os": TelemetryHealthPing.OsInfo,
    "reason": TelemetryHealthPing.Reason.IMMEDIATE
  });
});

add_task(async function test_sendOnTimeout() {
  TelemetryHealthPing.testReset();
  PingServer.clearRequests();
  let PING_TYPE = "ping-on-timeout";

  // Set up small ping submission timeout to always have timeout error.
  TelemetrySend.testSetTimeoutForPingSubmit(2);

  // Reset the timeout after receiving the first ping to be able to send health ping.
  PingServer.registerPingHandler((request, result) => {
    PingServer.resetPingHandler();
    TelemetrySend.testResetTimeOutToDefault();
  });

  await TelemetryController.submitExternalPing(PING_TYPE, {});
  let ping = await PingServer.promiseNextPing();
  checkHealthPingStructure(ping, {
    [TelemetryHealthPing.FailureType.SEND_FAILURE]: {
      "timeout": 1
    },
    "os": TelemetryHealthPing.OsInfo,
    "reason": TelemetryHealthPing.Reason.IMMEDIATE
  });

  // Clear pending pings to avoid resending pings which fail with time out error.
  await TelemetryStorage.testClearPendingPings();
});

add_task(async function test_sendOnlyTopTenDiscardedPings() {
  TelemetryHealthPing.testReset();
  PingServer.clearRequests();
  let PING_TYPE = "sort-discarded";

  // This first failure should immediately trigger a ping. After this, subsequent failures should be throttled.
  await TelemetryHealthPing.recordSendFailure("testFailure");
  let testPing = await PingServer.promiseNextPing();
  Assert.equal(testPing.type, TelemetryHealthPing.HEALTH_PING_TYPE, "Should have recorded a health ping.");


  // Retrieve delayed call back.
  let pingSubmissionCallBack = null;
  fakeHealthSchedulerTimer((callBack) => pingSubmissionCallBack = callBack, () => {
  });

  // Add failures
  for (let i = 1; i < 12; i++) {
    for (let j = 1; j < i; j++) {
      await TelemetryHealthPing.recordDiscardedPing(PING_TYPE + i);
    }
  }

  await pingSubmissionCallBack();
  let ping = await PingServer.promiseNextPing();

  checkHealthPingStructure(ping, {
    "os": TelemetryHealthPing.OsInfo,
    "reason": TelemetryHealthPing.Reason.DELAYED,
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
      [PING_TYPE + 2]: 1
    }
  });
});

add_task(async function test_usePingSenderOnShutdown() {
  if (gIsAndroid ||
      (AppConstants.platform == "linux" && OS.Constants.Sys.bits == 32)) {
    // We don't support the pingsender on Android, yet, see bug 1335917.
    // We also don't support the pingsender testing on Treeherder for
    // Linux 32 bit (due to missing libraries). So skip it there too.
    // See bug 1310703 comment 78.
    return;
  }

  TelemetryHealthPing.testReset();
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
      "testFailure": 1
    },
    "os": TelemetryHealthPing.OsInfo,
    "reason": TelemetryHealthPing.Reason.SHUT_DOWN
  });

  // Check that the health ping is sent at shutdown using the pingsender.
  Assert.equal(request.getHeader("User-Agent"), "pingsender/1.0",
    "Should have received the correct user agent string.");
  Assert.equal(request.getHeader("X-PingSender-Version"), "1.0",
    "Should have received the correct PingSender version string.");
});

add_task(async function cleanup() {
  await PingServer.stop();
});
