// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

"use strict";

// Tests that if Firefox attempts and fails to load a PKCS#11 module DB that was
// in FIPS mode, Firefox can still make use of keys in the key database.
// secomd.db can be created via `certutil -N -d <dir>`. Putting it in FIPS mode
// involves running `modutil -fips true -dbdir <dir>`. key4.db is from
// test_sdr_preexisting/key4.db.

function run_test() {
  // Append a single quote and non-ASCII characters to the profile path.
  let profd = Services.env.get("XPCSHELL_TEST_PROFILE_DIR");
  let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
  file.initWithPath(profd);
  file.append("'÷1");
  Services.env.set("XPCSHELL_TEST_PROFILE_DIR", file.path);

  let profile = do_get_profile(); // must be called before getting nsIX509CertDB
  Assert.ok(
    /[^\x20-\x7f]/.test(profile.path),
    "the profile path should contain a non-ASCII character"
  );

  let keyDBName = "key4.db";
  let keyDBFile = do_get_file(`test_broken_fips/${keyDBName}`);
  keyDBFile.copyTo(profile, keyDBName);

  let pkcs11modDBName = "pkcs11.txt";
  let pkcs11modDBFile = do_get_file(`test_broken_fips/${pkcs11modDBName}`);
  pkcs11modDBFile.copyTo(profile, pkcs11modDBName);

  let moduleDB = Cc["@mozilla.org/security/pkcs11moduledb;1"].getService(
    Ci.nsIPKCS11ModuleDB
  );
  ok(!moduleDB.isFIPSEnabled, "FIPS should not be enabled");

  let sdr = Cc["@mozilla.org/security/sdr;1"].getService(
    Ci.nsISecretDecoderRing
  );

  const encrypted =
    "MDoEEPgAAAAAAAAAAAAAAAAAAAEwFAYIKoZIhvcNAwcECGeDHwVfyFqzBBAYvqMq/kDMsrARVNdC1C8d";
  const expectedResult = "password";
  let decrypted = sdr.decryptString(encrypted);
  equal(
    decrypted,
    expectedResult,
    "decrypted ciphertext should match expected plaintext"
  );

  let pkcs11modDBFileFIPS = do_get_profile();
  pkcs11modDBFileFIPS.append(`${pkcs11modDBName}.fips`);
  ok(
    pkcs11modDBFileFIPS.exists(),
    "backed-up PKCS#11 module db should now exist"
  );
}
