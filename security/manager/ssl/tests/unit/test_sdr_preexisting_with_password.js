// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

"use strict";

// Tests that the SDR implementation is able to decrypt strings encrypted using
// a preexisting NSS key database that a) has a password and b) is in the old
// dbm format.
// To create such a database, run a version Firefox (or xpcshell) where the
// default database format is the old dbm format (i.e. pre-bug 783994), set a
// master password, and then encrypt something using nsISecretDecoderRing.
// This does not apply to Android as the dbm implementation was never enabled on
// that platform.

var gMockPrompter = {
  passwordToTry: "password",
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
  let windowWatcherCID =
    MockRegistrar.register("@mozilla.org/embedcomp/window-watcher;1",
                           gWindowWatcher);
  registerCleanupFunction(() => {
    MockRegistrar.unregister(windowWatcherCID);
  });

  let profile = do_get_profile();
  let keyDBFile = do_get_file("test_sdr_preexisting_with_password/key3.db");
  keyDBFile.copyTo(profile, "key3.db");

  let sdr = Cc["@mozilla.org/security/sdr;1"]
              .getService(Ci.nsISecretDecoderRing);

  let testcases = [
    // a full padding block
    { ciphertext: "MDoEEPgAAAAAAAAAAAAAAAAAAAEwFAYIKoZIhvcNAwcECGeDHwVfyFqzBBAYvqMq/kDMsrARVNdC1C8d",
      plaintext: "password" },
    // 7 bytes of padding
    { ciphertext: "MDIEEPgAAAAAAAAAAAAAAAAAAAEwFAYIKoZIhvcNAwcECCAzLDVmYG2/BAh3IoIsMmT8dQ==",
      plaintext: "a" },
    // 6 bytes of padding
    { ciphertext: "MDIEEPgAAAAAAAAAAAAAAAAAAAEwFAYIKoZIhvcNAwcECPN8zlZzn8FdBAiu2acpT8UHsg==",
      plaintext: "bb" },
    // 1 byte of padding
    { ciphertext: "MDIEEPgAAAAAAAAAAAAAAAAAAAEwFAYIKoZIhvcNAwcECD5px1eMKkJQBAgUPp35GlrDvQ==",
      plaintext: "!seven!" },
    // 2 bytes of padding
    { ciphertext: "MDIEEPgAAAAAAAAAAAAAAAAAAAEwFAYIKoZIhvcNAwcECMh0hLtKDyUdBAixw9UZsMt+vA==",
      plaintext: "sixsix" },
    // long plaintext requiring more than two blocks
    { ciphertext: "MFoEEPgAAAAAAAAAAAAAAAAAAAEwFAYIKoZIhvcNAwcECDRX1qi+/FX1BDATFIcIneQjvBuq3wdFxzllJt2VtUD69ACdOKAXH3eA87oHDvuHqOeCDwRy4UzoG5s=",
      plaintext: "thisismuchlongerandsotakesupmultipleblocks" },
    // this differs from the previous ciphertext by one bit and demonstrates
    // that this implementation does not enforce message integrity
    { ciphertext: "MFoEEPgAAAAAAAAAAAAAAAAAAAEwFAYIKoZIhvcNAwcECDRX1qi+/FX1BDAbFIcIneQjvBuq3wdFxzllJt2VtUD69ACdOKAXH3eA87oHDvuHqOeCDwRy4UzoG5s=",
      plaintext: "nnLbuwLRkhlongerandsotakesupmultipleblocks" },
  ];

  for (let testcase of testcases) {
    let decrypted = sdr.decryptString(testcase.ciphertext);
    equal(decrypted, testcase.plaintext,
          "decrypted ciphertext should match expected plaintext");
  }
  equal(gMockPrompter.numPrompts, 1,
        "Should have been prompted for a password once");
}
