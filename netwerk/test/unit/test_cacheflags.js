Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/NetUtil.jsm");

var httpserver = new HttpServer();
httpserver.start(-1);

// Need to randomize, because apparently no one clears our cache
var suffix = Math.random();
var httpBase = "http://localhost:" + httpserver.identity.primaryPort;
var httpsBase = "http://localhost:4445";
var shortexpPath = "/shortexp" + suffix;
var longexpPath = "/longexp/" + suffix;
var longexp2Path = "/longexp/2/" + suffix;
var nocachePath = "/nocache" + suffix;
var nostorePath = "/nostore" + suffix;
var test410Path = "/test410" + suffix;
var test404Path = "/test404" + suffix;

// We attach this to channel when we want to test Private Browsing mode
function LoadContext(usePrivateBrowsing) {
  this.usePrivateBrowsing = usePrivateBrowsing;
  this.originAttributes.privateBrowsingId = usePrivateBrowsing ? 1 : 0;
}

LoadContext.prototype = {
  originAttributes: {},
  usePrivateBrowsing: false,
  // don't bother defining rest of nsILoadContext fields: don't need 'em

  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsILoadContext))
      return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
  },

  getInterface: function(iid) {
    if (iid.equals(Ci.nsILoadContext))
      return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
  },

  originAttributes: {}
};

PrivateBrowsingLoadContext = new LoadContext(true);

function make_channel(url, flags, usePrivateBrowsing) {
  var securityFlags = Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL;
  if (usePrivateBrowsing) {
    securityFlags |= Ci.nsILoadInfo.SEC_FORCE_PRIVATE_BROWSING;
  }
  var req = NetUtil.newChannel({uri: url, loadUsingSystemPrincipal: true,
                                securityFlags: securityFlags});
  req.loadFlags = flags;
  if (usePrivateBrowsing) {
    req.notificationCallbacks = PrivateBrowsingLoadContext;
  }
  req.loadInfo.originAttributes = {privateBrowsingId: usePrivateBrowsing ? 1 : 0};
  return req;
}

function Test(path, flags, expectSuccess, readFromCache, hitServer, 
              usePrivateBrowsing /* defaults to false */) {
  this.path = path;
  this.flags = flags;
  this.expectSuccess = expectSuccess;
  this.readFromCache = readFromCache;
  this.hitServer = hitServer;
  this.usePrivateBrowsing = usePrivateBrowsing;
}

