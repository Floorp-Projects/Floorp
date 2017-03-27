/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// The purpose of this test is to create an empty site security service state
// file and see that the site security service doesn't fail when reading it.

var gSSService = null;

function checkStateRead(aSubject, aTopic, aData) {
  // nonexistent.example.com should never be an HSTS host
  ok(!gSSService.isSecureURI(
       Ci.nsISiteSecurityService.HEADER_HSTS,
       Services.io.newURI("https://nonexistent.example.com"), 0));
  ok(gSSService.isSecureURI(Ci.nsISiteSecurityService.HEADER_HSTS,
                            Services.io.newURI("https://includesubdomains.preloaded.test"),
                            0));
  // notexpired.example.com is an HSTS host in a different test - we
  // want to make sure that test hasn't interfered with this one.
  ok(!gSSService.isSecureURI(
       Ci.nsISiteSecurityService.HEADER_HSTS,
       Services.io.newURI("https://notexpired.example.com"), 0));
  do_test_finished();
}

function run_test() {
  let profileDir = do_get_profile();
  let stateFile = profileDir.clone();
  stateFile.append(SSS_STATE_FILE_NAME);
  // Assuming we're working with a clean slate, the file shouldn't exist
  // until we create it.
  ok(!stateFile.exists());
  stateFile.create(Ci.nsIFile.NORMAL_FILE_TYPE, 0x1a4); // 0x1a4 == 0o644
  ok(stateFile.exists());
  // Initialize nsISiteSecurityService after do_get_profile() so it
  // can read the state file.
  Services.obs.addObserver(checkStateRead, "data-storage-ready", false);
  do_test_pending();
  gSSService = Cc["@mozilla.org/ssservice;1"]
                 .getService(Ci.nsISiteSecurityService);
  notEqual(gSSService, null);
}
