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

function openCacheEntry()
{
    var nsICache = Components.interfaces.nsICache;
    var session = createCacheSession("javascript", nsICache.STORE_ON_DISK, true);
    var entry = session.openCacheEntry("theme:button", nsICache.ACCESS_READ_WRITE);
    return entry;
}

function dumpLeaks()
{
    var leakDetector = Components.classes["@mozilla.org/xpcom/leakdetector;1"].getService(Components.interfaces.nsILeakDetector);
    leakDetector.dumpLeaks();
}

var entry = openCacheEntry();
var transport = entry.transport;
var output = transport.openOutputStream(0, -1, 0);
if (output.write("foo", 3) == 3)
    print("disk cache works!");
else
    print("disk cache sucks!");
entry.markValid();
entry.close();
