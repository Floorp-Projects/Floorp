// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/
"use strict";

// Tests import PKCS12 file by nsIX509CertDB.

do_get_profile();

const gCertDB = Cc["@mozilla.org/security/x509certdb;1"]
                  .getService(Ci.nsIX509CertDB);

const PKCS12_FILE = "test_certDB_import/cert_from_windows.pfx";
const CERT_COMMON_NAME = "test_cert_from_windows";
const TEST_CERT_PASSWORD = "黒い";

// Has getPKCS12FilePassword been called since we last reset this?
let gGetPKCS12FilePasswordCalled = false;
let gCurrentTestcase = null;

let gTestcases = [
  // Test that importing a PKCS12 file with the wrong password fails.
  {
    name: "import using incorrect password",
    filename: PKCS12_FILE,
    // cancel after the first failed password prompt
    multiplePasswordPromptsMeans: "cancel",
    expectingAlert: true,
    expectedAlertRegexp: /^The password entered was incorrect\.$/,
    passwordToUse: "this is the wrong password",
    successExpected: false,
  },
  // Test that importing something that isn't a PKCS12 file fails.
  {
    name: "import non-PKCS12 file",
    filename: "test_certDB_import_pkcs12.js",
    // cancel after the first failed password prompt
    multiplePasswordPromptsMeans: "cancel",
    expectingAlert: true,
    expectedAlertRegexp: /^Failed to decode the file\./,
    passwordToUse: TEST_CERT_PASSWORD,
    successExpected: false,
  },
  // Test that importing a PKCS12 file with the correct password succeeds.
  // This needs to be last because currently there isn't a way to delete the
  // imported certificate (and thus reset the test state) that doesn't depend on
  // the garbage collector running.
  {
    name: "import PKCS12 file",
    filename: PKCS12_FILE,
    // If we see more than one password prompt, this is a test failure.
    multiplePasswordPromptsMeans: "test failure",
    expectingAlert: false,
    expectedAlertRegexp: null,
    passwordToUse: TEST_CERT_PASSWORD,
    successExpected: true,
  },
];

// Mock implementation of nsICertificateDialogs.
const gCertificateDialogs = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsICertificateDialogs]),

  getPKCS12FilePassword: (ctx, password) => {
    if (gGetPKCS12FilePasswordCalled) {
      if (gCurrentTestcase.multiplePasswordPromptsMeans == "cancel") {
        return false;
      }
      ok(false, "getPKCS12FilePassword should be called only once");
    }

    password.value = gCurrentTestcase.passwordToUse;
    info("getPKCS12FilePassword() called");
    gGetPKCS12FilePasswordCalled = true;
    return true;
  },
};

const gPrompt = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIPrompt]),
  alert: (title, text) => {
    info(`alert('${text}')`);
    ok(gCurrentTestcase.expectingAlert,
       "alert() should only be called if we're expecting it");
    ok(gCurrentTestcase.expectedAlertRegexp.test(text),
       "alert text should match expected message");
  },
};

const gPromptFactory = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIPromptFactory]),
  getPrompt: (aWindow, aIID) => gPrompt,
};

function doesCertExist(commonName) {
  let allCerts = gCertDB.getCerts();
  let enumerator = allCerts.getEnumerator();
  while (enumerator.hasMoreElements()) {
    let cert = enumerator.getNext().QueryInterface(Ci.nsIX509Cert);
    if (cert.isBuiltInRoot) {
      continue;
    }
    if (cert.commonName == commonName) {
      return true;
    }
  }

  return false;
}

function runOneTestcase(testcase) {
  info(`running ${testcase.name}`);
  ok(!doesCertExist(CERT_COMMON_NAME),
     "cert should not be in the database before import");

  // Import and check for failure.
  let certFile = do_get_file(testcase.filename);
  ok(certFile, `${testcase.filename} should exist`);
  gGetPKCS12FilePasswordCalled = false;
  gCurrentTestcase = testcase;
  gCertDB.importPKCS12File(certFile);
  ok(gGetPKCS12FilePasswordCalled,
     "getPKCS12FilePassword should have been called");

  equal(doesCertExist(CERT_COMMON_NAME), testcase.successExpected,
        `cert should${testcase.successExpected ? "" : " not"} be found now`);
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

  for (let testcase of gTestcases) {
    runOneTestcase(testcase);
  }
}
