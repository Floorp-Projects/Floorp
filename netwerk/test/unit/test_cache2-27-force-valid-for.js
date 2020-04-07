"use strict";

function run_test() {
  do_get_profile();

  var mc = new MultipleCallbacks(2, function() {
    finish_cache2_test();
  });

  asyncOpenCacheEntry(
    "http://m1/",
    "memory",
    Ci.nsICacheStorage.OPEN_NORMALLY,
    Services.loadContextInfo.default,
    new OpenCallback(NEW, "meta", "data", function(entry) {
      // Check the default
      equal(entry.isForcedValid, false);

      // Forced valid and confirm
      entry.forceValidFor(2);
      do_timeout(1000, function() {
        equal(entry.isForcedValid, true);
        mc.fired();
      });

      // Confirm the timeout occurs
      do_timeout(3000, function() {
        equal(entry.isForcedValid, false);
        mc.fired();
      });
    })
  );

  do_test_pending();
}
