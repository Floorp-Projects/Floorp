/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that TelemetryController sends close to shutdown don't lead
// to AsyncShutdown timeouts.

"use strict";

Cu.import("resource://gre/modules/Preferences.jsm", this);
Cu.import("resource://gre/modules/Services.jsm", this);
Cu.import("resource://gre/modules/TelemetryController.jsm", this);
Cu.import("resource://gre/modules/TelemetrySend.jsm", this);
Cu.import("resource://gre/modules/TelemetryReportingPolicy.jsm", this);
Cu.import("resource://gre/modules/TelemetryUtils.jsm", this);
Cu.import("resource://gre/modules/Timer.jsm", this);
Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);
Cu.import("resource://gre/modules/UpdateUtils.jsm", this);

const PREF_BRANCH = "toolkit.telemetry.";
const PREF_SERVER = PREF_BRANCH + "server";

const TEST_CHANNEL = "TestChannelABC";

const PREF_POLICY_BRANCH = "datareporting.policy.";
const PREF_BYPASS_NOTIFICATION = PREF_POLICY_BRANCH + "dataSubmissionPolicyBypassNotification";
const PREF_DATA_SUBMISSION_ENABLED = PREF_POLICY_BRANCH + "dataSubmissionEnabled";
const PREF_CURRENT_POLICY_VERSION = PREF_POLICY_BRANCH + "currentPolicyVersion";
const PREF_MINIMUM_POLICY_VERSION = PREF_POLICY_BRANCH + "minimumPolicyVersion";
const PREF_MINIMUM_CHANNEL_POLICY_VERSION = PREF_MINIMUM_POLICY_VERSION + ".channel-" + TEST_CHANNEL;
const PREF_ACCEPTED_POLICY_VERSION = PREF_POLICY_BRANCH + "dataSubmissionPolicyAcceptedVersion";
const PREF_ACCEPTED_POLICY_DATE = PREF_POLICY_BRANCH + "dataSubmissionPolicyNotifiedTime";

function fakeShowPolicyTimeout(set, clear) {
  let reportingPolicy = Cu.import("resource://gre/modules/TelemetryReportingPolicy.jsm", {});
  reportingPolicy.Policy.setShowInfobarTimeout = set;
  reportingPolicy.Policy.clearShowInfobarTimeout = clear;
}

function fakeResetAcceptedPolicy() {
  Preferences.reset(PREF_ACCEPTED_POLICY_DATE);
  Preferences.reset(PREF_ACCEPTED_POLICY_VERSION);
}

function setMinimumPolicyVersion(aNewPolicyVersion) {
  const CHANNEL_NAME = UpdateUtils.getUpdateChannel(false);
  // We might have channel-dependent minimum policy versions.
  const CHANNEL_DEPENDENT_PREF = PREF_MINIMUM_POLICY_VERSION + ".channel-" + CHANNEL_NAME;

  // Does the channel-dependent pref exist? If so, set its value.
  if (Preferences.get(CHANNEL_DEPENDENT_PREF, undefined)) {
    Preferences.set(CHANNEL_DEPENDENT_PREF, aNewPolicyVersion);
    return;
  }

  // We don't have a channel specific minimu, so set the common one.
  Preferences.set(PREF_MINIMUM_POLICY_VERSION, aNewPolicyVersion);
}

