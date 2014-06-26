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
  let error = certdb.verifyCertNow(cert, usage,
                                   NO_FLAGS, verifiedChain, hasEVPolicy);
  do_check_eq(error, expected_error);
}

function check_cert_err(cert, expected_error) {
  check_cert_err_generic(cert, expected_error, certificateUsageSSLServer)
}

function check_ca_err(cert, expected_error) {
  check_cert_err_generic(cert, expected_error, certificateUsageSSLCA)
}

function check_ok(x) {
  return check_cert_err(x, 0);
}

function check_ok_ca(x) {
  return check_cert_err_generic(x, 0, certificateUsageSSLCA);
}

function run_test() {
  load_cert("v1_ca", "CTu,CTu,CTu");
  load_cert("v1_ca_bc", "CTu,CTu,CTu");
  load_cert("v2_ca", "CTu,CTu,CTu");
  load_cert("v2_ca_bc", "CTu,CTu,CTu");
  load_cert("v3_ca", "CTu,CTu,CTu");
  load_cert("v3_ca_missing_bc", "CTu,CTu,CTu");

  check_ok_ca(cert_from_file('v1_ca.der'));
  check_ca_err(cert_from_file('v1_ca_bc.der'), SEC_ERROR_EXTENSION_VALUE_INVALID);
  check_ca_err(cert_from_file('v2_ca.der'), SEC_ERROR_CA_CERT_INVALID);
  check_ca_err(cert_from_file('v2_ca_bc.der'), SEC_ERROR_EXTENSION_VALUE_INVALID);
  check_ok_ca(cert_from_file('v3_ca.der'));
  check_ca_err(cert_from_file('v3_ca_missing_bc.der'), SEC_ERROR_CA_CERT_INVALID);

  // Classic allows v1 and v2 certs to be CA certs in trust anchor positions and
  // intermediates when they have a v3 basic constraints extenstion (which
  // makes them invalid certs). Insanity only allows v1 certs to be CA in
  // anchor position (even if they have invalid encodings), v2 certs are not
  // considered CAs in any position.
  // Note that currently there are no change of behavior based on the
  // version of the end entity.

  let ee_error = 0;
  let ca_error = 0;

  //////////////
  // v1 CA supersection
  //////////////////

  // v1 intermediate with v1 trust anchor
  ca_error = SEC_ERROR_CA_CERT_INVALID;
  ee_error = SEC_ERROR_CA_CERT_INVALID;
  check_ca_err(cert_from_file('v1_int-v1_ca.der'), ca_error);
  check_cert_err(cert_from_file('v1_ee-v1_int-v1_ca.der'), ee_error);
  check_cert_err(cert_from_file('v2_ee-v1_int-v1_ca.der'), ee_error);
  check_cert_err(cert_from_file('v3_missing_bc_ee-v1_int-v1_ca.der'), ee_error);
  check_cert_err(cert_from_file('v3_bc_ee-v1_int-v1_ca.der'), ee_error);
  ee_error = SEC_ERROR_EXTENSION_VALUE_INVALID;
  check_cert_err(cert_from_file('v1_bc_ee-v1_int-v1_ca.der'), ee_error);
  check_cert_err(cert_from_file('v2_bc_ee-v1_int-v1_ca.der'), ee_error);
  check_cert_err(cert_from_file('v4_bc_ee-v1_int-v1_ca.der'), SEC_ERROR_BAD_DER);

  // v1 intermediate with v3 extensions. CA is invalid.
  ca_error = SEC_ERROR_EXTENSION_VALUE_INVALID;
  ee_error = SEC_ERROR_EXTENSION_VALUE_INVALID;
  check_ca_err(cert_from_file('v1_int_bc-v1_ca.der'), ca_error);
  check_cert_err(cert_from_file('v1_ee-v1_int_bc-v1_ca.der'), ee_error);
  check_cert_err(cert_from_file('v1_bc_ee-v1_int_bc-v1_ca.der'), ee_error);
  check_cert_err(cert_from_file('v2_ee-v1_int_bc-v1_ca.der'), ee_error);
  check_cert_err(cert_from_file('v2_bc_ee-v1_int_bc-v1_ca.der'), ee_error);
  check_cert_err(cert_from_file('v3_missing_bc_ee-v1_int_bc-v1_ca.der'), ee_error);
  check_cert_err(cert_from_file('v3_bc_ee-v1_int_bc-v1_ca.der'), ee_error);
  check_cert_err(cert_from_file('v4_bc_ee-v1_int_bc-v1_ca.der'), SEC_ERROR_BAD_DER);

  // A v2 intermediate with a v1 CA
  ca_error = SEC_ERROR_CA_CERT_INVALID;
  ee_error = SEC_ERROR_CA_CERT_INVALID;
  check_ca_err(cert_from_file('v2_int-v1_ca.der'), ca_error);
  check_cert_err(cert_from_file('v1_ee-v2_int-v1_ca.der'), ee_error);
  check_cert_err(cert_from_file('v2_ee-v2_int-v1_ca.der'), ee_error);
  check_cert_err(cert_from_file('v3_missing_bc_ee-v2_int-v1_ca.der'), ee_error);
  check_cert_err(cert_from_file('v3_bc_ee-v2_int-v1_ca.der'), ee_error);
  ee_error = SEC_ERROR_EXTENSION_VALUE_INVALID;
  check_cert_err(cert_from_file('v1_bc_ee-v2_int-v1_ca.der'), ee_error);
  check_cert_err(cert_from_file('v2_bc_ee-v2_int-v1_ca.der'), ee_error);
  check_cert_err(cert_from_file('v4_bc_ee-v2_int-v1_ca.der'), SEC_ERROR_BAD_DER);

  // A v2 intermediate with basic constraints (not allowed in insanity)
  ca_error = SEC_ERROR_EXTENSION_VALUE_INVALID;
  ee_error = SEC_ERROR_EXTENSION_VALUE_INVALID;
  check_ca_err(cert_from_file('v2_int_bc-v1_ca.der'), ca_error);
  check_cert_err(cert_from_file('v1_ee-v2_int_bc-v1_ca.der'), ee_error);
  check_cert_err(cert_from_file('v1_bc_ee-v2_int_bc-v1_ca.der'), ee_error);
  check_cert_err(cert_from_file('v2_ee-v2_int_bc-v1_ca.der'), ee_error);
  check_cert_err(cert_from_file('v2_bc_ee-v2_int_bc-v1_ca.der'), ee_error);
  check_cert_err(cert_from_file('v3_missing_bc_ee-v2_int_bc-v1_ca.der'), ee_error);
  check_cert_err(cert_from_file('v3_bc_ee-v2_int_bc-v1_ca.der'), ee_error);
  check_cert_err(cert_from_file('v4_bc_ee-v2_int_bc-v1_ca.der'), SEC_ERROR_BAD_DER);

  // Section is OK. A x509 v3 CA MUST have bc
  // http://tools.ietf.org/html/rfc5280#section-4.2.1.9
  ca_error = SEC_ERROR_CA_CERT_INVALID;
  ee_error = SEC_ERROR_CA_CERT_INVALID;
  check_ca_err(cert_from_file('v3_int_missing_bc-v1_ca.der'), ca_error);
  check_cert_err(cert_from_file('v1_ee-v3_int_missing_bc-v1_ca.der'), ee_error);
  check_cert_err(cert_from_file('v2_ee-v3_int_missing_bc-v1_ca.der'), ee_error);
  check_cert_err(cert_from_file('v3_missing_bc_ee-v3_int_missing_bc-v1_ca.der'), ee_error);
  check_cert_err(cert_from_file('v3_bc_ee-v3_int_missing_bc-v1_ca.der'), ee_error);
  ee_error = SEC_ERROR_EXTENSION_VALUE_INVALID;
  check_cert_err(cert_from_file('v1_bc_ee-v3_int_missing_bc-v1_ca.der'), ee_error);
  check_cert_err(cert_from_file('v2_bc_ee-v3_int_missing_bc-v1_ca.der'), ee_error);
  check_cert_err(cert_from_file('v4_bc_ee-v3_int_missing_bc-v1_ca.der'), SEC_ERROR_BAD_DER);

  // It is valid for a v1 ca to sign a v3 intemediate.
  check_ok_ca(cert_from_file('v3_int-v1_ca.der'));
  check_ok(cert_from_file('v1_ee-v3_int-v1_ca.der'));
  check_ok(cert_from_file('v2_ee-v3_int-v1_ca.der'));
  check_ok(cert_from_file('v3_missing_bc_ee-v3_int-v1_ca.der'));
  check_ok(cert_from_file('v3_bc_ee-v3_int-v1_ca.der'));
  ca_error = SEC_ERROR_EXTENSION_VALUE_INVALID;
  ee_error = SEC_ERROR_EXTENSION_VALUE_INVALID;
  check_cert_err(cert_from_file('v1_bc_ee-v3_int-v1_ca.der'), ee_error);
  check_cert_err(cert_from_file('v2_bc_ee-v3_int-v1_ca.der'), ee_error);
  check_cert_err(cert_from_file('v4_bc_ee-v3_int-v1_ca.der'), SEC_ERROR_BAD_DER);

  // The next groups change the v1 ca for a v1 ca with base constraints
  // (invalid trust anchor). The error pattern is the same as the groups
  // above

  // Using A v1 intermediate
  ca_error = SEC_ERROR_CA_CERT_INVALID;
  ee_error = SEC_ERROR_CA_CERT_INVALID;
  check_ca_err(cert_from_file('v1_int-v1_ca_bc.der'), ca_error);
  check_cert_err(cert_from_file('v1_ee-v1_int-v1_ca_bc.der'), ee_error);
  check_cert_err(cert_from_file('v2_ee-v1_int-v1_ca_bc.der'), ee_error);
  check_cert_err(cert_from_file('v3_missing_bc_ee-v1_int-v1_ca_bc.der'), ee_error);
  check_cert_err(cert_from_file('v3_bc_ee-v1_int-v1_ca_bc.der'), ee_error);
  ee_error = SEC_ERROR_EXTENSION_VALUE_INVALID;
  check_cert_err(cert_from_file('v1_bc_ee-v1_int-v1_ca_bc.der'), ee_error);
  check_cert_err(cert_from_file('v2_bc_ee-v1_int-v1_ca_bc.der'), ee_error);
  check_cert_err(cert_from_file('v4_bc_ee-v1_int-v1_ca_bc.der'), SEC_ERROR_BAD_DER);

  // Using a v1 intermediate with v3 extenstions (invalid).
  ca_error = SEC_ERROR_EXTENSION_VALUE_INVALID;
  ee_error = SEC_ERROR_EXTENSION_VALUE_INVALID;
  check_ca_err(cert_from_file('v1_int_bc-v1_ca_bc.der'), ca_error);
  check_cert_err(cert_from_file('v1_ee-v1_int_bc-v1_ca_bc.der'), ee_error);
  check_cert_err(cert_from_file('v1_bc_ee-v1_int_bc-v1_ca_bc.der'), ee_error);
  check_cert_err(cert_from_file('v2_ee-v1_int_bc-v1_ca_bc.der'), ee_error);
  check_cert_err(cert_from_file('v2_bc_ee-v1_int_bc-v1_ca_bc.der'), ee_error);
  check_cert_err(cert_from_file('v3_missing_bc_ee-v1_int_bc-v1_ca_bc.der'), ee_error);
  check_cert_err(cert_from_file('v3_bc_ee-v1_int_bc-v1_ca_bc.der'), ee_error);
  check_cert_err(cert_from_file('v4_bc_ee-v1_int_bc-v1_ca_bc.der'), SEC_ERROR_BAD_DER);

  // Using v2 intermediate
  ca_error = SEC_ERROR_CA_CERT_INVALID;
  ee_error = SEC_ERROR_CA_CERT_INVALID;
  check_ca_err(cert_from_file('v2_int-v1_ca_bc.der'), ca_error);
  check_cert_err(cert_from_file('v1_ee-v2_int-v1_ca_bc.der'), ee_error);
  check_cert_err(cert_from_file('v2_ee-v2_int-v1_ca_bc.der'), ee_error);
  check_cert_err(cert_from_file('v3_missing_bc_ee-v2_int-v1_ca_bc.der'), ee_error);
  check_cert_err(cert_from_file('v3_bc_ee-v2_int-v1_ca_bc.der'), ee_error);
  ee_error = SEC_ERROR_EXTENSION_VALUE_INVALID;
  check_cert_err(cert_from_file('v1_bc_ee-v2_int-v1_ca_bc.der'), ee_error);
  check_cert_err(cert_from_file('v2_bc_ee-v2_int-v1_ca_bc.der'), ee_error);
  check_cert_err(cert_from_file('v4_bc_ee-v2_int-v1_ca_bc.der'), SEC_ERROR_BAD_DER);

  // Using a v2 intermediate with basic constraints (invalid)
  ca_error = SEC_ERROR_EXTENSION_VALUE_INVALID;
  ee_error = SEC_ERROR_EXTENSION_VALUE_INVALID;
  check_ca_err(cert_from_file('v2_int_bc-v1_ca_bc.der'), ca_error);
  check_cert_err(cert_from_file('v1_ee-v2_int_bc-v1_ca_bc.der'), ee_error);
  check_cert_err(cert_from_file('v1_bc_ee-v2_int_bc-v1_ca_bc.der'), ee_error);
  check_cert_err(cert_from_file('v2_ee-v2_int_bc-v1_ca_bc.der'), ee_error);
  check_cert_err(cert_from_file('v2_bc_ee-v2_int_bc-v1_ca_bc.der'), ee_error);
  check_cert_err(cert_from_file('v3_missing_bc_ee-v2_int_bc-v1_ca_bc.der'), ee_error);
  check_cert_err(cert_from_file('v3_bc_ee-v2_int_bc-v1_ca_bc.der'), ee_error);
  check_cert_err(cert_from_file('v4_bc_ee-v2_int_bc-v1_ca_bc.der'), SEC_ERROR_BAD_DER);

  // Using a v3 intermediate that is missing basic constraints (invalid)
  ca_error = SEC_ERROR_CA_CERT_INVALID;
  ee_error = SEC_ERROR_CA_CERT_INVALID;
  check_ca_err(cert_from_file('v3_int_missing_bc-v1_ca_bc.der'), ca_error);
  check_cert_err(cert_from_file('v1_ee-v3_int_missing_bc-v1_ca_bc.der'), ee_error);
  check_cert_err(cert_from_file('v2_ee-v3_int_missing_bc-v1_ca_bc.der'), ee_error);
  check_cert_err(cert_from_file('v3_missing_bc_ee-v3_int_missing_bc-v1_ca_bc.der'), ee_error);
  check_cert_err(cert_from_file('v3_bc_ee-v3_int_missing_bc-v1_ca_bc.der'), ee_error);
  ee_error = SEC_ERROR_EXTENSION_VALUE_INVALID;
  check_cert_err(cert_from_file('v1_bc_ee-v3_int_missing_bc-v1_ca_bc.der'), ee_error);
  check_cert_err(cert_from_file('v2_bc_ee-v3_int_missing_bc-v1_ca_bc.der'), ee_error);
  check_cert_err(cert_from_file('v4_bc_ee-v3_int_missing_bc-v1_ca_bc.der'), SEC_ERROR_BAD_DER);

  // these should pass assuming we are OK with v1 ca signing v3 intermediates
  ca_error = SEC_ERROR_EXTENSION_VALUE_INVALID;
  ee_error = SEC_ERROR_EXTENSION_VALUE_INVALID;
  check_ca_err(cert_from_file('v3_int-v1_ca_bc.der'), ca_error);
  check_cert_err(cert_from_file('v1_ee-v3_int-v1_ca_bc.der'), ee_error);
  check_cert_err(cert_from_file('v1_bc_ee-v3_int-v1_ca_bc.der'), ee_error);
  check_cert_err(cert_from_file('v2_ee-v3_int-v1_ca_bc.der'), ee_error);
  check_cert_err(cert_from_file('v2_bc_ee-v3_int-v1_ca_bc.der'), ee_error);
  check_cert_err(cert_from_file('v3_missing_bc_ee-v3_int-v1_ca_bc.der'), ee_error);
  check_cert_err(cert_from_file('v3_bc_ee-v3_int-v1_ca_bc.der'), ee_error);
  check_cert_err(cert_from_file('v4_bc_ee-v3_int-v1_ca_bc.der'), SEC_ERROR_BAD_DER);


  //////////////
  // v2 CA supersection
  //////////////////

  // v2 ca, v1 intermediate
  ca_error = SEC_ERROR_CA_CERT_INVALID;
  ee_error = SEC_ERROR_CA_CERT_INVALID;
  check_ca_err(cert_from_file('v1_int-v2_ca.der'), ca_error);
  check_cert_err(cert_from_file('v1_ee-v1_int-v2_ca.der'), ee_error);
  check_cert_err(cert_from_file('v2_ee-v1_int-v2_ca.der'), ee_error);
  check_cert_err(cert_from_file('v3_missing_bc_ee-v1_int-v2_ca.der'), ee_error);
  check_cert_err(cert_from_file('v3_bc_ee-v1_int-v2_ca.der'), ee_error);
  ee_error = SEC_ERROR_EXTENSION_VALUE_INVALID;
  check_cert_err(cert_from_file('v1_bc_ee-v1_int-v2_ca.der'), ee_error)
  check_cert_err(cert_from_file('v2_bc_ee-v1_int-v2_ca.der'), ee_error);
  check_cert_err(cert_from_file('v4_bc_ee-v1_int-v2_ca.der'), SEC_ERROR_BAD_DER);

  // v2 ca, v1 intermediate with basic constraints (invalid)
  ca_error = SEC_ERROR_EXTENSION_VALUE_INVALID;
  ee_error = SEC_ERROR_EXTENSION_VALUE_INVALID;
  check_ca_err(cert_from_file('v1_int_bc-v2_ca.der'), ca_error);
  check_cert_err(cert_from_file('v1_ee-v1_int_bc-v2_ca.der'), ee_error);
  check_cert_err(cert_from_file('v1_bc_ee-v1_int_bc-v2_ca.der'), ee_error);
  check_cert_err(cert_from_file('v2_ee-v1_int_bc-v2_ca.der'), ee_error);
  check_cert_err(cert_from_file('v2_bc_ee-v1_int_bc-v2_ca.der'), ee_error);
  check_cert_err(cert_from_file('v3_missing_bc_ee-v1_int_bc-v2_ca.der'), ee_error);
  check_cert_err(cert_from_file('v3_bc_ee-v1_int_bc-v2_ca.der'), ee_error);
  check_cert_err(cert_from_file('v4_bc_ee-v1_int_bc-v2_ca.der'), SEC_ERROR_BAD_DER);

  // v2 ca, v2 intermediate
  ca_error = SEC_ERROR_CA_CERT_INVALID;
  ee_error = SEC_ERROR_CA_CERT_INVALID;
  check_ca_err(cert_from_file('v2_int-v2_ca.der'), ca_error);
  check_cert_err(cert_from_file('v1_ee-v2_int-v2_ca.der'), ee_error);
  check_cert_err(cert_from_file('v2_ee-v2_int-v2_ca.der'), ee_error);
  check_cert_err(cert_from_file('v3_missing_bc_ee-v2_int-v2_ca.der'), ee_error);
  check_cert_err(cert_from_file('v3_bc_ee-v2_int-v2_ca.der'), ee_error);
  ee_error = SEC_ERROR_EXTENSION_VALUE_INVALID;
  check_cert_err(cert_from_file('v1_bc_ee-v2_int-v2_ca.der'), ee_error);
  check_cert_err(cert_from_file('v2_bc_ee-v2_int-v2_ca.der'), ee_error);
  check_cert_err(cert_from_file('v4_bc_ee-v2_int-v2_ca.der'), SEC_ERROR_BAD_DER);

  // v2 ca, v2 intermediate with basic constraints (invalid)
  ca_error = SEC_ERROR_EXTENSION_VALUE_INVALID;
  ee_error = SEC_ERROR_EXTENSION_VALUE_INVALID;
  check_ca_err(cert_from_file('v2_int_bc-v2_ca.der'), ca_error);
  check_cert_err(cert_from_file('v1_ee-v2_int_bc-v2_ca.der'), ee_error);
  check_cert_err(cert_from_file('v1_bc_ee-v2_int_bc-v2_ca.der'), ee_error);
  check_cert_err(cert_from_file('v2_ee-v2_int_bc-v2_ca.der'), ee_error);
  check_cert_err(cert_from_file('v2_bc_ee-v2_int_bc-v2_ca.der'), ee_error);
  check_cert_err(cert_from_file('v3_missing_bc_ee-v2_int_bc-v2_ca.der'), ee_error);
  check_cert_err(cert_from_file('v3_bc_ee-v2_int_bc-v2_ca.der'), ee_error);
  check_cert_err(cert_from_file('v4_bc_ee-v2_int_bc-v2_ca.der'), SEC_ERROR_BAD_DER);

  // v2 ca, v3 intermediate missing basic constraints
  ca_error = SEC_ERROR_CA_CERT_INVALID;
  ee_error = SEC_ERROR_CA_CERT_INVALID;
  check_ca_err(cert_from_file('v3_int_missing_bc-v2_ca.der'), ca_error);
  check_cert_err(cert_from_file('v1_ee-v3_int_missing_bc-v2_ca.der'), ee_error);
  check_cert_err(cert_from_file('v2_ee-v3_int_missing_bc-v2_ca.der'), ee_error);
  check_cert_err(cert_from_file('v3_missing_bc_ee-v3_int_missing_bc-v2_ca.der'), ee_error);
  check_cert_err(cert_from_file('v3_bc_ee-v3_int_missing_bc-v2_ca.der'), ee_error);
  ee_error = SEC_ERROR_EXTENSION_VALUE_INVALID;
  check_cert_err(cert_from_file('v1_bc_ee-v3_int_missing_bc-v2_ca.der'), ee_error);
  check_cert_err(cert_from_file('v2_bc_ee-v3_int_missing_bc-v2_ca.der'), ee_error);
  check_cert_err(cert_from_file('v4_bc_ee-v3_int_missing_bc-v2_ca.der'), SEC_ERROR_BAD_DER);

  // v2 ca, v3 intermediate
  ca_error = SEC_ERROR_CA_CERT_INVALID;
  ee_error = SEC_ERROR_CA_CERT_INVALID;
  check_ca_err(cert_from_file('v3_int-v2_ca.der'), ca_error);
  check_cert_err(cert_from_file('v1_ee-v3_int-v2_ca.der'), ee_error);
  check_cert_err(cert_from_file('v2_ee-v3_int-v2_ca.der'), ee_error);
  check_cert_err(cert_from_file('v3_missing_bc_ee-v3_int-v2_ca.der'), ee_error);
  check_cert_err(cert_from_file('v3_bc_ee-v3_int-v2_ca.der'), ee_error);
  ca_error = SEC_ERROR_EXTENSION_VALUE_INVALID;
  ee_error = SEC_ERROR_EXTENSION_VALUE_INVALID;
  check_cert_err(cert_from_file('v1_bc_ee-v3_int-v2_ca.der'), ee_error);
  check_cert_err(cert_from_file('v2_bc_ee-v3_int-v2_ca.der'), ee_error);
  check_cert_err(cert_from_file('v4_bc_ee-v3_int-v2_ca.der'), SEC_ERROR_BAD_DER);

  // v2 ca, v1 intermediate
  ca_error = SEC_ERROR_CA_CERT_INVALID;
  ee_error = SEC_ERROR_CA_CERT_INVALID;
  check_ca_err(cert_from_file('v1_int-v2_ca_bc.der'), ca_error);
  check_cert_err(cert_from_file('v1_ee-v1_int-v2_ca_bc.der'), ee_error);
  check_cert_err(cert_from_file('v2_ee-v1_int-v2_ca_bc.der'), ee_error);
  check_cert_err(cert_from_file('v3_missing_bc_ee-v1_int-v2_ca_bc.der'), ee_error);
  check_cert_err(cert_from_file('v3_bc_ee-v1_int-v2_ca_bc.der'), ee_error);
  ee_error = SEC_ERROR_EXTENSION_VALUE_INVALID;
  check_cert_err(cert_from_file('v1_bc_ee-v1_int-v2_ca_bc.der'), ee_error);
  check_cert_err(cert_from_file('v2_bc_ee-v1_int-v2_ca_bc.der'), ee_error);
  check_cert_err(cert_from_file('v4_bc_ee-v1_int-v2_ca_bc.der'), SEC_ERROR_BAD_DER);

  // v2 ca, v1 intermediate with bc (invalid)
  ca_error = SEC_ERROR_EXTENSION_VALUE_INVALID;
  ee_error = SEC_ERROR_EXTENSION_VALUE_INVALID;
  check_ca_err(cert_from_file('v1_int_bc-v2_ca_bc.der'), ca_error);
  check_cert_err(cert_from_file('v1_ee-v1_int_bc-v2_ca_bc.der'), ee_error);
  check_cert_err(cert_from_file('v1_bc_ee-v1_int_bc-v2_ca_bc.der'), ee_error);
  check_cert_err(cert_from_file('v2_ee-v1_int_bc-v2_ca_bc.der'), ee_error);
  check_cert_err(cert_from_file('v2_bc_ee-v1_int_bc-v2_ca_bc.der'), ee_error);
  check_cert_err(cert_from_file('v3_missing_bc_ee-v1_int_bc-v2_ca_bc.der'), ee_error);
  check_cert_err(cert_from_file('v3_bc_ee-v1_int_bc-v2_ca_bc.der'), ee_error);
  check_cert_err(cert_from_file('v4_bc_ee-v1_int_bc-v2_ca_bc.der'), SEC_ERROR_BAD_DER);

  // v2 ca, v2 intermediate
  ca_error = SEC_ERROR_CA_CERT_INVALID;
  ee_error = SEC_ERROR_CA_CERT_INVALID;
  check_ca_err(cert_from_file('v2_int-v2_ca_bc.der'), ca_error);
  check_cert_err(cert_from_file('v1_ee-v2_int-v2_ca_bc.der'), ee_error);
  check_cert_err(cert_from_file('v2_ee-v2_int-v2_ca_bc.der'), ee_error);
  check_cert_err(cert_from_file('v3_missing_bc_ee-v2_int-v2_ca_bc.der'), ee_error);
  check_cert_err(cert_from_file('v3_bc_ee-v2_int-v2_ca_bc.der'), ee_error);
  ee_error = SEC_ERROR_EXTENSION_VALUE_INVALID;
  check_cert_err(cert_from_file('v1_bc_ee-v2_int-v2_ca_bc.der'), ee_error);
  check_cert_err(cert_from_file('v2_bc_ee-v2_int-v2_ca_bc.der'), ee_error);
  check_cert_err(cert_from_file('v4_bc_ee-v2_int-v2_ca_bc.der'), SEC_ERROR_BAD_DER);

  // v2 ca, v2 intermediate with bc (invalid)
  ca_error = SEC_ERROR_EXTENSION_VALUE_INVALID;
  ee_error = SEC_ERROR_EXTENSION_VALUE_INVALID;
  check_ca_err(cert_from_file('v2_int_bc-v2_ca_bc.der'), ca_error);
  check_cert_err(cert_from_file('v1_ee-v2_int_bc-v2_ca_bc.der'), ee_error);
  check_cert_err(cert_from_file('v1_bc_ee-v2_int_bc-v2_ca_bc.der'), ee_error);
  check_cert_err(cert_from_file('v2_ee-v2_int_bc-v2_ca_bc.der'), ee_error);
  check_cert_err(cert_from_file('v2_bc_ee-v2_int_bc-v2_ca_bc.der'), ee_error);
  check_cert_err(cert_from_file('v3_missing_bc_ee-v2_int_bc-v2_ca_bc.der'), ee_error);
  check_cert_err(cert_from_file('v3_bc_ee-v2_int_bc-v2_ca_bc.der'), ee_error);
  check_cert_err(cert_from_file('v4_bc_ee-v2_int_bc-v2_ca_bc.der'), SEC_ERROR_BAD_DER);

  // v2 ca, invalid v3 intermediate
  ca_error = SEC_ERROR_CA_CERT_INVALID;
  ee_error = SEC_ERROR_CA_CERT_INVALID;
  check_ca_err(cert_from_file('v3_int_missing_bc-v2_ca_bc.der'), ca_error);
  check_cert_err(cert_from_file('v1_ee-v3_int_missing_bc-v2_ca_bc.der'), ee_error);
  check_cert_err(cert_from_file('v2_ee-v3_int_missing_bc-v2_ca_bc.der'), ee_error);
  check_cert_err(cert_from_file('v3_missing_bc_ee-v3_int_missing_bc-v2_ca_bc.der'), ee_error);
  check_cert_err(cert_from_file('v3_bc_ee-v3_int_missing_bc-v2_ca_bc.der'), ee_error);
  ee_error = SEC_ERROR_EXTENSION_VALUE_INVALID;
  check_cert_err(cert_from_file('v1_bc_ee-v3_int_missing_bc-v2_ca_bc.der'), ee_error);
  check_cert_err(cert_from_file('v2_bc_ee-v3_int_missing_bc-v2_ca_bc.der'), ee_error)
  check_cert_err(cert_from_file('v4_bc_ee-v3_int_missing_bc-v2_ca_bc.der'), SEC_ERROR_BAD_DER);

  // v2 ca, valid v3 intermediate (is OK if we use 'classic' semantics)
  ca_error = SEC_ERROR_EXTENSION_VALUE_INVALID;
  ee_error = SEC_ERROR_EXTENSION_VALUE_INVALID;
  check_ca_err(cert_from_file('v3_int-v2_ca_bc.der'), ca_error);
  check_cert_err(cert_from_file('v1_ee-v3_int-v2_ca_bc.der'), ee_error);
  check_cert_err(cert_from_file('v1_bc_ee-v3_int-v2_ca_bc.der'), ee_error);
  check_cert_err(cert_from_file('v2_ee-v3_int-v2_ca_bc.der'), ee_error);
  check_cert_err(cert_from_file('v2_bc_ee-v3_int-v2_ca_bc.der'), ee_error);
  check_cert_err(cert_from_file('v3_missing_bc_ee-v3_int-v2_ca_bc.der'), ee_error);
  check_cert_err(cert_from_file('v3_bc_ee-v3_int-v2_ca_bc.der'), ee_error);
  check_cert_err(cert_from_file('v4_bc_ee-v3_int-v2_ca_bc.der'), SEC_ERROR_BAD_DER);

  //////////////
  // v3 CA supersection
  //////////////////

  // v3 ca, v1 intermediate
  ca_error = SEC_ERROR_CA_CERT_INVALID;
  ee_error = SEC_ERROR_CA_CERT_INVALID;
  check_ca_err(cert_from_file('v1_int-v3_ca.der'), ca_error);
  check_cert_err(cert_from_file('v1_ee-v1_int-v3_ca.der'), ee_error);
  check_cert_err(cert_from_file('v2_ee-v1_int-v3_ca.der'), ee_error);
  check_cert_err(cert_from_file('v3_missing_bc_ee-v1_int-v3_ca.der'), ee_error);
  check_cert_err(cert_from_file('v3_bc_ee-v1_int-v3_ca.der'), ee_error);
  ee_error = SEC_ERROR_EXTENSION_VALUE_INVALID;
  check_cert_err(cert_from_file('v1_bc_ee-v1_int-v3_ca.der'), ee_error);
  check_cert_err(cert_from_file('v2_bc_ee-v1_int-v3_ca.der'), ee_error);
  check_cert_err(cert_from_file('v4_bc_ee-v1_int-v3_ca.der'), SEC_ERROR_BAD_DER);

  // A v1 intermediate with v3 extensions
  ca_error = SEC_ERROR_EXTENSION_VALUE_INVALID;
  ee_error = SEC_ERROR_EXTENSION_VALUE_INVALID;
  check_ca_err(cert_from_file('v1_int_bc-v3_ca.der'), ca_error);
  check_cert_err(cert_from_file('v1_ee-v1_int_bc-v3_ca.der'), ee_error);
  check_cert_err(cert_from_file('v1_bc_ee-v1_int_bc-v3_ca.der'), ee_error);
  check_cert_err(cert_from_file('v2_ee-v1_int_bc-v3_ca.der'), ee_error);
  check_cert_err(cert_from_file('v2_bc_ee-v1_int_bc-v3_ca.der'), ee_error);
  check_cert_err(cert_from_file('v3_missing_bc_ee-v1_int_bc-v3_ca.der'), ee_error);
  check_cert_err(cert_from_file('v3_bc_ee-v1_int_bc-v3_ca.der'), ee_error);
  check_cert_err(cert_from_file('v4_bc_ee-v1_int_bc-v3_ca.der'), SEC_ERROR_BAD_DER);

  // reject a v2 cert as intermediate
  ca_error = SEC_ERROR_CA_CERT_INVALID;
  ee_error = SEC_ERROR_CA_CERT_INVALID;
  check_ca_err(cert_from_file('v2_int-v3_ca.der'), ca_error);
  check_cert_err(cert_from_file('v1_ee-v2_int-v3_ca.der'), ee_error);
  check_cert_err(cert_from_file('v2_ee-v2_int-v3_ca.der'), ee_error);
  check_cert_err(cert_from_file('v3_missing_bc_ee-v2_int-v3_ca.der'), ee_error);
  check_cert_err(cert_from_file('v3_bc_ee-v2_int-v3_ca.der'), ee_error);
  ee_error = SEC_ERROR_EXTENSION_VALUE_INVALID;
  check_cert_err(cert_from_file('v1_bc_ee-v2_int-v3_ca.der'), ee_error);
  check_cert_err(cert_from_file('v2_bc_ee-v2_int-v3_ca.der'), ee_error);
  check_cert_err(cert_from_file('v4_bc_ee-v2_int-v3_ca.der'), SEC_ERROR_BAD_DER);

  // v2 intermediate with bc (invalid)
  ca_error = SEC_ERROR_EXTENSION_VALUE_INVALID;
  ee_error = SEC_ERROR_EXTENSION_VALUE_INVALID;
  check_ca_err(cert_from_file('v2_int_bc-v3_ca.der'), ca_error);
  check_cert_err(cert_from_file('v1_ee-v2_int_bc-v3_ca.der'), ee_error);
  check_cert_err(cert_from_file('v1_bc_ee-v2_int_bc-v3_ca.der'), ee_error);
  check_cert_err(cert_from_file('v2_ee-v2_int_bc-v3_ca.der'), ee_error);
  check_cert_err(cert_from_file('v2_bc_ee-v2_int_bc-v3_ca.der'), ee_error);
  check_cert_err(cert_from_file('v3_missing_bc_ee-v2_int_bc-v3_ca.der'), ee_error);
  check_cert_err(cert_from_file('v3_bc_ee-v2_int_bc-v3_ca.der'), ee_error);
  check_cert_err(cert_from_file('v4_bc_ee-v2_int_bc-v3_ca.der'), SEC_ERROR_BAD_DER);

  // invalid v3 intermediate
  ca_error = SEC_ERROR_CA_CERT_INVALID;
  ee_error = SEC_ERROR_CA_CERT_INVALID;
  check_ca_err(cert_from_file('v3_int_missing_bc-v3_ca.der'), ca_error);
  check_cert_err(cert_from_file('v1_ee-v3_int_missing_bc-v3_ca.der'), ee_error);
  check_cert_err(cert_from_file('v2_ee-v3_int_missing_bc-v3_ca.der'), ee_error);
  check_cert_err(cert_from_file('v3_missing_bc_ee-v3_int_missing_bc-v3_ca.der'), ee_error);
  check_cert_err(cert_from_file('v3_bc_ee-v3_int_missing_bc-v3_ca.der'), ee_error);
  ee_error = SEC_ERROR_EXTENSION_VALUE_INVALID;
  check_cert_err(cert_from_file('v1_bc_ee-v3_int_missing_bc-v3_ca.der'), ee_error);
  check_cert_err(cert_from_file('v2_bc_ee-v3_int_missing_bc-v3_ca.der'), ee_error);
  check_cert_err(cert_from_file('v4_bc_ee-v3_int_missing_bc-v3_ca.der'), SEC_ERROR_BAD_DER);

  // I dont think that v3 intermediates should be allowed to sign v1 or v2
  // certs, but other thanthat this  is what we usually get in the wild.
  check_ok_ca(cert_from_file('v3_int-v3_ca.der'));
  check_ok(cert_from_file('v1_ee-v3_int-v3_ca.der'));
  check_ok(cert_from_file('v2_ee-v3_int-v3_ca.der'));
  check_ok(cert_from_file('v3_missing_bc_ee-v3_int-v3_ca.der'));
  check_ok(cert_from_file('v3_bc_ee-v3_int-v3_ca.der'));
  ca_error = SEC_ERROR_EXTENSION_VALUE_INVALID;
  ee_error = SEC_ERROR_EXTENSION_VALUE_INVALID;
  check_cert_err(cert_from_file('v1_bc_ee-v3_int-v3_ca.der'), ee_error);
  check_cert_err(cert_from_file('v2_bc_ee-v3_int-v3_ca.der'), ee_error);
  check_cert_err(cert_from_file('v4_bc_ee-v3_int-v3_ca.der'), SEC_ERROR_BAD_DER);

  // v3 CA, invalid v3 intermediate
  ca_error = SEC_ERROR_CA_CERT_INVALID;
  ee_error = SEC_ERROR_CA_CERT_INVALID;
  check_ca_err(cert_from_file('v1_int-v3_ca_missing_bc.der'), ca_error);
  check_cert_err(cert_from_file('v1_ee-v1_int-v3_ca_missing_bc.der'), ee_error);
  check_cert_err(cert_from_file('v2_ee-v1_int-v3_ca_missing_bc.der'), ee_error);
  check_cert_err(cert_from_file('v3_missing_bc_ee-v1_int-v3_ca_missing_bc.der'), ee_error);
  check_cert_err(cert_from_file('v3_bc_ee-v1_int-v3_ca_missing_bc.der'), ee_error);
  ee_error = SEC_ERROR_EXTENSION_VALUE_INVALID;
  check_cert_err(cert_from_file('v1_bc_ee-v1_int-v3_ca_missing_bc.der'), ee_error);
  check_cert_err(cert_from_file('v2_bc_ee-v1_int-v3_ca_missing_bc.der'), ee_error);
  check_cert_err(cert_from_file('v4_bc_ee-v1_int-v3_ca_missing_bc.der'), SEC_ERROR_BAD_DER);

  // Int v1 with BC that is just invalid (classic fail insanity OK)
  ca_error = SEC_ERROR_EXTENSION_VALUE_INVALID;
  ee_error = SEC_ERROR_EXTENSION_VALUE_INVALID;
  check_ca_err(cert_from_file('v1_int_bc-v3_ca_missing_bc.der'), ca_error);
  check_cert_err(cert_from_file('v1_ee-v1_int_bc-v3_ca_missing_bc.der'), ee_error);
  check_cert_err(cert_from_file('v1_bc_ee-v1_int_bc-v3_ca_missing_bc.der'), ee_error);
  check_cert_err(cert_from_file('v2_ee-v1_int_bc-v3_ca_missing_bc.der'), ee_error);
  check_cert_err(cert_from_file('v2_bc_ee-v1_int_bc-v3_ca_missing_bc.der'), ee_error);
  check_cert_err(cert_from_file('v3_missing_bc_ee-v1_int_bc-v3_ca_missing_bc.der'), ee_error);
  check_cert_err(cert_from_file('v3_bc_ee-v1_int_bc-v3_ca_missing_bc.der'), ee_error);
  check_cert_err(cert_from_file('v4_bc_ee-v1_int_bc-v3_ca_missing_bc.der'), SEC_ERROR_BAD_DER);

  // Good section (all fail)
  ca_error = SEC_ERROR_CA_CERT_INVALID;
  ee_error = SEC_ERROR_CA_CERT_INVALID;
  check_ca_err(cert_from_file('v2_int-v3_ca_missing_bc.der'), ca_error);
  check_cert_err(cert_from_file('v1_ee-v2_int-v3_ca_missing_bc.der'), ee_error);
  check_cert_err(cert_from_file('v2_ee-v2_int-v3_ca_missing_bc.der'), ee_error);
  check_cert_err(cert_from_file('v3_missing_bc_ee-v2_int-v3_ca_missing_bc.der'), ee_error);
  check_cert_err(cert_from_file('v3_bc_ee-v2_int-v3_ca_missing_bc.der'), ee_error);
  ee_error = SEC_ERROR_EXTENSION_VALUE_INVALID;
  check_cert_err(cert_from_file('v1_bc_ee-v2_int-v3_ca_missing_bc.der'), ee_error);
  check_cert_err(cert_from_file('v2_bc_ee-v2_int-v3_ca_missing_bc.der'), ee_error);
  check_cert_err(cert_from_file('v4_bc_ee-v2_int-v3_ca_missing_bc.der'), SEC_ERROR_BAD_DER);

  // v2 intermediate (even with basic constraints) is invalid
  ca_error = SEC_ERROR_EXTENSION_VALUE_INVALID;
  ee_error = SEC_ERROR_EXTENSION_VALUE_INVALID;
  check_ca_err(cert_from_file('v2_int_bc-v3_ca_missing_bc.der'), ca_error);
  check_cert_err(cert_from_file('v1_ee-v2_int_bc-v3_ca_missing_bc.der'), ee_error);
  check_cert_err(cert_from_file('v1_bc_ee-v2_int_bc-v3_ca_missing_bc.der'), ee_error);
  check_cert_err(cert_from_file('v2_ee-v2_int_bc-v3_ca_missing_bc.der'), ee_error);
  check_cert_err(cert_from_file('v2_bc_ee-v2_int_bc-v3_ca_missing_bc.der'), ee_error);
  check_cert_err(cert_from_file('v3_missing_bc_ee-v2_int_bc-v3_ca_missing_bc.der'), ee_error);
  check_cert_err(cert_from_file('v3_bc_ee-v2_int_bc-v3_ca_missing_bc.der'), ee_error);
  check_cert_err(cert_from_file('v4_bc_ee-v2_int_bc-v3_ca_missing_bc.der'), SEC_ERROR_BAD_DER);

  // v3 intermediate missing basic constraints is invalid
  ca_error = SEC_ERROR_CA_CERT_INVALID;
  ee_error = SEC_ERROR_CA_CERT_INVALID;
  check_ca_err(cert_from_file('v3_int_missing_bc-v3_ca_missing_bc.der'), ca_error);
  check_cert_err(cert_from_file('v1_ee-v3_int_missing_bc-v3_ca_missing_bc.der'), ee_error);
  check_cert_err(cert_from_file('v2_ee-v3_int_missing_bc-v3_ca_missing_bc.der'), ee_error);
  check_cert_err(cert_from_file('v3_missing_bc_ee-v3_int_missing_bc-v3_ca_missing_bc.der'), ee_error);
  check_cert_err(cert_from_file('v3_bc_ee-v3_int_missing_bc-v3_ca_missing_bc.der'), ee_error);
  ee_error = SEC_ERROR_EXTENSION_VALUE_INVALID;
  check_cert_err(cert_from_file('v1_bc_ee-v3_int_missing_bc-v3_ca_missing_bc.der'), ee_error);
  check_cert_err(cert_from_file('v2_bc_ee-v3_int_missing_bc-v3_ca_missing_bc.der'), ee_error);
  check_cert_err(cert_from_file('v4_bc_ee-v3_int_missing_bc-v3_ca_missing_bc.der'), SEC_ERROR_BAD_DER);

  // With a v3 root missing bc and valid v3 intermediate
  ca_error = SEC_ERROR_CA_CERT_INVALID;
  ee_error = SEC_ERROR_CA_CERT_INVALID;
  check_ca_err(cert_from_file('v3_int-v3_ca_missing_bc.der'), ca_error);
  check_cert_err(cert_from_file('v1_ee-v3_int-v3_ca_missing_bc.der'), ee_error);
  check_cert_err(cert_from_file('v2_ee-v3_int-v3_ca_missing_bc.der'), ee_error);
  check_cert_err(cert_from_file('v3_missing_bc_ee-v3_int-v3_ca_missing_bc.der'), ee_error);
  check_cert_err(cert_from_file('v3_bc_ee-v3_int-v3_ca_missing_bc.der'), ee_error);
  ca_error = SEC_ERROR_EXTENSION_VALUE_INVALID;
  ee_error = SEC_ERROR_EXTENSION_VALUE_INVALID;
  check_cert_err(cert_from_file('v1_bc_ee-v3_int-v3_ca_missing_bc.der'), ee_error);
  check_cert_err(cert_from_file('v2_bc_ee-v3_int-v3_ca_missing_bc.der'), ee_error);
  check_cert_err(cert_from_file('v4_bc_ee-v3_int-v3_ca_missing_bc.der'), SEC_ERROR_BAD_DER);
}
