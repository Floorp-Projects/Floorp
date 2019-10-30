/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/
"use strict";

// Tests that the platform can load the osclientcerts module.

// Ensure that the appropriate initialization has happened.
do_get_profile();

function run_test() {
  // Check that if we have never added the osclientcerts module, that we don't
  // find it in the module list.
  checkPKCS11ModuleNotPresent("OS Client Cert Module", "osclientcerts");

  // Check that adding the osclientcerts module makes it appear in the module
  // list.
  let libraryFile = Services.dirsvc.get("GreBinD", Ci.nsIFile);
  libraryFile.append(ctypes.libraryName("osclientcerts"));
  loadPKCS11Module(libraryFile, "OS Client Cert Module", true);
  let testModule = checkPKCS11ModuleExists(
    "OS Client Cert Module",
    "osclientcerts"
  );

  // Check that listing the slots for the osclientcerts module works.
  let testModuleSlotNames = Array.from(
    testModule.listSlots(),
    slot => slot.name
  );
  testModuleSlotNames.sort();
  const expectedSlotNames = ["OS Client Cert Slot"];
  deepEqual(
    testModuleSlotNames,
    expectedSlotNames,
    "Actual and expected slot names should be equal"
  );

  // Check that deleting the osclientcerts module makes it disappear from the
  // module list.
  let pkcs11ModuleDB = Cc["@mozilla.org/security/pkcs11moduledb;1"].getService(
    Ci.nsIPKCS11ModuleDB
  );
  pkcs11ModuleDB.deleteModule("OS Client Cert Module");
  checkPKCS11ModuleNotPresent("OS Client Cert Module", "osclientcerts");
}
