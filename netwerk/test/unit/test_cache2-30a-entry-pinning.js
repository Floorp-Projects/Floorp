"use strict";

function run_test() {
  do_get_profile();

  // Open for write, write
  asyncOpenCacheEntry(
    "http://a/",
    "pin",
    Ci.nsICacheStorage.OPEN_TRUNCATE,
    Services.loadContextInfo.default,
    new OpenCallback(NEW | WAITFORWRITE, "a1m", "a1d", function () {
      // Open for read and check
      asyncOpenCacheEntry(
        "http://a/",
        "disk",
        Ci.nsICacheStorage.OPEN_NORMALLY,
        Services.loadContextInfo.default,
        new OpenCallback(NORMAL, "a1m", "a1d", function () {
          // Now clear the whole cache
          Services.cache2.clear();

          // The pinned entry should be intact
          asyncOpenCacheEntry(
            "http://a/",
            "disk",
            Ci.nsICacheStorage.OPEN_NORMALLY,
            Services.loadContextInfo.default,
            new OpenCallback(NORMAL, "a1m", "a1d", function () {
              finish_cache2_test();
            })
          );
        })
      );
    })
  );

  do_test_pending();
}
