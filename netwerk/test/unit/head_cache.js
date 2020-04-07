"use strict";

var { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

var _CSvc;
function get_cache_service() {
  if (_CSvc) {
    return _CSvc;
  }

  return (_CSvc = Cc["@mozilla.org/netwerk/cache-storage-service;1"].getService(
    Ci.nsICacheStorageService
  ));
}

function evict_cache_entries(where) {
  var clearDisk = !where || where == "disk" || where == "all";
  var clearMem = !where || where == "memory" || where == "all";
  var clearAppCache = where == "appcache";

  var svc = get_cache_service();
  var storage;

  if (clearMem) {
    storage = svc.memoryCacheStorage(Services.loadContextInfo.default);
    storage.asyncEvictStorage(null);
  }

  if (clearDisk) {
    storage = svc.diskCacheStorage(Services.loadContextInfo.default, false);
    storage.asyncEvictStorage(null);
  }

  if (clearAppCache) {
    storage = svc.appCacheStorage(Services.loadContextInfo.default, null);
    storage.asyncEvictStorage(null);
  }
}

function createURI(urispec) {
  var ioServ = Cc["@mozilla.org/network/io-service;1"].getService(
    Ci.nsIIOService
  );
  return ioServ.newURI(urispec);
}

function getCacheStorage(where, lci, appcache) {
  if (!lci) {
    lci = Services.loadContextInfo.default;
  }
  var svc = get_cache_service();
  switch (where) {
    case "disk":
      return svc.diskCacheStorage(lci, false);
    case "memory":
      return svc.memoryCacheStorage(lci);
    case "appcache":
      return svc.appCacheStorage(lci, appcache);
    case "pin":
      return svc.pinningCacheStorage(lci);
  }
  return null;
}

function asyncOpenCacheEntry(key, where, flags, lci, callback, appcache) {
  key = createURI(key);

  function CacheListener() {}
  CacheListener.prototype = {
    _appCache: appcache,

    QueryInterface: ChromeUtils.generateQI(["nsICacheEntryOpenCallback"]),

    onCacheEntryCheck(entry, appCache) {
      if (typeof callback === "object") {
        return callback.onCacheEntryCheck(entry, appCache);
      }
      return Ci.nsICacheEntryOpenCallback.ENTRY_WANTED;
    },

    onCacheEntryAvailable(entry, isnew, appCache, status) {
      if (typeof callback === "object") {
        // Root us at the callback
        callback.__cache_listener_root = this;
        callback.onCacheEntryAvailable(entry, isnew, appCache, status);
      } else {
        callback(status, entry, appCache);
      }
    },

    run() {
      var storage = getCacheStorage(where, lci, this._appCache);
      storage.asyncOpenURI(key, "", flags, this);
    },
  };

  new CacheListener().run();
}

function syncWithCacheIOThread(callback, force) {
  if (force) {
    asyncOpenCacheEntry(
      "http://nonexistententry/",
      "disk",
      Ci.nsICacheStorage.OPEN_READONLY,
      null,
      function(status, entry) {
        Assert.equal(status, Cr.NS_ERROR_CACHE_KEY_NOT_FOUND);
        callback();
      }
    );
  } else {
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
    onCacheStorageInfo(entryCount, consumption) {
      executeSoon(function() {
        continuation(entryCount, consumption);
      });
    },
  };

  // get the device entry count
  storage.asyncVisitStorage(visitor, false);
}

function asyncCheckCacheEntryPresence(
  key,
  where,
  shouldExist,
  continuation,
  appCache
) {
  asyncOpenCacheEntry(
    key,
    where,
    Ci.nsICacheStorage.OPEN_READONLY,
    null,
    function(status, entry) {
      if (shouldExist) {
        dump("TEST-INFO | checking cache key " + key + " exists @ " + where);
        Assert.equal(status, Cr.NS_OK);
        Assert.ok(!!entry);
      } else {
        dump(
          "TEST-INFO | checking cache key " + key + " doesn't exist @ " + where
        );
        Assert.equal(status, Cr.NS_ERROR_CACHE_KEY_NOT_FOUND);
        Assert.equal(null, entry);
      }
      continuation();
    },
    appCache
  );
}
