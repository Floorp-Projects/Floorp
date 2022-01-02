/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/
*/
/* This testcase triggers two telemetry pings.
 *
 * Telemetry code keeps histograms of past telemetry pings. The first
 * ping populates these histograms. One of those histograms is then
 * checked in the second request.
 */

const { CommonUtils } = ChromeUtils.import(
  "resource://services-common/utils.js"
);
const { ClientID } = ChromeUtils.import("resource://gre/modules/ClientID.jsm");
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { TelemetryController } = ChromeUtils.import(
  "resource://gre/modules/TelemetryController.jsm"
);
const { TelemetryStorage } = ChromeUtils.import(
  "resource://gre/modules/TelemetryStorage.jsm"
);
const { TelemetrySend } = ChromeUtils.import(
  "resource://gre/modules/TelemetrySend.jsm"
);
const { TelemetryArchive } = ChromeUtils.import(
  "resource://gre/modules/TelemetryArchive.jsm"
);
const { TelemetryUtils } = ChromeUtils.import(
  "resource://gre/modules/TelemetryUtils.jsm"
);
const { Preferences } = ChromeUtils.import(
  "resource://gre/modules/Preferences.jsm"
);
const { ContentTaskUtils } = ChromeUtils.import(
  "resource://testing-common/ContentTaskUtils.jsm"
);
const { TestUtils } = ChromeUtils.import(
  "resource://testing-common/TestUtils.jsm"
);
const { TelemetryArchiveTesting } = ChromeUtils.import(
  "resource://testing-common/TelemetryArchiveTesting.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "jwcrypto",
  "resource://services-crypto/jwcrypto.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "JsonSchemaValidator",
  "resource://gre/modules/components-utils/JsonSchemaValidator.jsm"
);

const PING_FORMAT_VERSION = 4;
const DELETION_REQUEST_PING_TYPE = "deletion-request";
const TEST_PING_TYPE = "test-ping-type";

var gClientID = null;

XPCOMUtils.defineLazyGetter(this, "DATAREPORTING_PATH", async function() {
  let profileDir = await PathUtils.getProfileDir();
  return PathUtils.join(profileDir, "datareporting");
});

function sendPing(aSendClientId, aSendEnvironment) {
  if (PingServer.started) {
    TelemetrySend.setServer("http://localhost:" + PingServer.port);
  } else {
    TelemetrySend.setServer("http://doesnotexist");
  }

  let options = {
    addClientId: aSendClientId,
    addEnvironment: aSendEnvironment,
  };
  return TelemetryController.submitExternalPing(TEST_PING_TYPE, {}, options);
}

function checkPingFormat(aPing, aType, aHasClientId, aHasEnvironment) {
  const MANDATORY_PING_FIELDS = [
    "type",
    "id",
    "creationDate",
    "version",
    "application",
    "payload",
  ];

  const APPLICATION_TEST_DATA = {
    buildId: gAppInfo.appBuildID,
    name: APP_NAME,
    version: APP_VERSION,
    displayVersion: AppConstants.MOZ_APP_VERSION_DISPLAY,
    vendor: "Mozilla",
    platformVersion: PLATFORM_VERSION,
    xpcomAbi: "noarch-spidermonkey",
  };

  // Check that the ping contains all the mandatory fields.
  for (let f of MANDATORY_PING_FIELDS) {
    Assert.ok(f in aPing, f + " must be available.");
  }

  Assert.equal(aPing.type, aType, "The ping must have the correct type.");
  Assert.equal(
    aPing.version,
    PING_FORMAT_VERSION,
    "The ping must have the correct version."
  );

  // Test the application section.
  for (let f in APPLICATION_TEST_DATA) {
    Assert.equal(
      aPing.application[f],
      APPLICATION_TEST_DATA[f],
      f + " must have the correct value."
    );
  }

  // We can't check the values for channel and architecture. Just make
  // sure they are in.
  Assert.ok(
    "architecture" in aPing.application,
    "The application section must have an architecture field."
  );
  Assert.ok(
    "channel" in aPing.application,
    "The application section must have a channel field."
  );

  // Check the clientId and environment fields, as needed.
  Assert.equal("clientId" in aPing, aHasClientId);
  Assert.equal("environment" in aPing, aHasEnvironment);
}

add_task(async function test_setup() {
  // Addon manager needs a profile directory
  do_get_profile();
  await loadAddonManager(
    "xpcshell@tests.mozilla.org",
    "XPCShell",
    "1",
    "1.9.2"
  );
  finishAddonManagerStartup();
  fakeIntlReady();
  // Make sure we don't generate unexpected pings due to pref changes.
  await setEmptyPrefWatchlist();

  Services.prefs.setBoolPref(TelemetryUtils.Preferences.FhrUploadEnabled, true);

  await new Promise(resolve =>
    Telemetry.asyncFetchTelemetryData(wrapWithExceptionHandler(resolve))
  );
});

