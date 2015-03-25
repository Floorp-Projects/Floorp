Components.utils.import('resource://gre/modules/LoadContextInfo.jsm');

function run_test()
{
  do_get_profile();

  if (!newCacheBackEndUsed()) {
    do_check_true(true, "This test checks only cache2 specific behavior.");
    return;
  }

  const kChunkSize = (256 * 1024);

  var payload = "";
  for (var i = 0; i < (kChunkSize + 10); ++i) {
    if (i < (kChunkSize - 5))
      payload += "0";
    else
      payload += String.fromCharCode(i + 65);
  }

  asyncOpenCacheEntry("http://read/", "disk", Ci.nsICacheStorage.OPEN_TRUNCATE, LoadContextInfo.default,
    new OpenCallback(NEW|WAITFORWRITE, "", payload, function(entry) {
      var is = entry.openInputStream(0);
      pumpReadStream(is, function(read) {
        do_check_eq(read.length, kChunkSize + 10);
        is.close();
        do_check_true(read == payload); // not using do_check_eq since logger will fail for the 1/4MB string
        finish_cache2_test();
      });
    })
  );

  do_test_pending();
}
