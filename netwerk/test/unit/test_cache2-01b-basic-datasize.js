"use strict";

function run_test() {
  do_get_profile();

  // Open for write, write
  asyncOpenCacheEntry(
    "http://a/",
    "disk",
    Ci.nsICacheStorage.OPEN_NORMALLY,
    null,
    new OpenCallback(NEW | WAITFORWRITE, "a1m", "a1d", function (entry1) {
      // Open for read and check
      Assert.equal(entry1.dataSize, 3);
      asyncOpenCacheEntry(
        "http://a/",
        "disk",
        Ci.nsICacheStorage.OPEN_NORMALLY,
        null,
        new OpenCallback(NORMAL, "a1m", "a1d", function (entry2) {
          // Open for rewrite (truncate), write different meta and data
          Assert.equal(entry2.dataSize, 3);
          asyncOpenCacheEntry(
            "http://a/",
            "disk",
            Ci.nsICacheStorage.OPEN_TRUNCATE,
            null,
            new OpenCallback(NEW | WAITFORWRITE, "a2m", "a2d", function (
              entry3
            ) {
              // Open for read and check
              Assert.equal(entry3.dataSize, 3);
              asyncOpenCacheEntry(
                "http://a/",
                "disk",
                Ci.nsICacheStorage.OPEN_NORMALLY,
                null,
                new OpenCallback(NORMAL, "a2m", "a2d", function (entry4) {
                  Assert.equal(entry4.dataSize, 3);
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
