/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// The purpose of this test is to create a site security service state file
// that is too large and see that the site security service reads it properly
// (this means discarding all entries after the 1024th).

function writeLine(aLine, aOutputStream) {
  aOutputStream.write(aLine, aLine.length);
}

var gSSService = null;

function checkStateRead(aSubject, aTopic, aData) {
  equal(aData, SSS_STATE_FILE_NAME);

  ok(gSSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HSTS,
                             "example0.example.com", 0));
  ok(gSSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HSTS,
                             "example423.example.com", 0));
  ok(gSSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HSTS,
                             "example1023.example.com", 0));
  ok(!gSSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HSTS,
                              "example1024.example.com", 0));
  ok(!gSSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HSTS,
                              "example1025.example.com", 0));
  ok(!gSSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HSTS,
                              "example9000.example.com", 0));
  ok(!gSSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HSTS,
                              "example99999.example.com", 0));
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
  let now = (new Date()).getTime();
  for (let i = 0; i < 10000; i++) {
    // The 0s will all get squashed down into one 0 when they are read.
    // This is just to make the file size large (>2MB).
    writeLine("example" + i + ".example.com:HSTS\t0000000000000000000000000000000000000000000000000\t00000000000000000000000000000000000000\t" + (now + 100000) + ",1,0000000000000000000000000000000000000000000000000000000000000000000000000\n", outputStream);
  }
  outputStream.close();
  Services.obs.addObserver(checkStateRead, "data-storage-ready", false);
  do_test_pending();
  gSSService = Cc["@mozilla.org/ssservice;1"]
                 .getService(Ci.nsISiteSecurityService);
  notEqual(gSSService, null);
}
