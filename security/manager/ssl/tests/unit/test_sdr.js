// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/
"use strict";

// Tests various aspects of the nsISecretDecoderRing implementation.

do_get_profile();

let gSetPasswordShownCount = 0;

// Mock implementation of nsITokenPasswordDialogs.
const gTokenPasswordDialogs = {
  setPassword: (ctx, tokenName, canceled) => {
    gSetPasswordShownCount++;
    do_print(`setPassword() called; shown ${gSetPasswordShownCount} times`);
    do_print(`tokenName: ${tokenName}`);
    canceled.value = false;
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsITokenPasswordDialogs])
};

function run_test() {
  // We have to set a password and login before we attempt to encrypt anything.
  // In particular, failing to do so will cause the Encrypt() implementation to
  // pop up a dialog asking for a password to be set. This won't work in the
  // xpcshell environment and will lead to an assertion.
  loginToDBWithDefaultPassword();

  let sdr = Cc["@mozilla.org/security/sdr;1"]
              .getService(Ci.nsISecretDecoderRing);

  // Test valid inputs for encryptString() and decryptString().
  let inputs = [
    "",
    "foo",
    "1234567890`~!#$%^&*()-_=+{[}]|\\:;'\",<.>/?",
  ];
  for (let input of inputs) {
    let encrypted = sdr.encryptString(input);

    notEqual(input, encrypted,
             "Encypted input should not just be the input itself");

    try {
      atob(encrypted);
    } catch (e) {
      ok(false, `encryptString() should have returned Base64: ${e}`);
    }

    equal(input, sdr.decryptString(encrypted),
          "decryptString(encryptString(input)) should return input");
  }

  // Test invalid inputs for decryptString().
  throws(() => sdr.decryptString("*"), /NS_ERROR_ILLEGAL_VALUE/,
         "decryptString() should throw if given non-Base64 input");

  // Test calling changePassword() pops up the appropriate dialog.
  // Note: On Android, nsITokenPasswordDialogs is apparently not implemented,
  //       which also seems to prevent us from mocking out the interface.
  if (AppConstants.platform != "android") {
    let tokenPasswordDialogsCID =
      MockRegistrar.register("@mozilla.org/nsTokenPasswordDialogs;1",
                             gTokenPasswordDialogs);
    do_register_cleanup(() => {
      MockRegistrar.unregister(tokenPasswordDialogsCID);
    });

    equal(gSetPasswordShownCount, 0,
          "changePassword() dialog should have been shown zero times");
    sdr.changePassword();
    equal(gSetPasswordShownCount, 1,
          "changePassword() dialog should have been shown exactly once");
  }
}
