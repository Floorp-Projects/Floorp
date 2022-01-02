// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/
"use strict";

// Tests the various nsIX509CertDB import methods.

do_get_profile();

const gCertDB = Cc["@mozilla.org/security/x509certdb;1"].getService(
  Ci.nsIX509CertDB
);

const CA_CERT_COMMON_NAME = "importedCA";
const TEST_EMAIL_ADDRESS = "test@example.com";

let gCACertImportDialogCount = 0;

// Mock implementation of nsICertificateDialogs.
const gCertificateDialogs = {
  confirmDownloadCACert: (ctx, cert, trust) => {
    gCACertImportDialogCount++;
    equal(
      cert.commonName,
      CA_CERT_COMMON_NAME,
      "CA cert to import should have the correct CN"
    );
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

  QueryInterface: ChromeUtils.generateQI(["nsICertificateDialogs"]),
};

// Implements nsIInterfaceRequestor. Mostly serves to mock nsIPrompt.
const gInterfaceRequestor = {
  alert: (title, text) => {
    // We don't test anything that calls this method yet.
    ok(false, `alert() should not have been called: ${text}`);
  },

  getInterface: iid => {
    if (iid.equals(Ci.nsIPrompt)) {
      return this;
    }

    throw Components.Exception("", Cr.NS_ERROR_NO_INTERFACE);
  },
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

function commonFindCertBy(propertyName, value) {
  for (let cert of gCertDB.getCerts()) {
    if (cert[propertyName] == value) {
      return cert;
    }
  }
  return null;
}

function findCertByCommonName(commonName) {
  return commonFindCertBy("commonName", commonName);
}

function findCertByEmailAddress(emailAddress) {
  return commonFindCertBy("emailAddress", emailAddress);
}

function testImportCACert() {
  // Sanity check the CA cert is missing.
  equal(
    findCertByCommonName(CA_CERT_COMMON_NAME),
    null,
    "CA cert should not be in the database before import"
  );

  // Import and check for success.
  let caArray = getCertAsByteArray("test_certDB_import/importedCA.pem");
  gCertDB.importCertificates(
    caArray,
    caArray.length,
    Ci.nsIX509Cert.CA_CERT,
    gInterfaceRequestor
  );
  equal(
    gCACertImportDialogCount,
    1,
    "Confirmation dialog for the CA cert should only be shown once"
  );

  let caCert = findCertByCommonName(CA_CERT_COMMON_NAME);
  notEqual(caCert, null, "CA cert should now be found in the database");
  ok(
    gCertDB.isCertTrusted(
      caCert,
      Ci.nsIX509Cert.CA_CERT,
      Ci.nsIX509CertDB.TRUSTED_EMAIL
    ),
    "CA cert should be trusted for e-mail"
  );
}

function testImportEmptyCertPackage() {
  // Because this is an empty cert package, nothing will be imported. We know it succeeded if no errors are thrown.
  let byteArray = [
    0x30,
    0x0f,
    0x06,
    0x09,
    0x60,
    0x86,
    0x48,
    0x01,
    0x86,
    0xf8,
    0x42,
    0x02,
    0x05,
    0xa0,
    0x02,
    0x30,
    0x00,
  ];
  gCertDB.importCertificates(
    byteArray,
    byteArray.length,
    Ci.nsIX509Cert.CA_CERT,
    gInterfaceRequestor
  );
}

function testImportEmptyUserCert() {
  // Because this is an empty cert package, nothing will be imported. We know it succeeded if no errors are thrown.
  let byteArray = [
    0x30,
    0x0f,
    0x06,
    0x09,
    0x60,
    0x86,
    0x48,
    0x01,
    0x86,
    0xf8,
    0x42,
    0x02,
    0x05,
    0xa0,
    0x02,
    0x30,
    0x00,
  ];
  gCertDB.importUserCertificate(
    byteArray,
    byteArray.length,
    gInterfaceRequestor
  );
}

function run_test() {
  let certificateDialogsCID = MockRegistrar.register(
    "@mozilla.org/nsCertificateDialogs;1",
    gCertificateDialogs
  );
  registerCleanupFunction(() => {
    MockRegistrar.unregister(certificateDialogsCID);
  });

  // Sanity check the e-mail cert is missing.
  equal(
    findCertByEmailAddress(TEST_EMAIL_ADDRESS),
    null,
    "E-mail cert should not be in the database before import"
  );

  // Import the CA cert so that the e-mail import succeeds.
  testImportCACert();
  testImportEmptyCertPackage();
  testImportEmptyUserCert();

  // Import the e-mail cert and check for success.
  let emailArray = getCertAsByteArray("test_certDB_import/emailEE.pem");
  gCertDB.importEmailCertificate(
    emailArray,
    emailArray.length,
    gInterfaceRequestor
  );
  let emailCert = findCertByEmailAddress(TEST_EMAIL_ADDRESS);
  notEqual(emailCert, null, "E-mail cert should now be found in the database");
  let bundle = Services.strings.createBundle(
    "chrome://pipnss/locale/pipnss.properties"
  );
  equal(
    emailCert.tokenName,
    bundle.GetStringFromName("PrivateTokenDescription"),
    "cert's tokenName should be the expected localized value"
  );
}
