/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Evil.
let btoa = Cu.import("resource://services-common/utils.js").btoa;
Cu.import("resource://services-crypto/utils.js");

function run_test() {
  let symmKey16 = CryptoUtils.pbkdf2Generate("secret phrase", "DNXPzPpiwn", 4096, 16);
  do_check_eq(symmKey16.length, 16);
  do_check_eq(btoa(symmKey16), "d2zG0d2cBfXnRwMUGyMwyg==");
  do_check_eq(CommonUtils.encodeBase32(symmKey16), "O5WMNUO5TQC7LZ2HAMKBWIZQZI======");
  let symmKey32 = CryptoUtils.pbkdf2Generate("passphrase", "salt", 4096, 32);
  do_check_eq(symmKey32.length, 32);
}
