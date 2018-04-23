// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/
"use strict";

// Tests that PSM can successfully ask for a password from the user and relay it
// back to NSS. Does so by mocking out the actual dialog and "filling in" the
// password. Also tests that providing an incorrect password will fail (well,
// technically the user will just get prompted again, but if they then cancel
// the dialog the overall operation will fail).

var gMockPrompter = {
  passwordToTry: null,
  numPrompts: 0,

  // This intentionally does not use arrow function syntax to avoid an issue
  // where in the context of the arrow function, |this != gMockPrompter| due to
  // how objects get wrapped when going across xpcom boundaries.
  promptPassword(dialogTitle, text, password, checkMsg, checkValue) {
    this.numPrompts++;
    if (this.numPrompts > 1) { // don't keep retrying a bad password
      return false;
    }
    equal(text,
          "Please enter your master password.",
          "password prompt text should be as expected");
    equal(checkMsg, null, "checkMsg should be null");
    ok(this.passwordToTry, "passwordToTry should be non-null");
    password.value = this.passwordToTry;
    return true;
  },

  QueryInterface: ChromeUtils.generateQI([Ci.nsIPrompt]),
};

// Mock nsIWindowWatcher. PSM calls getNewPrompter on this to get an nsIPrompt
// to call promptPassword. We return the mock one, above.
var gWindowWatcher = {
  getNewPrompter: () => gMockPrompter,
  QueryInterface: ChromeUtils.generateQI([Ci.nsIWindowWatcher]),
};

function run_test() {
  do_get_profile();

  let windowWatcherCID =
    MockRegistrar.register("@mozilla.org/embedcomp/window-watcher;1",
                           gWindowWatcher);
  registerCleanupFunction(() => {
    MockRegistrar.unregister(windowWatcherCID);
  });

  // Set an initial password.
  let tokenDB = Cc["@mozilla.org/security/pk11tokendb;1"]
                  .getService(Ci.nsIPK11TokenDB);
  let token = tokenDB.getInternalKeyToken();
  token.initPassword("hunter2");
  token.logoutSimple();

  // Try with the correct password.
  gMockPrompter.passwordToTry = "hunter2";
  // Using nsISecretDecoderRing will cause the password prompt to come up if the
  // token has a password and is logged out.
  let sdr = Cc["@mozilla.org/security/sdr;1"]
              .getService(Ci.nsISecretDecoderRing);
  sdr.encryptString("poke");
  equal(gMockPrompter.numPrompts, 1, "should have prompted for password once");

  // Reset state.
  gMockPrompter.numPrompts = 0;
  token.logoutSimple();

  // Try with an incorrect password.
  gMockPrompter.passwordToTry = "*******";
  throws(() => sdr.encryptString("poke2"), /NS_ERROR_FAILURE/,
         "logging in with the wrong password should fail");
  equal(gMockPrompter.numPrompts, 2, "should have prompted for password twice");
}
