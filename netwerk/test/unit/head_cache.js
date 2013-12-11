Components.utils.import('resource://gre/modules/XPCOMUtils.jsm');
Components.utils.import('resource://gre/modules/LoadContextInfo.jsm');

var _CSvc;
function get_cache_service() {
  if (_CSvc)
    return _CSvc;

  return _CSvc = Components.classes["@mozilla.org/netwerk/cache-storage-service;1"]
                           .getService(Components.interfaces.nsICacheStorageService);
}

function evict_cache_entries(where)
{
  var clearDisk = (!where || where == "disk" || where == "all");
  var clearMem = (!where || where == "memory" || where == "all");
  var clearAppCache = (where == "appcache");

  var svc = get_cache_service();
  var storage;

  if (clearMem) {
    storage = svc.memoryCacheStorage(LoadContextInfo.default);
    storage.asyncEvictStorage(null);
  }

  if (clearDisk) {
    storage = svc.diskCacheStorage(LoadContextInfo.default, false);
    storage.asyncEvictStorage(null);
  }

  if (clearAppCache) {
    storage = svc.appCacheStorage(LoadContextInfo.default, null);
    storage.asyncEvictStorage(null);
  }
}

function createURI(urispec)
{
  var ioServ = Components.classes["@mozilla.org/network/io-service;1"]
                         .getService(Components.interfaces.nsIIOService);
  return ioServ.newURI(urispec, null, null);
}

function getCacheStorage(where, lci, appcache)
{
  if (!lci) lci = LoadContextInfo.default;
  var svc = get_cache_service();
  switch (where) {
    case "disk": return svc.diskCacheStorage(lci, false);
    case "memory": return svc.memoryCacheStorage(lci);
    case "appcache": return svc.appCacheStorage(lci, appcache);
  }
  return null;
}

function asyncOpenCacheEntry(key, where, flags, lci, callback, appcache)
{
  key = createURI(key);

  function CacheListener() { }
  CacheListener.prototype = {
    _appCache: appcache,

    QueryInterface: function (iid) {
      if (iid.equals(Components.interfaces.nsICacheEntryOpenCallback) ||
          iid.equals(Components.interfaces.nsISupports))
        return this;
      throw Components.results.NS_ERROR_NO_INTERFACE;
    },

    onCacheEntryCheck: function(entry, appCache) {
      if (typeof callback === "object")
        return callback.onCacheEntryCheck(entry, appCache);
      return Ci.nsICacheEntryOpenCallback.ENTRY_WANTED;
    },

    onCacheEntryAvailable: function (entry, isnew, appCache, status) {
      if (typeof callback === "object") {
        // Root us at the callback
        callback.__cache_listener_root = this;
        callback.onCacheEntryAvailable(entry, isnew, appCache, status);
      }
      else
        callback(status, entry, appCache);
    },

    run: function () {
      var storage = getCacheStorage(where, lci, this._appCache);
      storage.asyncOpenURI(key, "", flags, this);
    }
  };

  (new CacheListener()).run();
}

function syncWithCacheIOThread(callback)
{
  if (!newCacheBackEndUsed()) {
    asyncOpenCacheEntry(
      "http://nonexistententry/", "disk", Ci.nsICacheStorage.OPEN_READONLY, null,
      function(status, entry) {
        do_check_eq(status, Components.results.NS_ERROR_CACHE_KEY_NOT_FOUND);
        callback();
      });
  }
  else {
    callback();
  }
}

function get_device_entry_count(where, lci, continuation) {
  var storage = getCacheStorage(where, lci);
  if (!storage) {
    continuation(-1, 0);
    return;
  }

  var visitor = {
    onCacheStorageInfo: function (entryCount, consumption) {
      do_execute_soon(function() {
        continuation(entryCount, consumption);
      });
    },
  };

  // get the device entry count
  storage.asyncVisitStorage(visitor, false);
}

function asyncCheckCacheEntryPresence(key, where, shouldExist, continuation, appCache)
{
  asyncOpenCacheEntry(key, where, Ci.nsICacheStorage.OPEN_READONLY, null,
    function(status, entry) {
      if (shouldExist) {
        dump("TEST-INFO | checking cache key " + key + " exists @ " + where);
        do_check_eq(status, Cr.NS_OK);
        do_check_true(!!entry);
      } else {
        dump("TEST-INFO | checking cache key " + key + " doesn't exist @ " + where);
        do_check_eq(status, Cr.NS_ERROR_CACHE_KEY_NOT_FOUND);
        do_check_null(entry);
      }
      continuation();
    }, appCache);
}
