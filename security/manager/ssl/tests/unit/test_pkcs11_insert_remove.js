/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// This test loads a testing PKCS #11 module that simulates a token being
// inserted and removed from a slot every 50ms. This causes the observer
// service to broadcast the observation topics "smartcard-insert" and
// "smartcard-remove", respectively. This test ensures that one of each event
// has been succssfully observed, and then it unloads the test module.

// Ensure that the appropriate initialization has happened.
do_get_profile();
Cc["@mozilla.org/psm;1"].getService(Ci.nsISupports);

const gExpectedTokenLabel = "Test PKCS11 Tokeñ Label";

function SmartcardObserver(type) {
  this.type = type;
  do_test_pending();
}

SmartcardObserver.prototype = {
  observe: function(subject, topic, data) {
    equal(topic, this.type, "Observed and expected types should match");
    equal(gExpectedTokenLabel, data,
          "Expected and observed token labels should match");
    Services.obs.removeObserver(this, this.type);
    do_test_finished();
  }
};

function run_test() {
  Services.obs.addObserver(new SmartcardObserver("smartcard-insert"),
                           "smartcard-insert", false);
  Services.obs.addObserver(new SmartcardObserver("smartcard-remove"),
                           "smartcard-remove", false);

  let pkcs11 = Cc["@mozilla.org/security/pkcs11;1"].getService(Ci.nsIPKCS11);
  do_register_cleanup(function() {
    pkcs11.deleteModule("PKCS11 Test Module");
  });
  let libraryName = ctypes.libraryName("pkcs11testmodule");
  let libraryFile = Services.dirsvc.get("CurWorkD", Ci.nsILocalFile);
  libraryFile.append("pkcs11testmodule");
  libraryFile.append(libraryName);
  ok(libraryFile.exists(), "The pkcs11testmodule file should exist");
  pkcs11.addModule("PKCS11 Test Module", libraryFile.path, 0, 0);
}
