"use strict";

function run_test() {
  do_get_profile();

  // Create and check an entry anon disk storage
  asyncOpenCacheEntry(
    "http://anon1/",
    "disk",
    Ci.nsICacheStorage.OPEN_NORMALLY,
    Services.loadContextInfo.anonymous,
    new OpenCallback(NEW, "an1", "an1", function () {
      asyncOpenCacheEntry(
        "http://anon1/",
        "disk",
        Ci.nsICacheStorage.OPEN_NORMALLY,
        Services.loadContextInfo.anonymous,
        new OpenCallback(NORMAL, "an1", "an1", function () {
          // Create and check an entry non-anon disk storage
          asyncOpenCacheEntry(
            "http://anon1/",
            "disk",
            Ci.nsICacheStorage.OPEN_NORMALLY,
            Services.loadContextInfo.default,
            new OpenCallback(NEW, "na1", "na1", function () {
              asyncOpenCacheEntry(
                "http://anon1/",
                "disk",
                Ci.nsICacheStorage.OPEN_NORMALLY,
                Services.loadContextInfo.default,
                new OpenCallback(NORMAL, "na1", "na1", function () {
                  // check the anon entry is still there and intact
                  asyncOpenCacheEntry(
                    "http://anon1/",
                    "disk",
                    Ci.nsICacheStorage.OPEN_NORMALLY,
                    Services.loadContextInfo.anonymous,
                    new OpenCallback(NORMAL, "an1", "an1", function () {
                      finish_cache2_test();
                    })
                  );
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
