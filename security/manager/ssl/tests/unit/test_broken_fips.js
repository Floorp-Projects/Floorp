// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

"use strict";

// Tests that if Firefox attempts and fails to load a PKCS#11 module DB that was
// in FIPS mode, Firefox can still make use of keys in the key database.
// secomd.db can be created via `certutil -N -d <dir>`. Putting it in FIPS mode
// involves running `modutil -fips true -dbdir <dir>`. key3.db is from
// test_sdr_preexisting/key3.db.

function run_test() {
  let profile = do_get_profile();

  let keyDBName = "key3.db";
  let keyDBFile = do_get_file(`test_broken_fips/${keyDBName}`);
  keyDBFile.copyTo(profile, keyDBName);

  let secmodDBName = "secmod.db";
  let secmodDBFile = do_get_file(`test_broken_fips/${secmodDBName}`);
  secmodDBFile.copyTo(profile, secmodDBName);

  let moduleDB = Cc["@mozilla.org/security/pkcs11moduledb;1"]
                   .getService(Ci.nsIPKCS11ModuleDB);
  ok(!moduleDB.isFIPSEnabled, "FIPS should not be enabled");

  let sdr = Cc["@mozilla.org/security/sdr;1"]
              .getService(Ci.nsISecretDecoderRing);

  const encrypted = "MDoEEPgAAAAAAAAAAAAAAAAAAAEwFAYIKoZIhvcNAwcECGeDHwVfyFqzBBAYvqMq/kDMsrARVNdC1C8d";
  const expectedResult = "password";
  let decrypted = sdr.decryptString(encrypted);
  equal(decrypted, expectedResult,
        "decrypted ciphertext should match expected plaintext");

  let secmodDBFileFIPS = do_get_profile();
  secmodDBFileFIPS.append(`${secmodDBName}.fips`);
  ok(secmodDBFileFIPS.exists(), "backed-up PKCS#11 module db should now exist");
}
