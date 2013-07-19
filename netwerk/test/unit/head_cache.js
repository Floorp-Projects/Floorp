Components.utils.import('resource://gre/modules/XPCOMUtils.jsm');

var _CSvc;
function get_cache_service() {
  if (_CSvc)
    return _CSvc;

  return _CSvc = Components.classes["@mozilla.org/network/cache-service;1"]
                           .getService(Components.interfaces.nsICacheService);
}

function evict_cache_entries(where)
{
  if (where == null)
    where = Components.interfaces.nsICache.STORE_ANYWHERE;

  get_cache_service().evictEntries(where);
}

function asyncOpenCacheEntry(key, sessionName, storagePolicy, access, callback)
{
  function CacheListener() { }
  CacheListener.prototype = {
    QueryInterface: function (iid) {
      if (iid.equals(Components.interfaces.nsICacheListener) ||
          iid.equals(Components.interfaces.nsISupports))
        return this;
      throw Components.results.NS_ERROR_NO_INTERFACE;
    },

    onCacheEntryAvailable: function (entry, access, status) {
      callback(status, entry);
    },

    run: function () {
      var cache = get_cache_service();
      var session = cache.createSession(
                      sessionName,
                      storagePolicy,
                      Components.interfaces.nsICache.STREAM_BASED);
      session.asyncOpenCacheEntry(key, access, this);
    }
  };

  (new CacheListener()).run();
}

function syncWithCacheIOThread(callback)
{
  asyncOpenCacheEntry(
    "nonexistententry",
    "HTTP",
    Components.interfaces.nsICache.STORE_ANYWHERE,
    Components.interfaces.nsICache.ACCESS_READ,
    function(status, entry) {
      do_check_eq(status, Components.results.NS_ERROR_CACHE_KEY_NOT_FOUND);
      callback();
    });
}

function get_device_entry_count(device) {
  var cs = get_cache_service();
  var entry_count = -1;

  var visitor = {
    visitDevice: function (deviceID, deviceInfo) {
      if (device == deviceID)
        entry_count = deviceInfo.entryCount;
      return false;
    },
    visitEntry: function (deviceID, entryInfo) {
      do_throw("nsICacheVisitor.visitEntry should not be called " +
        "when checking the availability of devices");
    }
  };

  // get the device entry count
  cs.visitEntries(visitor);

  return entry_count;
}

function asyncCheckCacheEntryPresence(key, sessionName, storagePolicy, shouldExist, doomOnExpire, continuation)
{
  var listener =
  {
    QueryInterface : function(iid)
    {
      if (iid.equals(Components.interfaces.nsICacheListener))
        return this;
      throw Components.results.NS_NOINTERFACE;
    },
    onCacheEntryAvailable : function(descriptor, accessGranted, status)
    {
      if (shouldExist) {
        dump("TEST-INFO | checking cache key " + key + " exists @ " + sessionName);
        do_check_eq(status, Cr.NS_OK);
        do_check_true(!!descriptor);
      } else {
        dump("TEST-INFO | checking cache key " + key + " doesn't exist @ " + sessionName);
        do_check_eq(status, Cr.NS_ERROR_CACHE_KEY_NOT_FOUND);
        do_check_null(descriptor);
      }
      continuation();
    }
  };

  var service = Cc["@mozilla.org/network/cache-service;1"].getService(Ci.nsICacheService);
  var session = service.createSession(sessionName,
                                      storagePolicy,
                                      true);
  session.doomEntriesIfExpired = doomOnExpire;
  session.asyncOpenCacheEntry(key, Ci.nsICache.ACCESS_READ, listener);
}
