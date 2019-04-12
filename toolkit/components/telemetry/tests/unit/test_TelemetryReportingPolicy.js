/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that TelemetryController sends close to shutdown don't lead
// to AsyncShutdown timeouts.

"use strict";

ChromeUtils.import("resource://gre/modules/Preferences.jsm", this);
ChromeUtils.import("resource://gre/modules/Services.jsm", this);
ChromeUtils.import("resource://gre/modules/TelemetryController.jsm", this);
ChromeUtils.import("resource://gre/modules/TelemetrySend.jsm", this);
ChromeUtils.import("resource://gre/modules/TelemetryReportingPolicy.jsm", this);
ChromeUtils.import("resource://gre/modules/TelemetryUtils.jsm", this);
ChromeUtils.import("resource://gre/modules/Timer.jsm", this);
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm", this);
ChromeUtils.import("resource://gre/modules/UpdateUtils.jsm", this);

const TEST_CHANNEL = "TestChannelABC";

const PREF_MINIMUM_CHANNEL_POLICY_VERSION =
  TelemetryUtils.Preferences.MinimumPolicyVersion + ".channel-" + TEST_CHANNEL;

function fakeShowPolicyTimeout(set, clear) {
  let reportingPolicy = ChromeUtils.import("resource://gre/modules/TelemetryReportingPolicy.jsm", null);
  reportingPolicy.Policy.setShowInfobarTimeout = set;
  reportingPolicy.Policy.clearShowInfobarTimeout = clear;
}

function fakeResetAcceptedPolicy() {
  Preferences.reset(TelemetryUtils.Preferences.AcceptedPolicyDate);
  Preferences.reset(TelemetryUtils.Preferences.AcceptedPolicyVersion);
}

function setMinimumPolicyVersion(aNewPolicyVersion) {
  const CHANNEL_NAME = UpdateUtils.getUpdateChannel(false);
  // We might have channel-dependent minimum policy versions.
  const CHANNEL_DEPENDENT_PREF =
    TelemetryUtils.Preferences.MinimumPolicyVersion + ".channel-" + CHANNEL_NAME;

  // Does the channel-dependent pref exist? If so, set its value.
  if (Preferences.get(CHANNEL_DEPENDENT_PREF, undefined)) {
    Preferences.set(CHANNEL_DEPENDENT_PREF, aNewPolicyVersion);
    return;
  }

  // We don't have a channel specific minimu, so set the common one.
  Preferences.set(TelemetryUtils.Preferences.MinimumPolicyVersion, aNewPolicyVersion);
}

add_task(async function test_setup() {
  // Addon manager needs a profile directory
  do_get_profile(true);
  loadAddonManager("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");
  finishAddonManagerStartup();
  fakeIntlReady();

  // Make sure SearchService is ready for it to be called.
  await Services.search.init();

  // Make sure we don't generate unexpected pings due to pref changes.
  await setEmptyPrefWatchlist();

  // Don't bypass the notifications in this test, we'll fake it.
  Services.prefs.setBoolPref(TelemetryUtils.Preferences.BypassNotification, false);

  TelemetryReportingPolicy.setup();
});

add_task(async function test_firstRun() {
  const FIRST_RUN_TIMEOUT_MSEC = 60 * 1000; // 60s
  const OTHER_RUNS_TIMEOUT_MSEC = 10 * 1000; // 10s

  Preferences.reset(TelemetryUtils.Preferences.FirstRun);

  let startupTimeout = 0;
  fakeShowPolicyTimeout((callback, timeout) => startupTimeout = timeout, () => {});
  TelemetryReportingPolicy.reset();

  Services.obs.notifyObservers(null, "sessionstore-windows-restored");
  Assert.equal(startupTimeout, FIRST_RUN_TIMEOUT_MSEC,
               "The infobar display timeout should be 60s on the first run.");

  // Run again, and check that we actually wait only 10 seconds.
  TelemetryReportingPolicy.reset();
  Services.obs.notifyObservers(null, "sessionstore-windows-restored");
  Assert.equal(startupTimeout, OTHER_RUNS_TIMEOUT_MSEC,
               "The infobar display timeout should be 10s on other runs.");
});

