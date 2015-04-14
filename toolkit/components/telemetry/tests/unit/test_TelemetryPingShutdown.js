/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that TelemetryPing sends close to shutdown don't lead
// to AsyncShutdown timeouts.

"use strict";

Cu.import("resource://gre/modules/Services.jsm", this);
Cu.import("resource://gre/modules/TelemetryPing.jsm", this);
Cu.import("resource://gre/modules/Timer.jsm", this);
Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);
Cu.import("resource://gre/modules/AsyncShutdown.jsm", this);
Cu.import("resource://testing-common/httpd.js", this);

const PREF_BRANCH = "toolkit.telemetry.";
const PREF_ENABLED = PREF_BRANCH + "enabled";
const PREF_FHR_UPLOAD_ENABLED = "datareporting.healthreport.uploadEnabled";

function contentHandler(metadata, response)
{
  dump("contentHandler called for path: " + metadata._path + "\n");
  // We intentionally don't finish writing the response here to let the
  // client time out.
  response.processAsync();
  response.setHeader("Content-Type", "text/plain");
}

function run_test() {
  // Addon manager needs a profile directory
  do_get_profile();
  loadAddonManager("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  Services.prefs.setBoolPref(PREF_ENABLED, true);
  Services.prefs.setBoolPref(PREF_FHR_UPLOAD_ENABLED, true);

  // Send the needed startup notifications to the datareporting service
  // to ensure that it has been initialized.
  if (HAS_DATAREPORTINGSERVICE) {
    let drs = Cc["@mozilla.org/datareporting/service;1"]
                .getService(Ci.nsISupports)
                .wrappedJSObject;
    drs.observe(null, "app-startup", null);
    drs.observe(null, "profile-after-change", null);
  }

  run_next_test();
}

add_task(function* test_sendTimeout() {
  const TIMEOUT = 100;
  // Enable testing mode for AsyncShutdown, otherwise some testing-only functionality
  // is not available.
  Services.prefs.setBoolPref("toolkit.asyncshutdown.testing", true);
  Services.prefs.setIntPref("toolkit.asyncshutdown.crash_timeout", TIMEOUT);

  let httpServer = new HttpServer();
  httpServer.registerPrefixHandler("/", contentHandler);
  httpServer.start(-1);

  yield TelemetryPing.setup();
  TelemetryPing.setServer("http://localhost:" + httpServer.identity.primaryPort);
  TelemetryPing.send("test-ping-type", {});

  // Trigger the AsyncShutdown phase TelemetryPing hangs off.
  AsyncShutdown.profileBeforeChange._trigger();
  AsyncShutdown.sendTelemetry._trigger();

  // If we get here, we didn't time out in the shutdown routines.
  Assert.ok(true, "Didn't time out on shutdown.");
});
