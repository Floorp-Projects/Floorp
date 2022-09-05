/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/
*/
/* eslint-disable mozilla/no-arbitrary-setTimeout */

// This tests submitting a ping using the stand-alone pingsender program.

"use strict";

const { ClientID } = ChromeUtils.import("resource://gre/modules/ClientID.jsm");
const { PromiseUtils } = ChromeUtils.import(
  "resource://gre/modules/PromiseUtils.jsm"
);
const { TelemetryReportingPolicy } = ChromeUtils.import(
  "resource://gre/modules/TelemetryReportingPolicy.jsm"
);
const { TelemetrySend } = ChromeUtils.import(
  "resource://gre/modules/TelemetrySend.jsm"
);
const { TelemetryStorage } = ChromeUtils.import(
  "resource://gre/modules/TelemetryStorage.jsm"
);
const { TelemetryUtils } = ChromeUtils.import(
  "resource://gre/modules/TelemetryUtils.jsm"
);
const { setTimeout } = ChromeUtils.import("resource://gre/modules/Timer.jsm");

const REASON_SHUTDOWN = "shutdown";

function generateTestPingData() {
  return {
    type: "test-pingsender-type",
    id: TelemetryUtils.generateUUID(),
    creationDate: new Date().toISOString(),
    version: 4,
    payload: {
      dummy: "stuff",
    },
  };
}

function testSendingPings(pingPaths) {
  const url = "http://localhost:" + PingServer.port + "/submit/telemetry/";
  const pings = pingPaths.map(path => {
    return {
      url,
      path,
    };
  });
  TelemetrySend.testRunPingSender(pings, (_, topic, __) => {
    switch (topic) {
      case "process-finished": // finished indicates an exit code of 0
        Assert.ok(true, "Pingsender should be able to post to localhost");
        break;
      case "process-failed": // failed indicates an exit code != 0
        Assert.ok(false, "Pingsender should be able to post to localhost");
        break;
    }
  });
}

/**
 * Wait for a ping file to be deleted from the pending pings directory.
 */
function waitForPingDeletion(pingId) {
  const path = PathUtils.join(TelemetryStorage.pingDirectoryPath, pingId);

  let checkFn = (resolve, reject) =>
    setTimeout(() => {
      IOUtils.exists(path).then(exists => {
        if (!exists) {
          Assert.ok(true, pingId + " was deleted");
          resolve();
        } else {
          checkFn(resolve, reject);
        }
      }, reject);
    }, 250);

  return new Promise((resolve, reject) => checkFn(resolve, reject));
}

add_setup(async () => {
  // Init the profile.
  do_get_profile(true);
  createAppInfo();

  // Make sure we don't generate unexpected pings due to pref changes.
  await setEmptyPrefWatchlist();

  Services.prefs.setBoolPref(TelemetryUtils.Preferences.FhrUploadEnabled, true);

  await new Promise(resolve =>
    Telemetry.asyncFetchTelemetryData(wrapWithExceptionHandler(resolve))
  );
  await TelemetryController.testSetup();

  // Start the ping server and let Telemetry know about it.
  PingServer.start();
  registerCleanupFunction(async () => {
    await PingServer.stop();
  });
  TelemetrySend.setServer("http://localhost:" + PingServer.port);
  Preferences.set(
    TelemetryUtils.Preferences.Server,
    "http://localhost:" + PingServer.port
  );
});

add_task(
  { pref_set: [[TelemetryUtils.Preferences.HealthPingEnabled, true]] },
  async function test_usePingSenderOnShutdown() {
    const { TelemetryHealthPing } = ChromeUtils.import(
      "resource://gre/modules/HealthPing.jsm"
    );
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

    let ping = decodeRequestPayload(request);
    const expectedFailuresDict = {
      [TelemetryHealthPing.FailureType.SEND_FAILURE]: {
        testFailure: 1,
      },
      os: TelemetryHealthPing.OsInfo,
      reason: TelemetryHealthPing.Reason.SHUT_DOWN,
    };

    Assert.equal(
      ping.type,
      TelemetryHealthPing.HEALTH_PING_TYPE,
      "Should have recorded a health ping."
    );

    for (let [key, value] of Object.entries(expectedFailuresDict)) {
      Assert.deepEqual(
        ping.payload[key],
        value,
        "Should have recorded correct entry with key: " + key
      );
    }
  }
);

