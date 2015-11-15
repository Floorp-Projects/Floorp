/*

This is a complex test checking the internal "deferred doom" functionality in both CacheEntry and CacheFileHandle.

- We create a batch of 10 non-pinned and 10 pinned entries, write something to them.
- Then we purge them from memory, so they have to reload from disk.
- After that the IO thread is suspended not to process events on the READ (3) level.  This forces opening operation and eviction
  sync operations happen before we know actual pinning status of already cached entries.
- We async-open the same batch of the 10+10 entries again, all should open as existing with the expected, previously stored
  content
- After all these entries are made to open, we clear the cache.  This does some synchronous operations on the entries
  being open and also on the handles being in an already open state (but before the entry metadata has started to be read.)
  Expected is to leave the pinned entries only.
- Now, we resume the IO thread, so it start reading.  One could say this is a hack, but this can very well happen in reality
  on slow disk or when a large number of entries is about to be open at once.  Suspending the IO thread is just doing this
  simulation is a fully deterministic way and actually very easily and elegantly.
- After the resume we want to open all those 10+10 entries once again (no purgin involved this time.).  It is expected
  to open all the pinning entries intact and loose all the non-pinned entries (get them as new and empty again.)

*/

const kENTRYCOUNT = 10;

function log_(msg) { if (true) dump(">>>>>>>>>>>>> " + msg + "\n"); }

function run_test()
{
  do_get_profile();
  if (!newCacheBackEndUsed()) {
    do_check_true(true, "This test checks only cache2 specific behavior.");
    return;
  }

  var lci = LoadContextInfo.default;
  var testingInterface = get_cache_service().QueryInterface(Ci.nsICacheTesting);
  do_check_true(testingInterface);

  var mc = new MultipleCallbacks(1, function() {
    // (2)

    mc = new MultipleCallbacks(1, finish_cache2_test);
    // Release all references to cache entries so that they can be purged
    // Calling gc() four times is needed to force it to actually release
    // entries that are obviously unreferenced.  Yeah, I know, this is wacky...
    gc();
    gc();
    do_execute_soon(() => {
      gc();
      gc();
      log_("purging");

      // Invokes cacheservice:purge-memory-pools when done.
      get_cache_service().purgeFromMemory(Ci.nsICacheStorageService.PURGE_EVERYTHING); // goes to (3)
    });
  }, true);

  // (1), here we start

  var i;
  for (i = 0; i < kENTRYCOUNT; ++i) {
    log_("first set of opens");

    // Callbacks 1-20
    mc.add();
    asyncOpenCacheEntry("http://pinned" + i + "/", "pin", Ci.nsICacheStorage.OPEN_TRUNCATE, lci,
      new OpenCallback(NEW|WAITFORWRITE, "m" + i, "p" + i, function(entry) { mc.fired(); }));

    mc.add();
    asyncOpenCacheEntry("http://common" + i + "/", "disk", Ci.nsICacheStorage.OPEN_TRUNCATE, lci,
      new OpenCallback(NEW|WAITFORWRITE, "m" + i, "d" + i, function(entry) { mc.fired(); }));
  }

  mc.fired(); // Goes to (2)

  var os = Cc["@mozilla.org/observer-service;1"].getService(Ci.nsIObserverService);
  os.addObserver({
    observe: function(subject, topic, data)
    {
      // (3)

      log_("after purge, second set of opens");
      // Prevent the I/O thread from reading the data.  We first want to schedule clear of the cache.
      // This deterministically emulates a slow hard drive.
      testingInterface.suspendCacheIOThread(3);

      // All entries should load
      // Callbacks 21-40
      for (i = 0; i < kENTRYCOUNT; ++i) {
        mc.add();
        asyncOpenCacheEntry("http://pinned" + i + "/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, lci,
          new OpenCallback(NORMAL, "m" + i, "p" + i, function(entry) { mc.fired(); }));

        // Unfortunately we cannot ensure that entries existing in the cache will be delivered to the consumer
        // when soon after are evicted by some cache API call.  It's better to not ensure getting an entry
        // than allowing to get an entry that was just evicted from the cache.  Entries may be delievered
        // as new, but are already doomed.  Output stream cannot be openned, or the file handle is already
        // writing to a doomed file.
        //
        // The API now just ensures that entries removed by any of the cache eviction APIs are never more
        // available to consumers.
        mc.add();
        asyncOpenCacheEntry("http://common" + i + "/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, lci,
          new OpenCallback(MAYBE_NEW|DOOMED, "m" + i, "d" + i, function(entry) { mc.fired(); }));
      }

      log_("clearing");
      // Now clear everything except pinned, all entries are in state of reading
      get_cache_service().clear();
      log_("cleared");

      // Resume reading the cache data, only now the pinning status on entries will be discovered,
      // the deferred dooming code will trigger.
      testingInterface.resumeCacheIOThread();

      log_("third set of opens");
      // Now open again.  Pinned entries should be there, disk entries should be the renewed entries.
      // Callbacks 41-60
      for (i = 0; i < kENTRYCOUNT; ++i) {
        mc.add();
        asyncOpenCacheEntry("http://pinned" + i + "/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, lci,
          new OpenCallback(NORMAL, "m" + i, "p" + i, function(entry) { mc.fired(); }));

        mc.add();
        asyncOpenCacheEntry("http://common" + i + "/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, lci,
          new OpenCallback(NEW, "m2" + i, "d2" + i, function(entry) { mc.fired(); }));
      }

      mc.fired(); // Finishes this test
    }
  }, "cacheservice:purge-memory-pools", false);


  do_test_pending();
}
