"use strict";

var { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

function evict_cache_entries(where) {
  var clearDisk = !where || where == "disk" || where == "all";
  var clearMem = !where || where == "memory" || where == "all";

  var storage;

  if (clearMem) {
    storage = Services.cache2.memoryCacheStorage(
      Services.loadContextInfo.default
    );
    storage.asyncEvictStorage(null);
  }

  if (clearDisk) {
    storage = Services.cache2.diskCacheStorage(
      Services.loadContextInfo.default
    );
    storage.asyncEvictStorage(null);
  }
}

function createURI(urispec) {
  return Services.io.newURI(urispec);
}

function getCacheStorage(where, lci) {
  if (!lci) {
    lci = Services.loadContextInfo.default;
  }
  switch (where) {
    case "disk":
      return Services.cache2.diskCacheStorage(lci);
    case "memory":
      return Services.cache2.memoryCacheStorage(lci);
    case "pin":
      return Services.cache2.pinningCacheStorage(lci);
  }
  return null;
}

function asyncOpenCacheEntry(key, where, flags, lci, callback) {
  key = createURI(key);

  function CacheListener() {}
  CacheListener.prototype = {
    QueryInterface: ChromeUtils.generateQI(["nsICacheEntryOpenCallback"]),

    onCacheEntryCheck(entry) {
      if (typeof callback === "object") {
        return callback.onCacheEntryCheck(entry);
      }
      return Ci.nsICacheEntryOpenCallback.ENTRY_WANTED;
    },

    onCacheEntryAvailable(entry, isnew, status) {
      if (typeof callback === "object") {
        // Root us at the callback
        callback.__cache_listener_root = this;
        callback.onCacheEntryAvailable(entry, isnew, status);
      } else {
        callback(status, entry);
      }
    },

    run() {
      var storage = getCacheStorage(where, lci);
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

function asyncCheckCacheEntryPresence(key, where, shouldExist, continuation) {
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
    }
  );
}
