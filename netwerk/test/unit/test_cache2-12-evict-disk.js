"use strict";

function run_test() {
  do_get_profile();

  var mc = new MultipleCallbacks(3, function () {
    var diskStorage = getCacheStorage("disk");
    diskStorage.asyncEvictStorage(
      new EvictionCallback(true, function () {
        diskStorage.asyncVisitStorage(
          new VisitCallback(0, 0, [], function () {
            var memoryStorage = getCacheStorage("memory");
            memoryStorage.asyncVisitStorage(
              new VisitCallback(0, 0, [], function () {
                finish_cache2_test();
              }),
              true
            );
          }),
          true
        );
      })
    );
  });

  asyncOpenCacheEntry(
    "http://mem1/",
    "memory",
    Ci.nsICacheStorage.OPEN_NORMALLY,
    null,
    new OpenCallback(NEW, "m2m", "m2d", function () {
      asyncOpenCacheEntry(
        "http://mem1/",
        "memory",
        Ci.nsICacheStorage.OPEN_NORMALLY,
        null,
        new OpenCallback(NORMAL, "m2m", "m2d", function () {
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
    new OpenCallback(NEW, "a1m", "a1d", function () {
      asyncOpenCacheEntry(
        "http://a/",
        "disk",
        Ci.nsICacheStorage.OPEN_NORMALLY,
        null,
        new OpenCallback(NORMAL, "a1m", "a1d", function () {
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
    new OpenCallback(NEW, "b1m", "b1d", function () {
      asyncOpenCacheEntry(
        "http://b/",
        "disk",
        Ci.nsICacheStorage.OPEN_NORMALLY,
        null,
        new OpenCallback(NORMAL, "b1m", "b1d", function () {
          mc.fired();
        })
      );
    })
  );

  do_test_pending();
}