add_task(async function test_prefs() {
  TelemetryReportingPolicy.reset();

  let now = fakeNow(2009, 11, 18);

  // If the date is not valid (earlier than 2012), we don't regard the policy as accepted.
  TelemetryReportingPolicy.testInfobarShown();
  Assert.ok(!TelemetryReportingPolicy.testIsUserNotified());
  Assert.equal(Preferences.get(TelemetryUtils.Preferences.AcceptedPolicyDate, null), 0,
                "Invalid dates should not make the policy accepted.");

  // Check that the notification date and version are correctly saved to the prefs.
  now = fakeNow(2012, 11, 18);
  TelemetryReportingPolicy.testInfobarShown();
  Assert.equal(Preferences.get(TelemetryUtils.Preferences.AcceptedPolicyDate, null), now.getTime(),
                "A valid date must correctly be saved.");

  // Now that user is notified, check if we are allowed to upload.
  Assert.ok(TelemetryReportingPolicy.canUpload(),
            "We must be able to upload after the policy is accepted.");

  // Disable submission and check that we're no longer allowed to upload.
  Preferences.set(TelemetryUtils.Preferences.DataSubmissionEnabled, false);
  Assert.ok(!TelemetryReportingPolicy.canUpload(),
            "We must not be able to upload if data submission is disabled.");

  // Turn the submission back on.
  Preferences.set(TelemetryUtils.Preferences.DataSubmissionEnabled, true);
  Assert.ok(TelemetryReportingPolicy.canUpload(),
            "We must be able to upload if data submission is enabled and the policy was accepted.");

  // Set a new minimum policy version and check that user is no longer notified.
  let newMinimum = Preferences.get(TelemetryUtils.Preferences.CurrentPolicyVersion, 1) + 1;
  setMinimumPolicyVersion(newMinimum);
  Assert.ok(!TelemetryReportingPolicy.testIsUserNotified(),
            "A greater minimum policy version must invalidate the policy and disable upload.");

  // Eventually accept the policy and make sure user is notified.
  Preferences.set(TelemetryUtils.Preferences.CurrentPolicyVersion, newMinimum);
  TelemetryReportingPolicy.testInfobarShown();
  Assert.ok(TelemetryReportingPolicy.testIsUserNotified(),
            "Accepting the policy again should show the user as notified.");
  Assert.ok(TelemetryReportingPolicy.canUpload(),
            "Accepting the policy again should let us upload data.");

  // Set a new, per channel, minimum policy version. Start by setting a test current channel.
  let defaultPrefs = new Preferences({ defaultBranch: true });
  defaultPrefs.set("app.update.channel", TEST_CHANNEL);

  // Increase and set the new minimum version, then check that we're not notified anymore.
  newMinimum++;
  Preferences.set(PREF_MINIMUM_CHANNEL_POLICY_VERSION, newMinimum);
  Assert.ok(!TelemetryReportingPolicy.testIsUserNotified(),
            "Increasing the minimum policy version should invalidate the policy.");

  // Eventually accept the policy and make sure user is notified.
  Preferences.set(TelemetryUtils.Preferences.CurrentPolicyVersion, newMinimum);
  TelemetryReportingPolicy.testInfobarShown();
  Assert.ok(TelemetryReportingPolicy.testIsUserNotified(),
            "Accepting the policy again should show the user as notified.");
  Assert.ok(TelemetryReportingPolicy.canUpload(),
            "Accepting the policy again should let us upload data.");
});

add_task(async function test_migratePrefs() {
  const DEPRECATED_FHR_PREFS = {
    "datareporting.policy.dataSubmissionPolicyAccepted": true,
    "datareporting.policy.dataSubmissionPolicyBypassAcceptance": true,
    "datareporting.policy.dataSubmissionPolicyResponseType": "foxyeah",
    "datareporting.policy.dataSubmissionPolicyResponseTime": Date.now().toString(),
  };

  // Make sure the preferences are set before setting up the policy.
  for (let name in DEPRECATED_FHR_PREFS) {
    Preferences.set(name, DEPRECATED_FHR_PREFS[name]);
  }
  // Set up the policy.
  TelemetryReportingPolicy.reset();
  // They should have been removed by now.
  for (let name in DEPRECATED_FHR_PREFS) {
    Assert.ok(!Preferences.has(name), name + " should have been removed.");
  }
});

