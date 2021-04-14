"use strict";

const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");

// Generate a post with known pre-calculated md5 sum.
function generateContent(size) {
  let content = "";
  for (let i = 0; i < size; i++) {
    content += "0";
  }
  return content;
}

let post = generateContent(10);

// Max concurent stream number in neqo is 100.
// Openning 120 streams will test queuing of streams.
let number_of_parallel_requests = 120;
let h1Server = null;
let h3Route;
let httpsOrigin;
let httpOrigin;
let h3AltSvc;

let prefs;

let tests = [
  // This test must be the first because it setsup alt-svc connection, that
  // other tests use.
  test_https_alt_svc,
  test_multiple_requests,
  test_request_cancelled_by_server,
  test_stream_cancelled_by_necko,
  test_multiple_request_one_is_cancelled,
  test_multiple_request_one_is_cancelled_by_necko,
  test_post,
  test_patch,
  test_http_alt_svc,
  test_slow_receiver,
  // This test should be at the end, because it will close http3
  // connection and the transaction will switch to already existing http2
  // connection.
  // TODO: Bug 1582667 should try to fix issue with connection being closed.
  test_version_fallback,
  testsDone,
];

let current_test = 0;

function run_next_test() {
  if (current_test < tests.length) {
    dump("starting test number " + current_test + "\n");
    tests[current_test]();
    current_test++;
  }
}

function run_test() {
  let env = Cc["@mozilla.org/process/environment;1"].getService(
    Ci.nsIEnvironment
  );
  let h2Port = env.get("MOZHTTP2_PORT");
  Assert.notEqual(h2Port, null);
  Assert.notEqual(h2Port, "");
  let h3Port = env.get("MOZHTTP3_PORT");
  Assert.notEqual(h3Port, null);
  Assert.notEqual(h3Port, "");
  h3AltSvc = ":" + h3Port;

  h3Route = "foo.example.com:" + h3Port;
  do_get_profile();
  prefs = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);

  prefs.setBoolPref("network.http.http3.enabled", true);
  prefs.setCharPref("network.dns.localDomains", "foo.example.com");
  // We always resolve elements of localDomains as it's hardcoded without the
  // following pref:
  prefs.setBoolPref("network.proxy.allow_hijacking_localhost", true);

  // The certificate for the http3server server is for foo.example.com and
  // is signed by http2-ca.pem so add that cert to the trust list as a
  // signing cert.
  let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
  addCertFromFile(certdb, "http2-ca.pem", "CTu,u,u");
  httpsOrigin = "https://foo.example.com:" + h2Port + "/";

  h1Server = new HttpServer();
  h1Server.registerPathHandler("/http3-test", h1Response);
  h1Server.registerPathHandler("/.well-known/http-opportunistic", h1ServerWK);
  h1Server.registerPathHandler("/VersionFallback", h1Response);
  h1Server.start(-1);
  h1Server.identity.setPrimary(
    "http",
    "foo.example.com",
    h1Server.identity.primaryPort
  );
  httpOrigin = "http://foo.example.com:" + h1Server.identity.primaryPort + "/";

  run_next_test();
}

