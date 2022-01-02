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
var gMessage = "";

function respondWithGoodOCSP(request, response) {
  info("returning 200 OK");
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "application/ocsp-response");
  response.write(gGoodOCSPResponse);
}

function respondWithSHA1OCSP(request, response) {
  info("returning 200 OK with sha-1 delegated response");
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "application/ocsp-response");

  let args = [["good-delegated", "default-ee", "delegatedSHA1Signer", 0]];
  let responses = generateOCSPResponses(args, "ocsp_certs");
  response.write(responses[0]);
}

function respondWithError(request, response) {
  info("returning 500 Internal Server Error");
  response.setStatusLine(request.httpVersion, 500, "Internal Server Error");
  let body = "Refusing to return a response";
  response.bodyOutputStream.write(body, body.length);
}

function generateGoodOCSPResponse(thisUpdateSkew) {
  let args = [["good", "default-ee", "unused", thisUpdateSkew]];
  let responses = generateOCSPResponses(args, "ocsp_certs");
  return responses[0];
}

function add_ocsp_test(
  aHost,
  aExpectedResult,
  aResponses,
  aMessage,
  aOriginAttributes
) {
  add_connection_test(
    aHost,
    aExpectedResult,
    function() {
      clearSessionCache();
      gFetchCount = 0;
      gResponsePattern = aResponses;
      gMessage = aMessage;
    },
    function() {
      // check the number of requests matches the size of aResponses
      equal(
        gFetchCount,
        aResponses.length,
        "should have made " +
          aResponses.length +
          " OCSP request" +
          (aResponses.length == 1 ? "" : "s")
      );
    },
    null,
    aOriginAttributes
  );
}

function run_test() {
  do_get_profile();
  Services.prefs.setBoolPref("security.ssl.enable_ocsp_stapling", true);
  Services.prefs.setIntPref("security.OCSP.enabled", 1);
  Services.prefs.setIntPref("security.pki.sha1_enforcement_level", 4);
  add_tls_server_setup("OCSPStaplingServer", "ocsp_certs");

  let ocspResponder = new HttpServer();
  ocspResponder.registerPrefixHandler("/", function(request, response) {
    info("gFetchCount: " + gFetchCount);
    let responseFunction = gResponsePattern[gFetchCount];
    Assert.notEqual(undefined, responseFunction);

    ++gFetchCount;
    responseFunction(request, response);
  });
  ocspResponder.start(8888);

  add_tests();

  add_test(function() {
    ocspResponder.stop(run_next_test);
  });
  run_next_test();
}

