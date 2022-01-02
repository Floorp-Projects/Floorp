"use strict";

function run_test() {
  do_get_profile();

  // Open for write, write
  asyncOpenCacheEntry(
    "http://mt/",
    "disk",
    Ci.nsICacheStorage.OPEN_NORMALLY,
    null,
    new OpenCallback(NEW | METAONLY, "a1m", "a1d", function(entry) {
      // Open for read and check
      asyncOpenCacheEntry(
        "http://mt/",
        "disk",
        Ci.nsICacheStorage.OPEN_NORMALLY,
        null,
        new OpenCallback(NORMAL, "a1m", "", function(entry) {
          // Open for rewrite (truncate), write different meta and data
          asyncOpenCacheEntry(
            "http://mt/",
            "disk",
            Ci.nsICacheStorage.OPEN_TRUNCATE,
            null,
            new OpenCallback(NEW, "a2m", "a2d", function(entry) {
              // Open for read and check
              asyncOpenCacheEntry(
                "http://mt/",
                "disk",
                Ci.nsICacheStorage.OPEN_NORMALLY,
                null,
                new OpenCallback(NORMAL, "a2m", "a2d", function(entry) {
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
