/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// The purpose of this test is to check that a frequently visited site
// will not be evicted over an infrequently visited site.

var gSSService = null;
var gProfileDir = null;

function do_state_written(aSubject, aTopic, aData) {
  if (aData == PRELOAD_STATE_FILE_NAME) {
    return;
  }

  equal(aData, SSS_STATE_FILE_NAME);

  let stateFile = gProfileDir.clone();
  stateFile.append(SSS_STATE_FILE_NAME);
  ok(stateFile.exists());
  let stateFileContents = readFile(stateFile);
  // the last part is removed because it's the empty string after the final \n
  let lines = stateFileContents.split("\n").slice(0, -1);
  // We can receive multiple data-storage-written events. In particular, we
  // may receive one where DataStorage wrote out data before we were done
  // processing all of our headers. In this case, the data may not be
  // as we expect. We only care about the final one being correct, however,
  // so we return and wait for the next event if things aren't as we expect.
  // There should be 1024 entries.
  if (lines.length != 1024) {
    return;
  }

  let foundLegitSite = false;
  for (let line of lines) {
    if (line.startsWith("frequentlyused.example.com:HSTS")) {
      foundLegitSite = true;
      break;
    }
  }

  ok(foundLegitSite);
  do_test_finished();
}

function do_state_read(aSubject, aTopic, aData) {
  if (aData == PRELOAD_STATE_FILE_NAME) {
    return;
  }

  equal(aData, SSS_STATE_FILE_NAME);

  ok(gSSService.isSecureURI(
       Ci.nsISiteSecurityService.HEADER_HSTS,
       Services.io.newURI("https://frequentlyused.example.com"), 0));
  let sslStatus = new FakeSSLStatus();
  for (let i = 0; i < 2000; i++) {
    let uri = Services.io.newURI("http://bad" + i + ".example.com");
    gSSService.processHeader(Ci.nsISiteSecurityService.HEADER_HSTS, uri,
                            "max-age=1000", sslStatus, 0);
  }
  do_test_pending();
  Services.obs.addObserver(do_state_written, "data-storage-written", false);
  do_test_finished();
}

function run_test() {
  Services.prefs.setIntPref("test.datastorage.write_timer_ms", 100);
  gProfileDir = do_get_profile();
  let stateFile = gProfileDir.clone();
  stateFile.append(SSS_STATE_FILE_NAME);
  // Assuming we're working with a clean slate, the file shouldn't exist
  // until we create it.
  ok(!stateFile.exists());
  let outputStream = FileUtils.openFileOutputStream(stateFile);
  let now = (new Date()).getTime();
  let line = "frequentlyused.example.com:HSTS\t4\t0\t" + (now + 100000) + ",1,0\n";
  outputStream.write(line, line.length);
  outputStream.close();
  Services.obs.addObserver(do_state_read, "data-storage-ready", false);
  do_test_pending();
  gSSService = Cc["@mozilla.org/ssservice;1"]
                 .getService(Ci.nsISiteSecurityService);
  notEqual(gSSService, null);
}
