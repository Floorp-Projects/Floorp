var DEBUG = true;

var clientID = "javascript";
var nsICache = Components.interfaces.nsICache;

function getCacheService()
{
    var nsCacheService = Components.classes["@mozilla.org/network/cache-service;1"];
    var service = nsCacheService.getService(Components.interfaces.nsICacheService);
    return service;
}

function CacheVisitor()
{
}

CacheVisitor.prototype = {
    QueryInterface : function(iid)
    {
        if (iid.equals(Components.interfaces.nsICacheVisitor))
            return this;
        throw Components.results.NS_NOINTERFACE;
    },

    visitDevice : function(deviceID, deviceInfo)
    {
        print("[visiting device (deviceID = " + deviceID + ", description = " + deviceInfo.description + ")]");
        return true;
    },
    
    visitEntry : function(deviceID, entryInfo)
    {
        print("[visiting entry (clientID = " + entryInfo.clientID + ", key = " + entryInfo.key + ")]");
        return true;
    }
};

function test()
{
    var cacheService = getCacheService();
    var visitor = new CacheVisitor();
    cacheService.visitEntries(visitor);
}

// load the cache service before doing anything with Java...
getCacheService();

if (DEBUG) {
    print("cache service loaded.");
} else {
    print("running cache visitor test.");
    test();
    print("cache visitor test complete.");
}