function add_tests() {
  // Test that verifying a certificate with a "short lifetime" doesn't result
  // in OCSP fetching. Due to longevity requirements in our testing
  // infrastructure, the certificate we encounter is valid for a very long
  // time, so we have to define a "short lifetime" as something very long.
  add_test(function() {
    Services.prefs.setIntPref(
      "security.pki.cert_short_lifetime_in_days",
      12000
    );
    run_next_test();
  });

  add_ocsp_test(
    "ocsp-stapling-none.example.com",
    PRErrorCodeSuccess,
    [],
    "expected zero OCSP requests for a short-lived certificate"
  );

  add_test(function() {
    Services.prefs.setIntPref("security.pki.cert_short_lifetime_in_days", 100);
    run_next_test();
  });

  // If a "short lifetime" is something more reasonable, ensure that we do OCSP
  // fetching for this long-lived certificate.

  add_ocsp_test(
    "ocsp-stapling-none.example.com",
    PRErrorCodeSuccess,
    [respondWithError],
    "expected one OCSP request for a long-lived certificate"
  );
  add_test(function() {
    Services.prefs.clearUserPref("security.pki.cert_short_lifetime_in_days");
    run_next_test();
  });
  // ---------------------------------------------------------------------------

  // Reset state
  add_test(function() {
    clearOCSPCache();
    run_next_test();
  });

  // This test assumes that OCSPStaplingServer uses the same cert for
  // ocsp-stapling-unknown.example.com and ocsp-stapling-none.example.com.

  // Get an Unknown response for the *.example.com cert and put it in the
  // OCSP cache.
  add_ocsp_test(
    "ocsp-stapling-unknown.example.com",
    SEC_ERROR_OCSP_UNKNOWN_CERT,
    [],
    "Stapled Unknown response -> a fetch should not have been attempted"
  );

  // A failure to retrieve an OCSP response must result in the cached Unknown
  // response being recognized and honored.
  add_ocsp_test(
    "ocsp-stapling-none.example.com",
    SEC_ERROR_OCSP_UNKNOWN_CERT,
    [
      respondWithError,
      respondWithError,
      respondWithError,
      respondWithError,
      respondWithError,
      respondWithError,
    ],
    "No stapled response -> a fetch should have been attempted"
  );

  // A valid Good response from the OCSP responder must override the cached
  // Unknown response.
  //
  // Note that We need to make sure that the Unknown response and the Good
  // response have different thisUpdate timestamps; otherwise, the Good
  // response will be seen as "not newer" and it won't replace the existing
  // entry.
  add_test(function() {
    gGoodOCSPResponse = generateGoodOCSPResponse(1200);
    run_next_test();
  });
  add_ocsp_test(
    "ocsp-stapling-none.example.com",
    PRErrorCodeSuccess,
    [respondWithGoodOCSP],
    "Cached Unknown response, no stapled response -> a fetch" +
      " should have been attempted"
  );

  // The Good response retrieved from the previous fetch must have replaced
  // the Unknown response in the cache, resulting in the catched Good response
  // being returned and no fetch.
  add_ocsp_test(
    "ocsp-stapling-none.example.com",
    PRErrorCodeSuccess,
    [],
    "Cached Good response -> a fetch should not have been attempted"
  );

  // ---------------------------------------------------------------------------

  // Reset state
  add_test(function() {
    clearOCSPCache();
    run_next_test();
  });

  // A failure to retrieve an OCSP response will result in an error entry being
  // added to the cache.
  add_ocsp_test(
    "ocsp-stapling-none.example.com",
    PRErrorCodeSuccess,
    [respondWithError],
    "No stapled response -> a fetch should have been attempted"
  );

  // The error entry will prevent a fetch from happening for a while.
  add_ocsp_test(
    "ocsp-stapling-none.example.com",
    PRErrorCodeSuccess,
    [],
    "Noted OCSP server failure -> a fetch should not have been attempted"
  );

  // The error entry must not prevent a stapled OCSP response from being
  // honored.
  add_ocsp_test(
    "ocsp-stapling-revoked.example.com",
    SEC_ERROR_REVOKED_CERTIFICATE,
    [],
    "Stapled Revoked response -> a fetch should not have been attempted"
  );

  // ---------------------------------------------------------------------------

  // Ensure OCSP responses from signers with SHA1 certificates are OK. This
  // is included in the OCSP caching tests since there were OCSP cache-related
  // regressions when sha-1 telemetry probes were added.
  add_test(function() {
    clearOCSPCache();
    // set security.OCSP.require so that checking the OCSP signature fails
    Services.prefs.setBoolPref("security.OCSP.require", true);
    run_next_test();
  });

  add_ocsp_test(
    "ocsp-stapling-none.example.com",
    PRErrorCodeSuccess,
    [respondWithSHA1OCSP],
    "signing cert is good (though sha1) - should succeed"
  );

  add_test(function() {
    Services.prefs.setBoolPref("security.OCSP.require", false);
    run_next_test();
  });

  // ---------------------------------------------------------------------------

  // Reset state
  add_test(function() {
    clearOCSPCache();
    run_next_test();
  });

  // This test makes sure that OCSP cache are isolated by firstPartyDomain.

  let gObservedCnt = 0;
  let protocolProxyService = Cc[
    "@mozilla.org/network/protocol-proxy-service;1"
  ].getService(Ci.nsIProtocolProxyService);

  // Observe all channels and make sure the firstPartyDomain in their loadInfo's
  // origin attributes are aFirstPartyDomain.
  function startObservingChannels(aFirstPartyDomain) {
    // We use a dummy proxy filter to catch all channels, even those that do not
    // generate an "http-on-modify-request" notification.
    let proxyFilter = {
      applyFilter(aChannel, aProxy, aCallback) {
        // We have the channel; provide it to the callback.
        if (aChannel.originalURI.spec == "http://localhost:8888/") {
          gObservedCnt++;
          equal(
            aChannel.loadInfo.originAttributes.firstPartyDomain,
            aFirstPartyDomain,
            "firstPartyDomain should match"
          );
        }
        // Pass on aProxy unmodified.
        aCallback.onProxyFilterResult(aProxy);
      },
    };
    protocolProxyService.registerChannelFilter(proxyFilter, 0);
    // Return the stop() function:
    return () => protocolProxyService.unregisterChannelFilter(proxyFilter);
  }

  let stopObservingChannels;
  add_test(function() {
    stopObservingChannels = startObservingChannels("foo.com");
    run_next_test();
  });

  // A good OCSP response will be cached.
  add_ocsp_test(
    "ocsp-stapling-none.example.com",
    PRErrorCodeSuccess,
    [respondWithGoodOCSP],
    "No stapled response (firstPartyDomain = foo.com) -> a fetch " +
      "should have been attempted",
    { firstPartyDomain: "foo.com" }
  );

  // The cache will prevent a fetch from happening.
  add_ocsp_test(
    "ocsp-stapling-none.example.com",
    PRErrorCodeSuccess,
    [],
    "Noted OCSP server failure (firstPartyDomain = foo.com) -> a " +
      "fetch should not have been attempted",
    { firstPartyDomain: "foo.com" }
  );

  add_test(function() {
    stopObservingChannels();
    equal(gObservedCnt, 1, "should have observed only 1 OCSP requests");
    gObservedCnt = 0;
    run_next_test();
  });

  add_test(function() {
    stopObservingChannels = startObservingChannels("bar.com");
    run_next_test();
  });

  // But using a different firstPartyDomain should result in a fetch.
  add_ocsp_test(
    "ocsp-stapling-none.example.com",
    PRErrorCodeSuccess,
    [respondWithGoodOCSP],
    "No stapled response (firstPartyDomain = bar.com) -> a fetch " +
      "should have been attempted",
    { firstPartyDomain: "bar.com" }
  );

  add_test(function() {
    stopObservingChannels();
    equal(gObservedCnt, 1, "should have observed only 1 OCSP requests");
    gObservedCnt = 0;
    run_next_test();
  });

  // ---------------------------------------------------------------------------

  // Reset state
  add_test(function() {
    clearOCSPCache();
    run_next_test();
  });

  // Test that the OCSP cache is not isolated by userContextId.

  // A good OCSP response will be cached.
  add_ocsp_test(
    "ocsp-stapling-none.example.com",
    PRErrorCodeSuccess,
    [respondWithGoodOCSP],
    "No stapled response (userContextId = 1) -> a fetch " +
      "should have been attempted",
    { userContextId: 1 }
  );

  // The cache will prevent a fetch from happening.
  add_ocsp_test(
    "ocsp-stapling-none.example.com",
    PRErrorCodeSuccess,
    [],
    "Noted OCSP server failure (userContextId = 1) -> a " +
      "fetch should not have been attempted",
    { userContextId: 1 }
  );

  // Fetching is prevented even if in a different userContextId.
  add_ocsp_test(
    "ocsp-stapling-none.example.com",
    PRErrorCodeSuccess,
    [],
    "Noted OCSP server failure (userContextId = 2) -> a " +
      "fetch should not have been attempted",
    { userContextId: 2 }
  );

  // ---------------------------------------------------------------------------

  // Reset state
  add_test(function() {
    clearOCSPCache();
    run_next_test();
  });

  // This test makes sure that OCSP cache are isolated by partitionKey.

  add_test(function() {
    Services.prefs.setBoolPref(
      "privacy.partition.network_state.ocsp_cache",
      true
    );
    run_next_test();
  });

  // A good OCSP response will be cached.
  add_ocsp_test(
    "ocsp-stapling-none.example.com",
    PRErrorCodeSuccess,
    [respondWithGoodOCSP],
    "No stapled response (partitionKey = (https,foo.com)) -> a fetch " +
      "should have been attempted",
    { partitionKey: "(https,foo.com)" }
  );

  // The cache will prevent a fetch from happening.
  add_ocsp_test(
    "ocsp-stapling-none.example.com",
    PRErrorCodeSuccess,
    [],
    "Noted OCSP server failure (partitionKey = (https,foo.com)) -> a " +
      "fetch should not have been attempted",
    { partitionKey: "(https,foo.com)" }
  );

  // Using a different partitionKey should result in a fetch.
  add_ocsp_test(
    "ocsp-stapling-none.example.com",
    PRErrorCodeSuccess,
    [respondWithGoodOCSP],
    "Noted OCSP server failure (partitionKey = (https,bar.com)) -> a " +
      "fetch should have been attempted",
    { partitionKey: "(https,bar.com)" }
  );

  // ---------------------------------------------------------------------------

  // Reset state
  add_test(function() {
    Services.prefs.clearUserPref("privacy.partition.network_state.ocsp_cache");
    clearOCSPCache();
    run_next_test();
  });

  // This test makes sure that OCSP cache are isolated by partitionKey in
  // private mode.

  // A good OCSP response will be cached.
  add_ocsp_test(
    "ocsp-stapling-none.example.com",
    PRErrorCodeSuccess,
    [respondWithGoodOCSP],
    "No stapled response (partitionKey = (https,foo.com)) -> a fetch " +
      "should have been attempted",
    { partitionKey: "(https,foo.com)", privateBrowsingId: 1 }
  );

  // The cache will prevent a fetch from happening.
  add_ocsp_test(
    "ocsp-stapling-none.example.com",
    PRErrorCodeSuccess,
    [],
    "Noted OCSP server failure (partitionKey = (https,foo.com)) -> a " +
      "fetch should not have been attempted",
    { partitionKey: "(https,foo.com)", privateBrowsingId: 1 }
  );

  // Using a different partitionKey should result in a fetch.
  add_ocsp_test(
    "ocsp-stapling-none.example.com",
    PRErrorCodeSuccess,
    [respondWithGoodOCSP],
    "Noted OCSP server failure (partitionKey = (https,bar.com)) -> a " +
      "fetch should have been attempted",
    { partitionKey: "(https,bar.com)", privateBrowsingId: 1 }
  );

  // ---------------------------------------------------------------------------

  // Reset state
  add_test(function() {
    clearOCSPCache();
    run_next_test();
  });
}
