function run_test()
{
  do_get_profile();

  if (!newCacheBackEndUsed()) {
    do_check_true(true, "This test doesn't run when the old cache back end is used since the behavior is different");
    return;
  }

  var storage = getCacheStorage("disk");
  var entry = storage.openTruncate(createURI("http://new1/"), "");
  do_check_true(!!entry);

  // Fill the entry, and when done, check it's content
  (new OpenCallback(NEW, "meta", "data", function() {
    asyncOpenCacheEntry("http://new1/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
      new OpenCallback(NORMAL, "meta", "data", function() {
        finish_cache2_test();
      })
    );
  })).onCacheEntryAvailable(entry, true, null, 0);

  do_test_pending();
}