add_task(async function asyncSetup() {
  await TelemetryController.testSetup();
});

// Ensure that not overwriting an existing file fails silently
add_task(async function test_overwritePing() {
  let ping = { id: "foo" };
  await TelemetryStorage.savePing(ping, true);
  await TelemetryStorage.savePing(ping, false);
  await TelemetryStorage.cleanupPingFile(ping);
});

// Checks that a sent ping is correctly received by a dummy http server.
add_task(async function test_simplePing() {
  PingServer.start();
  // Update the Telemetry Server preference with the address of the local server.
  // Otherwise we might end up sending stuff to a non-existing server after
  // |TelemetryController.testReset| is called.
  Preferences.set(
    TelemetryUtils.Preferences.Server,
    "http://localhost:" + PingServer.port
  );

  await sendPing(false, false);
  let request = await PingServer.promiseNextRequest();

  let ping = decodeRequestPayload(request);
  checkPingFormat(ping, TEST_PING_TYPE, false, false);
});

add_task(async function test_disableDataUpload() {
  const OPTIN_PROBE = "telemetry.data_upload_optin";
  const isUnified = Preferences.get(TelemetryUtils.Preferences.Unified, false);
  if (!isUnified) {
    // Skipping the test if unified telemetry is off, as no deletion-request ping will be generated.
    return;
  }

  // Check that the optin probe is not set.
  // (If there are no recorded scalars, "parent" will be undefined).
  let snapshot = Telemetry.getSnapshotForScalars("main", false).parent || {};
  Assert.ok(
    !(OPTIN_PROBE in snapshot),
    "Data optin scalar should not be set at start"
  );

  // Send a first ping to get the current used client id
  await sendPing(true, false);
  let ping = await PingServer.promiseNextPing();
  checkPingFormat(ping, TEST_PING_TYPE, true, false);
  let firstClientId = ping.clientId;

  Assert.ok(firstClientId, "Test ping needs a client ID");
  Assert.notEqual(
    TelemetryUtils.knownClientID,
    firstClientId,
    "Client ID should be valid and random"
  );

  // The next step should trigger an event, watch for it.
  let disableObserved = TestUtils.topicObserved(
    TelemetryUtils.TELEMETRY_UPLOAD_DISABLED_TOPIC
  );

  // Disable FHR upload: this should trigger a deletion-request ping.
  Preferences.set(TelemetryUtils.Preferences.FhrUploadEnabled, false);

  // Wait for the disable event
  await disableObserved;

  ping = await PingServer.promiseNextPing();
  checkPingFormat(ping, DELETION_REQUEST_PING_TYPE, true, false);
  // Wait on ping activity to settle.
  await TelemetrySend.testWaitOnOutgoingPings();

  snapshot = Telemetry.getSnapshotForScalars("main", false).parent || {};
  Assert.ok(
    !(OPTIN_PROBE in snapshot),
    "Data optin scalar should not be set after opt out"
  );

  // Restore FHR Upload.
  Preferences.set(TelemetryUtils.Preferences.FhrUploadEnabled, true);

  // We need to wait until the scalar is set
  await ContentTaskUtils.waitForCondition(() => {
    const scalarSnapshot = Telemetry.getSnapshotForScalars("main", false);
    return (
      Object.keys(scalarSnapshot).includes("parent") &&
      OPTIN_PROBE in scalarSnapshot.parent
    );
  });

  snapshot = Telemetry.getSnapshotForScalars("main", false).parent || {};
  Assert.ok(
    snapshot[OPTIN_PROBE],
    "Enabling data upload should set optin probe"
  );

  // The clientId should've been reset when we restored FHR Upload.
  let secondClientId = TelemetryController.getCurrentPingData().clientId;
  Assert.notEqual(
    firstClientId,
    secondClientId,
    "The client id must have changed"
  );
  // Simulate a failure in sending the deletion-request ping by disabling the HTTP server.
  await PingServer.stop();

  // Try to send a ping. It will be saved as pending  and get deleted when disabling upload.
  TelemetryController.submitExternalPing(TEST_PING_TYPE, {});

  // Disable FHR upload to send a deletion-request ping again.
  Preferences.set(TelemetryUtils.Preferences.FhrUploadEnabled, false);
  // Wait for the deletion-request ping to be submitted.
  await TelemetryController.testPromiseDeletionRequestPingSubmitted();

  // Wait on sending activity to settle, as |TelemetryController.testReset()| doesn't do that.
  await TelemetrySend.testWaitOnOutgoingPings();
  // Wait for the pending pings to be deleted. Resetting TelemetryController doesn't
  // trigger the shutdown, so we need to call it ourselves.
  await TelemetryStorage.shutdown();
  // Simulate a restart, and spin the send task.
  await TelemetryController.testReset();

  // Disabling Telemetry upload must clear out all the pending pings.
  let pendingPings = await TelemetryStorage.loadPendingPingList();
  Assert.equal(
    pendingPings.length,
    1,
    "All the pending pings should have been deleted, except the deletion-request ping"
  );

  // Enable the ping server again.
  PingServer.start();
  // We set the new server using the pref, otherwise it would get reset with
  // |TelemetryController.testReset|.
  Preferences.set(
    TelemetryUtils.Preferences.Server,
    "http://localhost:" + PingServer.port
  );

  // Stop the sending task and then start it again.
  await TelemetrySend.shutdown();
  // Reset the controller to spin the ping sending task.
  await TelemetryController.testReset();

  // Re-enable Telemetry
  Preferences.set(TelemetryUtils.Preferences.FhrUploadEnabled, true);

  // Send a test ping
  await sendPing(true, false);

  // We should have received the test ping first.
  ping = await PingServer.promiseNextPing();
  checkPingFormat(ping, TEST_PING_TYPE, true, false);

  // The data in the test ping should be different than before
  Assert.notEqual(
    TelemetryUtils.knownClientID,
    ping.clientId,
    "Client ID should be reset to a random value"
  );
  Assert.notEqual(
    firstClientId,
    ping.clientId,
    "Client ID should be different from the previous value"
  );

  // The "deletion-request" ping should come next, as it was pending.
  ping = await PingServer.promiseNextPing();
  checkPingFormat(ping, DELETION_REQUEST_PING_TYPE, true, false);
  Assert.equal(
    secondClientId,
    ping.clientId,
    "Deletion must be requested for correct client id"
  );

  // Wait on ping activity to settle before moving on to the next test. If we were
  // to shut down telemetry, even though the PingServer caught the expected pings,
  // TelemetrySend could still be processing them (clearing pings would happen in
  // a couple of ticks). Shutting down would cancel the request and save them as
  // pending pings.
  await TelemetrySend.testWaitOnOutgoingPings();
});

