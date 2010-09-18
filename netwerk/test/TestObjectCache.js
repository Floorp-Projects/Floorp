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

function wrapString(str)
{
    var nsISupportsCString = Components.interfaces.nsISupportsCString;
    var factory = Components.classes["@mozilla.org/supports-cstring;1"];
    var wrapper = factory.createInstance(nsISupportsCString);
    wrapper.data = str;
    return wrapper;
}

function test()
{
    var data = wrapString("javascript");
    var entry = openCacheEntry("javascript", "theme:button");
    entry.cacheElement = data;
    entry.markValid();
    entry.close();

    var newEntry = openCacheEntry("javascript", "theme:button");
    if (newEntry.cacheElement === data)
        print("object cache works.");
    else
        print("object cache failed.");
}

test();
