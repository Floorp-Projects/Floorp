// test HTTP/2

"use strict";

/* import-globals-from head_channels.js */

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
var md5s = [
  "f1b708bba17f1ce948dc979f4d7092bc",
  "2ef8d3b6c8f329318eb1a119b12622b6",
];

var bigListenerData = generateContent(128 * 1024);
var bigListenerMD5 = "8f607cfdd2c87d6a7eedb657dafbd836";

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

var Http2CheckListener = function () {};

Http2CheckListener.prototype = {
  onStartRequestFired: false,
  onDataAvailableFired: false,
  isHttp2Connection: false,
  shouldBeHttp2: true,
  accum: 0,
  expected: -1,
  shouldSucceed: true,

  onStartRequest: function testOnStartRequest(request) {
    this.onStartRequestFired = true;
    if (this.shouldSucceed && !Components.isSuccessCode(request.status)) {
      do_throw("Channel should have a success code! (" + request.status + ")");
    } else if (
      !this.shouldSucceed &&
      Components.isSuccessCode(request.status)
    ) {
      do_throw("Channel succeeded unexpectedly!");
    }

    Assert.ok(request instanceof Ci.nsIHttpChannel);
    Assert.equal(request.requestSucceeded, this.shouldSucceed);
    if (this.shouldSucceed) {
      Assert.equal(request.responseStatus, 200);
    }
  },

  onDataAvailable: function testOnDataAvailable(request, stream, off, cnt) {
    this.onDataAvailableFired = true;
    this.isHttp2Connection = checkIsHttp2(request);
    this.accum += cnt;
    read_stream(stream, cnt);
  },

  onStopRequest: function testOnStopRequest(request, status) {
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
    request.QueryInterface(Ci.nsIProxiedChannel);
    var httpProxyConnectResponseCode = request.httpProxyConnectResponseCode;
    this.finish({ httpProxyConnectResponseCode });
  },
};

/*
 * Support for testing valid multiplexing of streams
 */

var multiplexContent = generateContent(30 * 1024);

/* Listener class to control the testing of multiplexing */
var Http2MultiplexListener = function () {};

Http2MultiplexListener.prototype = new Http2CheckListener();

Http2MultiplexListener.prototype.streamID = 0;
Http2MultiplexListener.prototype.buffer = "";

Http2MultiplexListener.prototype.onDataAvailable = function (
  request,
  stream,
  off,
  cnt
) {
  this.onDataAvailableFired = true;
  this.isHttp2Connection = checkIsHttp2(request);
  this.streamID = parseInt(request.getResponseHeader("X-Http2-StreamID"));
  var data = read_stream(stream, cnt);
  this.buffer = this.buffer.concat(data);
};

Http2MultiplexListener.prototype.onStopRequest = function (request) {
  Assert.ok(this.onStartRequestFired);
  Assert.ok(this.onDataAvailableFired);
  Assert.ok(this.isHttp2Connection);
  Assert.ok(this.buffer == multiplexContent);

  request.QueryInterface(Ci.nsIProxiedChannel);
  // This is what does most of the hard work for us
  var httpProxyConnectResponseCode = request.httpProxyConnectResponseCode;
  var streamID = this.streamID;
  this.finish({ httpProxyConnectResponseCode, streamID });
};

// Does the appropriate checks for header gatewaying
var Http2HeaderListener = function (name, callback) {
  this.name = name;
  this.callback = callback;
};

Http2HeaderListener.prototype = new Http2CheckListener();
Http2HeaderListener.prototype.value = "";

Http2HeaderListener.prototype.onDataAvailable = function (
  request,
  stream,
  off,
  cnt
) {
  this.onDataAvailableFired = true;
  this.isHttp2Connection = checkIsHttp2(request);
  var hvalue = request.getResponseHeader(this.name);
  Assert.notEqual(hvalue, "");
  this.callback(hvalue);
  read_stream(stream, cnt);
};

var Http2PushListener = function (shouldBePushed) {
  this.shouldBePushed = shouldBePushed;
};

Http2PushListener.prototype = new Http2CheckListener();

Http2PushListener.prototype.onDataAvailable = function (
  request,
  stream,
  off,
  cnt
) {
  this.onDataAvailableFired = true;
  this.isHttp2Connection = checkIsHttp2(request);
  if (
    request.originalURI.spec ==
      `https://localhost:${this.serverPort}/push.js` ||
    request.originalURI.spec ==
      `https://localhost:${this.serverPort}/push2.js` ||
    request.originalURI.spec == `https://localhost:${this.serverPort}/push5.js`
  ) {
    Assert.equal(
      request.getResponseHeader("pushed"),
      this.shouldBePushed ? "yes" : "no"
    );
  }
  read_stream(stream, cnt);
};

const pushHdrTxt =
  "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
const pullHdrTxt = pushHdrTxt.split("").reverse().join("");

function checkContinuedHeaders(getHeader, headerPrefix, headerText) {
  for (var i = 0; i < 265; i++) {
    Assert.equal(getHeader(headerPrefix + 1), headerText);
  }
}

var Http2ContinuedHeaderListener = function () {};

Http2ContinuedHeaderListener.prototype = new Http2CheckListener();

Http2ContinuedHeaderListener.prototype.onStopsLeft = 2;

Http2ContinuedHeaderListener.prototype.QueryInterface = ChromeUtils.generateQI([
  "nsIHttpPushListener",
  "nsIStreamListener",
]);

Http2ContinuedHeaderListener.prototype.getInterface = function (aIID) {
  return this.QueryInterface(aIID);
};

Http2ContinuedHeaderListener.prototype.onDataAvailable = function (
  request,
  stream,
  off,
  cnt
) {
  this.onDataAvailableFired = true;
  this.isHttp2Connection = checkIsHttp2(request);
  if (
    request.originalURI.spec ==
    `https://localhost:${this.serverPort}/continuedheaders`
  ) {
    // This is the original request, so the only one where we'll have continued response headers
    checkContinuedHeaders(
      request.getResponseHeader,
      "X-Pull-Test-Header-",
      pullHdrTxt
    );
  }
  read_stream(stream, cnt);
};

