// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

"use strict";

// This test tests two specific items:
// 1. Are name constraints properly enforced across the entire constructed
// certificate chain? This makes use of a certificate hierarchy like so:
//  - (trusted) root CA with permitted subtree dNSName example.com
//  - intermediate CA with permitted subtree dNSName example.org
//    a. end-entity with dNSNames example.com and example.org
//       (the first entry is allowed by the root but not by the intermediate,
//        and the second entry is allowed by the intermediate but not by the
//        root)
//    b. end-entity with dNSName example.com (not allowed by the intermediate)
//    c. end-entity with dNSName examle.org (not allowed by the root)
//    d. end-entity with dNSName example.test (not allowed by either)
//  All of these cases should fail to verify with the error that the
//  end-entity is not in the name space permitted by the hierarchy.
//
// 2. Are externally-imposed name constraints properly enforced? This makes use
// of a certificate hierarchy rooted by a certificate with the same DN as an
// existing hierarchy that has externally-imposed name constraints (DCISS).

do_get_profile(); // must be called before getting nsIX509CertDB
const certdb = Cc["@mozilla.org/security/x509certdb;1"]
                 .getService(Ci.nsIX509CertDB);

function certFromFile(name) {
  return constructCertFromFile(`test_name_constraints/${name}.pem`);
}

function loadCertWithTrust(certName, trustString) {
  addCertFromFile(certdb, `test_name_constraints/${certName}.pem`,
                  trustString);
}

function checkCertNotInNameSpace(cert) {
  checkCertErrorGeneric(certdb, cert, SEC_ERROR_CERT_NOT_IN_NAME_SPACE,
                        certificateUsageSSLServer);
}

function checkCertInNameSpace(cert) {
  checkCertErrorGeneric(certdb, cert, PRErrorCodeSuccess,
                        certificateUsageSSLServer);
}

function run_test() {
  // Test that name constraints from the entire certificate chain are enforced.
  loadCertWithTrust("ca-example-com-permitted", "CTu,,");
  loadCertWithTrust("int-example-org-permitted", ",,");
  checkCertNotInNameSpace(certFromFile("ee-example-com-and-org"));
  checkCertNotInNameSpace(certFromFile("ee-example-com"));
  checkCertNotInNameSpace(certFromFile("ee-example-org"));
  checkCertNotInNameSpace(certFromFile("ee-example-test"));

  // Test that externally-imposed name constraints are enforced (DCISS tests).
  loadCertWithTrust("dciss", "CTu,,");
  checkCertInNameSpace(certFromFile("NameConstraints.dcissallowed"));
  checkCertNotInNameSpace(certFromFile("NameConstraints.dcissblocked"));
}
