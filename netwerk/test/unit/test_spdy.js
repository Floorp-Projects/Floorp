// test spdy/3

var Ci = Components.interfaces;
var Cc = Components.classes;

// Generate a small and a large post with known pre-calculated md5 sums
function generateContent(size) {
  var content = "";
  for (var i = 0; i < size; i++) {
    content += "0";
  }
  return content;
}

var posts = [];
posts.push(generateContent(10));
posts.push(generateContent(250000));

// pre-calculated md5sums (in hex) of the above posts
var md5s = ['f1b708bba17f1ce948dc979f4d7092bc',
            '2ef8d3b6c8f329318eb1a119b12622b6'];

var bigListenerData = generateContent(128 * 1024);
var bigListenerMD5 = '8f607cfdd2c87d6a7eedb657dafbd836';

function checkIsSpdy(request) {
  try {
    if (request.getResponseHeader("X-Firefox-Spdy") == "3") {
      if (request.getResponseHeader("X-Connection-Spdy") == "yes") {
        return true;
      }
      return false; // Weird case, but the server disagrees with us
    }
  } catch (e) {
    // Nothing to do here
  }
  return false;
}

var SpdyCheckListener = function() {};

SpdyCheckListener.prototype = {
  onStartRequestFired: false,
  onDataAvailableFired: false,
  isSpdyConnection: false,

  onStartRequest: function testOnStartRequest(request, ctx) {
    this.onStartRequestFired = true;

    if (!Components.isSuccessCode(request.status))
      do_throw("Channel should have a success code! (" + request.status + ")");
    if (!(request instanceof Components.interfaces.nsIHttpChannel))
      do_throw("Expecting an HTTP channel");

    do_check_eq(request.responseStatus, 200);
    do_check_eq(request.requestSucceeded, true);
  },

  onDataAvailable: function testOnDataAvailable(request, ctx, stream, off, cnt) {
    this.onDataAvailableFired = true;
    this.isSpdyConnection = checkIsSpdy(request);

    read_stream(stream, cnt);
  },

  onStopRequest: function testOnStopRequest(request, ctx, status) {
    do_check_true(this.onStartRequestFired);
    do_check_true(this.onDataAvailableFired);
    do_check_true(this.isSpdyConnection);

    run_next_test();
    do_test_finished();
  }
};

/*
 * Support for testing valid multiplexing of streams
 */

var multiplexContent = generateContent(30*1024);
var completed_channels = [];
function register_completed_channel(listener) {
  completed_channels.push(listener);
  if (completed_channels.length == 2) {
    do_check_neq(completed_channels[0].streamID, completed_channels[1].streamID);
    run_next_test();
    do_test_finished();
  }
}

/* Listener class to control the testing of multiplexing */
var SpdyMultiplexListener = function() {};

SpdyMultiplexListener.prototype = new SpdyCheckListener();

SpdyMultiplexListener.prototype.streamID = 0;
SpdyMultiplexListener.prototype.buffer = "";

SpdyMultiplexListener.prototype.onDataAvailable = function(request, ctx, stream, off, cnt) {
  this.onDataAvailableFired = true;
  this.isSpdyConnection = checkIsSpdy(request);
  this.streamID = parseInt(request.getResponseHeader("X-Spdy-StreamID"));
  var data = read_stream(stream, cnt);
  this.buffer = this.buffer.concat(data);
};

SpdyMultiplexListener.prototype.onStopRequest = function(request, ctx, status) {
  do_check_true(this.onStartRequestFired);
  do_check_true(this.onDataAvailableFired);
  do_check_true(this.isSpdyConnection);
  do_check_true(this.buffer == multiplexContent);
  
  // This is what does most of the hard work for us
  register_completed_channel(this);
};

// Does the appropriate checks for header gatewaying
var SpdyHeaderListener = function(value) {
  this.value = value
};

SpdyHeaderListener.prototype = new SpdyCheckListener();
SpdyHeaderListener.prototype.value = "";

SpdyHeaderListener.prototype.onDataAvailable = function(request, ctx, stream, off, cnt) {
  this.onDataAvailableFired = true;
  this.isSpdyConnection = checkIsSpdy(request);
  do_check_eq(request.getResponseHeader("X-Received-Test-Header"), this.value);
  read_stream(stream, cnt);
};

var SpdyPushListener = function() {};

SpdyPushListener.prototype = new SpdyCheckListener();

SpdyPushListener.prototype.onDataAvailable = function(request, ctx, stream, off, cnt) {
  this.onDataAvailableFired = true;
  this.isSpdyConnection = checkIsSpdy(request);
  if (ctx.originalURI.spec == "https://localhost:4443/push.js" ||
      ctx.originalURI.spec == "https://localhost:4443/push2.js") {
    do_check_eq(request.getResponseHeader("pushed"), "yes");
  }
  read_stream(stream, cnt);
};

