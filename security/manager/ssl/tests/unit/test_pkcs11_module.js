/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/
"use strict";

// Tests the methods and attributes for interfacing with a PKCS #11 module and
// the module database.

// Ensure that the appropriate initialization has happened.
do_get_profile();

const gModuleDB = Cc["@mozilla.org/security/pkcs11moduledb;1"]
                    .getService(Ci.nsIPKCS11ModuleDB);

function checkTestModuleNotPresent() {
  let modules = gModuleDB.listModules();
  ok(modules.hasMoreElements(),
     "One or more modules should be present with test module not present");
  for (let module of modules) {
    notEqual(module.name, "PKCS11 Test Module",
             "Non-test module name shouldn't equal 'PKCS11 Test Module'");
    ok(!(module.libName && module.libName.includes("pkcs11testmodule")),
       "Non-test module lib name should not include 'pkcs11testmodule'");
  }
}

/**
 * Checks that the test module exists in the module list.
 * Also checks various attributes of the test module for correctness.
 *
 * @returns {nsIPKCS11Module}
 *          The test module.
 */
function checkTestModuleExists() {
  let modules = gModuleDB.listModules();
  ok(modules.hasMoreElements(),
     "One or more modules should be present with test module present");
  let testModule = null;
  for (let module of modules) {
    if (module.name == "PKCS11 Test Module") {
      testModule = module;
      break;
    }
  }
  notEqual(testModule, null, "Test module should have been found");
  notEqual(testModule.libName, null, "Test module lib name should not be null");
  ok(testModule.libName.includes(ctypes.libraryName("pkcs11testmodule")),
     "Test module lib name should include lib name of 'pkcs11testmodule'");

  return testModule;
}

function run_test() {
  // Check that if we have never added the test module, that we don't find it
  // in the module list.
  checkTestModuleNotPresent();

  // Check that adding the test module makes it appear in the module list.
  loadPKCS11TestModule(true);
  let testModule = checkTestModuleExists();

  // Check that listing the slots for the test module works.
  let testModuleSlotNames = Array.from(testModule.listSlots(),
                                       slot => slot.name);
  testModuleSlotNames.sort();
  const expectedSlotNames = ["Empty PKCS11 Slot", "Test PKCS11 Slot", "Test PKCS11 Slot 二"];
  deepEqual(testModuleSlotNames, expectedSlotNames,
            "Actual and expected slot names should be equal");

  // Check that deleting the test module makes it disappear from the module list.
  let pkcs11ModuleDB = Cc["@mozilla.org/security/pkcs11moduledb;1"]
                         .getService(Ci.nsIPKCS11ModuleDB);
  pkcs11ModuleDB.deleteModule("PKCS11 Test Module");
  checkTestModuleNotPresent();

  // Check miscellaneous module DB methods and attributes.
  ok(!gModuleDB.canToggleFIPS, "It should NOT be possible to toggle FIPS");
  ok(!gModuleDB.isFIPSEnabled, "FIPS should not be enabled");
}
