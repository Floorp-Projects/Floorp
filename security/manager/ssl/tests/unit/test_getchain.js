// -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

"use strict";

do_get_profile(); // must be called before getting nsIX509CertDB
const certdb  = Cc["@mozilla.org/security/x509certdb;1"]
                  .getService(Ci.nsIX509CertDB);
const certdb2 = Cc["@mozilla.org/security/x509certdb;1"]
                  .getService(Ci.nsIX509CertDB2);

// This is the list of certificates needed for the test
// The certificates prefixed by 'int-' are intermediates
let certList = [
  'ee',
  'ca-1',
  'ca-2',
]

function load_cert(cert_name, trust_string) {
  var cert_filename = cert_name + ".der";
  addCertFromFile(certdb, "test_getchain/" + cert_filename, trust_string);
}

// Since all the ca's are identical expect for the serial number
// I have to grab them by enumerating all the certs and then finding
// the ones that I am interested in.
function get_ca_array() {
  let ret_array = new Array();
  let allCerts = certdb2.getCerts();
  let enumerator = allCerts.getEnumerator();
  while (enumerator.hasMoreElements()) {
    let cert = enumerator.getNext().QueryInterface(Ci.nsIX509Cert);
    if (cert.commonName == 'ca') {
      ret_array[parseInt(cert.serialNumber)] = cert;
    }
  }
  return ret_array;
}


function check_matching_issuer_and_getchain(expected_issuer_serial, cert) {
  const nsIX509Cert = Components.interfaces.nsIX509Cert;

  do_check_eq(expected_issuer_serial, cert.issuer.serialNumber);
  let chain = cert.getChain();
  let issuer_via_getchain = chain.queryElementAt(1, nsIX509Cert);
  // The issuer returned by cert.issuer or cert.getchain should be consistent.
  do_check_eq(cert.issuer.serialNumber, issuer_via_getchain.serialNumber);
}

function check_getchain(ee_cert, ssl_ca, email_ca){
  // A certificate should first build a chain/issuer to
  // a SSL trust domain, then an EMAIL trust domain and then
  // and object signer trust domain

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
  // be cosistent (the actual value is non-deterministic).
  check_matching_issuer_and_getchain(ee_cert.issuer.serialNumber, ee_cert);
}

function run_test() {
  for (let i = 0 ; i < certList.length; i++) {
    load_cert(certList[i], ',,');
  }

  let ee_cert = certdb.findCertByNickname(null, 'ee');
  do_check_false(!ee_cert);

  let ca = get_ca_array();

  check_getchain(ee_cert, ca[1], ca[2]);
  // Swap ca certs to deal alternate trust settings.
  check_getchain(ee_cert, ca[2], ca[1]);

  run_next_test();
}

