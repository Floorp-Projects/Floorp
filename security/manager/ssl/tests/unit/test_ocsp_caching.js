// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
"use strict";

// Checks various aspects of the OCSP cache, mainly to to ensure we do not fetch
// responses more than necessary.

var gFetchCount = 0;
var gGoodOCSPResponse = null;
var gResponsePattern = [];
var gMessage= "";

function respondWithGoodOCSP(request, response) {
  do_print("returning 200 OK");
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "application/ocsp-response");
  response.write(gGoodOCSPResponse);
}

function respondWithSHA1OCSP(request, response) {
  do_print("returning 200 OK with sha-1 delegated response");
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "application/ocsp-response");

  let args = [ ["good-delegated", "default-ee", "delegatedSHA1Signer" ] ];
  let responses = generateOCSPResponses(args, "tlsserver");
  response.write(responses[0]);
}

function respondWithError(request, response) {
  do_print("returning 500 Internal Server Error");
  response.setStatusLine(request.httpVersion, 500, "Internal Server Error");
  let body = "Refusing to return a response";
  response.bodyOutputStream.write(body, body.length);
}

function generateGoodOCSPResponse() {
  let args = [ ["good", "default-ee", "unused" ] ];
  let responses = generateOCSPResponses(args, "tlsserver");
  return responses[0];
}

function add_ocsp_test(aHost, aExpectedResult, aResponses, aMessage) {
  add_connection_test(aHost, aExpectedResult,
      function() {
        clearSessionCache();
        gFetchCount = 0;
        gResponsePattern = aResponses;
        gMessage = aMessage;
      },
      function() {
        // check the number of requests matches the size of aResponses
        equal(gFetchCount, aResponses.length,
              "should have made " + aResponses.length +
              " OCSP request" + (aResponses.length == 1 ? "" : "s"));
      });
}

function run_test() {
  do_get_profile();
  Services.prefs.setBoolPref("security.ssl.enable_ocsp_stapling", true);
  Services.prefs.setIntPref("security.OCSP.enabled", 1);
  add_tls_server_setup("OCSPStaplingServer");

  let ocspResponder = new HttpServer();
  ocspResponder.registerPrefixHandler("/", function(request, response) {

    do_print("gFetchCount: " + gFetchCount);
    let responseFunction = gResponsePattern[gFetchCount];
    Assert.notEqual(undefined, responseFunction);

    ++gFetchCount;
    responseFunction(request, response);
  });
  ocspResponder.start(8888);

  add_tests();

  add_test(function() { ocspResponder.stop(run_next_test); });
  run_next_test();
}

function add_tests() {
  // Test that verifying a certificate with a "short lifetime" doesn't result
  // in OCSP fetching. Due to longevity requirements in our testing
  // infrastructure, the certificate we encounter is valid for a very long
  // time, so we have to define a "short lifetime" as something very long.
  add_test(function() {
    Services.prefs.setIntPref("security.pki.cert_short_lifetime_in_days",
                              12000);
    run_next_test();
  });

  add_ocsp_test("ocsp-stapling-none.example.com", PRErrorCodeSuccess, [],
                "expected zero OCSP requests for a short-lived certificate");

  add_test(function() {
    Services.prefs.setIntPref("security.pki.cert_short_lifetime_in_days", 100);
    run_next_test();
  });

  // If a "short lifetime" is something more reasonable, ensure that we do OCSP
  // fetching for this long-lived certificate.

  add_ocsp_test("ocsp-stapling-none.example.com", PRErrorCodeSuccess,
                [respondWithError],
                "expected one OCSP request for a long-lived certificate");
  add_test(function() {
    Services.prefs.clearUserPref("security.pki.cert_short_lifetime_in_days");
    run_next_test();
  });
  //---------------------------------------------------------------------------

  // Reset state
  add_test(function() { clearOCSPCache(); run_next_test(); });

  // This test assumes that OCSPStaplingServer uses the same cert for
  // ocsp-stapling-unknown.example.com and ocsp-stapling-none.example.com.

  // Get an Unknown response for the *.example.com cert and put it in the
  // OCSP cache.
  add_ocsp_test("ocsp-stapling-unknown.example.com",
                SEC_ERROR_OCSP_UNKNOWN_CERT, [],
                "Stapled Unknown response -> a fetch should not have been" +
                " attempted");

  // A failure to retrieve an OCSP response must result in the cached Unknown
  // response being recognized and honored.
  add_ocsp_test("ocsp-stapling-none.example.com", SEC_ERROR_OCSP_UNKNOWN_CERT,
                [
                  respondWithError,
                  respondWithError,
                  respondWithError,
                  respondWithError,
                  respondWithError,
                  respondWithError,
                  respondWithError,
                  respondWithError,
                ],
                "No stapled response -> a fetch should have been attempted");

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
  add_test(function() {
    gGoodOCSPResponse = generateGoodOCSPResponse();
    run_next_test();
  });
  add_ocsp_test("ocsp-stapling-none.example.com", PRErrorCodeSuccess,
                [respondWithGoodOCSP],
                "Cached Unknown response, no stapled response -> a fetch" +
                " should have been attempted");

  // The Good response retrieved from the previous fetch must have replaced
  // the Unknown response in the cache, resulting in the catched Good response
  // being returned and no fetch.
  add_ocsp_test("ocsp-stapling-none.example.com", PRErrorCodeSuccess,
                [],
                "Cached Good response -> a fetch should not have been" +
                " attempted");


  //---------------------------------------------------------------------------

  // Reset state
  add_test(function() { clearOCSPCache(); run_next_test(); });

  // A failure to retrieve an OCSP response will result in an error entry being
  // added to the cache.
  add_ocsp_test("ocsp-stapling-none.example.com", PRErrorCodeSuccess,
                [respondWithError],
                "No stapled response -> a fetch should have been attempted");

  // The error entry will prevent a fetch from happening for a while.
  add_ocsp_test("ocsp-stapling-none.example.com", PRErrorCodeSuccess, [],
                "Noted OCSP server failure -> a fetch should not have been" +
                " attempted");

  // The error entry must not prevent a stapled OCSP response from being
  // honored.
  add_ocsp_test("ocsp-stapling-revoked.example.com",
                SEC_ERROR_REVOKED_CERTIFICATE, [],
                "Stapled Revoked response -> a fetch should not have been" +
                " attempted");

  //---------------------------------------------------------------------------

  // Ensure OCSP responses from signers with SHA1 certificates are OK. This
  // is included in the OCSP caching tests since there were OCSP cache-related
  // regressions when sha-1 telemetry probes were added.
  add_test(function() {
    clearOCSPCache();
    // set security.OCSP.require so that checking the OCSP signature fails
    Services.prefs.setBoolPref("security.OCSP.require", true);
    run_next_test();
  });

  add_ocsp_test("ocsp-stapling-none.example.com", PRErrorCodeSuccess,
                [respondWithSHA1OCSP],
                "signing cert is good (though sha1) - should succeed");

  add_test(function() {
    Services.prefs.setBoolPref("security.OCSP.require", false);
    run_next_test();
  });

  //---------------------------------------------------------------------------

  // Reset state
  add_test(function() { clearOCSPCache(); run_next_test(); });
}
