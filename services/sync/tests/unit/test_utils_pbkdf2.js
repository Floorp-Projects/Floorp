// Evil.
let btoa = Cu.import("resource://services-sync/util.js").btoa;

function run_test() {
  let symmKey16 = Utils.pbkdf2Generate("secret phrase", "DNXPzPpiwn", 4096, 16);
  do_check_eq(symmKey16.length, 16);
  do_check_eq(btoa(symmKey16), "d2zG0d2cBfXnRwMUGyMwyg==");
  do_check_eq(Utils.encodeBase32(symmKey16), "O5WMNUO5TQC7LZ2HAMKBWIZQZI======");
  let symmKey32 = Utils.pbkdf2Generate("passphrase", "salt", 4096, 32);
  do_check_eq(symmKey32.length, 32);
}
