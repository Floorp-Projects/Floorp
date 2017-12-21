function run_test()
{
  do_get_profile();
  function NowSeconds() {
    return parseInt((new Date()).getTime() / 1000);
  }
  function do_check_time(a, b) {
    Assert.ok(Math.abs(a - b) < 0.5);
  }

  asyncOpenCacheEntry("http://t/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
    new OpenCallback(NEW, "m", "d", function(entry) {

      var now1 = NowSeconds();
      Assert.equal(entry.fetchCount, 1);
      do_check_time(entry.lastFetched, now1);
      do_check_time(entry.lastModified, now1);

      do_timeout(2000, () => {
        asyncOpenCacheEntry("http://t/", "disk", Ci.nsICacheStorage.OPEN_SECRETLY, null,
          new OpenCallback(NORMAL, "m", "d", function(entry) {
            Assert.equal(entry.fetchCount, 1);
            do_check_time(entry.lastFetched, now1);
            do_check_time(entry.lastModified, now1);

            finish_cache2_test();
          })
        );
      })
    })
  );

  do_test_pending();
}
