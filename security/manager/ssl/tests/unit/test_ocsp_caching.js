// -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
"use strict";

let gFetchCount = 0;

function run_test() {
  do_get_profile();
  Services.prefs.setBoolPref("security.ssl.enable_ocsp_stapling", true);
  add_tls_server_setup("OCSPStaplingServer");

  let ocspResponder = new HttpServer();
  ocspResponder.registerPrefixHandler("/", function(request, response) {
    ++gFetchCount;

    do_print("gFetchCount: " + gFetchCount);

    if (gFetchCount != 2) {
      do_print("returning 500 Internal Server Error");

      response.setStatusLine(request.httpVersion, 500, "Internal Server Error");
      let body = "Refusing to return a response";
      response.bodyOutputStream.write(body, body.length);
      return;
    }

    do_print("returning 200 OK");

    let nickname = "localhostAndExampleCom";
    do_print("Generating ocsp response for '" + nickname + "'");
    let args = [ ["good", nickname, "unused" ] ];
    let ocspResponses = generateOCSPResponses(args, "tlsserver");
    let goodResponse = ocspResponses[0];

    response.setStatusLine(request.httpVersion, 200, "OK");
    response.setHeader("Content-Type", "application/ocsp-response");
    response.bodyOutputStream.write(goodResponse, goodResponse.length);
  });
  ocspResponder.start(8080);

  // This test assumes that OCSPStaplingServer uses the same cert for
  // ocsp-stapling-unknown.example.com and ocsp-stapling-none.example.com.

  // Get an Unknown response for the *.exmaple.com cert and put it in the
  // OCSP cache.
  add_connection_test("ocsp-stapling-unknown.example.com",
                      getXPCOMStatusFromNSS(SEC_ERROR_OCSP_UNKNOWN_CERT),
                      clearSessionCache);
  add_test(function() { do_check_eq(gFetchCount, 0); run_next_test(); });

  // A failure to retrieve an OCSP response must result in the cached Unkown
  // response being recognized and honored.
  add_connection_test("ocsp-stapling-none.example.com",
                      getXPCOMStatusFromNSS(SEC_ERROR_OCSP_UNKNOWN_CERT),
                      clearSessionCache);
  add_test(function() { do_check_eq(gFetchCount, 1); run_next_test(); });

  // A valid Good response from the OCSP responder must override the cached
  // Unknown response.
  //
  // Note that We need to make sure that the Unknown response and the Good
  // response have different thisUpdate timestamps; otherwise, the Good
  // response will be seen as "not newer" and it won't replace the existing
  // entry.
  add_test(function() {
    let duration = 1200;
    do_print("Sleeping for " + duration + "ms");
    let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    timer.initWithCallback(run_next_test, duration, Ci.nsITimer.TYPE_ONE_SHOT);
  });
  add_connection_test("ocsp-stapling-none.example.com", Cr.NS_OK,
                      clearSessionCache);
  add_test(function() { do_check_eq(gFetchCount, 2); run_next_test(); });

  // The Good response retrieved from the previous fetch must have replaced
  // the Unknown response in the cache, resulting in the catched Good response
  // being returned and no fetch.
  add_connection_test("ocsp-stapling-none.example.com", Cr.NS_OK,
                      clearSessionCache);
  add_test(function() { do_check_eq(gFetchCount, 2); run_next_test(); });


  //---------------------------------------------------------------------------

  // Reset state
  add_test(function() { clearOCSPCache(); gFetchCount = 0; run_next_test(); });

  // A failure to retrieve an OCSP response will result in an error entry being
  // added to the cache.
  add_connection_test("ocsp-stapling-none.example.com", Cr.NS_OK,
                      clearSessionCache);
  add_test(function() { do_check_eq(gFetchCount, 1); run_next_test(); });

  // The error entry will prevent a fetch from happening for a while.
  add_connection_test("ocsp-stapling-none.example.com", Cr.NS_OK,
                      clearSessionCache);
  add_test(function() { do_check_eq(gFetchCount, 1); run_next_test(); });

  // The error entry must not prevent a stapled OCSP response from being
  // honored.
  add_connection_test("ocsp-stapling-revoked.example.com",
                      getXPCOMStatusFromNSS(SEC_ERROR_REVOKED_CERTIFICATE),
                      clearSessionCache);
  add_test(function() { do_check_eq(gFetchCount, 1); run_next_test(); });

  //---------------------------------------------------------------------------

  add_test(function() { ocspResponder.stop(run_next_test); run_next_test(); });

  run_next_test();
}