add_task(async function test_pingHasClientId() {
  // Make sure we have no cached client ID for this test: we'll try to send
  // a ping with it while Telemetry is being initialized.
  Preferences.reset(TelemetryUtils.Preferences.CachedClientId);
  await TelemetryController.testShutdown();
  await ClientID._reset();
  await TelemetryStorage.testClearPendingPings();
  // And also clear the counter histogram since we're here.
  let h = Telemetry.getHistogramById(
    "TELEMETRY_PING_SUBMISSION_WAITING_CLIENTID"
  );
  h.clear();

  // Init telemetry and try to send a ping with a client ID.
  let promisePingSetup = TelemetryController.testReset();
  await sendPing(true, false);
  Assert.equal(
    h.snapshot().sum,
    1,
    "We must have a ping waiting for the clientId early during startup."
  );
  // Wait until we are fully initialized. Pings will be assembled but won't get
  // sent before then.
  await promisePingSetup;

  let ping = await PingServer.promiseNextPing();
  // Fetch the client ID after initializing and fetching the the ping, so we
  // don't unintentionally trigger its loading. We'll still need the client ID
  // to see if the ping looks sane.
  gClientID = await ClientID.getClientID();

  checkPingFormat(ping, TEST_PING_TYPE, true, false);
  Assert.equal(
    ping.clientId,
    gClientID,
    "The correct clientId must be reported."
  );

  // Shutdown Telemetry so we can safely restart it.
  await TelemetryController.testShutdown();
  await TelemetryStorage.testClearPendingPings();

  // We should have cached the client ID now. Lets confirm that by checking it before
  // the async ping setup is finished.
  h.clear();
  promisePingSetup = TelemetryController.testReset();
  await sendPing(true, false);
  await promisePingSetup;

  // Check that we received the cached client id.
  Assert.equal(h.snapshot().sum, 0, "We must have used the cached clientId.");
  ping = await PingServer.promiseNextPing();
  checkPingFormat(ping, TEST_PING_TYPE, true, false);
  Assert.equal(
    ping.clientId,
    gClientID,
    "Telemetry should report the correct cached clientId."
  );

  // Check that sending a ping without relying on the cache, after the
  // initialization, still works.
  Preferences.reset(TelemetryUtils.Preferences.CachedClientId);
  await TelemetryController.testShutdown();
  await TelemetryStorage.testClearPendingPings();
  await TelemetryController.testReset();
  await sendPing(true, false);
  ping = await PingServer.promiseNextPing();
  checkPingFormat(ping, TEST_PING_TYPE, true, false);
  Assert.equal(
    ping.clientId,
    gClientID,
    "The correct clientId must be reported."
  );
  Assert.equal(
    h.snapshot().sum,
    0,
    "No ping should have been waiting for a clientId."
  );
});

add_task(async function test_pingHasEnvironment() {
  // Send a ping with the environment data.
  await sendPing(false, true);
  let ping = await PingServer.promiseNextPing();
  checkPingFormat(ping, TEST_PING_TYPE, false, true);

  // Test a field in the environment build section.
  Assert.equal(ping.application.buildId, ping.environment.build.buildId);
});

