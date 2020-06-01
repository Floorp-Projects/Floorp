/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This test is for the TLS error reporting functionality exposed by
 * SecurityReporter.js in /toolkit/components/securityreporter. The test is
 * here because we make use of the tlsserver functionality that lives with the
 * PSM ssl tests.
 *
 * The testing here will be augmented by the existing mochitests for the
 * error reporting functionality in aboutNetError.xhtml and
 * aboutCertError.xhtml once these make use of this component.
 */

"use strict";
const CC = Components.Constructor;
const Cm = Components.manager;

const { updateAppInfo } = ChromeUtils.import(
  "resource://testing-common/AppInfo.jsm"
); // Imported via AppInfo.jsm.
updateAppInfo();

// We must get the profile before performing operations on the cert db.
do_get_profile();

const certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
  Ci.nsIX509CertDB
);
const reporter = Cc["@mozilla.org/securityreporter;1"].getService(
  Ci.nsISecurityReporter
);

const BinaryInputStream = CC(
  "@mozilla.org/binaryinputstream;1",
  "nsIBinaryInputStream",
  "setInputStream"
);

var server;

// this allows us to create a callback which checks that a report is as
// expected.
function getReportCheck(expectReport, expectedError) {
  return function sendReportWithInfo(transportSecurityInfo) {
    // register a path handler on the server
    server.registerPathHandler("/submit/sslreports", function(
      request,
      response
    ) {
      if (expectReport) {
        let report = JSON.parse(readDataFromRequest(request));
        throws(() => request.getHeader("Cookie"), /NS_ERROR_NOT_AVAILABLE/);
        Assert.equal(report.errorCode, expectedError);
        response.setStatusLine(null, 201, "Created");
        response.write("Created");
        run_next_test(); // resolve a "test" waiting on this report to be sent/received
      } else {
        do_throw("No report should have been received");
      }
    });

    reporter.reportTLSError(transportSecurityInfo, "example.com", -1);
  };
}

// read the request body from a request
function readDataFromRequest(aRequest) {
  if (aRequest.method == "POST" || aRequest.method == "PUT") {
    if (aRequest.bodyInputStream) {
      let inputStream = new BinaryInputStream(aRequest.bodyInputStream);
      let bytes = [];
      let available;

      while ((available = inputStream.available()) > 0) {
        Array.prototype.push.apply(bytes, inputStream.readByteArray(available));
      }

      return String.fromCharCode.apply(null, bytes);
    }
  }
  return null;
}

function run_test() {
  // start a report server
  server = new HttpServer();
  server.start(-1);

  let port = server.identity.primaryPort;

  // Set the reporting URL to ensure any reports are sent to the test server
  Services.prefs.setCharPref(
    "security.ssl.errorReporting.url",
    `http://localhost:${port}/submit/sslreports`
  );
  // set strict-mode pinning enforcement so we can cause connection failures.
  Services.prefs.setIntPref("security.cert_pinning.enforcement_level", 2);

  // Add a cookie so that we can assert it's not sent along with the report.
  Services.cookies.add(
    "localhost",
    "/",
    "foo",
    "bar",
    false,
    false,
    false,
    Date.now() + 24000 * 60 * 60,
    {},
    Ci.nsICookie.SAMESITE_NONE
  );

  registerCleanupFunction(() => {
    Services.cookies.removeAll();
  });

  // start a TLS server
  add_tls_server_setup("BadCertAndPinningServer", "bad_certs");

  // Add a user-specified trust anchor.
  addCertFromFile(certdb, "bad_certs/other-test-ca.pem", "CTu,u,u");

  // Cause a reportable condition with error reporting disabled. No report
  // should be sent.
  Services.prefs.setBoolPref("security.ssl.errorReporting.enabled", false);
  add_connection_test(
    "expired.example.com",
    SEC_ERROR_EXPIRED_CERTIFICATE,
    null,
    getReportCheck(false)
  );

  // Now enable reporting
  add_test(function() {
    Services.prefs.setBoolPref("security.ssl.errorReporting.enabled", true);
    run_next_test();
  });

  // test calling the component with no transportSecurityInfo. No report should
  // be sent even though reporting is enabled.
  add_test(function() {
    server.registerPathHandler("/submit/sslreports", function(
      request,
      response
    ) {
      do_throw("No report should be sent");
    });
    reporter.reportTLSError(null, "example.com", -1);
    run_next_test();
  });

  // Test sending a report with no error. This allows us to check the case
  // where there is no failed cert chain
  add_connection_test(
    "good.include-subdomains.pinning.example.com",
    PRErrorCodeSuccess,
    null,
    getReportCheck(true, PRErrorCodeSuccess)
  );
  add_test(() => {}); // add a "test" so we wait for the report to be sent

  // Test sending a report where there is an error and a failed cert chain.
  add_connection_test(
    "expired.example.com",
    SEC_ERROR_EXPIRED_CERTIFICATE,
    null,
    getReportCheck(true, SEC_ERROR_EXPIRED_CERTIFICATE)
  );
  add_test(() => {}); // add a "test" so we wait for the report to be sent

  run_next_test();
}
