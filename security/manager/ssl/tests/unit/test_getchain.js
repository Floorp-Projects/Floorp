// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

"use strict";

do_get_profile(); // must be called before getting nsIX509CertDB
const certdb  = Cc["@mozilla.org/security/x509certdb;1"]
                  .getService(Ci.nsIX509CertDB);
// This is the list of certificates needed for the test.
var certList = [
  'ee',
  'ca-1',
  'ca-2',
];

// Since all the ca's are identical expect for the serial number
// I have to grab them by enumerating all the certs and then finding
// the ones that I am interested in.
function get_ca_array() {
  let ret_array = [];
  let allCerts = certdb.getCerts();
  let enumerator = allCerts.getEnumerator();
  while (enumerator.hasMoreElements()) {
    let cert = enumerator.getNext().QueryInterface(Ci.nsIX509Cert);
    if (cert.commonName == 'ca') {
      ret_array[parseInt(cert.serialNumber, 16)] = cert;
    }
  }
  return ret_array;
}


function check_matching_issuer_and_getchain(expected_issuer_serial, cert) {
  const nsIX509Cert = Components.interfaces.nsIX509Cert;

  equal(expected_issuer_serial, cert.issuer.serialNumber,
        "Expected and actual issuer serial numbers should match");
  let chain = cert.getChain();
  let issuer_via_getchain = chain.queryElementAt(1, nsIX509Cert);
  // The issuer returned by cert.issuer or cert.getchain should be consistent.
  equal(cert.issuer.serialNumber, issuer_via_getchain.serialNumber,
        "Serial numbers via cert.issuer and via getChain() should match");
}

function check_getchain(ee_cert, ssl_ca, email_ca) {
  // A certificate should first build a chain/issuer to
  // a SSL trust domain, then an EMAIL trust domain and then
  // an object signer trust domain.

  const nsIX509Cert = Components.interfaces.nsIX509Cert;
  certdb.setCertTrust(ssl_ca, nsIX509Cert.CA_CERT,
                      Ci.nsIX509CertDB.TRUSTED_SSL);
  certdb.setCertTrust(email_ca, nsIX509Cert.CA_CERT,
                      Ci.nsIX509CertDB.TRUSTED_EMAIL);
  check_matching_issuer_and_getchain(ssl_ca.serialNumber, ee_cert);
  certdb.setCertTrust(ssl_ca, nsIX509Cert.CA_CERT, 0);
  check_matching_issuer_and_getchain(email_ca.serialNumber, ee_cert);
  certdb.setCertTrust(email_ca, nsIX509Cert.CA_CERT, 0);
  // Do a final test on the case of no trust. The results must
  // be consistent (the actual value is non-deterministic).
  check_matching_issuer_and_getchain(ee_cert.issuer.serialNumber, ee_cert);
}

function run_test() {
  clearOCSPCache();
  clearSessionCache();

  let ee_cert = null;
  for (let cert of certList) {
    let result = addCertFromFile(certdb, `test_getchain/${cert}.pem`, ",,");
    if (cert == "ee") {
      ee_cert = result;
    }
  }

  notEqual(ee_cert, null, "EE cert should have successfully loaded");

  let ca = get_ca_array();

  check_getchain(ee_cert, ca[1], ca[2]);
  // Swap ca certs to deal alternate trust settings.
  check_getchain(ee_cert, ca[2], ca[1]);
}
