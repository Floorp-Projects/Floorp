// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

"use strict";

// Tests that the SDR implementation is able to decrypt strings encrypted using
// a preexisting NSS key database. Creating the database is straight-forward:
// simply run Firefox (or xpcshell) and encrypt something using
// nsISecretDecoderRing (e.g. by saving a password or directly using the
// interface). The resulting key4.db file (in the profile directory) now
// contains the private key used to encrypt the data.

function run_test() {
  const keyDBName = "key4.db";
  let profile = do_get_profile();
  let keyDBFile = do_get_file(`test_sdr_preexisting/${keyDBName}`);
  keyDBFile.copyTo(profile, keyDBName);

  let sdr = Cc["@mozilla.org/security/sdr;1"].getService(
    Ci.nsISecretDecoderRing
  );

  let testcases = [
    // a full padding block
    {
      ciphertext:
        "MDoEEPgAAAAAAAAAAAAAAAAAAAEwFAYIKoZIhvcNAwcECGeDHwVfyFqzBBAYvqMq/kDMsrARVNdC1C8d",
      plaintext: "password",
    },
    // 7 bytes of padding
    {
      ciphertext:
        "MDIEEPgAAAAAAAAAAAAAAAAAAAEwFAYIKoZIhvcNAwcECCAzLDVmYG2/BAh3IoIsMmT8dQ==",
      plaintext: "a",
    },
    // 6 bytes of padding
    {
      ciphertext:
        "MDIEEPgAAAAAAAAAAAAAAAAAAAEwFAYIKoZIhvcNAwcECPN8zlZzn8FdBAiu2acpT8UHsg==",
      plaintext: "bb",
    },
    // 1 byte of padding
    {
      ciphertext:
        "MDIEEPgAAAAAAAAAAAAAAAAAAAEwFAYIKoZIhvcNAwcECD5px1eMKkJQBAgUPp35GlrDvQ==",
      plaintext: "!seven!",
    },
    // 2 bytes of padding
    {
      ciphertext:
        "MDIEEPgAAAAAAAAAAAAAAAAAAAEwFAYIKoZIhvcNAwcECMh0hLtKDyUdBAixw9UZsMt+vA==",
      plaintext: "sixsix",
    },
    // long plaintext requiring more than two blocks
    {
      ciphertext:
        "MFoEEPgAAAAAAAAAAAAAAAAAAAEwFAYIKoZIhvcNAwcECDRX1qi+/FX1BDATFIcIneQjvBuq3wdFxzllJt2VtUD69ACdOKAXH3eA87oHDvuHqOeCDwRy4UzoG5s=",
      plaintext: "thisismuchlongerandsotakesupmultipleblocks",
    },
    // this differs from the previous ciphertext by one bit and demonstrates
    // that this implementation does not enforce message integrity
    {
      ciphertext:
        "MFoEEPgAAAAAAAAAAAAAAAAAAAEwFAYIKoZIhvcNAwcECDRX1qi+/FX1BDAbFIcIneQjvBuq3wdFxzllJt2VtUD69ACdOKAXH3eA87oHDvuHqOeCDwRy4UzoG5s=",
      plaintext: "nnLbuwLRkhlongerandsotakesupmultipleblocks",
    },
  ];

  for (let testcase of testcases) {
    let decrypted = sdr.decryptString(testcase.ciphertext);
    equal(
      decrypted,
      testcase.plaintext,
      "decrypted ciphertext should match expected plaintext"
    );
  }
}