add_task(async function test_userNotifiedOfCurrentPolicy() {
  fakeResetAcceptedPolicy();
  TelemetryReportingPolicy.reset();

  // User should be reported as not notified by default.
  Assert.ok(!TelemetryReportingPolicy.testIsUserNotified(),
            "The initial state should be unnotified.");

  // Forcing a policy version should not automatically make the user notified.
  Preferences.set(TelemetryUtils.Preferences.AcceptedPolicyVersion,
                  TelemetryReportingPolicy.DEFAULT_DATAREPORTING_POLICY_VERSION);
  Assert.ok(!TelemetryReportingPolicy.testIsUserNotified(),
                 "The default state of the date should have a time of 0 and it should therefore fail");

  // Showing the notification bar should make the user notified.
  fakeNow(2012, 11, 11);
  TelemetryReportingPolicy.testInfobarShown();
  Assert.ok(TelemetryReportingPolicy.testIsUserNotified(),
            "Using the proper API causes user notification to report as true.");

  // It is assumed that later versions of the policy will incorporate previous
  // ones, therefore this should also return true.
  let newVersion =
    Preferences.get(TelemetryUtils.Preferences.CurrentPolicyVersion, 1) + 1;
  Preferences.set(TelemetryUtils.Preferences.AcceptedPolicyVersion, newVersion);
  Assert.ok(TelemetryReportingPolicy.testIsUserNotified(),
            "A future version of the policy should pass.");

  newVersion =
    Preferences.get(TelemetryUtils.Preferences.CurrentPolicyVersion, 1) - 1;
  Preferences.set(TelemetryUtils.Preferences.AcceptedPolicyVersion, newVersion);
  Assert.ok(!TelemetryReportingPolicy.testIsUserNotified(),
            "A previous version of the policy should fail.");
});

add_task(async function test_canSend() {
  const TEST_PING_TYPE = "test-ping";

  PingServer.start();
  Preferences.set(TelemetryUtils.Preferences.Server, "http://localhost:" + PingServer.port);

  await TelemetryController.testReset();
  TelemetryReportingPolicy.reset();

  // User should be reported as not notified by default.
  Assert.ok(!TelemetryReportingPolicy.testIsUserNotified(),
            "The initial state should be unnotified.");

  // Assert if we receive any ping before the policy is accepted.
  PingServer.registerPingHandler(() => Assert.ok(false, "Should not have received any pings now"));
  await TelemetryController.submitExternalPing(TEST_PING_TYPE, {});

  // Reset the ping handler.
  PingServer.resetPingHandler();

  // Fake the infobar: this should also trigger the ping send task.
  TelemetryReportingPolicy.testInfobarShown();
  let ping = await PingServer.promiseNextPings(1);
  Assert.equal(ping.length, 1, "We should have received one ping.");
  Assert.equal(ping[0].type, TEST_PING_TYPE,
               "We should have received the previous ping.");

  // Submit another ping, to make sure it gets sent.
  await TelemetryController.submitExternalPing(TEST_PING_TYPE, {});

  // Get the ping and check its type.
  ping = await PingServer.promiseNextPings(1);
  Assert.equal(ping.length, 1, "We should have received one ping.");
  Assert.equal(ping[0].type, TEST_PING_TYPE, "We should have received the new ping.");

  // Fake a restart with a pending ping.
  await TelemetryController.addPendingPing(TEST_PING_TYPE, {});
  await TelemetryController.testReset();

  // We should be immediately sending the ping out.
  ping = await PingServer.promiseNextPings(1);
  Assert.equal(ping.length, 1, "We should have received one ping.");
  Assert.equal(ping[0].type, TEST_PING_TYPE, "We should have received the pending ping.");

  // Submit another ping, to make sure it gets sent.
  await TelemetryController.submitExternalPing(TEST_PING_TYPE, {});

  // Get the ping and check its type.
  ping = await PingServer.promiseNextPings(1);
  Assert.equal(ping.length, 1, "We should have received one ping.");
  Assert.equal(ping[0].type, TEST_PING_TYPE, "We should have received the new ping.");

  await PingServer.stop();
});
