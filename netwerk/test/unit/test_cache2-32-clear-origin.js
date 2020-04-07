"use strict";

const URL = "http://example.net";
const URL2 = "http://foo.bar";

function run_test() {
  do_get_profile();

  asyncOpenCacheEntry(
    URL + "/a",
    "disk",
    Ci.nsICacheStorage.OPEN_NORMALLY,
    null,
    new OpenCallback(NEW, "e1m", "e1d", function(entry) {
      asyncOpenCacheEntry(
        URL + "/a",
        "disk",
        Ci.nsICacheStorage.OPEN_NORMALLY,
        null,
        new OpenCallback(NORMAL, "e1m", "e1d", function(entry) {
          asyncOpenCacheEntry(
            URL2 + "/a",
            "disk",
            Ci.nsICacheStorage.OPEN_NORMALLY,
            null,
            new OpenCallback(NEW, "f1m", "f1d", function(entry) {
              asyncOpenCacheEntry(
                URL2 + "/a",
                "disk",
                Ci.nsICacheStorage.OPEN_NORMALLY,
                null,
                new OpenCallback(NORMAL, "f1m", "f1d", function(entry) {
                  var url = Services.io.newURI(URL);
                  var principal = Services.scriptSecurityManager.createContentPrincipal(
                    url,
                    {}
                  );

                  get_cache_service().clearOrigin(principal);

                  asyncOpenCacheEntry(
                    URL + "/a",
                    "disk",
                    Ci.nsICacheStorage.OPEN_NORMALLY,
                    null,
                    new OpenCallback(NEW, "e1m", "e1d", function(entry) {
                      asyncOpenCacheEntry(
                        URL2 + "/a",
                        "disk",
                        Ci.nsICacheStorage.OPEN_NORMALLY,
                        null,
                        new OpenCallback(NORMAL, "f1m", "f1d", function(entry) {
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
    })
  );

  do_test_pending();
}
