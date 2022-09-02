// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
"use strict";

// In which we connect to a domain (as faked by a server running locally) and
// start up an OCSP responder (also basically faked) that gives a response with
// a bad signature (and later, an empty response). With security.OCSP.require
// set to true, these connections should fail (but they also shouldn't cause
// assertion failures).

var gOCSPRequestCount = 0;
var gOCSPResponse;

function run_test() {
  do_get_profile();
  Services.prefs.setBoolPref("security.OCSP.require", true);
  Services.prefs.setIntPref("security.OCSP.enabled", 1);

  // We don't actually make use of stapling in this test. This is just how we
  // get a TLS connection.
  add_tls_server_setup("OCSPStaplingServer", "ocsp_certs");

  let args = [["bad-signature", "default-ee", "unused", 0]];
  let ocspResponses = generateOCSPResponses(args, "ocsp_certs");
  // Start by replying with a response with a bad signature.
  gOCSPResponse = ocspResponses[0];

  let ocspResponder = new HttpServer();
  ocspResponder.registerPrefixHandler("/", function(request, response) {
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.setHeader("Content-Type", "application/ocsp-response");
    response.write(gOCSPResponse);
    gOCSPRequestCount++;
  });
  ocspResponder.start(8888);

  add_tests();

  add_test(function() {
    ocspResponder.stop(run_next_test);
  });

  run_next_test();
}

function add_tests() {
  add_connection_test(
    "ocsp-stapling-none.example.com",
    SEC_ERROR_OCSP_BAD_SIGNATURE,
    function() {},
    function(aTransportSecurityInfo) {
      Assert.ok(
        aTransportSecurityInfo.madeOCSPRequests,
        "An OCSP Request should have been made."
      );
    }
  );
  add_connection_test(
    "ocsp-stapling-none.example.com",
    SEC_ERROR_OCSP_BAD_SIGNATURE,
    function() {},
    function(aTransportSecurityInfo) {
      Assert.ok(
        !aTransportSecurityInfo.madeOCSPRequests,
        "An OCSP Request should not have been made."
      );
    }
  );
  add_test(function() {
    equal(
      gOCSPRequestCount,
      1,
      "OCSP request count should be 1 due to OCSP response caching"
    );
    gOCSPRequestCount = 0;
    // Now set the OCSP responder to reply with 200 OK but empty content.
    gOCSPResponse = "";
    clearOCSPCache();
    run_next_test();
  });

  add_connection_test(
    "ocsp-stapling-none.example.com",
    SEC_ERROR_OCSP_MALFORMED_RESPONSE,
    function() {},
    function(aTransportSecurityInfo) {
      Assert.ok(
        aTransportSecurityInfo.madeOCSPRequests,
        "An OCSP Request should have been made."
      );
    }
  );
}
