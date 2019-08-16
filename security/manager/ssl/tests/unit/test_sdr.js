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

  QueryInterface: ChromeUtils.generateQI([Ci.nsITokenPasswordDialogs]),
};

add_task(function testEncryptString() {
  let sdr = Cc["@mozilla.org/security/sdr;1"].getService(
    Ci.nsISecretDecoderRing
  );

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
    let converter = Cc[
      "@mozilla.org/intl/scriptableunicodeconverter"
    ].createInstance(Ci.nsIScriptableUnicodeConverter);
    converter.charset = "UTF-8";

    let convertedInput = converter.ConvertFromUnicode(input);
    convertedInput += converter.Finish();

    let encrypted = sdr.encryptString(convertedInput);

    notEqual(
      convertedInput,
      encrypted,
      "Encrypted input should not just be the input itself"
    );

    try {
      atob(encrypted);
    } catch (e) {
      ok(false, `encryptString() should have returned Base64: ${e}`);
    }

    equal(
      convertedInput,
      sdr.decryptString(encrypted),
      "decryptString(encryptString(input)) should return input"
    );
  }

  // Test invalid inputs for decryptString().
  throws(
    () => sdr.decryptString("*"),
    /NS_ERROR_ILLEGAL_VALUE/,
    "decryptString() should throw if given non-Base64 input"
  );

  // Test calling changePassword() pops up the appropriate dialog.
  // Note: On Android, nsITokenPasswordDialogs is apparently not implemented,
  //       which also seems to prevent us from mocking out the interface.
  if (AppConstants.platform != "android") {
    let tokenPasswordDialogsCID = MockRegistrar.register(
      "@mozilla.org/nsTokenPasswordDialogs;1",
      gTokenPasswordDialogs
    );
    registerCleanupFunction(() => {
      MockRegistrar.unregister(tokenPasswordDialogsCID);
    });

    equal(
      gSetPasswordShownCount,
      0,
      "changePassword() dialog should have been shown zero times"
    );
    sdr.changePassword();
    equal(
      gSetPasswordShownCount,
      1,
      "changePassword() dialog should have been shown exactly once"
    );
  }
});

add_task(async function testAsyncEncryptStrings() {
  let sdr = Cc["@mozilla.org/security/sdr;1"].getService(
    Ci.nsISecretDecoderRing
  );

  // Test valid inputs for encryptString() and decryptString().
  let inputs = [
    "",
    " ", // First printable latin1 character (code point 32).
    "foo",
    "1234567890`~!@#$%^&*()-_=+{[}]|\\:;'\",<.>/?",
    "¡äöüÿ", // Misc + last printable latin1 character (code point 255).
    "aaa 一二三", // Includes Unicode with code points outside [0, 255].
  ];

  let encrypteds = await sdr.asyncEncryptStrings(inputs);
  for (let i = 0; i < inputs.length; i++) {
    let encrypted = encrypteds[i];
    let input = inputs[i];
    let converter = Cc[
      "@mozilla.org/intl/scriptableunicodeconverter"
    ].createInstance(Ci.nsIScriptableUnicodeConverter);
    converter.charset = "UTF-8";

    let convertedInput = converter.ConvertFromUnicode(input);
    convertedInput += converter.Finish();
    notEqual(
      convertedInput,
      encrypted,
      "Encrypted input should not just be the input itself"
    );

    try {
      atob(encrypted);
    } catch (e) {
      ok(false, `encryptString() should have returned Base64: ${e}`);
    }

    equal(
      convertedInput,
      sdr.decryptString(encrypted),
      "decryptString(encryptString(input)) should return input"
    );
  }
});

add_task(async function testAsyncDecryptStrings() {
  let sdr = Cc["@mozilla.org/security/sdr;1"].getService(
    Ci.nsISecretDecoderRing
  );

  // Test valid inputs for encryptString() and decryptString().
  let testCases = [
    "",
    " ", // First printable latin1 character (code point 32).
    "foo",
    "1234567890`~!@#$%^&*()-_=+{[}]|\\:;'\",<.>/?",
    "¡äöüÿ", // Misc + last printable latin1 character (code point 255).
    "aaa 一二三", // Includes Unicode with code points outside [0, 255].
  ];

  let convertedTestCases = testCases.map(tc => {
    let converter = Cc[
      "@mozilla.org/intl/scriptableunicodeconverter"
    ].createInstance(Ci.nsIScriptableUnicodeConverter);
    converter.charset = "UTF-8";

    let convertedInput = converter.ConvertFromUnicode(tc);
    convertedInput += converter.Finish();
    return convertedInput;
  });

  let encryptedStrings = convertedTestCases.map(tc => sdr.encryptString(tc));
  let decrypteds = await sdr.asyncDecryptStrings(encryptedStrings);
  for (let i = 0; i < encryptedStrings.length; i++) {
    let decrypted = decrypteds[i];

    equal(
      decrypted,
      testCases[i],
      "decrypted string should match expected value"
    );
    equal(
      sdr.decryptString(encryptedStrings[i]),
      convertedTestCases[i],
      "decryptString(encryptString(input)) should return the initial decrypted string value"
    );
  }
});

add_task(async function testAsyncDecryptInvalidStrings() {
  let sdr = Cc["@mozilla.org/security/sdr;1"].getService(
    Ci.nsISecretDecoderRing
  );

  // Test invalid inputs for sdr.asyncDecryptStrings
  let testCases = [
    "~bmV0cGxheQ==", // invalid base64 encoding
    "bmV0cGxheQ==", // valid base64 characters but not encrypted
    "https://www.example.com", // website address from erroneous migration
  ];

  let decrypteds = await sdr.asyncDecryptStrings(testCases);
  equal(
    decrypteds.length,
    testCases.length,
    "each testcase should still return a response"
  );
  for (let i = 0; i < decrypteds.length; i++) {
    let decrypted = decrypteds[i];

    equal(
      decrypted,
      "",
      "decrypted string should be empty when trying to decrypt an invalid input with asyncDecryptStrings"
    );

    Assert.throws(
      () => sdr.decryptString(testCases[i]),
      /NS_ERROR_ILLEGAL_VALUE|NS_ERROR_FAILURE/,
      `Check testcase would have thrown: ${testCases[i]}`
    );
  }
});

add_task(async function testAsyncDecryptLoggedOut() {
  // Set a master password.
  let token = Cc["@mozilla.org/security/pk11tokendb;1"]
    .getService(Ci.nsIPK11TokenDB)
    .getInternalKeyToken();
  token.initPassword("password");
  token.logoutSimple();

  let sdr = Cc["@mozilla.org/security/sdr;1"].getService(
    Ci.nsISecretDecoderRing
  );

  await Assert.rejects(
    sdr.asyncDecryptStrings(["irrelevant"]),
    /NS_ERROR_NOT_AVAILABLE/,
    "Check error is thrown instead of returning empty strings"
  );

  token.reset();
  token.initPassword("");
});