add_task(async function test_pingHasEnvironmentAndClientId() {
  // Send a ping with the environment data and client id.
  await sendPing(true, true);
  let ping = await PingServer.promiseNextPing();
  checkPingFormat(ping, TEST_PING_TYPE, true, true);

  // Test a field in the environment build section.
  Assert.equal(ping.application.buildId, ping.environment.build.buildId);
  // Test that we have the correct clientId.
  Assert.equal(
    ping.clientId,
    gClientID,
    "The correct clientId must be reported."
  );
});

add_task(async function test_archivePings() {
  let now = new Date(2009, 10, 18, 12, 0, 0);
  fakeNow(now);

  // Disable ping upload so that pings don't get sent.
  // With unified telemetry the FHR upload pref controls this,
  // with non-unified telemetry the Telemetry enabled pref.
  const isUnified = Preferences.get(TelemetryUtils.Preferences.Unified, false);
  const uploadPref = isUnified
    ? TelemetryUtils.Preferences.FhrUploadEnabled
    : TelemetryUtils.Preferences.TelemetryEnabled;
  Preferences.set(uploadPref, false);

  // If we're using unified telemetry, disabling ping upload will generate a "deletion-request" ping. Catch it.
  if (isUnified) {
    let ping = await PingServer.promiseNextPing();
    checkPingFormat(ping, DELETION_REQUEST_PING_TYPE, true, false);
  }

  // Register a new Ping Handler that asserts if a ping is received, then send a ping.
  PingServer.registerPingHandler(() =>
    Assert.ok(false, "Telemetry must not send pings if not allowed to.")
  );
  let pingId = await sendPing(true, true);

  // Check that the ping was archived, even with upload disabled.
  let ping = await TelemetryArchive.promiseArchivedPingById(pingId);
  Assert.equal(
    ping.id,
    pingId,
    "TelemetryController should still archive pings."
  );

  // Check that pings don't get archived if not allowed to.
  now = new Date(2010, 10, 18, 12, 0, 0);
  fakeNow(now);
  Preferences.set(TelemetryUtils.Preferences.ArchiveEnabled, false);
  pingId = await sendPing(true, true);
  let promise = TelemetryArchive.promiseArchivedPingById(pingId);
  Assert.ok(
    await promiseRejects(promise),
    "TelemetryController should not archive pings if the archive pref is disabled."
  );

  // Enable archiving and the upload so that pings get sent and archived again.
  Preferences.set(uploadPref, true);
  Preferences.set(TelemetryUtils.Preferences.ArchiveEnabled, true);

  now = new Date(2014, 6, 18, 22, 0, 0);
  fakeNow(now);
  // Restore the non asserting ping handler.
  PingServer.resetPingHandler();
  pingId = await sendPing(true, true);

  // Check that we archive pings when successfully sending them.
  await PingServer.promiseNextPing();
  ping = await TelemetryArchive.promiseArchivedPingById(pingId);
  Assert.equal(
    ping.id,
    pingId,
    "TelemetryController should still archive pings if ping upload is enabled."
  );
});

// Test that we fuzz the submission time around midnight properly
// to avoid overloading the telemetry servers.
add_task(async function test_midnightPingSendFuzzing() {
  const fuzzingDelay = 60 * 60 * 1000;
  fakeMidnightPingFuzzingDelay(fuzzingDelay);
  let now = new Date(2030, 5, 1, 11, 0, 0);
  fakeNow(now);

  let waitForTimer = () =>
    new Promise(resolve => {
      fakePingSendTimer(
        (callback, timeout) => {
          resolve([callback, timeout]);
        },
        () => {}
      );
    });

  PingServer.clearRequests();
  await TelemetryController.testReset();

  // A ping after midnight within the fuzzing delay should not get sent.
  now = new Date(2030, 5, 2, 0, 40, 0);
  fakeNow(now);
  PingServer.registerPingHandler((req, res) => {
    Assert.ok(false, "No ping should be received yet.");
  });
  let timerPromise = waitForTimer();
  await sendPing(true, true);
  let [timerCallback, timerTimeout] = await timerPromise;
  Assert.ok(!!timerCallback);
  Assert.deepEqual(
    futureDate(now, timerTimeout),
    new Date(2030, 5, 2, 1, 0, 0)
  );

  // A ping just before the end of the fuzzing delay should not get sent.
  now = new Date(2030, 5, 2, 0, 59, 59);
  fakeNow(now);
  timerPromise = waitForTimer();
  await sendPing(true, true);
  [timerCallback, timerTimeout] = await timerPromise;
  Assert.deepEqual(timerTimeout, 1 * 1000);

  // Restore the previous ping handler.
  PingServer.resetPingHandler();

  // Setting the clock to after the fuzzing delay, we should trigger the two ping sends
  // with the timer callback.
  now = futureDate(now, timerTimeout);
  fakeNow(now);
  await timerCallback();
  const pings = await PingServer.promiseNextPings(2);
  for (let ping of pings) {
    checkPingFormat(ping, TEST_PING_TYPE, true, true);
  }
  await TelemetrySend.testWaitOnOutgoingPings();

  // Moving the clock further we should still send pings immediately.
  now = futureDate(now, 5 * 60 * 1000);
  await sendPing(true, true);
  let ping = await PingServer.promiseNextPing();
  checkPingFormat(ping, TEST_PING_TYPE, true, true);
  await TelemetrySend.testWaitOnOutgoingPings();

  // Check that pings shortly before midnight are immediately sent.
  now = fakeNow(2030, 5, 3, 23, 59, 0);
  await sendPing(true, true);
  ping = await PingServer.promiseNextPing();
  checkPingFormat(ping, TEST_PING_TYPE, true, true);
  await TelemetrySend.testWaitOnOutgoingPings();

  // Clean-up.
  fakeMidnightPingFuzzingDelay(0);
  fakePingSendTimer(
    () => {},
    () => {}
  );
});