add_task(async function test_pingSender() {
  // Generate a new ping and save it among the pending pings.
  const data = generateTestPingData();
  await TelemetryStorage.savePing(data, true);

  // Get the local path of the saved ping.
  const pingPath = PathUtils.join(TelemetryStorage.pingDirectoryPath, data.id);

  // Spawn an HTTP server that returns an error. We will be running the
  // PingSender twice, trying to send the ping to this server. After the
  // second time, we will resolve |deferred404Hit|.
  let failingServer = new HttpServer();
  let deferred404Hit = PromiseUtils.defer();
  let hitCount = 0;
  failingServer.registerPathHandler("/lookup_fail", (metadata, response) => {
    response.setStatusLine("1.1", 404, "Not Found");
    hitCount++;

    if (hitCount >= 2) {
      // Resolve the promise on the next tick.
      Services.tm.dispatchToMainThread(() => deferred404Hit.resolve());
    }
  });
  failingServer.start(-1);

  // Try to send the ping twice using the pingsender (we expect 404 both times).
  const errorUrl =
    "http://localhost:" + failingServer.identity.primaryPort + "/lookup_fail";
  TelemetrySend.testRunPingSender([{ url: errorUrl, path: pingPath }]);
  TelemetrySend.testRunPingSender([{ url: errorUrl, path: pingPath }]);

  // Wait until we hit the 404 server twice. After that, make sure that the ping
  // still exists locally.
  await deferred404Hit.promise;
  Assert.ok(
    await IOUtils.exists(pingPath),
    "The pending ping must not be deleted if we fail to send using the PingSender"
  );

  // Try to send it using the pingsender.
  testSendingPings([pingPath]);

  let req = await PingServer.promiseNextRequest();
  let ping = decodeRequestPayload(req);

  Assert.equal(
    req.getHeader("User-Agent"),
    "pingsender/1.0",
    "Should have received the correct user agent string."
  );
  Assert.equal(
    req.getHeader("X-PingSender-Version"),
    "1.0",
    "Should have received the correct PingSender version string."
  );
  Assert.equal(
    req.getHeader("Content-Encoding"),
    "gzip",
    "Should have a gzip encoded ping."
  );
  Assert.ok(req.getHeader("Date"), "Should have received a Date header.");
  Assert.equal(ping.id, data.id, "Should have received the correct ping id.");
  Assert.equal(
    ping.type,
    data.type,
    "Should have received the correct ping type."
  );
  Assert.deepEqual(
    ping.payload,
    data.payload,
    "Should have received the correct payload."
  );

  // Check that the PingSender removed the pending ping.
  await waitForPingDeletion(data.id);

  // Shut down the failing server.
  await new Promise(r => failingServer.stop(r));
});

add_task(async function test_bannedDomains() {
  // Generate a new ping and save it among the pending pings.
  const data = generateTestPingData();
  await TelemetryStorage.savePing(data, true);

  // Get the local path of the saved ping.
  const pingPath = PathUtils.join(TelemetryStorage.pingDirectoryPath, data.id);

  // Confirm we can't send a ping to another destination url
  let bannedUris = [
    "https://example.com",
    "http://localhost.com",
    "http://localHOST.com",
    "http://localhost@example.com",
    "http://localhost:bob@example.com",
    "http://localhost:localhost@localhost.example.com",
  ];
  for (let url of bannedUris) {
    let result = await new Promise(resolve =>
      TelemetrySend.testRunPingSender(
        [{ url, path: pingPath }],
        (_, topic, __) => {
          switch (topic) {
            case "process-finished": // finished indicates an exit code of 0
            case "process-failed": // failed indicates an exit code != 0
              resolve(topic);
          }
        }
      )
    );
    Assert.equal(
      result,
      "process-failed",
      `Pingsender should not be able to post to ${url}`
    );
  }
});

add_task(async function test_pingSender_multiple_pings() {
  // Generate two new pings and save them among the pending pings.
  const data = [generateTestPingData(), generateTestPingData()];

  for (const d of data) {
    await TelemetryStorage.savePing(d, true);
  }

  // Get the local path of the saved pings.
  const pingPaths = data.map(d =>
    PathUtils.join(TelemetryStorage.pingDirectoryPath, d.id)
  );

  // Try to send them using the pingsender.
  testSendingPings(pingPaths);

  // Check the pings. We don't have an ordering guarantee, so we move the
  // elements to a new array when we find them.
  let data2 = [];
  while (data.length) {
    let req = await PingServer.promiseNextRequest();
    let ping = decodeRequestPayload(req);
    let idx = data.findIndex(d => d.id == ping.id);
    Assert.ok(
      idx >= 0,
      `Should have received the correct ping id: ${data[idx].id}`
    );
    data2.push(data[idx]);
    data.splice(idx, 1);
  }

  // Check that the PingSender removed the pending pings.
  for (const d of data2) {
    await waitForPingDeletion(d.id);
  }
});

