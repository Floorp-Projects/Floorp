"use strict";

function run_test() {
  do_get_profile();

  // Open for write, write
  asyncOpenCacheEntry(
    "http://r206/",
    "disk",
    Ci.nsICacheStorage.OPEN_NORMALLY,
    null,
    new OpenCallback(NEW, "206m", "206part1-", function () {
      // Open normally but wait for validation from the server
      asyncOpenCacheEntry(
        "http://r206/",
        "disk",
        Ci.nsICacheStorage.OPEN_NORMALLY,
        null,
        new OpenCallback(PARTIAL, "206m", "206part1-", function (entry) {
          // emulate 206 from the server, i.e. resume transaction and write content to the output stream
          new OpenCallback(
            NEW | WAITFORWRITE | PARTIAL,
            "206m",
            "-part2",
            function (entry1) {
              entry1.setValid();
            }
          ).onCacheEntryAvailable(entry, true, Cr.NS_OK);
        })
      );

      var mc = new MultipleCallbacks(3, finish_cache2_test);

      asyncOpenCacheEntry(
        "http://r206/",
        "disk",
        Ci.nsICacheStorage.OPEN_NORMALLY,
        null,
        new OpenCallback(NORMAL, "206m", "206part1--part2", function (entry) {
          mc.fired();
        })
      );
      asyncOpenCacheEntry(
        "http://r206/",
        "disk",
        Ci.nsICacheStorage.OPEN_NORMALLY,
        null,
        new OpenCallback(NORMAL, "206m", "206part1--part2", function (entry) {
          mc.fired();
        })
      );
      asyncOpenCacheEntry(
        "http://r206/",
        "disk",
        Ci.nsICacheStorage.OPEN_NORMALLY,
        null,
        new OpenCallback(NORMAL, "206m", "206part1--part2", function (entry) {
          mc.fired();
        })
      );
    })
  );

  do_test_pending();
}
