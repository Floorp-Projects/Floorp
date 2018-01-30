Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/NetUtil.jsm");

var prefs;
var spdypref;
var http2pref;
var origin;
var rcwnpref;

function run_test() {
  var env = Cc["@mozilla.org/process/environment;1"].getService(Ci.nsIEnvironment);
  var h2Port = env.get("MOZHTTP2_PORT");
  Assert.notEqual(h2Port, null);
  Assert.notEqual(h2Port, "");

  // Set to allow the cert presented by our H2 server
  do_get_profile();
  prefs = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);

  spdypref = prefs.getBoolPref("network.http.spdy.enabled");
  http2pref = prefs.getBoolPref("network.http.spdy.enabled.http2");
  rcwnpref = prefs.getBoolPref("network.http.rcwn.enabled");

  prefs.setBoolPref("network.http.spdy.enabled", true);
  prefs.setBoolPref("network.http.spdy.enabled.http2", true);
  prefs.setCharPref("network.dns.localDomains", "foo.example.com, bar.example.com");
  // Disable rcwn to make cache behavior deterministic.
  prefs.setBoolPref("network.http.rcwn.enabled", false);

  // The moz-http2 cert is for foo.example.com and is signed by CA.cert.der
  // so add that cert to the trust list as a signing cert.  // the foo.example.com domain name.
  let certdb = Cc["@mozilla.org/security/x509certdb;1"]
                  .getService(Ci.nsIX509CertDB);
  addCertFromFile(certdb, "CA.cert.der", "CTu,u,u");

  origin = "https://foo.example.com:" + h2Port;
  dump ("origin - " + origin + "\n");
  doTest1();
}

function resetPrefs() {
  prefs.setBoolPref("network.http.spdy.enabled", spdypref);
  prefs.setBoolPref("network.http.spdy.enabled.http2", http2pref);
  prefs.setBoolPref("network.http.rcwn.enabled", rcwnpref);
  prefs.clearUserPref("network.dns.localDomains");
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

function makeChan(origin, path) {
  return NetUtil.newChannel({
    uri: origin + path,
    loadUsingSystemPrincipal: true
  }).QueryInterface(Ci.nsIHttpChannel);
}

var nextTest;
var expectPass = true;
var expectConditional = false;

var Listener = function() {};
Listener.prototype = {
  onStartRequest: function testOnStartRequest(request, ctx) {
    Assert.ok(request instanceof Components.interfaces.nsIHttpChannel);

    if (expectPass) {
      if (!Components.isSuccessCode(request.status)) {
        do_throw("Channel should have a success code! (" + request.status + ")");
      }
      Assert.equal(request.responseStatus, 200);
    } else {
      Assert.equal(Components.isSuccessCode(request.status), false);
    }
  },

  onDataAvailable: function testOnDataAvailable(request, ctx, stream, off, cnt) {
    read_stream(stream, cnt);
  },

  onStopRequest: function testOnStopRequest(request, ctx, status) {
      if (expectConditional) {
        Assert.equal(request.getResponseHeader("x-conditional"), "true");
      } else {
	  try { Assert.notEqual(request.getResponseHeader("x-conditional"), "true"); }
	  catch (e) { Assert.ok(true); }
    }
    nextTest();
    do_test_finished();
  }
};

function testsDone()
{
  dump("testDone\n");
  resetPrefs();
}

function doTest1()
{
  dump("execute doTest1 - resource without immutable. initial request\n");
  do_test_pending();
  expectConditional = false;
  var chan = makeChan(origin, "/immutable-test-without-attribute");
  var listener = new Listener();
  nextTest = doTest2;
  chan.asyncOpen2(listener);
}

function doTest2()
{
  dump("execute doTest2 - resource without immutable. reload\n");
  do_test_pending();
  expectConditional = true;
  var chan = makeChan(origin, "/immutable-test-without-attribute");
  var listener = new Listener();
  nextTest = doTest3;
  chan.loadFlags = Ci.nsIRequest.VALIDATE_ALWAYS;
  chan.asyncOpen2(listener);
}

function doTest3()
{
  dump("execute doTest3 - resource without immutable. shift reload\n");
  do_test_pending();
  expectConditional = false;
  var chan = makeChan(origin, "/immutable-test-without-attribute");
  var listener = new Listener();
  nextTest = doTest4;
  chan.loadFlags = Ci.nsIRequest.LOAD_BYPASS_CACHE;
  chan.asyncOpen2(listener);
}

function doTest4()
{
  dump("execute doTest1 - resource with immutable. initial request\n");
  do_test_pending();
  expectConditional = false;
  var chan = makeChan(origin, "/immutable-test-with-attribute");
  var listener = new Listener();
  nextTest = doTest5;
  chan.asyncOpen2(listener);
}

function doTest5()
{
  dump("execute doTest5 - resource with immutable. reload\n");
  do_test_pending();
  expectConditional = false;
  var chan = makeChan(origin, "/immutable-test-with-attribute");
  var listener = new Listener();
  nextTest = doTest6;
  chan.loadFlags = Ci.nsIRequest.VALIDATE_ALWAYS;
  chan.asyncOpen2(listener);
}

function doTest6()
{
  dump("execute doTest3 - resource with immutable. shift reload\n");
  do_test_pending();
  expectConditional = false;
  var chan = makeChan(origin, "/immutable-test-with-attribute");
  var listener = new Listener();
  nextTest = testsDone;
  chan.loadFlags = Ci.nsIRequest.LOAD_BYPASS_CACHE;
  chan.asyncOpen2(listener);
}


