// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
"use strict";

// This test uses a specially-crafted NSS cert DB containing 12 self-signed certificates that all
// have the same subject and issuer distinguished name. Since they all have different keys and none
// of them are trust anchors, there are a large number of potential trust paths that could be
// explored. If our trust domain were naive enough to allow mozilla::pkix to explore them all, it
// would take a long time to perform (mozilla::pkix does have the concept of a path-building budget,
// but even on a fast computer, it takes an unacceptable amount of time to exhaust). To prevent the
// full exploration of this space, NSSCertDBTrustDomain skips searching through self-signed
// certificates that aren't trust anchors, since those would never otherwise be essential to
// complete a path (note that this is only true as long as the extensions we support are restrictive
// rather than additive).
// When we try to verify one of these certificates in this test, we should finish relatively
// quickly, even on slow hardware.
// Should these certificates ever need regenerating, they were produced with the following commands:
// certutil -N -d . --empty-password
// for num in 00 01 02 03 04 05 06 07 08 09 10 11; do
//   echo -ne "5\n6\n9\ny\ny\n\ny\n" | certutil -d . -S -s "CN=self-signed cert" -t ,, \
//   -q secp256r1 -x -k ec -z <(date +%s) -1 -2 -n cert$num; sleep 2;
// done

add_task(async function test_no_overlong_path_building() {
  let profile = do_get_profile();
  const CERT_DB_NAME = "cert9.db";
  let srcCertDBFile = do_get_file(`test_self_signed_certs/${CERT_DB_NAME}`);
  srcCertDBFile.copyTo(profile, CERT_DB_NAME);

  let certDB = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
  let certToVerify = null;
  for (let cert of certDB.getCerts()) {
    if (cert.subjectName == "CN=self-signed cert") {
      certToVerify = cert;
      break;
    }
  }
  notEqual(
    certToVerify,
    null,
    "should have found one of the preloaded self-signed certs"
  );
  let timeBefore = Date.now();
  // As mentioned above, mozilla::pkix limits how much it will search for a trusted path, even if a
  // trust domain keeps providing potential issuers. So, if we only tried to verify a certificate
  // once, this test could potentially pass on a fast computer even if we weren't properly skipping
  // unnecessary paths. If we were to try and lower our time limit (the comparison with
  // secondsElapsed, below), this test would intermittently fail on slow hardware. By trying to
  // verify the certificate 10 times, we hopefully end up with a meaningful test (it should still
  // fail on fast hardware if we don't properly skip unproductive paths) that won't intermittently
  // time out on slow hardware.
  for (let i = 0; i < 10; i++) {
    let date = new Date("2019-05-15T00:00:00.000Z");
    await checkCertErrorGenericAtTime(
      certDB,
      certToVerify,
      SEC_ERROR_UNKNOWN_ISSUER,
      certificateUsageSSLCA,
      date.getTime() / 1000
    );
  }
  let timeAfter = Date.now();
  let secondsElapsed = (timeAfter - timeBefore) / 1000;
  ok(secondsElapsed < 120, "verifications shouldn't take too long");
});

add_task(async function test_no_bad_signature() {
  // If there are two self-signed CA certificates with the same subject and
  // issuer but different keys, where one is trusted, test that using the other
  // one as a server certificate doesn't result in a non-overridable "bad
  // signature" error but rather a "self-signed cert" error.
  let selfSignedCert = constructCertFromFile("test_self_signed_certs/ca1.pem");
  let certDB = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
  addCertFromFile(certDB, "test_self_signed_certs/ca2.pem", "CTu,,");
  await checkCertErrorGeneric(
    certDB,
    selfSignedCert,
    MOZILLA_PKIX_ERROR_SELF_SIGNED_CERT,
    certificateUsageSSLServer,
    false,
    "example.com"
  );
});

add_task(async function test_no_inadequate_key_usage() {
  // If there are two different non-CA, self-signed certificates with the same
  // subject and issuer but different keys, test that using one of them as a
  // server certificate doesn't result in a non-overridable "inadequate key
  // usage" error but rather a "self-signed cert" error.
  let selfSignedCert = constructCertFromFile("test_self_signed_certs/ee1.pem");
  let certDB = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
  addCertFromFile(certDB, "test_self_signed_certs/ee2.pem", ",,");
  await checkCertErrorGeneric(
    certDB,
    selfSignedCert,
    MOZILLA_PKIX_ERROR_SELF_SIGNED_CERT,
    certificateUsageSSLServer,
    false,
    "example.com"
  );
});
