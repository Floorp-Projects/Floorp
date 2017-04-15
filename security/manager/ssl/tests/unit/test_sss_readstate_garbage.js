/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// The purpose of this test is to create a mostly bogus site security service
// state file and see that the site security service handles it properly.

function writeLine(aLine, aOutputStream) {
  aOutputStream.write(aLine, aLine.length);
}

var gSSService = null;

function checkStateRead(aSubject, aTopic, aData) {
  if (aData == PRELOAD_STATE_FILE_NAME) {
    return;
  }

  equal(aData, SSS_STATE_FILE_NAME);

  ok(gSSService.isSecureURI(Ci.nsISiteSecurityService.HEADER_HSTS,
                            Services.io.newURI("https://example1.example.com"),
                            0));
  ok(gSSService.isSecureURI(Ci.nsISiteSecurityService.HEADER_HSTS,
                            Services.io.newURI("https://example2.example.com"),
                            0));
  ok(!gSSService.isSecureURI(Ci.nsISiteSecurityService.HEADER_HSTS,
                             Services.io.newURI("https://example.com"), 0));
  ok(!gSSService.isSecureURI(Ci.nsISiteSecurityService.HEADER_HSTS,
                             Services.io.newURI("https://example3.example.com"),
                             0));
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
  writeLine("example1.example.com:HSTS\t0\t0\t" + (now + 100000) + ",1,0\n", outputStream);
  writeLine("I'm a lumberjack and I'm okay; I work all night and I sleep all day!\n", outputStream);
  writeLine("This is a totally bogus entry\t\n", outputStream);
  writeLine("0\t0\t0\t0\t\n", outputStream);
  writeLine("\t\t\t\t\t\t\t\n", outputStream);
  writeLine("example.com:HSTS\t\t\t\t\t\t\t\n", outputStream);
  writeLine("example3.example.com:HSTS\t0\t\t\t\t\t\t\n", outputStream);
  writeLine("example2.example.com:HSTS\t0\t0\t" + (now + 100000) + ",1,0\n", outputStream);
  outputStream.close();
  Services.obs.addObserver(checkStateRead, "data-storage-ready");
  do_test_pending();
  gSSService = Cc["@mozilla.org/ssservice;1"]
                 .getService(Ci.nsISiteSecurityService);
  notEqual(gSSService, null);
}
