/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.importGlobalProperties(["Glean", "GleanPings"]);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

// FOG needs a profile directory to put its data in.
do_get_profile();

const FOG = Cc["@mozilla.org/toolkit/glean;1"].createInstance(Ci.nsIFOG);
// We need to initialize it once, otherwise operations will be stuck in the pre-init queue.
FOG.initializeFOG();

const TELEMETRY_SERVER_PREF = "toolkit.telemetry.server";
const UPLOAD_PREF = "datareporting.healthreport.uploadEnabled";
const LOCALHOST_PREF = "telemetry.fog.test.localhost_port";

add_task(function test_fog_upload_only() {
  // Don't forget to point the telemetry server to localhost, or Telemetry
  // might make a non-local connection during the test run.
  Services.prefs.setStringPref(
    TELEMETRY_SERVER_PREF,
    "http://localhost/telemetry-fake/"
  );
  // Be sure to set port=-1 for faking success _before_ enabling upload.
  // Or else there's a short window where we might send something.
  Services.prefs.setIntPref(LOCALHOST_PREF, -1);
  Services.prefs.setBoolPref(UPLOAD_PREF, true);

  // Ensure we don't send "test-ping".
  GleanPings.testPing.testBeforeNextSubmit(() => {
    Assert.ok(false, "Ping 'test-ping' must not be sent.");
  });

  const value = 42;
  Glean.testOnly.meaningOfLife.set(value);
  // We specifically look at the custom ping's value because we know it
  // won't be reset by being sent.
  Assert.equal(value, Glean.testOnly.meaningOfLife.testGetValue("test-ping"));

  // Despite upload being disabled, we keep the old values around.
  Services.prefs.setBoolPref(UPLOAD_PREF, false);
  Assert.equal(value, Glean.testOnly.meaningOfLife.testGetValue("test-ping"));

  // Now this is the part where we'd test the behaviour of "when we turn upload
  // back on and turn fake upload off, we remember to delete any stored data".
  // Except, when we disabled upload we triggered a "deletion-request" ping.
  // If we turn fake upload off, it'll get sent and crash the test runner.

  // Thus, we'll test that in the next test instead of in this one.

  // Cleanup: Replace the asserting callback with a noop.
  GleanPings.testPing.testBeforeNextSubmit(() => {});
});

add_task(function test_fog_deletes_upload_only_data() {
  Services.prefs.setIntPref(LOCALHOST_PREF, -1);

  // Ensure we don't send "test-ping".
  GleanPings.testPing.testBeforeNextSubmit(() => {
    Assert.ok(false, "Ping 'test-ping' must not be sent.");
  });

  const value = 42 * 2; // It's the second time we're using it in this file.
  Glean.testOnly.meaningOfLife.set(value);
  // We specifically look at the custom ping's value because we know it
  // won't be reset by being sent.
  Assert.equal(value, Glean.testOnly.meaningOfLife.testGetValue("test-ping"));

  // Now, when we turn the fake upload off, we clear the stores
  Services.prefs.setIntPref(LOCALHOST_PREF, 0);
  Assert.equal(
    undefined,
    Glean.testOnly.meaningOfLife.testGetValue("test-ping")
  );
});
