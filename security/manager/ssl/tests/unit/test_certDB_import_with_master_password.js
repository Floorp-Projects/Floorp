// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/
"use strict";

// Tests that a CA certificate can still be imported if the user has a master
// password set.

do_get_profile();

const gCertDB = Cc["@mozilla.org/security/x509certdb;1"]
                  .getService(Ci.nsIX509CertDB);

const CA_CERT_COMMON_NAME = "importedCA";

let gCACertImportDialogCount = 0;

// Mock implementation of nsICertificateDialogs.
const gCertificateDialogs = {
  confirmDownloadCACert: (ctx, cert, trust) => {
    gCACertImportDialogCount++;
    equal(cert.commonName, CA_CERT_COMMON_NAME,
          "CA cert to import should have the correct CN");
    trust.value = Ci.nsIX509CertDB.TRUSTED_EMAIL;
    return true;
  },
  setPKCS12FilePassword: (ctx, password) => {
    // This is only relevant to exporting.
    ok(false, "setPKCS12FilePassword() should not have been called");
  },
  getPKCS12FilePassword: (ctx, password) => {
    // We don't test anything that calls this method yet.
    ok(false, "getPKCS12FilePassword() should not have been called");
  },
  viewCert: (ctx, cert) => {
    // This shouldn't be called for import methods.
    ok(false, "viewCert() should not have been called");
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsICertificateDialogs])
};

var gMockPrompter = {
  passwordToTry: "password",
  numPrompts: 0,

  // This intentionally does not use arrow function syntax to avoid an issue
  // where in the context of the arrow function, |this != gMockPrompter| due to
  // how objects get wrapped when going across xpcom boundaries.
  promptPassword(dialogTitle, text, password, checkMsg, checkValue) {
    this.numPrompts++;
    if (this.numPrompts > 1) { // don't keep retrying a bad password
      return false;
    }
    equal(text,
          "Please enter your master password.",
          "password prompt text should be as expected");
    equal(checkMsg, null, "checkMsg should be null");
    ok(this.passwordToTry, "passwordToTry should be non-null");
    password.value = this.passwordToTry;
    return true;
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPrompt]),

  // Again with the arrow function issue.
  getInterface(iid) {
    if (iid.equals(Ci.nsIPrompt)) {
      return this;
    }

    throw new Error(Cr.NS_ERROR_NO_INTERFACE);
  }
};

function getCertAsByteArray(certPath) {
  let certFile = do_get_file(certPath, false);
  let certBytes = readFile(certFile);

  let byteArray = [];
  for (let i = 0; i < certBytes.length; i++) {
    byteArray.push(certBytes.charCodeAt(i));
  }

  return byteArray;
}

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
  registerCleanupFunction(() => {
    MockRegistrar.unregister(certificateDialogsCID);
  });

  // Set a master password.
  let tokenDB = Cc["@mozilla.org/security/pk11tokendb;1"]
                  .getService(Ci.nsIPK11TokenDB);
  let token = tokenDB.getInternalKeyToken();
  token.initPassword("password");
  token.logoutSimple();

  // Sanity check the CA cert is missing.
  equal(findCertByCommonName(CA_CERT_COMMON_NAME), null,
        "CA cert should not be in the database before import");

  // Import and check for success.
  let caArray = getCertAsByteArray("test_certDB_import/importedCA.pem");
  gCertDB.importCertificates(caArray, caArray.length, Ci.nsIX509Cert.CA_CERT,
                             gMockPrompter);
  equal(gCACertImportDialogCount, 1,
        "Confirmation dialog for the CA cert should only be shown once");

  let caCert = findCertByCommonName(CA_CERT_COMMON_NAME);
  notEqual(caCert, null, "CA cert should now be found in the database");
  ok(gCertDB.isCertTrusted(caCert, Ci.nsIX509Cert.CA_CERT,
                           Ci.nsIX509CertDB.TRUSTED_EMAIL),
     "CA cert should be trusted for e-mail");
}
