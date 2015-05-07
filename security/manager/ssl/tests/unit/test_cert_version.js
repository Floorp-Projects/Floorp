// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

"use strict";

do_get_profile(); // must be called before getting nsIX509CertDB
const certdb = Cc["@mozilla.org/security/x509certdb;1"]
                 .getService(Ci.nsIX509CertDB);

function cert_from_file(filename) {
  return constructCertFromFile("test_cert_version/" + filename);
}

function load_cert(cert_name, trust_string) {
  var cert_filename = cert_name + ".der";
  addCertFromFile(certdb, "test_cert_version/" + cert_filename, trust_string);
}

function check_cert_err_generic(cert, expected_error, usage) {
  do_print("cert cn=" + cert.commonName);
  do_print("cert issuer cn=" + cert.issuerCommonName);
  let hasEVPolicy = {};
  let verifiedChain = {};
  let error = certdb.verifyCertNow(cert, usage, NO_FLAGS, null, verifiedChain,
                                   hasEVPolicy);
  do_check_eq(error, expected_error);
}

function check_cert_err(cert, expected_error) {
  check_cert_err_generic(cert, expected_error, certificateUsageSSLServer)
}

function check_ca_err(cert, expected_error) {
  check_cert_err_generic(cert, expected_error, certificateUsageSSLCA)
}

function check_ok(x) {
  return check_cert_err(x, PRErrorCodeSuccess);
}

function check_ok_ca(x) {
  return check_cert_err_generic(x, PRErrorCodeSuccess, certificateUsageSSLCA);
}

