// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
"use strict";

// In which we connect to a server that staples an OCSP response for a
// certificate signed by an intermediate that has an OCSP AIA to ensure
// that an OCSP request is not made for the intermediate.

var gOCSPRequestCount = 0;

function add_ocsp_test(aHost, aExpectedResult) {
  add_connection_test(aHost, aExpectedResult,
    function() {
      clearOCSPCache();
      clearSessionCache();
    });
}

function run_test() {
  do_get_profile();
  Services.prefs.setBoolPref("security.ssl.enable_ocsp_stapling", true);

  let ocspResponder = new HttpServer();
  ocspResponder.registerPrefixHandler("/", function(request, response) {
    gOCSPRequestCount++;
    response.setStatusLine(request.httpVersion, 500, "Internal Server Error");
    let body = "Refusing to return a response";
    response.bodyOutputStream.write(body, body.length);
  });
  ocspResponder.start(8888);

  add_tls_server_setup("OCSPStaplingServer", "ocsp_certs");

  add_ocsp_test("ocsp-stapling-with-intermediate.example.com",
                PRErrorCodeSuccess);

  add_test(function () { ocspResponder.stop(run_next_test); });
  add_test(function() {
    equal(gOCSPRequestCount, 0, "No OCSP requests should have been made");
    run_next_test();
  });
  run_next_test();
}
