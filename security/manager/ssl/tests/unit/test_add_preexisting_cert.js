/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Tests that adding a certificate already present in the certificate database
// with different trust bits than those stored in the database does not result
// in the new trust bits being ignored.

do_get_profile();
var certDB = Cc["@mozilla.org/security/x509certdb;1"].getService(
  Ci.nsIX509CertDB
);

function load_cert(cert, trust) {
  let file = "test_intermediate_basic_usage_constraints/" + cert + ".pem";
  return addCertFromFile(certDB, file, trust);
}

add_task(async function() {
  load_cert("ca", "CTu,CTu,CTu");
  let int_cert = load_cert("int-limited-depth", "CTu,CTu,CTu");
  let file =
    "test_intermediate_basic_usage_constraints/ee-int-limited-depth.pem";
  let cert_pem = readFile(do_get_file(file));
  let ee = certDB.constructX509FromBase64(pemToBase64(cert_pem));
  await checkCertErrorGeneric(
    certDB,
    ee,
    PRErrorCodeSuccess,
    certificateUsageSSLServer
  );
  // Change the already existing intermediate certificate's trust using
  // addCertFromBase64().
  notEqual(int_cert, null, "Intermediate cert should be in the cert DB");
  let base64_cert = int_cert.getBase64DERString();
  let returnedEE = certDB.addCertFromBase64(base64_cert, "p,p,p");
  notEqual(returnedEE, null, "addCertFromBase64 should return a certificate");
  await checkCertErrorGeneric(
    certDB,
    ee,
    SEC_ERROR_UNTRUSTED_ISSUER,
    certificateUsageSSLServer
  );
});