add_task(async function test_changePingAfterSubmission() {
  // Submit a ping with a custom payload.
  let payload = { canary: "test" };
  let pingPromise = TelemetryController.submitExternalPing(
    TEST_PING_TYPE,
    payload
  );

  // Change the payload with a predefined value.
  payload.canary = "changed";

  // Wait for the ping to be archived.
  const pingId = await pingPromise;

  // Make sure our changes didn't affect the submitted payload.
  let archivedCopy = await TelemetryArchive.promiseArchivedPingById(pingId);
  Assert.equal(
    archivedCopy.payload.canary,
    "test",
    "The payload must not be changed after being submitted."
  );
});

add_task(
  {
    skip_if: () =>
      Services.prefs.getBoolPref(TelemetryUtils.Preferences.Unified, false),
  },
  async function test_telemetryEnabledUnexpectedValue() {
    // Remove the default value for toolkit.telemetry.enabled from the default prefs.
    // Otherwise, we wouldn't be able to set the pref to a string.
    let defaultPrefBranch = Services.prefs.getDefaultBranch(null);
    defaultPrefBranch.deleteBranch(TelemetryUtils.Preferences.TelemetryEnabled);

    // Set the preferences controlling the Telemetry status to a string.
    Preferences.set(TelemetryUtils.Preferences.TelemetryEnabled, "false");
    // Check that Telemetry is not enabled.
    await TelemetryController.testReset();
    Assert.equal(
      Telemetry.canRecordExtended,
      false,
      "Invalid values must not enable Telemetry recording."
    );

    // Delete the pref again.
    defaultPrefBranch.deleteBranch(TelemetryUtils.Preferences.TelemetryEnabled);

    // Make sure that flipping it to true works.
    Preferences.set(TelemetryUtils.Preferences.TelemetryEnabled, true);
    await TelemetryController.testReset();
    Assert.equal(
      Telemetry.canRecordExtended,
      true,
      "True must enable Telemetry recording."
    );

    // Also check that the false works as well.
    Preferences.set(TelemetryUtils.Preferences.TelemetryEnabled, false);
    await TelemetryController.testReset();
    Assert.equal(
      Telemetry.canRecordExtended,
      false,
      "False must disable Telemetry recording."
    );

    // Restore the state of the pref.
    Preferences.set(TelemetryUtils.Preferences.TelemetryEnabled, true);
  }
);

add_task(async function test_telemetryCleanFHRDatabase() {
  const FHR_DBNAME_PREF = "datareporting.healthreport.dbName";
  const CUSTOM_DB_NAME = "unlikely.to.be.used.sqlite";
  const DEFAULT_DB_NAME = "healthreport.sqlite";

  // Check that we're able to remove a FHR DB with a custom name.
  const profileDir = await PathUtils.getProfileDir();
  const CUSTOM_DB_PATHS = [
    PathUtils.join(profileDir, CUSTOM_DB_NAME),
    PathUtils.join(profileDir, CUSTOM_DB_NAME + "-wal"),
    PathUtils.join(profileDir, CUSTOM_DB_NAME + "-shm"),
  ];
  Preferences.set(FHR_DBNAME_PREF, CUSTOM_DB_NAME);

  // Write fake DB files to the profile directory.
  for (let dbFilePath of CUSTOM_DB_PATHS) {
    await IOUtils.writeUTF8(dbFilePath, "some data");
  }

  // Trigger the cleanup and check that the files were removed.
  await TelemetryStorage.removeFHRDatabase();
  for (let dbFilePath of CUSTOM_DB_PATHS) {
    try {
      await IOUtils.read(dbFilePath);
    } catch (e) {
      Assert.ok(e instanceof DOMException);
      Assert.equal(
        e.name,
        "NotFoundError",
        "The DB must not be on the disk anymore: " + dbFilePath
      );
    }
  }

  // We should not break anything if there's no DB file.
  await TelemetryStorage.removeFHRDatabase();

  // Check that we're able to remove a FHR DB with the default name.
  Preferences.reset(FHR_DBNAME_PREF);

  const DEFAULT_DB_PATHS = [
    PathUtils.join(profileDir, DEFAULT_DB_NAME),
    PathUtils.join(profileDir, DEFAULT_DB_NAME + "-wal"),
    PathUtils.join(profileDir, DEFAULT_DB_NAME + "-shm"),
  ];

  // Write fake DB files to the profile directory.
  for (let dbFilePath of DEFAULT_DB_PATHS) {
    await IOUtils.writeUTF8(dbFilePath, "some data");
  }

  // Trigger the cleanup and check that the files were removed.
  await TelemetryStorage.removeFHRDatabase();
  for (let dbFilePath of DEFAULT_DB_PATHS) {
    try {
      await IOUtils.read(dbFilePath);
    } catch (e) {
      Assert.ok(e instanceof DOMException);
      Assert.equal(
        e.name,
        "NotFoundError",
        "The DB must not be on the disk anymore: " + dbFilePath
      );
    }
  }
});

