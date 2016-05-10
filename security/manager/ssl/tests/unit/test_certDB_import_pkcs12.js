// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/
"use strict";

// Tests import PKCS12 file by nsIX509CertDB.

do_get_profile();

const gCertDB = Cc["@mozilla.org/security/x509certdb;1"]
                  .getService(Ci.nsIX509CertDB);

const CERT_COMMON_NAME = "test_cert_from_windows";
const TEST_CERT_PASSWORD = "黒い";

let gGetPKCS12Password = false;

// Mock implementation of nsICertificateDialogs.
const gCertificateDialogs = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsICertificateDialogs]),

  getPKCS12FilePassword: (ctx, password) => {
    ok(!gGetPKCS12Password,
       "getPKCS12FilePassword should be called only once.");

    password.value = TEST_CERT_PASSWORD;
    do_print("getPKCS12FilePassword() is called");
    gGetPKCS12Password = true;
    return true;
  },
};

const gPrompt = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPrompt]),
  alert: (title, text) => {
    do_print("alert('" + text + "')");
  },
};

const gPromptFactory = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPromptFactory]),
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

function testImportPKCS12Cert() {
  ok(!doesCertExist(CERT_COMMON_NAME),
     "Cert should not be in the database before import");

  // Import and check for success.
  let certFile = do_get_file("test_certDB_import/cert_from_windows.pfx");
  gCertDB.importPKCS12File(null, certFile);

  ok(gGetPKCS12Password, "PKCS12 password should be asked");

  ok(doesCertExist(CERT_COMMON_NAME),
     "Cert should now be found in the database");
}

function run_test() {
  // We have to set a password and login before we attempt to import anything.
  loginToDBWithDefaultPassword();

  let certificateDialogsCID =
    MockRegistrar.register("@mozilla.org/nsCertificateDialogs;1",
                           gCertificateDialogs);
  let promptFactoryCID =
    MockRegistrar.register("@mozilla.org/prompter;1", gPromptFactory);

  do_register_cleanup(() => {
    MockRegistrar.unregister(certificateDialogsCID);
    MockRegistrar.unregister(promptFactoryCID);
  });

  // Import PKCS12 file with utf-8 password
  testImportPKCS12Cert();
}
