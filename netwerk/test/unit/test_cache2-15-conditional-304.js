"use strict";

function run_test() {
  do_get_profile();

  // Open for write, write
  asyncOpenCacheEntry(
    "http://304/",
    "disk",
    Ci.nsICacheStorage.OPEN_NORMALLY,
    null,
    new OpenCallback(NEW, "31m", "31d", function () {
      // Open normally but wait for validation from the server
      asyncOpenCacheEntry(
        "http://304/",
        "disk",
        Ci.nsICacheStorage.OPEN_NORMALLY,
        null,
        new OpenCallback(REVAL, "31m", "31d", function (entry) {
          // emulate 304 from the server
          executeSoon(function () {
            entry.setValid(); // this will trigger OpenCallbacks bellow
          });
        })
      );

      var mc = new MultipleCallbacks(3, finish_cache2_test);

      asyncOpenCacheEntry(
        "http://304/",
        "disk",
        Ci.nsICacheStorage.OPEN_NORMALLY,
        null,
        new OpenCallback(NORMAL, "31m", "31d", function (entry) {
          mc.fired();
        })
      );
      asyncOpenCacheEntry(
        "http://304/",
        "disk",
        Ci.nsICacheStorage.OPEN_NORMALLY,
        null,
        new OpenCallback(NORMAL, "31m", "31d", function (entry) {
          mc.fired();
        })
      );
      asyncOpenCacheEntry(
        "http://304/",
        "disk",
        Ci.nsICacheStorage.OPEN_NORMALLY,
        null,
        new OpenCallback(NORMAL, "31m", "31d", function (entry) {
          mc.fired();
        })
      );
    })
  );

  do_test_pending();
}