add_task(async function test_sendNewProfile() {
  if (
    gIsAndroid ||
    (AppConstants.platform == "linux" && OS.Constants.Sys.bits == 32)
  ) {
    // We don't support the pingsender on Android, yet, see bug 1335917.
    // We also don't suppor the pingsender testing on Treeherder for
    // Linux 32 bit (due to missing libraries). So skip it there too.
    // See bug 1310703 comment 78.
    return;
  }

  const NEWPROFILE_PING_TYPE = "new-profile";
  const PREF_NEWPROFILE_ENABLED = "toolkit.telemetry.newProfilePing.enabled";
  const PREF_NEWPROFILE_DELAY = "toolkit.telemetry.newProfilePing.delay";

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
    await DATAREPORTING_PATH,
    "session-state.json"
  );
  await IOUtils.remove(stateFilePath);
  Preferences.set(PREF_NEWPROFILE_DELAY, 1);
  Preferences.set(PREF_NEWPROFILE_ENABLED, true);

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
  Preferences.reset(PREF_NEWPROFILE_DELAY);

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
    Assert.ok(false, "The new-profile ping must be sent only on new profiles.")
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
  await CommonUtils.writeJSON(sessionState, stateFilePath);
  await TelemetryController.testReset();
  await TelemetryController.testShutdown();

  // Reset the pref and restart Telemetry.
  Preferences.reset(PREF_NEWPROFILE_ENABLED);
  PingServer.resetPingHandler();
});

