/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/
"use strict";

// Tests the methods and attributes for interfacing with a PKCS #11 token, using
// the internal key token.
// Note: We don't use the test token in the test PKCS #11 module because it is
//       inconvenient to test. See the top level comment in
//       test_pkcs11_insert_remove.js for why. However, some token DB tests are
//       located in that file out of convenience.

// Ensure that the appropriate initialization has happened.
do_get_profile();

function checkBasicAttributes(token) {
  let strBundleSvc = Cc["@mozilla.org/intl/stringbundle;1"]
                       .getService(Ci.nsIStringBundleService);
  let bundle =
    strBundleSvc.createBundle("chrome://pipnss/locale/pipnss.properties");

  let expectedTokenName = bundle.GetStringFromName("PrivateTokenDescription");
  equal(token.tokenName, expectedTokenName,
        "Actual and expected name should match");
  equal(token.tokenLabel, expectedTokenName,
        "Actual and expected label should match");
  equal(token.tokenManID, bundle.GetStringFromName("ManufacturerID"),
        "Actual and expected manufacturer ID should match");
  equal(token.tokenHWVersion, "0.0",
        "Actual and expected hardware version should match");
  equal(token.tokenFWVersion, "0.0",
        "Actual and expected firmware version should match");
  equal(token.tokenSerialNumber, "0000000000000000",
        "Actual and expected serial number should match");
}

/**
 * Checks the various password related features of the given token.
 * The token should already have been init with a password and be logged into.
 * The password of the token will be reset after calling this function.
 *
 * @param {nsIPK11Token} token
 *        The token to test.
 * @param {String} initialPW
 *        The password that the token should have been init with.
 */
function checkPasswordFeaturesAndResetPassword(token, initialPW) {
  ok(!token.needsUserInit,
     "Token should not need user init after setting a password");

  equal(token.minimumPasswordLength, 0,
        "Actual and expected min password length should match");

  token.setAskPasswordDefaults(10, 20);
  equal(token.getAskPasswordTimes(), 10,
        "Actual and expected ask password times should match");
  equal(token.getAskPasswordTimeout(), 20,
        "Actual and expected ask password timeout should match");

  ok(token.checkPassword(initialPW),
     "checkPassword() should succeed if the correct initial password is given");
  token.changePassword(initialPW, "newPW");
  ok(token.checkPassword("newPW"),
     "checkPassword() should succeed if the correct new password is given");

  ok(!token.checkPassword("wrongPW"),
     "checkPassword() should fail if an incorrect password is given");
  ok(!token.isLoggedIn(),
     "Token should be logged out after an incorrect password was given");
  ok(!token.needsUserInit,
     "Token should still be init with a password even if an incorrect " +
     "password was given");

  token.reset();
  ok(token.needsUserInit,
     "Token should need password init after reset");
  ok(!token.isLoggedIn(), "Token should be logged out of after reset");
}

function run_test() {
  let tokenDB = Cc["@mozilla.org/security/pk11tokendb;1"]
                  .getService(Ci.nsIPK11TokenDB);
  let token = tokenDB.getInternalKeyToken();
  notEqual(token, null, "The internal token should be present");

  checkBasicAttributes(token);

  ok(!token.isLoggedIn(), "Token should not be logged into yet");
  // Test that attempting to log out even when the token was not logged into
  // does not result in an error.
  token.logoutSimple();
  ok(!token.isLoggedIn(), "Token should still not be logged into");

  let initialPW = "foo 1234567890`~!@#$%^&*()-_=+{[}]|\\:;'\",<.>/? 一二三";
  token.initPassword(initialPW);
  token.login(/*force*/ false);
  ok(token.isLoggedIn(), "Token should now be logged into");

  checkPasswordFeaturesAndResetPassword(token, initialPW);

  // We reset the password previously, so we need to initialize again.
  token.initPassword("arbitrary");
  ok(token.isLoggedIn(),
     "Token should be logged into after initializing password again");
  token.logoutSimple();
  ok(!token.isLoggedIn(),
     "Token should be logged out after calling logoutSimple()");

  ok(!token.isHardwareToken(),
     "The internal token should not be considered a hardware token");
  ok(token.isFriendly(),
     "The internal token should always be considered friendly");
  ok(token.needsLogin(),
     "The internal token should always need authentication");
}
