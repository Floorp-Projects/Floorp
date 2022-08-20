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

// This helps us away from test timed out. If service worker manager(swm) hasn't
// been initilaized before profile-change-teardown, this test would fail due to
// the shutdown blocker added by swm. Normally, swm should be initialized before
// that and the similar crash signatures are fixed. So, assume this cannot
// happen in the real world and initilaize swm here as a workaround.
Cc["@mozilla.org/serviceworkers/manager;1"].getService(
  Ci.nsIServiceWorkerManager
);

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
  let secInfo = Cc[
    "@mozilla.org/security/transportsecurityinfo;1"
  ].createInstance(Ci.nsITransportSecurityInfo);
  let header = "max-age=50000";
  SSService.processHeader(
    Services.io.newURI("http://example.com"),
    header,
    secInfo
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
  Services.startup.advanceShutdownPhase(
    Services.startup.SHUTDOWN_PHASE_APPSHUTDOWNTEARDOWN
  );
  Services.startup.advanceShutdownPhase(
    Services.startup.SHUTDOWN_PHASE_APPSHUTDOWN
  );

  equal(getStateFileContents(), "", "state file should be empty");
});