Http2ContinuedHeaderListener.prototype.onStopRequest = function (
  request,
  status
) {
  Assert.ok(this.onStartRequestFired);
  Assert.ok(Components.isSuccessCode(status));
  Assert.ok(this.onDataAvailableFired);
  Assert.ok(this.isHttp2Connection);

  --this.onStopsLeft;
  if (this.onStopsLeft === 0) {
    request.QueryInterface(Ci.nsIProxiedChannel);
    var httpProxyConnectResponseCode = request.httpProxyConnectResponseCode;
    this.finish({ httpProxyConnectResponseCode });
  }
};

Http2ContinuedHeaderListener.prototype.onPush = function (
  associatedChannel,
  pushChannel
) {
  Assert.equal(
    associatedChannel.originalURI.spec,
    "https://localhost:" + this.serverPort + "/continuedheaders"
  );
  Assert.equal(pushChannel.getRequestHeader("x-pushed-request"), "true");
  checkContinuedHeaders(
    pushChannel.getRequestHeader,
    "X-Push-Test-Header-",
    pushHdrTxt
  );

  pushChannel.asyncOpen(this);
};

// Does the appropriate checks for a large GET response
var Http2BigListener = function () {};

Http2BigListener.prototype = new Http2CheckListener();
Http2BigListener.prototype.buffer = "";

Http2BigListener.prototype.onDataAvailable = function (
  request,
  stream,
  off,
  cnt
) {
  this.onDataAvailableFired = true;
  this.isHttp2Connection = checkIsHttp2(request);
  this.buffer = this.buffer.concat(read_stream(stream, cnt));
  // We know the server should send us the same data as our big post will be,
  // so the md5 should be the same
  Assert.equal(bigListenerMD5, request.getResponseHeader("X-Expected-MD5"));
};

Http2BigListener.prototype.onStopRequest = function (request) {
  Assert.ok(this.onStartRequestFired);
  Assert.ok(this.onDataAvailableFired);
  Assert.ok(this.isHttp2Connection);

  // Don't want to flood output, so don't use do_check_eq
  Assert.ok(this.buffer == bigListenerData);

  request.QueryInterface(Ci.nsIProxiedChannel);
  var httpProxyConnectResponseCode = request.httpProxyConnectResponseCode;
  this.finish({ httpProxyConnectResponseCode });
};

var Http2HugeSuspendedListener = function () {};

Http2HugeSuspendedListener.prototype = new Http2CheckListener();
Http2HugeSuspendedListener.prototype.count = 0;

Http2HugeSuspendedListener.prototype.onDataAvailable = function (
  request,
  stream,
  off,
  cnt
) {
  this.onDataAvailableFired = true;
  this.isHttp2Connection = checkIsHttp2(request);
  this.count += cnt;
  read_stream(stream, cnt);
};

Http2HugeSuspendedListener.prototype.onStopRequest = function (request) {
  Assert.ok(this.onStartRequestFired);
  Assert.ok(this.onDataAvailableFired);
  Assert.ok(this.isHttp2Connection);
  Assert.equal(this.count, 1024 * 1024 * 1); // 1mb of data expected
  request.QueryInterface(Ci.nsIProxiedChannel);
  var httpProxyConnectResponseCode = request.httpProxyConnectResponseCode;
  this.finish({ httpProxyConnectResponseCode });
};

// Does the appropriate checks for POSTs
var Http2PostListener = function (expected_md5) {
  this.expected_md5 = expected_md5;
};

Http2PostListener.prototype = new Http2CheckListener();
Http2PostListener.prototype.expected_md5 = "";

Http2PostListener.prototype.onDataAvailable = function (
  request,
  stream,
  off,
  cnt
) {
  this.onDataAvailableFired = true;
  this.isHttp2Connection = checkIsHttp2(request);
  read_stream(stream, cnt);
  Assert.equal(
    this.expected_md5,
    request.getResponseHeader("X-Calculated-MD5")
  );
};

var ResumeStalledChannelListener = function () {};

ResumeStalledChannelListener.prototype = {
  onStartRequestFired: false,
  onDataAvailableFired: false,
  isHttp2Connection: false,
  shouldBeHttp2: true,
  resumable: null,

  onStartRequest: function testOnStartRequest(request) {
    this.onStartRequestFired = true;
    if (!Components.isSuccessCode(request.status)) {
      do_throw("Channel should have a success code! (" + request.status + ")");
    }

    Assert.ok(request instanceof Ci.nsIHttpChannel);
    Assert.equal(request.responseStatus, 200);
    Assert.equal(request.requestSucceeded, true);
  },

  onDataAvailable: function testOnDataAvailable(request, stream, off, cnt) {
    this.onDataAvailableFired = true;
    this.isHttp2Connection = checkIsHttp2(request);
    read_stream(stream, cnt);
  },

  onStopRequest: function testOnStopRequest(request, status) {
    Assert.ok(this.onStartRequestFired);
    Assert.ok(Components.isSuccessCode(status));
    Assert.ok(this.onDataAvailableFired);
    Assert.ok(this.isHttp2Connection == this.shouldBeHttp2);
    this.resumable.resume();
  },
};

// test a large download that creates stream flow control and
// confirm we can do another independent stream while the download
// stream is stuck
async function test_http2_blocking_download(serverPort) {
  var chan = makeHTTPChannel(`https://localhost:${serverPort}/bigdownload`);
  var internalChannel = chan.QueryInterface(Ci.nsIHttpChannelInternal);
  internalChannel.initialRwin = 500000; // make the stream.suspend push back in h2
  var p = new Promise(resolve => {
    var listener = new Http2CheckListener();
    listener.finish = resolve;
    listener.expected = 3 * 1024 * 1024;
    chan.asyncOpen(listener);
    chan.suspend();
  });
  // wait 5 seconds so that stream flow control kicks in and then see if we
  // can do a basic transaction (i.e. session not blocked). afterwards resume
  // channel
  do_timeout(5000, function () {
    var simpleChannel = makeHTTPChannel(`https://localhost:${serverPort}/`);
    var sl = new ResumeStalledChannelListener();
    sl.resumable = chan;
    simpleChannel.asyncOpen(sl);
  });
  return p;
}

// Make sure we make a HTTP2 connection and both us and the server mark it as such
async function test_http2_basic(serverPort) {
  var chan = makeHTTPChannel(`https://localhost:${serverPort}/`);
  var p = new Promise(resolve => {
    var listener = new Http2CheckListener();
    listener.finish = resolve;
    chan.asyncOpen(listener);
  });
  return p;
}

