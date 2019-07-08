/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// The purpose of this test is to ensure that Firefox sanitizes site security
// service data on shutdown if configured to do so.

XPCOMUtils.defineLazyModuleGetters(this, {
  TestUtils: "resource://testing-common/TestUtils.jsm",
  Sanitizer: "resource:///modules/Sanitizer.jsm",
});

Sanitizer.onStartup();

function getStateFileContents() {
  let stateFile = do_get_profile();
  stateFile.append(SSS_STATE_FILE_NAME);
  ok(stateFile.exists());
  return readFile(stateFile);
}

add_task(async function run_test() {
  Services.prefs.setIntPref("test.datastorage.write_timer_ms", 100);
  do_get_profile();
  let SSService = Cc["@mozilla.org/ssservice;1"].getService(
    Ci.nsISiteSecurityService
  );
  let secInfo = new FakeTransportSecurityInfo();
  let header = "max-age=50000";
  SSService.processHeader(
    Ci.nsISiteSecurityService.HEADER_HSTS,
    Services.io.newURI("http://example.com"),
    header,
    secInfo,
    0,
    Ci.nsISiteSecurityService.SOURCE_ORGANIC_REQUEST
  );
  await TestUtils.topicObserved(
    "data-storage-written",
    (_, data) => data == SSS_STATE_FILE_NAME
  );
  let stateFileContents = getStateFileContents();
  ok(
    stateFileContents.includes("example.com"),
    "should have written out state file"
  );

  // Configure Firefox to clear this data on shutdown.
  Services.prefs.setBoolPref(
    Sanitizer.PREF_SHUTDOWN_BRANCH + "siteSettings",
    true
  );
  Services.prefs.setBoolPref(Sanitizer.PREF_SANITIZE_ON_SHUTDOWN, true);

  // Simulate shutdown.
  Services.obs.notifyObservers(null, "profile-change-teardown");
  Services.obs.notifyObservers(null, "profile-before-change");

  equal(getStateFileContents(), "", "state file should be empty");
});
