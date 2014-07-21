Components.utils.import('resource://gre/modules/LoadContextInfo.jsm');

function run_test()
{
  do_get_profile();

  if (!newCacheBackEndUsed()) {
    do_check_true(true, "This test checks only cache2 specific behavior.");
    return;
  }

  var mc = new MultipleCallbacks(2, function() {
    finish_cache2_test();
  });

  asyncOpenCacheEntry("http://m1/", "memory", Ci.nsICacheStorage.OPEN_NORMALLY, LoadContextInfo.default,
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
