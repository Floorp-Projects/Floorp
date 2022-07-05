/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

do_get_profile(); // must be done before instantiating nsIX509CertDB

const SECS_IN_A_WEEK = 7 * 24 * 60 * 60 * 1000;
const TESTCASES = [
  {
    hostname: "a.pinning.example.com",
    includeSubdomains: true,
    expireTime: Date.now() + 12 * SECS_IN_A_WEEK * 1000,
  },
  {
    hostname: "b.pinning.example.com",
    includeSubdomains: false,
    expireTime: Date.now() + 13 * SECS_IN_A_WEEK * 1000,
  },
].sort((a, b) => a.expireTime - b.expireTime);

let sss = Cc["@mozilla.org/ssservice;1"].getService(Ci.nsISiteSecurityService);

function getEntries() {
  return Array.from(sss.enumerate());
}

function checkSiteSecurityStateAttrs(entries) {
  entries.sort((a, b) => a.expireTime - b.expireTime);
  equal(
    entries.length,
    TESTCASES.length,
    "Should get correct number of entries"
  );
  for (let i = 0; i < TESTCASES.length; i++) {
    equal(entries[i].hostname, TESTCASES[i].hostname, "Hostnames should match");
    equal(
      entries[i].securityPropertyState,
      Ci.nsISiteSecurityState.SECURITY_PROPERTY_SET,
      "Entries should have security property set"
    );
    equal(
      entries[i].includeSubdomains,
      TESTCASES[i].includeSubdomains,
      "IncludeSubdomains should match"
    );
    // There's a delay from our "now" and the "now" that the implementation uses.
    less(
      Math.abs(entries[i].expireTime - TESTCASES[i].expireTime),
      60000,
      "ExpireTime should be within 60-second error"
    );
  }
}

function add_tests() {
  sss.clearAll();

  for (const testcase of TESTCASES) {
    add_connection_test(
      testcase.hostname,
      PRErrorCodeSuccess,
      undefined,
      function insertEntry(secInfo) {
        const uri = Services.io.newURI(`https://${testcase.hostname}`);

        // MaxAge is in seconds.
        let maxAge = Math.round((testcase.expireTime - Date.now()) / 1000);
        let header = `max-age=${maxAge}`;
        if (testcase.includeSubdomains) {
          header += "; includeSubdomains";
        }
        sss.processHeader(uri, header, secInfo);
      }
    );
  }

  add_task(() => {
    let hstsEntries = getEntries();

    checkSiteSecurityStateAttrs(hstsEntries);

    sss.clearAll();
    hstsEntries = getEntries();

    equal(hstsEntries.length, 0, "Should clear all HSTS entries");
  });
}

function run_test() {
  add_tls_server_setup("BadCertAndPinningServer", "bad_certs");
  add_tests();
  run_next_test();
}
