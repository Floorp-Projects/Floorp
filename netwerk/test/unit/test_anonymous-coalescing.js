
/*
- test to check we use only a single connection for both onymous and anonymous requests over an existing h2 session
- request from a domain w/o LOAD_ANONYMOUS flag
- request again from the same domain, but different URI, with LOAD_ANONYMOUS flag, check the client is using the same conn
- close all and do it in the opposite way (do an anonymous req first)
*/

var h2Port;
var prefs;
var spdypref;
var http2pref;
var extpref;

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
var origin;
var nextPortExpectedToBeSame = false;
var currentPort = 0;
var forceReload = false;
var anonymous = false;

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

function testsDone()
{
  dump("testsDone\n");
  resetPrefs();
}

function doTest()
{
  dump("execute doTest " + origin + "\n");

  var loadFlags = Ci.nsIChannel.LOAD_INITIAL_DOCUMENT_URI;
  if (anonymous) {
    loadFlags |= Ci.nsIRequest.LOAD_ANONYMOUS;
  }
  anonymous = false;
  if (forceReload) {
    loadFlags |= Ci.nsIRequest.LOAD_FRESH_CONNECTION;
  }
  forceReload = false;

  var chan = makeChan(origin);
  chan.loadFlags = loadFlags;

  var listener = new Listener();
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
  // connection expected to be reused for an anonymous request
  dump("doTest2()\n");
  origin = "https://foo.example.com:" + h2Port + "/origin-2";
  nextTest = doTest3;
  nextPortExpectedToBeSame = true;
  anonymous = true;
  do_test_pending();
  doTest();
}

function doTest3()
{
  dump("doTest3()\n");
  origin = "https://foo.example.com:" + h2Port + "/origin-3";
  nextTest = doTest4;
  nextPortExpectedToBeSame = false;
  forceReload = true;
  anonymous = true;
  do_test_pending();
  doTest();
}

function doTest4()
{
  dump("doTest4()\n");
  origin = "https://foo.example.com:" + h2Port + "/origin-4";
  nextTest = testsDone;
  nextPortExpectedToBeSame = true;
  do_test_pending();
  doTest();
}
