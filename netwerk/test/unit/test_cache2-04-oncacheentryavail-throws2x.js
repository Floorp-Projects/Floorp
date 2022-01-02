"use strict";

function run_test() {
  do_get_profile();

  // Open but let OCEA throw
  asyncOpenCacheEntry(
    "http://d/",
    "disk",
    Ci.nsICacheStorage.OPEN_NORMALLY,
    null,
    new OpenCallback(NEW | THROWAVAIL, null, null, function(entry) {
      // Open but let OCEA throw ones again
      asyncOpenCacheEntry(
        "http://d/",
        "disk",
        Ci.nsICacheStorage.OPEN_NORMALLY,
        null,
        new OpenCallback(NEW | THROWAVAIL, null, null, function(entry) {
          // Try it again, should go
          asyncOpenCacheEntry(
            "http://d/",
            "disk",
            Ci.nsICacheStorage.OPEN_NORMALLY,
            null,
            new OpenCallback(NEW, "d1m", "d1d", function(entry) {
              // ...and check
              asyncOpenCacheEntry(
                "http://d/",
                "disk",
                Ci.nsICacheStorage.OPEN_NORMALLY,
                null,
                new OpenCallback(NORMAL, "d1m", "d1d", function(entry) {
                  finish_cache2_test();
                })
              );
            })
          );
        })
      );
    })
  );

  do_test_pending();
}
