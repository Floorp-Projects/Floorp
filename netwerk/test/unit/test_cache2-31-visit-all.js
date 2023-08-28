"use strict";

function run_test() {
  getCacheStorage("disk");
  var lcis = [
    Services.loadContextInfo.default,
    Services.loadContextInfo.custom(false, { userContextId: 1 }),
    Services.loadContextInfo.custom(false, { userContextId: 2 }),
    Services.loadContextInfo.custom(false, { userContextId: 3 }),
  ];

  do_get_profile();

  var mc = new MultipleCallbacks(
    8,
    function () {
      executeSoon(function () {
        var expectedConsumption = 8192;
        var entries = [
          { uri: "http://a/", lci: lcis[0] }, // default
          { uri: "http://b/", lci: lcis[0] }, // default
          { uri: "http://a/", lci: lcis[1] }, // user Context 1
          { uri: "http://b/", lci: lcis[1] }, // user Context 1
          { uri: "http://a/", lci: lcis[2] }, // user Context 2
          { uri: "http://b/", lci: lcis[2] }, // user Context 2
          { uri: "http://a/", lci: lcis[3] }, // user Context 3
          { uri: "http://b/", lci: lcis[3] },
        ]; // user Context 3

        Services.cache2.asyncVisitAllStorages(
          // Test should store 8 entries across 4 originAttributes
          new VisitCallback(8, expectedConsumption, entries, function () {
            Services.cache2.asyncVisitAllStorages(
              // Still 8 entries expected, now don't walk them
              new VisitCallback(8, expectedConsumption, null, function () {
                finish_cache2_test();
              }),
              false
            );
          }),
          true
        );
      });
    },
    true
  );

  // Add two cache entries for each originAttributes.
  for (var i = 0; i < lcis.length; i++) {
    asyncOpenCacheEntry(
      "http://a/",
      "disk",
      Ci.nsICacheStorage.OPEN_NORMALLY,
      lcis[i],
      new OpenCallback(NEW, "a1m", "a1d", function () {
        asyncOpenCacheEntry(
          "http://a/",
          "disk",
          Ci.nsICacheStorage.OPEN_NORMALLY,
          lcis[i],
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
      lcis[i],
      new OpenCallback(NEW, "b1m", "b1d", function () {
        asyncOpenCacheEntry(
          "http://b/",
          "disk",
          Ci.nsICacheStorage.OPEN_NORMALLY,
          lcis[i],
          new OpenCallback(NORMAL, "b1m", "b1d", function () {
            mc.fired();
          })
        );
      })
    );
  }

  do_test_pending();
}
