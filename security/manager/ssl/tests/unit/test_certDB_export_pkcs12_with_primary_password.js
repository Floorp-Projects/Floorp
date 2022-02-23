// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/
"use strict";

// Tests exporting a certificate and key as a PKCS#12 blob if the user has a
// primary password set.

do_get_profile();

const gCertDB = Cc["@mozilla.org/security/x509certdb;1"].getService(
  Ci.nsIX509CertDB
);

const PKCS12_FILE = "test_certDB_import/cert_from_windows.pfx";
const CERT_COMMON_NAME = "test_cert_from_windows";
const TEST_CERT_PASSWORD = "黒い";

var gPrompt = {
  password: "password",
  clickOk: true,

  QueryInterface: ChromeUtils.generateQI(["nsIPrompt"]),

  // This intentionally does not use arrow function syntax to avoid an issue
  // where in the context of the arrow function, |this != gPrompt| due to
  // how objects get wrapped when going across xpcom boundaries.
  alert(title, text) {
    info(`alert('${text}')`);
    ok(false, "not expecting alert() to be called");
  },

  promptPassword(dialogTitle, text, password, checkMsg, checkValue) {
    equal(
      text,
      "Please enter your Primary Password.",
      "password prompt text should be as expected"
    );
    equal(checkMsg, null, "checkMsg should be null");
    password.value = this.password;
    return this.clickOk;
  },
};

const gPromptFactory = {
  QueryInterface: ChromeUtils.generateQI(["nsIPromptFactory"]),
  getPrompt: (aWindow, aIID) => gPrompt,
};

function findCertByCommonName(commonName) {
  for (let cert of gCertDB.getCerts()) {
    if (cert.commonName == commonName) {
      return cert;
    }
  }
  return null;
}

function run_test() {
  let promptFactoryCID = MockRegistrar.register(
    "@mozilla.org/prompter;1",
    gPromptFactory
  );

  registerCleanupFunction(() => {
    MockRegistrar.unregister(promptFactoryCID);
  });

  // Set a primary password.
  let tokenDB = Cc["@mozilla.org/security/pk11tokendb;1"].getService(
    Ci.nsIPK11TokenDB
  );
  let token = tokenDB.getInternalKeyToken();
  token.initPassword("password");
  token.logoutSimple();

  // Import the certificate and key so we have something to export.
  let cert = findCertByCommonName(CERT_COMMON_NAME);
  equal(cert, null, "cert should not be found before import");
  let certFile = do_get_file(PKCS12_FILE);
  ok(certFile, `${PKCS12_FILE} should exist`);
  let errorCode = gCertDB.importPKCS12File(certFile, TEST_CERT_PASSWORD);
  equal(errorCode, Ci.nsIX509CertDB.Success, "cert should import");
  cert = findCertByCommonName(CERT_COMMON_NAME);
  notEqual(cert, null, "cert should be found now");

  // Log out so we're prompted for the password.
  token.logoutSimple();

  // Export the certificate and key (and don't cancel the password request
  // dialog).
  let output = do_get_tempdir();
  output.append("output.p12");
  ok(!output.exists(), "output shouldn't exist before exporting PKCS12 file");
  errorCode = gCertDB.exportPKCS12File(output, [cert], TEST_CERT_PASSWORD);
  equal(errorCode, Ci.nsIX509CertDB.Success, "cert should export");
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
  errorCode = gCertDB.exportPKCS12File(output, [cert], TEST_CERT_PASSWORD);
  equal(
    errorCode,
    Ci.nsIX509CertDB.ERROR_PKCS12_BACKUP_FAILED,
    "cert should not export"
  );

  ok(!output2.exists(), "output2 shouldn't exist after failing to export");
}
