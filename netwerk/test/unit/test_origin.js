
var h2Port;
var prefs;
var spdypref;
var http2pref;
var extpref;
var loadGroup;

function run_test() {
  var env = Cc["@mozilla.org/process/environment;1"].getService(Ci.nsIEnvironment);
  h2Port = env.get("MOZHTTP2_PORT");
  Assert.notEqual(h2Port, null);
  Assert.notEqual(h2Port, "");

  // Set to allow the cert presented by our H2 server
  do_get_profile();
  prefs = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);

  spdypref = prefs.getBoolPref("network.http.spdy.enabled");
  http2pref = prefs.getBoolPref("network.http.spdy.enabled.http2");
  extpref = prefs.getBoolPref("network.http.originextension");

  prefs.setBoolPref("network.http.spdy.enabled", true);
  prefs.setBoolPref("network.http.spdy.enabled.http2", true);
  prefs.setBoolPref("network.http.originextension", true);
  prefs.setCharPref("network.dns.localDomains", "foo.example.com, alt1.example.com");

  // The moz-http2 cert is for {foo, alt1, alt2}.example.com and is signed by http2-ca.pem
  // so add that cert to the trust list as a signing cert.
  let certdb = Cc["@mozilla.org/security/x509certdb;1"]
                  .getService(Ci.nsIX509CertDB);
  addCertFromFile(certdb, "http2-ca.pem", "CTu,u,u");

  doTest1();
}

function resetPrefs() {
  prefs.setBoolPref("network.http.spdy.enabled", spdypref);
  prefs.setBoolPref("network.http.spdy.enabled.http2", http2pref);
  prefs.setBoolPref("network.http.originextension", extpref);
  prefs.clearUserPref("network.dns.localDomains");
}

function makeChan(origin) {
  return NetUtil.newChannel({
    uri: origin,
    loadUsingSystemPrincipal: true
  }).QueryInterface(Ci.nsIHttpChannel);
}

var nextTest;
var nextPortExpectedToBeSame = false;
var currentPort = 0;
var forceReload = false;
var forceFailListener = false;

var Listener = function() {};
Listener.prototype.clientPort = 0;
Listener.prototype = {
  onStartRequest: function testOnStartRequest(request) {
    Assert.ok(request instanceof Ci.nsIHttpChannel);

    if (!Components.isSuccessCode(request.status)) {
      do_throw("Channel should have a success code! (" + request.status + ")");
    }
    Assert.equal(request.responseStatus, 200);
    this.clientPort = parseInt(request.getResponseHeader("x-client-port"));
  },

  onDataAvailable: function testOnDataAvailable(request, stream, off, cnt) {
    read_stream(stream, cnt);
  },

  onStopRequest: function testOnStopRequest(request, status) {
    Assert.ok(Components.isSuccessCode(status));
    if (nextPortExpectedToBeSame) {
     Assert.equal(currentPort, this.clientPort);
    } else {
     Assert.notEqual(currentPort, this.clientPort);
    }
    currentPort = this.clientPort;
    nextTest();
    do_test_finished();
  }
};

var FailListener = function() {};
FailListener.prototype = {
  onStartRequest: function testOnStartRequest(request) {
    Assert.ok(request instanceof Ci.nsIHttpChannel);
    Assert.ok(!Components.isSuccessCode(request.status));
  },
  onDataAvailable: function testOnDataAvailable(request, stream, off, cnt) {
    read_stream(stream, cnt);
  },
  onStopRequest: function testOnStopRequest(request, status) {
    Assert.ok(!Components.isSuccessCode(request.status));
    nextTest();
    do_test_finished();
  }
};

function testsDone()
{
  dump("testsDone\n");
  resetPrefs();
}

function doTest()
{
  dump("execute doTest " + origin + "\n");
  var chan = makeChan(origin);
  var listener;
  if (!forceFailListener) {
    listener = new Listener();
  } else {
    listener = new FailListener();
  }
  forceFailListener = false;

  if (!forceReload) {
    chan.loadFlags = Ci.nsIChannel.LOAD_INITIAL_DOCUMENT_URI;
  } else {
    chan.loadFlags = Ci.nsIRequest.LOAD_FRESH_CONNECTION |
                     Ci.nsIChannel.LOAD_INITIAL_DOCUMENT_URI;
  }
  forceReload = false;
  chan.asyncOpen(listener);
}

function doTest1()
{
  dump("doTest1()\n");
  origin = "https://foo.example.com:" + h2Port + "/origin-1";
  nextTest = doTest2;
  nextPortExpectedToBeSame = false;
  do_test_pending();
  doTest();
}

function doTest2()
{
  // plain connection reuse
  dump("doTest2()\n");
  origin = "https://foo.example.com:" + h2Port + "/origin-2";
  nextTest = doTest3;
  nextPortExpectedToBeSame = true;
  do_test_pending();
  doTest();
}

function doTest3()
{
  // 7540 style coalescing
  dump("doTest3()\n");
  origin = "https://alt1.example.com:" + h2Port + "/origin-3";
  nextTest = doTest4;
  nextPortExpectedToBeSame = true;
  do_test_pending();
  doTest();
}

