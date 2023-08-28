"use strict";

function run_test() {
  do_get_profile();

  // Open non-existing for read, should fail
  asyncOpenCacheEntry(
    "http://b/",
    "disk",
    Ci.nsICacheStorage.OPEN_READONLY,
    null,
    new OpenCallback(NOTFOUND, null, null, function () {
      // Open the same non-existing for read again, should fail second time
      asyncOpenCacheEntry(
        "http://b/",
        "disk",
        Ci.nsICacheStorage.OPEN_READONLY,
        null,
        new OpenCallback(NOTFOUND, null, null, function () {
          // Try it again normally, should go
          asyncOpenCacheEntry(
            "http://b/",
            "disk",
            Ci.nsICacheStorage.OPEN_NORMALLY,
            null,
            new OpenCallback(NEW, "b1m", "b1d", function () {
              // ...and check
              asyncOpenCacheEntry(
                "http://b/",
                "disk",
                Ci.nsICacheStorage.OPEN_NORMALLY,
                null,
                new OpenCallback(NORMAL, "b1m", "b1d", function () {
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
