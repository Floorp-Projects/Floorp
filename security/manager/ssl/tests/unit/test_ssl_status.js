// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
"use strict";

do_get_profile();

function run_test() {
  Services.prefs.setIntPref("security.OCSP.enabled", 1);
  add_tls_server_setup("BadCertServer", "bad_certs");

  let fakeOCSPResponder = new HttpServer();
  fakeOCSPResponder.registerPrefixHandler("/", function (request, response) {
    response.setStatusLine(request.httpVersion, 500, "Internal Server Error");
  });
  fakeOCSPResponder.start(8888);

  // Test successful connection (failedCertChain should be null,
  // succeededCertChain should be set as expected)
  add_connection_test(
    "good.include-subdomains.pinning.example.com", PRErrorCodeSuccess, null,
    function withSecurityInfo(aSSLStatus) {
      let sslstatus = aSSLStatus.QueryInterface(Ci.nsISSLStatusProvider).SSLStatus;
      equal(sslstatus.failedCertChain, null,
            "failedCertChain for a successful connection should be null");
      ok(sslstatus.succeededCertChain.equals(build_cert_chain(["default-ee", "test-ca"])),
            "succeededCertChain for a successful connection should be as expected");
    }
  );

  // Test failed connection (failedCertChain should be set as expected,
  // succeededCertChain should be null)
  add_connection_test(
    "expired.example.com", SEC_ERROR_EXPIRED_CERTIFICATE, null,
    function withSecurityInfo(aSSLStatus) {
      let sslstatus = aSSLStatus.QueryInterface(Ci.nsISSLStatusProvider).SSLStatus;
      equal(sslstatus.succeededCertChain, null,
            "succeededCertChain for a failed connection should be null");
      ok(sslstatus.failedCertChain.equals(build_cert_chain(["expired-ee", "test-ca"])),
            "failedCertChain for a failed connection should be as expected");
    }
  );

  // Ensure the correct failed cert chain is set on cert override
  let overrideStatus = {
    failedCertChain: build_cert_chain(["expired-ee", "test-ca"])
  };
  add_cert_override_test("expired.example.com",
                         Ci.nsICertOverrideService.ERROR_TIME,
                         SEC_ERROR_EXPIRED_CERTIFICATE, undefined,
                         overrideStatus);

  run_next_test();
}
