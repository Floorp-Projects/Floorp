"use strict";

function run_test() {
  do_get_profile();

  asyncOpenCacheEntry(
    "http://x/",
    "disk",
    Ci.nsICacheStorage.OPEN_NORMALLY,
    null,
    new OpenCallback(NEW, "x1m", "x1d", function () {
      // nothing to do here, we expect concurent callbacks to get
      // all notified, then the test finishes
    })
  );

  var mc = new MultipleCallbacks(3, finish_cache2_test);

  asyncOpenCacheEntry(
    "http://x/",
    "disk",
    Ci.nsICacheStorage.OPEN_NORMALLY,
    null,
    new OpenCallback(NORMAL, "x1m", "x1d", function () {
      mc.fired();
    })
  );
  asyncOpenCacheEntry(
    "http://x/",
    "disk",
    Ci.nsICacheStorage.OPEN_NORMALLY,
    null,
    new OpenCallback(NORMAL, "x1m", "x1d", function () {
      mc.fired();
    })
  );
  asyncOpenCacheEntry(
    "http://x/",
    "disk",
    Ci.nsICacheStorage.OPEN_NORMALLY,
    null,
    new OpenCallback(NORMAL, "x1m", "x1d", function () {
      mc.fired();
    })
  );

  do_test_pending();
}
