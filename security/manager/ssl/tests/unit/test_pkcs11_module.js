/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/
"use strict";

// Tests the methods and attributes for interfacing with a PKCS #11 module and
// the module database.

// Ensure that the appropriate initialization has happened.
do_get_profile();

const gModuleDB = Cc["@mozilla.org/security/pkcs11moduledb;1"].getService(
  Ci.nsIPKCS11ModuleDB
);

function run_test() {
  // Check that if we have never added the test module, that we don't find it
  // in the module list.
  checkPKCS11ModuleNotPresent("PKCS11 Test Module", "pkcs11testmodule");

  // Check that adding the test module makes it appear in the module list.
  let libraryFile = Services.dirsvc.get("CurWorkD", Ci.nsIFile);
  libraryFile.append("pkcs11testmodule");
  libraryFile.append(ctypes.libraryName("pkcs11testmodule"));
  loadPKCS11Module(libraryFile, "PKCS11 Test Module", true);
  let testModule = checkPKCS11ModuleExists(
    "PKCS11 Test Module",
    "pkcs11testmodule"
  );

  // Check that listing the slots for the test module works.
  let testModuleSlotNames = Array.from(
    testModule.listSlots(),
    slot => slot.name
  );
  testModuleSlotNames.sort();
  const expectedSlotNames = [
    "Empty PKCS11 Slot",
    "Test PKCS11 Slot",
    "Test PKCS11 Slot 二",
  ];
  deepEqual(
    testModuleSlotNames,
    expectedSlotNames,
    "Actual and expected slot names should be equal"
  );

  // Check that deleting the test module makes it disappear from the module list.
  let pkcs11ModuleDB = Cc["@mozilla.org/security/pkcs11moduledb;1"].getService(
    Ci.nsIPKCS11ModuleDB
  );
  pkcs11ModuleDB.deleteModule("PKCS11 Test Module");
  checkPKCS11ModuleNotPresent("PKCS11 Test Module", "pkcs11testmodule");

  // Check miscellaneous module DB methods and attributes.
  ok(!gModuleDB.canToggleFIPS, "It should NOT be possible to toggle FIPS");
  ok(!gModuleDB.isFIPSEnabled, "FIPS should not be enabled");
}
