/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// The purpose of this test is to create a site security service state file
// and see that the site security service reads it properly.

function run_test() {
  let stateFile = do_get_profile();
  stateFile.append(SSS_STATE_FILE_NAME);
  // Assuming we're working with a clean slate, the file shouldn't exist
  // until we create it.
  ok(!stateFile.exists());
  let outputStream = FileUtils.openFileOutputStream(stateFile);
  let now = Date.now();
  let keyValuePairs = [
    { key: "expired.example.com", value: `${now - 100000},1,0` },
    { key: "notexpired.example.com", value: `${now + 100000},1,0` },
    // This overrides an entry on the preload list.
    { key: "includesubdomains.preloaded.test", value: `${now + 100000},1,0` },
    { key: "incsubdomain.example.com", value: `${now + 100000},1,1` },
    // This overrides an entry on the preload list.
    { key: "includesubdomains2.preloaded.test", value: "0,2,0" },
  ];
  for (let keyValuePair of keyValuePairs) {
    append_line_to_data_storage_file(
      outputStream,
      1,
      1,
      keyValuePair.key,
      keyValuePair.value
    );
  }
  // Append a line with a bad checksum.
  append_line_to_data_storage_file(
    outputStream,
    1,
    1,
    "badchecksum.example.com",
    `${now + 100000},1,0`,
    24,
    true
  );
  outputStream.close();
  let siteSecurityService = Cc["@mozilla.org/ssservice;1"].getService(
    Ci.nsISiteSecurityService
  );
  notEqual(siteSecurityService, null);

  // The backing data storage will block until the background task that reads
  // the backing file has finished.
  ok(
    !siteSecurityService.isSecureURI(
      Services.io.newURI("https://expired.example.com")
    )
  );
  ok(
    siteSecurityService.isSecureURI(
      Services.io.newURI("https://notexpired.example.com")
    )
  );
  ok(
    siteSecurityService.isSecureURI(
      Services.io.newURI("https://includesubdomains.preloaded.test")
    )
  );
  ok(
    !siteSecurityService.isSecureURI(
      Services.io.newURI("https://sub.includesubdomains.preloaded.test")
    )
  );
  ok(
    siteSecurityService.isSecureURI(
      Services.io.newURI("https://incsubdomain.example.com")
    )
  );
  ok(
    siteSecurityService.isSecureURI(
      Services.io.newURI("https://sub.incsubdomain.example.com")
    )
  );
  ok(
    !siteSecurityService.isSecureURI(
      Services.io.newURI("https://includesubdomains2.preloaded.test")
    )
  );
  ok(
    !siteSecurityService.isSecureURI(
      Services.io.newURI("https://sub.includesubdomains2.preloaded.test")
    )
  );

  // Clearing the data should make everything go back to default.
  siteSecurityService.clearAll();
  ok(
    !siteSecurityService.isSecureURI(
      Services.io.newURI("https://expired.example.com")
    )
  );
  ok(
    !siteSecurityService.isSecureURI(
      Services.io.newURI("https://notexpired.example.com")
    )
  );
  ok(
    siteSecurityService.isSecureURI(
      Services.io.newURI("https://includesubdomains.preloaded.test")
    )
  );
  ok(
    siteSecurityService.isSecureURI(
      Services.io.newURI("https://sub.includesubdomains.preloaded.test")
    )
  );
  ok(
    !siteSecurityService.isSecureURI(
      Services.io.newURI("https://incsubdomain.example.com")
    )
  );
  ok(
    !siteSecurityService.isSecureURI(
      Services.io.newURI("https://sub.incsubdomain.example.com")
    )
  );
  ok(
    siteSecurityService.isSecureURI(
      Services.io.newURI("https://includesubdomains2.preloaded.test")
    )
  );
  ok(
    siteSecurityService.isSecureURI(
      Services.io.newURI("https://sub.includesubdomains2.preloaded.test")
    )
  );
  ok(
    !siteSecurityService.isSecureURI(
      Services.io.newURI("https://badchecksum.example.com")
    )
  );
}