async function test_http2_basic_unblocked_dep(serverPort) {
  var chan = makeHTTPChannel(
    `https://localhost:${serverPort}/basic_unblocked_dep`
  );
  var cos = chan.QueryInterface(Ci.nsIClassOfService);
  cos.addClassFlags(Ci.nsIClassOfService.Unblocked);
  return new Promise(resolve => {
    var listener = new Http2CheckListener();
    listener.finish = resolve;
    chan.asyncOpen(listener);
  });
}

// make sure we don't use h2 when disallowed
async function test_http2_nospdy(serverPort) {
  var chan = makeHTTPChannel(`https://localhost:${serverPort}/`);
  return new Promise(resolve => {
    var listener = new Http2CheckListener();
    listener.finish = resolve;
    var internalChannel = chan.QueryInterface(Ci.nsIHttpChannelInternal);
    internalChannel.allowSpdy = false;
    listener.shouldBeHttp2 = false;
    chan.asyncOpen(listener);
  });
}

// Support for making sure XHR works over SPDY
function checkXhr(xhr, finish) {
  if (xhr.readyState != 4) {
    return;
  }

  Assert.equal(xhr.status, 200);
  Assert.equal(checkIsHttp2(xhr), true);
  finish();
}

// Fires off an XHR request over h2
async function test_http2_xhr(serverPort) {
  return new Promise(resolve => {
    var req = new XMLHttpRequest();
    req.open("GET", `https://localhost:${serverPort}/`, true);
    req.addEventListener("readystatechange", function () {
      checkXhr(req, resolve);
    });
    req.send(null);
  });
}

var Http2ConcurrentListener = function () {};

Http2ConcurrentListener.prototype = new Http2CheckListener();
Http2ConcurrentListener.prototype.count = 0;
Http2ConcurrentListener.prototype.target = 0;
Http2ConcurrentListener.prototype.reset = 0;
Http2ConcurrentListener.prototype.recvdHdr = 0;

Http2ConcurrentListener.prototype.onStopRequest = function (request) {
  this.count++;
  Assert.ok(this.isHttp2Connection);
  if (this.recvdHdr > 0) {
    Assert.equal(request.getResponseHeader("X-Recvd"), this.recvdHdr);
  }

  if (this.count == this.target) {
    if (this.reset > 0) {
      Services.prefs.setIntPref(
        "network.http.http2.default-concurrent",
        this.reset
      );
    }
    request.QueryInterface(Ci.nsIProxiedChannel);
    var httpProxyConnectResponseCode = request.httpProxyConnectResponseCode;
    this.finish({ httpProxyConnectResponseCode });
  }
};

async function test_http2_concurrent(concurrent_channels, serverPort) {
  var p = new Promise(resolve => {
    var concurrent_listener = new Http2ConcurrentListener();
    concurrent_listener.finish = resolve;
    concurrent_listener.target = 201;
    concurrent_listener.reset = Services.prefs.getIntPref(
      "network.http.http2.default-concurrent"
    );
    Services.prefs.setIntPref("network.http.http2.default-concurrent", 100);

    for (var i = 0; i < concurrent_listener.target; i++) {
      concurrent_channels[i] = makeHTTPChannel(
        `https://localhost:${serverPort}/750ms`
      );
      concurrent_channels[i].loadFlags = Ci.nsIRequest.LOAD_BYPASS_CACHE;
      concurrent_channels[i].asyncOpen(concurrent_listener);
    }
  });
  return p;
}

async function test_http2_concurrent_post(concurrent_channels, serverPort) {
  return new Promise(resolve => {
    var concurrent_listener = new Http2ConcurrentListener();
    concurrent_listener.finish = resolve;
    concurrent_listener.target = 8;
    concurrent_listener.recvdHdr = posts[2].length;
    concurrent_listener.reset = Services.prefs.getIntPref(
      "network.http.http2.default-concurrent"
    );
    Services.prefs.setIntPref("network.http.http2.default-concurrent", 3);

    for (var i = 0; i < concurrent_listener.target; i++) {
      concurrent_channels[i] = makeHTTPChannel(
        `https://localhost:${serverPort}/750msPost`
      );
      concurrent_channels[i].loadFlags = Ci.nsIRequest.LOAD_BYPASS_CACHE;
      var stream = Cc["@mozilla.org/io/string-input-stream;1"].createInstance(
        Ci.nsIStringInputStream
      );
      stream.data = posts[2];
      var uchan = concurrent_channels[i].QueryInterface(Ci.nsIUploadChannel);
      uchan.setUploadStream(stream, "text/plain", stream.available());
      concurrent_channels[i].requestMethod = "POST";
      concurrent_channels[i].asyncOpen(concurrent_listener);
    }
  });
}

// Test to make sure we get multiplexing right
async function test_http2_multiplex(serverPort) {
  let chan1 = makeHTTPChannel(`https://localhost:${serverPort}/multiplex1`);
  let chan2 = makeHTTPChannel(`https://localhost:${serverPort}/multiplex2`);
  let listener1 = new Http2MultiplexListener();
  let listener2 = new Http2MultiplexListener();

  let promises = [];
  let p1 = new Promise(resolve => {
    listener1.finish = resolve;
  });
  promises.push(p1);
  let p2 = new Promise(resolve => {
    listener2.finish = resolve;
  });
  promises.push(p2);

  chan1.asyncOpen(listener1);
  chan2.asyncOpen(listener2);
  return Promise.all(promises);
}

// Test to make sure we gateway non-standard headers properly
async function test_http2_header(serverPort) {
  let chan = makeHTTPChannel(`https://localhost:${serverPort}/header`);
  let hvalue = "Headers are fun";
  chan.setRequestHeader("X-Test-Header", hvalue, false);
  return new Promise(resolve => {
    let listener = new Http2HeaderListener("X-Received-Test-Header", function (
      received_hvalue
    ) {
      Assert.equal(received_hvalue, hvalue);
    });
    listener.finish = resolve;
    chan.asyncOpen(listener);
  });
}

// Test to make sure headers with invalid characters in the name are rejected
async function test_http2_invalid_response_header(serverPort, invalid_kind) {
  return new Promise(resolve => {
    var listener = new Http2CheckListener();
    listener.finish = resolve;
    listener.shouldSucceed = false;
    var chan = makeHTTPChannel(
      `https://localhost:${serverPort}/invalid_response_header/${invalid_kind}`
    );
    chan.asyncOpen(listener);
  });
}

