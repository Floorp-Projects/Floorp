/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ 
*/
/* This testcase triggers two telemetry pings.
 *
 * Telemetry code keeps histograms of past telemetry pings. The first
 * ping populates these histograms. One of those histograms is then
 * checked in the second request.
 */

Cu.import("resource://gre/modules/ClientID.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);
Cu.import("resource://gre/modules/TelemetryController.jsm", this);
Cu.import("resource://gre/modules/TelemetryStorage.jsm", this);
Cu.import("resource://gre/modules/TelemetrySend.jsm", this);
Cu.import("resource://gre/modules/TelemetryArchive.jsm", this);
Cu.import("resource://gre/modules/Task.jsm", this);
Cu.import("resource://gre/modules/Promise.jsm", this);
Cu.import("resource://gre/modules/Preferences.jsm");

const PING_FORMAT_VERSION = 4;
const DELETION_PING_TYPE = "deletion";
const TEST_PING_TYPE = "test-ping-type";

const PLATFORM_VERSION = "1.9.2";
const APP_VERSION = "1";
const APP_NAME = "XPCShell";

const PREF_BRANCH = "toolkit.telemetry.";
const PREF_ENABLED = PREF_BRANCH + "enabled";
const PREF_ARCHIVE_ENABLED = PREF_BRANCH + "archive.enabled";
const PREF_FHR_UPLOAD_ENABLED = "datareporting.healthreport.uploadEnabled";
const PREF_FHR_SERVICE_ENABLED = "datareporting.healthreport.service.enabled";
const PREF_UNIFIED = PREF_BRANCH + "unified";
const PREF_OPTOUT_SAMPLE = PREF_BRANCH + "optoutSample";

var gClientID = null;

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
    "type", "id", "creationDate", "version", "application", "payload"
  ];

  const APPLICATION_TEST_DATA = {
    buildId: "2007010101",
    name: APP_NAME,
    version: APP_VERSION,
    vendor: "Mozilla",
    platformVersion: PLATFORM_VERSION,
    xpcomAbi: "noarch-spidermonkey",
  };

  // Check that the ping contains all the mandatory fields.
  for (let f of MANDATORY_PING_FIELDS) {
    Assert.ok(f in aPing, f + " must be available.");
  }

  Assert.equal(aPing.type, aType, "The ping must have the correct type.");
  Assert.equal(aPing.version, PING_FORMAT_VERSION, "The ping must have the correct version.");

  // Test the application section.
  for (let f in APPLICATION_TEST_DATA) {
    Assert.equal(aPing.application[f], APPLICATION_TEST_DATA[f],
                 f + " must have the correct value.");
  }

  // We can't check the values for channel and architecture. Just make
  // sure they are in.
  Assert.ok("architecture" in aPing.application,
            "The application section must have an architecture field.");
  Assert.ok("channel" in aPing.application,
            "The application section must have a channel field.");

  // Check the clientId and environment fields, as needed.
  Assert.equal("clientId" in aPing, aHasClientId);
  Assert.equal("environment" in aPing, aHasEnvironment);
}

