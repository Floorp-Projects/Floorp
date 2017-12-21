// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

"use strict";

// Tests enterprise root certificate support. When configured to do so, the
// platform will attempt to find and import enterprise root certificates. This
// feature is specific to Windows.

do_get_profile(); // must be called before getting nsIX509CertDB

function check_no_enterprise_roots_imported(certDB, dbKey = undefined) {
  let enterpriseRoots = certDB.getEnterpriseRoots();
  equal(enterpriseRoots, null, "should not have imported any enterprise roots");
  if (dbKey) {
    let cert = certDB.findCertByDBKey(dbKey);
    // If the garbage-collector hasn't run, there may be reachable copies of
    // imported enterprise root certificates. If so, they shouldn't be trusted
    // to issue TLS server auth certificates.
    if (cert) {
      ok(!certDB.isCertTrusted(cert, Ci.nsIX509Cert.CA_CERT,
                               Ci.nsIX509CertDB.TRUSTED_SSL),
         "previously-imported enterprise root shouldn't be trusted to issue " +
         "TLS server auth certificates");
    }
  }
}

function check_some_enterprise_roots_imported(certDB) {
  let enterpriseRoots = certDB.getEnterpriseRoots();
  notEqual(enterpriseRoots, null, "should have imported some enterprise roots");
  let enumerator = enterpriseRoots.getEnumerator();
  let foundNonBuiltIn = false;
  let savedDBKey = null;
  while (enumerator.hasMoreElements()) {
    let cert = enumerator.getNext().QueryInterface(Ci.nsIX509Cert);
    if (!cert.isBuiltInRoot && !savedDBKey) {
      foundNonBuiltIn = true;
      savedDBKey = cert.dbKey;
      info("saving dbKey from " + cert.commonName);
    }
  }
  ok(foundNonBuiltIn, "should have found non-built-in root");
  return savedDBKey;
}

function run_test() {
  let certDB = Cc["@mozilla.org/security/x509certdb;1"]
                 .getService(Ci.nsIX509CertDB);
  Services.prefs.setBoolPref("security.enterprise_roots.enabled", false);
  check_no_enterprise_roots_imported(certDB);
  Services.prefs.setBoolPref("security.enterprise_roots.enabled", true);
  let savedDBKey = check_some_enterprise_roots_imported(certDB);
  Services.prefs.setBoolPref("security.enterprise_roots.enabled", false);
  check_no_enterprise_roots_imported(certDB, savedDBKey);
}
