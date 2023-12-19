// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/
"use strict";

// Tests import PKCS12 file by nsIX509CertDB.

do_get_profile();

const gCertDB = Cc["@mozilla.org/security/x509certdb;1"].getService(
  Ci.nsIX509CertDB
);

const PKCS12_FILE = "test_certDB_import/cert_from_windows.pfx";
const PKCS12_FILE_EMPTY_PASS =
  "test_certDB_import/cert_from_windows_emptypass.pfx";
const PKCS12_FILE_NO_PASS = "test_certDB_import/cert_from_windows_nopass.pfx";
const CERT_COMMON_NAME = "test_cert_from_windows";
const TEST_CERT_PASSWORD = "黒い";

let gTestcases = [
  // Test that importing a PKCS12 file with the wrong password fails.
  {
    name: "import using incorrect password",
    filename: PKCS12_FILE,
    passwordToUse: "this is the wrong password",
    successExpected: false,
    errorCode: Ci.nsIX509CertDB.ERROR_BAD_PASSWORD,
    checkCertExist: true,
    certCommonName: CERT_COMMON_NAME,
  },
  // Test that importing something that isn't a PKCS12 file fails.
  {
    name: "import non-PKCS12 file",
    filename: "test_certDB_import_pkcs12.js",
    passwordToUse: TEST_CERT_PASSWORD,
    successExpected: false,
    errorCode: Ci.nsIX509CertDB.ERROR_DECODE_ERROR,
    checkCertExist: true,
    certCommonName: CERT_COMMON_NAME,
  },
  // Test that importing a PKCS12 file with the correct password succeeds.
  // This needs to be last because currently there isn't a way to delete the
  // imported certificate (and thus reset the test state) that doesn't depend on
  // the garbage collector running.
  {
    name: "import PKCS12 file",
    filename: PKCS12_FILE,
    passwordToUse: TEST_CERT_PASSWORD,
    successExpected: true,
    errorCode: Ci.nsIX509CertDB.Success,
    checkCertExist: true,
    certCommonName: CERT_COMMON_NAME,
  },
  // Same cert file protected with empty string password
  {
    name: "import PKCS12 file empty password",
    filename: PKCS12_FILE_EMPTY_PASS,
    passwordToUse: "",
    successExpected: true,
    errorCode: Ci.nsIX509CertDB.Success,
    checkCertExist: false,
    certCommonName: CERT_COMMON_NAME,
  },
  // Same cert file protected with no password
  {
    name: "import PKCS12 file no password",
    filename: PKCS12_FILE_NO_PASS,
    passwordToUse: null,
    successExpected: true,
    errorCode: Ci.nsIX509CertDB.Success,
    checkCertExist: false,
    certCommonName: CERT_COMMON_NAME,
  },
  // Test a PKCS12 file encrypted using AES
  {
    name: "import PKCS12 file using AES",
    filename: "test_certDB_import/encrypted_with_aes.p12",
    passwordToUse: "password",
    successExpected: true,
    errorCode: Ci.nsIX509CertDB.Success,
    checkCertExist: true,
    certCommonName: "John Doe",
  },
];

function doesCertExist(commonName) {
  let allCerts = gCertDB.getCerts();
  for (let cert of allCerts) {
    if (cert.commonName == commonName) {
      return true;
    }
  }

  return false;
}

function runOneTestcase(testcase) {
  info(`running ${testcase.name}`);
  if (testcase.checkCertExist) {
    ok(
      !doesCertExist(testcase.certCommonName),
      "cert should not be in the database before import"
    );
  }

  // Import and check for failure.
  let certFile = do_get_file(testcase.filename);
  ok(certFile, `${testcase.filename} should exist`);
  let errorCode = gCertDB.importPKCS12File(certFile, testcase.passwordToUse);
  equal(errorCode, testcase.errorCode, `verifying error code`);
  equal(
    doesCertExist(testcase.certCommonName),
    testcase.successExpected,
    `cert should${testcase.successExpected ? "" : " not"} be found now`
  );
}

function run_test() {
  for (let testcase of gTestcases) {
    runOneTestcase(testcase);
  }
}