function doTest4()
{
  // forces an empty origin frame to be omitted
  dump("doTest4()\n");
  origin = "https://foo.example.com:" + h2Port + "/origin-4";
  nextTest = doTest5;
  nextPortExpectedToBeSame = true;
  do_test_pending();
  doTest();
}

function doTest5()
{
  // 7540 style coalescing should not work due to empty origin set
  dump("doTest5()\n");
  origin = "https://alt1.example.com:" + h2Port + "/origin-5";
  nextTest = doTest6;
  nextPortExpectedToBeSame = false;
  do_test_pending();
  doTest();
}

function doTest6()
{
  // get a fresh connection with alt1 and alt2 in origin set
  // note that there is no dns for alt2
  dump("doTest6()\n");
  origin = "https://foo.example.com:" + h2Port + "/origin-6";
  nextTest = doTest7;
  nextPortExpectedToBeSame = false;
  forceReload = true;
  do_test_pending();
  doTest();
}

function doTest7()
{
  // check conn reuse to ensure sni is implicit in origin set
  dump("doTest7()\n");
  origin = "https://foo.example.com:" + h2Port + "/origin-7";
  nextTest = doTest8;
  nextPortExpectedToBeSame = true;
  do_test_pending();
  doTest();
}

function doTest8()
{
  // alt1 is in origin set (and is 7540 eligible)
  dump("doTest8()\n");
  origin = "https://alt1.example.com:" + h2Port + "/origin-8";
  nextTest = doTest9;
  nextPortExpectedToBeSame = true;
  do_test_pending();
  doTest();
}

function doTest9()
{
  // alt2 is in origin set but does not have dns
  dump("doTest9()\n");
  origin = "https://alt2.example.com:" + h2Port + "/origin-9";
  nextTest = doTest10;
  nextPortExpectedToBeSame = true;
  do_test_pending();
  doTest();
}

function doTest10()
{
  // bar is in origin set but does not have dns like alt2
  // but the cert is not valid for bar. so expect a failure
  dump("doTest10()\n");
  origin = "https://bar.example.com:" + h2Port + "/origin-10";
  nextTest = doTest11;
  nextPortExpectedToBeSame = false;
  forceFailListener = true;
  do_test_pending();
  doTest();
}

var Http2PushApiListener = function() {};

Http2PushApiListener.prototype = {
  fooOK : false,
  alt1OK : false,

  getInterface: function(aIID) {
    return this.QueryInterface(aIID);
  },

  QueryInterface: function(aIID) {
    if (aIID.equals(Ci.nsIHttpPushListener) ||
        aIID.equals(Ci.nsIStreamListener))
      return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
  },

  // nsIHttpPushListener
  onPush: function onPush(associatedChannel, pushChannel) {
    dump("push api onpush " + pushChannel.originalURI.spec +
         " associated to " + associatedChannel.originalURI.spec + "\n");

    Assert.equal(associatedChannel.originalURI.spec, "https://foo.example.com:" + h2Port + "/origin-11-a");
    Assert.equal(pushChannel.getRequestHeader("x-pushed-request"), "true");

    if (pushChannel.originalURI.spec === "https://foo.example.com:" + h2Port + "/origin-11-b") {
      this.fooOK = true;
    } else if (pushChannel.originalURI.spec === "https://alt1.example.com:" + h2Port + "/origin-11-e") {
      this.alt1OK = true;
    } else {
      // any push of bar or madeup should not end up in onPush()
      Assert.equal(true, false);
    }
    pushChannel.cancel(Cr.NS_ERROR_ABORT);
  },

 // normal Channel listeners
  onStartRequest: function pushAPIOnStart(request) {
    dump("push api onstart " + request.originalURI.spec + "\n");
  },

  onDataAvailable: function pushAPIOnDataAvailable(request, stream, offset, cnt) {
    var data = read_stream(stream, cnt);
  },

  onStopRequest: function test_onStopR(request, status) {
    dump("push api onstop " + request.originalURI.spec + "\n");
    Assert.ok(this.fooOK);
    Assert.ok(this.alt1OK);
    nextTest();
    do_test_finished();
  }
};


function doTest11()
{
  // we are connected with an SNI of foo from test6
  // but the origin set is alt1, alt2, bar - foo is implied
  // and bar is not actually covered by the cert
  //
  // the server will push foo (b-OK), bar (c-NOT OK), madeup (d-NOT OK), alt1 (e-OK),

  dump("doTest11()\n");
  do_test_pending();
  loadGroup = Cc["@mozilla.org/network/load-group;1"].createInstance(Ci.nsILoadGroup);
  var chan = makeChan("https://foo.example.com:" + h2Port + "/origin-11-a");
  chan.loadGroup = loadGroup;
  var listener = new Http2PushApiListener();
  nextTest = testsDone;
  chan.notificationCallbacks = listener;
  chan.asyncOpen(listener);
}
