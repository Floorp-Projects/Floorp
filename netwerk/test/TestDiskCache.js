var DEBUG = false;

var clientID = "javascript";
var key = "theme:button";
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
    var session = createCacheSession(clientID, nsICache.STORE_ON_DISK, true);
    var entry = session.openCacheEntry(key, mode);
    return entry;
}

function wrapInputStream(input)
{
    var nsIScriptableInputStream = Components.interfaces.nsIScriptableInputStream;
    var factory = Components.classes["@mozilla.org/scriptableinputstream;1"];
    var wrapper = factory.createInstance(nsIScriptableInputStream);
    wrapper.init(input);
    return wrapper;
}

function test()
{
    var outputEntry = openCacheEntry(nsICache.ACCESS_WRITE);
    var output = outputEntry.transport.openOutputStream(0, -1, 0);
    if (output.write("foo", 3) == 3)
        print("disk cache write works!");
    else
        print("disk cache write broken!");

    // store some metadata.
    outputEntry.setMetaDataElement("size", "3");

    output.close();
    outputEntry.markValid();
    outputEntry.close();

    var inputEntry = openCacheEntry(nsICache.ACCESS_READ);
    var input = wrapInputStream(inputEntry.transport.openInputStream(0, -1, 0));

    if (input.read(input.available()) == "foo")
        print("disk cache read works!");
    else
        print("disk cache read broken!");

    if (inputEntry.getMetaDataElement("size") == "3")
        print("disk cache metadata works!");
    else
        print("disk cache metadata broken!");

    input.close();
    inputEntry.close();
}

function doom()
{
    var doomedEntry = openCacheEntry(nsICache.ACCESS_READ_WRITE);
    doomedEntry.doom();
    doomedEntry.close();
}

if (DEBUG) {
    getCacheService();
    print("cache service loaded.");
} else {
    print("running disk cache test.");
    test();
    print("disk cache test complete.");
}
