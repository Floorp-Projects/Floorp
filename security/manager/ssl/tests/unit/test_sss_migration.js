/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// The purpose of this test is to create a site security service state file
// and see that the site security service reads and migrates it properly.

function run_test() {
  let profileDir = do_get_profile();
  let stateFile = profileDir.clone();
  stateFile.append(SSS_STATE_OLD_FILE_NAME);
  // Assuming we're working with a clean slate, the file shouldn't exist
  // until we create it.
  ok(!stateFile.exists());
  let outputStream = FileUtils.openFileOutputStream(stateFile);
  let now = Date.now();
  let lines = [];
  lines.push(
    `no-origin-attributes.example.com:HSTS\t0\t0\t${now + 100000},1,0`
  );
  lines.push(`not-hsts.example.com:HPKP\t0\t0\t${now + 100000},1,0`);
  lines.push(
    `with-port.example.com^partitionKey=%28http%2Cexample.com%2C8443%29:HSTS\t0\t0\t${
      now + 100000
    },1,0`
  );
  for (let i = 0; lines.length < 1024; i++) {
    lines.push(`filler-${i}.example.com:HPKP\t0\t0\t${now + 100000},1,0`);
  }
  writeLinesAndClose(lines, outputStream);
  let sss = Cc["@mozilla.org/ssservice;1"].getService(
    Ci.nsISiteSecurityService
  );
  notEqual(sss, null);

  // nsISiteSecurityService.isSecureURI will block until the backing file is read.
  ok(
    sss.isSecureURI(
      Services.io.newURI("https://no-origin-attributes.example.com")
    )
  );
  ok(!sss.isSecureURI(Services.io.newURI("https://not-hsts.example.com")));
  ok(
    sss.isSecureURI(Services.io.newURI("https://with-port.example.com"), {
      partitionKey: "(http,example.com,8443)",
    })
  );
  ok(
    sss.isSecureURI(Services.io.newURI("https://with-port.example.com"), {
      partitionKey: "(http,example.com)",
    })
  );
  ok(
    sss.isSecureURI(Services.io.newURI("https://with-port.example.com"), {
      partitionKey: "(http,example.com,8000)",
    })
  );
  ok(
    sss.isSecureURI(Services.io.newURI("https://with-port.example.com"), {
      partitionKey: "(https,example.com)",
    })
  );
}