// Does the appropriate checks for a large GET response
var SpdyBigListener = function() {};

SpdyBigListener.prototype = new SpdyCheckListener();
SpdyBigListener.prototype.buffer = "";

SpdyBigListener.prototype.onDataAvailable = function(request, ctx, stream, off, cnt) {
  this.onDataAvailableFired = true;
  this.isSpdyConnection = checkIsSpdy(request);
  this.buffer = this.buffer.concat(read_stream(stream, cnt));
  // We know the server should send us the same data as our big post will be,
  // so the md5 should be the same
  do_check_eq(bigListenerMD5, request.getResponseHeader("X-Expected-MD5"));
};

SpdyBigListener.prototype.onStopRequest = function(request, ctx, status) {
  do_check_true(this.onStartRequestFired);
  do_check_true(this.onDataAvailableFired);
  do_check_true(this.isSpdyConnection);

  // Don't want to flood output, so don't use do_check_eq
  do_check_true(this.buffer == bigListenerData);

  run_next_test();
  do_test_finished();
};

// Does the appropriate checks for POSTs
var SpdyPostListener = function(expected_md5) {
  this.expected_md5 = expected_md5;
};

SpdyPostListener.prototype = new SpdyCheckListener();
SpdyPostListener.prototype.expected_md5 = "";

SpdyPostListener.prototype.onDataAvailable = function(request, ctx, stream, off, cnt) {
  this.onDataAvailableFired = true;
  this.isSpdyConnection = checkIsSpdy(request);
  read_stream(stream, cnt);
  do_check_eq(this.expected_md5, request.getResponseHeader("X-Calculated-MD5"));
};

function makeChan(url) {
  var ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
  var chan = ios.newChannel(url, null, null).QueryInterface(Ci.nsIHttpChannel);

  return chan;
}

// Make sure we make a spdy connection and both us and the server mark it as such
function test_spdy_basic() {
  var chan = makeChan("https://localhost:4443/");
  var listener = new SpdyCheckListener();
  chan.asyncOpen(listener, null);
}

// Support for making sure XHR works over SPDY
function checkXhr(xhr) {
  if (xhr.readyState != 4) {
    return;
  }

  do_check_eq(xhr.status, 200);
  do_check_eq(checkIsSpdy(xhr), true);
  run_next_test();
  do_test_finished();
}

// Fires off an XHR request over SPDY
function test_spdy_xhr() {
  var req = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"]
            .createInstance(Ci.nsIXMLHttpRequest);
  req.open("GET", "https://localhost:4443/", true);
  req.addEventListener("readystatechange", function (evt) { checkXhr(req); },
                       false);
  req.send(null);
}

// Test to make sure we get multiplexing right
function test_spdy_multiplex() {
  var chan1 = makeChan("https://localhost:4443/multiplex1");
  var chan2 = makeChan("https://localhost:4443/multiplex2");
  var listener1 = new SpdyMultiplexListener();
  var listener2 = new SpdyMultiplexListener();
  chan1.asyncOpen(listener1, null);
  chan2.asyncOpen(listener2, null);
}

// Test to make sure we gateway non-standard headers properly
function test_spdy_header() {
  var chan = makeChan("https://localhost:4443/header");
  var hvalue = "Headers are fun";
  var listener = new SpdyHeaderListener(hvalue);
  chan.setRequestHeader("X-Test-Header", hvalue, false);
  chan.asyncOpen(listener, null);
}

function test_spdy_push1() {
  var chan = makeChan("https://localhost:4443/push");
  chan.loadGroup = loadGroup;
  var listener = new SpdyPushListener();
  chan.asyncOpen(listener, chan);
}

function test_spdy_push2() {
  var chan = makeChan("https://localhost:4443/push.js");
  chan.loadGroup = loadGroup;
  var listener = new SpdyPushListener();
  chan.asyncOpen(listener, chan);
}

function test_spdy_push3() {
  var chan = makeChan("https://localhost:4443/push2");
  chan.loadGroup = loadGroup;
  var listener = new SpdyPushListener();
  chan.asyncOpen(listener, chan);
}

function test_spdy_push4() {
  var chan = makeChan("https://localhost:4443/push2.js");
  chan.loadGroup = loadGroup;
  var listener = new SpdyPushListener();
  chan.asyncOpen(listener, chan);
}

// Make sure we handle GETs that cover more than 2 frames properly
function test_spdy_big() {
  var chan = makeChan("https://localhost:4443/big");
  var listener = new SpdyBigListener();
  chan.asyncOpen(listener, null);
}

