Components.utils.import('resource://gre/modules/LoadContextInfo.jsm');

function gen_200k()
{
  var i;
  var data="0123456789ABCDEFGHIJLKMNO";
  for (i=0; i<13; i++)
    data+=data;
  return data;
}

// Keep the output stream of the first entry in a global variable, so the
// CacheFile and its buffer isn't released before we write the data to the
// second entry.
var oStr;

function run_test()
{
  do_get_profile();

  if (!newCacheBackEndUsed()) {
    do_check_true(true, "This test doesn't run when the old cache back end is used since the behavior is different");
    return;
  }

  var prefBranch = Cc["@mozilla.org/preferences-service;1"].
                     getService(Ci.nsIPrefBranch);

  // set max chunks memory so that only one full chunk fits within the limit
  prefBranch.setIntPref("browser.cache.disk.max_chunks_memory_usage", 300);

  asyncOpenCacheEntry("http://a/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
    function(status, entry) {
      do_check_eq(status, Cr.NS_OK);
      oStr = entry.openOutputStream(0);
      var data = gen_200k();
      do_check_eq(data.length, oStr.write(data, data.length));

      asyncOpenCacheEntry("http://b/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
        function(status, entry) {
          do_check_eq(status, Cr.NS_OK);
          var oStr2 = entry.openOutputStream(0);
          do_check_throws_nsIException(() => oStr2.write(data, data.length), 'NS_ERROR_OUT_OF_MEMORY');
          finish_cache2_test();
        }
      );
    }
  );

  do_test_pending();
}
