/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// The purpose of this test is to check that a frequently visited site
// will not be evicted over an infrequently visited site.
function run_test() {
  let stateFile = do_get_profile();
  stateFile.append(SSS_STATE_FILE_NAME);
  // Assuming we're working with a clean slate, the file shouldn't exist
  // until we create it.
  ok(!stateFile.exists());
  let outputStream = FileUtils.openFileOutputStream(stateFile);
  let now = new Date().getTime();
  let key = "frequentlyused.example.com";
  let value = `${now + 100000},1,0`;
  append_line_to_data_storage_file(outputStream, 4, 1000, key, value);
  outputStream.close();
  let siteSecurityService = Cc["@mozilla.org/ssservice;1"].getService(
    Ci.nsISiteSecurityService
  );
  notEqual(siteSecurityService, null);
  // isSecureURI blocks until the backing data is read.
  ok(
    siteSecurityService.isSecureURI(
      Services.io.newURI("https://frequentlyused.example.com")
    )
  );
  // The storage limit is currently 2048, so this should cause evictions.
  for (let i = 0; i < 3000; i++) {
    let uri = Services.io.newURI("http://bad" + i + ".example.com");
    siteSecurityService.processHeader(uri, "max-age=1000");
  }
  // The frequently used entry should not be evicted.
  ok(
    siteSecurityService.isSecureURI(
      Services.io.newURI("https://frequentlyused.example.com")
    )
  );
}
