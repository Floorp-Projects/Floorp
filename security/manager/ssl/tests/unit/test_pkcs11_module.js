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
  while (modules.hasMoreElements()) {
    let module = modules.getNext().QueryInterface(Ci.nsIPKCS11Module);
    notEqual(module.name, "PKCS11 Test Module",
             "Non-test module name shouldn't equal 'PKCS11 Test Module'");
    ok(!(module.libName && module.libName.includes("pkcs11testmodule")),
       "Non-test module lib name should not include 'pkcs11testmodule'");
  }

  throws(() => gModuleDB.findModuleByName("PKCS11 Test Module"),
         /NS_ERROR_FAILURE/, "Test module should not be findable by name");
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
  while (modules.hasMoreElements()) {
    let module = modules.getNext().QueryInterface(Ci.nsIPKCS11Module);
    if (module.name == "PKCS11 Test Module") {
      testModule = module;
      break;
    }
  }
  notEqual(testModule, null, "Test module should have been found");
  notEqual(testModule.libName, null, "Test module lib name should not be null");
  ok(testModule.libName.includes(ctypes.libraryName("pkcs11testmodule")),
     "Test module lib name should include lib name of 'pkcs11testmodule'");

  notEqual(gModuleDB.findModuleByName("PKCS11 Test Module"), null,
           "Test module should be findable by name");

  return testModule;
}

function run_test() {
  let libraryFile = Services.dirsvc.get("CurWorkD", Ci.nsILocalFile);
  libraryFile.append("pkcs11testmodule");
  libraryFile.append(ctypes.libraryName("pkcs11testmodule"));
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

  // Check that finding the test slot by name is possible, and that trying to
  // find a non-present slot fails.
  notEqual(testModule.findSlotByName("Test PKCS11 Slot"), null,
           "Test slot should be findable by name");
  throws(() => testModule.findSlotByName("Not Present"), /NS_ERROR_FAILURE/,
         "Non-present slot should not be findable by name");

  // Check that the strangely named nsIPKCS11ModuleDB.findSlotByName() works.
  // In particular, a comment in nsPKCS11Slot.cpp notes that the method
  // "is essentially the same as nsIPK11Token::findTokenByName, except that it
  //  returns an nsIPKCS11Slot".
  let strBundleSvc = Cc["@mozilla.org/intl/stringbundle;1"]
                       .getService(Ci.nsIStringBundleService);
  let bundle =
    strBundleSvc.createBundle("chrome://pipnss/locale/pipnss.properties");
  let internalTokenName = bundle.GetStringFromName("PrivateTokenDescription");
  let internalTokenAsSlot = gModuleDB.findSlotByName(internalTokenName);
  notEqual(internalTokenAsSlot, null,
           "Internal 'slot' should be findable by name via the module DB");
  ok(internalTokenAsSlot instanceof Ci.nsIPKCS11Slot,
     "Module DB findSlotByName() should return a token as an nsIPKCS11Slot");
  equal(internalTokenAsSlot.name,
        bundle.GetStringFromName("PrivateSlotDescription"),
        "Spot check: actual and expected internal 'slot' names should be equal");
  throws(() => gModuleDB.findSlotByName("Not Present"), /NS_ERROR_FAILURE/,
         "Non-present 'slot' should not be findable by name via the module DB");

  // Check that deleting the test module makes it disappear from the module list.
  pkcs11.deleteModule("PKCS11 Test Module");
  checkTestModuleNotPresent();

  // Check miscellaneous module DB methods and attributes.
  notEqual(gModuleDB.getInternal(), null,
           "The internal module should be present");
  notEqual(gModuleDB.getInternalFIPS(), null,
           "The internal FIPS module should be present");
  ok(gModuleDB.canToggleFIPS, "It should be possible to toggle FIPS");
  ok(!gModuleDB.isFIPSEnabled, "FIPS should not be enabled");
}
