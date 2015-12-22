Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/NetUtil.jsm");

var httpserver = new HttpServer();

var expectedOnStopRequests = 3;

function setupChannel(suffix, xRequest, flags) {
    var chan = NetUtil.newChannel({
        uri: "http://localhost:" + httpserver.identity.primaryPort + suffix,
        loadUsingSystemPrincipal: true
    });
    if (flags)
        chan.loadFlags |= flags;

    var httpChan = chan.QueryInterface(Components.interfaces.nsIHttpChannel);
    httpChan.setRequestHeader("x-request", xRequest, false);
        
    return httpChan;
}

function Listener(response) {
    this._response = response;
}
Listener.prototype = {
    _response: null,
    _buffer: null,

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
        do_check_eq(this._buffer, this._response);
        if (--expectedOnStopRequests == 0)
            do_timeout(10, function() {
                        httpserver.stop(do_test_finished);
                    });
    }
};

function run_test() {
    httpserver.registerPathHandler("/bug596443", handler);
    httpserver.start(-1);

    // make sure we have a profile so we can use the disk-cache
    do_get_profile();

    // clear cache
    evict_cache_entries();

    var ch0 = setupChannel("/bug596443", "Response0", Ci.nsIRequest.LOAD_BYPASS_CACHE);
    ch0.asyncOpen2(new Listener("Response0"));

    var ch1 = setupChannel("/bug596443", "Response1", Ci.nsIRequest.LOAD_BYPASS_CACHE);
    ch1.asyncOpen2(new Listener("Response1"));

    var ch2 = setupChannel("/bug596443", "Should not be used");
    ch2.asyncOpen2(new Listener("Response1")); // Note param: we expect this to come from cache

    do_test_pending();
}

function triggerHandlers() {
    do_timeout(100, handlers[1]);
    do_timeout(100, handlers[0]);
}

var handlers = [];
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
    var request = metadata.getHeader("x-request");
    handlers.push(func(request));

    if (handlers.length > 1)
        triggerHandlers();
}