// Support for doing a POST
function do_post(content, chan, listener) {
  var stream = Cc["@mozilla.org/io/string-input-stream;1"]
               .createInstance(Ci.nsIStringInputStream);
  stream.data = content;

  var uchan = chan.QueryInterface(Ci.nsIUploadChannel);
  uchan.setUploadStream(stream, "text/plain", stream.available());

  chan.requestMethod = "POST";

  chan.asyncOpen(listener, null);
}

// Make sure we can do a simple POST
function test_spdy_post() {
  var chan = makeChan("https://localhost:4443/post");
  var listener = new SpdyPostListener(md5s[0]);
  do_post(posts[0], chan, listener);
}

// Make sure we can do a POST that covers more than 2 frames
function test_spdy_post_big() {
  var chan = makeChan("https://localhost:4443/post");
  var listener = new SpdyPostListener(md5s[1]);
  do_post(posts[1], chan, listener);
}

// hack - the header test resets the multiplex object on the server,
// so make sure header is always run before the multiplex test.
//
// make sure post_big runs first to test race condition in restarting
// a stalled stream when a SETTINGS frame arrives
var tests = [ test_spdy_post_big
            , test_spdy_basic
            , test_spdy_push1
            , test_spdy_push2
            , test_spdy_push3
            , test_spdy_push4
            , test_spdy_xhr
            , test_spdy_header
            , test_spdy_multiplex
            , test_spdy_big
            , test_spdy_post
            ];
var current_test = 0;

function run_next_test() {
  if (current_test < tests.length) {
    tests[current_test]();
    current_test++;
    do_test_pending();
  }
}

// Support for making sure we can talk to the invalid cert the server presents
var CertOverrideListener = function(host, port, bits) {
  this.host = host;
  if (port) {
    this.port = port;
  }
  this.bits = bits;
};

CertOverrideListener.prototype = {
  host: null,
  port: -1,
  bits: null,

  getInterface: function(aIID) {
    return this.QueryInterface(aIID);
  },

  QueryInterface: function(aIID) {
    if (aIID.equals(Ci.nsIBadCertListener2) ||
        aIID.equals(Ci.nsIInterfaceRequestor) ||
        aIID.equals(Ci.nsISupports))
      return this;
    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  notifyCertProblem: function(socketInfo, sslStatus, targetHost) {
    var cert = sslStatus.QueryInterface(Ci.nsISSLStatus).serverCert;
    var cos = Cc["@mozilla.org/security/certoverride;1"].
              getService(Ci.nsICertOverrideService);
    cos.rememberValidityOverride(this.host, this.port, cert, this.bits, false);
    return true;
  },
};

function addCertOverride(host, port, bits) {
  var req = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"]
            .createInstance(Ci.nsIXMLHttpRequest);
  try {
    var url;
    if (port) {
      url = "https://" + host + ":" + port + "/";
    } else {
      url = "https://" + host + "/";
    }
    req.open("GET", url, false);
    req.channel.notificationCallbacks = new CertOverrideListener(host, port, bits);
    req.send(null);
  } catch (e) {
    // This will fail since the server is not trusted yet
  }
}

var prefs;
var spdypref;
var spdy3pref;
var spdypush;

var loadGroup;

function resetPrefs() {
  prefs.setBoolPref("network.http.spdy.enabled", spdypref);
  prefs.setBoolPref("network.http.spdy.enabled.v3", spdy3pref);
  prefs.setBoolPref("network.http.spdy.allow-push", spdypush);
}

function run_test() {
  // Set to allow the cert presented by our SPDY server
  do_get_profile();
  var prefs = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);
  var oldPref = prefs.getIntPref("network.http.speculative-parallel-limit");
  prefs.setIntPref("network.http.speculative-parallel-limit", 0);

  addCertOverride("localhost", 4443,
                  Ci.nsICertOverrideService.ERROR_UNTRUSTED |
                  Ci.nsICertOverrideService.ERROR_MISMATCH |
                  Ci.nsICertOverrideService.ERROR_TIME);

  prefs.setIntPref("network.http.speculative-parallel-limit", oldPref);

  // Enable all versions of spdy to see that we auto negotiate spdy/3
  spdypref = prefs.getBoolPref("network.http.spdy.enabled");
  spdy3pref = prefs.getBoolPref("network.http.spdy.enabled.v3");
  spdypush = prefs.getBoolPref("network.http.spdy.allow-push");
  prefs.setBoolPref("network.http.spdy.enabled", true);
  prefs.setBoolPref("network.http.spdy.enabled.v3", true);
  prefs.setBoolPref("network.http.spdy.allow-push", true);

  loadGroup = Cc["@mozilla.org/network/load-group;1"].createInstance(Ci.nsILoadGroup);

  // And make go!
  run_next_test();
}
