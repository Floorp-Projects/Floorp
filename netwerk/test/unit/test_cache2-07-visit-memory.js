"use strict";

function run_test() {
  do_get_profile();

  // Add entry to the memory storage
  var mc = new MultipleCallbacks(5, function() {
    // Check it's there by visiting the storage
    syncWithCacheIOThread(function() {
      var storage = getCacheStorage("memory");
      storage.asyncVisitStorage(
        new VisitCallback(1, 12, ["http://mem1/"], function() {
          storage = getCacheStorage("disk");
          storage.asyncVisitStorage(
            // Previous tests should store 4 disk entries
            new VisitCallback(
              4,
              4096,
              ["http://a/", "http://b/", "http://c/", "http://d/"],
              function() {
                finish_cache2_test();
              }
            ),
            true
          );
        }),
        true
      );
    });
  });

  asyncOpenCacheEntry(
    "http://mem1/",
    "memory",
    Ci.nsICacheStorage.OPEN_NORMALLY,
    null,
    new OpenCallback(NEW, "m1m", "m1d", function(entry) {
      asyncOpenCacheEntry(
        "http://mem1/",
        "memory",
        Ci.nsICacheStorage.OPEN_NORMALLY,
        null,
        new OpenCallback(NORMAL, "m1m", "m1d", function(entry) {
          mc.fired();
        })
      );
    })
  );

  asyncOpenCacheEntry(
    "http://a/",
    "disk",
    Ci.nsICacheStorage.OPEN_NORMALLY,
    null,
    new OpenCallback(NEW, "a1m", "a1d", function(entry) {
      asyncOpenCacheEntry(
        "http://a/",
        "disk",
        Ci.nsICacheStorage.OPEN_NORMALLY,
        null,
        new OpenCallback(NORMAL, "a1m", "a1d", function(entry) {
          mc.fired();
        })
      );
    })
  );

  asyncOpenCacheEntry(
    "http://b/",
    "disk",
    Ci.nsICacheStorage.OPEN_NORMALLY,
    null,
    new OpenCallback(NEW, "a1m", "a1d", function(entry) {
      asyncOpenCacheEntry(
        "http://b/",
        "disk",
        Ci.nsICacheStorage.OPEN_NORMALLY,
        null,
        new OpenCallback(NORMAL, "a1m", "a1d", function(entry) {
          mc.fired();
        })
      );
    })
  );

  asyncOpenCacheEntry(
    "http://c/",
    "disk",
    Ci.nsICacheStorage.OPEN_NORMALLY,
    null,
    new OpenCallback(NEW, "a1m", "a1d", function(entry) {
      asyncOpenCacheEntry(
        "http://c/",
        "disk",
        Ci.nsICacheStorage.OPEN_NORMALLY,
        null,
        new OpenCallback(NORMAL, "a1m", "a1d", function(entry) {
          mc.fired();
        })
      );
    })
  );

  asyncOpenCacheEntry(
    "http://d/",
    "disk",
    Ci.nsICacheStorage.OPEN_NORMALLY,
    null,
    new OpenCallback(NEW, "a1m", "a1d", function(entry) {
      asyncOpenCacheEntry(
        "http://d/",
        "disk",
        Ci.nsICacheStorage.OPEN_NORMALLY,
        null,
        new OpenCallback(NORMAL, "a1m", "a1d", function(entry) {
          mc.fired();
        })
      );
    })
  );

  do_test_pending();
}
