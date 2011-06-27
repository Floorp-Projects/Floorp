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

function make_input_stream_scriptable(input) {
  var wrapper = Cc["@mozilla.org/scriptableinputstream;1"].
                createInstance(Ci.nsIScriptableInputStream);
  wrapper.init(input);
  return wrapper;
}

function write_datafile()
{
  var entry = {};
  var oStr = get_ostream_for_entry("data", true, false, entry);
  var data = gen_1MiB();

  write_and_check(oStr, data, data.length);

  oStr.close();
  entry.value.close();
}

function test_read_after_doom()
{
  var entry = {};
  var oStr = get_ostream_for_entry("data", false, true, entry);
  var data = gen_1MiB();

  entry.value.doom();
  write_and_check(oStr, data, data.length);

  oStr.close();

  var iStr = make_input_stream_scriptable(entry.value.openInputStream(0));
  var read = iStr.read(iStr.available());
  do_check_eq(read.length, 2*1024*1024);
  iStr.close();

  entry.value.close();
}

function run_test() {
  do_get_profile();

  // clear the cache
  get_cache_service().evictEntries(Ci.nsICache.STORE_ANYWHERE);

  // force to write file bigger than 5MiB
  write_datafile();

  // open, doom, append, read
  test_read_after_doom();
}
