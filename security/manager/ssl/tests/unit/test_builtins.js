// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/
"use strict";

// Tests that use a mock builtins module.

// Ensure that the appropriate initialization has happened.
do_get_profile();

add_setup(function load_nssckbi_testlib() {
  let moduleName = "Mock Builtins";
  let libraryName = "test-builtins";

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