add_task(async function test_encryptedPing() {
  if (gIsAndroid) {
    // The underlying jwcrypto module being used here is not currently available on Android.
    return;
  }
  Cu.importGlobalProperties(["crypto"]);

  const ECDH_PARAMS = {
    name: "ECDH",
    namedCurve: "P-256",
  };

  const privateKey = {
    crv: "P-256",
    d: "rcs093UlGDG6piwHenmSDoAxbzMIXT43JkQbkt3xEmI",
    ext: true,
    key_ops: ["deriveKey"],
    kty: "EC",
    x: "h12feyTYBZ__wO_AnM1a5-KTDlko3-YyQ_en19jyrs0",
    y: "6GSfzo14ehDyH5E-xCOedJDAYlN0AGPMCtIgFbheLko",
  };

  const publicKey = {
    crv: "P-256",
    ext: true,
    kty: "EC",
    x: "h12feyTYBZ__wO_AnM1a5-KTDlko3-YyQ_en19jyrs0",
    y: "6GSfzo14ehDyH5E-xCOedJDAYlN0AGPMCtIgFbheLko",
  };

  const pioneerId = "12345";
  const schemaName = "abc";
  const schemaNamespace = "def";
  const schemaVersion = 2;

  Services.prefs.setStringPref("toolkit.telemetry.pioneerId", pioneerId);

  // Stop the sending task and then start it again.
  await TelemetrySend.shutdown();
  // Reset the controller to spin the ping sending task.
  await TelemetryController.testReset();

  // Submit a ping with a custom payload, which will be encrypted.
  let payload = { canary: "test" };
  let pingPromise = TelemetryController.submitExternalPing(
    "pioneer-study",
    payload,
    {
      studyName: "pioneer-dev-1@allizom.org",
      addPioneerId: true,
      useEncryption: true,
      encryptionKeyId: "pioneer-dev-20200423",
      publicKey,
      schemaName,
      schemaNamespace,
      schemaVersion,
    }
  );

  // Wait for the ping to be archived.
  const pingId = await pingPromise;

  let archivedCopy = await TelemetryArchive.promiseArchivedPingById(pingId);

  Assert.notEqual(
    archivedCopy.payload.encryptedData,
    payload,
    "The encrypted payload must not match the plaintext."
  );

  Assert.equal(
    archivedCopy.payload.pioneerId,
    pioneerId,
    "Pioneer ID in ping must match the pref."
  );

  // Validate ping against schema.
  const schema = {
    $schema: "http://json-schema.org/draft-04/schema#",
    properties: {
      application: {
        additionalProperties: false,
        properties: {
          architecture: {
            type: "string",
          },
          buildId: {
            pattern: "^[0-9]{10}",
            type: "string",
          },
          channel: {
            type: "string",
          },
          displayVersion: {
            pattern: "^[0-9]{2,3}\\.",
            type: "string",
          },
          name: {
            type: "string",
          },
          platformVersion: {
            pattern: "^[0-9]{2,3}\\.",
            type: "string",
          },
          vendor: {
            type: "string",
          },
          version: {
            pattern: "^[0-9]{2,3}\\.",
            type: "string",
          },
          xpcomAbi: {
            type: "string",
          },
        },
        required: [
          "architecture",
          "buildId",
          "channel",
          "name",
          "platformVersion",
          "version",
          "vendor",
          "xpcomAbi",
        ],
        type: "object",
      },
      creationDate: {
        pattern:
          "^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}\\.[0-9]{3}Z$",
        type: "string",
      },
      id: {
        pattern:
          "^[a-fA-F0-9]{8}-[a-fA-F0-9]{4}-[a-fA-F0-9]{4}-[a-fA-F0-9]{4}-[a-fA-F0-9]{12}$",
        type: "string",
      },
      payload: {
        description: "",
        properties: {
          encryptedData: {
            description: "JOSE/JWE encrypted payload.",
            type: "string",
          },
          encryptionKeyId: {
            description: "JOSE/JWK key id, e.g. pioneer-20170520.",
            type: "string",
          },
          pioneerId: {
            description: "Custom pioneer id, must not be Telemetry clientId",
            pattern:
              "^[a-fA-F0-9]{8}-[a-fA-F0-9]{4}-[a-fA-F0-9]{4}-[a-fA-F0-9]{4}-[a-fA-F0-9]{12}$",
            type: "string",
          },
          schemaName: {
            description:
              "Name of a schema used for validation of the encryptedData",
            maxLength: 100,
            minLength: 1,
            pattern: "^\\S+$",
            type: "string",
          },
          schemaNamespace: {
            description:
              "The namespace of the schema used for validation and routing to a dataset.",
            maxLength: 100,
            minLength: 1,
            pattern: "^\\S+$",
            type: "string",
          },
          schemaVersion: {
            description: "Integer version number of the schema",
            minimum: 1,
            type: "integer",
          },
          studyName: {
            description: "Name of a particular study. Usually the addon_id.",
            maxLength: 100,
            minLength: 1,
            pattern: "^\\S+$",
            type: "string",
          },
        },
        required: [
          "encryptedData",
          "encryptionKeyId",
          "pioneerId",
          "studyName",
          "schemaName",
          "schemaNamespace",
          "schemaVersion",
        ],
        title: "pioneer-study",
        type: "object",
      },
      type: {
        description: "doc_type, restated",
        enum: ["pioneer-study"],
        type: "string",
      },
      version: {
        maximum: 4,
        minimum: 4,
        type: "integer",
      },
    },
    required: [
      "application",
      "creationDate",
      "id",
      "payload",
      "type",
      "version",
    ],
    title: "pioneer-study",
    type: "object",
  };

  const result = JsonSchemaValidator.validate(archivedCopy, schema);

  Assert.ok(
    result.valid,
    `Archived ping should validate against schema: ${result.error}`
  );

  // check that payload can be decrypted.
  const privateJWK = await crypto.subtle.importKey(
    "jwk",
    privateKey,
    ECDH_PARAMS,
    false,
    ["deriveKey"]
  );

  const decryptedJWE = await jwcrypto.decryptJWE(
    archivedCopy.payload.encryptedData,
    privateJWK
  );

  Assert.deepEqual(
    JSON.parse(new TextDecoder("utf-8").decode(decryptedJWE)),
    payload,
    "decrypted payload should match"
  );
});

