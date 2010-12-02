Cu.import("resource://services-sync/util.js");

function run_test() {
  // Testing byte array manipulation.
  do_check_eq("FOOBAR", Utils.byteArrayToString([70, 79, 79, 66, 65, 82]));
  do_check_eq("", Utils.byteArrayToString([]));
  
  _("Testing encoding...");
  // Test vectors from RFC 4648
  do_check_eq(Utils.encodeBase32(""), "");
  do_check_eq(Utils.encodeBase32("f"), "MY======");
  do_check_eq(Utils.encodeBase32("fo"), "MZXQ====");
  do_check_eq(Utils.encodeBase32("foo"), "MZXW6===");
  do_check_eq(Utils.encodeBase32("foob"), "MZXW6YQ=");
  do_check_eq(Utils.encodeBase32("fooba"), "MZXW6YTB");
  do_check_eq(Utils.encodeBase32("foobar"), "MZXW6YTBOI======");

  do_check_eq(Utils.encodeBase32("Bacon is a vegetable."),
              "IJQWG33OEBUXGIDBEB3GKZ3FORQWE3DFFY======");

  _("Checking assumptions...");
  for (let i = 0; i <= 255; ++i)
    do_check_eq(undefined | i, i);

  _("Testing decoding...");
  do_check_eq(Utils.decodeBase32(""), "");
  do_check_eq(Utils.decodeBase32("MY======"), "f");
  do_check_eq(Utils.decodeBase32("MZXQ===="), "fo");
  do_check_eq(Utils.decodeBase32("MZXW6YTB"), "fooba");
  do_check_eq(Utils.decodeBase32("MZXW6YTBOI======"), "foobar");

  // Same with incorrect or missing padding.
  do_check_eq(Utils.decodeBase32("MZXW6YTBOI=="), "foobar");
  do_check_eq(Utils.decodeBase32("MZXW6YTBOI"), "foobar");

  let encoded = Utils.encodeBase32("Bacon is a vegetable.");
  _("Encoded to " + JSON.stringify(encoded));
  do_check_eq(Utils.decodeBase32(encoded), "Bacon is a vegetable.");

  // Test failure.
  let err;
  try {
    Utils.decodeBase32("000");
  } catch (ex) {
    err = ex;
  }
  do_check_eq(err, "Unknown character in base32: 0");
  
  // Testing our own variant.
  do_check_eq(Utils.encodeKeyBase32("foobarbafoobarba"), "mzxw6ytb9jrgcztpn5rgc4tcme");
  do_check_eq(Utils.decodeKeyBase32("mzxw6ytb9jrgcztpn5rgc4tcme"), "foobarbafoobarba");
  do_check_eq(
      Utils.encodeKeyBase32("\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01"),
      "aeaqcaibaeaqcaibaeaqcaibae");
  do_check_eq(
      Utils.decodeKeyBase32("aeaqcaibaeaqcaibaeaqcaibae"),
      "\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01");
}
