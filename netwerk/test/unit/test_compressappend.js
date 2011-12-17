//
// Test that data can be appended to a cache entry even when the data is 
// compressed by the cache compression feature - bug 648429.
//
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

function get_ostream_for_entry(key, append, compress, entryRef)
{
  var cache = get_cache_service();
  var session = cache.createSession(
                  "HTTP",
                  Ci.nsICache.STORE_ON_DISK,
                  Ci.nsICache.STREAM_BASED);
  var cacheEntry = session.openCacheEntry(
                     key,
                     append ? Ci.nsICache.ACCESS_READ_WRITE
                            : Ci.nsICache.ACCESS_WRITE,
                     true);

  if (compress)
    if (!append)
      cacheEntry.setMetaDataElement("uncompressed-len", "0");

  var oStream = cacheEntry.openOutputStream(append ? cacheEntry.storageDataSize : 0);
  entryRef.value = cacheEntry;
  return oStream;
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

function check_datafile()
{
  var cache = get_cache_service();
  var session = cache.createSession(
                  "HTTP",
                  Ci.nsICache.STORE_ON_DISK,
                  Ci.nsICache.STREAM_BASED);
  var entry = session.openCacheEntry(
                "data",
                Ci.nsICache.ACCESS_READ,
                true);

  var wrapper = Cc["@mozilla.org/scriptableinputstream;1"].
                createInstance(Ci.nsIScriptableInputStream);

  wrapper.init(entry.openInputStream(0));

  var str = wrapper.read(wrapper.available());
  do_check_eq(str.length, 10);
  do_check_eq(str, "12345abcde");

  wrapper.close();
  entry.close();
}


function write_datafile(compress)
{
  var entry = {};
  var oStr = get_ostream_for_entry("data", false, compress, entry);
  write_and_check(oStr, "12345", 5);
  oStr.close();
  entry.value.close();
}

function append_datafile()
{
  var entry = {};
  var oStr = get_ostream_for_entry("data", true, false, entry);
  write_and_check(oStr, "abcde", 5);
  oStr.close();
  entry.value.close();
}

function test_append(compress)
{
  get_cache_service().evictEntries(Ci.nsICache.STORE_ANYWHERE);
  write_datafile(compress);
  append_datafile();
  check_datafile();
}

function run_test() {
  do_get_profile();
  test_append(false);
  test_append(true);
}
