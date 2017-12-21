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
    if (request.getResponseHeader("X-Firefox-Spdy") == "h2") {
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
  accum : 0,
  expected: -1,
  shouldSucceed: true,

  onStartRequest: function testOnStartRequest(request, ctx) {
    this.onStartRequestFired = true;
    if (this.shouldSucceed && !Components.isSuccessCode(request.status)) {
      do_throw("Channel should have a success code! (" + request.status + ")");
    } else if (!this.shouldSucceed && Components.isSuccessCode(request.status)) {
      do_throw("Channel succeeded unexpectedly!");
    }

    Assert.ok(request instanceof Components.interfaces.nsIHttpChannel);
    Assert.equal(request.requestSucceeded, this.shouldSucceed);
    if (this.shouldSucceed) {
      Assert.equal(request.responseStatus, 200);
    }
  },

  onDataAvailable: function testOnDataAvailable(request, ctx, stream, off, cnt) {
    this.onDataAvailableFired = true;
    this.isHttp2Connection = checkIsHttp2(request);
    this.accum += cnt;
    read_stream(stream, cnt);
  },

  onStopRequest: function testOnStopRequest(request, ctx, status) {
    Assert.ok(this.onStartRequestFired);
    if (this.expected != -1) {
      Assert.equal(this.accum, this.expected);
    }
    if (this.shouldSucceed) {
      Assert.ok(Components.isSuccessCode(status));
      Assert.ok(this.onDataAvailableFired);
      Assert.ok(this.isHttp2Connection == this.shouldBeHttp2);
    } else {
      Assert.ok(!Components.isSuccessCode(status));
    }

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
    Assert.notEqual(completed_channels[0].streamID, completed_channels[1].streamID);
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
  Assert.ok(this.onStartRequestFired);
  Assert.ok(this.onDataAvailableFired);
  Assert.ok(this.isHttp2Connection);
  Assert.ok(this.buffer == multiplexContent);

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
  Assert.notEqual(hvalue, "");
  this.callback(hvalue);
  read_stream(stream, cnt);
};

var Http2PushListener = function(shouldBePushed) {
  this.shouldBePushed = shouldBePushed;
};

Http2PushListener.prototype = new Http2CheckListener();

Http2PushListener.prototype.onDataAvailable = function(request, ctx, stream, off, cnt) {
  this.onDataAvailableFired = true;
  this.isHttp2Connection = checkIsHttp2(request);
  if (request.originalURI.spec == "https://localhost:" + serverPort + "/push.js"  ||
      request.originalURI.spec == "https://localhost:" + serverPort + "/push2.js" ||
      request.originalURI.spec == "https://localhost:" + serverPort + "/push5.js") {
    Assert.equal(request.getResponseHeader("pushed"), this.shouldBePushed ? "yes" : "no");
  }
  read_stream(stream, cnt);
};

const pushHdrTxt = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
const pullHdrTxt = pushHdrTxt.split('').reverse().join('');

function checkContinuedHeaders(getHeader, headerPrefix, headerText) {
  for (var i = 0; i < 265; i++) {
    Assert.equal(getHeader(headerPrefix + 1), headerText);
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
  Assert.ok(this.onStartRequestFired);
  Assert.ok(Components.isSuccessCode(status));
  Assert.ok(this.onDataAvailableFired);
  Assert.ok(this.isHttp2Connection);

  --this.onStopsLeft;
  if (this.onStopsLeft === 0) {
    run_next_test();
    do_test_finished();
  }
};

Http2ContinuedHeaderListener.prototype.onPush = function(associatedChannel, pushChannel) {
  Assert.equal(associatedChannel.originalURI.spec, "https://localhost:" + serverPort + "/continuedheaders");
  Assert.equal(pushChannel.getRequestHeader("x-pushed-request"), "true");
  checkContinuedHeaders(pushChannel.getRequestHeader, "X-Push-Test-Header-", pushHdrTxt);

  pushChannel.asyncOpen2(this);
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
  Assert.equal(bigListenerMD5, request.getResponseHeader("X-Expected-MD5"));
};

Http2BigListener.prototype.onStopRequest = function(request, ctx, status) {
  Assert.ok(this.onStartRequestFired);
  Assert.ok(this.onDataAvailableFired);
  Assert.ok(this.isHttp2Connection);

  // Don't want to flood output, so don't use do_check_eq
  Assert.ok(this.buffer == bigListenerData);

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
  Assert.ok(this.onStartRequestFired);
  Assert.ok(this.onDataAvailableFired);
  Assert.ok(this.isHttp2Connection);
  Assert.equal(this.count, 1024 * 1024 * 1); // 1mb of data expected
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
  Assert.equal(this.expected_md5, request.getResponseHeader("X-Calculated-MD5"));
};

function makeChan(url) {
  return NetUtil.newChannel({uri: url, loadUsingSystemPrincipal: true})
         .QueryInterface(Ci.nsIHttpChannel);
}

var ResumeStalledChannelListener = function() {};

ResumeStalledChannelListener.prototype = {
  onStartRequestFired: false,
  onDataAvailableFired: false,
  isHttp2Connection: false,
  shouldBeHttp2 : true,
  resumable : null,

  onStartRequest: function testOnStartRequest(request, ctx) {
    this.onStartRequestFired = true;
    if (!Components.isSuccessCode(request.status))
      do_throw("Channel should have a success code! (" + request.status + ")");

    Assert.ok(request instanceof Components.interfaces.nsIHttpChannel);
    Assert.equal(request.responseStatus, 200);
    Assert.equal(request.requestSucceeded, true);
  },

  onDataAvailable: function testOnDataAvailable(request, ctx, stream, off, cnt) {
    this.onDataAvailableFired = true;
    this.isHttp2Connection = checkIsHttp2(request);
    read_stream(stream, cnt);
  },

  onStopRequest: function testOnStopRequest(request, ctx, status) {
    Assert.ok(this.onStartRequestFired);
    Assert.ok(Components.isSuccessCode(status));
    Assert.ok(this.onDataAvailableFired);
    Assert.ok(this.isHttp2Connection == this.shouldBeHttp2);
    this.resumable.resume();
  }
};

// test a large download that creates stream flow control and
// confirm we can do another independent stream while the download
// stream is stuck
function test_http2_blocking_download() {
  var chan = makeChan("https://localhost:" + serverPort + "/bigdownload");
  var internalChannel = chan.QueryInterface(Ci.nsIHttpChannelInternal);
  internalChannel.initialRwin = 500000; // make the stream.suspend push back in h2
  var listener = new Http2CheckListener();
  listener.expected = 3 * 1024 * 1024;
  chan.asyncOpen2(listener);
  chan.suspend();
  // wait 5 seconds so that stream flow control kicks in and then see if we
  // can do a basic transaction (i.e. session not blocked). afterwards resume
  // channel
  do_timeout(5000, function() {
      var simpleChannel = makeChan("https://localhost:" + serverPort + "/");
      var sl = new ResumeStalledChannelListener();
      sl.resumable = chan;
      simpleChannel.asyncOpen2(sl);
  });
}

// Make sure we make a HTTP2 connection and both us and the server mark it as such
function test_http2_basic() {
  var chan = makeChan("https://localhost:" + serverPort + "/");
  var listener = new Http2CheckListener();
  chan.asyncOpen2(listener);
}

function test_http2_basic_unblocked_dep() {
  var chan = makeChan("https://localhost:" + serverPort + "/basic_unblocked_dep");
  var cos = chan.QueryInterface(Ci.nsIClassOfService);
  cos.addClassFlags(Ci.nsIClassOfService.Unblocked);
  var listener = new Http2CheckListener();
  chan.asyncOpen2(listener);
}

// make sure we don't use h2 when disallowed
function test_http2_nospdy() {
  var chan = makeChan("https://localhost:" + serverPort + "/");
  var listener = new Http2CheckListener();
  var internalChannel = chan.QueryInterface(Ci.nsIHttpChannelInternal);
  internalChannel.allowSpdy = false;
  listener.shouldBeHttp2 = false;
  chan.asyncOpen2(listener);
}

// Support for making sure XHR works over SPDY
function checkXhr(xhr) {
  if (xhr.readyState != 4) {
    return;
  }

  Assert.equal(xhr.status, 200);
  Assert.equal(checkIsHttp2(xhr), true);
  run_next_test();
  do_test_finished();
}

// Fires off an XHR request over h2
function test_http2_xhr() {
  var req = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"]
            .createInstance(Ci.nsIXMLHttpRequest);
  req.open("GET", "https://localhost:" + serverPort + "/", true);
  req.addEventListener("readystatechange", function (evt) { checkXhr(req); });
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
  Assert.ok(this.isHttp2Connection);
  if (this.recvdHdr > 0) {
    Assert.equal(request.getResponseHeader("X-Recvd"), this.recvdHdr);
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
    concurrent_channels[i].asyncOpen2(concurrent_listener);
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
    concurrent_channels[i].asyncOpen2(concurrent_listener);
  }
}

// Test to make sure we get multiplexing right
function test_http2_multiplex() {
  var chan1 = makeChan("https://localhost:" + serverPort + "/multiplex1");
  var chan2 = makeChan("https://localhost:" + serverPort + "/multiplex2");
  var listener1 = new Http2MultiplexListener();
  var listener2 = new Http2MultiplexListener();
  chan1.asyncOpen2(listener1);
  chan2.asyncOpen2(listener2);
}

// Test to make sure we gateway non-standard headers properly
function test_http2_header() {
  var chan = makeChan("https://localhost:" + serverPort + "/header");
  var hvalue = "Headers are fun";
  chan.setRequestHeader("X-Test-Header", hvalue, false);
  var listener = new Http2HeaderListener("X-Received-Test-Header", function(received_hvalue) {
    Assert.equal(received_hvalue, hvalue);
  });
  chan.asyncOpen2(listener);
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
    Assert.equal(cookiesReceived.length, cookiesSent.length);
    cookiesReceived.forEach(function(cookieReceived, index) {
      Assert.equal(cookiesSent[index], cookieReceived)
    });
  });
  chan.asyncOpen2(listener);
}