// Test to make sure cookies are split into separate fields before compression
async function test_http2_cookie_crumbling(serverPort) {
  var chan = makeHTTPChannel(
    `https://localhost:${serverPort}/cookie_crumbling`
  );
  var cookiesSent = ["a=b", "c=d01234567890123456789", "e=f"].sort();
  chan.setRequestHeader("Cookie", cookiesSent.join("; "), false);
  return new Promise(resolve => {
    var listener = new Http2HeaderListener("X-Received-Header-Pairs", function (
      pairsReceived
    ) {
      var cookiesReceived = JSON.parse(pairsReceived)
        .filter(function (pair) {
          return pair[0] == "cookie";
        })
        .map(function (pair) {
          return pair[1];
        })
        .sort();
      Assert.equal(cookiesReceived.length, cookiesSent.length);
      cookiesReceived.forEach(function (cookieReceived, index) {
        Assert.equal(cookiesSent[index], cookieReceived);
      });
    });
    listener.finish = resolve;
    chan.asyncOpen(listener);
  });
}

async function test_http2_push1(loadGroup, serverPort) {
  var chan = makeHTTPChannel(`https://localhost:${serverPort}/push`);
  chan.loadGroup = loadGroup;
  return new Promise(resolve => {
    var listener = new Http2PushListener(true);
    listener.finish = resolve;
    listener.serverPort = serverPort;
    chan.asyncOpen(listener);
  });
}

async function test_http2_push2(loadGroup, serverPort) {
  var chan = makeHTTPChannel(`https://localhost:${serverPort}/push.js`);
  chan.loadGroup = loadGroup;
  return new Promise(resolve => {
    var listener = new Http2PushListener(true);
    listener.finish = resolve;
    listener.serverPort = serverPort;
    chan.asyncOpen(listener);
  });
}

async function test_http2_push3(loadGroup, serverPort) {
  var chan = makeHTTPChannel(`https://localhost:${serverPort}/push2`);
  chan.loadGroup = loadGroup;
  return new Promise(resolve => {
    var listener = new Http2PushListener(true);
    listener.finish = resolve;
    listener.serverPort = serverPort;
    chan.asyncOpen(listener);
  });
}

async function test_http2_push4(loadGroup, serverPort) {
  var chan = makeHTTPChannel(`https://localhost:${serverPort}/push2.js`);
  chan.loadGroup = loadGroup;
  return new Promise(resolve => {
    var listener = new Http2PushListener(true);
    listener.finish = resolve;
    listener.serverPort = serverPort;
    chan.asyncOpen(listener);
  });
}

async function test_http2_push5(loadGroup, serverPort) {
  var chan = makeHTTPChannel(`https://localhost:${serverPort}/push5`);
  chan.loadGroup = loadGroup;
  return new Promise(resolve => {
    var listener = new Http2PushListener(true);
    listener.finish = resolve;
    listener.serverPort = serverPort;
    chan.asyncOpen(listener);
  });
}

async function test_http2_push6(loadGroup, serverPort) {
  var chan = makeHTTPChannel(`https://localhost:${serverPort}/push5.js`);
  chan.loadGroup = loadGroup;
  return new Promise(resolve => {
    var listener = new Http2PushListener(true);
    listener.finish = resolve;
    listener.serverPort = serverPort;
    chan.asyncOpen(listener);
  });
}

// this is a basic test where the server sends a simple document with 2 header
// blocks. bug 1027364
async function test_http2_doubleheader(serverPort) {
  var chan = makeHTTPChannel(`https://localhost:${serverPort}/doubleheader`);
  return new Promise(resolve => {
    var listener = new Http2CheckListener();
    listener.finish = resolve;
    chan.asyncOpen(listener);
  });
}

// Make sure we handle GETs that cover more than 2 frames properly
async function test_http2_big(serverPort) {
  var chan = makeHTTPChannel(`https://localhost:${serverPort}/big`);
  return new Promise(resolve => {
    var listener = new Http2BigListener();
    listener.finish = resolve;
    chan.asyncOpen(listener);
  });
}

async function test_http2_huge_suspended(serverPort) {
  var chan = makeHTTPChannel(`https://localhost:${serverPort}/huge`);
  return new Promise(resolve => {
    var listener = new Http2HugeSuspendedListener();
    listener.finish = resolve;
    chan.asyncOpen(listener);
    chan.suspend();
    do_timeout(500, chan.resume);
  });
}

// Support for doing a POST
function do_post(content, chan, listener, method) {
  var stream = Cc["@mozilla.org/io/string-input-stream;1"].createInstance(
    Ci.nsIStringInputStream
  );
  stream.data = content;

  var uchan = chan.QueryInterface(Ci.nsIUploadChannel);
  uchan.setUploadStream(stream, "text/plain", stream.available());

  chan.requestMethod = method;

  chan.asyncOpen(listener);
}

// Make sure we can do a simple POST
async function test_http2_post(serverPort) {
  var chan = makeHTTPChannel(`https://localhost:${serverPort}/post`);
  var p = new Promise(resolve => {
    var listener = new Http2PostListener(md5s[0]);
    listener.finish = resolve;
    do_post(posts[0], chan, listener, "POST");
  });
  return p;
}

async function test_http2_empty_post(serverPort) {
  var chan = makeHTTPChannel(`https://localhost:${serverPort}/post`);
  var p = new Promise(resolve => {
    var listener = new Http2PostListener("0");
    listener.finish = resolve;
    do_post("", chan, listener, "POST");
  });
  return p;
}

// Make sure we can do a simple PATCH
async function test_http2_patch(serverPort) {
  var chan = makeHTTPChannel(`https://localhost:${serverPort}/patch`);
  return new Promise(resolve => {
    var listener = new Http2PostListener(md5s[0]);
    listener.finish = resolve;
    do_post(posts[0], chan, listener, "PATCH");
  });
}

// Make sure we can do a POST that covers more than 2 frames
async function test_http2_post_big(serverPort) {
  var chan = makeHTTPChannel(`https://localhost:${serverPort}/post`);
  return new Promise(resolve => {
    var listener = new Http2PostListener(md5s[1]);
    listener.finish = resolve;
    do_post(posts[1], chan, listener, "POST");
  });
}

