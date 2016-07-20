var _PSvc;
function get_pref_service() {
  if (_PSvc)
    return _PSvc;

  return _PSvc = Cc["@mozilla.org/preferences-service;1"].
                 getService(Ci.nsIPrefBranch);
}

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

  // write 2MiB
  var i;
  for (i=0 ; i<2 ; i++)
    write_and_check(os, data, data.length);

  os.close();
  entry.close();

  // now change max_entry_size so that the existing entry is too big
  get_pref_service().setIntPref("browser.cache.disk.max_entry_size", 1024);

  // append to entry
  asyncOpenCacheEntry("http://data/",
                      "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
                      append_datafile);
}

function append_datafile(status, entry)
{
  do_check_eq(status, Cr.NS_OK);
  var os = entry.openOutputStream(entry.dataSize);
  var data = gen_1MiB();

  // append 1MiB
  try {
    write_and_check(os, data, data.length);
    do_throw();
  }
  catch (ex) { }

  // closing the ostream should fail in this case
  try {
    os.close();
    do_throw();
  }
  catch (ex) { }

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