add_task(
  {
    pref_set: [
      [TelemetryUtils.Preferences.NewProfilePingEnabled, true],
      [TelemetryUtils.Preferences.NewProfilePingDelay, 1],
    ],
  },
  async function test_sendNewProfile() {
    const NEWPROFILE_PING_TYPE = "new-profile";

    // Make sure Telemetry is shut down before beginning and that we have
    // no pending pings.
    let resetTest = async function() {
      await TelemetryController.testShutdown();
      await TelemetryStorage.testClearPendingPings();
      PingServer.clearRequests();
    };
    await resetTest();

    // Make sure to reset all the new-profile ping prefs.
    const stateFilePath = PathUtils.join(
      PathUtils.profileDir,
      "datareporting",
      "session-state.json"
    );
    await IOUtils.remove(stateFilePath);

    // Check that a new-profile ping is sent on the first session.
    let nextReq = PingServer.promiseNextRequest();
    await TelemetryController.testReset();
    let req = await nextReq;
    let ping = decodeRequestPayload(req);
    checkPingFormat(ping, NEWPROFILE_PING_TYPE, true, true);
    Assert.equal(
      ping.payload.reason,
      "startup",
      "The new-profile ping generated after startup must have the correct reason"
    );
    Assert.ok(
      "parent" in ping.payload.processes,
      "The new-profile ping generated after startup must have processes.parent data"
    );

    // Check that is not sent with the pingsender during startup.
    Assert.throws(
      () => req.getHeader("X-PingSender-Version"),
      /NS_ERROR_NOT_AVAILABLE/,
      "Should not have used the pingsender."
    );

    // Make sure that the new-profile ping is sent at shutdown if it wasn't sent before.
    await resetTest();
    await IOUtils.remove(stateFilePath);
    Preferences.reset(TelemetryUtils.Preferences.NewProfilePingDelay);

    nextReq = PingServer.promiseNextRequest();
    await TelemetryController.testReset();
    await TelemetryController.testShutdown();
    req = await nextReq;
    ping = decodeRequestPayload(req);
    checkPingFormat(ping, NEWPROFILE_PING_TYPE, true, true);
    Assert.equal(
      ping.payload.reason,
      "shutdown",
      "The new-profile ping generated at shutdown must have the correct reason"
    );
    Assert.ok(
      "parent" in ping.payload.processes,
      "The new-profile ping generated at shutdown must have processes.parent data"
    );

    // Check that the new-profile ping is sent at shutdown using the pingsender.
    Assert.equal(
      req.getHeader("User-Agent"),
      "pingsender/1.0",
      "Should have received the correct user agent string."
    );
    Assert.equal(
      req.getHeader("X-PingSender-Version"),
      "1.0",
      "Should have received the correct PingSender version string."
    );

    // Check that no new-profile ping is sent on second sessions, not at startup
    // nor at shutdown.
    await resetTest();
    PingServer.registerPingHandler(() =>
      Assert.ok(
        false,
        "The new-profile ping must be sent only on new profiles."
      )
    );
    await TelemetryController.testReset();
    await TelemetryController.testShutdown();

    // Check that we don't send the new-profile ping if the profile already contains
    // a state file (but no "newProfilePingSent" property).
    await resetTest();
    await IOUtils.remove(stateFilePath);
    const sessionState = {
      sessionId: null,
      subsessionId: null,
      profileSubsessionCounter: 3785,
    };
    await IOUtils.writeJSON(stateFilePath, sessionState);
    await TelemetryController.testReset();
    await TelemetryController.testShutdown();

    // Restart Telemetry.
    PingServer.resetPingHandler();
  }
);

