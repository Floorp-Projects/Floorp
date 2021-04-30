"use strict";

function run_test() {
  do_get_profile();

  var storage = getCacheStorage("disk");
  var entry = storage.openTruncate(createURI("http://new1/"), "");
  Assert.ok(!!entry);

  // Fill the entry, and when done, check it's content
  new OpenCallback(NEW, "meta", "data", function() {
    asyncOpenCacheEntry(
      "http://new1/",
      "disk",
      Ci.nsICacheStorage.OPEN_NORMALLY,
      null,
      new OpenCallback(NORMAL, "meta", "data", function() {
        finish_cache2_test();
      })
    );
  }).onCacheEntryAvailable(entry, true, 0);

  do_test_pending();
}
