function dumpLeaks()
{
    var leakDetector = Components.classes["@mozilla.org/xpcom/leakdetector;1"].getService(Components.interfaces.nsILeakDetector);
    leakDetector.dumpLeaks();
}

function getCacheService()
{
    var nsCacheService = Components.classes["@mozilla.org/network/cache-service;1"];
    var service = nsCacheService.getService(Components.interfaces.nsICacheService);
    return service;
}

function createCacheSession(clientID, storagePolicy, streamable)
{
    var service = getCacheService();
    var nsICache = Components.interfaces.nsICache;
    var session = service.createSession(clientID, storagePolicy, streamable);
    return session;
}

function openCacheEntry(clientID, url)
{
    var nsICache = Components.interfaces.nsICache;
    var session = createCacheSession(clientID, nsICache.STORE_ANYWHERE, false);
    var entry = session.openCacheEntry(url, nsICache.ACCESS_READ_WRITE);
    return entry;
}

var entry = openCacheEntry("javascript", "theme:button");
entry.cacheElement = entry;
entry.markValid();

var newEntry = openCacheEntry("javascript", "theme:button");
if (newEntry.cacheElement === entry)
    print("object cache works.");
else
    print("object cache failed.");
