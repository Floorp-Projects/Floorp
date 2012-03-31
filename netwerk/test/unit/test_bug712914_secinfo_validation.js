var Ci = Components.interfaces;
var Cc = Components.classes;
var Cr = Components.results;

var _cacheSvc;
var _ios;

var ACCESS_WRITE = Ci.nsICache.ACCESS_WRITE;
var ACCESS_READ = Ci.nsICache.ACCESS_READ;

var KEY_CORRUPT_SECINFO = "corruptSecurityInfo";
var ENTRY_DATA = "foobar";

function create_scriptable_input(unscriptable_input) {
  var istream = Cc["@mozilla.org/scriptableinputstream;1"].
                createInstance(Ci.nsIScriptableInputStream);
  istream.init(unscriptable_input);
  return istream;
}

function CacheListener(cb) {
  this._cb = cb;
}

CacheListener.prototype = {
  _cb: null,

  QueryInterface: function (iid) {
    if (iid.equals(Ci.nsISupports) ||
        iid.equals(Ci.nsICacheListener))
      return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
  },

  onCacheEntryAvailable: function (descriptor, accessGranted, status) {
    this._cb(descriptor, accessGranted, status);
  },

  onCacheEntryDoomed: function (status) {
    // Nothing to do here
  }
};

function CacheVisitor() {
}

CacheVisitor.prototype = {
  QueryInterface: function (iid) {
    if (iid.equals(Ci.nsISupports) ||
        iid.equals(Ci.nsICacheVisitor))
      return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
  },

  visitDevice: function (deviceID, deviceInfo) {
    if (deviceID == "disk") {
      // We only care about the disk device, since that's the only place we
      // actually serialize security info
      do_check_eq(deviceInfo.entryCount, 0);
      do_check_eq(deviceInfo.totalSize, 0);
    }

    // We don't care about visiting entries since we get all the info we need
    // from the device itself
    return false;
  },

  visitEntry: function (deviceID, entryInfo) {
    // Just in case we somehow got here, just skip on
    return false;
  }
};

function get_cache_service() {
  if (!_cacheSvc) {
    _cacheSvc = Cc["@mozilla.org/network/cache-service;1"].
                getService(Ci.nsICacheService);
  }

  return _cacheSvc
}

function get_io_service() {
  if (!_ios) {
    _ios = Cc["@mozilla.org/network/io-service;1"].
           getService(Ci.nsIIOService);
  }

  return _ios;
}

function open_entry(key, access, cb) {
  var cache = get_cache_service();
  var session = cache.createSession("HTTP", Ci.nsICache.STORE_ON_DISK,
                                    Ci.nsICache.STREAM_BASED);
  var listener = new CacheListener(cb);
  session.asyncOpenCacheEntry(key, access, listener);
}

function write_data(entry) {
  var ostream = entry.openOutputStream(0);
  if (ostream.write(ENTRY_DATA, ENTRY_DATA.length) != ENTRY_DATA.length) {
    do_throw("Could not write all data!");
  }
  ostream.close();
}

function continue_failure(descriptor, accessGranted, status) {
  // Make sure we couldn't open this for reading
  do_check_eq(status, Cr.NS_ERROR_CACHE_KEY_NOT_FOUND);

  // Make sure the cache is empty
  var cache = get_cache_service();
  var visitor = new CacheVisitor();
  cache.visitEntries(visitor);
  run_next_test();
}

function try_read_corrupt_secinfo() {
  open_entry(KEY_CORRUPT_SECINFO, ACCESS_READ, continue_failure);
}

function write_corrupt_secinfo(entry, accessGranted, status) {
  write_data(entry);
  entry.setMetaDataElement("security-info", "blablabla");
  try {
    entry.close();
  } catch (e) {
    do_throw("Unexpected exception closing corrupt entry: " + e);
  }

  try_read_corrupt_secinfo();
}

function test_corrupt_secinfo() {
  open_entry(KEY_CORRUPT_SECINFO, ACCESS_WRITE, write_corrupt_secinfo);
}

function run_test() {
  // Make sure we have a cache to use
  do_get_profile();

  // Make sure the cache is empty
  var cache = get_cache_service();
  cache.evictEntries(Ci.nsICache.STORE_ANYWHERE);

  // Add new tests at the end of this section
  add_test(test_corrupt_secinfo);

  // Let's get going!
  run_next_test();
}