// When a http proxy is used alt-svc is disable. Therefore if withProxy is true,
// try numberOfTries times to connect and make sure that alt-svc is not use and we never
// connect to the HTTP/2 server.
var altsvcClientListener = function (
  finish,
  httpserv,
  httpserv2,
  withProxy,
  numberOfTries
) {
  this.finish = finish;
  this.httpserv = httpserv;
  this.httpserv2 = httpserv2;
  this.withProxy = withProxy;
  this.numberOfTries = numberOfTries;
};

altsvcClientListener.prototype = {
  onStartRequest: function test_onStartR(request) {
    Assert.equal(request.status, Cr.NS_OK);
  },

  onDataAvailable: function test_ODA(request, stream, offset, cnt) {
    read_stream(stream, cnt);
  },

  onStopRequest: function test_onStopR(request) {
    var isHttp2Connection = checkIsHttp2(
      request.QueryInterface(Ci.nsIHttpChannel)
    );
    if (!isHttp2Connection) {
      dump("/altsvc1 not over h2 yet - retry\n");
      if (this.withProxy && this.numberOfTries == 0) {
        request.QueryInterface(Ci.nsIProxiedChannel);
        var httpProxyConnectResponseCode = request.httpProxyConnectResponseCode;
        this.finish({ httpProxyConnectResponseCode });
        return;
      }
      let chan = makeHTTPChannel(
        `http://foo.example.com:${this.httpserv}/altsvc1`,
        this.withProxy
      ).QueryInterface(Ci.nsIHttpChannel);
      // we use this header to tell the server to issue a altsvc frame for the
      // speficied origin we will use in the next part of the test
      chan.setRequestHeader(
        "x-redirect-origin",
        `http://foo.example.com:${this.httpserv2}`,
        false
      );
      chan.loadFlags = Ci.nsIRequest.LOAD_BYPASS_CACHE;
      chan.asyncOpen(
        new altsvcClientListener(
          this.finish,
          this.httpserv,
          this.httpserv2,
          this.withProxy,
          this.numberOfTries - 1
        )
      );
    } else {
      Assert.ok(isHttp2Connection);
      let chan = makeHTTPChannel(
        `http://foo.example.com:${this.httpserv2}/altsvc2`
      ).QueryInterface(Ci.nsIHttpChannel);
      chan.loadFlags = Ci.nsIRequest.LOAD_BYPASS_CACHE;
      chan.asyncOpen(
        new altsvcClientListener2(this.finish, this.httpserv, this.httpserv2)
      );
    }
  },
};

var altsvcClientListener2 = function (finish, httpserv, httpserv2) {
  this.finish = finish;
  this.httpserv = httpserv;
  this.httpserv2 = httpserv2;
};

altsvcClientListener2.prototype = {
  onStartRequest: function test_onStartR(request) {
    Assert.equal(request.status, Cr.NS_OK);
  },

  onDataAvailable: function test_ODA(request, stream, offset, cnt) {
    read_stream(stream, cnt);
  },

  onStopRequest: function test_onStopR(request) {
    var isHttp2Connection = checkIsHttp2(
      request.QueryInterface(Ci.nsIHttpChannel)
    );
    if (!isHttp2Connection) {
      dump("/altsvc2 not over h2 yet - retry\n");
      var chan = makeHTTPChannel(
        `http://foo.example.com:${this.httpserv2}/altsvc2`
      ).QueryInterface(Ci.nsIHttpChannel);
      chan.loadFlags = Ci.nsIRequest.LOAD_BYPASS_CACHE;
      chan.asyncOpen(
        new altsvcClientListener2(this.finish, this.httpserv, this.httpserv2)
      );
    } else {
      Assert.ok(isHttp2Connection);
      request.QueryInterface(Ci.nsIProxiedChannel);
      var httpProxyConnectResponseCode = request.httpProxyConnectResponseCode;
      this.finish({ httpProxyConnectResponseCode });
    }
  },
};

async function test_http2_altsvc(httpserv, httpserv2, withProxy) {
  var chan = makeHTTPChannel(
    `http://foo.example.com:${httpserv}/altsvc1`,
    withProxy
  ).QueryInterface(Ci.nsIHttpChannel);
  return new Promise(resolve => {
    var numberOfTries = 0;
    if (withProxy) {
      numberOfTries = 20;
    }
    chan.asyncOpen(
      new altsvcClientListener(
        resolve,
        httpserv,
        httpserv2,
        withProxy,
        numberOfTries
      )
    );
  });
}

var Http2PushApiListener = function (finish, serverPort) {
  this.finish = finish;
  this.serverPort = serverPort;
};

Http2PushApiListener.prototype = {
  checksPending: 9, // 4 onDataAvailable and 5 onStop

  getInterface(aIID) {
    return this.QueryInterface(aIID);
  },

  QueryInterface: ChromeUtils.generateQI([
    "nsIHttpPushListener",
    "nsIStreamListener",
  ]),

  // nsIHttpPushListener
  onPush: function onPush(associatedChannel, pushChannel) {
    Assert.equal(
      associatedChannel.originalURI.spec,
      "https://localhost:" + this.serverPort + "/pushapi1"
    );
    Assert.equal(pushChannel.getRequestHeader("x-pushed-request"), "true");

    pushChannel.asyncOpen(this);
    if (
      pushChannel.originalURI.spec ==
      "https://localhost:" + this.serverPort + "/pushapi1/2"
    ) {
      pushChannel.cancel(Cr.NS_ERROR_ABORT);
    } else if (
      pushChannel.originalURI.spec ==
      "https://localhost:" + this.serverPort + "/pushapi1/3"
    ) {
      Assert.ok(pushChannel.getRequestHeader("Accept-Encoding").includes("br"));
    }
  },

  // normal Channel listeners
  onStartRequest: function pushAPIOnStart() {},

  onDataAvailable: function pushAPIOnDataAvailable(
    request,
    stream,
    offset,
    cnt
  ) {
    Assert.notEqual(
      request.originalURI.spec,
      `https://localhost:${this.serverPort}/pushapi1/2`
    );

    var data = read_stream(stream, cnt);

    if (
      request.originalURI.spec ==
      `https://localhost:${this.serverPort}/pushapi1`
    ) {
      Assert.equal(data[0], "0");
      --this.checksPending;
    } else if (
      request.originalURI.spec ==
      `https://localhost:${this.serverPort}/pushapi1/1`
    ) {
      Assert.equal(data[0], "1");
      --this.checksPending; // twice
    } else if (
      request.originalURI.spec ==
      `https://localhost:${this.serverPort}/pushapi1/3`
    ) {
      Assert.equal(data[0], "3");
      --this.checksPending;
    } else {
      Assert.equal(true, false);
    }
  },

  onStopRequest: function test_onStopR(request) {
    if (
      request.originalURI.spec ==
      `https://localhost:${this.serverPort}/pushapi1/2`
    ) {
      Assert.equal(request.status, Cr.NS_ERROR_ABORT);
    } else {
      Assert.equal(request.status, Cr.NS_OK);
    }

    --this.checksPending; // 5 times - one for each push plus the pull
    if (!this.checksPending) {
      request.QueryInterface(Ci.nsIProxiedChannel);
      var httpProxyConnectResponseCode = request.httpProxyConnectResponseCode;
      this.finish({ httpProxyConnectResponseCode });
    }
  },
};

