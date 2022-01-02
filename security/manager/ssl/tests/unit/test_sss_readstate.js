/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// The purpose of this test is to create a site security service state file
// and see that the site security service reads it properly.

var gSSService = null;

function checkStateRead(aSubject, aTopic, aData) {
  if (aData == CLIENT_AUTH_FILE_NAME) {
    return;
  }

  equal(aData, SSS_STATE_FILE_NAME);

  ok(
    !gSSService.isSecureURI(
      Services.io.newURI("https://expired.example.com"),
      0
    )
  );
  ok(
    gSSService.isSecureURI(
      Services.io.newURI("https://notexpired.example.com"),
      0
    )
  );
  ok(
    gSSService.isSecureURI(
      Services.io.newURI("https://includesubdomains.preloaded.test"),
      0
    )
  );
  ok(
    !gSSService.isSecureURI(
      Services.io.newURI("https://sub.includesubdomains.preloaded.test"),
      0
    )
  );
  ok(
    gSSService.isSecureURI(
      Services.io.newURI("https://incsubdomain.example.com"),
      0
    )
  );
  ok(
    gSSService.isSecureURI(
      Services.io.newURI("https://sub.incsubdomain.example.com"),
      0
    )
  );
  ok(
    !gSSService.isSecureURI(
      Services.io.newURI("https://includesubdomains2.preloaded.test"),
      0
    )
  );
  ok(
    !gSSService.isSecureURI(
      Services.io.newURI("https://sub.includesubdomains2.preloaded.test"),
      0
    )
  );

  // Clearing the data should make everything go back to default.
  gSSService.clearAll();
  ok(
    !gSSService.isSecureURI(
      Services.io.newURI("https://expired.example.com"),
      0
    )
  );
  ok(
    !gSSService.isSecureURI(
      Services.io.newURI("https://notexpired.example.com"),
      0
    )
  );
  ok(
    gSSService.isSecureURI(
      Services.io.newURI("https://includesubdomains.preloaded.test"),
      0
    )
  );
  ok(
    gSSService.isSecureURI(
      Services.io.newURI("https://sub.includesubdomains.preloaded.test"),
      0
    )
  );
  ok(
    !gSSService.isSecureURI(
      Services.io.newURI("https://incsubdomain.example.com"),
      0
    )
  );
  ok(
    !gSSService.isSecureURI(
      Services.io.newURI("https://sub.incsubdomain.example.com"),
      0
    )
  );
  ok(
    gSSService.isSecureURI(
      Services.io.newURI("https://includesubdomains2.preloaded.test"),
      0
    )
  );
  ok(
    gSSService.isSecureURI(
      Services.io.newURI("https://sub.includesubdomains2.preloaded.test"),
      0
    )
  );
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
  Services.obs.addObserver(checkStateRead, "data-storage-ready");
  do_test_pending();
  gSSService = Cc["@mozilla.org/ssservice;1"].getService(
    Ci.nsISiteSecurityService
  );
  notEqual(gSSService, null);
}
