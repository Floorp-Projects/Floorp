/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-common/utils.js");

function run_test() {
  // Testing byte array manipulation.
  Assert.equal("FOOBAR", CommonUtils.byteArrayToString([70, 79, 79, 66, 65, 82]));
  Assert.equal("", CommonUtils.byteArrayToString([]));

  _("Testing encoding...");
  // Test vectors from RFC 4648
  Assert.equal(CommonUtils.encodeBase32(""), "");
  Assert.equal(CommonUtils.encodeBase32("f"), "MY======");
  Assert.equal(CommonUtils.encodeBase32("fo"), "MZXQ====");
  Assert.equal(CommonUtils.encodeBase32("foo"), "MZXW6===");
  Assert.equal(CommonUtils.encodeBase32("foob"), "MZXW6YQ=");
  Assert.equal(CommonUtils.encodeBase32("fooba"), "MZXW6YTB");
  Assert.equal(CommonUtils.encodeBase32("foobar"), "MZXW6YTBOI======");

  Assert.equal(CommonUtils.encodeBase32("Bacon is a vegetable."),
               "IJQWG33OEBUXGIDBEB3GKZ3FORQWE3DFFY======");

  _("Checking assumptions...");
  for (let i = 0; i <= 255; ++i)
    Assert.equal(undefined | i, i);

  _("Testing decoding...");
  Assert.equal(CommonUtils.decodeBase32(""), "");
  Assert.equal(CommonUtils.decodeBase32("MY======"), "f");
  Assert.equal(CommonUtils.decodeBase32("MZXQ===="), "fo");
  Assert.equal(CommonUtils.decodeBase32("MZXW6YTB"), "fooba");
  Assert.equal(CommonUtils.decodeBase32("MZXW6YTBOI======"), "foobar");

  // Same with incorrect or missing padding.
  Assert.equal(CommonUtils.decodeBase32("MZXW6YTBOI=="), "foobar");
  Assert.equal(CommonUtils.decodeBase32("MZXW6YTBOI"), "foobar");

  let encoded = CommonUtils.encodeBase32("Bacon is a vegetable.");
  _("Encoded to " + JSON.stringify(encoded));
  Assert.equal(CommonUtils.decodeBase32(encoded), "Bacon is a vegetable.");

  // Test failure.
  let err;
  try {
    CommonUtils.decodeBase32("000");
  } catch (ex) {
    err = ex;
  }
  Assert.equal(err.message, "Unknown character in base32: 0");
}
