/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/
*/

ChromeUtils.import("resource://services-common/utils.js");
ChromeUtils.import("resource://gre/modules/ClientID.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/TelemetryController.jsm", this);
ChromeUtils.import("resource://gre/modules/TelemetryStorage.jsm", this);
ChromeUtils.import("resource://gre/modules/TelemetrySend.jsm", this);
ChromeUtils.import("resource://gre/modules/TelemetryUtils.jsm", this);
ChromeUtils.import("resource://gre/modules/Preferences.jsm");

const PING_FORMAT_VERSION = 4;
const OPTOUT_PING_TYPE = "optout";
const TEST_PING_TYPE = "test-ping-type";

function sendPing(addEnvironment = false) {
  let options = {
    addClientId: true,
    addEnvironment,
  };
  return TelemetryController.submitExternalPing(TEST_PING_TYPE, {}, options);
}

add_task(async function test_setup() {
  // Addon manager needs a profile directory
  do_get_profile();
  // Make sure we don't generate unexpected pings due to pref changes.
  await setEmptyPrefWatchlist();

  Services.prefs.setBoolPref(TelemetryUtils.Preferences.FhrUploadEnabled, true);

  await new Promise(resolve =>
    Telemetry.asyncFetchTelemetryData(wrapWithExceptionHandler(resolve)));

  PingServer.start();
  Preferences.set(TelemetryUtils.Preferences.Server, "http://localhost:" + PingServer.port);
  await TelemetryController.testSetup();
});

/**
 * Testing the following scenario:
 *
 * 1. Telemetry upload gets disabled
 * 2. Canary client ID is set
 * 3. Instance is shut down
 * 4. Telemetry upload flag is toggled
 * 5. Instance is started again
 * 6. Detect that upload is enabled and reset client ID
 *
 * This scenario e.g. happens when switching between channels
 * with and without the optout ping reset included.
 */
add_task(async function test_clientid_reset_after_reenabling() {
  const isUnified = Preferences.get(TelemetryUtils.Preferences.Unified, false);
  if (!isUnified) {
    // Skipping the test if unified telemetry is off, as no optout ping will
    // be generated.
    return;
  }

  await sendPing();
  let ping = await PingServer.promiseNextPing();
  Assert.equal(ping.type, TEST_PING_TYPE, "The ping must be a test ping");
  Assert.ok("clientId" in ping);

  let firstClientId = ping.clientId;
  Assert.notEqual(TelemetryUtils.knownClientID, firstClientId, "Client ID should be valid and random");

  // Disable FHR upload: this should trigger a optout ping.
  Preferences.set(TelemetryUtils.Preferences.FhrUploadEnabled, false);

  ping = await PingServer.promiseNextPing();
  Assert.equal(ping.type, OPTOUT_PING_TYPE, "The ping must be an optout ping");
  Assert.ok(!("clientId" in ping));
  let clientId = await ClientID.getClientID();
  Assert.equal(TelemetryUtils.knownClientID, clientId);

  // Now shutdown the instance
  await TelemetryController.testShutdown();
  await TelemetryStorage.testClearPendingPings();

  // Flip the pref again
  Preferences.set(TelemetryUtils.Preferences.FhrUploadEnabled, true);

  // Start the instance
  await TelemetryController.testReset();

  let newClientId = await ClientID.getClientID();
  Assert.notEqual(TelemetryUtils.knownClientID, newClientId, "Client ID should be valid and random");
  Assert.notEqual(firstClientId, newClientId, "Client ID should be newly generated");
});

/**
 * Testing the following scenario:
 * (Reverse of the first test)
 *
 * 1. Telemetry upload gets disabled, canary client ID is set
 * 2. Telemetry upload is enabled
 * 3. New client ID is generated.
 * 3. Instance is shut down
 * 4. Telemetry upload flag is toggled
 * 5. Instance is started again
 * 6. Detect that upload is disabled and sets canary client ID
 *
 * This scenario e.g. happens when switching between channels
 * with and without the optout ping reset included.
 */