// pushAPI testcase 1 expects
// 1 to pull /pushapi1 with 0
// 2 to see /pushapi1/1 with 1
// 3 to see /pushapi1/1 with 1 (again)
// 4 to see /pushapi1/2 that it will cancel
// 5 to see /pushapi1/3 with 3 with brotli

async function test_http2_pushapi_1(loadGroup, serverPort) {
  var chan = makeHTTPChannel(`https://localhost:${serverPort}/pushapi1`);
  chan.loadGroup = loadGroup;
  return new Promise(resolve => {
    var listener = new Http2PushApiListener(resolve, serverPort);
    chan.notificationCallbacks = listener;
    chan.asyncOpen(listener);
  });
}

var WrongSuiteListener = function () {};

WrongSuiteListener.prototype = new Http2CheckListener();
WrongSuiteListener.prototype.shouldBeHttp2 = false;
WrongSuiteListener.prototype.onStopRequest = function (request, status) {
  Services.prefs.setBoolPref(
    "security.ssl3.ecdhe_rsa_aes_128_gcm_sha256",
    true
  );
  Services.prefs.clearUserPref("security.tls.version.max");
  Http2CheckListener.prototype.onStopRequest.call(this, request, status);
};

// test that we use h1 without the mandatory cipher suite available when
// offering at most tls1.2
async function test_http2_wrongsuite_tls12(serverPort) {
  Services.prefs.setBoolPref(
    "security.ssl3.ecdhe_rsa_aes_128_gcm_sha256",
    false
  );
  Services.prefs.setIntPref("security.tls.version.max", 3);
  var chan = makeHTTPChannel(`https://localhost:${serverPort}/wrongsuite`);
  chan.loadFlags =
    Ci.nsIRequest.LOAD_FRESH_CONNECTION |
    Ci.nsIChannel.LOAD_INITIAL_DOCUMENT_URI;
  return new Promise(resolve => {
    var listener = new WrongSuiteListener();
    listener.finish = resolve;
    chan.asyncOpen(listener);
  });
}

// test that we use h2 when offering tls1.3 or higher regardless of if the
// mandatory cipher suite is available
async function test_http2_wrongsuite_tls13(serverPort) {
  Services.prefs.setBoolPref(
    "security.ssl3.ecdhe_rsa_aes_128_gcm_sha256",
    false
  );
  var chan = makeHTTPChannel(`https://localhost:${serverPort}/wrongsuite`);
  chan.loadFlags =
    Ci.nsIRequest.LOAD_FRESH_CONNECTION |
    Ci.nsIChannel.LOAD_INITIAL_DOCUMENT_URI;
  return new Promise(resolve => {
    var listener = new WrongSuiteListener();
    listener.finish = resolve;
    listener.shouldBeHttp2 = true;
    chan.asyncOpen(listener);
  });
}

async function test_http2_h11required_stream(serverPort) {
  var chan = makeHTTPChannel(
    `https://localhost:${serverPort}/h11required_stream`
  );
  return new Promise(resolve => {
    var listener = new Http2CheckListener();
    listener.finish = resolve;
    listener.shouldBeHttp2 = false;
    chan.asyncOpen(listener);
  });
}

function H11RequiredSessionListener() {}

H11RequiredSessionListener.prototype = new Http2CheckListener();

H11RequiredSessionListener.prototype.onStopRequest = function (request) {
  var streamReused = request.getResponseHeader("X-H11Required-Stream-Ok");
  Assert.equal(streamReused, "yes");

  Assert.ok(this.onStartRequestFired);
  Assert.ok(this.onDataAvailableFired);
  Assert.ok(this.isHttp2Connection == this.shouldBeHttp2);

  request.QueryInterface(Ci.nsIProxiedChannel);
  var httpProxyConnectResponseCode = request.httpProxyConnectResponseCode;
  this.finish({ httpProxyConnectResponseCode });
};

async function test_http2_h11required_session(serverPort) {
  var chan = makeHTTPChannel(
    `https://localhost:${serverPort}/h11required_session`
  );
  return new Promise(resolve => {
    var listener = new H11RequiredSessionListener();
    listener.finish = resolve;
    listener.shouldBeHttp2 = false;
    chan.asyncOpen(listener);
  });
}

async function test_http2_retry_rst(serverPort) {
  var chan = makeHTTPChannel(`https://localhost:${serverPort}/rstonce`);
  return new Promise(resolve => {
    var listener = new Http2CheckListener();
    listener.finish = resolve;
    chan.asyncOpen(listener);
  });
}

async function test_http2_continuations(loadGroup, serverPort) {
  var chan = makeHTTPChannel(
    `https://localhost:${serverPort}/continuedheaders`
  );
  chan.loadGroup = loadGroup;
  return new Promise(resolve => {
    var listener = new Http2ContinuedHeaderListener();
    listener.finish = resolve;
    listener.serverPort = serverPort;
    chan.notificationCallbacks = listener;
    chan.asyncOpen(listener);
  });
}

async function test_http2_continuations_over_max_response_limit(
  loadGroup,
  serverPort
) {
  var chan = makeHTTPChannel(
    `https://localhost:${serverPort}/hugecontinuedheaders?size=385`
  );
  chan.loadGroup = loadGroup;
  return new Promise(resolve => {
    var listener = new Http2CheckListener();
    listener.finish = resolve;
    listener.shouldSucceed = false;
    chan.asyncOpen(listener);
  });
}

function Http2IllegalHpackValidationListener() {}

