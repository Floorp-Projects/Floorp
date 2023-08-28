"use strict";

function gen_200k() {
  var i;
  var data = "0123456789ABCDEFGHIJLKMNO";
  for (i = 0; i < 13; i++) {
    data += data;
  }
  return data;
}

// Keep the output stream of the first entry in a global variable, so the
// CacheFile and its buffer isn't released before we write the data to the
// second entry.
var oStr;

function run_test() {
  do_get_profile();

  // set max chunks memory so that only one full chunk fits within the limit
  Services.prefs.setIntPref("browser.cache.disk.max_chunks_memory_usage", 300);

  asyncOpenCacheEntry(
    "http://a/",
    "disk",
    Ci.nsICacheStorage.OPEN_NORMALLY,
    null,
    function (status, entry) {
      Assert.equal(status, Cr.NS_OK);
      var data = gen_200k();
      oStr = entry.openOutputStream(0, data.length);
      Assert.equal(data.length, oStr.write(data, data.length));

      asyncOpenCacheEntry(
        "http://b/",
        "disk",
        Ci.nsICacheStorage.OPEN_NORMALLY,
        null,
        function (status1, entry1) {
          Assert.equal(status1, Cr.NS_OK);
          var oStr2 = entry1.openOutputStream(0, data.length);
          do_check_throws_nsIException(
            () => oStr2.write(data, data.length),
            "NS_ERROR_OUT_OF_MEMORY"
          );
          finish_cache2_test();
        }
      );
    }
  );

  do_test_pending();
}