function test_http2_push1() {
  var chan = makeChan("https://localhost:" + serverPort + "/push");
  chan.loadGroup = loadGroup;
  var listener = new Http2PushListener(true);
  chan.asyncOpen2(listener);
}

function test_http2_push2() {
  var chan = makeChan("https://localhost:" + serverPort + "/push.js");
  chan.loadGroup = loadGroup;
  var listener = new Http2PushListener(true);
  chan.asyncOpen2(listener);
}

function test_http2_push3() {
  var chan = makeChan("https://localhost:" + serverPort + "/push2");
  chan.loadGroup = loadGroup;
  var listener = new Http2PushListener(true);
  chan.asyncOpen2(listener);
}

function test_http2_push4() {
  var chan = makeChan("https://localhost:" + serverPort + "/push2.js");
  chan.loadGroup = loadGroup;
  var listener = new Http2PushListener(true);
  chan.asyncOpen2(listener);
}

function test_http2_push5() {
  var chan = makeChan("https://localhost:" + serverPort + "/push5");
  chan.loadGroup = loadGroup;
  var listener = new Http2PushListener(true);
  chan.asyncOpen2(listener);
}

function test_http2_push6() {
  var chan = makeChan("https://localhost:" + serverPort + "/push5.js");
  chan.loadGroup = loadGroup;
  var listener = new Http2PushListener(true);
  chan.asyncOpen2(listener);
}

