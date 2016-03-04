/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// In safe mode, PKCS#11 modules should not be loaded. This test tests this by
// simulating starting in safe mode and then attempting to load a module.

function run_test() {
  do_get_profile();

  // Simulate starting in safe mode.
  let xulRuntime = {
    inSafeMode: true,
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
        throw new Error(Cr.NS_ERROR_NO_AGGREGATION);
      }
      return xulRuntime.QueryInterface(iid);
    }
  };

  let registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
  const XULRUNTIME_CONTRACTID = "@mozilla.org/xre/runtime;1";
  const XULRUNTIME_CID = Components.ID("{f0f0b230-5525-4127-98dc-7bca39059e70}");
  registrar.registerFactory(XULRUNTIME_CID, "XULRuntime", XULRUNTIME_CONTRACTID,
                            xulRuntimeFactory);

  // When starting in safe mode, the test module should fail to load.
  let pkcs11 = Cc["@mozilla.org/security/pkcs11;1"].getService(Ci.nsIPKCS11);
  let libraryName = ctypes.libraryName("pkcs11testmodule");
  let libraryFile = Services.dirsvc.get("CurWorkD", Ci.nsILocalFile);
  libraryFile.append("pkcs11testmodule");
  libraryFile.append(libraryName);
  ok(libraryFile.exists(), "The pkcs11testmodule file should exist");
  throws(() => pkcs11.addModule("PKCS11 Test Module", libraryFile.path, 0, 0),
         /NS_ERROR_FAILURE/, "addModule should throw when in safe mode");
}
