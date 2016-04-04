/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/
"use strict";

// Tests the methods and attributes for interfacing with a PKCS #11 slot.

// Ensure that the appropriate initialization has happened.
do_get_profile();

function run_test() {
  let libraryFile = Services.dirsvc.get("CurWorkD", Ci.nsILocalFile);
  libraryFile.append("pkcs11testmodule");
  libraryFile.append(ctypes.libraryName("pkcs11testmodule"));
  ok(libraryFile.exists(), "The pkcs11testmodule file should exist");

  let pkcs11 = Cc["@mozilla.org/security/pkcs11;1"].getService(Ci.nsIPKCS11);
  do_register_cleanup(() => {
    pkcs11.deleteModule("PKCS11 Test Module");
  });
  pkcs11.addModule("PKCS11 Test Module", libraryFile.path, 0, 0);

  let moduleDB = Cc["@mozilla.org/security/pkcs11moduledb;1"]
                   .getService(Ci.nsIPKCS11ModuleDB);
  let testModule = moduleDB.findModuleByName("PKCS11 Test Module");
  let testSlot = testModule.findSlotByName("Test PKCS11 Slot");

  equal(testSlot.name, "Test PKCS11 Slot",
        "Actual and expected name should match");
  equal(testSlot.desc, "Test PKCS11 Slot",
        "Actual and expected description should match");
  equal(testSlot.manID, "Test PKCS11 Manufacturer ID",
        "Actual and expected manufacturer ID should match");
  equal(testSlot.HWVersion, "0.0",
        "Actual and expected hardware version should match");
  equal(testSlot.FWVersion, "0.0",
        "Actual and expected firmware version should match");
  // Note: testSlot.status is not tested because the implementation calls
  //       PK11_IsPresent(), which checks whether the test token is present.
  //       The test module inserts and removes the test token in a tight loop,
  //       so the result might not be deterministic.

  // Note: testSlot.tokenName isn't tested for the same reason testSlot.status
  //       isn't.
  let testToken = testSlot.getToken();
  notEqual(testToken, null, "getToken() should succeed");
  equal(testToken.tokenLabel, "Test PKCS11 Tokeñ Label",
        "Spot check: the actual and expected test token labels should be equal");
}