add_task(async function test_encryptedPing_overrideId() {
  if (gIsAndroid) {
    // The underlying jwcrypto module being used here is not currently available on Android.
    return;
  }
  Cu.importGlobalProperties(["crypto"]);

  const publicKey = {
    crv: "P-256",
    ext: true,
    kty: "EC",
    x: "h12feyTYBZ__wO_AnM1a5-KTDlko3-YyQ_en19jyrs0",
    y: "6GSfzo14ehDyH5E-xCOedJDAYlN0AGPMCtIgFbheLko",
  };

  const prefPioneerId = "12345";
  const overriddenPioneerId = "c0ffeeaa-bbbb-abab-baba-eeff0ceeff0c";
  const schemaName = "abc";
  const schemaNamespace = "def";
  const schemaVersion = 2;

  Services.prefs.setStringPref("toolkit.telemetry.pioneerId", prefPioneerId);

  let archiveTester = new TelemetryArchiveTesting.Checker();
  await archiveTester.promiseInit();

  // Submit a ping with a custom payload, which will be encrypted.
  let payload = { canary: "test" };
  let pingPromise = TelemetryController.submitExternalPing(
    "test-pioneer-study-override",
    payload,
    {
      studyName: "pioneer-dev-1@allizom.org",
      addPioneerId: true,
      overridePioneerId: overriddenPioneerId,
      useEncryption: true,
      encryptionKeyId: "pioneer-dev-20200423",
      publicKey,
      schemaName,
      schemaNamespace,
      schemaVersion,
    }
  );

  // Wait for the ping to be submitted, to have the ping id to scan the
  // archive for.
  const pingId = await pingPromise;

  // And then wait for the ping to be available in the archive.
  await TestUtils.waitForCondition(
    () => archiveTester.promiseFindPing("test-pioneer-study-override", []),
    "Failed to find the pioneer ping"
  );

  let archivedCopy = await TelemetryArchive.promiseArchivedPingById(pingId);

  Assert.notEqual(
    archivedCopy.payload.encryptedData,
    payload,
    "The encrypted payload must not match the plaintext."
  );

  Assert.equal(
    archivedCopy.payload.pioneerId,
    overriddenPioneerId,
    "Pioneer ID in ping must match the provided override."
  );
});

// Testing shutdown and checking that pings sent afterwards are rejected.
add_task(async function test_pingRejection() {
  await TelemetryController.testReset();
  await TelemetryController.testShutdown();
  await sendPing(false, false).then(
    () => Assert.ok(false, "Pings submitted after shutdown must be rejected."),
    () => Assert.ok(true, "Ping submitted after shutdown correctly rejected.")
  );
});

add_task(async function test_newCanRecordsMatchTheOld() {
  Assert.equal(
    Telemetry.canRecordBase,
    Telemetry.canRecordReleaseData,
    "Release Data is the new way to say Base Collection"
  );
  Assert.equal(
    Telemetry.canRecordExtended,
    Telemetry.canRecordPrereleaseData,
    "Prerelease Data is the new way to say Extended Collection"
  );
});

add_task(function test_histogram_filtering() {
  const COUNT_ID = "TELEMETRY_TEST_COUNT";
  const KEYED_ID = "TELEMETRY_TEST_KEYED_COUNT";
  const count = Telemetry.getHistogramById(COUNT_ID);
  const keyed = Telemetry.getKeyedHistogramById(KEYED_ID);

  count.add(1);
  keyed.add("a", 1);

  let snapshot = Telemetry.getSnapshotForHistograms(
    "main",
    false,
    /* filter */ false
  ).parent;
  let keyedSnapshot = Telemetry.getSnapshotForKeyedHistograms(
    "main",
    false,
    /* filter */ false
  ).parent;
  Assert.ok(COUNT_ID in snapshot, "test histogram should be snapshotted");
  Assert.ok(
    KEYED_ID in keyedSnapshot,
    "test keyed histogram should be snapshotted"
  );

  snapshot = Telemetry.getSnapshotForHistograms(
    "main",
    false,
    /* filter */ true
  ).parent;
  keyedSnapshot = Telemetry.getSnapshotForKeyedHistograms(
    "main",
    false,
    /* filter */ true
  ).parent;
  Assert.ok(
    !(COUNT_ID in snapshot),
    "test histogram should not be snapshotted"
  );
  Assert.ok(
    !(KEYED_ID in keyedSnapshot),
    "test keyed histogram should not be snapshotted"
  );
});

add_task(function test_scalar_filtering() {
  const COUNT_ID = "telemetry.test.unsigned_int_kind";
  const KEYED_ID = "telemetry.test.keyed_unsigned_int";

  Telemetry.scalarSet(COUNT_ID, 2);
  Telemetry.keyedScalarSet(KEYED_ID, "a", 2);

  let snapshot = Telemetry.getSnapshotForScalars(
    "main",
    false,
    /* filter */ false
  ).parent;
  let keyedSnapshot = Telemetry.getSnapshotForKeyedScalars(
    "main",
    false,
    /* filter */ false
  ).parent;
  Assert.ok(COUNT_ID in snapshot, "test scalars should be snapshotted");
  Assert.ok(
    KEYED_ID in keyedSnapshot,
    "test keyed scalars should be snapshotted"
  );

  snapshot = Telemetry.getSnapshotForScalars("main", false, /* filter */ true)
    .parent;
  keyedSnapshot = Telemetry.getSnapshotForKeyedScalars(
    "main",
    false,
    /* filter */ true
  ).parent;
  Assert.ok(!(COUNT_ID in snapshot), "test scalars should not be snapshotted");
  Assert.ok(
    !(KEYED_ID in keyedSnapshot),
    "test keyed scalars should not be snapshotted"
  );
});

add_task(async function stopServer() {
  await PingServer.stop();
});
