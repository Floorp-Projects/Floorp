/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// The purpose of this test is to create a site security service state file
// and see that the site security service reads it properly. We also verify
// that state changes are reflected in the child process.

function start_test_in_child() {
  run_test_in_child("sss_readstate_child_worker.js");
  do_test_finished();
}

function run_test() {
  let profileDir = do_get_profile();
  let stateFile = profileDir.clone();
  stateFile.append(SSS_STATE_FILE_NAME);
  // Assuming we're working with a clean slate, the file shouldn't exist
  // until we create it.
  ok(!stateFile.exists());
  let outputStream = FileUtils.openFileOutputStream(stateFile);
  let now = Date.now();
  let lines = [
    `expired.example.com:HSTS\t0\t0\t${now - 100000},1,0`,
    `notexpired.example.com:HSTS\t0\t0\t${now + 100000},1,0`,
    // This overrides an entry on the preload list.
    `includesubdomains.preloaded.test:HSTS\t0\t0\t${now + 100000},1,0`,
    `incsubdomain.example.com:HSTS\t0\t0\t${now + 100000},1,1`,
    // This overrides an entry on the preload list.
    "includesubdomains2.preloaded.test:HSTS\t0\t0\t0,2,0",
  ];
  writeLinesAndClose(lines, outputStream);
  Services.obs.addObserver(start_test_in_child, "data-storage-ready");
  do_test_pending();
  let SSService = Cc["@mozilla.org/ssservice;1"].getService(
    Ci.nsISiteSecurityService
  );
  notEqual(SSService, null);
}