add_task(function* test_setup() {
  // Addon manager needs a profile directory
  do_get_profile(true);
  loadAddonManager("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  // Make sure we don't generate unexpected pings due to pref changes.
  yield setEmptyPrefWatchlist();

  Services.prefs.setBoolPref(PREF_TELEMETRY_ENABLED, true);
  // Don't bypass the notifications in this test, we'll fake it.
  Services.prefs.setBoolPref(PREF_BYPASS_NOTIFICATION, false);

  TelemetryReportingPolicy.setup();
});

add_task(function* test_firstRun() {
  const PREF_FIRST_RUN = "toolkit.telemetry.reportingpolicy.firstRun";
  const FIRST_RUN_TIMEOUT_MSEC = 60 * 1000; // 60s
  const OTHER_RUNS_TIMEOUT_MSEC = 10 * 1000; // 10s

  Preferences.reset(PREF_FIRST_RUN);

  let startupTimeout = 0;
  fakeShowPolicyTimeout((callback, timeout) => startupTimeout = timeout, () => {});
  TelemetryReportingPolicy.reset();

  Services.obs.notifyObservers(null, "sessionstore-windows-restored", null);
  Assert.equal(startupTimeout, FIRST_RUN_TIMEOUT_MSEC,
               "The infobar display timeout should be 60s on the first run.");

  // Run again, and check that we actually wait only 10 seconds.
  TelemetryReportingPolicy.reset();
  Services.obs.notifyObservers(null, "sessionstore-windows-restored", null);
  Assert.equal(startupTimeout, OTHER_RUNS_TIMEOUT_MSEC,
               "The infobar display timeout should be 10s on other runs.");
});

add_task(function* test_prefs() {
  TelemetryReportingPolicy.reset();

  let now = fakeNow(2009, 11, 18);

  // If the date is not valid (earlier than 2012), we don't regard the policy as accepted.
  TelemetryReportingPolicy.testInfobarShown();
  Assert.ok(!TelemetryReportingPolicy.testIsUserNotified());
  Assert.equal(Preferences.get(PREF_ACCEPTED_POLICY_DATE, null), 0,
               "Invalid dates should not make the policy accepted.");

  // Check that the notification date and version are correctly saved to the prefs.
  now = fakeNow(2012, 11, 18);
  TelemetryReportingPolicy.testInfobarShown();
  Assert.equal(Preferences.get(PREF_ACCEPTED_POLICY_DATE, null), now.getTime(),
               "A valid date must correctly be saved.");

  // Now that user is notified, check if we are allowed to upload.
  Assert.ok(TelemetryReportingPolicy.canUpload(),
            "We must be able to upload after the policy is accepted.");

  // Disable submission and check that we're no longer allowed to upload.
  Preferences.set(PREF_DATA_SUBMISSION_ENABLED, false);
  Assert.ok(!TelemetryReportingPolicy.canUpload(),
            "We must not be able to upload if data submission is disabled.");

  // Turn the submission back on.
  Preferences.set(PREF_DATA_SUBMISSION_ENABLED, true);
  Assert.ok(TelemetryReportingPolicy.canUpload(),
            "We must be able to upload if data submission is enabled and the policy was accepted.");

  // Set a new minimum policy version and check that user is no longer notified.
  let newMinimum = Preferences.get(PREF_CURRENT_POLICY_VERSION, 1) + 1;
  setMinimumPolicyVersion(newMinimum);
  Assert.ok(!TelemetryReportingPolicy.testIsUserNotified(),
            "A greater minimum policy version must invalidate the policy and disable upload.");

  // Eventually accept the policy and make sure user is notified.
  Preferences.set(PREF_CURRENT_POLICY_VERSION, newMinimum);
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
  Preferences.set(PREF_CURRENT_POLICY_VERSION, newMinimum);
  TelemetryReportingPolicy.testInfobarShown();
  Assert.ok(TelemetryReportingPolicy.testIsUserNotified(),
            "Accepting the policy again should show the user as notified.");
  Assert.ok(TelemetryReportingPolicy.canUpload(),
            "Accepting the policy again should let us upload data.");
});

add_task(function* test_migratePrefs() {
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

add_task(function* test_userNotifiedOfCurrentPolicy() {
  fakeResetAcceptedPolicy();
  TelemetryReportingPolicy.reset();

  // User should be reported as not notified by default.
  Assert.ok(!TelemetryReportingPolicy.testIsUserNotified(),
            "The initial state should be unnotified.");

  // Forcing a policy version should not automatically make the user notified.
  Preferences.set(PREF_ACCEPTED_POLICY_VERSION,
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
    Preferences.get(PREF_CURRENT_POLICY_VERSION, 1) + 1;
  Preferences.set(PREF_ACCEPTED_POLICY_VERSION, newVersion);
  Assert.ok(TelemetryReportingPolicy.testIsUserNotified(),
            "A future version of the policy should pass.");

  newVersion =
    Preferences.get(PREF_CURRENT_POLICY_VERSION, 1) - 1;
  Preferences.set(PREF_ACCEPTED_POLICY_VERSION, newVersion);
  Assert.ok(!TelemetryReportingPolicy.testIsUserNotified(),
            "A previous version of the policy should fail.");
});

add_task(function* test_canSend() {
  const TEST_PING_TYPE = "test-ping";

  PingServer.start();
  Preferences.set(PREF_SERVER, "http://localhost:" + PingServer.port);

  yield TelemetryController.testReset();
  TelemetryReportingPolicy.reset();

  // User should be reported as not notified by default.
  Assert.ok(!TelemetryReportingPolicy.testIsUserNotified(),
            "The initial state should be unnotified.");

  // Assert if we receive any ping before the policy is accepted.
  PingServer.registerPingHandler(() => Assert.ok(false, "Should not have received any pings now"));
  yield TelemetryController.submitExternalPing(TEST_PING_TYPE, {});

  // Reset the ping handler.
  PingServer.resetPingHandler();

  // Fake the infobar: this should also trigger the ping send task.
  TelemetryReportingPolicy.testInfobarShown();
  let ping = yield PingServer.promiseNextPings(1);
  Assert.equal(ping.length, 1, "We should have received one ping.");
  Assert.equal(ping[0].type, TEST_PING_TYPE,
               "We should have received the previous ping.");

  // Submit another ping, to make sure it gets sent.
  yield TelemetryController.submitExternalPing(TEST_PING_TYPE, {});

  // Get the ping and check its type.
  ping = yield PingServer.promiseNextPings(1);
  Assert.equal(ping.length, 1, "We should have received one ping.");
  Assert.equal(ping[0].type, TEST_PING_TYPE, "We should have received the new ping.");

  // Fake a restart with a pending ping.
  yield TelemetryController.addPendingPing(TEST_PING_TYPE, {});
  yield TelemetryController.testReset();

  // We should be immediately sending the ping out.
  ping = yield PingServer.promiseNextPings(1);
  Assert.equal(ping.length, 1, "We should have received one ping.");
  Assert.equal(ping[0].type, TEST_PING_TYPE, "We should have received the pending ping.");

  // Submit another ping, to make sure it gets sent.
  yield TelemetryController.submitExternalPing(TEST_PING_TYPE, {});

  // Get the ping and check its type.
  ping = yield PingServer.promiseNextPings(1);
  Assert.equal(ping.length, 1, "We should have received one ping.");
  Assert.equal(ping[0].type, TEST_PING_TYPE, "We should have received the new ping.");

  yield PingServer.stop();
});
