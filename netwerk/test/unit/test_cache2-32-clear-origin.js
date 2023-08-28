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
    new OpenCallback(NEW, "e1m", "e1d", function () {
      asyncOpenCacheEntry(
        URL + "/a",
        "disk",
        Ci.nsICacheStorage.OPEN_NORMALLY,
        null,
        new OpenCallback(NORMAL, "e1m", "e1d", function () {
          asyncOpenCacheEntry(
            URL2 + "/a",
            "disk",
            Ci.nsICacheStorage.OPEN_NORMALLY,
            null,
            new OpenCallback(NEW, "f1m", "f1d", function () {
              asyncOpenCacheEntry(
                URL2 + "/a",
                "disk",
                Ci.nsICacheStorage.OPEN_NORMALLY,
                null,
                new OpenCallback(NORMAL, "f1m", "f1d", function () {
                  var url = Services.io.newURI(URL);
                  var principal =
                    Services.scriptSecurityManager.createContentPrincipal(
                      url,
                      {}
                    );

                  Services.cache2.clearOrigin(principal);

                  asyncOpenCacheEntry(
                    URL + "/a",
                    "disk",
                    Ci.nsICacheStorage.OPEN_NORMALLY,
                    null,
                    new OpenCallback(NEW, "e1m", "e1d", function () {
                      asyncOpenCacheEntry(
                        URL2 + "/a",
                        "disk",
                        Ci.nsICacheStorage.OPEN_NORMALLY,
                        null,
                        new OpenCallback(NORMAL, "f1m", "f1d", function () {
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
