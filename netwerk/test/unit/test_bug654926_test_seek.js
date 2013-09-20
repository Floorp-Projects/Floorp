function gen_1MiB()
{
  var i;
  var data="x";
  for (i=0 ; i < 20 ; i++)
    data+=data;
  return data;
}

function write_and_check(str, data, len)
{
  var written = str.write(data, len);
  if (written != len) {
    do_throw("str.write has not written all data!\n" +
             "  Expected: " + len  + "\n" +
             "  Actual: " + written + "\n");
  }
}

function write_datafile(status, entry)
{
  do_check_eq(status, Cr.NS_OK);
  var os = entry.openOutputStream(0);
  var data = gen_1MiB();

  write_and_check(os, data, data.length);

  os.close();
  entry.close();

  // try to open the entry for appending
  asyncOpenCacheEntry("http://data/",
                      "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
                      open_for_readwrite);
}

function open_for_readwrite(status, entry)
{
  do_check_eq(status, Cr.NS_OK);
  var os = entry.openOutputStream(entry.dataSize);

  // Opening the entry for appending data calls nsDiskCacheStreamIO::Seek()
  // which initializes mFD. If no data is written then mBufDirty is false and
  // mFD won't be closed in nsDiskCacheStreamIO::Flush().

  os.close();
  entry.close();

  do_test_finished();
}

function run_test() {
  do_get_profile();

  // clear the cache
  evict_cache_entries();

  asyncOpenCacheEntry("http://data/",
                      "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
                      write_datafile);

  do_test_pending();
}
