do_load_httpd_js();
var httpserver = new nsHttpServer();

function getCacheService() {
    return Components.classes["@mozilla.org/network/cache-service;1"]
            .getService(Components.interfaces.nsICacheService);
}

function setupChannel(suffix, flags) {
    var ios = Components.classes["@mozilla.org/network/io-service;1"]
            .getService(Ci.nsIIOService);
    var chan = ios.newChannel("http://localhost:4444" + suffix, "", null);
    if (flags)
        chan.loadFlags |= flags;
    return chan;
}

function Listener(response, finalResponse) {
    this._response = response;
    this._finalResponse = finalResponse;
}
Listener.prototype = {
    _response: null,
    _buffer: null,
    _finalResponse: false,

    QueryInterface: function(iid) {
        if (iid.equals(Components.interfaces.nsIStreamListener) ||
            iid.equals(Components.interfaces.nsIRequestObserver) ||
            iid.equals(Components.interfaces.nsISupports))
          return this;
        throw Components.results.NS_ERROR_NO_INTERFACE;
    },

    onStartRequest: function (request, ctx) {
        this._buffer = "";
    },
    onDataAvailable: function (request, ctx, stream, offset, count) {
        this._buffer = this._buffer.concat(read_stream(stream, count));
    },
    onStopRequest: function (request, ctx, status) {
        var expected = "Response"+this._response;
        do_check_eq(this._buffer, expected);
        if (this._finalResponse)
            do_timeout(10, function() {
                        httpserver.stop(do_test_finished);
                    });
    }
};

function run_test() {
    httpserver.registerPathHandler("/bug596443", handler);
    httpserver.start(4444);

    // make sure we have a profile so we can use the disk-cache
    do_get_profile();

    // clear cache
    getCacheService().evictEntries(
            Components.interfaces.nsICache.STORE_ANYWHERE);

    var ch0 = setupChannel("/bug596443", Ci.nsIRequest.LOAD_BYPASS_CACHE);
    ch0.asyncOpen(new Listener(0), null);

    var ch1 = setupChannel("/bug596443", Ci.nsIRequest.LOAD_BYPASS_CACHE);
    ch1.asyncOpen(new Listener(1), null);

    var ch2 = setupChannel("/bug596443");
    ch2.asyncOpen(new Listener(1, true), null); // Note param: we expect this to come from cache

    do_test_pending();
}

function triggerHandlers() {
    do_timeout(100, function() {
        do_timeout(100, initialHandlers[1]);
        do_timeout(100, initialHandlers[0]);
    });
}

var initialHandlers = [];
var handlerNo = 0;
function handler(metadata, response) {
    var func = function(body) {
        return function() {
            response.setStatusLine(metadata.httpVersion, 200, "Ok");
            response.setHeader("Content-Type", "text/plain", false);
            response.setHeader("Content-Length", "" + body.length, false);
            response.setHeader("Cache-Control", "max-age=600", false);
            response.bodyOutputStream.write(body, body.length);
            response.finish();
         }};

    response.processAsync();
    initialHandlers[handlerNo] = func("Response"+handlerNo);
    handlerNo++;

    if (handlerNo > 1)
        triggerHandlers();
}
