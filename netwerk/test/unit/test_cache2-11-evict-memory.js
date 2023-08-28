"use strict";

function run_test() {
  do_get_profile();

  var memoryStorage = getCacheStorage("memory");
  var mc = new MultipleCallbacks(3, function () {
    memoryStorage.asyncEvictStorage(
      new EvictionCallback(true, function () {
        memoryStorage.asyncVisitStorage(
          new VisitCallback(0, 0, [], function () {
            var diskStorage = getCacheStorage("disk");

            var expectedConsumption = 2048;

            diskStorage.asyncVisitStorage(
              new VisitCallback(
                2,
                expectedConsumption,
                ["http://a/", "http://b/"],
                function () {
                  finish_cache2_test();
                }
              ),
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
    new OpenCallback(NEW, "a1m", "a1d", function () {
      asyncOpenCacheEntry(
        "http://b/",
        "disk",
        Ci.nsICacheStorage.OPEN_NORMALLY,
        null,
        new OpenCallback(NORMAL, "a1m", "a1d", function () {
          mc.fired();
        })
      );
    })
  );

  do_test_pending();
}
