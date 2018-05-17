// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/
"use strict";

// Tests exporting a certificate and key as a PKCS#12 blob if the user has a
// master password set.

do_get_profile();

const gCertDB = Cc["@mozilla.org/security/x509certdb;1"]
                  .getService(Ci.nsIX509CertDB);

const PKCS12_FILE = "test_certDB_import/cert_from_windows.pfx";
const CERT_COMMON_NAME = "test_cert_from_windows";
const TEST_CERT_PASSWORD = "黒い";

// Mock implementation of nsICertificateDialogs.
const gCertificateDialogs = {
  confirmDownloadCACert: () => {
    // We don't test anything that calls this method.
    ok(false, "confirmDownloadCACert() should not have been called");
  },
  setPKCS12FilePassword: (ctx, password) => {
    password.value = TEST_CERT_PASSWORD;
    return true;
  },
  getPKCS12FilePassword: (ctx, password) => {
    password.value = TEST_CERT_PASSWORD;
    return true;
  },
  viewCert: (ctx, cert) => {
    // This shouldn't be called for import methods.
    ok(false, "viewCert() should not have been called");
  },

  QueryInterface: ChromeUtils.generateQI([Ci.nsICertificateDialogs])
};

var gPrompt = {
  password: "password",
  clickOk: true,
  expectingAlert: false,
  expectedAlertRegexp: null,

  QueryInterface: ChromeUtils.generateQI([Ci.nsIPrompt]),

  // This intentionally does not use arrow function syntax to avoid an issue
  // where in the context of the arrow function, |this != gPrompt| due to
  // how objects get wrapped when going across xpcom boundaries.
  alert(title, text) {
    info(`alert('${text}')`);
    ok(this.expectingAlert,
       "alert() should only be called if we're expecting it");
    ok(this.expectedAlertRegexp.test(text),
       "alert text should match expected message");
  },

  promptPassword(dialogTitle, text, password, checkMsg, checkValue) {
    equal(text,
          "Please enter your master password.",
          "password prompt text should be as expected");
    equal(checkMsg, null, "checkMsg should be null");
    password.value = this.password;
    return this.clickOk;
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

  // Set a master password.
  let tokenDB = Cc["@mozilla.org/security/pk11tokendb;1"]
                  .getService(Ci.nsIPK11TokenDB);
  let token = tokenDB.getInternalKeyToken();
  token.initPassword("password");
  token.logoutSimple();

  // Import the certificate and key so we have something to export.
  let cert = findCertByCommonName(CERT_COMMON_NAME);
  equal(cert, null, "cert should not be found before import");
  let certFile = do_get_file(PKCS12_FILE);
  ok(certFile, `${PKCS12_FILE} should exist`);
  gCertDB.importPKCS12File(certFile);
  cert = findCertByCommonName(CERT_COMMON_NAME);
  notEqual(cert, null, "cert should be found now");

  // Log out so we're prompted for the password.
  token.logoutSimple();

  // Export the certificate and key (and don't cancel the password request
  // dialog).
  let output = do_get_tempdir();
  output.append("output.p12");
  ok(!output.exists(), "output shouldn't exist before exporting PKCS12 file");
  gCertDB.exportPKCS12File(output, 1, [cert]);
  ok(output.exists(), "output should exist after exporting PKCS12 file");
  output.remove(false /* not a directory; recursive doesn't apply */);

  // Log out again so we're prompted for the password.
  token.logoutSimple();

  // Attempt to export the certificate and key, but this time cancel the
  // password request dialog. The export operation should also be canceled.
  gPrompt.clickOk = false;
  let output2 = do_get_tempdir();
  output2.append("output2.p12");
  ok(!output2.exists(), "output2 shouldn't exist before exporting PKCS12 file");
  gPrompt.expectingAlert = true;
  gPrompt.expectedAlertRegexp = /Failed to create the PKCS #12 backup file for unknown reasons\./;
  throws(() => gCertDB.exportPKCS12File(output, 1, [cert]), /NS_ERROR_FAILURE/);
  ok(!output2.exists(), "output2 shouldn't exist after failing to export");
}
