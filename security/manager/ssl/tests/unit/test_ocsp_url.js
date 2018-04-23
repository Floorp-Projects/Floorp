// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

"use strict";

// In which we try to validate several ocsp responses, checking in particular
// if the ocsp url is valid and the path expressed is correctly passed to
// the caller.

do_get_profile(); // must be called before getting nsIX509CertDB
const certdb = Cc["@mozilla.org/security/x509certdb;1"]
                 .getService(Ci.nsIX509CertDB);

const SERVER_PORT = 8888;

function failingOCSPResponder() {
  return getFailingHttpServer(SERVER_PORT, ["www.example.com"]);
}

function start_ocsp_responder(expectedCertNames, expectedPaths) {
  return startOCSPResponder(SERVER_PORT, "www.example.com",
                            "test_ocsp_url", expectedCertNames, expectedPaths);
}

function check_cert_err(cert_name, expected_error) {
  let cert = constructCertFromFile("test_ocsp_url/" + cert_name + ".pem");
  return checkCertErrorGeneric(certdb, cert, expected_error,
                               certificateUsageSSLServer);
}

add_task(async function() {
  addCertFromFile(certdb, "test_ocsp_url/ca.pem", "CTu,CTu,CTu");
  addCertFromFile(certdb, "test_ocsp_url/int.pem", ",,");

  // Enabled so that we can force ocsp failure responses.
  Services.prefs.setBoolPref("security.OCSP.require", true);

  Services.prefs.setCharPref("network.dns.localDomains",
                             "www.example.com");
  Services.prefs.setIntPref("security.OCSP.enabled", 1);

  // Note: We don't test the case of a well-formed HTTP URL with an empty port
  //       because the OCSP code would then send a request to port 80, which we
  //       can't use in tests.

  clearOCSPCache();
  let ocspResponder = failingOCSPResponder();
  await check_cert_err("bad-scheme", SEC_ERROR_CERT_BAD_ACCESS_LOCATION);
  await stopOCSPResponder(ocspResponder);

  clearOCSPCache();
  ocspResponder = failingOCSPResponder();
  await check_cert_err("empty-scheme-url", SEC_ERROR_CERT_BAD_ACCESS_LOCATION);
  await stopOCSPResponder(ocspResponder);

  clearOCSPCache();
  ocspResponder = failingOCSPResponder();
  await check_cert_err("ftp-url", SEC_ERROR_CERT_BAD_ACCESS_LOCATION);
  await stopOCSPResponder(ocspResponder);

  clearOCSPCache();
  ocspResponder = failingOCSPResponder();
  await check_cert_err("https-url", SEC_ERROR_CERT_BAD_ACCESS_LOCATION);
  await stopOCSPResponder(ocspResponder);

  clearOCSPCache();
  ocspResponder = start_ocsp_responder(["hTTp-url"], ["hTTp-url"]);
  await check_cert_err("hTTp-url", PRErrorCodeSuccess);
  await stopOCSPResponder(ocspResponder);

  clearOCSPCache();
  ocspResponder = failingOCSPResponder();
  await check_cert_err("negative-port", SEC_ERROR_CERT_BAD_ACCESS_LOCATION);
  await stopOCSPResponder(ocspResponder);

  clearOCSPCache();
  ocspResponder = failingOCSPResponder();
  await check_cert_err("no-host-url", SEC_ERROR_CERT_BAD_ACCESS_LOCATION);
  await stopOCSPResponder(ocspResponder);

  clearOCSPCache();
  ocspResponder = start_ocsp_responder(["no-path-url"], [""]);
  await check_cert_err("no-path-url", PRErrorCodeSuccess);
  await stopOCSPResponder(ocspResponder);

  clearOCSPCache();
  ocspResponder = failingOCSPResponder();
  await check_cert_err("no-scheme-host-port", SEC_ERROR_CERT_BAD_ACCESS_LOCATION);
  await stopOCSPResponder(ocspResponder);

  clearOCSPCache();
  ocspResponder = failingOCSPResponder();
  await check_cert_err("no-scheme-url", SEC_ERROR_CERT_BAD_ACCESS_LOCATION);
  await stopOCSPResponder(ocspResponder);

  clearOCSPCache();
  ocspResponder = failingOCSPResponder();
  await check_cert_err("unknown-scheme", SEC_ERROR_CERT_BAD_ACCESS_LOCATION);
  await stopOCSPResponder(ocspResponder);

  // Note: We currently don't have anything that ensures user:pass sections
  //       weren't sent. The following test simply checks that such sections
  //       don't cause failures.
  clearOCSPCache();
  ocspResponder = start_ocsp_responder(["user-pass"], [""]);
  await check_cert_err("user-pass", PRErrorCodeSuccess);
  await stopOCSPResponder(ocspResponder);
});
