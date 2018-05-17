// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/
"use strict";

// Tests exporting a certificate and key as a PKCS#12 blob and importing it
// again with a new password set.

do_get_profile();

const gCertDB = Cc["@mozilla.org/security/x509certdb;1"]
                  .getService(Ci.nsIX509CertDB);

const PKCS12_FILE = "test_certDB_import/cert_from_windows.pfx";
const CERT_COMMON_NAME = "test_cert_from_windows";
const TEST_CERT_PASSWORD = "黒い";
const TEST_OUTPUT_PASSWORD = "other password";

let gPasswordToUse = TEST_CERT_PASSWORD;

// Mock implementation of nsICertificateDialogs.
const gCertificateDialogs = {
  confirmDownloadCACert: () => {
    // We don't test anything that calls this method.
    ok(false, "confirmDownloadCACert() should not have been called");
  },
  setPKCS12FilePassword: (ctx, password) => {
    password.value = gPasswordToUse;
    return true;
  },
  getPKCS12FilePassword: (ctx, password) => {
    password.value = gPasswordToUse;
    return true;
  },
  viewCert: (ctx, cert) => {
    // This shouldn't be called for import methods.
    ok(false, "viewCert() should not have been called");
  },

  QueryInterface: ChromeUtils.generateQI([Ci.nsICertificateDialogs])
};

var gPrompt = {
  clickOk: true,

  QueryInterface: ChromeUtils.generateQI([Ci.nsIPrompt]),

  // This intentionally does not use arrow function syntax to avoid an issue
  // where in the context of the arrow function, |this != gPrompt| due to
  // how objects get wrapped when going across xpcom boundaries.
  alert(title, text) {
    ok(false, "Not expecting alert to be called.");
  },

  promptPassword(dialogTitle, text, password, checkMsg, checkValue) {
    ok(false, "Not expecting a password prompt.");
    return false;
  },
};

const gPromptFactory = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIPromptFactory]),
  getPrompt: (aWindow, aIID) => gPrompt,
};

function findCertByCommonName(commonName) {
  let certEnumerator = gCertDB.getCerts().getEnumerator();
  while (certEnumerator.hasMoreElements()) {
    let cert = certEnumerator.getNext().QueryInterface(Ci.nsIX509Cert);
    if (cert.commonName == commonName) {
      return cert;
    }
  }
  return null;
}

function run_test() {
  let certificateDialogsCID =
    MockRegistrar.register("@mozilla.org/nsCertificateDialogs;1",
                           gCertificateDialogs);
  let promptFactoryCID =
    MockRegistrar.register("@mozilla.org/prompter;1", gPromptFactory);

  registerCleanupFunction(() => {
    MockRegistrar.unregister(certificateDialogsCID);
    MockRegistrar.unregister(promptFactoryCID);
  });

  // Import the certificate and key so we have something to export.
  let cert = findCertByCommonName(CERT_COMMON_NAME);
  equal(cert, null, "cert should not be found before import");
  let certFile = do_get_file(PKCS12_FILE);
  ok(certFile, `${PKCS12_FILE} should exist`);
  gPasswordToUse = TEST_CERT_PASSWORD;
  gCertDB.importPKCS12File(certFile);
  cert = findCertByCommonName(CERT_COMMON_NAME);
  notEqual(cert, null, "cert should be found now");

  // Export the certificate and key.
  let output = do_get_tempdir();
  output.append("output.p12");
  ok(!output.exists(), "output shouldn't exist before exporting PKCS12 file");
  gPasswordToUse = TEST_OUTPUT_PASSWORD;
  gCertDB.exportPKCS12File(output, 1, [cert]);
  ok(output.exists(), "output should exist after exporting PKCS12 file");

  // We should be able to import the exported blob again using the new password.
  gCertDB.importPKCS12File(output);
  output.remove(false /* not a directory; recursive doesn't apply */);

  // Ideally there would be some way to confirm that this actually did anything.
  // Unfortunately, since deleting a certificate currently doesn't actually do
  // anything until the platform is restarted, we can't confirm that we
  // successfully re-imported the certificate.
}