function run_test() {
  do_test_pending();

  // Addon manager needs a profile directory
  do_get_profile();
  loadAddonManager("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  Services.prefs.setBoolPref(PREF_ENABLED, true);
  Services.prefs.setBoolPref(PREF_FHR_UPLOAD_ENABLED, true);
  Services.prefs.setBoolPref(PREF_FHR_SERVICE_ENABLED, true);

  Telemetry.asyncFetchTelemetryData(wrapWithExceptionHandler(run_next_test));
}

add_task(function* asyncSetup() {
  yield TelemetryController.setup();

  gClientID = yield ClientID.getClientID();

  // We should have cached the client id now. Lets confirm that by
  // checking the client id before the async ping setup is finished.
  let promisePingSetup = TelemetryController.reset();
  do_check_eq(TelemetryController.clientID, gClientID);
  yield promisePingSetup;
});

// Ensure that not overwriting an existing file fails silently
add_task(function* test_overwritePing() {
  let ping = {id: "foo"};
  yield TelemetryStorage.savePing(ping, true);
  yield TelemetryStorage.savePing(ping, false);
  yield TelemetryStorage.cleanupPingFile(ping);
});

// Checks that a sent ping is correctly received by a dummy http server.
add_task(function* test_simplePing() {
  PingServer.start();

  yield sendPing(false, false);
  let request = yield PingServer.promiseNextRequest();

  // Check that we have a version query parameter in the URL.
  Assert.notEqual(request.queryString, "");

  // Make sure the version in the query string matches the new ping format version.
  let params = request.queryString.split("&");
  Assert.ok(params.find(p => p == ("v=" + PING_FORMAT_VERSION)));

  let ping = decodeRequestPayload(request);
  checkPingFormat(ping, TEST_PING_TYPE, false, false);
});

add_task(function* test_disableDataUpload() {
  const isUnified = Preferences.get(PREF_UNIFIED, false);
  if (!isUnified) {
    // Skipping the test if unified telemetry is off, as no deletion ping will
    // be generated.
    return;
  }

  const PREF_TELEMETRY_SERVER = "toolkit.telemetry.server";

  // Disable FHR upload: this should trigger a deletion ping.
  Preferences.set(PREF_FHR_UPLOAD_ENABLED, false);

  let ping = yield PingServer.promiseNextPing();
  checkPingFormat(ping, DELETION_PING_TYPE, true, false);
  // Wait on ping activity to settle.
  yield TelemetrySend.testWaitOnOutgoingPings();

  // Restore FHR Upload.
  Preferences.set(PREF_FHR_UPLOAD_ENABLED, true);

  // Simulate a failure in sending the deletion ping by disabling the HTTP server.
  yield PingServer.stop();

  // Try to send a ping. It will be saved as pending  and get deleted when disabling upload.
  TelemetryController.submitExternalPing(TEST_PING_TYPE, {});

  // Disable FHR upload to send a deletion ping again.
  Preferences.set(PREF_FHR_UPLOAD_ENABLED, false);

  // Wait on sending activity to settle, as |TelemetryController.reset()| doesn't do that.
  yield TelemetrySend.testWaitOnOutgoingPings();
  // Wait for the pending pings to be deleted. Resetting TelemetryController doesn't
  // trigger the shutdown, so we need to call it ourselves.
  yield TelemetryStorage.shutdown();
  // Simulate a restart, and spin the send task.
  yield TelemetryController.reset();

  // Disabling Telemetry upload must clear out all the pending pings.
  let pendingPings = yield TelemetryStorage.loadPendingPingList();
  Assert.equal(pendingPings.length, 1,
               "All the pending pings but the deletion ping should have been deleted");

  // Enable the ping server again.
  PingServer.start();
  // We set the new server using the pref, otherwise it would get reset with
  // |TelemetryController.reset|.
  Preferences.set(PREF_TELEMETRY_SERVER, "http://localhost:" + PingServer.port);

  // Reset the controller to spin the ping sending task.
  yield TelemetryController.reset();
  ping = yield PingServer.promiseNextPing();
  checkPingFormat(ping, DELETION_PING_TYPE, true, false);

  // Restore FHR Upload.
  Preferences.set(PREF_FHR_UPLOAD_ENABLED, true);
});

add_task(function* test_pingHasClientId() {
  // Send a ping with a clientId.
  yield sendPing(true, false);

  let ping = yield PingServer.promiseNextPing();
  checkPingFormat(ping, TEST_PING_TYPE, true, false);

  if (HAS_DATAREPORTINGSERVICE &&
      Services.prefs.getBoolPref(PREF_FHR_UPLOAD_ENABLED)) {
    Assert.equal(ping.clientId, gClientID,
                 "The correct clientId must be reported.");
  }
});

add_task(function* test_pingHasEnvironment() {
  // Send a ping with the environment data.
  yield sendPing(false, true);
  let ping = yield PingServer.promiseNextPing();
  checkPingFormat(ping, TEST_PING_TYPE, false, true);

  // Test a field in the environment build section.
  Assert.equal(ping.application.buildId, ping.environment.build.buildId);
});

add_task(function* test_pingHasEnvironmentAndClientId() {
  // Send a ping with the environment data and client id.
  yield sendPing(true, true);
  let ping = yield PingServer.promiseNextPing();
  checkPingFormat(ping, TEST_PING_TYPE, true, true);

  // Test a field in the environment build section.
  Assert.equal(ping.application.buildId, ping.environment.build.buildId);
  // Test that we have the correct clientId.
  if (HAS_DATAREPORTINGSERVICE &&
      Services.prefs.getBoolPref(PREF_FHR_UPLOAD_ENABLED)) {
    Assert.equal(ping.clientId, gClientID,
                 "The correct clientId must be reported.");
  }
});

add_task(function* test_archivePings() {
  const ARCHIVE_PATH =
    OS.Path.join(OS.Constants.Path.profileDir, "datareporting", "archived");

  let now = new Date(2009, 10, 18, 12, 0, 0);
  fakeNow(now);

  // Disable ping upload so that pings don't get sent.
  // With unified telemetry the FHR upload pref controls this,
  // with non-unified telemetry the Telemetry enabled pref.
  const isUnified = Preferences.get(PREF_UNIFIED, false);
  const uploadPref = isUnified ? PREF_FHR_UPLOAD_ENABLED : PREF_ENABLED;
  Preferences.set(uploadPref, false);

  // If we're using unified telemetry, disabling ping upload will generate a "deletion"
  // ping. Catch it.
  if (isUnified) {
    let ping = yield PingServer.promiseNextPing();
    checkPingFormat(ping, DELETION_PING_TYPE, true, false);
  }

  // Register a new Ping Handler that asserts if a ping is received, then send a ping.
  PingServer.registerPingHandler(() => Assert.ok(false, "Telemetry must not send pings if not allowed to."));
  let pingId = yield sendPing(true, true);

  // Check that the ping was archived, even with upload disabled.
  let ping = yield TelemetryArchive.promiseArchivedPingById(pingId);
  Assert.equal(ping.id, pingId, "TelemetryController should still archive pings.");

  // Check that pings don't get archived if not allowed to.
  now = new Date(2010, 10, 18, 12, 0, 0);
  fakeNow(now);
  Preferences.set(PREF_ARCHIVE_ENABLED, false);
  pingId = yield sendPing(true, true);
  let promise = TelemetryArchive.promiseArchivedPingById(pingId);
  Assert.ok((yield promiseRejects(promise)),
    "TelemetryController should not archive pings if the archive pref is disabled.");

  // Enable archiving and the upload so that pings get sent and archived again.
  Preferences.set(uploadPref, true);
  Preferences.set(PREF_ARCHIVE_ENABLED, true);

  now = new Date(2014, 6, 18, 22, 0, 0);
  fakeNow(now);
  // Restore the non asserting ping handler.
  PingServer.resetPingHandler();
  pingId = yield sendPing(true, true);

  // Check that we archive pings when successfully sending them.
  yield PingServer.promiseNextPing();
  ping = yield TelemetryArchive.promiseArchivedPingById(pingId);
  Assert.equal(ping.id, pingId,
    "TelemetryController should still archive pings if ping upload is enabled.");
});

// Test that we fuzz the submission time around midnight properly
// to avoid overloading the telemetry servers.
add_task(function* test_midnightPingSendFuzzing() {
  const fuzzingDelay = 60 * 60 * 1000;
  fakeMidnightPingFuzzingDelay(fuzzingDelay);
  let now = new Date(2030, 5, 1, 11, 0, 0);
  fakeNow(now);

  let waitForTimer = () => new Promise(resolve => {
    fakePingSendTimer((callback, timeout) => {
      resolve([callback, timeout]);
    }, () => {});
  });

  PingServer.clearRequests();
  yield TelemetryController.reset();

  // A ping after midnight within the fuzzing delay should not get sent.
  now = new Date(2030, 5, 2, 0, 40, 0);
  fakeNow(now);
  PingServer.registerPingHandler((req, res) => {
    Assert.ok(false, "No ping should be received yet.");
  });
  let timerPromise = waitForTimer();
  yield sendPing(true, true);
  let [timerCallback, timerTimeout] = yield timerPromise;
  Assert.ok(!!timerCallback);
  Assert.deepEqual(futureDate(now, timerTimeout), new Date(2030, 5, 2, 1, 0, 0));

  // A ping just before the end of the fuzzing delay should not get sent.
  now = new Date(2030, 5, 2, 0, 59, 59);
  fakeNow(now);
  timerPromise = waitForTimer();
  yield sendPing(true, true);
  [timerCallback, timerTimeout] = yield timerPromise;
  Assert.deepEqual(timerTimeout, 1 * 1000);

  // Restore the previous ping handler.
  PingServer.resetPingHandler();

  // Setting the clock to after the fuzzing delay, we should trigger the two ping sends
  // with the timer callback.
  now = futureDate(now, timerTimeout);
  fakeNow(now);
  yield timerCallback();
  const pings = yield PingServer.promiseNextPings(2);
  for (let ping of pings) {
    checkPingFormat(ping, TEST_PING_TYPE, true, true);
  }
  yield TelemetrySend.testWaitOnOutgoingPings();

  // Moving the clock further we should still send pings immediately.
  now = futureDate(now, 5 * 60 * 1000);
  yield sendPing(true, true);
  let ping = yield PingServer.promiseNextPing();
  checkPingFormat(ping, TEST_PING_TYPE, true, true);
  yield TelemetrySend.testWaitOnOutgoingPings();

  // Check that pings shortly before midnight are immediately sent.
  now = fakeNow(2030, 5, 3, 23, 59, 0);
  yield sendPing(true, true);
  ping = yield PingServer.promiseNextPing();
  checkPingFormat(ping, TEST_PING_TYPE, true, true);
  yield TelemetrySend.testWaitOnOutgoingPings();

  // Clean-up.
  fakeMidnightPingFuzzingDelay(0);
  fakePingSendTimer(() => {}, () => {});
});

add_task(function* test_changePingAfterSubmission() {
  // Submit a ping with a custom payload.
  let payload = { canary: "test" };
  let pingPromise = TelemetryController.submitExternalPing(TEST_PING_TYPE, payload, options);

  // Change the payload with a predefined value.
  payload.canary = "changed";

  // Wait for the ping to be archived.
  const pingId = yield pingPromise;

  // Make sure our changes didn't affect the submitted payload.
  let archivedCopy = yield TelemetryArchive.promiseArchivedPingById(pingId);
  Assert.equal(archivedCopy.payload.canary, "test",
               "The payload must not be changed after being submitted.");
});

add_task(function* test_optoutSampling() {
  if (!Preferences.get(PREF_UNIFIED, false)) {
    dump("Unified Telemetry is disabled, skipping.\n");
    return;
  }

  const DATA = [
    {uuid: null,                                   sampled: false}, // not to be sampled
    {uuid: "3d38d821-14a4-3d45-ab0b-02a9fb5a7505", sampled: false}, // samples to 0
    {uuid: "1331255e-7eb5-aa4f-b04e-494a0c6da282", sampled: false}, // samples to 41
    {uuid: "35393e78-a363-ea4e-9fc9-9f9abbee2077", sampled: true }, // samples to 42
    {uuid: "4dc81df6-db03-a34e-ba79-3e877afd22c4", sampled: true }, // samples to 43
    {uuid: "79e15be6-4884-8d4f-98e5-f94790251e5f", sampled: true }, // samples to 44
    {uuid: "c3841566-e39e-384d-826f-508ab6387b21", sampled: true }, // samples to 45
    {uuid: "cc7498a4-2cde-da47-89b3-f3ce5dd7c6fc", sampled: true }, // samples to 46
    {uuid: "0750d8ed-5969-3a4f-90ba-2e85f9074309", sampled: false}, // samples to 47
    {uuid: "0dfcbce7-d82b-b144-8d77-eb15935c9a8e", sampled: false}, // samples to 99
  ];

  // Test that the opt-out pref enables us sampling on 5% of release.
  Preferences.set(PREF_ENABLED, false);
  Preferences.set(PREF_OPTOUT_SAMPLE, true);
  fakeIsUnifiedOptin(true);

  for (let d of DATA) {
    dump("Testing sampling for uuid: " + d.uuid + "\n");
    fakeCachedClientId(d.uuid);
    yield TelemetryController.reset();
    Assert.equal(TelemetryController.isInOptoutSample, d.sampled,
                 "Opt-out sampling should behave as expected");
    Assert.equal(Telemetry.canRecordBase, d.sampled,
                 "Base recording setting should be correct");
  }

  // If we disable opt-out sampling Telemetry, have the opt-in setting on and extended Telemetry off,
  // we should not enable anything.
  Preferences.set(PREF_OPTOUT_SAMPLE, false);
  fakeIsUnifiedOptin(true);
  for (let d of DATA) {
    dump("Testing sampling for uuid: " + d.uuid + "\n");
    fakeCachedClientId(d.uuid);
    yield TelemetryController.reset();
    Assert.equal(Telemetry.canRecordBase, false,
                 "Sampling should not override the default opt-out behavior");
  }

  // If we fully enable opt-out Telemetry on release, the sampling should not override that.
  Preferences.set(PREF_OPTOUT_SAMPLE, true);
  fakeIsUnifiedOptin(false);
  for (let d of DATA) {
    dump("Testing sampling for uuid: " + d.uuid + "\n");
    fakeCachedClientId(d.uuid);
    yield TelemetryController.reset();
    Assert.equal(Telemetry.canRecordBase, true,
                 "Sampling should not override the default opt-out behavior");
  }
});

add_task(function* stopServer(){
  yield PingServer.stop();
  do_test_finished();
});