function h1Response(metadata, response) {
  response.setStatusLine(metadata.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/plain", false);
  response.setHeader("Connection", "close", false);
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Access-Control-Allow-Origin", "*", false);
  response.setHeader("Access-Control-Allow-Method", "GET", false);
  response.setHeader("Access-Control-Allow-Headers", "x-altsvc", false);

  try {
    let hval = "h3-27=" + metadata.getHeader("x-altsvc");
    response.setHeader("Alt-Svc", hval, false);
  } catch (e) {}

  let body = "Q: What did 0 say to 8? A: Nice Belt!\n";
  response.bodyOutputStream.write(body, body.length);
}

function h1ServerWK(metadata, response) {
  response.setStatusLine(metadata.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "application/json", false);
  response.setHeader("Connection", "close", false);
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Access-Control-Allow-Origin", "*", false);
  response.setHeader("Access-Control-Allow-Method", "GET", false);
  response.setHeader("Access-Control-Allow-Headers", "x-altsvc", false);

  let body = '["http://foo.example.com:' + h1Server.identity.primaryPort + '"]';
  response.bodyOutputStream.write(body, body.length);
}

function makeChan(uri) {
  let chan = NetUtil.newChannel({
    uri,
    loadUsingSystemPrincipal: true,
  }).QueryInterface(Ci.nsIHttpChannel);
  chan.loadFlags = Ci.nsIChannel.LOAD_INITIAL_DOCUMENT_URI;
  return chan;
}

let Http3CheckListener = function() {};

Http3CheckListener.prototype = {
  onDataAvailableFired: false,
  expectedStatus: Cr.NS_OK,
  expectedRoute: "",

  onStartRequest: function testOnStartRequest(request) {
    Assert.ok(request instanceof Ci.nsIHttpChannel);

    Assert.equal(request.status, this.expectedStatus);
    if (Components.isSuccessCode(this.expectedStatus)) {
      Assert.equal(request.responseStatus, 200);
    }
  },

  onDataAvailable: function testOnDataAvailable(request, stream, off, cnt) {
    this.onDataAvailableFired = true;
    read_stream(stream, cnt);
  },

  onStopRequest: function testOnStopRequest(request, status) {
    Assert.equal(status, this.expectedStatus);
    let routed = "NA";
    try {
      routed = request.getRequestHeader("Alt-Used");
    } catch (e) {}
    dump("routed is " + routed + "\n");

    Assert.equal(routed, this.expectedRoute);

    if (Components.isSuccessCode(this.expectedStatus)) {
      let httpVersion = "";
      try {
        httpVersion = request.protocolVersion;
      } catch (e) {}
      Assert.equal(httpVersion, "h3");
      Assert.equal(this.onDataAvailableFired, true);
      Assert.equal(request.getResponseHeader("X-Firefox-Http3"), "h3-27");
    }
    run_next_test();
    do_test_finished();
  },
};

let WaitForHttp3Listener = function() {};

WaitForHttp3Listener.prototype = new Http3CheckListener();

WaitForHttp3Listener.prototype.uri = "";
WaitForHttp3Listener.prototype.h3AltSvc = "";

WaitForHttp3Listener.prototype.onStopRequest = function testOnStopRequest(
  request,
  status
) {
  Assert.equal(status, this.expectedStatus);

  let routed = "NA";
  try {
    routed = request.getRequestHeader("Alt-Used");
  } catch (e) {}
  dump("routed is " + routed + "\n");

  let httpVersion = "";
  try {
    httpVersion = request.protocolVersion;
  } catch (e) {}

  if (routed == this.expectedRoute) {
    Assert.equal(routed, this.expectedRoute); // always true, but a useful log
    Assert.equal(httpVersion, "h3");
    run_next_test();
  } else {
    dump("poll later for alt svc mapping\n");
    if (httpVersion == "h2") {
      request.QueryInterface(Ci.nsIHttpChannelInternal);
      Assert.ok(request.supportsHTTP3);
    }
    do_test_pending();
    do_timeout(500, () => {
      doTest(this.uri, this.expectedRoute, this.h3AltSvc);
    });
  }

  do_test_finished();
};

function doTest(uri, expectedRoute, altSvc) {
  let chan = makeChan(uri);
  let listener = new WaitForHttp3Listener();
  listener.uri = uri;
  listener.expectedRoute = expectedRoute;
  listener.h3AltSvc = altSvc;
  chan.setRequestHeader("x-altsvc", altSvc, false);
  chan.asyncOpen(listener);
}

// Test Alt-Svc for http3.
// H2 server returns alt-svc=h3-27=:h3port
function test_https_alt_svc() {
  dump("test_https_alt_svc()\n");
  do_test_pending();
  doTest(httpsOrigin + "http3-test", h3Route, h3AltSvc);
}

// Listener for a number of parallel requests. if with_error is set, one of
// the channels will be cancelled (by the server or in onStartRequest).
let MultipleListener = function() {};

MultipleListener.prototype = {
  number_of_parallel_requests: 0,
  with_error: Cr.NS_OK,
  count_of_done_requests: 0,
  error_found_onstart: false,
  error_found_onstop: false,
  need_cancel_found: false,

  onStartRequest: function testOnStartRequest(request) {
    Assert.ok(request instanceof Ci.nsIHttpChannel);

    let need_cancel = "";
    try {
      need_cancel = request.getRequestHeader("CancelMe");
    } catch (e) {}
    if (need_cancel != "") {
      this.need_cancel_found = true;
      request.cancel(Cr.NS_ERROR_ABORT);
    } else if (Components.isSuccessCode(request.status)) {
      Assert.equal(request.responseStatus, 200);
    } else if (this.error_found_onstart) {
      do_throw("We should have only one request failing.");
    } else {
      Assert.equal(request.status, this.with_error);
      this.error_found_onstart = true;
    }
  },

  onDataAvailable: function testOnDataAvailable(request, stream, off, cnt) {
    read_stream(stream, cnt);
  },

  onStopRequest: function testOnStopRequest(request, status) {
    let routed = "";
    try {
      routed = request.getRequestHeader("Alt-Used");
    } catch (e) {}
    Assert.equal(routed, this.expectedRoute);

    if (Components.isSuccessCode(request.status)) {
      let httpVersion = "";
      try {
        httpVersion = request.protocolVersion;
      } catch (e) {}
      Assert.equal(httpVersion, "h3");
    }

    if (!Components.isSuccessCode(request.status)) {
      if (this.error_found_onstop) {
        do_throw("We should have only one request failing.");
      } else {
        Assert.equal(request.status, this.with_error);
        this.error_found_onstop = true;
      }
    }
    this.count_of_done_requests++;
    if (this.count_of_done_requests == this.number_of_parallel_requests) {
      if (Components.isSuccessCode(this.with_error)) {
        Assert.equal(this.error_found_onstart, false);
        Assert.equal(this.error_found_onstop, false);
      } else {
        Assert.ok(this.error_found_onstart || this.need_cancel_found);
        Assert.equal(this.error_found_onstop, true);
      }
      run_next_test();
    }
    do_test_finished();
  },
};

// Multiple requests
function test_multiple_requests() {
  dump("test_multiple_requests()\n");

  let listener = new MultipleListener();
  listener.number_of_parallel_requests = number_of_parallel_requests;
  listener.expectedRoute = h3Route;

  for (let i = 0; i < number_of_parallel_requests; i++) {
    let chan = makeChan(httpsOrigin + "20000");
    chan.asyncOpen(listener);
    do_test_pending();
  }
}

// A request cancelled by a server.
function test_request_cancelled_by_server() {
  dump("test_request_cancelled_by_server()\n");

  let listener = new Http3CheckListener();
  listener.expectedStatus = Cr.NS_ERROR_NET_INTERRUPT;
  listener.expectedRoute = h3Route;
  let chan = makeChan(httpsOrigin + "RequestCancelled");
  chan.asyncOpen(listener);
  do_test_pending();
}

let CancelRequestListener = function() {};

CancelRequestListener.prototype = new Http3CheckListener();

CancelRequestListener.prototype.expectedStatus = Cr.NS_ERROR_ABORT;

CancelRequestListener.prototype.onStartRequest = function testOnStartRequest(
  request
) {
  Assert.ok(request instanceof Ci.nsIHttpChannel);

  Assert.equal(Components.isSuccessCode(request.status), true);
  request.cancel(Cr.NS_ERROR_ABORT);
};

// Cancel stream after OnStartRequest.
function test_stream_cancelled_by_necko() {
  dump("test_stream_cancelled_by_necko()\n");

  let listener = new CancelRequestListener();
  listener.expectedRoute = h3Route;
  let chan = makeChan(httpsOrigin + "20000");
  chan.asyncOpen(listener);
  do_test_pending();
}

// Multiple requests, one gets cancelled by the server, the other should finish normally.
function test_multiple_request_one_is_cancelled() {
  dump("test_multiple_request_one_is_cancelled()\n");

  let listener = new MultipleListener();
  listener.number_of_parallel_requests = number_of_parallel_requests;
  listener.with_error = Cr.NS_ERROR_NET_INTERRUPT;
  listener.expectedRoute = h3Route;

  for (let i = 0; i < number_of_parallel_requests; i++) {
    let uri = httpsOrigin + "20000";
    if (i == 4) {
      // Add a request that will be cancelled by the server.
      uri = httpsOrigin + "RequestCancelled";
    }
    let chan = makeChan(uri);
    chan.asyncOpen(listener);
    do_test_pending();
  }
}

// Multiple requests, one gets cancelled by us, the other should finish normally.
function test_multiple_request_one_is_cancelled_by_necko() {
  dump("test_multiple_request_one_is_cancelled_by_necko()\n");

  let listener = new MultipleListener();
  listener.number_of_parallel_requests = number_of_parallel_requests;
  listener.with_error = Cr.NS_ERROR_ABORT;
  listener.expectedRoute = h3Route;
  for (let i = 0; i < number_of_parallel_requests; i++) {
    let chan = makeChan(httpsOrigin + "20000");
    if (i == 4) {
      // MultipleListener will cancel request with this header.
      chan.setRequestHeader("CancelMe", "true", false);
    }
    chan.asyncOpen(listener);
    do_test_pending();
  }
}

let PostListener = function() {};

PostListener.prototype = new Http3CheckListener();

PostListener.prototype.onDataAvailable = function(request, stream, off, cnt) {
  this.onDataAvailableFired = true;
  read_stream(stream, cnt);
};

// Support for doing a POST
function do_post(content, chan, listener, method) {
  let stream = Cc["@mozilla.org/io/string-input-stream;1"].createInstance(
    Ci.nsIStringInputStream
  );
  stream.data = content;

  let uchan = chan.QueryInterface(Ci.nsIUploadChannel);
  uchan.setUploadStream(stream, "text/plain", stream.available());

  chan.requestMethod = method;

  chan.asyncOpen(listener);
}

// Test a simple POST
function test_post() {
  dump("test_post()");
  let chan = makeChan(httpsOrigin + "post");
  let listener = new PostListener();
  listener.expectedRoute = h3Route;
  do_post(post, chan, listener, "POST");
  do_test_pending();
}

// Test a simple PATCH
function test_patch() {
  dump("test_patch()");
  let chan = makeChan(httpsOrigin + "patch");
  let listener = new PostListener();
  listener.expectedRoute = h3Route;
  do_post(post, chan, listener, "PATCH");
  do_test_pending();
}

// Test alt-svc for http (without s)
function test_http_alt_svc() {
  dump("test_http_alt_svc()\n");

  do_test_pending();
  doTest(httpOrigin + "http3-test", h3Route, h3AltSvc);
}

let SlowReceiverListener = function() {};

SlowReceiverListener.prototype = new Http3CheckListener();
SlowReceiverListener.prototype.count = 0;

SlowReceiverListener.prototype.onDataAvailable = function(
  request,
  stream,
  off,
  cnt
) {
  this.onDataAvailableFired = true;
  this.count += cnt;
  read_stream(stream, cnt);
};

SlowReceiverListener.prototype.onStopRequest = function(request, status) {
  Assert.equal(status, this.expectedStatus);
  Assert.equal(this.count, 10000000);
  let routed = "NA";
  try {
    routed = request.getRequestHeader("Alt-Used");
  } catch (e) {}
  dump("routed is " + routed + "\n");

  Assert.equal(routed, this.expectedRoute);

  if (Components.isSuccessCode(this.expectedStatus)) {
    let httpVersion = "";
    try {
      httpVersion = request.protocolVersion;
    } catch (e) {}
    Assert.equal(httpVersion, "h3");
    Assert.equal(this.onDataAvailableFired, true);
  }
  run_next_test();
  do_test_finished();
};

function test_slow_receiver() {
  dump("test_slow_receiver()\n");
  let chan = makeChan(httpsOrigin + "10000000");
  let listener = new SlowReceiverListener();
  listener.expectedRoute = h3Route;
  chan.asyncOpen(listener);
  do_test_pending();
  chan.suspend();
  do_timeout(1000, chan.resume);
}

let CheckFallbackListener = function() {};

CheckFallbackListener.prototype = {
  onStartRequest: function testOnStartRequest(request) {
    Assert.ok(request instanceof Ci.nsIHttpChannel);

    Assert.equal(request.status, Cr.NS_OK);
    Assert.equal(request.responseStatus, 200);
  },

  onDataAvailable: function testOnDataAvailable(request, stream, off, cnt) {
    read_stream(stream, cnt);
  },

  onStopRequest: function testOnStopRequest(request, status) {
    Assert.equal(status, Cr.NS_OK);
    let routed = "NA";
    try {
      routed = request.getRequestHeader("Alt-Used");
    } catch (e) {}
    dump("routed is " + routed + "\n");

    Assert.equal(routed, "0");

    let httpVersion = "";
    try {
      httpVersion = request.protocolVersion;
    } catch (e) {}
    Assert.equal(httpVersion, "http/1.1");
    run_next_test();
    do_test_finished();
  },
};

// Server cancels request with VersionFallback.
function test_version_fallback() {
  dump("test_version_fallback()\n");

  let chan = makeChan(httpsOrigin + "VersionFallback");
  let listener = new CheckFallbackListener();
  chan.asyncOpen(listener);
  do_test_pending();
}

function testsDone() {
  prefs.clearUserPref("network.http.http3.enabled");
  prefs.clearUserPref("network.dns.localDomains");
  prefs.clearUserPref("network.proxy.allow_hijacking_localhost");
  dump("testDone\n");
  do_test_pending();
  h1Server.stop(do_test_finished);
}
