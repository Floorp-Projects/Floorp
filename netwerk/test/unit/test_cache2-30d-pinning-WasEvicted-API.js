/*

This test exercises the CacheFileContextEvictor::WasEvicted API and code using it.

- We store 10+10 (pinned and non-pinned) entries to the cache, wait for them being written.
- Then we purge the memory pools.
- Now the IO thread is suspended on the EVICT (7) level to prevent actual deletion of the files.
- Index is disabled.
- We do clear() of the cache, this creates the "ce_*" file and posts to the EVICT level
  the eviction loop mechanics.
- We open again those 10+10 entries previously stored.
- IO is resumed
- We expect to get all the pinned and
  loose all the non-pinned (common) entries.

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

  log_("first set of opens");
  var i;
  for (i = 0; i < kENTRYCOUNT; ++i) {

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

      log_("after purge");
      // Prevent the I/O thread from evicting physically the data.  We first want to re-open the entries.
      // This deterministically emulates a slow hard drive.
      testingInterface.suspendCacheIOThread(7);

      log_("clearing");
      // Now clear everything except pinned.  Stores the "ce_*" file and schedules background eviction.
      get_cache_service().clear();
      log_("cleared");

      log_("second set of opens");
      // Now open again.  Pinned entries should be there, disk entries should be the renewed entries.
      // Callbacks 21-40
      for (i = 0; i < kENTRYCOUNT; ++i) {
        mc.add();
        asyncOpenCacheEntry("http://pinned" + i + "/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, lci,
          new OpenCallback(NORMAL, "m" + i, "p" + i, function(entry) { mc.fired(); }));

        mc.add();
        asyncOpenCacheEntry("http://common" + i + "/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, lci,
          new OpenCallback(NEW, "m2" + i, "d2" + i, function(entry) { mc.fired(); }));
      }

      // Resume IO, this will just pop-off the CacheFileContextEvictor::EvictEntries() because of
      // an early check on CacheIOThread::YieldAndRerun() in that method.
      // CacheFileIOManager::OpenFileInternal should now run and CacheFileContextEvictor::WasEvicted
      // should be checked on.
      log_("resuming");
      testingInterface.resumeCacheIOThread();
      log_("resumed");

      mc.fired(); // Finishes this test
    }
  }, "cacheservice:purge-memory-pools");


  do_test_pending();
}
