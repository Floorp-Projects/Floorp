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

const UPLOAD_PREF = "datareporting.healthreport.uploadEnabled";
const LOCALHOST_PREF = "telemetry.fog.test.localhost_port";

add_task(function test_fog_upload_only() {
  Services.prefs.setBoolPref(UPLOAD_PREF, true);
  Services.prefs.setIntPref(LOCALHOST_PREF, -1);

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

  // But when local loopback is disabled, we clear the stores.
  Services.prefs.setIntPref(LOCALHOST_PREF, 0);
  Assert.equal(
    undefined,
    Glean.testOnly.meaningOfLife.testGetValue("test-ping")
  );

  // Replace the asserting callback with a noop.
  GleanPings.testPing.testBeforeNextSubmit(() => {});
});
