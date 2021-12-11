/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

function continue_test(status, entry) {
  Assert.equal(status, Cr.NS_OK);
  // TODO - mayhemer: remove this tests completely
  // entry.deviceID;
  // if the above line does not crash, the test was successful
  do_test_finished();
}

function run_test() {
  asyncOpenCacheEntry(
    "http://some.key/",
    "disk",
    Ci.nsICacheStorage.OPEN_NORMALLY,
    null,
    continue_test
  );
  do_test_pending();
}
