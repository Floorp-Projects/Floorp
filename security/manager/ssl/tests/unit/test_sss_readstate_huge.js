/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// The purpose of this test is to create an old site security service state
// file that is too large and see that the site security service migrates it to
// the new format properly.

function run_test() {
  let profileDir = do_get_profile();
  let stateFile = profileDir.clone();
  stateFile.append(SSS_STATE_OLD_FILE_NAME);
  // Assuming we're working with a clean slate, the file shouldn't exist
  // until we create it.
  ok(!stateFile.exists());
  let outputStream = FileUtils.openFileOutputStream(stateFile);
  let expiryTime = Date.now() + 100000;
  let lines = [];
  for (let i = 0; i < 10000; i++) {
    // The 0s will all get squashed down into one 0 when they are read.
    // This is just to make the file size large (>2MB).
    lines.push(
      `example${i}.example.com\t` +
        "0000000000000000000000000000000000000000000000000\t" +
        "00000000000000000000000000000000000000\t" +
        `${expiryTime},1,0`
    );
  }
  writeLinesAndClose(lines, outputStream);

  let siteSecurityService = Cc["@mozilla.org/ssservice;1"].getService(
    Ci.nsISiteSecurityService
  );
  notEqual(siteSecurityService, null);

  ok(
    siteSecurityService.isSecureURI(
      Services.io.newURI("https://example0.example.com")
    )
  );
  ok(
    siteSecurityService.isSecureURI(
      Services.io.newURI("https://example423.example.com")
    )
  );
  ok(
    siteSecurityService.isSecureURI(
      Services.io.newURI("https://example1023.example.com")
    )
  );
  ok(
    !siteSecurityService.isSecureURI(
      Services.io.newURI("https://example1024.example.com")
    )
  );
  ok(
    !siteSecurityService.isSecureURI(
      Services.io.newURI("https://example1025.example.com")
    )
  );
  ok(
    !siteSecurityService.isSecureURI(
      Services.io.newURI("https://example9000.example.com")
    )
  );
  ok(
    !siteSecurityService.isSecureURI(
      Services.io.newURI("https://example99999.example.com")
    )
  );
}
