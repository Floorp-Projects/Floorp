"use strict";

function run_test() {
  do_get_profile();

  // Open for write, write
  asyncOpenCacheEntry(
    "http://200/",
    "disk",
    Ci.nsICacheStorage.OPEN_NORMALLY,
    null,
    new OpenCallback(NEW, "21m", "21d", function(entry) {
      asyncOpenCacheEntry(
        "http://200/",
        "disk",
        Ci.nsICacheStorage.OPEN_NORMALLY,
        null,
        new OpenCallback(NORMAL, "21m", "21d", function(entry) {
          // Open normally but wait for validation from the server
          asyncOpenCacheEntry(
            "http://200/",
            "disk",
            Ci.nsICacheStorage.OPEN_NORMALLY,
            null,
            new OpenCallback(REVAL, "21m", "21d", function(entry) {
              // emulate 200 from server (new content)
              executeSoon(function() {
                var entry2 = entry.recreate();

                // now fill the new entry, use OpenCallback directly for it
                new OpenCallback(
                  NEW,
                  "22m",
                  "22d",
                  function() {}
                ).onCacheEntryAvailable(entry2, true, Cr.NS_OK);
              });
            })
          );

          var mc = new MultipleCallbacks(3, finish_cache2_test);

          asyncOpenCacheEntry(
            "http://200/",
            "disk",
            Ci.nsICacheStorage.OPEN_NORMALLY,
            null,
            new OpenCallback(NORMAL, "22m", "22d", function(entry) {
              mc.fired();
            })
          );
          asyncOpenCacheEntry(
            "http://200/",
            "disk",
            Ci.nsICacheStorage.OPEN_NORMALLY,
            null,
            new OpenCallback(NORMAL, "22m", "22d", function(entry) {
              mc.fired();
            })
          );
          asyncOpenCacheEntry(
            "http://200/",
            "disk",
            Ci.nsICacheStorage.OPEN_NORMALLY,
            null,
            new OpenCallback(NORMAL, "22m", "22d", function(entry) {
              mc.fired();
            })
          );
        })
      );
    })
  );

  do_test_pending();
}
