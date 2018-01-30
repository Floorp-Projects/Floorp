/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-common/utils.js");

function run_test() {
  let str = "Umlaute: \u00FC \u00E4\n"; // Umlaute: ü ä
  let encoded = CommonUtils.encodeUTF8(str);
  let decoded = CommonUtils.decodeUTF8(encoded);
  Assert.equal(decoded, str);
}