Http2IllegalHpackValidationListener.prototype = new Http2CheckListener();
Http2IllegalHpackValidationListener.prototype.shouldGoAway = false;

Http2IllegalHpackValidationListener.prototype.onStopRequest = function (
  request
) {
  var wentAway = request.getResponseHeader("X-Did-Goaway") === "yes";
  Assert.equal(wentAway, this.shouldGoAway);

  Assert.ok(this.onStartRequestFired);
  Assert.ok(this.onDataAvailableFired);
  Assert.ok(this.isHttp2Connection == this.shouldBeHttp2);

  request.QueryInterface(Ci.nsIProxiedChannel);
  var httpProxyConnectResponseCode = request.httpProxyConnectResponseCode;
  this.finish({ httpProxyConnectResponseCode });
};

function Http2IllegalHpackListener() {}
Http2IllegalHpackListener.prototype = new Http2CheckListener();
Http2IllegalHpackListener.prototype.shouldGoAway = false;

Http2IllegalHpackListener.prototype.onStopRequest = function () {
  var chan = makeHTTPChannel(
    `https://localhost:${this.serverPort}/illegalhpack_validate`
  );
  var listener = new Http2IllegalHpackValidationListener();
  listener.finish = this.finish;
  listener.shouldGoAway = this.shouldGoAway;
  chan.asyncOpen(listener);
};

async function test_http2_illegalhpacksoft(serverPort) {
  var chan = makeHTTPChannel(
    `https://localhost:${serverPort}/illegalhpacksoft`
  );
  return new Promise(resolve => {
    var listener = new Http2IllegalHpackListener();
    listener.finish = resolve;
    listener.serverPort = serverPort;
    listener.shouldGoAway = false;
    listener.shouldSucceed = false;
    chan.asyncOpen(listener);
  });
}

async function test_http2_illegalhpackhard(serverPort) {
  var chan = makeHTTPChannel(
    `https://localhost:${serverPort}/illegalhpackhard`
  );
  return new Promise(resolve => {
    var listener = new Http2IllegalHpackListener();
    listener.finish = resolve;
    listener.serverPort = serverPort;
    listener.shouldGoAway = true;
    listener.shouldSucceed = false;
    chan.asyncOpen(listener);
  });
}

async function test_http2_folded_header(loadGroup, serverPort) {
  var chan = makeHTTPChannel(`https://localhost:${serverPort}/foldedheader`);
  chan.loadGroup = loadGroup;
  return new Promise(resolve => {
    var listener = new Http2CheckListener();
    listener.finish = resolve;
    listener.shouldSucceed = false;
    chan.asyncOpen(listener);
  });
}

async function test_http2_empty_data(serverPort) {
  var chan = makeHTTPChannel(`https://localhost:${serverPort}/emptydata`);
  return new Promise(resolve => {
    var listener = new Http2CheckListener();
    listener.finish = resolve;
    chan.asyncOpen(listener);
  });
}

async function test_http2_push_firstparty1(loadGroup, serverPort) {
  var chan = makeHTTPChannel(`https://localhost:${serverPort}/push`);
  chan.loadGroup = loadGroup;
  chan.loadInfo.originAttributes = { firstPartyDomain: "foo.com" };
  return new Promise(resolve => {
    var listener = new Http2PushListener(true);
    listener.finish = resolve;
    listener.serverPort = serverPort;
    chan.asyncOpen(listener);
  });
}

async function test_http2_push_firstparty2(loadGroup, serverPort) {
  var chan = makeHTTPChannel(`https://localhost:${serverPort}/push.js`);
  chan.loadGroup = loadGroup;
  chan.loadInfo.originAttributes = { firstPartyDomain: "bar.com" };
  return new Promise(resolve => {
    var listener = new Http2PushListener(false);
    listener.finish = resolve;
    listener.serverPort = serverPort;
    chan.asyncOpen(listener);
  });
}

async function test_http2_push_firstparty3(loadGroup, serverPort) {
  var chan = makeHTTPChannel(`https://localhost:${serverPort}/push.js`);
  chan.loadGroup = loadGroup;
  chan.loadInfo.originAttributes = { firstPartyDomain: "foo.com" };
  return new Promise(resolve => {
    var listener = new Http2PushListener(true);
    listener.finish = resolve;
    listener.serverPort = serverPort;
    chan.asyncOpen(listener);
  });
}

async function test_http2_push_userContext1(loadGroup, serverPort) {
  var chan = makeHTTPChannel(`https://localhost:${serverPort}/push`);
  chan.loadGroup = loadGroup;
  chan.loadInfo.originAttributes = { userContextId: 1 };
  return new Promise(resolve => {
    var listener = new Http2PushListener(true);
    listener.finish = resolve;
    listener.serverPort = serverPort;
    chan.asyncOpen(listener);
  });
}

async function test_http2_push_userContext2(loadGroup, serverPort) {
  var chan = makeHTTPChannel(`https://localhost:${serverPort}/push.js`);
  chan.loadGroup = loadGroup;
  chan.loadInfo.originAttributes = { userContextId: 2 };
  return new Promise(resolve => {
    var listener = new Http2PushListener(false);
    listener.finish = resolve;
    listener.serverPort = serverPort;
    chan.asyncOpen(listener);
  });
}

async function test_http2_push_userContext3(loadGroup, serverPort) {
  var chan = makeHTTPChannel(`https://localhost:${serverPort}/push.js`);
  chan.loadGroup = loadGroup;
  chan.loadInfo.originAttributes = { userContextId: 1 };
  return new Promise(resolve => {
    var listener = new Http2PushListener(true);
    listener.finish = resolve;
    listener.serverPort = serverPort;
    chan.asyncOpen(listener);
  });
}

async function test_http2_status_phrase(serverPort) {
  var chan = makeHTTPChannel(`https://localhost:${serverPort}/statusphrase`);
  return new Promise(resolve => {
    var listener = new Http2CheckListener();
    listener.finish = resolve;
    listener.shouldSucceed = false;
    chan.asyncOpen(listener);
  });
}

var PulledDiskCacheListener = function () {};
PulledDiskCacheListener.prototype = new Http2CheckListener();
PulledDiskCacheListener.prototype.EXPECTED_DATA = "this was pulled via h2";
PulledDiskCacheListener.prototype.readData = "";
PulledDiskCacheListener.prototype.onDataAvailable =
  function testOnDataAvailable(request, stream, off, cnt) {
    this.onDataAvailableFired = true;
    this.isHttp2Connection = checkIsHttp2(request);
    this.accum += cnt;
    this.readData += read_stream(stream, cnt);
  };
