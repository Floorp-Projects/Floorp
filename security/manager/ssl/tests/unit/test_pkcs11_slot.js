/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/
"use strict";

// Tests the methods and attributes for interfacing with a PKCS #11 slot.

// Ensure that the appropriate initialization has happened.
do_get_profile();

function find_slot_by_name(module, name) {
  for (let slot of XPCOMUtils.IterSimpleEnumerator(module.listSlots(),
                                                   Ci.nsIPKCS11Slot)) {
    if (slot.name == name) {
      return slot;
    }
  }
  return null;
}

function find_module_by_name(moduleDB, name) {
  for (let slot of XPCOMUtils.IterSimpleEnumerator(moduleDB.listModules(),
                                                   Ci.nsIPKCS11Module)) {
    if (slot.name == name) {
      return slot;
    }
  }
  return null;
}

function run_test() {
  loadPKCS11TestModule(false);

  let moduleDB = Cc["@mozilla.org/security/pkcs11moduledb;1"]
                   .getService(Ci.nsIPKCS11ModuleDB);
  let testModule = find_module_by_name(moduleDB, "PKCS11 Test Module");
  notEqual(testModule, null, "should be able to find test module");
  let testSlot = find_slot_by_name(testModule, "Test PKCS11 Slot 二");
  notEqual(testSlot, null, "should be able to find 'Test PKCS11 Slot 二'");

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
  equal(testToken.tokenName, "Test PKCS11 Tokeñ 2 Label",
        "Spot check: the actual and expected test token names should be equal");
  ok(!testToken.isInternalKeyToken, "This token is not the internal key token");

  testSlot = find_slot_by_name(testModule, "Empty PKCS11 Slot");
  notEqual(testSlot, null, "should be able to find 'Empty PKCS11 Slot'");
  equal(testSlot.tokenName, null, "Empty slot is empty");
  equal(testSlot.status, Ci.nsIPKCS11Slot.SLOT_NOT_PRESENT,
        "Actual and expected status should match");

  let bundle =
    Services.strings.createBundle("chrome://pipnss/locale/pipnss.properties");
  let internalModule = find_module_by_name(moduleDB,
                                           "NSS Internal PKCS #11 Module");
  notEqual(internalModule, null, "should be able to find internal module");
  let cryptoSlot = find_slot_by_name(
    internalModule, bundle.GetStringFromName("TokenDescription"));
  notEqual(cryptoSlot, "should be able to find internal crypto slot");
  equal(cryptoSlot.desc, bundle.GetStringFromName("SlotDescription"),
        "crypto slot should have expected 'desc'");
  equal(cryptoSlot.manID, bundle.GetStringFromName("ManufacturerID"),
        "crypto slot should have expected 'manID'");
  let keySlot = find_slot_by_name(
    internalModule, bundle.GetStringFromName("PrivateTokenDescription"));
  notEqual(keySlot, "should be able to find internal key slot");
  equal(keySlot.desc, bundle.GetStringFromName("PrivateSlotDescription"),
        "key slot should have expected 'desc'");
  equal(keySlot.manID, bundle.GetStringFromName("ManufacturerID"),
        "key slot should have expected 'manID'");
}
