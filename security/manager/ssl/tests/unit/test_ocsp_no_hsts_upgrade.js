// -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
"use strict";

// Test that if an OCSP request is made to a domain that (erroneously)
// has HSTS status, the request is not upgraded from HTTP to HTTPS.

function run_test() {
  do_get_profile();
  // OCSP required means this test will only pass if the request succeeds.
  Services.prefs.setBoolPref("security.OCSP.require", true);

  // We don't actually make use of stapling in this test. This is just how we
  // get a TLS connection.
  add_tls_server_setup("OCSPStaplingServer");

  let args = [["good", "localhostAndExampleCom", "unused"]];
  let ocspResponses = generateOCSPResponses(args, "tlsserver");
  let goodOCSPResponse = ocspResponses[0];

  let ocspResponder = new HttpServer();
  ocspResponder.registerPrefixHandler("/", function (request, response) {
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.setHeader("Content-Type", "application/ocsp-response");
    response.write(goodOCSPResponse);
  });
  ocspResponder.start(8080);

  add_tests_in_mode(true);
  add_tests_in_mode(false);

  add_test(function () { ocspResponder.stop(run_next_test); });

  let SSService = Cc["@mozilla.org/ssservice;1"]
                    .getService(Ci.nsISiteSecurityService);
  let uri = Services.io.newURI("http://localhost", null, null);
  SSService.processHeader(Ci.nsISiteSecurityService.HEADER_HSTS, uri,
                          "max-age=10000", 0);
  do_check_true(SSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HSTS,
                                       "localhost", 0));

  run_next_test();
}

function add_tests_in_mode(useMozillaPKIX) {
  add_test(function () {
    Services.prefs.setBoolPref("security.use_mozillapkix_verification",
                               useMozillaPKIX);
    run_next_test();
  });

  // ocsp-stapling-none.example.com does not staple an OCSP response in the
  // handshake, so the revocation checking code will attempt to fetch one.
  // Since the domain of the certificate's OCSP AIA URI is an HSTS host
  // (as added in the setup of this test), a buggy implementation would
  // upgrade the OCSP request to HTTPS. We specifically prevent this. This
  // test demonstrates that our implementation is correct in this regard.
  add_connection_test("ocsp-stapling-none.example.com", Cr.NS_OK);
  add_test(function () { run_next_test(); });
}
