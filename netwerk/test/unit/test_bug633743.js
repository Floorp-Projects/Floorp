do_load_httpd_js();

const VALUE_HDR_NAME = "X-HTTP-VALUE-HEADER";
const VARY_HDR_NAME = "X-HTTP-VARY-HEADER";
const CACHECTRL_HDR_NAME = "X-CACHE-CONTROL-HEADER";

var httpserver = null;

function make_channel(flags, vary, value) {
  var ios = Cc["@mozilla.org/network/io-service;1"].
    getService(Ci.nsIIOService);
  var chan = ios.newChannel("http://localhost:4444/bug633743", null, null);
  return chan.QueryInterface(Ci.nsIHttpChannel);
}

function Test(flags, varyHdr, sendValue, expectValue, cacheHdr) {
    this._flags = flags;
    this._varyHdr = varyHdr;
    this._sendVal = sendValue;
    this._expectVal = expectValue;
    this._cacheHdr = cacheHdr;
}

Test.prototype = {
  _buffer: "",
  _flags: null,
  _varyHdr: null,
  _sendVal: null,
  _expectVal: null,
  _cacheHdr: null,

  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsIStreamListener) ||
        iid.equals(Ci.nsIRequestObserver) ||
        iid.equals(Ci.nsISupports))
      return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
  },

  onStartRequest: function(request, context) { },

  onDataAvailable: function(request, context, stream, offset, count) {
    this._buffer = this._buffer.concat(read_stream(stream, count));
  },

  onStopRequest: function(request, context, status) {
    do_check_eq(this._buffer, this._expectVal);
    do_timeout(0, run_next_test);
  },

  run: function() {
    var channel = make_channel();
    channel.loadFlags = this._flags;
    channel.setRequestHeader(VALUE_HDR_NAME, this._sendVal, false);
    channel.setRequestHeader(VARY_HDR_NAME, this._varyHdr, false);
    if (this._cacheHdr)
        channel.setRequestHeader(CACHECTRL_HDR_NAME, this._cacheHdr, false);

    channel.asyncOpen(this, null);
  }
};

var gTests = [
// Test LOAD_FROM_CACHE: Load cache-entry
  new Test(Ci.nsIRequest.LOAD_NORMAL,
          "entity-initial", // hdr-value used to vary
          "request1", // echoed by handler
          "request1"  // value expected to receive in channel
          ),
// Verify that it was cached
  new Test(Ci.nsIRequest.LOAD_NORMAL,
          "entity-initial", // hdr-value used to vary
          "fresh value with LOAD_NORMAL",  // echoed by handler
          "request1"  // value expected to receive in channel
          ),
// Load same entity with LOAD_FROM_CACHE-flag
  new Test(Ci.nsIRequest.LOAD_FROM_CACHE,
          "entity-initial", // hdr-value used to vary
          "fresh value with LOAD_FROM_CACHE",  // echoed by handler
          "request1"  // value expected to receive in channel
          ),
// Load different entity with LOAD_FROM_CACHE-flag
  new Test(Ci.nsIRequest.LOAD_FROM_CACHE,
          "entity-l-f-c", // hdr-value used to vary
          "request2", // echoed by handler
          "request2"  // value expected to receive in channel
          ),
// Verify that new value was cached
  new Test(Ci.nsIRequest.LOAD_NORMAL,
          "entity-l-f-c", // hdr-value used to vary
          "fresh value with LOAD_NORMAL",  // echoed by handler
          "request2"  // value expected to receive in channel
          ),

// Test VALIDATE_NEVER: Note previous cache-entry
  new Test(Ci.nsIRequest.VALIDATE_NEVER,
          "entity-v-n", // hdr-value used to vary
          "request3",  // echoed by handler
          "request3"  // value expected to receive in channel
          ),
// Verify that cache-entry was replaced
  new Test(Ci.nsIRequest.LOAD_NORMAL,
          "entity-v-n", // hdr-value used to vary
          "fresh value with LOAD_NORMAL",  // echoed by handler
          "request3"  // value expected to receive in channel
          ),

// Test combination VALIDATE_NEVER && no-store: Load new cache-entry
  new Test(Ci.nsIRequest.LOAD_NORMAL,
          "entity-2",// hdr-value used to vary
          "request4",  // echoed by handler
          "request4",  // value expected to receive in channel
          "no-store"   // set no-store on response
          ),
// Ensure we validate without IMS header in this case (verified in handler)
  new Test(Ci.nsIRequest.VALIDATE_NEVER,
          "entity-2-v-n",// hdr-value used to vary
          "request5",  // echoed by handler
          "request5"  // value expected to receive in channel
          ),

// Test VALIDATE-ALWAYS: Load new entity
  new Test(Ci.nsIRequest.LOAD_NORMAL,
          "entity-3",// hdr-value used to vary
          "request6",  // echoed by handler
          "request6",  // value expected to receive in channel
          "no-cache"   // set no-cache on response
          ),
// Ensure we don't send IMS header also in this case (verified in handler)
  new Test(Ci.nsIRequest.VALIDATE_ALWAYS,
          "entity-3-v-a",// hdr-value used to vary
          "request7",  // echoed by handler
          "request7"  // value expected to receive in channel
          ),
  ];

function run_next_test()
{
  if (gTests.length == 0) {
    httpserver.stop(do_test_finished);
    return;
  }

  var test = gTests.shift();
  test.run();
}

function handler(metadata, response) {

  // None of the tests above should send an IMS
  do_check_false(metadata.hasHeader("If-Modified-Since"));

  // Pick up requested value to echo
  var hdr = "default value";
  try {
    hdr = metadata.getHeader(VALUE_HDR_NAME);
  } catch(ex) { }

  // Pick up requested cache-control header-value
  var cctrlVal = "max-age=10000";
  try {
      cctrlVal = metadata.getHeader(CACHECTRL_HDR_NAME);
  } catch(ex) { }

  response.setStatusLine(metadata.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/plain", false);
  response.setHeader("Cache-Control", cctrlVal, false);
  response.setHeader("Vary", VARY_HDR_NAME, false);
  response.setHeader("Last-Modified", "Tue, 15 Nov 1994 12:45:26 GMT", false);
  response.bodyOutputStream.write(hdr, hdr.length);
}

function run_test() {

  // clear the cache
  evict_cache_entries();

  httpserver = new nsHttpServer();
  httpserver.registerPathHandler("/bug633743", handler);
  httpserver.start(4444);

  run_next_test();
  do_test_pending();
}
