var DEBUG = false;

var clientID = "javascript";
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

function openCacheEntry(url, mode)
{
    var session = createCacheSession(clientID, nsICache.STORE_ON_DISK, true);
    var entry = session.openCacheEntry(url, mode);
    return entry;
}

function dumpLeaks()
{
    var leakDetector = Components.classes["@mozilla.org/xpcom/leakdetector;1"].getService(Components.interfaces.nsILeakDetector);
    leakDetector.dumpLeaks();
}

function wrapInputStream(input)
{
    var nsIScriptableInputStream = Components.interfaces.nsIScriptableInputStream;
    var factory = Components.classes["@mozilla.org/scriptableinputstream;1"];
    var wrapper = factory.createInstance(nsIScriptableInputStream);
    wrapper.init(input);
    return wrapper;
}

function download(url)
{
    var data = "";
    var buffer = java.lang.reflect.Array.newInstance(java.lang.Byte.TYPE, 65536);
    var stream = url.getContent();
    while (true) {
        var count = stream.read(buffer);
        if (count <= 0)
            break;
        var str = new java.lang.String(buffer, 0, count);
        datas += str;
    }
    stream.close();
    return data;
}

function write(url, data)
{
    var key = url.toString();
    var outputEntry = openCacheEntry(key, nsICache.ACCESS_WRITE);
    var output = outputEntry.transport.openOutputStream(0, -1, 0);
    var count = output.write(data, data.length);

    // store some metadata.
    outputEntry.setMetaDataElement("size", data.length);

    output.close();
    outputEntry.markValid();
    outputEntry.close();

    return count;
}

function read(url)
{
    var key = url.toString();
    var inputEntry = openCacheEntry(key, nsICache.ACCESS_READ);
    var input = wrapInputStream(inputEntry.transport.openInputStream(0, -1, 0));
    var data = input.read(input.available());
    input.close();
    inputEntry.close();
    return data;
}

function readMetaData(url, element)
{
    var key = url.toString();
    var inputEntry = openCacheEntry(key, nsICache.ACCESS_READ);
    var metadata = inputEntry.getMetaDataElement(element);
    inputEntry.close();
    return metadata;
}

function doom(url)
{
    var key = url.toString();
    var doomedEntry = openCacheEntry(key, nsICache.ACCESS_READ_WRITE);
    doomedEntry.doom();
    doomedEntry.close();
}

function test()
{
    // download some real content from the network.
    var url = new java.net.URL("http://www.mozilla.org");
    var key = url.toString();
    var data = download(url);
    
    if (write(url, data) == data.length)
        print("disk cache write works!");
    else
        print("disk cache write broken!");

    if (read(url) == data)
        print("disk cache read works!");
    else
        print("disk cache read broken!");
        
    if (readMetaData(url, "size") == data.length)
        print("disk cache metadata works!");
    else
        print("disk cache metadata broken!");
}

function time()
{
    var System = java.lang.System;
    var url = new java.net.URL("http://www.mozilla.org");
    var begin = System.currentTimeMillis();
    for (var i = 0; i < 5; ++i)
        download(url);
    var end = System.currentTimeMillis();
    print("5 downloads took " + (end - begin) + " milliseconds.");
    print("average = " + ((end - begin) / 5) + " milliseconds.");

    begin = System.currentTimeMillis();
    for (var i = 0; i < 5; ++i)
        read(url);
    end = System.currentTimeMillis();
    print("5 reads took " + (end - begin) + " milliseconds.");
    print("average = " + ((end - begin) / 5) + " milliseconds.");
}

if (DEBUG) {
    getCacheService();
    print("cache service loaded.");
} else {
    print("running disk cache test.");
    test();
    print("disk cache test complete.");
}
