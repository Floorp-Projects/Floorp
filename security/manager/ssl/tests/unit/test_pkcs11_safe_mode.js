/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// This test loads a testing PKCS #11 module. After simulating a shutdown and
// startup, the test verifies that the module is automatically loaded again.
// After simulating a shutdown and restart in safe mode, the test verifies that
// the module is not automatically loaded again.

var { XPCOMUtils } = Cu.import("resource://gre/modules/XPCOMUtils.jsm", {});

// Registers an nsIXULRuntime so the test can control whether or not the
// platform thinks it's in safe mode.
function registerXULRuntime() {
  let xulRuntime = {
    inSafeMode: false,
    logConsoleErrors: true,
    OS: "XPCShell",
    XPCOMABI: "noarch-spidermonkey",
    invalidateCachesOnRestart: function invalidateCachesOnRestart() {
      // Do nothing
    },
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIXULRuntime])
  };

  let xulRuntimeFactory = {
    createInstance: function (outer, iid) {
      if (outer != null) {
        throw Cr.NS_ERROR_NO_AGGREGATION;
      }
      return xulRuntime.QueryInterface(iid);
    }
  };

  let registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
  const XULRUNTIME_CONTRACTID = "@mozilla.org/xre/runtime;1";
  const XULRUNTIME_CID = Components.ID("{f0f0b230-5525-4127-98dc-7bca39059e70}");
  registrar.registerFactory(XULRUNTIME_CID, "XULRuntime", XULRUNTIME_CONTRACTID,
                            xulRuntimeFactory);
  return xulRuntime;
}

// Loads the PKCS11 test module.
function loadTestModule() {
  let pkcs11 = Cc["@mozilla.org/security/pkcs11;1"].getService(Ci.nsIPKCS11);
  let libraryName = ctypes.libraryName("pkcs11testmodule");
  let libraryFile = Services.dirsvc.get("CurWorkD", Ci.nsILocalFile);
  libraryFile.append("pkcs11testmodule");
  libraryFile.append(libraryName);
  ok(libraryFile.exists(), "The pkcs11testmodule file should exist");
  pkcs11.addModule("PKCS11 Test Module", libraryFile.path, 0, 0);
}

// Tests finding the PKCS11 test module. 'shouldSucceed' is a boolean
// indicating whether or not the test module is expected to be found.
function testFindTestModule(shouldSucceed) {
  let pkcs11ModuleDB = Cc["@mozilla.org/security/pkcs11moduledb;1"]
                         .getService(Ci.nsIPKCS11ModuleDB);
  try {
    let module = pkcs11ModuleDB.findModuleByName("PKCS11 Test Module");
    ok(shouldSucceed, "Success expected: findModuleByName should not throw");
    ok(module, "module should be non-null");
  } catch (e) {
    ok(!shouldSucceed, "Failure expected: findModuleByName should throw");
  }
}

function simulateShutdown() {
  let psmComponent = Cc["@mozilla.org/psm;1"].getService(Ci.nsIObserver);
  psmComponent.observe(null, "profile-change-net-teardown", null);
  psmComponent.observe(null, "profile-before-change", null);
  psmComponent.observe(null, "xpcom-shutdown", null);
}

function simulateStartup() {
  let psmComponent = Cc["@mozilla.org/psm;1"].getService(Ci.nsIObserver);
  psmComponent.observe(null, "profile-do-change", null);
}

function run_test() {
  do_get_profile();
  let xulRuntime = registerXULRuntime();
  loadTestModule();

  // After having loaded the test module, it should be available.
  testFindTestModule(true);
  simulateShutdown();
  simulateStartup();
  // After shutting down and starting up, the test module should automatically
  // be loaded again.
  testFindTestModule(true);

  simulateShutdown();
  // Simulate starting in safe mode.
  xulRuntime.inSafeMode = true;
  simulateStartup();
  // When starting in safe mode, the test module should not be loaded.
  testFindTestModule(false);
}
