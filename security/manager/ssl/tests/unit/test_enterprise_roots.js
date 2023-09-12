// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

"use strict";

// Tests enterprise root certificate support. When configured to do so, the
// platform will attempt to find and import enterprise root certificates. This
// feature is specific to Windows.

do_get_profile(); // must be called before getting nsIX509CertDB

const { TestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TestUtils.sys.mjs"
);

async function check_no_enterprise_roots_imported(
  nssComponent,
  certDB,
  dbKey = undefined
) {
  let enterpriseRoots = nssComponent.getEnterpriseRoots();
  notEqual(enterpriseRoots, null, "enterprise roots list should not be null");
  equal(
    enterpriseRoots.length,
    0,
    "should not have imported any enterprise roots"
  );
  if (dbKey) {
    let cert = certDB.findCertByDBKey(dbKey);
    // If the garbage-collector hasn't run, there may be reachable copies of
    // imported enterprise root certificates. If so, they shouldn't be trusted
    // to issue TLS server auth certificates.
    if (cert) {
      await asyncTestCertificateUsages(certDB, cert, []);
    }
  }
}

async function check_some_enterprise_roots_imported(nssComponent, certDB) {
  let enterpriseRoots = nssComponent.getEnterpriseRoots();
  notEqual(enterpriseRoots, null, "enterprise roots list should not be null");
  notEqual(
    enterpriseRoots.length,
    0,
    "should have imported some enterprise roots"
  );
  let foundNonBuiltIn = false;
  let savedDBKey = null;
  for (let certDer of enterpriseRoots) {
    let cert = certDB.constructX509(certDer);
    notEqual(cert, null, "should be able to decode cert from DER");
    if (!cert.isBuiltInRoot && !savedDBKey) {
      foundNonBuiltIn = true;
      savedDBKey = cert.dbKey;
      info("saving dbKey from " + cert.commonName);
      await asyncTestCertificateUsages(certDB, cert, [certificateUsageSSLCA]);
      break;
    }
  }
  ok(foundNonBuiltIn, "should have found non-built-in root");
  return savedDBKey;
}

add_task(async function run_test() {
  let nssComponent = Cc["@mozilla.org/psm;1"].getService(Ci.nsINSSComponent);
  let certDB = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
  nssComponent.getEnterpriseRoots(); // blocks until roots are loaded
  await check_some_enterprise_roots_imported(nssComponent, certDB);
  Services.prefs.setBoolPref("security.enterprise_roots.enabled", false);
  await check_no_enterprise_roots_imported(nssComponent, certDB);
  Services.prefs.setBoolPref("security.enterprise_roots.enabled", true);
  await TestUtils.topicObserved("psm:enterprise-certs-imported");
  let savedDBKey = await check_some_enterprise_roots_imported(
    nssComponent,
    certDB
  );
  Services.prefs.setBoolPref("security.enterprise_roots.enabled", false);
  await check_no_enterprise_roots_imported(nssComponent, certDB, savedDBKey);
});
