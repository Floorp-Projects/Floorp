/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Tests that adding a certificate already present in the certificate database
// with different trust bits than those stored in the database does not result
// in the new trust bits being ignored.

do_get_profile();
let certDB = Cc["@mozilla.org/security/x509certdb;1"]
               .getService(Ci.nsIX509CertDB);

function load_cert(cert, trust) {
  let file = "test_intermediate_basic_usage_constraints/" + cert + ".pem";
  addCertFromFile(certDB, file, trust);
}

function getDERString(cert)
{
  var length = {};
  var cert_der = cert.getRawDER(length);
  var cert_der_string = '';
  for (var i = 0; i < cert_der.length; i++) {
    cert_der_string += String.fromCharCode(cert_der[i]);
  }
  return cert_der_string;
}

function run_test() {
  load_cert("ca", "CTu,CTu,CTu");
  load_cert("int-limited-depth", "CTu,CTu,CTu");
  let file = "test_intermediate_basic_usage_constraints/ee-int-limited-depth.pem";
  let cert_pem = readFile(do_get_file(file));
  let ee = certDB.constructX509FromBase64(pemToBase64(cert_pem));
  checkCertErrorGeneric(certDB, ee, PRErrorCodeSuccess,
                        certificateUsageSSLServer);
  // Change the already existing intermediate certificate's trust using
  // addCertFromBase64(). We use findCertByNickname first to ensure that the
  // certificate already exists.
  let int_cert = certDB.findCertByNickname(null, "int-limited-depth");
  notEqual(int_cert, null, "Intermediate cert should be in the cert DB");
  let base64_cert = btoa(getDERString(int_cert));
  certDB.addCertFromBase64(base64_cert, "p,p,p", "ignored_argument");
  checkCertErrorGeneric(certDB, ee, SEC_ERROR_UNTRUSTED_ISSUER,
                        certificateUsageSSLServer);
}
