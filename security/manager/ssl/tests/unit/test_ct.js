// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

"use strict";

do_get_profile(); // must be called before getting nsIX509CertDB
const certdb  = Cc["@mozilla.org/security/x509certdb;1"]
                  .getService(Ci.nsIX509CertDB);

function expectCT(value) {
  return (securityInfo) => {
    let sslStatus = securityInfo.QueryInterface(Ci.nsISSLStatusProvider)
                                .SSLStatus;
    Assert.equal(sslStatus.certificateTransparencyStatus, value,
                 "actual and expected CT status should match");
  };
}

registerCleanupFunction(() => {
  Services.prefs.clearUserPref("security.pki.certificate_transparency.mode");
});

function run_test() {
  Services.prefs.setIntPref("security.pki.certificate_transparency.mode", 1);
  add_tls_server_setup("BadCertServer", "test_ct");
  // These certificates have a validity period of 800 days, which is a little
  // over 2 years and 2 months. This gets rounded down to 2 years (since it's
  // less than 2 years and 3 months). Our policy requires N + 1 embedded SCTs,
  // where N is 2 in this case. So, a policy-compliant certificate would have at
  // least 3 SCTs.
  add_connection_test("ct-valid.example.com", PRErrorCodeSuccess, null,
    expectCT(Ci.nsISSLStatus.CERTIFICATE_TRANSPARENCY_POLICY_COMPLIANT));
  // This certificate has only 2 embedded SCTs, and so is not policy-compliant.
  add_connection_test("ct-insufficient-scts.example.com", PRErrorCodeSuccess,
    null,
    expectCT(Ci.nsISSLStatus.CERTIFICATE_TRANSPARENCY_POLICY_NOT_ENOUGH_SCTS));
  run_next_test();
}
