/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-common/utils.js");

function run_test() {
  // Testing byte array manipulation.
  do_check_eq("FOOBAR", CommonUtils.byteArrayToString([70, 79, 79, 66, 65, 82]));
  do_check_eq("", CommonUtils.byteArrayToString([]));

  _("Testing encoding...");
  // Test vectors from RFC 4648
  do_check_eq(CommonUtils.encodeBase32(""), "");
  do_check_eq(CommonUtils.encodeBase32("f"), "MY======");
  do_check_eq(CommonUtils.encodeBase32("fo"), "MZXQ====");
  do_check_eq(CommonUtils.encodeBase32("foo"), "MZXW6===");
  do_check_eq(CommonUtils.encodeBase32("foob"), "MZXW6YQ=");
  do_check_eq(CommonUtils.encodeBase32("fooba"), "MZXW6YTB");
  do_check_eq(CommonUtils.encodeBase32("foobar"), "MZXW6YTBOI======");

  do_check_eq(CommonUtils.encodeBase32("Bacon is a vegetable."),
              "IJQWG33OEBUXGIDBEB3GKZ3FORQWE3DFFY======");

  _("Checking assumptions...");
  for (let i = 0; i <= 255; ++i)
    do_check_eq(undefined | i, i);

  _("Testing decoding...");
  do_check_eq(CommonUtils.decodeBase32(""), "");
  do_check_eq(CommonUtils.decodeBase32("MY======"), "f");
  do_check_eq(CommonUtils.decodeBase32("MZXQ===="), "fo");
  do_check_eq(CommonUtils.decodeBase32("MZXW6YTB"), "fooba");
  do_check_eq(CommonUtils.decodeBase32("MZXW6YTBOI======"), "foobar");

  // Same with incorrect or missing padding.
  do_check_eq(CommonUtils.decodeBase32("MZXW6YTBOI=="), "foobar");
  do_check_eq(CommonUtils.decodeBase32("MZXW6YTBOI"), "foobar");

  let encoded = CommonUtils.encodeBase32("Bacon is a vegetable.");
  _("Encoded to " + JSON.stringify(encoded));
  do_check_eq(CommonUtils.decodeBase32(encoded), "Bacon is a vegetable.");

  // Test failure.
  let err;
  try {
    CommonUtils.decodeBase32("000");
  } catch (ex) {
    err = ex;
  }
  do_check_eq(err, "Unknown character in base32: 0");
}
