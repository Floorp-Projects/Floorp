"use strict";

function run_test() {
  do_get_profile();

  // Open for write, but never write and never mark valid
  asyncOpenCacheEntry(
    "http://no-data/",
    "disk",
    Ci.nsICacheStorage.OPEN_NORMALLY,
    null,
    new OpenCallback(
      NEW | METAONLY | DONTSETVALID | WAITFORWRITE,
      "meta",
      "",
      function(entry) {
        // Open again, we must get the callback and zero-length data
        executeSoon(() => {
          Cu.forceGC(); // invokes OnHandleClosed on the entry

          asyncOpenCacheEntry(
            "http://no-data/",
            "disk",
            Ci.nsICacheStorage.OPEN_NORMALLY,
            null,
            new OpenCallback(NORMAL, "meta", "", function(entry) {
              finish_cache2_test();
            })
          );
        });
      }
    )
  );

  do_test_pending();
}
