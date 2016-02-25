/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/
"use strict";

// Tests the methods for listing PKCS #11 modules and slots via loading and
// unloading a test PKCS #11 module.

// Note: Tests for listing PKCS #11 tokens are located in
//       test_pkcs11_insert_remove.js out of convenience.

// Ensure that the appropriate initialization has happened.
do_get_profile();
Cc["@mozilla.org/psm;1"].getService(Ci.nsISupports);

const gModuleDB = Cc["@mozilla.org/security/pkcs11moduledb;1"]
                    .getService(Ci.nsIPKCS11ModuleDB);

function checkTestModuleNotPresent() {
  let modules = gModuleDB.listModules();
  ok(modules.hasMoreElements(),
     "One or more modules should be present with test module not present");
  while (modules.hasMoreElements()) {
    let module = modules.getNext().QueryInterface(Ci.nsIPKCS11Module);
    notEqual(module.name, "PKCS11 Test Module",
             "Non-test module name shouldn't equal 'PKCS11 Test Module'");
  }
}

/**
 * Checks that the test module exists in the module list.
 *
 * @returns {nsIPKCS11Module}
 *          The test module.
 */
function checkTestModuleExists() {
  let modules = gModuleDB.listModules();
  ok(modules.hasMoreElements(),
     "One or more modules should be present with test module present");
  let testModule = null;
  while (modules.hasMoreElements()) {
    let module = modules.getNext().QueryInterface(Ci.nsIPKCS11Module);
    if (module.name == "PKCS11 Test Module") {
      testModule = module;
      break;
    }
  }
  notEqual(testModule, null, "Test module should have been found");

  return testModule;
}

function run_test() {
  let libraryName = ctypes.libraryName("pkcs11testmodule");
  let libraryFile = Services.dirsvc.get("CurWorkD", Ci.nsILocalFile);
  libraryFile.append("pkcs11testmodule");
  libraryFile.append(libraryName);
  ok(libraryFile.exists(), "The pkcs11testmodule file should exist");

  // Check that if we have never added the test module, that we don't find it
  // in the module list.
  checkTestModuleNotPresent();

  // Check that adding the test module makes it appear in the module list.
  let pkcs11 = Cc["@mozilla.org/security/pkcs11;1"].getService(Ci.nsIPKCS11);
  do_register_cleanup(() => {
    try {
      pkcs11.deleteModule("PKCS11 Test Module");
    } catch (e) {
      // deleteModule() throws if the module we tell it to delete is missing,
      // or if some other thing went wrong. Since we're just cleaning up,
      // there's nothing to do even if the call fails. In addition, we delete
      // the test module during a normal run of this test file, so we need to
      // catch the exception that is raised to not have the test fail.
    }
  });
  pkcs11.addModule("PKCS11 Test Module", libraryFile.path, 0, 0);
  let testModule = checkTestModuleExists();

  // Check that listing the slots for the test module works.
  let slots = testModule.listSlots();
  let testModuleSlotCount = 0;
  while (slots.hasMoreElements()) {
    let slot = slots.getNext().QueryInterface(Ci.nsIPKCS11Slot);
    equal(slot.name, "Test PKCS11 Slot",
          "Test module slot should have correct name");
    testModuleSlotCount++;
  }
  equal(testModuleSlotCount, 1, "Test module should only have one slot");

  // Check that deleting the test module makes it disappear from the module list.
  pkcs11.deleteModule("PKCS11 Test Module");
  checkTestModuleNotPresent();
}
