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
      var cacheEntry = session.asyncOpenCacheEntry(key, access, this);
    }
  };

  (new CacheListener()).run();
}


