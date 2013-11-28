// -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

"use strict";

do_get_profile(); // must be called before getting nsIX509CertDB
const certdb = Cc["@mozilla.org/security/x509certdb;1"]
                 .getService(Ci.nsIX509CertDB);

const evrootnick = "XPCShell EV Testing (untrustworthy) CA - Mozilla - " +
                   "EV debug test CA";

// This is the list of certificates needed for the test
// The certificates prefixed by 'int-' are intermediates
let certList = [
  // Test for successful EV validation
  'int-ev-valid',
  'ev-valid',

  // Testing a root that looks like EV but is not EV enabled
  'int-non-ev-root',
  'non-ev-root',
]

function load_ca(ca_name) {
  var ca_filename = ca_name + ".der";
  addCertFromFile(certdb, "test_ev_certs/" + ca_filename, 'CTu,CTu,CTu');
}

var gHttpServer;
var gOCSPResponseCounter = 0;

function start_ocsp_responder() {
  const SERVER_PORT = 8888;

  gHttpServer = new HttpServer();
  gHttpServer.registerPrefixHandler("/",
      function handleServerCallback(aRequest, aResponse) {
        let cert_nick = aRequest.path.slice(1, aRequest.path.length - 1);
        do_print("Generating ocsp response for '" + cert_nick + "'");
        aResponse.setStatusLine(aRequest.httpVersion, 200, "OK");
        aResponse.setHeader("Content-Type", "application/ocsp-response");
        // now we generate the response
        let ocsp_request_desc = new Array();
        ocsp_request_desc.push("good");
        ocsp_request_desc.push(cert_nick);
        ocsp_request_desc.push("unused_arg");
        let arg_array = new Array();
        arg_array.push(ocsp_request_desc);
        let retArray = generateOCSPResponses(arg_array, "test_ev_certs");
        let responseBody = retArray[0];
        aResponse.bodyOutputStream.write(responseBody, responseBody.length);
        gOCSPResponseCounter++;
      });
  gHttpServer.identity.setPrimary("http", "www.example.com", SERVER_PORT);
  gHttpServer.start(SERVER_PORT);
}

function check_cert_err(cert_name, expected_error) {
  let cert = certdb.findCertByNickname(null, cert_name);
  let hasEVPolicy = {};
  let verifiedChain = {};
  let error = certdb.verifyCertNow(cert, certificateUsageSSLServer,
                                   NO_FLAGS, verifiedChain, hasEVPolicy);
  do_check_eq(error,  expected_error);
}


function check_ee_for_ev(cert_name, expected_ev) {
    let cert = certdb.findCertByNickname(null, cert_name);
    let hasEVPolicy = {};
    let verifiedChain = {};
    let error = certdb.verifyCertNow(cert, certificateUsageSSLServer,
                                     NO_FLAGS, verifiedChain, hasEVPolicy);
    if (isDebugBuild) {
      do_check_eq(hasEVPolicy.value, expected_ev);
    } else {
      do_check_false(hasEVPolicy.value);
    }
    do_check_eq(0, error);
}

function run_test() {
  for (let i = 0 ; i < certList.length; i++) {
    let cert_filename = certList[i] + ".der";
    addCertFromFile(certdb, "test_ev_certs/" + cert_filename, ',,');
  }
  load_ca("evroot");
  load_ca("non-evroot-ca");

  // setup and start ocsp responder
  Services.prefs.setCharPref("network.dns.localDomains", 'www.example.com');
  start_ocsp_responder();

  run_next_test();
}


add_test(function() {
  check_ee_for_ev("ev-valid", true);
  run_next_test();
});

add_test(function() {
  check_ee_for_ev("non-ev-root", false);
  run_next_test();
});

// Test for bug 917380
add_test(function () {
  const nsIX509Cert = Ci.nsIX509Cert;
  let evRootCA = certdb.findCertByNickname(null, evrootnick);
  certdb.setCertTrust(evRootCA, nsIX509Cert.CA_CERT, 0);
  check_cert_err("ev-valid", SEC_ERROR_UNTRUSTED_ISSUER);
  certdb.setCertTrust(evRootCA, nsIX509Cert.CA_CERT,
                      Ci.nsIX509CertDB.TRUSTED_SSL |
                      Ci.nsIX509CertDB.TRUSTED_EMAIL |
                      Ci.nsIX509CertDB.TRUSTED_OBJSIGN);
  check_ee_for_ev("ev-valid", true);
  run_next_test();
});

// The following test should be the last as it performs cleanups
add_test(function() {
  do_check_eq(4, gOCSPResponseCounter);
  gHttpServer.stop(run_next_test);
});

