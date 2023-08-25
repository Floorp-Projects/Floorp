"use strict";

function run_test() {
  do_get_profile();

  // Open for write, write
  asyncOpenCacheEntry(
    "http://a/",
    "disk",
    Ci.nsICacheStorage.OPEN_NORMALLY,
    null,
    new OpenCallback(NEW, "a1m", "a1d", function () {
      // Open for read and check
      asyncOpenCacheEntry(
        "http://a/",
        "disk",
        Ci.nsICacheStorage.OPEN_NORMALLY,
        null,
        new OpenCallback(NORMAL, "a1m", "a1d", function () {
          // Open for rewrite (truncate), write different meta and data
          asyncOpenCacheEntry(
            "http://a/",
            "disk",
            Ci.nsICacheStorage.OPEN_TRUNCATE,
            null,
            new OpenCallback(NEW, "a2m", "a2d", function () {
              // Open for read and check
              asyncOpenCacheEntry(
                "http://a/",
                "disk",
                Ci.nsICacheStorage.OPEN_NORMALLY,
                null,
                new OpenCallback(NORMAL, "a2m", "a2d", function () {
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
