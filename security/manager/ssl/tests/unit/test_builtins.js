// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/
"use strict";

// Tests that use a mock builtins module.

// Ensure that the appropriate initialization has happened.
do_get_profile();
const gCertDb = Cc["@mozilla.org/security/x509certdb;1"].getService(
  Ci.nsIX509CertDB
);

add_setup(function load_nssckbi_testlib() {
  let moduleName = "Mock Builtins";
  let libraryName = "test_builtins";

  checkPKCS11ModuleNotPresent(moduleName, libraryName);

  let libraryFile = Services.dirsvc.get("CurWorkD", Ci.nsIFile);
  libraryFile.append("test_builtins");
  libraryFile.append(ctypes.libraryName(libraryName));
  loadPKCS11Module(libraryFile, moduleName, true);
  let testModule = checkPKCS11ModuleExists(moduleName, libraryName);

  // Check that listing the slots for the test module works.
  let testModuleSlotNames = Array.from(
    testModule.listSlots(),
    slot => slot.name
  );
  testModuleSlotNames.sort();
  const expectedSlotNames = ["NSS Builtin Objects"];
  deepEqual(
    testModuleSlotNames,
    expectedSlotNames,
    "Actual and expected slot names should be equal"
  );
});

add_task(async function test_distrust_after() {
  let ee_pre_distrust_cert = addCertFromFile(
    gCertDb,
    "test_builtins/ee-notBefore-2021.pem",
    ",,"
  );
  notEqual(
    ee_pre_distrust_cert,
    null,
    "EE cert should have successfully loaded"
  );

  let ee_post_distrust_cert = addCertFromFile(
    gCertDb,
    "test_builtins/ee-notBefore-2023.pem",
    ",,"
  );
  notEqual(
    ee_post_distrust_cert,
    null,
    "EE cert should have successfully loaded"
  );

  let int_cert = addCertFromFile(gCertDb, "test_builtins/int.pem", ",,");
  notEqual(int_cert, null, "Intermediate cert should have successfully loaded");

  // A certificate with a notBefore before the distrustAfter date
  // should verify.
  await checkCertErrorGeneric(
    gCertDb,
    ee_pre_distrust_cert,
    PRErrorCodeSuccess,
    certificateUsageSSLServer
  );

  // A certificate with a notBefore after the distrustAfter date
  // should not verify.
  await checkCertErrorGeneric(
    gCertDb,
    ee_post_distrust_cert,
    SEC_ERROR_UNTRUSTED_ISSUER,
    certificateUsageSSLServer
  );
});
