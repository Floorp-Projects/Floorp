/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/
"use strict";

// Tests that adding modules with invalid names are prevented.

// Ensure that the appropriate initialization has happened.
do_get_profile();

const gModuleDB = Cc["@mozilla.org/security/pkcs11moduledb;1"]
                    .getService(Ci.nsIPKCS11ModuleDB);

function run_test() {
  let libraryFile = Services.dirsvc.get("CurWorkD", Ci.nsIFile);
  libraryFile.append("pkcs11testmodule");
  libraryFile.append(ctypes.libraryName("pkcs11testmodule"));
  ok(libraryFile.exists(), "The pkcs11testmodule file should exist");

  let pkcs11ModuleDB = Cc["@mozilla.org/security/pkcs11moduledb;1"]
                         .getService(Ci.nsIPKCS11ModuleDB);
  throws(() => pkcs11ModuleDB.addModule("Root Certs", libraryFile.path, 0, 0),
         /NS_ERROR_ILLEGAL_VALUE/,
         "Adding a module named 'Root Certs' should fail.");
  throws(() => pkcs11ModuleDB.addModule("", libraryFile.path, 0, 0),
         /NS_ERROR_ILLEGAL_VALUE/,
         "Adding a module with an empty name should fail.");

  let bundle =
    Services.strings.createBundle("chrome://pipnss/locale/pipnss.properties");
  let rootsModuleName = bundle.GetStringFromName("RootCertModuleName");
  let rootsModule = pkcs11ModuleDB.findModuleByName(rootsModuleName);
  notEqual(rootsModule, null,
           "Should be able to find builtin roots module by localized name.");
  equal(rootsModule.name, rootsModuleName,
        "Builtin roots module should have correct localized name.");
}
