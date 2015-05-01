// test HTTP/2

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
posts.push(generateContent(128000));

// pre-calculated md5sums (in hex) of the above posts
var md5s = ['f1b708bba17f1ce948dc979f4d7092bc',
            '2ef8d3b6c8f329318eb1a119b12622b6'];

var bigListenerData = generateContent(128 * 1024);
var bigListenerMD5 = '8f607cfdd2c87d6a7eedb657dafbd836';

function checkIsHttp2(request) {
  try {
    if (request.getResponseHeader("X-Firefox-Spdy") == "h2-16") {
      if (request.getResponseHeader("X-Connection-Http2") == "yes") {
        return true;
      }
      return false; // Weird case, but the server disagrees with us
    }
  } catch (e) {
    // Nothing to do here
  }
  return false;
}

var Http2CheckListener = function() {};

Http2CheckListener.prototype = {
  onStartRequestFired: false,
  onDataAvailableFired: false,
  isHttp2Connection: false,
  shouldBeHttp2 : true,

  onStartRequest: function testOnStartRequest(request, ctx) {
    this.onStartRequestFired = true;
    if (!Components.isSuccessCode(request.status))
      do_throw("Channel should have a success code! (" + request.status + ")");

    do_check_true(request instanceof Components.interfaces.nsIHttpChannel);
    do_check_eq(request.responseStatus, 200);
    do_check_eq(request.requestSucceeded, true);
  },

  onDataAvailable: function testOnDataAvailable(request, ctx, stream, off, cnt) {
    this.onDataAvailableFired = true;
    this.isHttp2Connection = checkIsHttp2(request);

    read_stream(stream, cnt);
  },

  onStopRequest: function testOnStopRequest(request, ctx, status) {
    do_check_true(this.onStartRequestFired);
    do_check_true(Components.isSuccessCode(status));
    do_check_true(this.onDataAvailableFired);
    do_check_true(this.isHttp2Connection == this.shouldBeHttp2);

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
var Http2MultiplexListener = function() {};

Http2MultiplexListener.prototype = new Http2CheckListener();

Http2MultiplexListener.prototype.streamID = 0;
Http2MultiplexListener.prototype.buffer = "";

Http2MultiplexListener.prototype.onDataAvailable = function(request, ctx, stream, off, cnt) {
  this.onDataAvailableFired = true;
  this.isHttp2Connection = checkIsHttp2(request);
  this.streamID = parseInt(request.getResponseHeader("X-Http2-StreamID"));
  var data = read_stream(stream, cnt);
  this.buffer = this.buffer.concat(data);
};

Http2MultiplexListener.prototype.onStopRequest = function(request, ctx, status) {
  do_check_true(this.onStartRequestFired);
  do_check_true(this.onDataAvailableFired);
  do_check_true(this.isHttp2Connection);
  do_check_true(this.buffer == multiplexContent);

  // This is what does most of the hard work for us
  register_completed_channel(this);
};

// Does the appropriate checks for header gatewaying
var Http2HeaderListener = function(name, callback) {
  this.name = name;
  this.callback = callback;
};

Http2HeaderListener.prototype = new Http2CheckListener();
Http2HeaderListener.prototype.value = "";

Http2HeaderListener.prototype.onDataAvailable = function(request, ctx, stream, off, cnt) {
  this.onDataAvailableFired = true;
  this.isHttp2Connection = checkIsHttp2(request);
  var hvalue = request.getResponseHeader(this.name);
  do_check_neq(hvalue, "");
  this.callback(hvalue);
  read_stream(stream, cnt);
};

var Http2PushListener = function() {};

Http2PushListener.prototype = new Http2CheckListener();

Http2PushListener.prototype.onDataAvailable = function(request, ctx, stream, off, cnt) {
  this.onDataAvailableFired = true;
  this.isHttp2Connection = checkIsHttp2(request);
  if (ctx.originalURI.spec == "https://localhost:" + serverPort + "/push.js" ||
      ctx.originalURI.spec == "https://localhost:" + serverPort + "/push2.js") {
    do_check_eq(request.getResponseHeader("pushed"), "yes");
  }
  read_stream(stream, cnt);
};

const pushHdrTxt = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
const pullHdrTxt = pushHdrTxt.split('').reverse().join('');

function checkContinuedHeaders(getHeader, headerPrefix, headerText) {
  for (var i = 0; i < 265; i++) {
    do_check_eq(getHeader(headerPrefix + 1), headerText);
  }
}

var Http2ContinuedHeaderListener = function() {};

Http2ContinuedHeaderListener.prototype = new Http2CheckListener();

Http2ContinuedHeaderListener.prototype.onStopsLeft = 2;

Http2ContinuedHeaderListener.prototype.QueryInterface = function (aIID) {
  if (aIID.equals(Ci.nsIHttpPushListener) ||
      aIID.equals(Ci.nsIStreamListener))
    return this;
  throw Components.results.NS_ERROR_NO_INTERFACE;
};

Http2ContinuedHeaderListener.prototype.getInterface = function(aIID) {
  return this.QueryInterface(aIID);
};

Http2ContinuedHeaderListener.prototype.onDataAvailable = function (request, ctx, stream, off, cnt) {
  this.onDataAvailableFired = true;
  this.isHttp2Connection = checkIsHttp2(request);
  if (request.originalURI.spec == "https://localhost:" + serverPort + "/continuedheaders") {
    // This is the original request, so the only one where we'll have continued response headers
    checkContinuedHeaders(request.getResponseHeader, "X-Pull-Test-Header-", pullHdrTxt);
  }
  read_stream(stream, cnt);
};

Http2ContinuedHeaderListener.prototype.onStopRequest = function (request, ctx, status) {
  do_check_true(this.onStartRequestFired);
  do_check_true(Components.isSuccessCode(status));
  do_check_true(this.onDataAvailableFired);
  do_check_true(this.isHttp2Connection);

  --this.onStopsLeft;
  if (this.onStopsLeft === 0) {
    run_next_test();
    do_test_finished();
  }
};

Http2ContinuedHeaderListener.prototype.onPush = function(associatedChannel, pushChannel) {
  do_check_eq(associatedChannel.originalURI.spec, "https://localhost:" + serverPort + "/continuedheaders");
  do_check_eq(pushChannel.getRequestHeader("x-pushed-request"), "true");
  checkContinuedHeaders(pushChannel.getRequestHeader, "X-Push-Test-Header-", pushHdrTxt);

  pushChannel.asyncOpen(this, pushChannel);
};

// Does the appropriate checks for a large GET response
var Http2BigListener = function() {};

Http2BigListener.prototype = new Http2CheckListener();
Http2BigListener.prototype.buffer = "";

Http2BigListener.prototype.onDataAvailable = function(request, ctx, stream, off, cnt) {
  this.onDataAvailableFired = true;
  this.isHttp2Connection = checkIsHttp2(request);
  this.buffer = this.buffer.concat(read_stream(stream, cnt));
  // We know the server should send us the same data as our big post will be,
  // so the md5 should be the same
  do_check_eq(bigListenerMD5, request.getResponseHeader("X-Expected-MD5"));
};

Http2BigListener.prototype.onStopRequest = function(request, ctx, status) {
  do_check_true(this.onStartRequestFired);
  do_check_true(this.onDataAvailableFired);
  do_check_true(this.isHttp2Connection);

  // Don't want to flood output, so don't use do_check_eq
  do_check_true(this.buffer == bigListenerData);

  run_next_test();
  do_test_finished();
};

var Http2HugeSuspendedListener = function() {};

Http2HugeSuspendedListener.prototype = new Http2CheckListener();
Http2HugeSuspendedListener.prototype.count = 0;

Http2HugeSuspendedListener.prototype.onDataAvailable = function(request, ctx, stream, off, cnt) {
  this.onDataAvailableFired = true;
  this.isHttp2Connection = checkIsHttp2(request);
  this.count += cnt;
  read_stream(stream, cnt);
};

Http2HugeSuspendedListener.prototype.onStopRequest = function(request, ctx, status) {
  do_check_true(this.onStartRequestFired);
  do_check_true(this.onDataAvailableFired);
  do_check_true(this.isHttp2Connection);
  do_check_eq(this.count, 1024 * 1024 * 1); // 1mb of data expected
  run_next_test();
  do_test_finished();
};

// Does the appropriate checks for POSTs
var Http2PostListener = function(expected_md5) {
  this.expected_md5 = expected_md5;
};

Http2PostListener.prototype = new Http2CheckListener();
Http2PostListener.prototype.expected_md5 = "";

Http2PostListener.prototype.onDataAvailable = function(request, ctx, stream, off, cnt) {
  this.onDataAvailableFired = true;
  this.isHttp2Connection = checkIsHttp2(request);
  read_stream(stream, cnt);
  do_check_eq(this.expected_md5, request.getResponseHeader("X-Calculated-MD5"));
};

function makeChan(url) {
  var ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
  var chan = ios.newChannel2(url,
                             null,
                             null,
                             null,      // aLoadingNode
                             Services.scriptSecurityManager.getSystemPrincipal(),
                             null,      // aTriggeringPrincipal
                             Ci.nsILoadInfo.SEC_NORMAL,
                             Ci.nsIContentPolicy.TYPE_OTHER).QueryInterface(Ci.nsIHttpChannel);

  return chan;
}

// Make sure we make a HTTP2 connection and both us and the server mark it as such
function test_http2_basic() {
  var chan = makeChan("https://localhost:" + serverPort + "/");
  var listener = new Http2CheckListener();
  chan.asyncOpen(listener, null);
}

function test_http2_basic_unblocked_dep() {
  var chan = makeChan("https://localhost:" + serverPort + "/basic_unblocked_dep");
  var cos = chan.QueryInterface(Ci.nsIClassOfService);
  cos.addClassFlags(Ci.nsIClassOfService.Unblocked);
  var listener = new Http2CheckListener();
  chan.asyncOpen(listener, null);
}

// make sure we don't use h2 when disallowed
function test_http2_nospdy() {
  var chan = makeChan("https://localhost:" + serverPort + "/");
  var listener = new Http2CheckListener();
  var internalChannel = chan.QueryInterface(Ci.nsIHttpChannelInternal);
  internalChannel.allowSpdy = false;
  listener.shouldBeHttp2 = false;
  chan.asyncOpen(listener, null);
}

// Support for making sure XHR works over SPDY
function checkXhr(xhr) {
  if (xhr.readyState != 4) {
    return;
  }

  do_check_eq(xhr.status, 200);
  do_check_eq(checkIsHttp2(xhr), true);
  run_next_test();
  do_test_finished();
}

// Fires off an XHR request over h2
function test_http2_xhr() {
  var req = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"]
            .createInstance(Ci.nsIXMLHttpRequest);
  req.open("GET", "https://localhost:" + serverPort + "/", true);
  req.addEventListener("readystatechange", function (evt) { checkXhr(req); },
                       false);
  req.send(null);
}

var concurrent_channels = [];

var Http2ConcurrentListener = function() {};

Http2ConcurrentListener.prototype = new Http2CheckListener();
Http2ConcurrentListener.prototype.count = 0;
Http2ConcurrentListener.prototype.target = 0;
Http2ConcurrentListener.prototype.reset = 0;
Http2ConcurrentListener.prototype.recvdHdr = 0;

Http2ConcurrentListener.prototype.onStopRequest = function(request, ctx, status) {
  this.count++;
  do_check_true(this.isHttp2Connection);
  if (this.recvdHdr > 0) {
    do_check_eq(request.getResponseHeader("X-Recvd"), this.recvdHdr);
  }

  if (this.count == this.target) {
    if (this.reset > 0) {
      prefs.setIntPref("network.http.spdy.default-concurrent", this.reset);
    }
    run_next_test();
    do_test_finished();
  }
};

function test_http2_concurrent() {
  var concurrent_listener = new Http2ConcurrentListener();
  concurrent_listener.target = 201;
  concurrent_listener.reset = prefs.getIntPref("network.http.spdy.default-concurrent");
  prefs.setIntPref("network.http.spdy.default-concurrent", 100);

  for (var i = 0; i < concurrent_listener.target; i++) {
    concurrent_channels[i] = makeChan("https://localhost:" + serverPort + "/750ms");
    concurrent_channels[i].loadFlags = Ci.nsIRequest.LOAD_BYPASS_CACHE;
    concurrent_channels[i].asyncOpen(concurrent_listener, null);
  }
}

function test_http2_concurrent_post() {
  var concurrent_listener = new Http2ConcurrentListener();
  concurrent_listener.target = 8;
  concurrent_listener.recvdHdr = posts[2].length;
  concurrent_listener.reset = prefs.getIntPref("network.http.spdy.default-concurrent");
  prefs.setIntPref("network.http.spdy.default-concurrent", 3);

  for (var i = 0; i < concurrent_listener.target; i++) {
    concurrent_channels[i] = makeChan("https://localhost:" + serverPort + "/750msPost");
    concurrent_channels[i].loadFlags = Ci.nsIRequest.LOAD_BYPASS_CACHE;
    var stream = Cc["@mozilla.org/io/string-input-stream;1"]
               .createInstance(Ci.nsIStringInputStream);
    stream.data = posts[2];
    var uchan = concurrent_channels[i].QueryInterface(Ci.nsIUploadChannel);
    uchan.setUploadStream(stream, "text/plain", stream.available());
    concurrent_channels[i].requestMethod = "POST";
    concurrent_channels[i].asyncOpen(concurrent_listener, null);
  }
}

// Test to make sure we get multiplexing right
function test_http2_multiplex() {
  var chan1 = makeChan("https://localhost:" + serverPort + "/multiplex1");
  var chan2 = makeChan("https://localhost:" + serverPort + "/multiplex2");
  var listener1 = new Http2MultiplexListener();
  var listener2 = new Http2MultiplexListener();
  chan1.asyncOpen(listener1, null);
  chan2.asyncOpen(listener2, null);
}

// Test to make sure we gateway non-standard headers properly
function test_http2_header() {
  var chan = makeChan("https://localhost:" + serverPort + "/header");
  var hvalue = "Headers are fun";
  chan.setRequestHeader("X-Test-Header", hvalue, false);
  var listener = new Http2HeaderListener("X-Received-Test-Header", function(received_hvalue) {
    do_check_eq(received_hvalue, hvalue);
  });
  chan.asyncOpen(listener, null);
}

// Test to make sure cookies are split into separate fields before compression
function test_http2_cookie_crumbling() {
  var chan = makeChan("https://localhost:" + serverPort + "/cookie_crumbling");
  var cookiesSent = ['a=b', 'c=d01234567890123456789', 'e=f'].sort();
  chan.setRequestHeader("Cookie", cookiesSent.join('; '), false);
  var listener = new Http2HeaderListener("X-Received-Header-Pairs", function(pairsReceived) {
    var cookiesReceived = JSON.parse(pairsReceived).filter(function(pair) {
      return pair[0] == 'cookie';
    }).map(function(pair) {
      return pair[1];
    }).sort();
    do_check_eq(cookiesReceived.length, cookiesSent.length);
    cookiesReceived.forEach(function(cookieReceived, index) {
      do_check_eq(cookiesSent[index], cookieReceived)
    });
  });
  chan.asyncOpen(listener, null);
}

function test_http2_push1() {
  var chan = makeChan("https://localhost:" + serverPort + "/push");
  chan.loadGroup = loadGroup;
  var listener = new Http2PushListener();
  chan.asyncOpen(listener, chan);
}

function test_http2_push2() {
  var chan = makeChan("https://localhost:" + serverPort + "/push.js");
  chan.loadGroup = loadGroup;
  var listener = new Http2PushListener();
  chan.asyncOpen(listener, chan);
}

function test_http2_push3() {
  var chan = makeChan("https://localhost:" + serverPort + "/push2");
  chan.loadGroup = loadGroup;
  var listener = new Http2PushListener();
  chan.asyncOpen(listener, chan);
}

function test_http2_push4() {
  var chan = makeChan("https://localhost:" + serverPort + "/push2.js");
  chan.loadGroup = loadGroup;
  var listener = new Http2PushListener();
  chan.asyncOpen(listener, chan);
}

// this is a basic test where the server sends a simple document with 2 header
// blocks. bug 1027364
function test_http2_doubleheader() {
  var chan = makeChan("https://localhost:" + serverPort + "/doubleheader");
  var listener = new Http2CheckListener();
  chan.asyncOpen(listener, null);
}

// Make sure we handle GETs that cover more than 2 frames properly
function test_http2_big() {
  var chan = makeChan("https://localhost:" + serverPort + "/big");
  var listener = new Http2BigListener();
  chan.asyncOpen(listener, null);
}

function test_http2_huge_suspended() {
  var chan = makeChan("https://localhost:" + serverPort + "/huge");
  var listener = new Http2HugeSuspendedListener();
  chan.asyncOpen(listener, null);
  chan.suspend();
  do_timeout(500, chan.resume);
}

// Support for doing a POST
function do_post(content, chan, listener, method) {
  var stream = Cc["@mozilla.org/io/string-input-stream;1"]
               .createInstance(Ci.nsIStringInputStream);
  stream.data = content;

  var uchan = chan.QueryInterface(Ci.nsIUploadChannel);
  uchan.setUploadStream(stream, "text/plain", stream.available());

  chan.requestMethod = method;

  chan.asyncOpen(listener, null);
}

// Make sure we can do a simple POST
function test_http2_post() {
  var chan = makeChan("https://localhost:" + serverPort + "/post");
  var listener = new Http2PostListener(md5s[0]);
  do_post(posts[0], chan, listener, "POST");
}

// Make sure we can do a simple PATCH
function test_http2_patch() {
  var chan = makeChan("https://localhost:" + serverPort + "/patch");
  var listener = new Http2PostListener(md5s[0]);
  do_post(posts[0], chan, listener, "PATCH");
}

// Make sure we can do a POST that covers more than 2 frames
function test_http2_post_big() {
  var chan = makeChan("https://localhost:" + serverPort + "/post");
  var listener = new Http2PostListener(md5s[1]);
  do_post(posts[1], chan, listener, "POST");
}

Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/Services.jsm");

var httpserv = null;
var httpserv2 = null;
var ios = Components.classes["@mozilla.org/network/io-service;1"]
                    .getService(Components.interfaces.nsIIOService);

var altsvcClientListener = {
  onStartRequest: function test_onStartR(request, ctx) {
    do_check_eq(request.status, Components.results.NS_OK);
  },

  onDataAvailable: function test_ODA(request, cx, stream, offset, cnt) {
   read_stream(stream, cnt);
  },

  onStopRequest: function test_onStopR(request, ctx, status) {
    var isHttp2Connection = checkIsHttp2(request.QueryInterface(Components.interfaces.nsIHttpChannel));
    if (!isHttp2Connection) {
      dump("/altsvc1 not over h2 yet - retry\n");
      var chan = makeChan("http://localhost:" + httpserv.identity.primaryPort + "/altsvc1")
                .QueryInterface(Components.interfaces.nsIHttpChannel);
      // we use this header to tell the server to issue a altsvc frame for the
      // speficied origin we will use in the next part of the test
      chan.setRequestHeader("x-redirect-origin",
                 "http://localhost:" + httpserv2.identity.primaryPort, false);
      chan.loadFlags = Ci.nsIRequest.LOAD_BYPASS_CACHE;
      chan.asyncOpen(altsvcClientListener, chan);
    } else {
      do_check_true(isHttp2Connection);
      var chan = makeChan("http://localhost:" + httpserv2.identity.primaryPort + "/altsvc2")
                .QueryInterface(Components.interfaces.nsIHttpChannel);
      chan.loadFlags = Ci.nsIRequest.LOAD_BYPASS_CACHE;
      chan.asyncOpen(altsvcClientListener2, chan);
    }
  }
};

var altsvcClientListener2 = {
  onStartRequest: function test_onStartR(request, ctx) {
    do_check_eq(request.status, Components.results.NS_OK);
  },

  onDataAvailable: function test_ODA(request, cx, stream, offset, cnt) {
   read_stream(stream, cnt);
  },

  onStopRequest: function test_onStopR(request, ctx, status) {
    var isHttp2Connection = checkIsHttp2(request.QueryInterface(Components.interfaces.nsIHttpChannel));
    if (!isHttp2Connection) {
      dump("/altsvc2 not over h2 yet - retry\n");
      var chan = makeChan("http://localhost:" + httpserv2.identity.primaryPort + "/altsvc2")
                .QueryInterface(Components.interfaces.nsIHttpChannel);
      chan.loadFlags = Ci.nsIRequest.LOAD_BYPASS_CACHE;
      chan.asyncOpen(altsvcClientListener2, chan);
    } else {
      do_check_true(isHttp2Connection);
      run_next_test();
      do_test_finished();
    }
  }
};

function altsvcHttp1Server(metadata, response) {
  response.setStatusLine(metadata.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/plain", false);
  response.setHeader("Connection", "close", false);
  response.setHeader("Alt-Svc", 'h2=":' + serverPort + '"', false);
  var body = "this is where a cool kid would write something neat.\n";
  response.bodyOutputStream.write(body, body.length);
}

function altsvcHttp1Server2(metadata, response) {
// this server should never be used thanks to an alt svc frame from the
// h2 server.. but in case of some async lag in setting the alt svc route
// up we have it.
  response.setStatusLine(metadata.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/plain", false);
  response.setHeader("Connection", "close", false);
  var body = "hanging.\n";
  response.bodyOutputStream.write(body, body.length);
}

function test_http2_altsvc() {
  var chan = makeChan("http://localhost:" + httpserv.identity.primaryPort + "/altsvc1")
           .QueryInterface(Components.interfaces.nsIHttpChannel);
  chan.asyncOpen(altsvcClientListener, chan);
}

var Http2PushApiListener = function() {};

Http2PushApiListener.prototype = {
  checksPending: 9, // 4 onDataAvailable and 5 onStop

  getInterface: function(aIID) {
    return this.QueryInterface(aIID);
  },

  QueryInterface: function(aIID) {
    if (aIID.equals(Ci.nsIHttpPushListener) ||
        aIID.equals(Ci.nsIStreamListener))
      return this;
    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  // nsIHttpPushListener
  onPush: function onPush(associatedChannel, pushChannel) {
    do_check_eq(associatedChannel.originalURI.spec, "https://localhost:" + serverPort + "/pushapi1");
    do_check_eq (pushChannel.getRequestHeader("x-pushed-request"), "true");

    pushChannel.asyncOpen(this, pushChannel);
    if (pushChannel.originalURI.spec == "https://localhost:" + serverPort + "/pushapi1/2") {
      pushChannel.cancel(Components.results.NS_ERROR_ABORT);
    }
  },

 // normal Channel listeners
  onStartRequest: function pushAPIOnStart(request, ctx) {
  },

  onDataAvailable: function pushAPIOnDataAvailable(request, ctx, stream, offset, cnt) {
    do_check_neq(ctx.originalURI.spec, "https://localhost:" + serverPort + "/pushapi1/2");

    var data = read_stream(stream, cnt);

    if (ctx.originalURI.spec == "https://localhost:" + serverPort + "/pushapi1") {
      do_check_eq(data[0], '0');
      --this.checksPending;
    } else if (ctx.originalURI.spec == "https://localhost:" + serverPort + "/pushapi1/1") {
      do_check_eq(data[0], '1');
      --this.checksPending; // twice
    } else if (ctx.originalURI.spec == "https://localhost:" + serverPort + "/pushapi1/3") {
      do_check_eq(data[0], '3');
      --this.checksPending;
    } else {
      do_check_eq(true, false);
    }
  },

  onStopRequest: function test_onStopR(request, ctx, status) {
    if (ctx.originalURI.spec == "https://localhost:" + serverPort + "/pushapi1/2") {
      do_check_eq(request.status, Components.results.NS_ERROR_ABORT);
    } else {
      do_check_eq(request.status, Components.results.NS_OK);
    }

    --this.checksPending; // 5 times - one for each push plus the pull
    if (!this.checksPending) {
      run_next_test();
      do_test_finished();
    }
  }
};

// pushAPI testcase 1 expects
// 1 to pull /pushapi1 with 0
// 2 to see /pushapi1/1 with 1
// 3 to see /pushapi1/1 with 1 (again)
// 4 to see /pushapi1/2 that it will cancel
// 5 to see /pushapi1/3 with 3

function test_http2_pushapi_1() {
  var chan = makeChan("https://localhost:" + serverPort + "/pushapi1");
  chan.loadGroup = loadGroup;
  var listener = new Http2PushApiListener();
  chan.notificationCallbacks = listener;
  chan.asyncOpen(listener, chan);
}

var WrongSuiteListener = function() {};
WrongSuiteListener.prototype = new Http2CheckListener();
WrongSuiteListener.prototype.shouldBeHttp2 = false;
WrongSuiteListener.prototype.onStopRequest = function(request, ctx, status) {
  prefs.setBoolPref("security.ssl3.ecdhe_rsa_aes_128_gcm_sha256", true);
  Http2CheckListener.prototype.onStopRequest.call(this);
};

// test that we use h1 without the mandatory cipher suite available
function test_http2_wrongsuite() {
  prefs.setBoolPref("security.ssl3.ecdhe_rsa_aes_128_gcm_sha256", false);
  var chan = makeChan("https://localhost:" + serverPort + "/wrongsuite");
  chan.loadFlags = Ci.nsIRequest.LOAD_FRESH_CONNECTION | Ci.nsIChannel.LOAD_INITIAL_DOCUMENT_URI;
  var listener = new WrongSuiteListener();
  chan.asyncOpen(listener, null);
}

function test_http2_h11required_stream() {
  var chan = makeChan("https://localhost:" + serverPort + "/h11required_stream");
  var listener = new Http2CheckListener();
  listener.shouldBeHttp2 = false;
  chan.asyncOpen(listener, null);
}

function H11RequiredSessionListener () { }
H11RequiredSessionListener.prototype = new Http2CheckListener();

H11RequiredSessionListener.prototype.onStopRequest = function (request, ctx, status) {
  var streamReused = request.getResponseHeader("X-H11Required-Stream-Ok");
  do_check_eq(streamReused, "yes");

  do_check_true(this.onStartRequestFired);
  do_check_true(this.onDataAvailableFired);
  do_check_true(this.isHttp2Connection == this.shouldBeHttp2);

  run_next_test();
  do_test_finished();
};

function test_http2_h11required_session() {
  var chan = makeChan("https://localhost:" + serverPort + "/h11required_session");
  var listener = new H11RequiredSessionListener();
  listener.shouldBeHttp2 = false;
  chan.asyncOpen(listener, null);
}

function test_http2_retry_rst() {
  var chan = makeChan("https://localhost:" + serverPort + "/rstonce");
  var listener = new Http2CheckListener();
  chan.asyncOpen(listener, null);
}

function test_http2_continuations() {
  var chan = makeChan("https://localhost:" + serverPort + "/continuedheaders");
  chan.loadGroup = loadGroup;
  var listener = new Http2ContinuedHeaderListener();
  chan.notificationCallbacks = listener;
  chan.asyncOpen(listener, chan);
}

function test_complete() {
  resetPrefs();
  do_test_pending();
  httpserv.stop(do_test_finished);
  do_test_pending();
  httpserv2.stop(do_test_finished);

  do_test_finished();
  do_timeout(0,run_next_test);
}

// hack - the header test resets the multiplex object on the server,
// so make sure header is always run before the multiplex test.
//
// make sure post_big runs first to test race condition in restarting
// a stalled stream when a SETTINGS frame arrives
var tests = [ test_http2_post_big
            , test_http2_basic
            , test_http2_concurrent
            , test_http2_concurrent_post
            , test_http2_basic_unblocked_dep
            , test_http2_nospdy
            , test_http2_push1
            , test_http2_push2
            , test_http2_push3
            , test_http2_push4
            // , test_http2_altsvc
            , test_http2_doubleheader
            , test_http2_xhr
            , test_http2_header
            , test_http2_cookie_crumbling
            , test_http2_multiplex
            , test_http2_big
            , test_http2_huge_suspended
            , test_http2_post
            , test_http2_patch
            , test_http2_pushapi_1
            , test_http2_continuations
            // These next two must always come in this order
	    // best to add new tests before h1 streams get too involved
            , test_http2_h11required_stream
            , test_http2_h11required_session
            , test_http2_retry_rst
            , test_http2_wrongsuite

            // cleanup
            , test_complete
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
    dump("Certificate Override in place\n");
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
var http2draftpref;
var http2pref;
var tlspref;
var altsvcpref1;
var altsvcpref2;
var loadGroup;
var serverPort;

function resetPrefs() {
  prefs.setBoolPref("network.http.spdy.enabled", spdypref);
  prefs.setBoolPref("network.http.spdy.enabled.v3-1", spdy3pref);
  prefs.setBoolPref("network.http.spdy.allow-push", spdypush);
  prefs.setBoolPref("network.http.spdy.enabled.http2draft", http2draftpref);
  prefs.setBoolPref("network.http.spdy.enabled.http2", http2pref);
  prefs.setBoolPref("network.http.spdy.enforce-tls-profile", tlspref);
  prefs.setBoolPref("network.http.altsvc.enabled", altsvcpref1);
  prefs.setBoolPref("network.http.altsvc.oe", altsvcpref2);
}

function run_test() {
  var env = Cc["@mozilla.org/process/environment;1"].getService(Ci.nsIEnvironment);
  serverPort = env.get("MOZHTTP2-PORT");
  do_check_neq(serverPort, null);
  dump("using port " + serverPort + "\n");

  // Set to allow the cert presented by our H2 server
  do_get_profile();
  prefs = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);
  var oldPref = prefs.getIntPref("network.http.speculative-parallel-limit");
  prefs.setIntPref("network.http.speculative-parallel-limit", 0);

  addCertOverride("localhost", serverPort,
                  Ci.nsICertOverrideService.ERROR_UNTRUSTED |
                  Ci.nsICertOverrideService.ERROR_MISMATCH |
                  Ci.nsICertOverrideService.ERROR_TIME);

  prefs.setIntPref("network.http.speculative-parallel-limit", oldPref);

  // Enable all versions of spdy to see that we auto negotiate http/2
  spdypref = prefs.getBoolPref("network.http.spdy.enabled");
  spdy3pref = prefs.getBoolPref("network.http.spdy.enabled.v3-1");
  spdypush = prefs.getBoolPref("network.http.spdy.allow-push");
  http2draftpref = prefs.getBoolPref("network.http.spdy.enabled.http2draft");
  http2pref = prefs.getBoolPref("network.http.spdy.enabled.http2");
  tlspref = prefs.getBoolPref("network.http.spdy.enforce-tls-profile");
  altsvcpref1 = prefs.getBoolPref("network.http.altsvc.enabled");
  altsvcpref2 = prefs.getBoolPref("network.http.altsvc.oe", true);

  prefs.setBoolPref("network.http.spdy.enabled", true);
  prefs.setBoolPref("network.http.spdy.enabled.v3-1", true);
  prefs.setBoolPref("network.http.spdy.allow-push", true);
  prefs.setBoolPref("network.http.spdy.enabled.http2draft", true);
  prefs.setBoolPref("network.http.spdy.enabled.http2", true);
  prefs.setBoolPref("network.http.spdy.enforce-tls-profile", false);
  prefs.setBoolPref("network.http.altsvc.enabled", true);
  prefs.setBoolPref("network.http.altsvc.oe", true);

  loadGroup = Cc["@mozilla.org/network/load-group;1"].createInstance(Ci.nsILoadGroup);

  httpserv = new HttpServer();
  httpserv.registerPathHandler("/altsvc1", altsvcHttp1Server);
  httpserv.start(-1);

  httpserv2 = new HttpServer();
  httpserv2.registerPathHandler("/altsvc2", altsvcHttp1Server2);
  httpserv2.start(-1);

  // And make go!
  run_next_test();
}
