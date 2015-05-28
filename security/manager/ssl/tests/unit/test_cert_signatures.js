// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

"use strict";
/*
 * The purpose of this test is to verify that we correctly detect bad
 * signatures on tampered certificates. Eventually, we should also be
 * verifying that the error we return is the correct error.
 *
 * To regenerate the certificates for this test:
 *
 *      cd security/manager/ssl/tests/unit/test_cert_signatures
 *       ./generate.py
 *      cd ../../../../../..
 *      make -C $OBJDIR/security/manager/ssl/tests
 *
 * Check in the generated files. These steps are not done as part of the build
 * because we do not want to add a build-time dependency on the OpenSSL or NSS
 * tools or libraries built for the host platform.
 */

do_get_profile(); // must be called before getting nsIX509CertDB
const certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(Ci.nsIX509CertDB);

function load_ca(ca_name) {
  let ca_filename = ca_name + ".der";
  addCertFromFile(certdb, "test_cert_signatures/" + ca_filename, 'CTu,CTu,CTu');
}

function check_ca(ca_name) {
  do_print("ca_name=" + ca_name);
  let cert = certdb.findCertByNickname(null, ca_name);

  let verified = {};
  let usages = {};
  cert.getUsagesString(true, verified, usages);
  equal("SSL CA", usages.value, "Usages string for a CA cert should be 'SSL CA'");
}

function run_test() {
  // Load the ca into mem
  load_ca("ca-rsa");
  load_ca("ca-p384");

  clearOCSPCache();
  clearSessionCache();

  check_ca("ca-rsa");
  check_ca("ca-p384");

  // mozilla::pkix does not allow CA certs to be validated for end-entity
  // usages.
  const int_usage = 'SSL CA';

  // mozilla::pkix doesn't implement the Netscape Object Signer restriction.
  const ee_usage = 'Client,Server,Sign,Encrypt,Object Signer';

  let cert2usage = {
    // certs without the "int" prefix are end entity certs.
    'int-rsa-valid': int_usage,
    'rsa-valid': ee_usage,
    'int-p384-valid': int_usage,
    'p384-valid': ee_usage,

    'rsa-valid-int-tampered-ee': "",
    'p384-valid-int-tampered-ee': "",

    'int-rsa-tampered': "",
    'rsa-tampered-int-valid-ee': "",
    'int-p384-tampered': "",
    'p384-tampered-int-valid-ee': "",

  };

  // Load certs first
  for (let cert_name in cert2usage) {
    let cert_filename = cert_name + ".der";
    addCertFromFile(certdb, "test_cert_signatures/" + cert_filename, ',,');
  }

  for (let cert_name in cert2usage) {
    do_print("cert_name=" + cert_name);

    let cert = certdb.findCertByNickname(null, cert_name);

    let verified = {};
    let usages = {};
    cert.getUsagesString(true, verified, usages);
    equal(cert2usage[cert_name], usages.value,
          "Expected and actual usages string should match");
  }
}
