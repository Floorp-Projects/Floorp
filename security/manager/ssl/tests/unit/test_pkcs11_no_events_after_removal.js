/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// This test loads a testing PKCS #11 module that simulates a token being
// inserted and removed from a slot every 50ms. This causes the observer
// service to broadcast the observation topics "smartcard-insert" and
// "smartcard-remove", respectively. This test ensures that these events
// are no longer emitted once the module has been unloaded.

// Ensure that the appropriate initialization has happened.
do_get_profile();
Cc["@mozilla.org/psm;1"].getService(Ci.nsISupports);

function run_test() {
  let pkcs11 = Cc["@mozilla.org/security/pkcs11;1"].getService(Ci.nsIPKCS11);
  let libraryName = ctypes.libraryName("pkcs11testmodule");
  let libraryFile = Services.dirsvc.get("CurWorkD", Ci.nsILocalFile);
  libraryFile.append("pkcs11testmodule");
  libraryFile.append(libraryName);
  ok(libraryFile.exists(), "The pkcs11testmodule file should exist");
  pkcs11.addModule("PKCS11 Test Module", libraryFile.path, 0, 0);
  pkcs11.deleteModule("PKCS11 Test Module");
  Services.obs.addObserver(function() {
    ok(false, "smartcard-insert event should not have been emitted");
  }, "smartcard-insert", false);
  Services.obs.addObserver(function() {
    ok(false, "smartcard-remove event should not have been emitted");
  }, "smartcard-remove", false);
  do_timeout(500, do_test_finished);
  do_test_pending();
}
