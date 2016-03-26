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

const gTokenDB = Cc["@mozilla.org/security/pk11tokendb;1"]
                   .getService(Ci.nsIPK11TokenDB);

function SmartcardObserver(type) {
  this.type = type;
  do_test_pending();
}

SmartcardObserver.prototype = {
  observe: function(subject, topic, data) {
    equal(topic, this.type, "Observed and expected types should match");
    equal(gExpectedTokenLabel, data,
          "Expected and observed token labels should match");

    // Test that the token list contains the test token only when the test
    // module is loaded.
    // Note: This test is located here out of convenience. In particular,
    //       observing the "smartcard-insert" event is the only time where it
    //       is reasonably certain for the test token to be present (see the top
    //       level comment for this file for why).
    let tokenList = gTokenDB.listTokens();
    let testTokenLabelFound = false;
    while (tokenList.hasMoreElements()) {
      let token = tokenList.getNext().QueryInterface(Ci.nsIPK11Token);
      if (token.tokenLabel == gExpectedTokenLabel) {
        testTokenLabelFound = true;
        break;
      }
    }
    let testTokenShouldBePresent = this.type == "smartcard-insert";
    equal(testTokenLabelFound, testTokenShouldBePresent,
          "Should find test token only when the test module is loaded");

    // Test that the token is findable by name only when the test module is
    // loaded.
    // Note: Again, this test is located here out of convenience.
    if (testTokenShouldBePresent) {
      notEqual(gTokenDB.findTokenByName(gExpectedTokenLabel), null,
               "Test token should be findable by name");
    } else {
      throws(() => gTokenDB.findTokenByName(gExpectedTokenLabel),
             /NS_ERROR_FAILURE/,
             "Non-present test token should not be findable by name");
    }

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
