/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function continue_test(status, entry) {
  do_check_eq(status, Components.results.NS_OK);
  entry.deviceID;
  // if the above line does not crash, the test was successful
  do_test_finished();
}

function run_test() {
  asyncOpenCacheEntry("key",
                      "client",
                      Components.interfaces.nsICache.STORE_ANYWHERE,
                      Components.interfaces.nsICache.ACCESS_WRITE,
                      continue_test);
  do_test_pending();
}
