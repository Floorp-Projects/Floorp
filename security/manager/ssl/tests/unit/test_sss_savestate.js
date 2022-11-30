/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// The purpose of this test is to see that the site security service properly
// writes its state file.

const EXPECTED_ENTRIES = 5;
const EXPECTED_HSTS_COLUMNS = 4;

var gProfileDir = null;
var gExpectingWrites = true;

const NON_ISSUED_KEY_HASH = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=";

// For reference, the format of the state file is a list of:
// <domain name> <expiration time in milliseconds>,<sts status>,<includeSubdomains>
// separated by newlines ('\n')

function checkStateWritten(aSubject, aTopic, aData) {
  if (aData == CLIENT_AUTH_FILE_NAME) {
    return;
  }

  equal(aData, SSS_STATE_FILE_NAME);
  ok(gExpectingWrites);

  let stateFile = gProfileDir.clone();
  stateFile.append(SSS_STATE_FILE_NAME);
  ok(stateFile.exists());
  let stateFileContents = readFile(stateFile);
  // the last line is removed because it's just a trailing newline
  let lines = stateFileContents.split("\n").slice(0, -1);
  equal(lines.length, EXPECTED_ENTRIES);
  let sites = {}; // a map of domain name -> [the entry in the state file]
  for (let line of lines) {
    let parts = line.split("\t");
    let host = parts[0];
    let entry = parts[3].split(",");
    let expectedColumns = EXPECTED_HSTS_COLUMNS;
    equal(entry.length, expectedColumns);
    sites[host] = entry;
  }

  // While we're still processing headers, multiple writes of the backing data
  // can be scheduled, and thus we can receive multiple data-storage-written
  // notifications. In these cases, the data may not be as we expect. We only
  // care about the final one being correct, however, so we return and wait for
  // the next event if things aren't as we expect.
  // each sites[url][1] should be SecurityPropertySet (i.e. 1).
  // sites[url][2] corresponds to includeSubdomains, so every other one should
  // be set (i.e. 1);
  if (sites["includesubdomains.preloaded.test:HSTS"][1] != 1) {
    return;
  }
  if (sites["includesubdomains.preloaded.test:HSTS"][2] != 0) {
    return;
  }
  if (sites["a.example.com:HSTS"][1] != 1) {
    return;
  }
  if (sites["a.example.com:HSTS"][2] != 1) {
    return;
  }
  if (sites["b.example.com:HSTS"][1] != 1) {
    return;
  }
  if (sites["b.example.com:HSTS"][2] != 0) {
    return;
  }
  if (sites["c.c.example.com:HSTS"][1] != 1) {
    return;
  }
  if (sites["c.c.example.com:HSTS"][2] != 1) {
    return;
  }
  if (sites["d.example.com:HSTS"][1] != 1) {
    return;
  }
  if (sites["d.example.com:HSTS"][2] != 0) {
    return;
  }

  // If we get here, the file was as expected and we no longer expect any
  // data-storage-written notifications.
  gExpectingWrites = false;

  // Process the headers again to test that seeing them again after such a
  // short delay doesn't cause another write.
  process_headers();

  // Wait a bit before finishing the test, to see if another write happens.
  do_timeout(2000, function() {
    do_test_finished();
  });
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
    let secInfo = Cc[
      "@mozilla.org/security/transportsecurityinfo;1"
    ].createInstance(Ci.nsITransportSecurityInfo);
    SSService.processHeader(
      uris[uriIndex],
      maxAge + includeSubdomains,
      secInfo,
      Ci.nsISiteSecurityService.SOURCE_ORGANIC_REQUEST
    );
  }
}

function run_test() {
  Services.prefs.setIntPref("test.datastorage.write_timer_ms", 100);
  gProfileDir = do_get_profile();
  process_headers();
  do_test_pending();
  Services.obs.addObserver(checkStateWritten, "data-storage-written");
}
