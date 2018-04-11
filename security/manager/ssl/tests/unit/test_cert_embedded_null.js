// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Tests that a certificate with a clever subject common name like
// 'www.bank1.com[NUL]www.bad-guy.com' (where [NUL] is a single byte with
// value 0) will not be treated as valid for www.bank1.com.
// Includes a similar test case but for the subject alternative name extension.

"use strict";

do_get_profile(); // must be called before getting nsIX509CertDB
const certdb = Cc["@mozilla.org/security/x509certdb;1"]
                 .getService(Ci.nsIX509CertDB);

async function do_testcase(certname, checkCommonName) {
  let cert = constructCertFromFile(`test_cert_embedded_null/${certname}.pem`);
  // Where applicable, check that the testcase is meaningful (i.e. that the
  // certificate's subject common name has an embedded NUL in it).
  if (checkCommonName) {
    equal(cert.commonName, "www.bank1.com\\00www.bad-guy.com",
          "certificate subject common name should have an embedded NUL byte");
  }
  await checkCertErrorGeneric(certdb, cert, SSL_ERROR_BAD_CERT_DOMAIN,
                              certificateUsageSSLServer, undefined,
                              "www.bank1.com");
  await checkCertErrorGeneric(certdb, cert, SSL_ERROR_BAD_CERT_DOMAIN,
                              certificateUsageSSLServer, undefined,
                              "www.bad-guy.com");
}

add_task(async function() {
  addCertFromFile(certdb, "test_cert_embedded_null/ca.pem", "CTu,,");

  await do_testcase("embeddedNull", true);
  await do_testcase("embeddedNullSAN", false);
  await do_testcase("embeddedNullCNAndSAN", true);
  await do_testcase("embeddedNullSAN2", false);
});
