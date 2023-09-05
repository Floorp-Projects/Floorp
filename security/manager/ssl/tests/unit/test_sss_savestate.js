/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// The purpose of this test is to see that the site security service properly
// writes its state file.

ChromeUtils.defineESModuleGetters(this, {
  TestUtils: "resource://testing-common/TestUtils.sys.mjs",
});

const EXPECTED_ENTRIES = 5;
const EXPECTED_HSTS_COLUMNS = 3;

function contents_is_as_expected() {
  // The file consists of a series of [score][last accessed][key][value], where
  // score and last accessed are 2 bytes big-endian, key is 0-padded to 256
  // bytes, and value is 0-padded to 24 bytes.
  // Each score will be 1, and last accessed is some number of days (>255)
  // since the epoch, so there will be 3 non-0 bytes just in front of the key.
  // Splitting by 0 and filtering out zero-length strings will result in a series of
  // [BBBkey1, value1, BBBkey2, value2, ...], where "BBB" are the score and
  // last accessed bytes, which are ignored here.
  let contents = get_data_storage_contents(SSS_STATE_FILE_NAME);
  if (!contents) {
    return false;
  }
  let keysAndValues = contents.split("\0").filter(s => !!s.length);
  let keys = keysAndValues
    .filter((_, i) => i % 2 == 0)
    .map(key => key.substring(3));
  let values = keysAndValues.filter((_, i) => i % 2 == 1);

  if (keys.length != EXPECTED_ENTRIES || values.length != EXPECTED_ENTRIES) {
    return false;
  }

  let sites = {}; // a map of domain name -> [the entry in the state file]
  for (let i in keys) {
    let host = keys[i];
    let entry = values[i].split(",");
    equal(entry.length, EXPECTED_HSTS_COLUMNS);
    sites[host] = entry;
  }

  // each sites[url][1] should be SecurityPropertySet (i.e. 1).
  // sites[url][2] corresponds to includeSubdomains, so every other one should
  // be set (i.e. 1);
  return (
    sites["includesubdomains.preloaded.test"][1] == 1 &&
    sites["includesubdomains.preloaded.test"][2] == 0 &&
    sites["a.example.com"][1] == 1 &&
    sites["a.example.com"][2] == 1 &&
    sites["b.example.com"][1] == 1 &&
    sites["b.example.com"][2] == 0 &&
    sites["c.c.example.com"][1] == 1 &&
    sites["c.c.example.com"][2] == 1 &&
    sites["d.example.com"][1] == 1 &&
    sites["d.example.com"][2] == 0
  );
}

function process_headers() {
  let SSService = Cc["@mozilla.org/ssservice;1"].getService(
    Ci.nsISiteSecurityService
  );

  let uris = [
    Services.io.newURI("http://includesubdomains.preloaded.test"),
    Services.io.newURI("http://a.example.com"),
    Services.io.newURI("http://b.example.com"),
    Services.io.newURI("http://c.c.example.com"),
    Services.io.newURI("http://d.example.com"),
  ];

  for (let i = 0; i < 1000; i++) {
    let uriIndex = i % uris.length;
    // vary max-age, but have it be within one day of one year
    let maxAge = "max-age=" + (i + 31536000);
    // have every other URI set includeSubdomains
    let includeSubdomains = uriIndex % 2 == 1 ? "; includeSubdomains" : "";
    SSService.processHeader(uris[uriIndex], maxAge + includeSubdomains);
  }
}

function run_test() {
  do_get_profile();
  process_headers();
  TestUtils.waitForCondition(contents_is_as_expected);
}
