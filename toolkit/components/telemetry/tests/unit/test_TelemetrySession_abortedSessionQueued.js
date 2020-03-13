/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/
*/

/**
 * This file only contains the |test_abortedSessionQueued| test. This needs
 * to be in a separate, stand-alone file since we're initializing Telemetry
 * twice, in a non-standard way to simulate incorrect shutdowns. Doing this
 * in other files might interfere with the other tests.
 */

ChromeUtils.import("resource://services-common/utils.js", this);
ChromeUtils.import("resource://gre/modules/TelemetryStorage.jsm", this);
ChromeUtils.import("resource://gre/modules/Services.jsm", this);

const DATAREPORTING_DIR = "datareporting";
const ABORTED_PING_FILE_NAME = "aborted-session-ping";
const ABORTED_SESSION_UPDATE_INTERVAL_MS = 5 * 60 * 1000;

const PING_TYPE_MAIN = "main";
const REASON_ABORTED_SESSION = "aborted-session";
const TEST_PING_TYPE = "test-ping-type";

XPCOMUtils.defineLazyGetter(this, "DATAREPORTING_PATH", function() {
  return OS.Path.join(OS.Constants.Path.profileDir, DATAREPORTING_DIR);
});

function sendPing() {
  if (PingServer.started) {
    TelemetrySend.setServer("http://localhost:" + PingServer.port);
  } else {
    TelemetrySend.setServer("http://doesnotexist");
  }

  let options = {
    addClientId: true,
    addEnvironment: true,
  };
  return TelemetryController.submitExternalPing(TEST_PING_TYPE, {}, options);
}

add_task(async function test_setup() {
  do_get_profile();
  PingServer.start();
  Services.prefs.setCharPref(
    TelemetryUtils.Preferences.Server,
    "http://localhost:" + PingServer.port
  );
});

add_task(async function test_abortedSessionQueued() {
  const ABORTED_FILE = OS.Path.join(DATAREPORTING_PATH, ABORTED_PING_FILE_NAME);

  // Make sure the aborted sessions directory does not exist to test its creation.
  await OS.File.removeDir(DATAREPORTING_PATH, { ignoreAbsent: true });

  let schedulerTickCallback = null;
  let now = new Date(2040, 1, 1, 0, 0, 0);
  fakeNow(now);
  // Fake scheduler functions to control aborted-session flow in tests.
  fakeSchedulerTimer(
    callback => (schedulerTickCallback = callback),
    () => {}
  );
  await TelemetryController.testReset();

  Assert.ok(
    await OS.File.exists(DATAREPORTING_PATH),
    "Telemetry must create the aborted session directory when starting."
  );

  // Fake now again so that the scheduled aborted-session save takes place.
  now = futureDate(now, ABORTED_SESSION_UPDATE_INTERVAL_MS);
  fakeNow(now);
  // The first aborted session checkpoint must take place right after the initialisation.
  Assert.ok(!!schedulerTickCallback);
  // Execute one scheduler tick.
  await schedulerTickCallback();
  // Check that the aborted session is due at the correct time.
  Assert.ok(
    await OS.File.exists(ABORTED_FILE),
    "There must be an aborted session ping."
  );

  await TelemetryStorage.testClearPendingPings();
  PingServer.clearRequests();
  await TelemetryController.testReset();

  Assert.ok(
    !(await OS.File.exists(ABORTED_FILE)),
    "The aborted session ping must be removed from the aborted session ping directory."
  );

  // Restarting Telemetry again to trigger sending pings in TelemetrySend.
  await TelemetryController.testReset();

  // We should have received an aborted-session ping.
  const receivedPing = await PingServer.promiseNextPing();
  Assert.equal(
    receivedPing.type,
    PING_TYPE_MAIN,
    "Should have the correct type"
  );
  Assert.equal(
    receivedPing.payload.info.reason,
    REASON_ABORTED_SESSION,
    "Ping should have the correct reason"
  );

  await TelemetryController.testShutdown();
});

/*
 * An aborted-session ping might have been written when Telemetry upload was disabled and
 * the profile had a canary client ID.
 * These pings should not be sent out at a later point when Telemetry is enabled again.
 */
add_task(async function test_abortedSession_canary_clientid() {
  const ABORTED_FILE = OS.Path.join(DATAREPORTING_PATH, ABORTED_PING_FILE_NAME);

  // Make sure the aborted sessions directory does not exist to test its creation.
  await OS.File.removeDir(DATAREPORTING_PATH, { ignoreAbsent: true });

  let schedulerTickCallback = null;
  let now = new Date(2040, 1, 1, 0, 0, 0);
  fakeNow(now);
  // Fake scheduler functions to control aborted-session flow in tests.
  fakeSchedulerTimer(
    callback => (schedulerTickCallback = callback),
    () => {}
  );
  await TelemetryController.testReset();

  Assert.ok(
    await OS.File.exists(DATAREPORTING_PATH),
    "Telemetry must create the aborted session directory when starting."
  );

  // Fake now again so that the scheduled aborted-session save takes place.
  now = futureDate(now, ABORTED_SESSION_UPDATE_INTERVAL_MS);
  fakeNow(now);
  // The first aborted session checkpoint must take place right after the initialisation.
  Assert.ok(!!schedulerTickCallback);
  // Execute one scheduler tick.
  await schedulerTickCallback();
  // Check that the aborted session is due at the correct time.
  Assert.ok(
    await OS.File.exists(ABORTED_FILE),
    "There must be an aborted session ping."
  );

  // Set clientID in aborted-session ping to canary value
  let abortedPing = await CommonUtils.readJSON(ABORTED_FILE);
  abortedPing.clientId = TelemetryUtils.knownClientID;
  OS.File.writeAtomic(ABORTED_FILE, JSON.stringify(abortedPing), {
    encoding: "utf-8",
  });

  await TelemetryStorage.testClearPendingPings();
  PingServer.clearRequests();
  await TelemetryController.testReset();

  Assert.ok(
    !(await OS.File.exists(ABORTED_FILE)),
    "The aborted session ping must be removed from the aborted session ping directory."
  );

  // Restarting Telemetry again to trigger sending pings in TelemetrySend.
  await TelemetryController.testReset();

  // Trigger a test ping, so we can verify the server received something.
  sendPing();

  // We should have received an aborted-session ping.
  const receivedPing = await PingServer.promiseNextPing();
  Assert.equal(
    receivedPing.type,
    TEST_PING_TYPE,
    "Should have received test ping"
  );

  await TelemetryController.testShutdown();
});

add_task(async function stopServer() {
  await PingServer.stop();
});
