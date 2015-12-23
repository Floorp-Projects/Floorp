Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");

var h2Port;
var prefs;
var spdypref;
var http2pref;
var tlspref;
var altsvcpref1;
var altsvcpref2;

// https://foo.example.com:(h2Port)
// https://bar.example.com:(h2Port) <- invalid for bar, but ok for foo
var h1Foo; // server http://foo.example.com:(h1Foo.identity.primaryPort)
var h1Bar; // server http://bar.example.com:(h1bar.identity.primaryPort)

var h2FooRoute; // foo.example.com:H2PORT
var h2BarRoute; // bar.example.com:H2PORT
var h2Route;    // :H2PORT
var httpFooOrigin; // http://foo.exmaple.com:PORT/
var httpsFooOrigin; // https://foo.exmaple.com:PORT/
var httpBarOrigin; // http://bar.example.com:PORT/
var httpsBarOrigin; // https://bar.example.com:PORT/

function run_test() {
  var env = Cc["@mozilla.org/process/environment;1"].getService(Ci.nsIEnvironment);
  h2Port = env.get("MOZHTTP2_PORT");
  do_check_neq(h2Port, null);
  do_check_neq(h2Port, "");

  // Set to allow the cert presented by our H2 server
  do_get_profile();
  prefs = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);

  spdypref = prefs.getBoolPref("network.http.spdy.enabled");
  http2pref = prefs.getBoolPref("network.http.spdy.enabled.http2");
  tlspref = prefs.getBoolPref("network.http.spdy.enforce-tls-profile");
  altsvcpref1 = prefs.getBoolPref("network.http.altsvc.enabled");
  altsvcpref2 = prefs.getBoolPref("network.http.altsvc.oe", true);

  prefs.setBoolPref("network.http.spdy.enabled", true);
  prefs.setBoolPref("network.http.spdy.enabled.http2", true);
  prefs.setBoolPref("network.http.spdy.enforce-tls-profile", false);
  prefs.setBoolPref("network.http.altsvc.enabled", true);
  prefs.setBoolPref("network.http.altsvc.oe", true);
  prefs.setCharPref("network.dns.localDomains", "foo.example.com, bar.example.com");

  // The moz-http2 cert is for foo.example.com and is signed by CA.cert.der
  // so add that cert to the trust list as a signing cert. The same cert is used
  // for both h2FooRoute and h2BarRoute though it is only valid for
  // the foo.example.com domain name.
  let certdb = Cc["@mozilla.org/security/x509certdb;1"]
                  .getService(Ci.nsIX509CertDB);
  addCertFromFile(certdb, "CA.cert.der", "CTu,u,u");

  h1Foo = new HttpServer();
  h1Foo.registerPathHandler("/altsvc-test", h1Server);
  h1Foo.start(-1);
  h1Foo.identity.setPrimary("http", "foo.example.com", h1Foo.identity.primaryPort);

  h1Bar = new HttpServer();
  h1Bar.registerPathHandler("/altsvc-test", h1Server);
  h1Bar.start(-1);
  h1Bar.identity.setPrimary("http", "bar.example.com", h1Bar.identity.primaryPort);

  h2FooRoute = "foo.example.com:" + h2Port;
  h2BarRoute = "bar.example.com:" + h2Port;
  h2Route = ":" + h2Port;

  httpFooOrigin = "http://foo.example.com:" + h1Foo.identity.primaryPort + "/";
  httpsFooOrigin = "https://" + h2FooRoute + "/";
  httpBarOrigin = "http://bar.example.com:" + h1Bar.identity.primaryPort + "/";
  httpsBarOrigin = "https://" + h2BarRoute + "/";
  dump ("http foo - " + httpFooOrigin + "\n" +
        "https foo - " + httpsFooOrigin + "\n" +
        "http bar - " + httpBarOrigin + "\n" +
        "https bar - " + httpsBarOrigin + "\n");

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

function resetPrefs() {
  prefs.setBoolPref("network.http.spdy.enabled", spdypref);
  prefs.setBoolPref("network.http.spdy.enabled.http2", http2pref);
  prefs.setBoolPref("network.http.spdy.enforce-tls-profile", tlspref);
  prefs.setBoolPref("network.http.altsvc.enabled", altsvcpref1);
  prefs.setBoolPref("network.http.altsvc.oe", altsvcpref2);
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
  certdb.addCert(der, trustString, null);
}

function makeChan(origin) {
  var ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
  var chan = ios.newChannel2(origin + "altsvc-test",
                             null,
                             null,
                             null,      // aLoadingNode
                             Services.scriptSecurityManager.getSystemPrincipal(),
                             null,      // aTriggeringPrincipal
                             Ci.nsILoadInfo.SEC_NORMAL,
                             Ci.nsIContentPolicy.TYPE_OTHER).QueryInterface(Ci.nsIHttpChannel);

  return chan;
}

var origin;
var xaltsvc;
var retryCounter = 0;
var loadWithoutAltSvc = false;
var nextTest;
var expectPass = true;
var waitFor = 0;

