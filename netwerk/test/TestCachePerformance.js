var DEBUG = true;

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

function wrapInputStream(input)
{
    var nsIScriptableInputStream = Components.interfaces.nsIScriptableInputStream;
    var factory = Components.classes["@mozilla.org/scriptableinputstream;1"];
    var wrapper = factory.createInstance(nsIScriptableInputStream);
    wrapper.init(input);
    return wrapper;
}

function getIOService()
{
    var CID = Components.classes["@mozilla.org/network/io-service;1"];
    var service = CID.getService(Components.interfaces.nsIIOService);
    return service;
}

function downloadHTTP(spec)
{
    var ioService = getIOService();
    var uri = ioService.newURI(spec, null, null);
    var channel = ioService.newChannelFromURI(uri);
    var input = wrapInputStream(channel.open());
    var data = input.read(input.available());
    input.close();
    return data;
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
        data += str;
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

function median(array)
{
    var cmp = function(x, y) { return x - y; }
    array.sort(cmp);
    var middle = Math.floor(array.length / 2);
    return array[middle];
}

function sum(array)
{
    var s = 0;
    var len = array.length;
    for (var i = 0; i < len; ++i)
        s += array[i];
    return s;
}

function time()
{
    var N = 50;
    var System = java.lang.System;
    var url = new java.net.URL("http://www.mozilla.org");
    var downloadTimes = new Array();
    for (var i = 0; i < N; ++i) {
        var begin = System.currentTimeMillis();
        download(url);
        var end = System.currentTimeMillis();
        downloadTimes.push(end - begin);
    }
    var downloadTotal = sum(downloadTimes);
    var downloadMean = downloadTotal / N;
    var downloadMedian = median(downloadTimes);
    print("" + N + " downloads took " + downloadTotal + " milliseconds.");
    print("mean = " + downloadMean + " milliseconds.");
    print("median = " + downloadMedian + " milliseconds.");

    var readTimes = new Array();
    for (var i = 0; i < N; ++i) {
        var begin = System.currentTimeMillis();
        read(url);
        var end = System.currentTimeMillis();
        readTimes.push(end - begin);
    }
    var readTotal = sum(readTimes);
    var readMean = readTotal / N;
    var readMedian = median(readTimes);
    print("" + N + " reads took " + readTotal + " milliseconds.");
    print("mean = " + readMean + " milliseconds.");
    print("median = " + readMedian + " milliseconds.");
}

// load the cache service before doing anything with Java...
getCacheService();

if (DEBUG) {
    print("cache service loaded.");
} else {
    print("running disk cache test.");
    test();
    print("disk cache test complete.");
}
