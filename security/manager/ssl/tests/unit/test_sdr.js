// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/
"use strict";

// Tests various aspects of the nsISecretDecoderRing implementation.

do_get_profile();

let gSetPasswordShownCount = 0;

// Mock implementation of nsITokenPasswordDialogs.
const gTokenPasswordDialogs = {
  setPassword(ctx, tokenName) {
    gSetPasswordShownCount++;
    info(`setPassword() called; shown ${gSetPasswordShownCount} times`);
    info(`tokenName: ${tokenName}`);
    return false; // Returning false means "the user didn't cancel".
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsITokenPasswordDialogs])
};

function run_test() {
  let sdr = Cc["@mozilla.org/security/sdr;1"]
              .getService(Ci.nsISecretDecoderRing);

  // Test valid inputs for encryptString() and decryptString().
  let inputs = [
    "",
    " ", // First printable latin1 character (code point 32).
    "foo",
    "1234567890`~!@#$%^&*()-_=+{[}]|\\:;'\",<.>/?",
    "¡äöüÿ", // Misc + last printable latin1 character (code point 255).
    "aaa 一二三", // Includes Unicode with code points outside [0, 255].
  ];
  for (let input of inputs) {
    let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
                      .createInstance(Ci.nsIScriptableUnicodeConverter);
    converter.charset = "UTF-8";

    let convertedInput = converter.ConvertFromUnicode(input);
    convertedInput += converter.Finish();

    let encrypted = sdr.encryptString(convertedInput);

    notEqual(convertedInput, encrypted,
             "Encrypted input should not just be the input itself");

    try {
      atob(encrypted);
    } catch (e) {
      ok(false, `encryptString() should have returned Base64: ${e}`);
    }

    equal(convertedInput, sdr.decryptString(encrypted),
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
    registerCleanupFunction(() => {
      MockRegistrar.unregister(tokenPasswordDialogsCID);
    });

    equal(gSetPasswordShownCount, 0,
          "changePassword() dialog should have been shown zero times");
    sdr.changePassword();
    equal(gSetPasswordShownCount, 1,
          "changePassword() dialog should have been shown exactly once");
  }
}