// this is a basic test where the server sends a simple document with 2 header
// blocks. bug 1027364
function test_http2_doubleheader() {
  var chan = makeChan("https://localhost:" + serverPort + "/doubleheader");
  var listener = new Http2CheckListener();
  chan.asyncOpen2(listener);
}

// Make sure we handle GETs that cover more than 2 frames properly
function test_http2_big() {
  var chan = makeChan("https://localhost:" + serverPort + "/big");
  var listener = new Http2BigListener();
  chan.asyncOpen2(listener);
}

function test_http2_huge_suspended() {
  var chan = makeChan("https://localhost:" + serverPort + "/huge");
  var listener = new Http2HugeSuspendedListener();
  chan.asyncOpen2(listener);
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

  chan.asyncOpen2(listener);
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
Cu.import("resource://gre/modules/NetUtil.jsm");

var httpserv = null;
var httpserv2 = null;
var ios = Components.classes["@mozilla.org/network/io-service;1"]
                    .getService(Components.interfaces.nsIIOService);

var altsvcClientListener = {
  onStartRequest: function test_onStartR(request, ctx) {
    Assert.equal(request.status, Components.results.NS_OK);
  },

  onDataAvailable: function test_ODA(request, cx, stream, offset, cnt) {
   read_stream(stream, cnt);
  },

  onStopRequest: function test_onStopR(request, ctx, status) {
    var isHttp2Connection = checkIsHttp2(request.QueryInterface(Components.interfaces.nsIHttpChannel));
    if (!isHttp2Connection) {
      dump("/altsvc1 not over h2 yet - retry\n");
      var chan = makeChan("http://foo.example.com:" + httpserv.identity.primaryPort + "/altsvc1")
                .QueryInterface(Components.interfaces.nsIHttpChannel);
      // we use this header to tell the server to issue a altsvc frame for the
      // speficied origin we will use in the next part of the test
      chan.setRequestHeader("x-redirect-origin",
                 "http://foo.example.com:" + httpserv2.identity.primaryPort, false);
      chan.loadFlags = Ci.nsIRequest.LOAD_BYPASS_CACHE;
      chan.asyncOpen2(altsvcClientListener);
    } else {
      Assert.ok(isHttp2Connection);
      var chan = makeChan("http://foo.example.com:" + httpserv2.identity.primaryPort + "/altsvc2")
                .QueryInterface(Components.interfaces.nsIHttpChannel);
      chan.loadFlags = Ci.nsIRequest.LOAD_BYPASS_CACHE;
      chan.asyncOpen2(altsvcClientListener2);
    }
  }
};

var altsvcClientListener2 = {
  onStartRequest: function test_onStartR(request, ctx) {
    Assert.equal(request.status, Components.results.NS_OK);
  },

  onDataAvailable: function test_ODA(request, cx, stream, offset, cnt) {
   read_stream(stream, cnt);
  },

  onStopRequest: function test_onStopR(request, ctx, status) {
    var isHttp2Connection = checkIsHttp2(request.QueryInterface(Components.interfaces.nsIHttpChannel));
    if (!isHttp2Connection) {
      dump("/altsvc2 not over h2 yet - retry\n");
      var chan = makeChan("http://foo.example.com:" + httpserv2.identity.primaryPort + "/altsvc2")
                .QueryInterface(Components.interfaces.nsIHttpChannel);
      chan.loadFlags = Ci.nsIRequest.LOAD_BYPASS_CACHE;
      chan.asyncOpen2(altsvcClientListener2);
    } else {
      Assert.ok(isHttp2Connection);
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

function h1ServerWK(metadata, response) {
  response.setStatusLine(metadata.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "application/json", false);
  response.setHeader("Connection", "close", false);
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Access-Control-Allow-Origin", "*", false);
  response.setHeader("Access-Control-Allow-Method", "GET", false);

  var body = '{"http://foo.example.com:' + httpserv.identity.primaryPort + '": { "tls-ports": [' + serverPort + '] }}';
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

function h1ServerWK2(metadata, response) {
  response.setStatusLine(metadata.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "application/json", false);
  response.setHeader("Connection", "close", false);
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Access-Control-Allow-Origin", "*", false);
  response.setHeader("Access-Control-Allow-Method", "GET", false);

  var body = '{"http://foo.example.com:' + httpserv2.identity.primaryPort + '": { "tls-ports": [' + serverPort + '] }}';
  response.bodyOutputStream.write(body, body.length);
}
function test_http2_altsvc() {
  var chan = makeChan("http://foo.example.com:" + httpserv.identity.primaryPort + "/altsvc1")
           .QueryInterface(Components.interfaces.nsIHttpChannel);
  chan.asyncOpen2(altsvcClientListener);
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
    Assert.equal(associatedChannel.originalURI.spec, "https://localhost:" + serverPort + "/pushapi1");
    Assert.equal (pushChannel.getRequestHeader("x-pushed-request"), "true");

    pushChannel.asyncOpen2(this);
    if (pushChannel.originalURI.spec == "https://localhost:" + serverPort + "/pushapi1/2") {
      pushChannel.cancel(Components.results.NS_ERROR_ABORT);
    } else if (pushChannel.originalURI.spec == "https://localhost:" + serverPort + "/pushapi1/3") {
      Assert.ok(pushChannel.getRequestHeader("Accept-Encoding").includes("br"));
    }
  },

 // normal Channel listeners
  onStartRequest: function pushAPIOnStart(request, ctx) {
  },

  onDataAvailable: function pushAPIOnDataAvailable(request, ctx, stream, offset, cnt) {
    Assert.notEqual(request.originalURI.spec, "https://localhost:" + serverPort + "/pushapi1/2");

    var data = read_stream(stream, cnt);

    if (request.originalURI.spec == "https://localhost:" + serverPort + "/pushapi1") {
      Assert.equal(data[0], '0');
      --this.checksPending;
    } else if (request.originalURI.spec == "https://localhost:" + serverPort + "/pushapi1/1") {
      Assert.equal(data[0], '1');
      --this.checksPending; // twice
    } else if (request.originalURI.spec == "https://localhost:" + serverPort + "/pushapi1/3") {
      Assert.equal(data[0], '3');
      --this.checksPending;
    } else {
      Assert.equal(true, false);
    }
  },

  onStopRequest: function test_onStopR(request, ctx, status) {
    if (request.originalURI.spec == "https://localhost:" + serverPort + "/pushapi1/2") {
      Assert.equal(request.status, Components.results.NS_ERROR_ABORT);
    } else {
      Assert.equal(request.status, Components.results.NS_OK);
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
// 5 to see /pushapi1/3 with 3 with brotli

function test_http2_pushapi_1() {
  var chan = makeChan("https://localhost:" + serverPort + "/pushapi1");
  chan.loadGroup = loadGroup;
  var listener = new Http2PushApiListener();
  chan.notificationCallbacks = listener;
  chan.asyncOpen2(listener);
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
  chan.asyncOpen2(listener);
}

function test_http2_h11required_stream() {
  var chan = makeChan("https://localhost:" + serverPort + "/h11required_stream");
  var listener = new Http2CheckListener();
  listener.shouldBeHttp2 = false;
  chan.asyncOpen2(listener);
}

function H11RequiredSessionListener () { }
H11RequiredSessionListener.prototype = new Http2CheckListener();

H11RequiredSessionListener.prototype.onStopRequest = function (request, ctx, status) {
  var streamReused = request.getResponseHeader("X-H11Required-Stream-Ok");
  Assert.equal(streamReused, "yes");

  Assert.ok(this.onStartRequestFired);
  Assert.ok(this.onDataAvailableFired);
  Assert.ok(this.isHttp2Connection == this.shouldBeHttp2);

  run_next_test();
  do_test_finished();
};

function test_http2_h11required_session() {
  var chan = makeChan("https://localhost:" + serverPort + "/h11required_session");
  var listener = new H11RequiredSessionListener();
  listener.shouldBeHttp2 = false;
  chan.asyncOpen2(listener);
}

function test_http2_retry_rst() {
  var chan = makeChan("https://localhost:" + serverPort + "/rstonce");
  var listener = new Http2CheckListener();
  chan.asyncOpen2(listener);
}

function test_http2_continuations() {
  var chan = makeChan("https://localhost:" + serverPort + "/continuedheaders");
  chan.loadGroup = loadGroup;
  var listener = new Http2ContinuedHeaderListener();
  chan.notificationCallbacks = listener;
  chan.asyncOpen2(listener);
}

function Http2IllegalHpackValidationListener() { }
Http2IllegalHpackValidationListener.prototype = new Http2CheckListener();
Http2IllegalHpackValidationListener.prototype.shouldGoAway = false;

Http2IllegalHpackValidationListener.prototype.onStopRequest = function (request, ctx, status) {
  var wentAway = (request.getResponseHeader('X-Did-Goaway') === 'yes');
  Assert.equal(wentAway, this.shouldGoAway);

  Assert.ok(this.onStartRequestFired);
  Assert.ok(this.onDataAvailableFired);
  Assert.ok(this.isHttp2Connection == this.shouldBeHttp2);

  run_next_test();
  do_test_finished();
};

function Http2IllegalHpackListener() { }
Http2IllegalHpackListener.prototype = new Http2CheckListener();
Http2IllegalHpackListener.prototype.shouldGoAway = false;

Http2IllegalHpackListener.prototype.onStopRequest = function (request, ctx, status) {
  var chan = makeChan("https://localhost:" + serverPort + "/illegalhpack_validate");
  var listener = new Http2IllegalHpackValidationListener();
  listener.shouldGoAway = this.shouldGoAway;
  chan.asyncOpen2(listener);
};

function test_http2_illegalhpacksoft() {
  var chan = makeChan("https://localhost:" + serverPort + "/illegalhpacksoft");
  var listener = new Http2IllegalHpackListener();
  listener.shouldGoAway = false;
  listener.shouldSucceed = false;
  chan.asyncOpen2(listener);
}

function test_http2_illegalhpackhard() {
  var chan = makeChan("https://localhost:" + serverPort + "/illegalhpackhard");
  var listener = new Http2IllegalHpackListener();
  listener.shouldGoAway = true;
  listener.shouldSucceed = false;
  chan.asyncOpen2(listener);
}

function test_http2_folded_header() {
  var chan = makeChan("https://localhost:" + serverPort + "/foldedheader");
  chan.loadGroup = loadGroup;
  var listener = new Http2CheckListener();
  listener.shouldSucceed = false;
  chan.asyncOpen2(listener);
}

function test_http2_empty_data() {
  var chan = makeChan("https://localhost:" + serverPort + "/emptydata");
  var listener = new Http2CheckListener();
  chan.asyncOpen2(listener);
}

function test_http2_push_firstparty1() {
  var chan = makeChan("https://localhost:" + serverPort + "/push");
  chan.loadGroup = loadGroup;
  chan.loadInfo.originAttributes = { firstPartyDomain: "foo.com" };
  var listener = new Http2PushListener(true);
  chan.asyncOpen2(listener);
}

function test_http2_push_firstparty2() {
  var chan = makeChan("https://localhost:" + serverPort + "/push.js");
  chan.loadGroup = loadGroup;
  chan.loadInfo.originAttributes = { firstPartyDomain: "bar.com" };
  var listener = new Http2PushListener(false);
  chan.asyncOpen2(listener);
}

function test_http2_push_firstparty3() {
  var chan = makeChan("https://localhost:" + serverPort + "/push.js");
  chan.loadGroup = loadGroup;
  chan.loadInfo.originAttributes = { firstPartyDomain: "foo.com" };
  var listener = new Http2PushListener(true);
  chan.asyncOpen2(listener);
}

function test_http2_push_userContext1() {
  var chan = makeChan("https://localhost:" + serverPort + "/push");
  chan.loadGroup = loadGroup;
  chan.loadInfo.originAttributes = { userContextId: 1 };
  var listener = new Http2PushListener(true);
  chan.asyncOpen2(listener);
}

function test_http2_push_userContext2() {
  var chan = makeChan("https://localhost:" + serverPort + "/push.js");
  chan.loadGroup = loadGroup;
  chan.loadInfo.originAttributes = { userContextId: 2 };
  var listener = new Http2PushListener(false);
  chan.asyncOpen2(listener);
}

function test_http2_push_userContext3() {
  var chan = makeChan("https://localhost:" + serverPort + "/push.js");
  chan.loadGroup = loadGroup;
  chan.loadInfo.originAttributes = { userContextId: 1 };
  var listener = new Http2PushListener(true);
  chan.asyncOpen2(listener);
}

function test_http2_status_phrase() {
  var chan = makeChan("https://localhost:" + serverPort + "/statusphrase");
  var listener = new Http2CheckListener();
  listener.shouldSucceed = false;
  chan.asyncOpen2(listener);
}

var PulledDiskCacheListener = function() {};
PulledDiskCacheListener.prototype = new Http2CheckListener();
PulledDiskCacheListener.prototype.EXPECTED_DATA = "this was pulled via h2";
PulledDiskCacheListener.prototype.readData = "";
PulledDiskCacheListener.prototype.onDataAvailable = function testOnDataAvailable(request, ctx, stream, off, cnt) {
  this.onDataAvailableFired = true;
  this.isHttp2Connection = checkIsHttp2(request);
  this.accum += cnt;
  this.readData += read_stream(stream, cnt);
};
PulledDiskCacheListener.prototype.onStopRequest = function testOnStopRequest(request, ctx, status) {
  Assert.equal(this.EXPECTED_DATA, this.readData);
  Http2CheckListener.prorotype.onStopRequest.call(this, request, ctx, status);
};

const DISK_CACHE_DATA = "this is from disk cache";

var FromDiskCacheListener = function() {};
FromDiskCacheListener.prototype = {
  onStartRequestFired: false,
  onDataAvailableFired: false,
  readData: "",

  onStartRequest: function testOnStartRequest(request, ctx) {
    this.onStartRequestFired = true;
    if (!Components.isSuccessCode(request.status)) {
      do_throw("Channel should have a success code! (" + request.status + ")");
    }

    Assert.ok(request instanceof Components.interfaces.nsIHttpChannel);
    Assert.ok(request.requestSucceeded);
    Assert.equal(request.responseStatus, 200);
  },

  onDataAvailable: function testOnDataAvailable(request, ctx, stream, off, cnt) {
    this.onDataAvailableFired = true;
    this.readData += read_stream(stream, cnt);
  },

  onStopRequest: function testOnStopRequest(request, ctx, status) {
    Assert.ok(this.onStartRequestFired);
    Assert.ok(Components.isSuccessCode(status));
    Assert.ok(this.onDataAvailableFired);
    Assert.equal(this.readData, DISK_CACHE_DATA);

    evict_cache_entries("disk");
    syncWithCacheIOThread(() => {
      // Now that we know the entry is out of the disk cache, check to make sure
      // we don't have this hiding in the push cache somewhere - if we do, it
      // didn't get cancelled, and we have a bug.
      var chan = makeChan("https://localhost:" + serverPort + "/diskcache");
      chan.listener = new PulledDiskCacheListener();
      chan.loadGroup = loadGroup;
      chan.asyncOpen2(listener);
    });
  }
};

var Http2DiskCachePushListener = function() {};

Http2DiskCachePushListener.prototype = new Http2CheckListener();

Http2DiskCachePushListener.onStopRequest = function(request, ctx, status) {
    Assert.ok(this.onStartRequestFired);
    Assert.ok(Components.isSuccessCode(status));
    Assert.ok(this.onDataAvailableFired);
    Assert.ok(this.isHttp2Connection == this.shouldBeHttp2);

    // Now we need to open a channel to ensure we get data from the disk cache
    // for the pushed item, instead of from the push cache.
    var chan = makeChan("https://localhost:" + serverPort + "/diskcache");
    chan.listener = new FromDiskCacheListener();
    chan.loadGroup = loadGroup;
    chan.asyncOpen2(listener);
};

function continue_test_http2_disk_cache_push(status, entry, appCache) {
  // TODO - store stuff in cache entry, then open an h2 channel that will push
  // this, once that completes, open a channel for the cache entry we made and
  // ensure it came from disk cache, not the push cache.
  var outputStream = entry.openOutputStream(0);
  outputStream.write(DISK_CACHE_DATA, DISK_CACHE_DATA.length);

  // Now we open our URL that will push data for the URL above
  var chan = makeChan("https://localhost:" + serverPort + "/pushindisk");
  var listener = new Http2DiskCachePushListener();
  chan.loadGroup = loadGroup;
  chan.asyncOpen2(listener);
}

function test_http2_disk_cache_push() {
  asyncOpenCacheEntry("https://localhost:" + serverPort + "/diskcache",
                      "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
                      continue_test_http2_disk_cache_push, false);
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

var Http2DoublepushListener = function () {};
Http2DoublepushListener.prototype = new Http2CheckListener();
Http2DoublepushListener.prototype.onStopRequest = function (request, ctx, status) {
  Assert.ok(this.onStartRequestFired);
  Assert.ok(Components.isSuccessCode(status));
  Assert.ok(this.onDataAvailableFired);
  Assert.ok(this.isHttp2Connection == this.shouldBeHttp2);

  var chan = makeChan("https://localhost:" + serverPort + "/doublypushed");
  var listener = new Http2DoublypushedListener();
  chan.loadGroup = loadGroup;
  chan.asyncOpen2(listener);
};

var Http2DoublypushedListener = function () {};
Http2DoublypushedListener.prototype = new Http2CheckListener();
Http2DoublypushedListener.prototype.readData = "";
Http2DoublypushedListener.prototype.onDataAvailable = function (request, ctx, stream, off, cnt) {
  this.onDataAvailableFired = true;
  this.accum += cnt;
  this.readData += read_stream(stream, cnt);
};
Http2DoublypushedListener.prototype.onStopRequest = function (request, ctx, status) {
  Assert.ok(this.onStartRequestFired);
  Assert.ok(Components.isSuccessCode(status));
  Assert.ok(this.onDataAvailableFired);
  Assert.equal(this.readData, "pushed");

  run_next_test();
  do_test_finished();
};

function test_http2_doublepush() {
  var chan = makeChan("https://localhost:" + serverPort + "/doublepush");
  var listener = new Http2DoublepushListener();
  chan.loadGroup = loadGroup;
  chan.asyncOpen2(listener);
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
            , test_http2_push5
            , test_http2_push6
            , test_http2_altsvc
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
            , test_http2_blocking_download
            , test_http2_illegalhpacksoft
            , test_http2_illegalhpackhard
            , test_http2_folded_header
            , test_http2_empty_data
            , test_http2_status_phrase
            , test_http2_doublepush
            , test_http2_disk_cache_push
            // Add new tests above here - best to add new tests before h1
            // streams get too involved
            // These next two must always come in this order
            , test_http2_h11required_stream
            , test_http2_h11required_session
            , test_http2_retry_rst
            , test_http2_wrongsuite
            , test_http2_push_firstparty1
            , test_http2_push_firstparty2
            , test_http2_push_firstparty3
            , test_http2_push_userContext1
            , test_http2_push_userContext2
            , test_http2_push_userContext3

            // cleanup
            , test_complete
            ];
var current_test = 0;

function run_next_test() {
  if (current_test < tests.length) {
    dump("starting test number " + current_test + "\n");
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
var spdypush;
var http2pref;
var altsvcpref1;
var altsvcpref2;
var loadGroup;
var serverPort;
var speculativeLimit;

function resetPrefs() {
  prefs.setIntPref("network.http.speculative-parallel-limit", speculativeLimit);
  prefs.setBoolPref("network.http.spdy.enabled", spdypref);
  prefs.setBoolPref("network.http.spdy.allow-push", spdypush);
  prefs.setBoolPref("network.http.spdy.enabled.http2", http2pref);
  prefs.setBoolPref("network.http.altsvc.enabled", altsvcpref1);
  prefs.setBoolPref("network.http.altsvc.oe", altsvcpref2);
  prefs.clearUserPref("network.dns.localDomains");
}

function run_test() {
  var env = Cc["@mozilla.org/process/environment;1"].getService(Ci.nsIEnvironment);
  serverPort = env.get("MOZHTTP2_PORT");
  Assert.notEqual(serverPort, null);
  dump("using port " + serverPort + "\n");

  // Set to allow the cert presented by our H2 server
  do_get_profile();
  prefs = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);
  speculativeLimit = prefs.getIntPref("network.http.speculative-parallel-limit");
  prefs.setIntPref("network.http.speculative-parallel-limit", 0);

  // The moz-http2 cert is for foo.example.com and is signed by CA.cert.der
  // so add that cert to the trust list as a signing cert. Some older tests in
  // this suite use localhost with a TOFU exception, but new ones should use
  // foo.example.com
  let certdb = Cc["@mozilla.org/security/x509certdb;1"]
                  .getService(Ci.nsIX509CertDB);
  addCertFromFile(certdb, "CA.cert.der", "CTu,u,u");

  addCertOverride("localhost", serverPort,
                  Ci.nsICertOverrideService.ERROR_UNTRUSTED |
                  Ci.nsICertOverrideService.ERROR_MISMATCH |
                  Ci.nsICertOverrideService.ERROR_TIME);

  // Enable all versions of spdy to see that we auto negotiate http/2
  spdypref = prefs.getBoolPref("network.http.spdy.enabled");
  spdypush = prefs.getBoolPref("network.http.spdy.allow-push");
  http2pref = prefs.getBoolPref("network.http.spdy.enabled.http2");
  altsvcpref1 = prefs.getBoolPref("network.http.altsvc.enabled");
  altsvcpref2 = prefs.getBoolPref("network.http.altsvc.oe", true);

  prefs.setBoolPref("network.http.spdy.enabled", true);
  prefs.setBoolPref("network.http.spdy.allow-push", true);
  prefs.setBoolPref("network.http.spdy.enabled.http2", true);
  prefs.setBoolPref("network.http.altsvc.enabled", true);
  prefs.setBoolPref("network.http.altsvc.oe", true);
  prefs.setCharPref("network.dns.localDomains", "foo.example.com, bar.example.com");

  loadGroup = Cc["@mozilla.org/network/load-group;1"].createInstance(Ci.nsILoadGroup);

  httpserv = new HttpServer();
  httpserv.registerPathHandler("/altsvc1", altsvcHttp1Server);
  httpserv.registerPathHandler("/.well-known/http-opportunistic", h1ServerWK);
  httpserv.start(-1);
  httpserv.identity.setPrimary("http", "foo.example.com", httpserv.identity.primaryPort);

  httpserv2 = new HttpServer();
  httpserv2.registerPathHandler("/altsvc2", altsvcHttp1Server2);
  httpserv2.registerPathHandler("/.well-known/http-opportunistic", h1ServerWK2);
  httpserv2.start(-1);
  httpserv2.identity.setPrimary("http", "foo.example.com", httpserv2.identity.primaryPort);

  // And make go!
  run_next_test();
}

function readFile(file) {
  let fstream = Cc["@mozilla.org/network/file-input-stream;1"]
                  .createInstance(Ci.nsIFileInputStream);
  fstream.init(file, -1, 0, 0);
  let data = NetUtil.readInputStreamToString(fstream, fstream.available());
  fstream.close();
  return data;
}

function addCertFromFile(certdb, filename, trustString) {
  let certFile = do_get_file(filename, false);
  let der = readFile(certFile);
  certdb.addCert(der, trustString);
}
