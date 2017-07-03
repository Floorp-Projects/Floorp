/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that TelemetryController sends close to shutdown don't lead
// to AsyncShutdown timeouts.

"use strict";

Cu.import("resource://gre/modules/Services.jsm", this);
Cu.import("resource://gre/modules/TelemetryController.jsm", this);
Cu.import("resource://gre/modules/TelemetrySend.jsm", this);
Cu.import("resource://gre/modules/Timer.jsm", this);
Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);
Cu.import("resource://gre/modules/AsyncShutdown.jsm", this);
Cu.import("resource://testing-common/httpd.js", this);

const PREF_BRANCH = "toolkit.telemetry.";
const PREF_FHR_UPLOAD_ENABLED = "datareporting.healthreport.uploadEnabled";

function contentHandler(metadata, response) {
  dump("contentHandler called for path: " + metadata._path + "\n");
  // We intentionally don't finish writing the response here to let the
  // client time out.
  response.processAsync();
  response.setHeader("Content-Type", "text/plain");
}

add_task(async function test_setup() {
  // Addon manager needs a profile directory
  do_get_profile();
  loadAddonManager("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");
  // Make sure we don't generate unexpected pings due to pref changes.
  await setEmptyPrefWatchlist();

  Services.prefs.setBoolPref(PREF_TELEMETRY_ENABLED, true);
  Services.prefs.setBoolPref(PREF_FHR_UPLOAD_ENABLED, true);
});

/**
 * Ensures that TelemetryController does not hang processing shutdown
 * phases. Assumes that Telemetry shutdown routines do not take longer than
 * CRASH_TIMEOUT_MS to complete.
 */
add_task(async function test_sendTelemetryShutsDownWithinReasonableTimeout() {
  const CRASH_TIMEOUT_MS = 10 * 1000;
  // Enable testing mode for AsyncShutdown, otherwise some testing-only functionality
  // is not available.
  Services.prefs.setBoolPref("toolkit.asyncshutdown.testing", true);
  // Reducing the max delay for waitiing on phases to complete from 1 minute
  // (standard) to 20 seconds to avoid blocking the tests in case of misbehavior.
  Services.prefs.setIntPref("toolkit.asyncshutdown.crash_timeout", CRASH_TIMEOUT_MS);

  let httpServer = new HttpServer();
  httpServer.registerPrefixHandler("/", contentHandler);
  httpServer.start(-1);

  await TelemetryController.testSetup();
  TelemetrySend.setServer("http://localhost:" + httpServer.identity.primaryPort);
  let submissionPromise = TelemetryController.submitExternalPing("test-ping-type", {});

  // Trigger the AsyncShutdown phase TelemetryController hangs off.
  AsyncShutdown.profileBeforeChange._trigger();
  AsyncShutdown.sendTelemetry._trigger();
  // Now wait for the ping submission.
  await submissionPromise;

  // If we get here, we didn't time out in the shutdown routines.
  Assert.ok(true, "Didn't time out on shutdown.");
});
