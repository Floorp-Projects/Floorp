/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

_("Make sure sha1 digests works with various messages");

Cu.import("resource://services-crypto/utils.js");

function run_test() {
  let mes1 = "hello";
  let mes2 = "world";

  let dig0 = CryptoUtils.UTF8AndSHA1(mes1);
  Assert.equal(dig0,
               "\xaa\xf4\xc6\x1d\xdc\xc5\xe8\xa2\xda\xbe\xde\x0f\x3b\x48\x2c\xd9\xae\xa9\x43\x4d");

  _("Make sure right sha1 digests are generated");
  let dig1 = CryptoUtils.sha1(mes1);
  Assert.equal(dig1, "aaf4c61ddcc5e8a2dabede0f3b482cd9aea9434d");
  let dig2 = CryptoUtils.sha1(mes2);
  Assert.equal(dig2, "7c211433f02071597741e6ff5a8ea34789abbf43");
  let dig12 = CryptoUtils.sha1(mes1 + mes2);
  Assert.equal(dig12, "6adfb183a4a2c94a2f92dab5ade762a47889a5a1");
  let dig21 = CryptoUtils.sha1(mes2 + mes1);
  Assert.equal(dig21, "5715790a892990382d98858c4aa38d0617151575");

  _("Repeated sha1s shouldn't change the digest");
  Assert.equal(CryptoUtils.sha1(mes1), dig1);
  Assert.equal(CryptoUtils.sha1(mes2), dig2);
  Assert.equal(CryptoUtils.sha1(mes1 + mes2), dig12);
  Assert.equal(CryptoUtils.sha1(mes2 + mes1), dig21);

  _("Nested sha1 should work just fine");
  let nest1 = CryptoUtils.sha1(CryptoUtils.sha1(CryptoUtils.sha1(CryptoUtils.sha1(CryptoUtils.sha1(mes1)))));
  Assert.equal(nest1, "23f340d0cff31e299158b3181b6bcc7e8c7f985a");
  let nest2 = CryptoUtils.sha1(CryptoUtils.sha1(CryptoUtils.sha1(CryptoUtils.sha1(CryptoUtils.sha1(mes2)))));
  Assert.equal(nest2, "1f6453867e3fb9876ae429918a64cdb8dc5ff2d0");
}
