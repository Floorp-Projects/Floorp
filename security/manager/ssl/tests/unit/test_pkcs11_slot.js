/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/
"use strict";

// Tests the methods and attributes for interfacing with a PKCS #11 slot.

// Ensure that the appropriate initialization has happened.
do_get_profile();

function run_test() {
  loadPKCS11TestModule(false);

  let moduleDB = Cc["@mozilla.org/security/pkcs11moduledb;1"]
                   .getService(Ci.nsIPKCS11ModuleDB);
  let testModule = moduleDB.findModuleByName("PKCS11 Test Module");
  let testSlot = testModule.findSlotByName("Test PKCS11 Slot 二");

  equal(testSlot.name, "Test PKCS11 Slot 二",
        "Actual and expected name should match");
  equal(testSlot.desc, "Test PKCS11 Slot 二",
        "Actual and expected description should match");
  equal(testSlot.manID, "Test PKCS11 Manufacturer ID",
        "Actual and expected manufacturer ID should match");
  equal(testSlot.HWVersion, "0.0",
        "Actual and expected hardware version should match");
  equal(testSlot.FWVersion, "0.0",
        "Actual and expected firmware version should match");
  equal(testSlot.status, Ci.nsIPKCS11Slot.SLOT_READY,
        "Actual and expected status should match");
  equal(testSlot.tokenName, "Test PKCS11 Tokeñ 2 Label",
        "Actual and expected token name should match");

  let testToken = testSlot.getToken();
  notEqual(testToken, null, "getToken() should succeed");
  equal(testToken.tokenLabel, "Test PKCS11 Tokeñ 2 Label",
        "Spot check: the actual and expected test token labels should be equal");
}