function run_test() {
  load_cert("v1_ca", "CTu,CTu,CTu");
  load_cert("v1_ca_bc", "CTu,CTu,CTu");
  load_cert("v2_ca", "CTu,CTu,CTu");
  load_cert("v2_ca_bc", "CTu,CTu,CTu");
  load_cert("v3_ca", "CTu,CTu,CTu");
  load_cert("v3_ca_missing_bc", "CTu,CTu,CTu");

  check_ok_ca(cert_from_file('v1_ca.der'));
  check_ok_ca(cert_from_file('v1_ca_bc.der'));
  check_ca_err(cert_from_file('v2_ca.der'), SEC_ERROR_CA_CERT_INVALID);
  check_ok_ca(cert_from_file('v2_ca_bc.der'));
  check_ok_ca(cert_from_file('v3_ca.der'));
  check_ca_err(cert_from_file('v3_ca_missing_bc.der'), SEC_ERROR_CA_CERT_INVALID);

  // A v1 certificate may be a CA if it has a basic constraints extension with
  // CA: TRUE or if it is a trust anchor.

  //////////////
  // v1 CA supersection
  //////////////////

  // v1 intermediate with v1 trust anchor
  let error = MOZILLA_PKIX_ERROR_V1_CERT_USED_AS_CA;
  check_ca_err(cert_from_file('v1_int-v1_ca.der'), error);
  check_cert_err(cert_from_file('v1_ee-v1_int-v1_ca.der'), error);
  check_cert_err(cert_from_file('v2_ee-v1_int-v1_ca.der'), error);
  check_cert_err(cert_from_file('v3_missing_bc_ee-v1_int-v1_ca.der'), error);
  check_cert_err(cert_from_file('v3_bc_ee-v1_int-v1_ca.der'), error);
  check_cert_err(cert_from_file('v1_bc_ee-v1_int-v1_ca.der'), error);
  check_cert_err(cert_from_file('v2_bc_ee-v1_int-v1_ca.der'), error);
  check_cert_err(cert_from_file('v4_bc_ee-v1_int-v1_ca.der'), error);

  // v1 intermediate with v3 extensions.
  check_ok_ca(cert_from_file('v1_int_bc-v1_ca.der'));
  check_ok(cert_from_file('v1_ee-v1_int_bc-v1_ca.der'));
  check_ok(cert_from_file('v1_bc_ee-v1_int_bc-v1_ca.der'));
  check_ok(cert_from_file('v2_ee-v1_int_bc-v1_ca.der'));
  check_ok(cert_from_file('v2_bc_ee-v1_int_bc-v1_ca.der'));
  check_ok(cert_from_file('v3_missing_bc_ee-v1_int_bc-v1_ca.der'));
  check_ok(cert_from_file('v3_bc_ee-v1_int_bc-v1_ca.der'));
  check_ok(cert_from_file('v4_bc_ee-v1_int_bc-v1_ca.der'));

  // A v2 intermediate with a v1 CA
  error = SEC_ERROR_CA_CERT_INVALID;
  check_ca_err(cert_from_file('v2_int-v1_ca.der'), error);
  check_cert_err(cert_from_file('v1_ee-v2_int-v1_ca.der'), error);
  check_cert_err(cert_from_file('v2_ee-v2_int-v1_ca.der'), error);
  check_cert_err(cert_from_file('v3_missing_bc_ee-v2_int-v1_ca.der'), error);
  check_cert_err(cert_from_file('v3_bc_ee-v2_int-v1_ca.der'), error);
  check_cert_err(cert_from_file('v1_bc_ee-v2_int-v1_ca.der'), error);
  check_cert_err(cert_from_file('v2_bc_ee-v2_int-v1_ca.der'), error);
  check_cert_err(cert_from_file('v4_bc_ee-v2_int-v1_ca.der'), error);

  // A v2 intermediate with basic constraints
  check_ok_ca(cert_from_file('v2_int_bc-v1_ca.der'));
  check_ok(cert_from_file('v1_ee-v2_int_bc-v1_ca.der'));
  check_ok(cert_from_file('v1_bc_ee-v2_int_bc-v1_ca.der'));
  check_ok(cert_from_file('v2_ee-v2_int_bc-v1_ca.der'));
  check_ok(cert_from_file('v2_bc_ee-v2_int_bc-v1_ca.der'));
  check_ok(cert_from_file('v3_missing_bc_ee-v2_int_bc-v1_ca.der'));
  check_ok(cert_from_file('v3_bc_ee-v2_int_bc-v1_ca.der'));
  check_ok(cert_from_file('v4_bc_ee-v2_int_bc-v1_ca.der'));

  // Section is OK. A x509 v3 CA MUST have bc
  // http://tools.ietf.org/html/rfc5280#section-4.2.1.9
  error = SEC_ERROR_CA_CERT_INVALID;
  check_ca_err(cert_from_file('v3_int_missing_bc-v1_ca.der'), error);
  check_cert_err(cert_from_file('v1_ee-v3_int_missing_bc-v1_ca.der'), error);
  check_cert_err(cert_from_file('v2_ee-v3_int_missing_bc-v1_ca.der'), error);
  check_cert_err(cert_from_file('v3_missing_bc_ee-v3_int_missing_bc-v1_ca.der'), error);
  check_cert_err(cert_from_file('v3_bc_ee-v3_int_missing_bc-v1_ca.der'), error);
  check_cert_err(cert_from_file('v1_bc_ee-v3_int_missing_bc-v1_ca.der'), error);
  check_cert_err(cert_from_file('v2_bc_ee-v3_int_missing_bc-v1_ca.der'), error);
  check_cert_err(cert_from_file('v4_bc_ee-v3_int_missing_bc-v1_ca.der'), error);

  // It is valid for a v1 ca to sign a v3 intemediate.
  check_ok_ca(cert_from_file('v3_int-v1_ca.der'));
  check_ok(cert_from_file('v1_ee-v3_int-v1_ca.der'));
  check_ok(cert_from_file('v2_ee-v3_int-v1_ca.der'));
  check_ok(cert_from_file('v3_missing_bc_ee-v3_int-v1_ca.der'));
  check_ok(cert_from_file('v3_bc_ee-v3_int-v1_ca.der'));
  check_ok(cert_from_file('v1_bc_ee-v3_int-v1_ca.der'));
  check_ok(cert_from_file('v2_bc_ee-v3_int-v1_ca.der'));
  check_ok(cert_from_file('v4_bc_ee-v3_int-v1_ca.der'));

  // The next groups change the v1 ca for a v1 ca with base constraints
  // (invalid trust anchor). The error pattern is the same as the groups
  // above

  // Using A v1 intermediate
  error = MOZILLA_PKIX_ERROR_V1_CERT_USED_AS_CA;
  check_ca_err(cert_from_file('v1_int-v1_ca_bc.der'), error);
  check_cert_err(cert_from_file('v1_ee-v1_int-v1_ca_bc.der'), error);
  check_cert_err(cert_from_file('v2_ee-v1_int-v1_ca_bc.der'), error);
  check_cert_err(cert_from_file('v3_missing_bc_ee-v1_int-v1_ca_bc.der'), error);
  check_cert_err(cert_from_file('v3_bc_ee-v1_int-v1_ca_bc.der'), error);
  check_cert_err(cert_from_file('v1_bc_ee-v1_int-v1_ca_bc.der'), error);
  check_cert_err(cert_from_file('v2_bc_ee-v1_int-v1_ca_bc.der'), error);
  check_cert_err(cert_from_file('v4_bc_ee-v1_int-v1_ca_bc.der'), error);

  // Using a v1 intermediate with v3 extenstions
  check_ok_ca(cert_from_file('v1_int_bc-v1_ca_bc.der'));
  check_ok(cert_from_file('v1_ee-v1_int_bc-v1_ca_bc.der'));
  check_ok(cert_from_file('v1_bc_ee-v1_int_bc-v1_ca_bc.der'));
  check_ok(cert_from_file('v2_ee-v1_int_bc-v1_ca_bc.der'));
  check_ok(cert_from_file('v2_bc_ee-v1_int_bc-v1_ca_bc.der'));
  check_ok(cert_from_file('v3_missing_bc_ee-v1_int_bc-v1_ca_bc.der'));
  check_ok(cert_from_file('v3_bc_ee-v1_int_bc-v1_ca_bc.der'));
  check_ok(cert_from_file('v4_bc_ee-v1_int_bc-v1_ca_bc.der'));

  // Using v2 intermediate
  error = SEC_ERROR_CA_CERT_INVALID;
  check_ca_err(cert_from_file('v2_int-v1_ca_bc.der'), error);
  check_cert_err(cert_from_file('v1_ee-v2_int-v1_ca_bc.der'), error);
  check_cert_err(cert_from_file('v2_ee-v2_int-v1_ca_bc.der'), error);
  check_cert_err(cert_from_file('v3_missing_bc_ee-v2_int-v1_ca_bc.der'), error);
  check_cert_err(cert_from_file('v3_bc_ee-v2_int-v1_ca_bc.der'), error);
  check_cert_err(cert_from_file('v1_bc_ee-v2_int-v1_ca_bc.der'), error);
  check_cert_err(cert_from_file('v2_bc_ee-v2_int-v1_ca_bc.der'), error);
  check_cert_err(cert_from_file('v4_bc_ee-v2_int-v1_ca_bc.der'), error);

  // Using a v2 intermediate with basic constraints 
  check_ok_ca(cert_from_file('v2_int_bc-v1_ca_bc.der'));
  check_ok(cert_from_file('v1_ee-v2_int_bc-v1_ca_bc.der'));
  check_ok(cert_from_file('v1_bc_ee-v2_int_bc-v1_ca_bc.der'));
  check_ok(cert_from_file('v2_ee-v2_int_bc-v1_ca_bc.der'));
  check_ok(cert_from_file('v2_bc_ee-v2_int_bc-v1_ca_bc.der'));
  check_ok(cert_from_file('v3_missing_bc_ee-v2_int_bc-v1_ca_bc.der'));
  check_ok(cert_from_file('v3_bc_ee-v2_int_bc-v1_ca_bc.der'));
  check_ok(cert_from_file('v4_bc_ee-v2_int_bc-v1_ca_bc.der'));

  // Using a v3 intermediate that is missing basic constraints (invalid)
  error = SEC_ERROR_CA_CERT_INVALID;
  check_ca_err(cert_from_file('v3_int_missing_bc-v1_ca_bc.der'), error);
  check_cert_err(cert_from_file('v1_ee-v3_int_missing_bc-v1_ca_bc.der'), error);
  check_cert_err(cert_from_file('v2_ee-v3_int_missing_bc-v1_ca_bc.der'), error);
  check_cert_err(cert_from_file('v3_missing_bc_ee-v3_int_missing_bc-v1_ca_bc.der'), error);
  check_cert_err(cert_from_file('v3_bc_ee-v3_int_missing_bc-v1_ca_bc.der'), error);
  check_cert_err(cert_from_file('v1_bc_ee-v3_int_missing_bc-v1_ca_bc.der'), error);
  check_cert_err(cert_from_file('v2_bc_ee-v3_int_missing_bc-v1_ca_bc.der'), error);
  check_cert_err(cert_from_file('v4_bc_ee-v3_int_missing_bc-v1_ca_bc.der'), error);

  // these should pass assuming we are OK with v1 ca signing v3 intermediates
  check_ok_ca(cert_from_file('v3_int-v1_ca_bc.der'));
  check_ok(cert_from_file('v1_ee-v3_int-v1_ca_bc.der'));
  check_ok(cert_from_file('v1_bc_ee-v3_int-v1_ca_bc.der'));
  check_ok(cert_from_file('v2_ee-v3_int-v1_ca_bc.der'));
  check_ok(cert_from_file('v2_bc_ee-v3_int-v1_ca_bc.der'));
  check_ok(cert_from_file('v3_missing_bc_ee-v3_int-v1_ca_bc.der'));
  check_ok(cert_from_file('v3_bc_ee-v3_int-v1_ca_bc.der'));
  check_ok(cert_from_file('v4_bc_ee-v3_int-v1_ca_bc.der'));


  //////////////
  // v2 CA supersection
  //////////////////

  // v2 ca, v1 intermediate
  error = MOZILLA_PKIX_ERROR_V1_CERT_USED_AS_CA;
  check_ca_err(cert_from_file('v1_int-v2_ca.der'), error);
  check_cert_err(cert_from_file('v1_ee-v1_int-v2_ca.der'), error);
  check_cert_err(cert_from_file('v2_ee-v1_int-v2_ca.der'), error);
  check_cert_err(cert_from_file('v3_missing_bc_ee-v1_int-v2_ca.der'), error);
  check_cert_err(cert_from_file('v3_bc_ee-v1_int-v2_ca.der'), error);
  check_cert_err(cert_from_file('v1_bc_ee-v1_int-v2_ca.der'), error);
  check_cert_err(cert_from_file('v2_bc_ee-v1_int-v2_ca.der'), error);
  check_cert_err(cert_from_file('v4_bc_ee-v1_int-v2_ca.der'), error);

  // v2 ca, v1 intermediate with basic constraints (invalid)
  error = SEC_ERROR_CA_CERT_INVALID;
  check_ca_err(cert_from_file('v1_int_bc-v2_ca.der'), error);
  check_cert_err(cert_from_file('v1_ee-v1_int_bc-v2_ca.der'), error);
  check_cert_err(cert_from_file('v1_bc_ee-v1_int_bc-v2_ca.der'), error);
  check_cert_err(cert_from_file('v2_ee-v1_int_bc-v2_ca.der'), error);
  check_cert_err(cert_from_file('v2_bc_ee-v1_int_bc-v2_ca.der'), error);
  check_cert_err(cert_from_file('v3_missing_bc_ee-v1_int_bc-v2_ca.der'), error);
  check_cert_err(cert_from_file('v3_bc_ee-v1_int_bc-v2_ca.der'), error);
  check_cert_err(cert_from_file('v4_bc_ee-v1_int_bc-v2_ca.der'), error);

  // v2 ca, v2 intermediate
  error = SEC_ERROR_CA_CERT_INVALID;
  check_ca_err(cert_from_file('v2_int-v2_ca.der'), error);
  check_cert_err(cert_from_file('v1_ee-v2_int-v2_ca.der'), error);
  check_cert_err(cert_from_file('v2_ee-v2_int-v2_ca.der'), error);
  check_cert_err(cert_from_file('v3_missing_bc_ee-v2_int-v2_ca.der'), error);
  check_cert_err(cert_from_file('v3_bc_ee-v2_int-v2_ca.der'), error);
  check_cert_err(cert_from_file('v1_bc_ee-v2_int-v2_ca.der'), error);
  check_cert_err(cert_from_file('v2_bc_ee-v2_int-v2_ca.der'), error);
  check_cert_err(cert_from_file('v4_bc_ee-v2_int-v2_ca.der'), error);

  // v2 ca, v2 intermediate with basic constraints (invalid)
  error = SEC_ERROR_CA_CERT_INVALID;
  check_ca_err(cert_from_file('v1_int_bc-v2_ca.der'), error);
  check_cert_err(cert_from_file('v1_ee-v1_int_bc-v2_ca.der'), error);
  check_cert_err(cert_from_file('v1_bc_ee-v1_int_bc-v2_ca.der'), error);
  check_cert_err(cert_from_file('v2_ee-v1_int_bc-v2_ca.der'), error);
  check_cert_err(cert_from_file('v2_bc_ee-v1_int_bc-v2_ca.der'), error);
  check_cert_err(cert_from_file('v3_missing_bc_ee-v1_int_bc-v2_ca.der'), error);
  check_cert_err(cert_from_file('v3_bc_ee-v1_int_bc-v2_ca.der'), error);
  check_cert_err(cert_from_file('v4_bc_ee-v1_int_bc-v2_ca.der'), error);

  // v2 ca, v3 intermediate missing basic constraints
  error = SEC_ERROR_CA_CERT_INVALID;
  check_ca_err(cert_from_file('v3_int_missing_bc-v2_ca.der'), error);
  check_cert_err(cert_from_file('v1_ee-v3_int_missing_bc-v2_ca.der'), error);
  check_cert_err(cert_from_file('v2_ee-v3_int_missing_bc-v2_ca.der'), error);
  check_cert_err(cert_from_file('v3_missing_bc_ee-v3_int_missing_bc-v2_ca.der'), error);
  check_cert_err(cert_from_file('v3_bc_ee-v3_int_missing_bc-v2_ca.der'), error);
  check_cert_err(cert_from_file('v1_bc_ee-v3_int_missing_bc-v2_ca.der'), error);
  check_cert_err(cert_from_file('v2_bc_ee-v3_int_missing_bc-v2_ca.der'), error);
  check_cert_err(cert_from_file('v4_bc_ee-v3_int_missing_bc-v2_ca.der'), error);

  // v2 ca, v3 intermediate
  error = SEC_ERROR_CA_CERT_INVALID;
  check_ca_err(cert_from_file('v3_int-v2_ca.der'), error);
  check_cert_err(cert_from_file('v1_ee-v3_int-v2_ca.der'), error);
  check_cert_err(cert_from_file('v2_ee-v3_int-v2_ca.der'), error);
  check_cert_err(cert_from_file('v3_missing_bc_ee-v3_int-v2_ca.der'), error);
  check_cert_err(cert_from_file('v3_bc_ee-v3_int-v2_ca.der'), error);
  check_cert_err(cert_from_file('v1_bc_ee-v3_int-v2_ca.der'), error);
  check_cert_err(cert_from_file('v2_bc_ee-v3_int-v2_ca.der'), error);
  check_cert_err(cert_from_file('v4_bc_ee-v3_int-v2_ca.der'), error);

  // v2 ca, v1 intermediate
  error = MOZILLA_PKIX_ERROR_V1_CERT_USED_AS_CA;
  check_ca_err(cert_from_file('v1_int-v2_ca_bc.der'), error);
  check_cert_err(cert_from_file('v1_ee-v1_int-v2_ca_bc.der'), error);
  check_cert_err(cert_from_file('v2_ee-v1_int-v2_ca_bc.der'), error);
  check_cert_err(cert_from_file('v3_missing_bc_ee-v1_int-v2_ca_bc.der'), error);
  check_cert_err(cert_from_file('v3_bc_ee-v1_int-v2_ca_bc.der'), error);
  check_cert_err(cert_from_file('v1_bc_ee-v1_int-v2_ca_bc.der'), error);
  check_cert_err(cert_from_file('v2_bc_ee-v1_int-v2_ca_bc.der'), error);
  check_cert_err(cert_from_file('v4_bc_ee-v1_int-v2_ca_bc.der'), error);

  // v2 ca, v1 intermediate with bc
  check_ok_ca(cert_from_file('v1_int_bc-v2_ca_bc.der'));
  check_ok(cert_from_file('v1_ee-v1_int_bc-v2_ca_bc.der'));
  check_ok(cert_from_file('v1_bc_ee-v1_int_bc-v2_ca_bc.der'));
  check_ok(cert_from_file('v2_ee-v1_int_bc-v2_ca_bc.der'));
  check_ok(cert_from_file('v2_bc_ee-v1_int_bc-v2_ca_bc.der'));
  check_ok(cert_from_file('v3_missing_bc_ee-v1_int_bc-v2_ca_bc.der'));
  check_ok(cert_from_file('v3_bc_ee-v1_int_bc-v2_ca_bc.der'));
  check_ok(cert_from_file('v4_bc_ee-v1_int_bc-v2_ca_bc.der'));

  // v2 ca, v2 intermediate
  error = SEC_ERROR_CA_CERT_INVALID;
  check_ca_err(cert_from_file('v2_int-v2_ca_bc.der'), error);
  check_cert_err(cert_from_file('v1_ee-v2_int-v2_ca_bc.der'), error);
  check_cert_err(cert_from_file('v2_ee-v2_int-v2_ca_bc.der'), error);
  check_cert_err(cert_from_file('v3_missing_bc_ee-v2_int-v2_ca_bc.der'), error);
  check_cert_err(cert_from_file('v3_bc_ee-v2_int-v2_ca_bc.der'), error);
  check_cert_err(cert_from_file('v1_bc_ee-v2_int-v2_ca_bc.der'), error);
  check_cert_err(cert_from_file('v2_bc_ee-v2_int-v2_ca_bc.der'), error);
  check_cert_err(cert_from_file('v4_bc_ee-v2_int-v2_ca_bc.der'), error);

  // v2 ca, v2 intermediate with bc
  check_ok_ca(cert_from_file('v2_int_bc-v2_ca_bc.der'));
  check_ok(cert_from_file('v1_ee-v2_int_bc-v2_ca_bc.der'));
  check_ok(cert_from_file('v1_bc_ee-v2_int_bc-v2_ca_bc.der'));
  check_ok(cert_from_file('v2_ee-v2_int_bc-v2_ca_bc.der'));
  check_ok(cert_from_file('v2_bc_ee-v2_int_bc-v2_ca_bc.der'));
  check_ok(cert_from_file('v3_missing_bc_ee-v2_int_bc-v2_ca_bc.der'));
  check_ok(cert_from_file('v3_bc_ee-v2_int_bc-v2_ca_bc.der'));
  check_ok(cert_from_file('v4_bc_ee-v2_int_bc-v2_ca_bc.der'));

  // v2 ca, invalid v3 intermediate
  error = SEC_ERROR_CA_CERT_INVALID;
  check_ca_err(cert_from_file('v3_int_missing_bc-v2_ca_bc.der'), error);
  check_cert_err(cert_from_file('v1_ee-v3_int_missing_bc-v2_ca_bc.der'), error);
  check_cert_err(cert_from_file('v2_ee-v3_int_missing_bc-v2_ca_bc.der'), error);
  check_cert_err(cert_from_file('v3_missing_bc_ee-v3_int_missing_bc-v2_ca_bc.der'), error);
  check_cert_err(cert_from_file('v3_bc_ee-v3_int_missing_bc-v2_ca_bc.der'), error);
  check_cert_err(cert_from_file('v1_bc_ee-v3_int_missing_bc-v2_ca_bc.der'), error);
  check_cert_err(cert_from_file('v2_bc_ee-v3_int_missing_bc-v2_ca_bc.der'), error);
  check_cert_err(cert_from_file('v4_bc_ee-v3_int_missing_bc-v2_ca_bc.der'), error);

  // v2 ca, valid v3 intermediate
  check_ok_ca(cert_from_file('v3_int-v2_ca_bc.der'));
  check_ok(cert_from_file('v1_ee-v3_int-v2_ca_bc.der'));
  check_ok(cert_from_file('v1_bc_ee-v3_int-v2_ca_bc.der'));
  check_ok(cert_from_file('v2_ee-v3_int-v2_ca_bc.der'));
  check_ok(cert_from_file('v2_bc_ee-v3_int-v2_ca_bc.der'));
  check_ok(cert_from_file('v3_missing_bc_ee-v3_int-v2_ca_bc.der'));
  check_ok(cert_from_file('v3_bc_ee-v3_int-v2_ca_bc.der'));
  check_ok(cert_from_file('v4_bc_ee-v3_int-v2_ca_bc.der'));

  //////////////
  // v3 CA supersection
  //////////////////

  // v3 ca, v1 intermediate
  error = MOZILLA_PKIX_ERROR_V1_CERT_USED_AS_CA;
  check_ca_err(cert_from_file('v1_int-v3_ca.der'), error);
  check_cert_err(cert_from_file('v1_ee-v1_int-v3_ca.der'), error);
  check_cert_err(cert_from_file('v2_ee-v1_int-v3_ca.der'), error);
  check_cert_err(cert_from_file('v3_missing_bc_ee-v1_int-v3_ca.der'), error);
  check_cert_err(cert_from_file('v3_bc_ee-v1_int-v3_ca.der'), error);
  check_cert_err(cert_from_file('v1_bc_ee-v1_int-v3_ca.der'), error);
  check_cert_err(cert_from_file('v2_bc_ee-v1_int-v3_ca.der'), error);
  check_cert_err(cert_from_file('v4_bc_ee-v1_int-v3_ca.der'), error);

  // A v1 intermediate with v3 extensions
  check_ok_ca(cert_from_file('v1_int_bc-v3_ca.der'));
  check_ok(cert_from_file('v1_ee-v1_int_bc-v3_ca.der'));
  check_ok(cert_from_file('v1_bc_ee-v1_int_bc-v3_ca.der'));
  check_ok(cert_from_file('v2_ee-v1_int_bc-v3_ca.der'));
  check_ok(cert_from_file('v2_bc_ee-v1_int_bc-v3_ca.der'));
  check_ok(cert_from_file('v3_missing_bc_ee-v1_int_bc-v3_ca.der'));
  check_ok(cert_from_file('v3_bc_ee-v1_int_bc-v3_ca.der'));
  check_ok(cert_from_file('v4_bc_ee-v1_int_bc-v3_ca.der'));

  // reject a v2 cert as intermediate
  error = SEC_ERROR_CA_CERT_INVALID;
  check_ca_err(cert_from_file('v2_int-v3_ca.der'), error);
  check_cert_err(cert_from_file('v1_ee-v2_int-v3_ca.der'), error);
  check_cert_err(cert_from_file('v2_ee-v2_int-v3_ca.der'), error);
  check_cert_err(cert_from_file('v3_missing_bc_ee-v2_int-v3_ca.der'), error);
  check_cert_err(cert_from_file('v3_bc_ee-v2_int-v3_ca.der'), error);
  check_cert_err(cert_from_file('v1_bc_ee-v2_int-v3_ca.der'), error);
  check_cert_err(cert_from_file('v2_bc_ee-v2_int-v3_ca.der'), error);
  check_cert_err(cert_from_file('v4_bc_ee-v2_int-v3_ca.der'), error);

  // v2 intermediate with bc (invalid)
  check_ok_ca(cert_from_file('v2_int_bc-v3_ca.der'));
  check_ok(cert_from_file('v1_ee-v2_int_bc-v3_ca.der'));
  check_ok(cert_from_file('v1_bc_ee-v2_int_bc-v3_ca.der'));
  check_ok(cert_from_file('v2_ee-v2_int_bc-v3_ca.der'));
  check_ok(cert_from_file('v2_bc_ee-v2_int_bc-v3_ca.der'));
  check_ok(cert_from_file('v3_missing_bc_ee-v2_int_bc-v3_ca.der'));
  check_ok(cert_from_file('v3_bc_ee-v2_int_bc-v3_ca.der'));
  check_ok(cert_from_file('v4_bc_ee-v2_int_bc-v3_ca.der'));

  // invalid v3 intermediate
  error = SEC_ERROR_CA_CERT_INVALID;
  check_ca_err(cert_from_file('v3_int_missing_bc-v3_ca.der'), error);
  check_cert_err(cert_from_file('v1_ee-v3_int_missing_bc-v3_ca.der'), error);
  check_cert_err(cert_from_file('v2_ee-v3_int_missing_bc-v3_ca.der'), error);
  check_cert_err(cert_from_file('v3_missing_bc_ee-v3_int_missing_bc-v3_ca.der'), error);
  check_cert_err(cert_from_file('v3_bc_ee-v3_int_missing_bc-v3_ca.der'), error);
  check_cert_err(cert_from_file('v1_bc_ee-v3_int_missing_bc-v3_ca.der'), error);
  check_cert_err(cert_from_file('v2_bc_ee-v3_int_missing_bc-v3_ca.der'), error);
  check_cert_err(cert_from_file('v4_bc_ee-v3_int_missing_bc-v3_ca.der'), error);

  // v1/v2 end entity, v3 intermediate
  check_ok_ca(cert_from_file('v3_int-v3_ca.der'));
  check_ok(cert_from_file('v1_ee-v3_int-v3_ca.der'));
  check_ok(cert_from_file('v2_ee-v3_int-v3_ca.der'));
  check_ok(cert_from_file('v3_missing_bc_ee-v3_int-v3_ca.der'));
  check_ok(cert_from_file('v3_bc_ee-v3_int-v3_ca.der'));
  check_ok(cert_from_file('v1_bc_ee-v3_int-v3_ca.der'));
  check_ok(cert_from_file('v2_bc_ee-v3_int-v3_ca.der'));
  check_ok(cert_from_file('v4_bc_ee-v3_int-v3_ca.der'));

  // v3 CA, invalid v3 intermediate
  error = MOZILLA_PKIX_ERROR_V1_CERT_USED_AS_CA;
  check_ca_err(cert_from_file('v1_int-v3_ca_missing_bc.der'), error);
  check_cert_err(cert_from_file('v1_ee-v1_int-v3_ca_missing_bc.der'), error);
  check_cert_err(cert_from_file('v2_ee-v1_int-v3_ca_missing_bc.der'), error);
  check_cert_err(cert_from_file('v3_missing_bc_ee-v1_int-v3_ca_missing_bc.der'), error);
  check_cert_err(cert_from_file('v3_bc_ee-v1_int-v3_ca_missing_bc.der'), error);
  check_cert_err(cert_from_file('v1_bc_ee-v1_int-v3_ca_missing_bc.der'), error);
  check_cert_err(cert_from_file('v2_bc_ee-v1_int-v3_ca_missing_bc.der'), error);
  check_cert_err(cert_from_file('v4_bc_ee-v1_int-v3_ca_missing_bc.der'), error);

  // Int v1 with BC that is just invalid
  error = SEC_ERROR_CA_CERT_INVALID;
  check_ca_err(cert_from_file('v1_int_bc-v3_ca_missing_bc.der'), error);
  check_cert_err(cert_from_file('v1_ee-v1_int_bc-v3_ca_missing_bc.der'), error);
  check_cert_err(cert_from_file('v1_bc_ee-v1_int_bc-v3_ca_missing_bc.der'), error);
  check_cert_err(cert_from_file('v2_ee-v1_int_bc-v3_ca_missing_bc.der'), error);
  check_cert_err(cert_from_file('v2_bc_ee-v1_int_bc-v3_ca_missing_bc.der'), error);
  check_cert_err(cert_from_file('v3_missing_bc_ee-v1_int_bc-v3_ca_missing_bc.der'), error);
  check_cert_err(cert_from_file('v3_bc_ee-v1_int_bc-v3_ca_missing_bc.der'), error);
  check_cert_err(cert_from_file('v4_bc_ee-v1_int_bc-v3_ca_missing_bc.der'), error);

  // Good section (all fail)
  error = SEC_ERROR_CA_CERT_INVALID;
  check_ca_err(cert_from_file('v2_int-v3_ca_missing_bc.der'), error);
  check_cert_err(cert_from_file('v1_ee-v2_int-v3_ca_missing_bc.der'), error);
  check_cert_err(cert_from_file('v2_ee-v2_int-v3_ca_missing_bc.der'), error);
  check_cert_err(cert_from_file('v3_missing_bc_ee-v2_int-v3_ca_missing_bc.der'), error);
  check_cert_err(cert_from_file('v3_bc_ee-v2_int-v3_ca_missing_bc.der'), error);
  check_cert_err(cert_from_file('v1_bc_ee-v2_int-v3_ca_missing_bc.der'), error);
  check_cert_err(cert_from_file('v2_bc_ee-v2_int-v3_ca_missing_bc.der'), error);
  check_cert_err(cert_from_file('v4_bc_ee-v2_int-v3_ca_missing_bc.der'), error);

  // v3 intermediate missing basic constraints is invalid
  error = SEC_ERROR_CA_CERT_INVALID;
  check_ca_err(cert_from_file('v2_int_bc-v3_ca_missing_bc.der'), error);
  check_cert_err(cert_from_file('v1_ee-v2_int_bc-v3_ca_missing_bc.der'), error);
  check_cert_err(cert_from_file('v1_bc_ee-v2_int_bc-v3_ca_missing_bc.der'), error);
  check_cert_err(cert_from_file('v2_ee-v2_int_bc-v3_ca_missing_bc.der'), error);
  check_cert_err(cert_from_file('v2_bc_ee-v2_int_bc-v3_ca_missing_bc.der'), error);
  check_cert_err(cert_from_file('v3_missing_bc_ee-v2_int_bc-v3_ca_missing_bc.der'), error);
  check_cert_err(cert_from_file('v3_bc_ee-v2_int_bc-v3_ca_missing_bc.der'), error);
  check_cert_err(cert_from_file('v4_bc_ee-v2_int_bc-v3_ca_missing_bc.der'), error);

  // v3 intermediate missing basic constraints is invalid
  error = SEC_ERROR_CA_CERT_INVALID;
  check_ca_err(cert_from_file('v3_int_missing_bc-v3_ca_missing_bc.der'), error);
  check_cert_err(cert_from_file('v1_ee-v3_int_missing_bc-v3_ca_missing_bc.der'), error);
  check_cert_err(cert_from_file('v2_ee-v3_int_missing_bc-v3_ca_missing_bc.der'), error);
  check_cert_err(cert_from_file('v3_missing_bc_ee-v3_int_missing_bc-v3_ca_missing_bc.der'), error);
  check_cert_err(cert_from_file('v3_bc_ee-v3_int_missing_bc-v3_ca_missing_bc.der'), error);
  check_cert_err(cert_from_file('v1_bc_ee-v3_int_missing_bc-v3_ca_missing_bc.der'), error);
  check_cert_err(cert_from_file('v2_bc_ee-v3_int_missing_bc-v3_ca_missing_bc.der'), error);
  check_cert_err(cert_from_file('v4_bc_ee-v3_int_missing_bc-v3_ca_missing_bc.der'), error);

  // With a v3 root missing bc and valid v3 intermediate
  error = SEC_ERROR_CA_CERT_INVALID;
  check_ca_err(cert_from_file('v3_int-v3_ca_missing_bc.der'), error);
  check_cert_err(cert_from_file('v1_ee-v3_int-v3_ca_missing_bc.der'), error);
  check_cert_err(cert_from_file('v2_ee-v3_int-v3_ca_missing_bc.der'), error);
  check_cert_err(cert_from_file('v3_missing_bc_ee-v3_int-v3_ca_missing_bc.der'), error);
  check_cert_err(cert_from_file('v3_bc_ee-v3_int-v3_ca_missing_bc.der'), error);
  check_cert_err(cert_from_file('v1_bc_ee-v3_int-v3_ca_missing_bc.der'), error);
  check_cert_err(cert_from_file('v2_bc_ee-v3_int-v3_ca_missing_bc.der'), error);
  check_cert_err(cert_from_file('v4_bc_ee-v3_int-v3_ca_missing_bc.der'), error);

  // self-signed
  check_cert_err(cert_from_file('v1_self_signed.der'), SEC_ERROR_UNKNOWN_ISSUER);
  check_cert_err(cert_from_file('v1_self_signed_bc.der'), SEC_ERROR_UNKNOWN_ISSUER);
  check_cert_err(cert_from_file('v2_self_signed.der'), SEC_ERROR_UNKNOWN_ISSUER);
  check_cert_err(cert_from_file('v2_self_signed_bc.der'), SEC_ERROR_UNKNOWN_ISSUER);
  check_cert_err(cert_from_file('v3_self_signed.der'), SEC_ERROR_UNKNOWN_ISSUER);
  check_cert_err(cert_from_file('v3_self_signed_bc.der'), SEC_ERROR_UNKNOWN_ISSUER);
  check_cert_err(cert_from_file('v4_self_signed.der'), SEC_ERROR_UNKNOWN_ISSUER);
  check_cert_err(cert_from_file('v4_self_signed_bc.der'), SEC_ERROR_UNKNOWN_ISSUER);
}
