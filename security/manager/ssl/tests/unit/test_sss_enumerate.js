/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

do_get_profile(); // must be done before instantiating nsIX509CertDB

// This had better not be larger than the maximum maxAge for HPKP.
const NON_ISSUED_KEY_HASH = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=";
const PINNING_ROOT_KEY_HASH = "VCIlmPM9NkgFQtrs4Oa5TeFcDu6MWRTKSNdePEhOgD8=";
const KEY_HASHES = [NON_ISSUED_KEY_HASH, PINNING_ROOT_KEY_HASH];
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

function getEntries(type) {
  return Array.from(sss.enumerate(type));
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

function checkSha256Keys(hpkpEntries) {
  for (let hpkpEntry of hpkpEntries) {
    let keys = Array.from(
      hpkpEntry.QueryInterface(Ci.nsISiteHPKPState).sha256Keys,
      key => key.QueryInterface(Ci.nsIVariant)
    );

    equal(keys.length, KEY_HASHES.length, "Should get correct number of keys");
    keys.sort();
    for (let i = 0; i < KEY_HASHES.length; i++) {
      equal(keys[i], KEY_HASHES[i], "Should get correct keys");
    }
  }
}

registerCleanupFunction(() => {
  Services.prefs.clearUserPref(
    "security.cert_pinning.process_headers_from_non_builtin_roots"
  );
  Services.prefs.clearUserPref("security.cert_pinning.max_max_age_seconds");
});

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
        sss.processHeader(
          Ci.nsISiteSecurityService.HEADER_HSTS,
          uri,
          header,
          secInfo,
          0,
          Ci.nsISiteSecurityService.SOURCE_ORGANIC_REQUEST
        );
        for (let key of KEY_HASHES) {
          header += `; pin-sha256="${key}"`;
        }
        sss.processHeader(
          Ci.nsISiteSecurityService.HEADER_HPKP,
          uri,
          header,
          secInfo,
          0,
          Ci.nsISiteSecurityService.SOURCE_ORGANIC_REQUEST
        );
      }
    );
  }

  add_task(() => {
    let hstsEntries = getEntries(Ci.nsISiteSecurityService.HEADER_HSTS);
    let hpkpEntries = getEntries(Ci.nsISiteSecurityService.HEADER_HPKP);

    checkSiteSecurityStateAttrs(hstsEntries);
    checkSiteSecurityStateAttrs(hpkpEntries);

    checkSha256Keys(hpkpEntries);

    sss.clearAll();
    hstsEntries = getEntries(Ci.nsISiteSecurityService.HEADER_HSTS);
    hpkpEntries = getEntries(Ci.nsISiteSecurityService.HEADER_HPKP);

    equal(hstsEntries.length, 0, "Should clear all HSTS entries");
    equal(hpkpEntries.length, 0, "Should clear all HPKP entries");
  });
}

function run_test() {
  Services.prefs.setBoolPref(
    "security.cert_pinning.process_headers_from_non_builtin_roots",
    true
  );
  Services.prefs.setIntPref(
    "security.cert_pinning.max_max_age_seconds",
    20 * SECS_IN_A_WEEK
  );

  add_tls_server_setup("BadCertAndPinningServer", "bad_certs");

  add_tests();

  run_next_test();
}
