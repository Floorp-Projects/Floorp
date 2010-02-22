/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function run_test() {
  var cs = Components.classes["@mozilla.org/network/cache-service;1"].
           getService(Components.interfaces.nsICacheService);
  var nsICache = Components.interfaces.nsICache;
  var session = cs.createSession("client",
                                 nsICache.STORE_ANYWHERE,
                                 true);
  var entry = session.openCacheEntry("key", nsICache.STORE_ON_DISK, true);
  entry.deviceID;
  // if the above line does not crash, the test was successful
}