PulledDiskCacheListener.prototype.onStopRequest = function testOnStopRequest(
  request,
  status
) {
  Assert.equal(this.EXPECTED_DATA, this.readData);
  Http2CheckListener.prorotype.onStopRequest.call(this, request, status);
};

const DISK_CACHE_DATA = "this is from disk cache";

var FromDiskCacheListener = function (finish, loadGroup, serverPort) {
  this.finish = finish;
  this.loadGroup = loadGroup;
  this.serverPort = serverPort;
};
FromDiskCacheListener.prototype = {
  onStartRequestFired: false,
  onDataAvailableFired: false,
  readData: "",

  onStartRequest: function testOnStartRequest(request) {
    this.onStartRequestFired = true;
    if (!Components.isSuccessCode(request.status)) {
      do_throw("Channel should have a success code! (" + request.status + ")");
    }

    Assert.ok(request instanceof Ci.nsIHttpChannel);
    Assert.ok(request.requestSucceeded);
    Assert.equal(request.responseStatus, 200);
  },

  onDataAvailable: function testOnDataAvailable(request, stream, off, cnt) {
    this.onDataAvailableFired = true;
    this.readData += read_stream(stream, cnt);
  },

  onStopRequest: function testOnStopRequest(request, status) {
    Assert.ok(this.onStartRequestFired);
    Assert.ok(Components.isSuccessCode(status));
    Assert.ok(this.onDataAvailableFired);
    Assert.equal(this.readData, DISK_CACHE_DATA);

    evict_cache_entries("disk");
    syncWithCacheIOThread(() => {
      // Now that we know the entry is out of the disk cache, check to make sure
      // we don't have this hiding in the push cache somewhere - if we do, it
      // didn't get cancelled, and we have a bug.
      var chan = makeHTTPChannel(
        `https://localhost:${this.serverPort}/diskcache`
      );
      var listener = new PulledDiskCacheListener();
      listener.finish = this.finish;
      chan.loadGroup = this.loadGroup;
      chan.asyncOpen(listener);
    });
  },
};

var Http2DiskCachePushListener = function () {};
Http2DiskCachePushListener.prototype = new Http2CheckListener();

Http2DiskCachePushListener.onStopRequest = function (request, status) {
  Assert.ok(this.onStartRequestFired);
  Assert.ok(Components.isSuccessCode(status));
  Assert.ok(this.onDataAvailableFired);
  Assert.ok(this.isHttp2Connection == this.shouldBeHttp2);

  // Now we need to open a channel to ensure we get data from the disk cache
  // for the pushed item, instead of from the push cache.
  var chan = makeHTTPChannel(`https://localhost:${this.serverPort}/diskcache`);
  var listener = new FromDiskCacheListener(
    this.finish,
    this.loadGroup,
    this.serverPort
  );
  chan.loadGroup = this.loadGroup;
  chan.asyncOpen(listener);
};

function continue_test_http2_disk_cache_push(
  status,
  entry,
  finish,
  loadGroup,
  serverPort
) {
  // TODO - store stuff in cache entry, then open an h2 channel that will push
  // this, once that completes, open a channel for the cache entry we made and
  // ensure it came from disk cache, not the push cache.
  var outputStream = entry.openOutputStream(0, -1);
  outputStream.write(DISK_CACHE_DATA, DISK_CACHE_DATA.length);

  // Now we open our URL that will push data for the URL above
  var chan = makeHTTPChannel(`https://localhost:${serverPort}/pushindisk`);
  var listener = new Http2DiskCachePushListener();
  listener.finish = finish;
  listener.loadGroup = loadGroup;
  listener.serverPort = serverPort;
  chan.loadGroup = loadGroup;
  chan.asyncOpen(listener);
}

async function test_http2_disk_cache_push(loadGroup, serverPort) {
  return new Promise(resolve => {
    asyncOpenCacheEntry(
      `https://localhost:${serverPort}/diskcache`,
      "disk",
      Ci.nsICacheStorage.OPEN_NORMALLY,
      null,
      function (status, entry) {
        continue_test_http2_disk_cache_push(
          status,
          entry,
          resolve,
          loadGroup,
          serverPort
        );
      },
      false
    );
  });
}

var Http2DoublepushListener = function () {};
Http2DoublepushListener.prototype = new Http2CheckListener();
Http2DoublepushListener.prototype.onStopRequest = function (request, status) {
  Assert.ok(this.onStartRequestFired);
  Assert.ok(Components.isSuccessCode(status));
  Assert.ok(this.onDataAvailableFired);
  Assert.ok(this.isHttp2Connection == this.shouldBeHttp2);

  var chan = makeHTTPChannel(
    `https://localhost:${this.serverPort}/doublypushed`
  );
  var listener = new Http2DoublypushedListener();
  listener.finish = this.finish;
  chan.loadGroup = this.loadGroup;
  chan.asyncOpen(listener);
};

var Http2DoublypushedListener = function () {};
Http2DoublypushedListener.prototype = new Http2CheckListener();
Http2DoublypushedListener.prototype.readData = "";
Http2DoublypushedListener.prototype.onDataAvailable = function (
  request,
  stream,
  off,
  cnt
) {
  this.onDataAvailableFired = true;
  this.accum += cnt;
  this.readData += read_stream(stream, cnt);
};
Http2DoublypushedListener.prototype.onStopRequest = function (request, status) {
  Assert.ok(this.onStartRequestFired);
  Assert.ok(Components.isSuccessCode(status));
  Assert.ok(this.onDataAvailableFired);
  Assert.equal(this.readData, "pushed");

  request.QueryInterface(Ci.nsIProxiedChannel);
  let httpProxyConnectResponseCode = request.httpProxyConnectResponseCode;
  this.finish({ httpProxyConnectResponseCode });
};

function test_http2_doublepush(loadGroup, serverPort) {
  var chan = makeHTTPChannel(`https://localhost:${serverPort}/doublepush`);
  return new Promise(resolve => {
    var listener = new Http2DoublepushListener();
    listener.finish = resolve;
    listener.loadGroup = loadGroup;
    listener.serverPort = serverPort;
    chan.loadGroup = loadGroup;
    chan.asyncOpen(listener);
  });
}
