/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
do_get_profile();
let certDB = Cc["@mozilla.org/security/x509certdb;1"]
               .getService(Ci.nsIX509CertDB);

function load_cert(cert, trust) {
  let file = "test_intermediate_basic_usage_constraints/" + cert + ".der";
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
  let file = "test_intermediate_basic_usage_constraints/ee-int-limited-depth.der";
  let cert_der = readFile(do_get_file(file));
  let ee = certDB.constructX509(cert_der, cert_der.length);
  let hasEVPolicy = {};
  let verifiedChain = {};
  equal(Cr.NS_OK, certDB.verifyCertNow(ee, certificateUsageSSLServer,
                                       NO_FLAGS, verifiedChain, hasEVPolicy));
  // Change the already existing intermediate certificate's trust using
  // addCertFromBase64(). We use findCertByNickname first to ensure that the
  // certificate already exists.
  let int_cert = certDB.findCertByNickname(null, "int-limited-depth");
  ok(int_cert);
  let base64_cert = btoa(getDERString(int_cert));
  certDB.addCertFromBase64(base64_cert, "p,p,p", "ignored_argument");
  equal(SEC_ERROR_UNTRUSTED_ISSUER, certDB.verifyCertNow(ee,
                                                         certificateUsageSSLServer,
                                                         NO_FLAGS, verifiedChain,
                                                         hasEVPolicy));
}