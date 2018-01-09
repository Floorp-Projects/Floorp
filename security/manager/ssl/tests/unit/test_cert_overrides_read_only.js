// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
"use strict";

// Tests that permanent certificate error overrides can be added even if the
// certificate/key databases are in read-only mode.

// Helper function for add_read_only_cert_override_test. Probably doesn't need
// to be called directly.
function add_read_only_cert_override(aHost, aExpectedBits, aSecurityInfo) {
  let sslstatus = aSecurityInfo.QueryInterface(Ci.nsISSLStatusProvider)
                               .SSLStatus;
  let bits =
    (sslstatus.isUntrusted ? Ci.nsICertOverrideService.ERROR_UNTRUSTED : 0) |
    (sslstatus.isDomainMismatch ? Ci.nsICertOverrideService.ERROR_MISMATCH : 0) |
    (sslstatus.isNotValidAtThisTime ? Ci.nsICertOverrideService.ERROR_TIME : 0);

  Assert.equal(bits, aExpectedBits,
               "Actual and expected override bits should match");
  let cert = sslstatus.serverCert;
  let certOverrideService = Cc["@mozilla.org/security/certoverride;1"]
                              .getService(Ci.nsICertOverrideService);
  // Setting the last argument to false here ensures that we attempt to store a
  // permanent override (which is what was failing in bug 1427273).
  certOverrideService.rememberValidityOverride(aHost, 8443, cert, aExpectedBits,
                                               false);
}

// Given a host, expected error bits (see nsICertOverrideService.idl), and an
// expected error code, tests that an initial connection to the host fails with
// the expected errors and that adding an override results in a subsequent
// connection succeeding.
function add_read_only_cert_override_test(aHost, aExpectedBits, aExpectedError) {
  add_connection_test(aHost, aExpectedError, null,
                      add_read_only_cert_override.bind(this, aHost, aExpectedBits));
  add_connection_test(aHost, PRErrorCodeSuccess, null, aSecurityInfo => {
    Assert.ok(aSecurityInfo.securityState &
              Ci.nsIWebProgressListener.STATE_CERT_USER_OVERRIDDEN,
              "Cert override flag should be set on the security state");
  });
}

function run_test() {
  let profile = do_get_profile();
  const KEY_DB_NAME = "key4.db";
  const CERT_DB_NAME = "cert9.db";
  let srcKeyDBFile = do_get_file(`test_cert_overrides_read_only/${KEY_DB_NAME}`);
  srcKeyDBFile.copyTo(profile, KEY_DB_NAME);
  let srcCertDBFile = do_get_file(`test_cert_overrides_read_only/${CERT_DB_NAME}`);
  srcCertDBFile.copyTo(profile, CERT_DB_NAME);

  // set the databases to read-only
  let keyDBFile = do_get_profile();
  keyDBFile.append(KEY_DB_NAME);
  keyDBFile.permissions = 0o400;
  let certDBFile = do_get_profile();
  certDBFile.append(CERT_DB_NAME);
  certDBFile.permissions = 0o400;

  Services.prefs.setIntPref("security.OCSP.enabled", 1);
  // Specifying false as the last argument means we don't try to add the default
  // test root CA (which would fail).
  add_tls_server_setup("BadCertServer", "bad_certs", false);

  let fakeOCSPResponder = new HttpServer();
  fakeOCSPResponder.registerPrefixHandler("/", function (request, response) {
    response.setStatusLine(request.httpVersion, 500, "Internal Server Error");
  });
  fakeOCSPResponder.start(8888);

  // Since we can't add the root CA to the (read-only) trust db, all of these
  // will result in an "unknown issuer error" and need the "untrusted" error bit
  // set in addition to whatever other specific error bits are necessary.
  add_read_only_cert_override_test("expired.example.com",
                         Ci.nsICertOverrideService.ERROR_TIME |
                         Ci.nsICertOverrideService.ERROR_UNTRUSTED,
                         SEC_ERROR_UNKNOWN_ISSUER);
  add_read_only_cert_override_test("selfsigned.example.com",
                         Ci.nsICertOverrideService.ERROR_UNTRUSTED,
                         SEC_ERROR_UNKNOWN_ISSUER);
  add_read_only_cert_override_test("mismatch.example.com",
                         Ci.nsICertOverrideService.ERROR_MISMATCH |
                         Ci.nsICertOverrideService.ERROR_UNTRUSTED,
                         SEC_ERROR_UNKNOWN_ISSUER);

  add_test(function () {
    fakeOCSPResponder.stop(run_next_test);
  });

  run_next_test();
}
