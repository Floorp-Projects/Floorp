var nsICache = Components.interfaces.nsICache;

function getCacheService()
{
    var nsCacheService = Components.classes["@mozilla.org/network/cache-service;1"];
    var service = nsCacheService.getService(Components.interfaces.nsICacheService);
    return service;
}

function createCacheSession(clientID, storagePolicy, streamable)
{
    var service = getCacheService();
    var session = service.createSession(clientID, storagePolicy, streamable);
    return session;
}

function openCacheEntry(mode)
{
    var session = createCacheSession("javascript", nsICache.STORE_ON_DISK, true);
    var entry = session.openCacheEntry("theme:button", mode);
    return entry;
}

function dumpLeaks()
{
    var leakDetector = Components.classes["@mozilla.org/xpcom/leakdetector;1"].getService(Components.interfaces.nsILeakDetector);
    leakDetector.dumpLeaks();
}

var entry = openCacheEntry(nsICache.ACCESS_WRITE);
var output = entry.transport.openOutputStream(0, -1, 0);
if (output.write("foo", 3) == 3)
    print("disk cache write works!");
else
    print("disk cache write broken!");
output.close();
entry.markValid();
entry.close();

var newEntry = openCacheEntry(nsICache.ACCESS_READ);
var input = newEntry.transport.openInputStream(0, -1, 0);
if (input.available() == 3)
    print("disk cache read works!");
else
    print("disk cache read broken!");
input.close();
newEntry.close();
