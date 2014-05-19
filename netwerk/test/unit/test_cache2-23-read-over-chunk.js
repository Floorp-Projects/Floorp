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

  asyncOpenCacheEntry("http://read/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, LoadContextInfo.default,
    new OpenCallback(NEW|WAITFORWRITE, "", payload, function(entry) {
      var is = entry.openInputStream(0);
      do_check_eq(is.available(), kChunkSize + 10);
      var payloadCheck = read_stream(is, kChunkSize + 10);
      do_check_true(payloadCheck == payload); // not using do_check_eq since logger will fail for the 1/4MB string
      finish_cache2_test();
    })
  );

  do_test_pending();
}
