"use strict";

function run_test() {
  do_get_profile();

  asyncOpenCacheEntry(
    "http://x/",
    "disk",
    Ci.nsICacheStorage.OPEN_NORMALLY,
    null,
    new OpenCallback(NEW, "x1m", "x1d", function(entry) {
      // nothing to do here, we expect concurent callbacks to get
      // all notified, then the test finishes
    })
  );

  var mc = new MultipleCallbacks(3, finish_cache2_test);

  var order = 0;

  asyncOpenCacheEntry(
    "http://x/",
    "disk",
    Ci.nsICacheStorage.OPEN_NORMALLY,
    null,
    new OpenCallback(
      NORMAL | COMPLETE | NOTIFYBEFOREREAD,
      "x1m",
      "x1d",
      function(entry, beforeReading) {
        if (beforeReading) {
          ++order;
          Assert.equal(order, 3);
        } else {
          mc.fired();
        }
      }
    )
  );
  asyncOpenCacheEntry(
    "http://x/",
    "disk",
    Ci.nsICacheStorage.OPEN_NORMALLY,
    null,
    new OpenCallback(NORMAL | NOTIFYBEFOREREAD, "x1m", "x1d", function(
      entry,
      beforeReading
    ) {
      if (beforeReading) {
        ++order;
        Assert.equal(order, 1);
      } else {
        mc.fired();
      }
    })
  );
  asyncOpenCacheEntry(
    "http://x/",
    "disk",
    Ci.nsICacheStorage.OPEN_NORMALLY,
    null,
    new OpenCallback(NORMAL | NOTIFYBEFOREREAD, "x1m", "x1d", function(
      entry,
      beforeReading
    ) {
      if (beforeReading) {
        ++order;
        Assert.equal(order, 2);
      } else {
        mc.fired();
      }
    })
  );

  do_test_pending();
}
