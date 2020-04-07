"use strict";

function run_test() {
  do_get_profile();

  // Open for write, write
  asyncOpenCacheEntry(
    "http://a/",
    "disk",
    Ci.nsICacheStorage.OPEN_NORMALLY,
    null,
    new OpenCallback(NEW, "a1m", "a1d", function(entry) {
      // Open for read and check
      asyncOpenCacheEntry(
        "http://a/",
        "disk",
        Ci.nsICacheStorage.OPEN_NORMALLY,
        null,
        new OpenCallback(NORMAL, "a1m", "a1d", function(entry) {
          // Open but don't want the entry
          asyncOpenCacheEntry(
            "http://a/",
            "disk",
            Ci.nsICacheStorage.OPEN_NORMALLY,
            null,
            new OpenCallback(NOTWANTED, "a1m", "a1d", function(entry) {
              // Open for read again and check the entry is OK
              asyncOpenCacheEntry(
                "http://a/",
                "disk",
                Ci.nsICacheStorage.OPEN_NORMALLY,
                null,
                new OpenCallback(NORMAL, "a1m", "a1d", function(entry) {
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