Test.prototype = {
  flags: 0,
  expectSuccess: true,
  readFromCache: false,
  hitServer: true,
  usePrivateBrowsing: false,
  _buffer: "",
  _isFromCache: false,

  QueryInterface: function(iid) {
    if (iid.equals(Components.interfaces.nsIStreamListener) ||
        iid.equals(Components.interfaces.nsIRequestObserver) ||
        iid.equals(Components.interfaces.nsISupports))
      return this;
    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  onStartRequest: function(request, context) {
    var cachingChannel = request.QueryInterface(Ci.nsICacheInfoChannel);
    this._isFromCache = request.isPending() && cachingChannel.isFromCache();
  },

  onDataAvailable: function(request, context, stream, offset, count) {
    this._buffer = this._buffer.concat(read_stream(stream, count));
  },

  onStopRequest: function(request, context, status) {
    do_check_eq(Components.isSuccessCode(status), this.expectSuccess);
    do_check_eq(this._isFromCache, this.readFromCache);
    do_check_eq(gHitServer, this.hitServer);

    do_timeout(0, run_next_test);
  },

  run: function() {
    dump("Running:" +
         "\n  " + this.path +
         "\n  " + this.flags +
         "\n  " + this.expectSuccess +
         "\n  " + this.readFromCache +
         "\n  " + this.hitServer + "\n");
    gHitServer = false;
    var channel = make_channel(this.path, this.flags, this.usePrivateBrowsing);
    channel.asyncOpen2(this);
  }
};

var gHitServer = false;

var gTests = [

  new Test(httpBase + shortexpPath, 0,
           true,   // expect success
           false,  // read from cache
           true,   // hit server
           true),  // USE PRIVATE BROWSING, so not cached for later requests
  new Test(httpBase + shortexpPath, 0,
           true,   // expect success
           false,  // read from cache
           true),  // hit server
  new Test(httpBase + shortexpPath, 0,
           true,   // expect success
           true,   // read from cache
           true),  // hit server
  new Test(httpBase + shortexpPath, Ci.nsIRequest.LOAD_BYPASS_CACHE,
           true,   // expect success
           false,  // read from cache
           true),  // hit server
  new Test(httpBase + shortexpPath, Ci.nsICachingChannel.LOAD_ONLY_FROM_CACHE,
           false,  // expect success
           false,  // read from cache
           false), // hit server
  new Test(httpBase + shortexpPath,
           Ci.nsICachingChannel.LOAD_ONLY_FROM_CACHE |
           Ci.nsIRequest.VALIDATE_NEVER,
           true,   // expect success
           true,   // read from cache
           false), // hit server
  new Test(httpBase + shortexpPath, Ci.nsIRequest.LOAD_FROM_CACHE,
           true,   // expect success
           true,   // read from cache
           false), // hit server

  new Test(httpBase + longexpPath, 0,
           true,   // expect success
           false,  // read from cache
           true),  // hit server
  new Test(httpBase + longexpPath, 0,
           true,   // expect success
           true,   // read from cache
           false), // hit server
  new Test(httpBase + longexpPath, Ci.nsIRequest.LOAD_BYPASS_CACHE,
           true,   // expect success
           false,  // read from cache
           true),  // hit server
  new Test(httpBase + longexpPath,
           Ci.nsIRequest.VALIDATE_ALWAYS,
           true,   // expect success
           true,   // read from cache
           true),  // hit server
  new Test(httpBase + longexpPath, Ci.nsICachingChannel.LOAD_ONLY_FROM_CACHE,
           true,   // expect success
           true,   // read from cache
           false), // hit server
  new Test(httpBase + longexpPath,
           Ci.nsICachingChannel.LOAD_ONLY_FROM_CACHE |
           Ci.nsIRequest.VALIDATE_NEVER,
           true,   // expect success
           true,   // read from cache
           false), // hit server
  new Test(httpBase + longexpPath,
           Ci.nsICachingChannel.LOAD_ONLY_FROM_CACHE |
           Ci.nsIRequest.VALIDATE_ALWAYS,
           false,  // expect success
           false,  // read from cache
           false), // hit server
  new Test(httpBase + longexpPath, Ci.nsIRequest.LOAD_FROM_CACHE,
           true,   // expect success
           true,   // read from cache
           false), // hit server

  new Test(httpBase + longexp2Path, 0,
           true,   // expect success
           false,  // read from cache
           true),  // hit server
  new Test(httpBase + longexp2Path, 0,
           true,   // expect success
           true,   // read from cache
           false), // hit server

  new Test(httpBase + nocachePath, 0,
           true,   // expect success
           false,  // read from cache
           true),  // hit server
  new Test(httpBase + nocachePath, 0,
           true,   // expect success
           true,   // read from cache
           true),  // hit server
  new Test(httpBase + nocachePath, Ci.nsICachingChannel.LOAD_ONLY_FROM_CACHE,
           false,  // expect success
           false,  // read from cache
           false), // hit server

  // CACHE2: mayhemer - entry is doomed... I think the logic is wrong, we should not doom them
  // as they are not valid, but take them as they need to reval
  /*
  new Test(httpBase + nocachePath, Ci.nsIRequest.LOAD_FROM_CACHE,
           true,   // expect success
           true,   // read from cache
           false), // hit server
  */

  // LOAD_ONLY_FROM_CACHE would normally fail (because no-cache forces
  // a validation), but VALIDATE_NEVER should override that.
  new Test(httpBase + nocachePath,
           Ci.nsICachingChannel.LOAD_ONLY_FROM_CACHE |
           Ci.nsIRequest.VALIDATE_NEVER,
           true,   // expect success
           true,   // read from cache
           false), // hit server

  // ... however, no-cache over ssl should act like no-store and force
  // a validation (and therefore failure) even if VALIDATE_NEVER is
  // set.
  /* XXX bug 466524: We can't currently start an ssl server in xpcshell tests,
                     so this test is currently disabled.
  new Test(httpsBase + nocachePath,
           Ci.nsICachingChannel.LOAD_ONLY_FROM_CACHE |
           Ci.nsIRequest.VALIDATE_NEVER,
           false,  // expect success
           false,  // read from cache
           false)  // hit server
  */

  new Test(httpBase + nostorePath, 0,
           true,   // expect success
           false,  // read from cache
           true),  // hit server
  new Test(httpBase + nostorePath, 0,
           true,   // expect success
           false,  // read from cache
           true),  // hit server
  new Test(httpBase + nostorePath, Ci.nsICachingChannel.LOAD_ONLY_FROM_CACHE,
           false,  // expect success
           false,  // read from cache
           false), // hit server
  new Test(httpBase + nostorePath, Ci.nsIRequest.LOAD_FROM_CACHE,
           true,   // expect success
           true,   // read from cache
           false), // hit server
  // no-store should force the validation (and therefore failure, with
  // LOAD_ONLY_FROM_CACHE) even if VALIDATE_NEVER is set.
  new Test(httpBase + nostorePath,
           Ci.nsICachingChannel.LOAD_ONLY_FROM_CACHE |
           Ci.nsIRequest.VALIDATE_NEVER,
           false,  // expect success
           false,  // read from cache
           false), // hit server

  new Test(httpBase + test410Path, 0,
           true,   // expect success
           false,  // read from cache
           true),  // hit server
  new Test(httpBase + test410Path, 0,
           true,   // expect success
           true,   // read from cache
           false), // hit server

  new Test(httpBase + test404Path, 0,
           true,   // expect success
           false,  // read from cache
           true),  // hit server
  new Test(httpBase + test404Path, 0,
           true,   // expect success
           false,  // read from cache
           true)   // hit server
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

function handler(httpStatus, metadata, response) {
  gHitServer = true;
  try {
    var etag = metadata.getHeader("If-None-Match");
  } catch(ex) {
    var etag = "";
  }
  if (etag == "testtag") {
    // Allow using the cached data
    response.setStatusLine(metadata.httpVersion, 304, "Not Modified");
  } else {
    response.setStatusLine(metadata.httpVersion, httpStatus, "Useless Phrase");
    response.setHeader("Content-Type", "text/plain", false);
    response.setHeader("ETag", "testtag", false);
    const body = "data";
    response.bodyOutputStream.write(body, body.length);
  }
}

function nocache_handler(metadata, response) {
  response.setHeader("Cache-Control", "no-cache", false);
  handler(200, metadata, response);
}

function nostore_handler(metadata, response) {
  response.setHeader("Cache-Control", "no-store", false);
  handler(200, metadata, response);
}

function test410_handler(metadata, response) {
  handler(410, metadata, response);
}

function test404_handler(metadata, response) {
  handler(404, metadata, response);
}

function shortexp_handler(metadata, response) {
  response.setHeader("Cache-Control", "max-age=0", false);
  handler(200, metadata, response);
}

function longexp_handler(metadata, response) {
  response.setHeader("Cache-Control", "max-age=10000", false);
  handler(200, metadata, response);
}

// test spaces around max-age value token
function longexp2_handler(metadata, response) {
  response.setHeader("Cache-Control", "max-age = 10000", false);
  handler(200, metadata, response);
}

function run_test() {
  httpserver.registerPathHandler(shortexpPath, shortexp_handler);
  httpserver.registerPathHandler(longexpPath, longexp_handler);
  httpserver.registerPathHandler(longexp2Path, longexp2_handler);
  httpserver.registerPathHandler(nocachePath, nocache_handler);
  httpserver.registerPathHandler(nostorePath, nostore_handler);
  httpserver.registerPathHandler(test410Path, test410_handler);
  httpserver.registerPathHandler(test404Path, test404_handler);

  run_next_test();
  do_test_pending();
}
