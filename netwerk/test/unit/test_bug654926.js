const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

var _CSvc;
function get_cache_service() {
  if (_CSvc)
    return _CSvc;

  return _CSvc = Cc["@mozilla.org/network/cache-service;1"].
                 getService(Ci.nsICacheService);
}

function get_ostream_for_entry(key, asFile, append, entryRef)
{
  var cache = get_cache_service();
  var session = cache.createSession(
                  "HTTP",
                  asFile ? Ci.nsICache.STORE_ON_DISK_AS_FILE
                         : Ci.nsICache.STORE_ON_DISK,
                  Ci.nsICache.STREAM_BASED);
  var cacheEntry = session.openCacheEntry(
                     key,
                     append ? Ci.nsICache.ACCESS_READ_WRITE
                            : Ci.nsICache.ACCESS_WRITE,
                     true);
  var oStream = cacheEntry.openOutputStream(append ? cacheEntry.dataSize : 0);
  entryRef.value = cacheEntry;
  return oStream;
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

function write_datafile()
{
  var entry = {};
  var oStr = get_ostream_for_entry("data", true, false, entry);
  var data = gen_1MiB();

  // 6MiB
  var i;
  for (i=0 ; i<6 ; i++)
    write_and_check(oStr, data, data.length);

  oStr.close();
  entry.value.close();
}

function append_datafile()
{
  var entry = {};
  var oStr = get_ostream_for_entry("data", false, true, entry);
  var data = gen_1MiB();


  // append 1MiB
  try {
    write_and_check(oStr, data, data.length);
    do_throw();
  }
  catch (ex) { }

  // closing the ostream should fail in this case
  try {
    oStr.close();
    do_throw();
  }
  catch (ex) { }

  entry.value.close();
}

function run_test() {
  do_get_profile();

  // clear the cache
  get_cache_service().evictEntries(Ci.nsICache.STORE_ANYWHERE);

  // force to write file bigger than 5MiB
  write_datafile();

  // try to append the entry
  append_datafile();
}
