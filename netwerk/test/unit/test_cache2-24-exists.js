Components.utils.import('resource://gre/modules/LoadContextInfo.jsm');

function run_test()
{
  do_get_profile();

  if (!newCacheBackEndUsed()) {
    do_check_true(true, "This test checks only cache2 specific behavior.");
    return;
  }

  var mc = new MultipleCallbacks(2, function() {
    var mem = getCacheStorage("memory");
    var disk = getCacheStorage("disk");

    do_check_true(disk.exists(createURI("http://m1/"), ""));
    do_check_true(mem.exists(createURI("http://m1/"), ""));
    do_check_false(mem.exists(createURI("http://m2/"), ""));
    do_check_true(disk.exists(createURI("http://d1/"), ""));
    do_check_throws_nsIException(() => disk.exists(createURI("http://d2/"), ""), 'NS_ERROR_NOT_AVAILABLE');

    finish_cache2_test();
  });

  asyncOpenCacheEntry("http://d1/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, LoadContextInfo.default,
    new OpenCallback(NEW | WAITFORWRITE, "meta", "data", function(entry) {
      mc.fired();
    })
  );

  asyncOpenCacheEntry("http://m1/", "memory", Ci.nsICacheStorage.OPEN_NORMALLY, LoadContextInfo.default,
    new OpenCallback(NEW | WAITFORWRITE, "meta", "data", function(entry) {
      mc.fired();
    })
  );

  do_test_pending();
}
