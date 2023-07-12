"use strict";

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

var h2Port;
var prefs;
var http2pref;
var altsvcpref1;
var altsvcpref2;

// https://foo.example.com:(h2Port)
// https://bar.example.com:(h2Port) <- invalid for bar, but ok for foo
var h1Foo; // server http://foo.example.com:(h1Foo.identity.primaryPort)
var h1Bar; // server http://bar.example.com:(h1bar.identity.primaryPort)

var otherServer; // server socket listening for other connection.

var h2FooRoute; // foo.example.com:H2PORT
var h2BarRoute; // bar.example.com:H2PORT
var h2Route; // :H2PORT
var httpFooOrigin; // http://foo.exmaple.com:PORT/
var httpsFooOrigin; // https://foo.exmaple.com:PORT/
var httpBarOrigin; // http://bar.example.com:PORT/
var httpsBarOrigin; // https://bar.example.com:PORT/

function run_test() {
  h2Port = Services.env.get("MOZHTTP2_PORT");
  Assert.notEqual(h2Port, null);
  Assert.notEqual(h2Port, "");

  // Set to allow the cert presented by our H2 server
  do_get_profile();
  prefs = Services.prefs;

  http2pref = prefs.getBoolPref("network.http.http2.enabled");
  altsvcpref1 = prefs.getBoolPref("network.http.altsvc.enabled");
  altsvcpref2 = prefs.getBoolPref("network.http.altsvc.oe", true);

  prefs.setBoolPref("network.http.http2.enabled", true);
  prefs.setBoolPref("network.http.altsvc.enabled", true);
  prefs.setBoolPref("network.http.altsvc.oe", true);
  prefs.setCharPref(
    "network.dns.localDomains",
    "foo.example.com, bar.example.com"
  );

  // The moz-http2 cert is for foo.example.com and is signed by http2-ca.pem
  // so add that cert to the trust list as a signing cert. The same cert is used
  // for both h2FooRoute and h2BarRoute though it is only valid for
  // the foo.example.com domain name.
  let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
  addCertFromFile(certdb, "http2-ca.pem", "CTu,u,u");

  h1Foo = new HttpServer();
  h1Foo.registerPathHandler("/altsvc-test", h1Server);
  h1Foo.registerPathHandler("/.well-known/http-opportunistic", h1ServerWK);
  h1Foo.start(-1);
  h1Foo.identity.setPrimary(
    "http",
    "foo.example.com",
    h1Foo.identity.primaryPort
  );

  h1Bar = new HttpServer();
  h1Bar.registerPathHandler("/altsvc-test", h1Server);
  h1Bar.start(-1);
  h1Bar.identity.setPrimary(
    "http",
    "bar.example.com",
    h1Bar.identity.primaryPort
  );

  h2FooRoute = "foo.example.com:" + h2Port;
  h2BarRoute = "bar.example.com:" + h2Port;
  h2Route = ":" + h2Port;

  httpFooOrigin = "http://foo.example.com:" + h1Foo.identity.primaryPort + "/";
  httpsFooOrigin = "https://" + h2FooRoute + "/";
  httpBarOrigin = "http://bar.example.com:" + h1Bar.identity.primaryPort + "/";
  httpsBarOrigin = "https://" + h2BarRoute + "/";
  dump(
    "http foo - " +
      httpFooOrigin +
      "\n" +
      "https foo - " +
      httpsFooOrigin +
      "\n" +
      "http bar - " +
      httpBarOrigin +
      "\n" +
      "https bar - " +
      httpsBarOrigin +
      "\n"
  );

  doTest1();
}

