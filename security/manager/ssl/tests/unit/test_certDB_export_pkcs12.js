// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/
"use strict";

// Tests exporting a certificate and key as a PKCS#12 blob and importing it
// again with a new password set.

do_get_profile();

const gCertDB = Cc["@mozilla.org/security/x509certdb;1"].getService(
  Ci.nsIX509CertDB
);

const PKCS12_FILE = "test_certDB_import/cert_from_windows.pfx";
const CERT_COMMON_NAME = "test_cert_from_windows";
const TEST_CERT_PASSWORD = "黒い";

function findCertByCommonName(commonName) {
  for (let cert of gCertDB.getCerts()) {
    if (cert.commonName == commonName) {
      return cert;
    }
  }
  return null;
}

function run_test() {
  // Import the certificate and key so we have something to export.
  let cert = findCertByCommonName(CERT_COMMON_NAME);
  equal(cert, null, "cert should not be found before import");
  let certFile = do_get_file(PKCS12_FILE);
  ok(certFile, `${PKCS12_FILE} should exist`);
  let errorCode = gCertDB.importPKCS12File(certFile, TEST_CERT_PASSWORD);
  equal(errorCode, Ci.nsIX509CertDB.Success, "cert should be imported");
  cert = findCertByCommonName(CERT_COMMON_NAME);
  notEqual(cert, null, "cert should be found now");

  // Export the certificate and key.
  let output = do_get_tempdir();
  output.append("output.p12");
  ok(!output.exists(), "output shouldn't exist before exporting PKCS12 file");
  errorCode = gCertDB.exportPKCS12File(output, [cert], TEST_CERT_PASSWORD);
  equal(errorCode, Ci.nsIX509CertDB.Success, "cert should be exported");
  ok(output.exists(), "output should exist after exporting PKCS12 file");

  // We should be able to import the exported blob again using the new password.
  errorCode = gCertDB.importPKCS12File(output, TEST_CERT_PASSWORD);
  equal(errorCode, Ci.nsIX509CertDB.Success, "cert should be imported");
  output.remove(false /* not a directory; recursive doesn't apply */);

  // Ideally there would be some way to confirm that this actually did anything.
  // Unfortunately, since deleting a certificate currently doesn't actually do
  // anything until the platform is restarted, we can't confirm that we
  // successfully re-imported the certificate.
}
