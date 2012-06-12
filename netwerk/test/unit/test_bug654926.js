const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

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

  // max size in MB
  var max_size = get_pref_service().
                 getIntPref("browser.cache.disk.max_entry_size") / 1024;

  // write larger entry than is allowed
  var i;
  for (i=0 ; i<(max_size+1) ; i++)
    write_and_check(os, data, data.length);

  os.close();
  entry.close();

  // append to entry
  asyncOpenCacheEntry("data",
                      "HTTP",
                      Ci.nsICache.STORE_ON_DISK,
                      Ci.nsICache.ACCESS_READ_WRITE,
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

  // force to write file bigger than 5MiB
  asyncOpenCacheEntry("data",
                      "HTTP",
                      Ci.nsICache.STORE_ON_DISK_AS_FILE,
                      Ci.nsICache.ACCESS_WRITE,
                      write_datafile);

  do_test_pending();
}
