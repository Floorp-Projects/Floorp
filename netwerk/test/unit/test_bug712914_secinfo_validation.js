var _ios;

var ACCESS_WRITE = Ci.nsICache.ACCESS_WRITE;
var ACCESS_READ = Ci.nsICache.ACCESS_READ;

var KEY_CORRUPT_SECINFO = "http://corruptSecurityInfo/";
var ENTRY_DATA = "foobar";

function create_scriptable_input(unscriptable_input) {
  var istream = Cc["@mozilla.org/scriptableinputstream;1"].
                createInstance(Ci.nsIScriptableInputStream);
  istream.init(unscriptable_input);
  return istream;
}

function write_data(entry) {
  var ostream = entry.openOutputStream(0);
  if (ostream.write(ENTRY_DATA, ENTRY_DATA.length) != ENTRY_DATA.length) {
    do_throw("Could not write all data!");
  }
  ostream.close();
}

function continue_failure(status, entry) {
  // Make sure we couldn't open this for reading
  do_check_eq(status, Cr.NS_ERROR_CACHE_KEY_NOT_FOUND);

  // Make sure the cache is empty
  get_device_entry_count("disk", null, function(count, consumption) {
    do_check_eq(count, 0);
    do_check_eq(consumption, 0);
    run_next_test();
  });
}

function try_read_corrupt_secinfo() {
  asyncOpenCacheEntry(KEY_CORRUPT_SECINFO,
                      "disk", Ci.nsICacheStorage.OPEN_READONLY, null,
                      continue_failure);
}

function write_corrupt_secinfo(status, entry) {
  entry.setMetaDataElement("security-info", "blablabla");
  write_data(entry);
  try {
    entry.close();
  } catch (e) {
    do_throw("Unexpected exception closing corrupt entry: " + e);
  }

  try_read_corrupt_secinfo();
}

function test_corrupt_secinfo() {
  asyncOpenCacheEntry(KEY_CORRUPT_SECINFO,
                      "disk", Ci.nsICacheStorage.OPEN_TRUNCATE, null,
                      write_corrupt_secinfo);
}

function run_test() {
  if (newCacheBackEndUsed()) {
    // broken sec info should doom a cache entry (when broken sec info is written, load should fail with NOT_FOUND)
    do_check_true(true, "This test doesn't run with the new cache backend, the test or the cache needs to be fixed");
    return;
  }

  // Make sure we have a cache to use
  do_get_profile();

  // Make sure the cache is empty
  evict_cache_entries();

  // Add new tests at the end of this section
  add_test(test_corrupt_secinfo);

  // Let's get going!
  run_next_test();
}
