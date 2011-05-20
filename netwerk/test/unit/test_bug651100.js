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

function get_ostream_for_entry(key, asFile, entryRef)
{
  var cache = get_cache_service();
  var session = cache.createSession(
                  "HTTP",
                  asFile ? Ci.nsICache.STORE_ON_DISK_AS_FILE
                         : Ci.nsICache.STORE_ON_DISK,
                  Ci.nsICache.STREAM_BASED);
  var cacheEntry = session.openCacheEntry(key, Ci.nsICache.ACCESS_WRITE, true);
  var oStream = cacheEntry.openOutputStream(0);
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

function write_big_datafile()
{
  var entry = {};
  var oStr = get_ostream_for_entry("bigdata", true, entry);
  var data = gen_1MiB();

  // >64MiB
  var i;
  for (i=0 ; i<65 ; i++)
    write_and_check(oStr, data, data.length);

  oStr.close();
  entry.value.close();
}

function write_and_doom_big_metafile()
{
  var entry = {};
  var oStr = get_ostream_for_entry("bigmetadata", true, entry);
  var data = gen_1MiB();

  // >64MiB
  var i;
  for (i=0 ; i<65 ; i++)
    entry.value.setMetaDataElement("metadata_"+i, data);

  write_and_check(oStr, data, data.length);

  oStr.close();
  entry.value.doom();
  entry.value.close();
}

function check_cache_size() {
  var diskDeviceVisited;
  var visitor = {
    visitDevice: function (deviceID, deviceInfo) {
      if (deviceID == "disk") {
        diskDeviceVisited = true;
        do_check_eq(deviceInfo.totalSize, 0)
      }
      return false;
    },
    visitEntry: function (deviceID, entryInfo) {
      do_throw("unexpected call to visitEntry");
    }
  };

  var cs = get_cache_service();
  diskDeviceVisited = false;
  cs.visitEntries(visitor);
  do_check_true(diskDeviceVisited);
}

var _continue_with = null;
function sync_with_cache_IO_thread(aFunc)
{
  do_check_eq(sync_with_cache_IO_thread_cb.listener, null);
  sync_with_cache_IO_thread_cb.listener = aFunc;
  var cache = get_cache_service();
  var session = cache.createSession(
                  "HTTP",
                  Ci.nsICache.STORE_ON_DISK,
                  Ci.nsICache.STREAM_BASED);
  var cacheEntry = session.asyncOpenCacheEntry(
                     "nonexistententry",
                     Ci.nsICache.ACCESS_READ,
                     sync_with_cache_IO_thread_cb);
}

var sync_with_cache_IO_thread_cb = {
  listener: null,

  onCacheEntryAvailable: function oCEA(descriptor, accessGranted, status) {
    do_check_eq(status, Cr.NS_ERROR_CACHE_KEY_NOT_FOUND);
    cb = this.listener;
    this.listener = null;
    do_timeout(0, cb);
  }
};


function run_test() {
  var prefBranch = Cc["@mozilla.org/preferences-service;1"].
                     getService(Ci.nsIPrefBranch);
  prefBranch.setIntPref("browser.cache.disk.capacity", 50000);

  do_get_profile();

  // clear the cache
  get_cache_service().evictEntries(Ci.nsICache.STORE_ANYWHERE);

  // write an entry with data > 64MiB
  write_big_datafile();

  // DoomEntry() is called when the cache is full, but the data is really
  // deleted (and the cache size updated) on the background thread when the
  // entry is deactivated. We need to sync with the cache IO thread before we
  // continue with the test.
  sync_with_cache_IO_thread(run_test_2);

  do_test_pending();
}

function run_test_2()
{
  check_cache_size();

  // write an entry with metadata > 64MiB
  write_and_doom_big_metafile();

  sync_with_cache_IO_thread(run_test_3);
}

function run_test_3()
{
  check_cache_size();

  do_test_finished();
}
