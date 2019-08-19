// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
"use strict";

// Tests OCSP Must Staple handling by connecting to various domains (as faked by
// a server running locally) that correspond to combinations of whether the
// extension is present in intermediate and end-entity certificates.

var gExpectOCSPRequest;

function add_ocsp_test(
  aHost,
  aExpectedResult,
  aStaplingEnabled,
  aExpectOCSPRequest = false
) {
  add_connection_test(aHost, aExpectedResult, function() {
    gExpectOCSPRequest = aExpectOCSPRequest;
    clearOCSPCache();
    clearSessionCache();
    Services.prefs.setBoolPref(
      "security.ssl.enable_ocsp_stapling",
      aStaplingEnabled
    );
  });
}

function add_tests() {
  // ensure that the chain is checked for required features in children:
  // First a case where intermediate and ee both have the extension
  add_ocsp_test(
    "ocsp-stapling-must-staple-ee-with-must-staple-int.example.com",
    PRErrorCodeSuccess,
    true
  );

  add_test(() => {
    Services.prefs.setIntPref("security.cert_pinning.enforcement_level", 1);
    Services.prefs.setBoolPref(
      "security.cert_pinning.process_headers_from_non_builtin_roots",
      true
    );
    let uri = Services.io.newURI(
      "https://ocsp-stapling-must-staple-ee-with-must-staple-int.example.com"
    );
    let keyHash = "VCIlmPM9NkgFQtrs4Oa5TeFcDu6MWRTKSNdePEhOgD8=";
    let backupKeyHash = "KHAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAN=";
    let header = `max-age=1000; pin-sha256="${keyHash}"; pin-sha256="${backupKeyHash}"`;
    let ssservice = Cc["@mozilla.org/ssservice;1"].getService(
      Ci.nsISiteSecurityService
    );
    let secInfo = new FakeTransportSecurityInfo();
    secInfo.serverCert = constructCertFromFile(
      "ocsp_certs/must-staple-ee-with-must-staple-int.pem"
    );
    ssservice.processHeader(
      Ci.nsISiteSecurityService.HEADER_HPKP,
      uri,
      header,
      secInfo,
      0,
      Ci.nsISiteSecurityService.SOURCE_ORGANIC_REQUEST
    );
    ok(
      ssservice.isSecureURI(Ci.nsISiteSecurityService.HEADER_HPKP, uri, 0),
      "ocsp-stapling-must-staple-ee-with-must-staple-int.example.com should have HPKP set"
    );

    // Clear accumulated state.
    ssservice.resetState(Ci.nsISiteSecurityService.HEADER_HPKP, uri, 0);
    Services.prefs.clearUserPref(
      "security.cert_pinning.process_headers_from_non_builtin_roots"
    );
    Services.prefs.clearUserPref("security.cert_pinning.enforcement_level");
    run_next_test();
  });

  // Next, a case where it's present in the intermediate, not the ee
  add_ocsp_test(
    "ocsp-stapling-plain-ee-with-must-staple-int.example.com",
    MOZILLA_PKIX_ERROR_REQUIRED_TLS_FEATURE_MISSING,
    true
  );

  // We disable OCSP stapling in the next two tests so we can perform checks
  // on TLS Features in the chain without needing to support the TLS
  // extension values used.
  // Test an issuer with multiple TLS features in matched in the EE
  add_ocsp_test(
    "multi-tls-feature-good.example.com",
    PRErrorCodeSuccess,
    false
  );

  // Finally, an issuer with multiple TLS features not matched by the EE.
  add_ocsp_test(
    "multi-tls-feature-bad.example.com",
    MOZILLA_PKIX_ERROR_REQUIRED_TLS_FEATURE_MISSING,
    false
  );

  // Now a bunch of operations with only a must-staple ee
  add_ocsp_test(
    "ocsp-stapling-must-staple.example.com",
    PRErrorCodeSuccess,
    true
  );

  add_ocsp_test(
    "ocsp-stapling-must-staple-revoked.example.com",
    SEC_ERROR_REVOKED_CERTIFICATE,
    true
  );

  add_ocsp_test(
    "ocsp-stapling-must-staple-missing.example.com",
    MOZILLA_PKIX_ERROR_REQUIRED_TLS_FEATURE_MISSING,
    true,
    true
  );

  add_ocsp_test(
    "ocsp-stapling-must-staple-empty.example.com",
    SEC_ERROR_OCSP_MALFORMED_RESPONSE,
    true
  );

  add_ocsp_test(
    "ocsp-stapling-must-staple-missing.example.com",
    PRErrorCodeSuccess,
    false,
    true
  );

  // If the stapled response is expired, we will try to fetch a new one.
  // If that fails, we should report the original error.
  add_ocsp_test(
    "ocsp-stapling-must-staple-expired.example.com",
    SEC_ERROR_OCSP_OLD_RESPONSE,
    true,
    true
  );
  // Similarly with a "try server later" response.
  add_ocsp_test(
    "ocsp-stapling-must-staple-try-later.example.com",
    SEC_ERROR_OCSP_TRY_SERVER_LATER,
    true,
    true
  );
  // And again with an invalid OCSP response signing certificate.
  add_ocsp_test(
    "ocsp-stapling-must-staple-invalid-signer.example.com",
    SEC_ERROR_OCSP_INVALID_SIGNING_CERT,
    true,
    true
  );

  // check that disabling must-staple works
  add_test(function() {
    clearSessionCache();
    Services.prefs.setBoolPref("security.ssl.enable_ocsp_must_staple", false);
    run_next_test();
  });

  add_ocsp_test(
    "ocsp-stapling-must-staple-missing.example.com",
    PRErrorCodeSuccess,
    true,
    true
  );
}

function run_test() {
  do_get_profile();
  Services.prefs.setBoolPref("security.ssl.enable_ocsp_must_staple", true);
  Services.prefs.setIntPref("security.OCSP.enabled", 1);
  // This test may sometimes fail on android due to an OCSP request timing out.
  // That aspect of OCSP requests is not what we're testing here, so we can just
  // bump the timeout and hopefully avoid these failures.
  Services.prefs.setIntPref("security.OCSP.timeoutMilliseconds.soft", 5000);

  let fakeOCSPResponder = new HttpServer();
  fakeOCSPResponder.registerPrefixHandler("/", function(request, response) {
    response.setStatusLine(request.httpVersion, 500, "Internal Server Error");
    ok(
      gExpectOCSPRequest,
      "Should be getting an OCSP request only when expected"
    );
  });
  fakeOCSPResponder.start(8888);

  add_tls_server_setup("OCSPStaplingServer", "ocsp_certs");

  add_tests();

  add_test(function() {
    fakeOCSPResponder.stop(run_next_test);
  });

  run_next_test();
}