function h1Server(metadata, response) {
  response.setStatusLine(metadata.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/plain", false);
  response.setHeader("Connection", "close", false);
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Access-Control-Allow-Origin", "*", false);
  response.setHeader("Access-Control-Allow-Method", "GET", false);
  response.setHeader("Access-Control-Allow-Headers", "x-altsvc", false);

  try {
    var hval = "h2=" + metadata.getHeader("x-altsvc");
    response.setHeader("Alt-Svc", hval, false);
  } catch (e) {}

  var body = "Q: What did 0 say to 8? A: Nice Belt!\n";
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

  var body = '["http://foo.example.com:' + h1Foo.identity.primaryPort + '"]';
  response.bodyOutputStream.write(body, body.length);
}

function resetPrefs() {
  prefs.setBoolPref("network.http.http2.enabled", http2pref);
  prefs.setBoolPref("network.http.altsvc.enabled", altsvcpref1);
  prefs.setBoolPref("network.http.altsvc.oe", altsvcpref2);
  prefs.clearUserPref("network.dns.localDomains");
  prefs.clearUserPref("network.security.ports.banned");
}

function makeChan(origin) {
  return NetUtil.newChannel({
    uri: origin + "altsvc-test",
    loadUsingSystemPrincipal: true,
  }).QueryInterface(Ci.nsIHttpChannel);
}

var origin;
var xaltsvc;
var loadWithoutClearingMappings = false;
var disallowH3 = false;
var disallowH2 = false;
var nextTest;
var expectPass = true;
var waitFor = 0;
var originAttributes = {};

var Listener = function () {};
Listener.prototype = {
  onStartRequest: function testOnStartRequest(request) {
    Assert.ok(request instanceof Ci.nsIHttpChannel);

    if (expectPass) {
      if (!Components.isSuccessCode(request.status)) {
        do_throw(
          "Channel should have a success code! (" + request.status + ")"
        );
      }
      Assert.equal(request.responseStatus, 200);
    } else {
      Assert.equal(Components.isSuccessCode(request.status), false);
    }
  },

  onDataAvailable: function testOnDataAvailable(request, stream, off, cnt) {
    read_stream(stream, cnt);
  },

  onStopRequest: function testOnStopRequest(request, status) {
    var routed = "";
    try {
      routed = request.getRequestHeader("Alt-Used");
    } catch (e) {}
    dump("routed is " + routed + "\n");
    Assert.equal(Components.isSuccessCode(status), expectPass);

    if (waitFor != 0) {
      Assert.equal(routed, "");
      do_test_pending();
      loadWithoutClearingMappings = true;
      do_timeout(waitFor, doTest);
      waitFor = 0;
      xaltsvc = "NA";
    } else if (xaltsvc == "NA") {
      Assert.equal(routed, "");
      nextTest();
    } else if (routed == xaltsvc) {
      Assert.equal(routed, xaltsvc); // always true, but a useful log
      nextTest();
    } else {
      dump("poll later for alt svc mapping\n");
      do_test_pending();
      loadWithoutClearingMappings = true;
      do_timeout(500, doTest);
    }

    do_test_finished();
  },
};

function testsDone() {
  dump("testDone\n");
  resetPrefs();
  do_test_pending();
  otherServer.close();
  do_test_pending();
  h1Foo.stop(do_test_finished);
  do_test_pending();
  h1Bar.stop(do_test_finished);
}

function doTest() {
  dump("execute doTest " + origin + "\n");
  var chan = makeChan(origin);
  var listener = new Listener();
  if (xaltsvc != "NA") {
    chan.setRequestHeader("x-altsvc", xaltsvc, false);
  }
  if (loadWithoutClearingMappings) {
    chan.loadFlags = Ci.nsIChannel.LOAD_INITIAL_DOCUMENT_URI;
  } else {
    chan.loadFlags =
      Ci.nsIRequest.LOAD_FRESH_CONNECTION |
      Ci.nsIChannel.LOAD_INITIAL_DOCUMENT_URI;
  }
  if (disallowH3) {
    let internalChannel = chan.QueryInterface(Ci.nsIHttpChannelInternal);
    internalChannel.allowHttp3 = false;
    disallowH3 = false;
  }
  if (disallowH2) {
    let internalChannel = chan.QueryInterface(Ci.nsIHttpChannelInternal);
    internalChannel.allowSpdy = false;
    disallowH2 = false;
  }
  loadWithoutClearingMappings = false;
  chan.loadInfo.originAttributes = originAttributes;
  chan.asyncOpen(listener);
}

// xaltsvc is overloaded to do two things..
// 1] it is sent in the x-altsvc request header, and the response uses the value in the Alt-Svc response header
// 2] the test polls until necko sets Alt-Used to that value (i.e. it uses that route)
//
// When xaltsvc is set to h2Route (i.e. :port with the implied hostname) it doesn't match the alt-used,
// which is always explicit, so it needs to be changed after the channel is created but before the
// listener is invoked

// http://foo served from h2=:port
function doTest1() {
  dump("doTest1()\n");
  origin = httpFooOrigin;
  xaltsvc = h2Route;
  nextTest = doTest2;
  do_test_pending();
  doTest();
  xaltsvc = h2FooRoute;
}

// http://foo served from h2=foo:port
function doTest2() {
  dump("doTest2()\n");
  origin = httpFooOrigin;
  xaltsvc = h2FooRoute;
  nextTest = doTest3;
  do_test_pending();
  doTest();
}

// http://foo served from h2=bar:port
// requires cert for foo
function doTest3() {
  dump("doTest3()\n");
  origin = httpFooOrigin;
  xaltsvc = h2BarRoute;
  nextTest = doTest4;
  do_test_pending();
  doTest();
}

// https://bar should fail because host bar has cert for foo
function doTest4() {
  dump("doTest4()\n");
  origin = httpsBarOrigin;
  xaltsvc = "";
  expectPass = false;
  nextTest = doTest5;
  do_test_pending();
  doTest();
}

// https://foo no alt-svc (just check cert setup)
function doTest5() {
  dump("doTest5()\n");
  origin = httpsFooOrigin;
  xaltsvc = "NA";
  expectPass = true;
  nextTest = doTest6;
  do_test_pending();
  doTest();
}

// https://foo via bar (bar has cert for foo)
function doTest6() {
  dump("doTest6()\n");
  origin = httpsFooOrigin;
  xaltsvc = h2BarRoute;
  nextTest = doTest7;
  do_test_pending();
  doTest();
}

// check again https://bar should fail because host bar has cert for foo
function doTest7() {
  dump("doTest7()\n");
  origin = httpsBarOrigin;
  xaltsvc = "";
  expectPass = false;
  nextTest = doTest8;
  do_test_pending();
  doTest();
}

// http://bar via h2 on bar
// should not use TLS/h2 because h2BarRoute is not auth'd for bar
// however the test ought to PASS (i.e. get a 200) because fallback
// to plaintext happens.. thus the timeout
function doTest8() {
  dump("doTest8()\n");
  origin = httpBarOrigin;
  xaltsvc = h2BarRoute;
  expectPass = true;
  waitFor = 500;
  nextTest = doTest9;
  do_test_pending();
  doTest();
}

// http://bar served from h2=:port, which is like the bar route in 8
function doTest9() {
  dump("doTest9()\n");
  origin = httpBarOrigin;
  xaltsvc = h2Route;
  expectPass = true;
  waitFor = 500;
  nextTest = doTest10;
  do_test_pending();
  doTest();
  xaltsvc = h2BarRoute;
}

// check again https://bar should fail because host bar has cert for foo
function doTest10() {
  dump("doTest10()\n");
  origin = httpsBarOrigin;
  xaltsvc = "";
  expectPass = false;
  nextTest = doTest11;
  do_test_pending();
  doTest();
}

// http://bar served from h2=foo, should fail because host foo only has
// cert for foo. Fail in this case means alt-svc is not used, but content
// is served
function doTest11() {
  dump("doTest11()\n");
  origin = httpBarOrigin;
  xaltsvc = h2FooRoute;
  expectPass = true;
  waitFor = 500;
  nextTest = doTest12;
  do_test_pending();
  doTest();
}

// Test 12-15:
// Insert a cache of http://foo served from h2=:port with origin attributes.
function doTest12() {
  dump("doTest12()\n");
  origin = httpFooOrigin;
  xaltsvc = h2Route;
  originAttributes = {
    userContextId: 1,
    firstPartyDomain: "a.com",
  };
  nextTest = doTest13;
  do_test_pending();
  doTest();
  xaltsvc = h2FooRoute;
}

// Make sure we get a cache miss with a different userContextId.
function doTest13() {
  dump("doTest13()\n");
  origin = httpFooOrigin;
  xaltsvc = "NA";
  originAttributes = {
    userContextId: 2,
    firstPartyDomain: "a.com",
  };
  loadWithoutClearingMappings = true;
  nextTest = doTest14;
  do_test_pending();
  doTest();
}

// Make sure we get a cache miss with a different firstPartyDomain.
function doTest14() {
  dump("doTest14()\n");
  origin = httpFooOrigin;
  xaltsvc = "NA";
  originAttributes = {
    userContextId: 1,
    firstPartyDomain: "b.com",
  };
  loadWithoutClearingMappings = true;
  nextTest = doTest15;
  do_test_pending();
  doTest();
}
//
// Make sure we get a cache hit with the same origin attributes.
function doTest15() {
  dump("doTest15()\n");
  origin = httpFooOrigin;
  xaltsvc = "NA";
  originAttributes = {
    userContextId: 1,
    firstPartyDomain: "a.com",
  };
  loadWithoutClearingMappings = true;
  nextTest = doTest16;
  do_test_pending();
  doTest();
  // This ensures a cache hit.
  xaltsvc = h2FooRoute;
}

// Make sure we do not use H2 if it is disabled on a channel.
function doTest16() {
  dump("doTest16()\n");
  origin = httpFooOrigin;
  xaltsvc = "NA";
  disallowH2 = true;
  originAttributes = {
    userContextId: 1,
    firstPartyDomain: "a.com",
  };
  loadWithoutClearingMappings = true;
  nextTest = doTest17;
  do_test_pending();
  doTest();
}

// Make sure we use H2 if only Http3 is disabled on a channel.
function doTest17() {
  dump("doTest17()\n");
  origin = httpFooOrigin;
  xaltsvc = h2Route;
  disallowH3 = true;
  originAttributes = {
    userContextId: 1,
    firstPartyDomain: "a.com",
  };
  loadWithoutClearingMappings = true;
  nextTest = doTest18;
  do_test_pending();
  doTest();
  // This should ensures a cache hit.
  xaltsvc = h2FooRoute;
}

// Check we don't connect to blocked ports
function doTest18() {
  dump("doTest18()\n");
  origin = httpFooOrigin;
  nextTest = testsDone;
  otherServer = Cc["@mozilla.org/network/server-socket;1"].createInstance(
    Ci.nsIServerSocket
  );
  otherServer.init(-1, true, -1);
  xaltsvc = "localhost:" + otherServer.port;
  Services.prefs.setCharPref(
    "network.security.ports.banned",
    "" + otherServer.port
  );
  dump("Blocked port: " + otherServer.port);
  waitFor = 500;
  otherServer.asyncListen({
    onSocketAccepted() {
      Assert.ok(false, "Got connection to socket when we didn't expect it!");
    },
    onStopListening() {
      // We get closed when the entire file is done, which guarantees we get the socket accept
      // if we do connect to the alt-svc header
      do_test_finished();
    },
  });
  nextTest = doTest19;
  do_test_pending();
  doTest();
}

// Check we don't connect to blocked ports
function doTest19() {
  dump("doTest19()\n");
  origin = httpFooOrigin;
  nextTest = testsDone;
  otherServer = Cc["@mozilla.org/network/server-socket;1"].createInstance(
    Ci.nsIServerSocket
  );
  const BAD_PORT_U32 = 6667 + 65536;
  otherServer.init(BAD_PORT_U32, true, -1);
  Assert.ok(otherServer.port == 6667, "Trying to listen on port 6667");
  xaltsvc = "localhost:" + BAD_PORT_U32;
  dump("Blocked port: " + otherServer.port);
  waitFor = 500;
  otherServer.asyncListen({
    onSocketAccepted() {
      Assert.ok(false, "Got connection to socket when we didn't expect it!");
    },
    onStopListening() {
      // We get closed when the entire file is done, which guarantees we get the socket accept
      // if we do connect to the alt-svc header
      do_test_finished();
    },
  });
  nextTest = doTest20;
  do_test_pending();
  doTest();
}
function doTest20() {
  dump("doTest20()\n");
  origin = httpFooOrigin;
  nextTest = testsDone;
  otherServer = Cc["@mozilla.org/network/server-socket;1"].createInstance(
    Ci.nsIServerSocket
  );
  const BAD_PORT_U64 = 6666 + 429496729;
  otherServer.init(6666, true, -1);
  Assert.ok(otherServer.port == 6666, "Trying to listen on port 6666");
  xaltsvc = "localhost:" + BAD_PORT_U64;
  dump("Blocked port: " + otherServer.port);
  waitFor = 500;
  otherServer.asyncListen({
    onSocketAccepted() {
      Assert.ok(false, "Got connection to socket when we didn't expect it!");
    },
    onStopListening() {
      // We get closed when the entire file is done, which guarantees we get the socket accept
      // if we do connect to the alt-svc header
      do_test_finished();
    },
  });
  nextTest = doTest21;
  do_test_pending();
  doTest();
}
// Port 65535 should be OK
function doTest21() {
  dump("doTest21()\n");
  origin = httpFooOrigin;
  nextTest = testsDone;
  otherServer = Cc["@mozilla.org/network/server-socket;1"].createInstance(
    Ci.nsIServerSocket
  );
  const GOOD_PORT = 65535;
  otherServer.init(65535, true, -1);
  Assert.ok(otherServer.port == 65535, "Trying to listen on port 65535");
  xaltsvc = "localhost:" + GOOD_PORT;
  dump("Allowed port: " + otherServer.port);
  waitFor = 500;
  otherServer.asyncListen({
    onSocketAccepted() {
      Assert.ok(true, "Got connection to socket when we didn't expect it!");
    },
    onStopListening() {
      // We get closed when the entire file is done, which guarantees we get the socket accept
      // if we do connect to the alt-svc header
      do_test_finished();
    },
  });
  do_test_pending();
  doTest();
}