add_task(async function test_clientid_canary_after_disabling() {
  const isUnified = Preferences.get(TelemetryUtils.Preferences.Unified, false);
  if (!isUnified) {
    // Skipping the test if unified telemetry is off, as no optout ping will
    // be generated.
    return;
  }

  await sendPing();
  let ping = await PingServer.promiseNextPing();
  Assert.equal(ping.type, TEST_PING_TYPE, "The ping must be a test ping");
  Assert.ok("clientId" in ping);

  let firstClientId = ping.clientId;
  Assert.notEqual(TelemetryUtils.knownClientID, firstClientId, "Client ID should be valid and random");

  // Disable FHR upload: this should trigger a optout ping.
  Preferences.set(TelemetryUtils.Preferences.FhrUploadEnabled, false);

  ping = await PingServer.promiseNextPing();
  Assert.equal(ping.type, OPTOUT_PING_TYPE, "The ping must be an optout ping");
  Assert.ok(!("clientId" in ping));
  let clientId = await ClientID.getClientID();
  Assert.equal(TelemetryUtils.knownClientID, clientId);

  Preferences.set(TelemetryUtils.Preferences.FhrUploadEnabled, true);
  await sendPing();
  ping = await PingServer.promiseNextPing();
  Assert.equal(ping.type, TEST_PING_TYPE, "The ping must be a test ping");
  Assert.notEqual(firstClientId, ping.clientId, "Client ID should be newly generated");

  // Now shutdown the instance
  await TelemetryController.testShutdown();
  await TelemetryStorage.testClearPendingPings();

  // Flip the pref again
  Preferences.set(TelemetryUtils.Preferences.FhrUploadEnabled, false);

  // Start the instance
  await TelemetryController.testReset();

  let newClientId = await ClientID.getClientID();
  Assert.equal(TelemetryUtils.knownClientID, newClientId, "Client ID should be a canary when upload disabled");
});

/**
 * On Android Telemetry is not unified.
 * This tests that we reset the client ID if it was previously reset to a canary client ID by accident.
 */
add_task(async function test_clientid_canary_reset_canary_on_nonunified() {
  const isUnified = Preferences.get(TelemetryUtils.Preferences.Unified, false);
  if (isUnified) {
    // Skipping the test if unified telemetry is on.
    return;
  }

  await sendPing();
  let ping = await PingServer.promiseNextPing();
  Assert.equal(ping.type, TEST_PING_TYPE, "The ping must be a test ping");
  Assert.ok("clientId" in ping);

  let firstClientId = ping.clientId;
  Assert.notEqual(TelemetryUtils.knownClientID, firstClientId, "Client ID should be valid and random");

  // Force a canary client ID
  let clientId = await ClientID.setClientID(TelemetryUtils.knownClientID);
  Assert.equal(TelemetryUtils.knownClientID, clientId);

  // Now shutdown the instance
  await TelemetryController.testShutdown();
  await TelemetryStorage.testClearPendingPings();

  // Start the instance
  await TelemetryController.testReset();

  let newClientId = await ClientID.getClientID();
  Assert.notEqual(TelemetryUtils.knownClientID, newClientId, "Client ID should be valid and random");
  Assert.notEqual(firstClientId, newClientId, "Client ID should be valid and random");
});

/**
 * On Android Telemetry is not unified.
 * This tests that we don't touch the client ID if the pref is toggled for some reason.
 */
add_task(async function test_clientid_canary_nonunified_no_pref_trigger() {
  const isUnified = Preferences.get(TelemetryUtils.Preferences.Unified, false);
  if (isUnified) {
    // Skipping the test if unified telemetry is on.
    return;
  }

  await sendPing();
  let ping = await PingServer.promiseNextPing();
  Assert.equal(ping.type, TEST_PING_TYPE, "The ping must be a test ping");
  Assert.ok("clientId" in ping);

  let firstClientId = ping.clientId;
  Assert.notEqual(TelemetryUtils.knownClientID, firstClientId, "Client ID should be valid and random");

  // Flip the pref again
  Preferences.set(TelemetryUtils.Preferences.FhrUploadEnabled, true);

  // Restart the instance
  await TelemetryController.testShutdown();
  await TelemetryStorage.testClearPendingPings();
  await TelemetryController.testReset();

  let newClientId = await ClientID.getClientID();
  Assert.equal(firstClientId, newClientId, "Client ID should be unmodified");

  // Flip the pref again
  Preferences.set(TelemetryUtils.Preferences.FhrUploadEnabled, false);

  // Restart the instance
  await TelemetryController.testShutdown();
  await TelemetryStorage.testClearPendingPings();
  await TelemetryController.testReset();

  newClientId = await ClientID.getClientID();
  Assert.equal(firstClientId, newClientId, "Client ID should be unmodified");
});

add_task(async function stopServer() {
  await PingServer.stop();
});
