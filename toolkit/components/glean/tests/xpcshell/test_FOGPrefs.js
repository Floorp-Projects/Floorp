/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TELEMETRY_SERVER_PREF = "toolkit.telemetry.server";
const UPLOAD_PREF = "datareporting.healthreport.uploadEnabled";
const LOCALHOST_PREF = "telemetry.fog.test.localhost_port";

// FOG needs a profile directory to put its data in.
do_get_profile();

// We want Glean to use a localhost server so we can be SURE not to send data to the outside world.
// Yes, the port spells GLEAN on a T9 keyboard, why do you ask?
Services.prefs.setIntPref(LOCALHOST_PREF, 45326);
// We need to initialize it once, otherwise operations will be stuck in the pre-init queue.
Services.fog.initializeFOG();

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

  const value = 42;
  Glean.testOnly.meaningOfLife.set(value);
  // We specifically look at the custom ping's value because we know it
  // won't be reset by being sent.
  Assert.equal(value, Glean.testOnly.meaningOfLife.testGetValue("test-ping"));

  // Despite upload being disabled, we keep the old values around.
  Services.prefs.setBoolPref(UPLOAD_PREF, false);
  Assert.equal(value, Glean.testOnly.meaningOfLife.testGetValue("test-ping"));

  // Now, when we turn the fake upload off, we clear the stores
  Services.prefs.setIntPref(LOCALHOST_PREF, 0);
  Assert.equal(
    undefined,
    Glean.testOnly.meaningOfLife.testGetValue("test-ping")
  );
});