add_task(
  {
    pref_set: [
      [TelemetryUtils.Preferences.ShutdownPingSender, true],
      [TelemetryUtils.Preferences.ShutdownPingSenderFirstSession, false],
      [TelemetryUtils.Preferences.BypassNotification, true],
      [TelemetryUtils.Preferences.FirstRun, false],
    ],
  },
  async function test_sendShutdownPing() {
    let checkPendingShutdownPing = async function() {
      let pendingPings = await TelemetryStorage.loadPendingPingList();
      Assert.equal(
        pendingPings.length,
        1,
        "We expect 1 pending ping: shutdown."
      );
      // Load the pings off the disk.
      const shutdownPing = await TelemetryStorage.loadPendingPing(
        pendingPings[0].id
      );
      Assert.ok(shutdownPing, "The 'shutdown' ping must be saved to disk.");
      Assert.equal(
        "shutdown",
        shutdownPing.payload.info.reason,
        "The 'shutdown' ping must be saved to disk."
      );
    };

    let clientID = await ClientID.getClientID();
    await TelemetryController.testReset();

    // Make sure the reporting policy picks up the updated pref.
    TelemetryReportingPolicy.testUpdateFirstRun();
    PingServer.clearRequests();
    Telemetry.clearScalars();

    // Shutdown telemetry and wait for an incoming ping.
    let nextPing = PingServer.promiseNextPing();
    await TelemetryController.testShutdown();
    let ping = await nextPing;

    // Check that we received a shutdown ping.
    checkPingFormat(ping, ping.type, true, true);
    Assert.equal(ping.payload.info.reason, REASON_SHUTDOWN);
    Assert.equal(ping.clientId, clientID);
    // Try again, this time disable ping upload. The PingSender
    // should not be sending any ping!
    PingServer.registerPingHandler(() =>
      Assert.ok(false, "Telemetry must not send pings if not allowed to.")
    );
    Preferences.set(TelemetryUtils.Preferences.FhrUploadEnabled, false);
    await TelemetryController.testReset();
    await TelemetryController.testShutdown();

    // Make sure we have no pending pings between the runs.
    await TelemetryStorage.testClearPendingPings();

    // Enable ping upload and signal an OS shutdown. The pingsender
    // will not be spawned and no ping will be sent.
    Preferences.set(TelemetryUtils.Preferences.FhrUploadEnabled, true);
    await TelemetryController.testReset();
    Services.obs.notifyObservers(null, "quit-application-forced");
    await TelemetryController.testShutdown();
    // After re-enabling FHR, wait for the new client ID
    clientID = await ClientID.getClientID();

    // Check that the "shutdown" ping was correctly saved to disk.
    await checkPendingShutdownPing();

    // Make sure we have no pending pings between the runs.
    await TelemetryStorage.testClearPendingPings();
    Telemetry.clearScalars();

    await TelemetryController.testReset();
    Services.obs.notifyObservers(
      null,
      "quit-application-granted",
      "syncShutdown"
    );
    await TelemetryController.testShutdown();
    await checkPendingShutdownPing();

    // Make sure we have no pending pings between the runs.
    await TelemetryStorage.testClearPendingPings();

    // Disable the "submission policy". The shutdown ping must not be sent.
    Preferences.set(TelemetryUtils.Preferences.BypassNotification, false);
    await TelemetryController.testReset();
    await TelemetryController.testShutdown();

    // Make sure we have no pending pings between the runs.
    await TelemetryStorage.testClearPendingPings();

    // With both upload enabled and the policy shown, make sure we don't
    // send the shutdown ping using the pingsender on the first
    // subsession.
    Preferences.set(TelemetryUtils.Preferences.BypassNotification, true);
    Preferences.set(TelemetryUtils.Preferences.FirstRun, true);
    // Make sure the reporting policy picks up the updated pref.
    TelemetryReportingPolicy.testUpdateFirstRun();

    await TelemetryController.testReset();
    await TelemetryController.testShutdown();

    // Clear the state and prepare for the next test.
    await TelemetryStorage.testClearPendingPings();
    PingServer.clearRequests();
    PingServer.resetPingHandler();

    // Check that we're able to send the shutdown ping using the pingsender
    // from the first session if the related pref is on.
    Preferences.set(
      TelemetryUtils.Preferences.ShutdownPingSenderFirstSession,
      true
    );
    Preferences.set(TelemetryUtils.Preferences.FirstRun, true);
    TelemetryReportingPolicy.testUpdateFirstRun();

    // Restart/shutdown telemetry and wait for an incoming ping.
    await TelemetryController.testReset();
    await TelemetryController.testShutdown();
    ping = await PingServer.promiseNextPing();

    // Check that we received a shutdown ping.
    checkPingFormat(ping, ping.type, true, true);
    Assert.equal(ping.payload.info.reason, REASON_SHUTDOWN);
    Assert.equal(ping.clientId, clientID);

    // Restart Telemetry.
    PingServer.resetPingHandler();
  }
);