var Listener = function() {};
Listener.prototype = {
  onStartRequest: function testOnStartRequest(request, ctx) {
    do_check_true(request instanceof Components.interfaces.nsIHttpChannel);

    if (expectPass) {
      if (!Components.isSuccessCode(request.status)) {
        do_throw("Channel should have a success code! (" + request.status + ")");
      }
      do_check_eq(request.responseStatus, 200);
    } else {
      do_check_eq(Components.isSuccessCode(request.status), false);
    }
  },

  onDataAvailable: function testOnDataAvailable(request, ctx, stream, off, cnt) {
    read_stream(stream, cnt);
  },

  onStopRequest: function testOnStopRequest(request, ctx, status) {
    var routed = "";
    try {
      routed = request.getRequestHeader("Alt-Used");
    } catch (e) {}
    dump("routed is " + routed + "\n");

    if (waitFor != 0) {
      do_check_eq(routed, "");
      do_test_pending();
      do_timeout(waitFor, doTest);
      waitFor = 0;
      xaltsvc = "NA";
    } else if (xaltsvc == "NA") {
      do_check_eq(routed, "");
      nextTest();
    } else if (routed == xaltsvc) {
      do_check_eq(routed, xaltsvc); // always true, but a useful log
      nextTest();
    } else {
      dump ("poll later for alt svc mapping\n");
      do_test_pending();
      do_timeout(500, doTest);
    }

    do_test_finished();
  }
};

function testsDone()
{
  dump("testDone\n");
  resetPrefs();
  do_test_pending();
  h1Foo.stop(do_test_finished);
  do_test_pending();
  h1Bar.stop(do_test_finished);
}

function doTest()
{
  dump("execute doTest " + origin + "\n");
  var chan = makeChan(origin);
  var listener = new Listener();
  if (xaltsvc != "NA") {
    chan.setRequestHeader("x-altsvc", xaltsvc, false);
  }
  chan.loadFlags = Ci.nsIRequest.LOAD_FRESH_CONNECTION |
	           Ci.nsIChannel.LOAD_INITIAL_DOCUMENT_URI;
  chan.asyncOpen(listener, null);
}

// xaltsvc is overloaded to do two things..
// 1] it is sent in the x-altsvc request header, and the response uses the value in the Alt-Svc response header
// 2] the test polls until necko sets Alt-Used to that value (i.e. it uses that route)
//
// When xaltsvc is set to h2Route (i.e. :port with the implied hostname) it doesn't match the alt-used,
// which is always explicit, so it needs to be changed after the channel is created but before the
// listener is invoked
    
// http://foo served from h2=:port
function doTest1()
{
  dump("doTest1()\n");
  origin = httpFooOrigin;
  xaltsvc = h2Route;
  nextTest = doTest2;
  do_test_pending();
  doTest();
  xaltsvc = h2FooRoute;
}

// http://foo served from h2=foo:port
function doTest2()
{
  dump("doTest2()\n");
  origin = httpFooOrigin;
  xaltsvc = h2FooRoute;
  nextTest = doTest3;
  do_test_pending();
  doTest();
}

// http://foo served from h2=bar:port
// requires cert for foo
function doTest3()
{
  dump("doTest3()\n");
  origin = httpFooOrigin;
  xaltsvc = h2BarRoute;
  nextTest = doTest4;
  do_test_pending();
  doTest();
}

// https://bar should fail because host bar has cert for foo
function doTest4()
{
  dump("doTest4()\n");
  origin = httpsBarOrigin;
  xaltsvc = '';
  expectPass = false;
  nextTest = doTest5;
  do_test_pending();
  doTest();
}

// https://foo no alt-svc (just check cert setup)
function doTest5()
{
  dump("doTest5()\n");
  origin = httpsFooOrigin;
  xaltsvc = 'NA';
  expectPass = true;
  nextTest = doTest6;
  do_test_pending();
  doTest();
}

// https://foo via bar (bar has cert for foo)
function doTest6()
{
  dump("doTest6()\n");
  origin = httpsFooOrigin;
  xaltsvc = h2BarRoute;
  nextTest = doTest7;
  do_test_pending();
  doTest();
}

// check again https://bar should fail because host bar has cert for foo
function doTest7()
{
  dump("doTest7()\n");
  origin = httpsBarOrigin;
  xaltsvc = '';
  expectPass = false;
  nextTest = doTest8;
  do_test_pending();
  doTest();
}

// http://bar via h2 on bar
function doTest8()
{
  dump("doTest8()\n");
  origin = httpBarOrigin;
  xaltsvc = h2BarRoute;
  expectPass = true;
  nextTest = doTest9;
  do_test_pending();
  doTest();
}

// http://bar served from h2=:port
function doTest9()
{
  dump("doTest9()\n");
  origin = httpBarOrigin;
  xaltsvc = h2Route;
  nextTest = doTest10;
  do_test_pending();
  doTest();
  xaltsvc = h2BarRoute;
}

// check again https://bar should fail because host bar has cert for foo
function doTest10()
{
  dump("doTest10()\n");
  origin = httpsBarOrigin;
  xaltsvc = '';
  expectPass = false;
  nextTest = doTest11;
  do_test_pending();
  doTest();
}

// http://bar served from h2=foo, should fail because host foo only has
// cert for foo. Fail in this case means alt-svc is not used, but content
// is served
function doTest11()
{
  dump("doTest11()\n");
  origin = httpBarOrigin;
  xaltsvc = h2FooRoute;
  expectPass = true;
  waitFor = 1000;
  nextTest = testsDone;
  do_test_pending();
  doTest();
}