add_task(
  {
    pref_set: [
      [TelemetryUtils.Preferences.ShutdownPingSender, true],
      [TelemetryUtils.Preferences.ShutdownPingSenderFirstSession, false],
      [TelemetryUtils.Preferences.FirstShutdownPingEnabled, true],
      [TelemetryUtils.Preferences.FirstRun, true],
    ],
  },
  async function test_sendFirstShutdownPing() {
    let storageContainsFirstShutdown = async function() {
      let pendingPings = await TelemetryStorage.loadPendingPingList();
      let pings = await Promise.all(
        pendingPings.map(async p => {
          return TelemetryStorage.loadPendingPing(p.id);
        })
      );
      return pings.find(p => p.type == "first-shutdown");
    };

    let checkShutdownNotSent = async function() {
      // The failure-mode of the ping-sender is used to check that a ping was
      // *not* sent. This can be combined with the state of the storage to infer
      // the appropriate behavior from the preference flags.

      // Assert failure if we recive a ping.
      PingServer.registerPingHandler((req, res) => {
        const receivedPing = decodeRequestPayload(req);
        Assert.ok(
          false,
          `No ping should be received in this test (got ${receivedPing.id}).`
        );
      });

      // Assert that pings are sent on first run, forcing a forced application
      // quit. This should be equivalent to the first test in this suite.
      Preferences.set(TelemetryUtils.Preferences.FirstRun, true);
      TelemetryReportingPolicy.testUpdateFirstRun();

      await TelemetryController.testReset();
      Services.obs.notifyObservers(null, "quit-application-forced");
      await TelemetryController.testShutdown();
      Assert.ok(
        await storageContainsFirstShutdown(),
        "The 'first-shutdown' ping must be saved to disk."
      );

      await TelemetryStorage.testClearPendingPings();

      // Assert that it's not sent during subsequent runs
      Preferences.set(TelemetryUtils.Preferences.FirstRun, false);
      TelemetryReportingPolicy.testUpdateFirstRun();

      await TelemetryController.testReset();
      Services.obs.notifyObservers(null, "quit-application-forced");
      await TelemetryController.testShutdown();
      Assert.ok(
        !(await storageContainsFirstShutdown()),
        "The 'first-shutdown' ping should only be written during first run."
      );

      await TelemetryStorage.testClearPendingPings();

      // Assert that the the ping is only sent if the flag is enabled.
      Preferences.set(TelemetryUtils.Preferences.FirstRun, true);
      Preferences.set(
        TelemetryUtils.Preferences.FirstShutdownPingEnabled,
        false
      );
      TelemetryReportingPolicy.testUpdateFirstRun();

      await TelemetryController.testReset();
      await TelemetryController.testShutdown();
      Assert.ok(
        !(await storageContainsFirstShutdown()),
        "The 'first-shutdown' ping should only be written if enabled"
      );

      await TelemetryStorage.testClearPendingPings();

      // Assert that the the ping is not collected when the ping-sender is disabled.
      // The information would be made irrelevant by the main-ping in the second session.
      Preferences.set(
        TelemetryUtils.Preferences.FirstShutdownPingEnabled,
        true
      );
      Preferences.set(TelemetryUtils.Preferences.ShutdownPingSender, false);
      TelemetryReportingPolicy.testUpdateFirstRun();

      await TelemetryController.testReset();
      await TelemetryController.testShutdown();
      Assert.ok(
        !(await storageContainsFirstShutdown()),
        "The 'first-shutdown' ping should only be written if ping-sender is enabled"
      );

      // Clear the state and prepare for the next test.
      await TelemetryStorage.testClearPendingPings();
      PingServer.clearRequests();
      PingServer.resetPingHandler();
    };

    // Remove leftover pending pings from other tests
    await TelemetryStorage.testClearPendingPings();
    PingServer.clearRequests();
    Telemetry.clearScalars();

    // Set primary conditions of the 'first-shutdown' ping
    let clientID = await ClientID.getClientID();
    TelemetryReportingPolicy.testUpdateFirstRun();

    // Assert general 'first-shutdown' use-case.
    await TelemetryController.testReset();
    await TelemetryController.testShutdown();
    let ping = await PingServer.promiseNextPing();
    checkPingFormat(ping, "first-shutdown", true, true);
    Assert.equal(ping.payload.info.reason, REASON_SHUTDOWN);
    Assert.equal(ping.clientId, clientID);

    await TelemetryStorage.testClearPendingPings();

    // Assert that the shutdown is not sent under various conditions
    await checkShutdownNotSent();

    // Restart Telemetry.
    PingServer.resetPingHandler();
  }
);
